[![progress-banner](https://backend.codecrafters.io/progress/sqlite/9c322c10-fb9a-47d1-8e55-7e420221ce37)](https://app.codecrafters.io/users/Joeman4248?r=2qF)

# SQLite in C

A small SQLite written from scratch in C, built through the
["Build Your Own SQLite" challenge](https://codecrafters.io/challenges/sqlite)
on CodeCrafters. It reads real `.db` files made by SQLite: it parses the file
format by hand, walks the B-trees page by page, and answers simple SQL
queries, with no SQLite library involved.

```sh
$ ./your_program.sh sample.db "SELECT name, color FROM apples"
Granny Smith|Light Green
Fuji|Red
Honeycrisp|Blush Red
Golden Delicious|Yellow

$ ./your_program.sh companies.db "SELECT id, name FROM companies WHERE country = 'eritrea'"
121311|unilink s.c.
2102438|orange asmara it solutions
5729848|zara mining share company
6634629|asmara rental
```

## What it can do

| Command | Example |
|---|---|
| Database info | `.dbinfo` |
| List tables | `.tables` |
| Count rows | `SELECT COUNT(*) FROM apples` |
| Count non-NULL values | `SELECT COUNT(color) FROM apples` |
| Read columns | `SELECT name, color FROM apples` or `SELECT * FROM apples` |
| Filter rows | `SELECT id, name FROM apples WHERE color = 'Yellow'` or `WHERE id = 3` |
| Compare | `WHERE id > 2`, `WHERE 'G' < name`, `WHERE id != 3`: any of `= == < <= > >= != <>` |
| Combine conditions | `WHERE id > 1 AND id < 4`, `WHERE color = 'Red' OR id = 3` |

Tables can span many pages: the B-tree is walked through interior and leaf
pages, so the multi-megabyte sample databases work the same as the tiny one.
The schema is read the same way, so a database with many tables works too.
Values of every SQLite type are read and printed: integers, floats, text,
blobs and NULL, plus the `INTEGER PRIMARY KEY` column, which SQLite stores as
the row's rowid rather than in the row itself.

A `WHERE` compares columns, strings, integers and `NULL` the way SQLite does:
`AND` binds tighter than `OR`, and a comparison with `NULL` is neither true nor
false, so `WHERE color = NULL` matches nothing.

Not supported yet:

- `NOT`, parentheses and `IS NULL` in a `WHERE`, and type affinity: SQLite
  matches `WHERE year = 1990` against the text `'1990'`, but this doesn't.
- Rows too big for one page (overflow pages), and indexes.

## How it works

The code under [src/](src/) is grouped by layer:

| Folder | What it does |
|---|---|
| [sql/](src/sql/) | the SQL language: tokenizing and parsing |
| [query/](src/query/) | running commands: the dot commands, and planning and executing SQL |
| [record/](src/record/) | SQLite's row format, and the schema table built from it |
| [btree/](src/btree/) | B-tree pages and a cursor over a table's rows |
| [pager/](src/pager/) | the database file: its header, and a cache of the pages read from it |
| [util/](src/util/) | small shared types: string and byte views, values, and a byte reader |

A query goes through four stages:

1. **Tokenize** ([tokenizer.c](src/sql/tokenizer.c)): split the SQL text into
   tokens such as `SELECT`, `apples` and `'Yellow'`.
2. **Parse** ([sql_parser.c](src/sql/sql_parser.c)): turn the tokens into a
   statement: which table, which columns, what filter. It's a hand-written
   recursive-descent parser: each grammar rule is one `parse_*` function,
   with the rule in its comment, reading tokens with the small helpers in
   [parser.c](src/sql/parser.c). A `WHERE` becomes an expression tree
   ([expr.c](src/sql/expr.c)), one function per level of precedence.
3. **Plan** ([plan.c](src/query/plan.c)): look the table up in the schema, find
   the page its B-tree starts on, and work out where each column sits in a row,
   including every column the `WHERE` tree names.
4. **Execute** ([execute.c](src/query/execute.c)): walk the table's rows
   ([rows.c](src/query/rows.c), with a B-tree cursor), work out the `WHERE`
   tree's value for each row, and print the rows it's true for.

Underneath, these read the file itself:

- [pager.c](src/pager/pager.c) and [pcache.c](src/pager/pcache.c): every page
  comes from the pager, which reads it from disk the first time and keeps it in
  a hash table after that, the way SQLite's own pager does. Callers pin a page
  while they use it and release it when done; a page still pinned when the
  database closes is reported and fails the run, so leaks show up in testing.
- [file_header.c](src/pager/file_header.c) and [schema.c](src/record/schema.c):
  the file header and the `sqlite_schema` table that lists every table and index.
- [btree_page.c](src/btree/btree_page.c) and [btree_cursor.c](src/btree/btree_cursor.c):
  B-tree pages and a cursor that steps through a table's rows in order.
- [record.c](src/record/record.c) and [reader.c](src/util/reader.c): SQLite's
  row format (a header saying each column's type, then the values), and a
  reader for its big-endian integers, floats and variable-length integers
  (varints).
- [value.c](src/util/value.c): a `Value` holds one value of any of SQLite's five
  types, tagged with which one it is. Records produce them, and printing and
  `WHERE` work on them, so neither needs to know how values are stored.

## In progress

- **A bounded page cache**: growing the hash table as pages are added, then
  capping how many pages stay in memory and evicting ones nobody has pinned.
- **Index lookups**: using an index such as `idx_companies_country` to jump
  straight to matching rows instead of scanning the whole table. The parser
  already reads `CREATE INDEX` statements, so the next step is finding an
  index in the schema that fits a query.
- **The rest of `WHERE`**: `NOT`, parentheses and `IS [NOT] NULL`, each one
  more level in the expression parser.

## Running it

You need `cmake` and a C compiler. Then:

```sh
./your_program.sh <database> <command>
```

`sample.db` is in the repo. The two larger databases, `superheroes.db` (~1MB)
and `companies.db` (~7MB), can be downloaded with:

```sh
./download_sample_databases.sh
```

or directly from
[codecrafters-io/sample-sqlite-databases](https://github.com/codecrafters-io/sample-sqlite-databases).
To compare answers with the real thing, run the same query through `sqlite3`:

```sh
sqlite3 sample.db "SELECT name, color FROM apples"
```

[tests/check.ps1](tests/check.ps1) does that for a set of queries: it builds
the program, runs each query through both, and prints only the ones whose
output differs (it needs PowerShell and `sqlite3`).
