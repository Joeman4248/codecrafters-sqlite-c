# Builds the engine, runs each query below through it and through sqlite3, and
# prints only the queries whose output differs, then a one-line summary. Run
# from the repo root: pwsh tests/check.ps1
#
# Needs sqlite3 on the PATH and the sample databases (download_sample_databases.sh).
# Row order is compared as given: both walk tables in rowid order.

$ErrorActionPreference = "Stop"

$build = cmake --build build 2>&1
if ($LASTEXITCODE) {
    $build | Select-String "error" | Select-Object -First 10
    Write-Output "BUILD FAILED"
    exit 1
}

# [database, query]: the output must match sqlite3's
$same = @(
    @("sample.db",      ".tables"),
    @("superheroes.db", ".tables"),
    @("companies.db",   ".tables"),
    @("sample.db",      "SELECT COUNT(*) FROM apples"),
    @("sample.db",      "SELECT count(*) FROM oranges"),
    @("sample.db",      "SELECT COUNT(color) FROM apples"),
    @("sample.db",      "SELECT name FROM apples"),
    @("sample.db",      "SELECT name, color FROM apples"),
    @("sample.db",      "SELECT * FROM apples"),
    @("sample.db",      "SELECT id, name FROM apples"),
    @("sample.db",      "SELECT name, color FROM apples WHERE color = 'Yellow'"),
    @("sample.db",      "SELECT name FROM apples WHERE id = 3"),
    @("sample.db",      "SELECT name FROM apples WHERE color = 'nothing'"),
    @("sample.db",      "SELECT name FROM apples WHERE id > 2"),
    @("sample.db",      "SELECT name FROM apples WHERE 2 < id"),
    @("sample.db",      "SELECT name FROM apples WHERE id <= 2"),
    @("sample.db",      "SELECT name FROM apples WHERE id != 3"),
    @("sample.db",      "SELECT name FROM apples WHERE id <> 3"),
    @("sample.db",      "SELECT name FROM apples WHERE id == 3"),
    @("sample.db",      "SELECT name FROM apples WHERE name > 'G'"),
    @("sample.db",      "SELECT name FROM apples WHERE 'G' < name"),
    @("sample.db",      "SELECT name FROM apples WHERE color = NULL"),
    @("sample.db",      "SELECT name FROM apples WHERE color != null"),
    @("sample.db",      "SELECT COUNT(*) FROM apples WHERE NULL < id"),
    @("sample.db",      "SELECT name FROM apples WHERE id = color"),
    @("sample.db",      "SELECT name FROM apples WHERE name = name"),
    @("sample.db",      "SELECT name FROM apples WHERE 3 = 3"),
    @("sample.db",      "SELECT COUNT(*) FROM apples WHERE 1 > 2"),
    @("superheroes.db", "SELECT COUNT(*) FROM superheroes"),
    @("superheroes.db", "SELECT id, name FROM superheroes WHERE eye_color = 'Pink Eyes'"),
    @("superheroes.db", "SELECT name, hair_color FROM superheroes WHERE first_appearance_year = '1990'"),
    @("superheroes.db", "SELECT COUNT(*) FROM superheroes WHERE hair_color != 'Black Hair'"),
    @("superheroes.db", "SELECT COUNT(*) FROM superheroes WHERE first_appearance_year > '1990'"),
    @("superheroes.db", "SELECT COUNT(*) FROM superheroes WHERE '1990' < first_appearance_year"),
    @("companies.db",   "SELECT COUNT(*) FROM companies"),
    @("companies.db",   "SELECT id, name FROM companies WHERE country = 'eritrea'"),
    @("companies.db",   "SELECT id, name FROM companies WHERE country = 'tonga'")
)

# [database, query]: must fail with a nonzero exit
$fails = @(
    @("sample.db", "SELECT name FROM nowhere"),
    @("sample.db", "SELECT nothing FROM apples"),
    @("sample.db", "SELECT name apples"),
    @("sample.db", "SELECT name FROM apples WHERE"),
    @("sample.db", "SELECT name, FROM apples"),
    @("sample.db", "SELECT name FROM apples WHERE id ="),
    @("sample.db", "SELECT name FROM apples WHERE id 3"),
    @("sample.db", "SELECT name FROM apples WHERE = 3"),
    @("sample.db", "SELECT name FROM apples WHERE id = 3 3"),
    @("sample.db", "SELECT name FROM apples WHERE id = 3 = 3"),
    @("sample.db", "SELECT name FROM apples WHERE nothing = 3"),
    @("sample.db", "SELECT name FROM apples WHERE 3 = nothing"),
    @("sample.db", ".nonsense")
)

$failed = 0
foreach ($case in $same) {
    $db, $sql = $case
    $ours   = (& ./build/sqlite.exe $db $sql 2>&1) -join "`n"
    $theirs = (& sqlite3 $db $sql 2>&1) -join "`n"
    # sqlite3 pads .tables into columns; only the names matter
    if ($sql -eq ".tables") {
        $ours   = ($ours   -split "\s+" | Where-Object { $_ }) -join " "
        $theirs = ($theirs -split "\s+" | Where-Object { $_ }) -join " "
    }
    if ($ours -ne $theirs) {
        $failed++
        Write-Output "DIFF  $db  $sql"
        Write-Output "  ours:    $(($ours   -split "`n" | Select-Object -First 5) -join ' | ')"
        Write-Output "  sqlite3: $(($theirs -split "`n" | Select-Object -First 5) -join ' | ')"
    }
}
foreach ($case in $fails) {
    $db, $sql = $case
    & ./build/sqlite.exe $db $sql 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) {
        $failed++
        Write-Output "SHOULD FAIL  $db  $sql"
    }
}

$total = $same.Count + $fails.Count
Write-Output "$($total - $failed)/$total passed"
exit [int]($failed -gt 0)
