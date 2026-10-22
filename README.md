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
| Filter rows | `SELECT id, name FROM apples WHERE color = 'Yellow'` |

Tables can span many pages: the B-tree is walked through interior and leaf
pages, so the multi-megabyte sample databases work the same as the tiny one.
Values of every SQLite type are read and printed: integers, floats, text,
blobs and NULL, plus the `INTEGER PRIMARY KEY` column, which SQLite stores as
the row's rowid rather than in the row itself.

Not supported yet:

- `WHERE` takes one `column = 'text'` comparison. Comparing against numbers
  works underneath, but the parser only reads text after `WHERE` so far.
- A whole-number float in a `REAL` column prints as `3` where `sqlite3` prints
  `3.0`: SQLite stores it as an integer and only its column's declared type
  turns it back into a float, which isn't tracked yet.
- Rows too big for one page (overflow pages), and indexes.

## How it works

The code under [src/](src/) is grouped by layer:

| Folder | What it does |
|---|---|
| [sql/](src/sql/) | the SQL language: tokenizing and parsing |
| [query/](src/query/) | running a statement: planning, executing, and the dot commands |
| [storage/](src/storage/) | SQLite's file format, from raw bytes up to tables |
| [util/](src/util/) | small shared types: string and byte views, and values |

A query goes through four stages:

1. **Tokenize** ([tokenizer.c](src/sql/tokenizer.c)): split the SQL text into
   tokens such as `SELECT`, `apples` and `'Yellow'`.
2. **Parse** ([sql_parser.c](src/sql/sql_parser.c)): turn the tokens into a
   statement: which table, which columns, what filter.
3. **Plan** ([plan.c](src/query/plan.c)): look the table up in the schema, find
   the page its B-tree starts on, and work out where each column sits in a row.
4. **Execute** ([execute.c](src/query/execute.c)): walk the table's rows with a
   B-tree cursor, read each column as a value, and print the rows that match.

Underneath, these read the file itself:

- [file_header.c](src/storage/file_header.c) and [schema.c](src/storage/schema.c):
  the file header and the `sqlite_schema` table that lists every table and index.
- [btree_page.c](src/storage/btree_page.c) and [btree_cursor.c](src/storage/btree_cursor.c):
  B-tree pages and a cursor that steps through a table's rows in order.
- [record.c](src/storage/record.c) and [reader.c](src/storage/reader.c): SQLite's
  row format (a header saying each column's type, then the values), and a
  reader for its big-endian integers, floats and variable-length integers
  (varints).
- [value.c](src/util/value.c): a `Value` holds one value of any of SQLite's five
  types, tagged with which one it is. Records produce them, and printing and
  `WHERE` work on them, so neither needs to know how values are stored.

## In progress

- **A grammar-driven parser** (the `parser-overhaul` branch): the SQL grammar
  is written as data, and one generic matcher checks queries against it,
  instead of hand-writing a parsing function for every statement. It handles
  `CREATE TABLE` and single-column `SELECT` and `COUNT` so far; multiple
  columns, `WHERE` and `CREATE INDEX` come next, and then it replaces the
  current parser.
- **Index lookups**: using an index such as `idx_companies_country` to jump
  straight to matching rows instead of scanning the whole table.

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
