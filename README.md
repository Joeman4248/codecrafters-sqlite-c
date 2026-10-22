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
| Read columns | `SELECT name, color FROM apples` |
| Filter rows | `SELECT id, name FROM apples WHERE color = 'Yellow'` |

Tables can span many pages: the B-tree is walked through interior and leaf
pages, so the multi-megabyte sample databases work the same as the tiny one.
`WHERE` supports one `column = 'text'` comparison for now.

## How it works

A query goes through four stages, each in its own file under [lib/](lib/):

1. **Tokenize** ([tokenizer.c](lib/tokenizer.c)): split the SQL text into
   tokens such as `SELECT`, `apples` and `'Yellow'`.
2. **Parse** ([sql_parser.c](lib/sql_parser.c)): turn the tokens into a
   statement: which table, which columns, what filter.
3. **Plan** ([plan.c](lib/plan.c)): look the table up in the schema, find the
   page its B-tree starts on, and work out where each column sits in a row.
4. **Execute** ([execute.c](lib/execute.c)): walk the table's rows with a
   B-tree cursor and print the ones that match.

Underneath, these read the file itself:

- [file_header.c](lib/file_header.c) and [schema.c](lib/schema.c): the file
  header and the `sqlite_schema` table that lists every table and index.
- [btree_page.c](lib/btree_page.c) and [btree_cursor.c](lib/btree_cursor.c):
  B-tree pages and a cursor that steps through a table's rows in order.
- [record.c](lib/record.c) and [reader.c](lib/reader.c): SQLite's row format,
  and a reader for its big-endian integers, floats and variable-length
  integers (varints).

## In progress

- **A grammar-driven parser** (the `parser-overhaul` branch): the SQL grammar
  is written as data, and one generic matcher checks queries against it,
  instead of hand-writing a parsing function for every statement.
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
