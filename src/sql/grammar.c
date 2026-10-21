#include "sql/grammar.h"

bool match_rule(Parser* parser, const Rule* rule) {
    bool match = false;
    switch (rule->type) {
        case RULE_TOKEN: {
            // is the token at parser->pos of type rule->token?
            // if so, move pos forward one and return true; otherwise return false
            const Token token = parser->tokens->items[parser->pos];
            match = token.type == rule->token_type;
            if (match) parser->pos++;
            break;
        }
        case RULE_SEQ: {
            // remember where pos started
            // call match_rule on each child, rule->children[0] up to rule->count
            // if any child fails, put pos back to where it started and return false
            // if they all match, return true
            const uint32_t start = parser->pos;
            match = true;
            for (uint32_t i = 0; match && i < rule->count; i++) {
                match = match_rule(parser, rule->children[i]);
            }
            if (!match) parser->pos = start;
            break;
        }
        case RULE_CHOICE: {
            // call match_rule on each child in turn
            // return true at the first one that matches
            // if none match, return false (each failed child already put pos back)
            for (uint32_t i = 0; !match && i < rule->count; i++) {
                match = match_rule(parser, rule->children[i]);
            }
            break;
        }
    }
    return match;
}
