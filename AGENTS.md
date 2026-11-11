# SQLite in C

CodeCrafters "Build Your Own SQLite": reads real SQLite `.db` files and answers
simple SQL. C23, CMake, MinGW gcc on Windows (PowerShell). README.md has the
user-facing overview; don't re-read it for orientation.

## Build and test

- Build: `cmake --build build`. If `build/` is missing:
  `cmake -G "MinGW Makefiles" -S . -B build`.
- Test: `pwsh -NoProfile -File tests/check.ps1` builds, compares ~25 queries
  with sqlite3, and prints only differences plus `N/N passed`. Run this
  instead of ad-hoc queries; add a case there for new behaviour.
- Run one query: `./build/sqlite.exe sample.db "SELECT name FROM apples"`.
- `*.db` files are binary test data: never Read them; query them with sqlite3.

## Layout (src/, includes written as "folder/name.h")

Layers depend downwards only: util <- pager <- btree <- record <- query, and util <- sql <- query.
No two-way dependencies. (Exception for now: database.h in pager/ holds the schema types.)

- `util/` view.h (StringView/ByteView, not null-terminated), value.c (tagged-union Value, value_equal, value_print),
  reader.c (byte reader, sticky `failed`)
- `pager/` database.h (Database, Page), file_header.c, pager.c (page cache, in progress)
- `btree/` btree_page.c (page header, cells), btree_cursor.c (table row walker)
- `record/` record.c (records/serial types), schema.c
- `sql/` tokenizer.c, parser.c (token helpers: check/match/expect), sql_parser.c (one parse_*
  per statement rule, filling Statement), expr.c (WHERE expression trees)
- `query/` plan.c (schema lookup, column positions), rows.c (where rows come from: TableScan),
  execute.c (evaluates WHERE, prints rows), commands.c (runs .dbinfo/.tables and SQL statements)
- `main.c` argument handling

Header comments state the contract; read the .h before the .c.

## C style (follow exactly)

- `}` then `else` on its own line; never `} else {`.
- Bare error checks: `if (f(...)) return 1;`, not `!= 0`. Functions that can fail return int, nonzero on error.
- No goto: split into a helper and do cleanup once in the caller.
- No negative sentinels. A function that returns a value but can fail or find nothing signals it with:
  - a field that is never 0/NULL when valid, if the struct has one (`Page.data == NULL`,
    `FileHeader.page_size == 0`); say which field in the .h;
  - one past the end for a position in a list (`sv_find`, `find_column` return the count);
  - `FOO_UNKNOWN` for an enum (`comparison_op` returns `COMPARE_UNKNOWN`);
  - NULL for a pointer (`parse_expr` returns no tree);
  - otherwise a bool return with an out-parameter (`integer_literal`).
- Enums end with `FOO_COUNT` and `FOO_UNKNOWN = FOO_COUNT`, except ones that mirror fixed
  on-disk values (`PageType`, `TextEncoding`). Switches over an enum list every case, including
  `FOO_UNKNOWN`, with no `default:`, so the compiler flags a missing one. Exception: a switch on input (a byte from the file, a token
  from the query) uses `default:` for the "anything else" case, since any value can arrive.
- Byte readers (`read_u16(Reader*)` etc.) return the value and set the sticky `reader->failed`;
  check it once after a group. One read per statement (call order inside an initializer is unspecified).
- Contract comments in .h; .c comments only for the non-obvious, never repeating the header.
- Match the surrounding code's naming and comment density.

## Git

- Main line is `master`. `index-lookup` is a reference branch only: never merge it.
- Commit messages: lowercase subject, then a body explaining why. Use a PowerShell here-string:
  `git commit -m @'...'@`.
- Commit once it builds and `tests/check.ps1` passes. Pushing (to GitHub or `codecrafters submit`) needs the user's go-ahead.

## Known gaps

- No type affinity: `WHERE text_column = 1990` doesn't match the text '1990' (sqlite3 does).
- WHERE is one comparison of two columns or literals (an `Expr` tree from expr.c); no
  AND/OR, parentheses, NOT or IS NULL yet; no overflow pages; no index lookups yet.
