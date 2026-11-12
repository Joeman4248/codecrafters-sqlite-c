"""Builds the engine, runs each query below through it and through sqlite3, and
prints only the queries whose output differs, then a one-line summary. Run
from anywhere: python tests/check.py

Needs sqlite3 on the PATH and the sample databases (download_sample_databases.sh).
Row order is compared as given: both walk tables in rowid order.

--sanitize builds with AddressSanitizer (buffer overruns, use after free,
leaks) and UndefinedBehaviorSanitizer. Any sanitizer report fails its case,
even one that should fail. MinGW has no sanitizers, so on Windows the whole
script reruns itself in WSL. On Linux the build goes in ~/sqlite-build, or
~/sqlite-sanitize, not in the repo; on Windows it's build/ as usual.
"""

import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SANITIZE = "--sanitize" in sys.argv[1:]
WINDOWS = os.name == "nt"
SANITIZER_FLAGS = "-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"

# overflow.db: rows too big for one 4096-byte page, so their records spill onto
# overflow pages. Rebuilt each run (*.db is git-ignored). hex() doubles a
# blob's length, so hex(randomblob(n)) is 2n characters. The sizes sit around
# the 4061 bytes a cell holds, then go to one long chain of overflow pages.
OVERFLOW_DB = "tests/overflow.db"
OVERFLOW_SQL = """
CREATE TABLE big (id integer primary key, body text, tail text);
INSERT INTO big (body, tail) VALUES ('small', 'fits');
INSERT INTO big (body, tail) VALUES (hex(randomblob(2020)), 'just under');
INSERT INTO big (body, tail) VALUES (hex(randomblob(2040)), 'just over');
INSERT INTO big (body, tail) VALUES (hex(randomblob(5000)), 'a few pages');
INSERT INTO big (body, tail) VALUES (hex(randomblob(100000)), 'a long chain');
INSERT INTO big (body, tail) VALUES ('small again', 'fits');
"""

# a database file and a query to run on it
type Case = tuple[str, str]

