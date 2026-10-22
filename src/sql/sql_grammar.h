#ifndef CODECRAFTERS_SQLITE_SQL_GRAMMAR_H
#define CODECRAFTERS_SQLITE_SQL_GRAMMAR_H

#include "sql/grammar.h"
#include "sql/sql_parser.h"     // for enum StatementType

// The SQL grammar, written out in sql_grammar.c. It lives apart from the parser
// so that editing the parser doesn't make the editor re-expand the grammar's
// macros.

// each statement type's grammar, indexed by its StatementType; NULL if it has
// none yet
extern const Rule* const STATEMENT_RULES[STATEMENT_TYPE_COUNT];

#endif // CODECRAFTERS_SQLITE_SQL_GRAMMAR_H
