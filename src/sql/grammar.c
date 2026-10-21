#include "sql/grammar.h"

// records that token matched a rule tagged tag
static void add_capture(Parser* parser, const enum CaptureTag tag, const Token token) {
    parser->captures[parser->capture_count++] = (Capture) { tag, token };
}

// how far matching had got, so a failed attempt can go back to it. Holds only
// what a failed attempt undoes.
typedef struct {
    uint32_t pos;
    uint32_t capture_count;
} Mark;

static Mark save_mark(const Parser* parser) {
    return (Mark) { parser->pos, parser->capture_count };
}

static void restore_mark(Parser* parser, const Mark mark) {
    parser->pos = mark.pos;
    parser->capture_count = mark.capture_count;
}

bool match_rule(Parser* parser, const Rule* rule) {
    bool match = false;
    switch (rule->type) {
        case RULE_TOKEN: {
            const Token token = parser->tokens->items[parser->pos];
            match = token.type == rule->token_type;
            if (match) {
                parser->pos++;
                if (rule->tag != TAG_NONE) add_capture(parser, rule->tag, token);
            }
            break;
        }
        case RULE_SEQ: {
            const Mark start = save_mark(parser);
            match = true;
            for (uint32_t i = 0; match && i < rule->count; i++) {
                match = match_rule(parser, rule->children[i]);
            }
            if (!match) restore_mark(parser, start);
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
        case RULE_REPEAT: {
            // zero times is a match too, so this never fails. It also stops
            // when a pass matches without consuming a token, which would
            // otherwise repeat forever.
            match = true;
            while (true) {
                const uint32_t before = parser->pos;
                if (!match_rule(parser, rule->children[0]) || parser->pos == before) break;
            }
            break;
        }
    }
    return match;
}