# the output must match sqlite3's
SAME: list[Case] = [
    ("sample.db",      ".tables"),
    ("superheroes.db", ".tables"),
    ("companies.db",   ".tables"),
    ("sample.db",      "SELECT COUNT(*) FROM apples"),
    ("sample.db",      "SELECT count(*) FROM oranges"),
    ("sample.db",      "SELECT COUNT(color) FROM apples"),
    ("sample.db",      "SELECT name FROM apples"),
    ("sample.db",      "SELECT name, color FROM apples"),
    ("sample.db",      "SELECT * FROM apples"),
    ("sample.db",      "SELECT id, name FROM apples"),
    ("sample.db",      "SELECT name, color FROM apples WHERE color = 'Yellow'"),
    ("sample.db",      "SELECT name FROM apples WHERE id = 3"),
    ("sample.db",      "SELECT name FROM apples WHERE color = 'nothing'"),
    ("sample.db",      "SELECT name FROM apples WHERE id > 2"),
    ("sample.db",      "SELECT name FROM apples WHERE 2 < id"),
    ("sample.db",      "SELECT name FROM apples WHERE id <= 2"),
    ("sample.db",      "SELECT name FROM apples WHERE id >= 2"),
    ("sample.db",      "SELECT name FROM apples WHERE NOT NOT id = 3"),
    ("sample.db",      "SELECT name FROM apples WHERE id != 3"),
    ("sample.db",      "SELECT name FROM apples WHERE id <> 3"),
    ("sample.db",      "SELECT name FROM apples WHERE id == 3"),
    ("sample.db",      "SELECT name FROM apples WHERE name > 'G'"),
    ("sample.db",      "SELECT name FROM apples WHERE 'G' < name"),
    ("sample.db",      "SELECT name FROM apples WHERE color = NULL"),
    ("sample.db",      "SELECT name FROM apples WHERE color != null"),
    ("sample.db",      "SELECT COUNT(*) FROM apples WHERE NULL < id"),
    ("sample.db",      "SELECT name FROM apples WHERE id = color"),
    ("sample.db",      "SELECT name FROM apples WHERE name = name"),
    ("sample.db",      "SELECT name FROM apples WHERE 3 = 3"),
    ("sample.db",      "SELECT COUNT(*) FROM apples WHERE 1 > 2"),
    ("sample.db",      "SELECT name FROM apples WHERE id > 1 AND id < 4"),
    ("sample.db",      "SELECT name FROM apples WHERE id > 1 and color = NULL"),
    ("sample.db",      "SELECT COUNT(*) FROM apples WHERE id > 1 AND id < 4 AND name > 'G'"),
    ("sample.db",      "SELECT name FROM apples WHERE id = 1 OR id = 3"),
    ("sample.db",      "SELECT name FROM apples WHERE id = 1 or color = NULL"),
    ("sample.db",      "SELECT name FROM apples WHERE id = 1 AND id = 2 OR id = 3"),
    ("sample.db",      "SELECT name FROM apples WHERE id = 3 OR id = 1 AND id = 2"),
    ("sample.db",      "SELECT name FROM apples WHERE NOT id = 3"),
    ("sample.db",      "SELECT name FROM apples WHERE NOT id > 2"),
    ("sample.db",      "SELECT name FROM apples WHERE NOT id = 1 AND id < 4"),
    ("sample.db",      "SELECT name FROM apples WHERE NOT id = 1 OR id = 3"),
    ("sample.db",      "SELECT COUNT(*) FROM apples WHERE NOT NULL = 3"),
    ("sample.db",      "SELECT COUNT(*) FROM apples WHERE NOT 1 = 2"),
    ("sample.db",      "SELECT name FROM apples WHERE (id = 3)"),
    ("sample.db",      "SELECT name FROM apples WHERE (id = 1 OR id = 3) AND id < 3"),
    ("sample.db",      "SELECT name FROM apples WHERE id = 1 AND (id = 2 OR id = 3)"),
    ("sample.db",      "SELECT name FROM apples WHERE (id = 1 AND id = 2) OR id = 3"),
    ("sample.db",      "SELECT name FROM apples WHERE NOT (id = 3)"),
    ("sample.db",      "SELECT name FROM apples WHERE ((id = 3))"),
    ("superheroes.db", "SELECT COUNT(*) FROM superheroes WHERE eye_color = 'Pink Eyes' OR hair_color = 'Pink Hair' OR id < 10"),
    ("superheroes.db", "SELECT name FROM superheroes WHERE eye_color = 'Blue Eyes' AND hair_color = 'Blond Hair' AND first_appearance_year > '2000'"),
    ("superheroes.db", "SELECT COUNT(*) FROM superheroes"),
    ("superheroes.db", "SELECT COUNT(*) FROM superheroes WHERE eye_color IS NULL"),
    ("superheroes.db", "SELECT COUNT(*) FROM superheroes WHERE hair_color is not null"),
    ("superheroes.db", "SELECT COUNT(*) FROM superheroes WHERE NOT first_appearance_year IS NULL"),
    ("superheroes.db", "SELECT name FROM superheroes WHERE first_appearance_year IS NULL AND eye_color IS NOT NULL AND id < 2000"),
    ("superheroes.db", "SELECT COUNT(*) FROM superheroes WHERE (eye_color = 'Blue Eyes') IS NULL"),
    ("sample.db",      "SELECT name FROM apples WHERE NULL IS NULL"),
    ("sample.db",      "SELECT name FROM apples WHERE id IS NULL"),
    ("superheroes.db", "SELECT id, name FROM superheroes WHERE eye_color = 'Pink Eyes'"),
    ("superheroes.db", "SELECT name, hair_color FROM superheroes WHERE first_appearance_year = '1990'"),
    ("superheroes.db", "SELECT COUNT(*) FROM superheroes WHERE hair_color != 'Black Hair'"),
    ("superheroes.db", "SELECT COUNT(*) FROM superheroes WHERE first_appearance_year > '1990'"),
    ("superheroes.db", "SELECT COUNT(*) FROM superheroes WHERE '1990' < first_appearance_year"),
    (OVERFLOW_DB,      "SELECT COUNT(*) FROM big"),
    (OVERFLOW_DB,      "SELECT id, tail FROM big"),
    (OVERFLOW_DB,      "SELECT id FROM big WHERE tail = 'a long chain'"),
    (OVERFLOW_DB,      "SELECT body FROM big WHERE id = 2"),
    (OVERFLOW_DB,      "SELECT body FROM big WHERE id = 3"),
    (OVERFLOW_DB,      "SELECT body FROM big WHERE id = 5"),
    ("companies.db",   "SELECT COUNT(*) FROM companies"),
    ("companies.db",   "SELECT id, name FROM companies WHERE country = 'eritrea'"),
    ("companies.db",   "SELECT id, name FROM companies WHERE country = 'tonga'"),
]

# must fail with a nonzero exit
FAILS: list[Case] = [
    ("sample.db", "SELECT name FROM nowhere"),
    ("sample.db", "SELECT nothing FROM apples"),
    ("sample.db", "SELECT name apples"),
    ("sample.db", "SELECT name FROM apples WHERE"),
    ("sample.db", "SELECT name, FROM apples"),
    ("sample.db", "SELECT name FROM apples WHERE id ="),
    ("sample.db", "SELECT name FROM apples WHERE id 3"),
    ("sample.db", "SELECT name FROM apples WHERE = 3"),
    ("sample.db", "SELECT name FROM apples WHERE id = 3 3"),
    ("sample.db", "SELECT name FROM apples WHERE id = 3 = 3"),
    ("sample.db", "SELECT name FROM apples WHERE id ! 3"),
    ("sample.db", "SELECT name FROM apples WHERE nothing = 3"),
    ("sample.db", "SELECT name FROM apples WHERE 3 = nothing"),
    ("sample.db", "SELECT name FROM apples WHERE id > 1 AND"),
    ("sample.db", "SELECT name FROM apples WHERE AND id > 1"),
    ("sample.db", "SELECT name FROM apples WHERE id > 1 AND nothing = 2"),
    ("sample.db", "SELECT name FROM apples WHERE id = 1 OR"),
    ("sample.db", "SELECT name FROM apples WHERE id = 1 OR AND id = 2"),
    ("sample.db", "SELECT name FROM apples WHERE NOT"),
    ("sample.db", "SELECT name FROM apples WHERE NOT id ="),
    ("sample.db", "SELECT name FROM apples WHERE NOT AND id = 1"),
    ("sample.db", "SELECT name FROM apples WHERE id = 1 AND NOT"),
    ("sample.db", "SELECT name FROM apples WHERE (id = 3"),
    ("sample.db", "SELECT name FROM apples WHERE id = 3)"),
    ("sample.db", "SELECT name FROM apples WHERE ()"),
    ("sample.db", "SELECT name FROM apples WHERE (id = 3))"),
    ("sample.db", "SELECT name FROM apples WHERE color IS"),
    ("sample.db", "SELECT name FROM apples WHERE color IS NOT"),
    ("sample.db", "SELECT name FROM apples WHERE color IS 3"),
    ("sample.db", ".nonsense"),
]


