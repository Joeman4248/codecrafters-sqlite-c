#include <stdio.h>

#include "sql/sql_parser.h"
#include "sql/tokenizer.h"

bool find_column(const TableColumns* columns, const Token name, uint32_t* index) {
    // an empty name, such as the zeroed token of a missing WHERE, matches no
    // column: every column name has at least one character
    for (uint32_t i = 0; i < columns->names.count; i++) {
        if (sv_equal_nocase(columns->names.items[i].text, name.text)) {
            *index = i;
            return true;
        }
    }
    return false;
}

int parse_statement(const char* sql, Statement* statement) {
    // TODO: the new parser
    statement->type = STATEMENT_TYPE_UNKNOWN;
    fprintf(stderr, "the parser is being rewritten: %s\n", sql);
    return 1;
}

void free_statement(Statement* statement) {
    switch (statement->type) {
        case STATEMENT_TYPE_SELECT:         free_token_list(&statement->select.names); break;
        case STATEMENT_TYPE_CREATE_TABLE:   free_token_list(&statement->create_table.names); break;
        case STATEMENT_TYPE_UNKNOWN:        break;
    }
}