def run(args: list[str | Path], input: str | None = None) -> tuple[int, list[str]]:
    """runs a program, giving (exit code, output lines with stderr mixed in)"""
    # check=False: a nonzero exit is a result to compare, not an error
    result = subprocess.run(args, check=False, input=input, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors="replace")
    return result.returncode, result.stdout.splitlines()


def build() -> Path | None:
    """builds ours and gives the program's path, or None if the build failed"""
    configure: list[str | Path]
    if WINDOWS:
        build_dir = REPO / "build"
        configure = ["cmake", "-G", "MinGW Makefiles", "-S", ".", "-B", build_dir]
    else:
        build_dir = Path.home() / ("sqlite-sanitize" if SANITIZE else "sqlite-build")
        configure = ["cmake", "-S", ".", "-B", build_dir, "-DCMAKE_BUILD_TYPE=Debug",
                     "-DCMAKE_C_FLAGS=" + (SANITIZER_FLAGS if SANITIZE else "")]
    # configuring again is cheap, and picks up a change of flags
    if not WINDOWS or not build_dir.exists():
        code, output = run(configure)
        if code:
            print("\n".join(output[-10:]))
            return None
    code, output = run(["cmake", "--build", build_dir])
    if code:
        print("\n".join([line for line in output if "error" in line][:10]))
        return None
    return build_dir / ("sqlite.exe" if WINDOWS else "sqlite")


def sanitizer_report(output: list[str], db: str, sql: str) -> bool:
    """true, after printing the report's first lines, if a sanitizer reported an error"""
    if not any(re.search("Sanitizer|runtime error", line) for line in output):
        return False
    print(f"SANITIZER  {db}  {sql}")
    report = [line for line in output if re.search(r"ERROR|runtime error|SUMMARY|#[0-3] ", line)]
    for line in report[:8]:
        print("  " + line)
    return True


def words(lines: list[str]) -> list[str]:
    """the whitespace-separated words in lines, for output laid out in columns"""
    return [" ".join(" ".join(lines).split())]


def shorten(lines: list[str]) -> str:
    """the first few lines on one line, cut short so a huge row stays readable"""
    text = " | ".join(lines[:5])
    return text if len(text) <= 200 else text[:200] + "..."


def main() -> int:
    os.chdir(REPO)
    if SANITIZE and WINDOWS:
        # cwd carries over: WSL starts in the same folder, seen through /mnt/c
        wsl = ["wsl", "-e", "python3", "tests/check.py", "--sanitize"]
        return subprocess.run(wsl, check=False).returncode

    program = build()
    if program is None:
        print("BUILD FAILED")
        return 1

    Path(OVERFLOW_DB).unlink(missing_ok=True)
    code, output = run(["sqlite3", OVERFLOW_DB], input=OVERFLOW_SQL)
    if code:
        print("\n".join(output[:10]))
        print("COULD NOT MAKE " + OVERFLOW_DB)
        return 1

    failed = 0
    for db, sql in SAME:
        _, ours = run([program, db, sql])
        if sanitizer_report(ours, db, sql):
            failed += 1
            continue
        _, theirs = run(["sqlite3", db, sql])
        # sqlite3 pads .tables into columns; only the names matter
        if sql == ".tables":
            ours, theirs = words(ours), words(theirs)
        if ours != theirs:
            failed += 1
            print(f"DIFF  {db}  {sql}")
            print(f"  ours:    {shorten(ours)}")
            print(f"  sqlite3: {shorten(theirs)}")

    for db, sql in FAILS:
        code, ours = run([program, db, sql])
        if sanitizer_report(ours, db, sql):
            failed += 1
            continue
        if code == 0:
            failed += 1
            print(f"SHOULD FAIL  {db}  {sql}")

    total = len(SAME) + len(FAILS)
    print(f"{total - failed}/{total} passed")
    return int(failed > 0)


if __name__ == "__main__":
    sys.exit(main())
