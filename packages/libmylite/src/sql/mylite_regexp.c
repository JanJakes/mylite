#include "mylite_regexp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

// NOLINTBEGIN(misc-no-recursion)

enum {
    MYLITE_REGEXP_ERROR_ILLEGAL_ARGUMENT = 3685,
    MYLITE_REGEXP_ERROR_CODE = 3691,
    regexp_decimal_radix = 10,
};

enum mylite_regexp_node_kind {
    MYLITE_REGEXP_NODE_EMPTY = 0,
    MYLITE_REGEXP_NODE_SEQUENCE = 1,
    MYLITE_REGEXP_NODE_ALTERNATION = 2,
    MYLITE_REGEXP_NODE_REPEAT = 3,
    MYLITE_REGEXP_NODE_LITERAL = 4,
    MYLITE_REGEXP_NODE_DOT = 5,
    MYLITE_REGEXP_NODE_CLASS = 6,
    MYLITE_REGEXP_NODE_ANCHOR_START = 7,
    MYLITE_REGEXP_NODE_ANCHOR_END = 8,
};

enum mylite_regexp_class_item_kind {
    MYLITE_REGEXP_CLASS_LITERAL = 0,
    MYLITE_REGEXP_CLASS_RANGE = 1,
    MYLITE_REGEXP_CLASS_DIGIT = 2,
    MYLITE_REGEXP_CLASS_NOT_DIGIT = 3,
    MYLITE_REGEXP_CLASS_WORD = 4,
    MYLITE_REGEXP_CLASS_NOT_WORD = 5,
    MYLITE_REGEXP_CLASS_SPACE = 6,
    MYLITE_REGEXP_CLASS_NOT_SPACE = 7,
    MYLITE_REGEXP_CLASS_ALPHA = 8,
    MYLITE_REGEXP_CLASS_ALNUM = 9,
    MYLITE_REGEXP_CLASS_LOWER = 10,
    MYLITE_REGEXP_CLASS_UPPER = 11,
    MYLITE_REGEXP_CLASS_XDIGIT = 12,
    MYLITE_REGEXP_CLASS_BLANK = 13,
};

struct mylite_regexp_node;

struct mylite_regexp_node_list {
    struct mylite_regexp_node **items;
    size_t count;
    size_t capacity;
};

struct mylite_regexp_class_item {
    enum mylite_regexp_class_item_kind kind;
    unsigned char first;
    unsigned char last;
};

struct mylite_regexp_class {
    struct mylite_regexp_class_item *items;
    size_t count;
    size_t capacity;
    bool negated;
};

struct mylite_regexp_repeat {
    struct mylite_regexp_node *child;
    size_t minimum;
    size_t maximum;
};

struct mylite_regexp_repeat_bounds {
    size_t minimum;
    size_t maximum;
    bool matched;
};

struct mylite_regexp_node {
    enum mylite_regexp_node_kind kind;
    struct mylite_regexp_node_list children;
    struct mylite_regexp_class character_class;
    struct mylite_regexp_repeat repeat;
    unsigned char literal;
};

struct mylite_regexp_parser {
    const char *pattern;
    size_t length;
    size_t offset;
    unsigned int error_code;
    const char *error_message;
};

struct mylite_regexp_match_context {
    const char *value;
    size_t length;
    struct mylite_regexp_options options;
};

struct mylite_regexp_position_list {
    size_t *items;
    size_t count;
    size_t capacity;
};

struct mylite_regexp_repeat_match_state {
    size_t position;
    size_t count;
};

static struct mylite_regexp_node *parse_regexp_pattern(struct mylite_regexp_parser *parser);
static struct mylite_regexp_node *parse_regexp_alternation(struct mylite_regexp_parser *parser,
                                                           bool stop_at_right_parenthesis);
static struct mylite_regexp_node *parse_regexp_sequence(struct mylite_regexp_parser *parser,
                                                        bool stop_at_right_parenthesis);
static struct mylite_regexp_node *parse_regexp_repeat(struct mylite_regexp_parser *parser);
static struct mylite_regexp_node *parse_regexp_atom(struct mylite_regexp_parser *parser);
static struct mylite_regexp_node *parse_regexp_group(struct mylite_regexp_parser *parser);
static struct mylite_regexp_node *parse_regexp_escape(struct mylite_regexp_parser *parser);
static struct mylite_regexp_node *parse_regexp_class(struct mylite_regexp_parser *parser);
static bool parse_regexp_repeat_bounds(struct mylite_regexp_parser *parser,
                                       struct mylite_regexp_repeat_bounds *out_bounds);
static bool parse_regexp_count(struct mylite_regexp_parser *parser, size_t *out_count);
static bool parse_regexp_class_item(struct mylite_regexp_parser *parser,
                                    struct mylite_regexp_class_item *out_item);
static bool parse_regexp_class_escape(struct mylite_regexp_parser *parser,
                                      struct mylite_regexp_class_item *out_item);
static bool parse_regexp_posix_class(struct mylite_regexp_parser *parser,
                                     struct mylite_regexp_class_item *out_item, bool *out_matched);
static bool regexp_posix_class_item(const char *name, size_t name_length,
                                    struct mylite_regexp_class_item *out_item);
static struct mylite_regexp_node *regexp_make_empty(void);
static struct mylite_regexp_node *regexp_make_node(enum mylite_regexp_node_kind kind);
static struct mylite_regexp_node *regexp_make_literal(unsigned char literal);
static struct mylite_regexp_node *
regexp_make_class_escape(enum mylite_regexp_class_item_kind item_kind);
static struct mylite_regexp_node *regexp_make_repeat(struct mylite_regexp_node *child,
                                                     size_t minimum, size_t maximum);
static bool regexp_node_list_append(struct mylite_regexp_node_list *list,
                                    struct mylite_regexp_node *node);
static bool regexp_class_append(struct mylite_regexp_class *class_value,
                                struct mylite_regexp_class_item item);
static bool regexp_class_item_is_literal(struct mylite_regexp_class_item item);
static bool regexp_pattern_at_end(const struct mylite_regexp_parser *parser);
static unsigned char regexp_pattern_current(const struct mylite_regexp_parser *parser);
static bool regexp_pattern_has_next(const struct mylite_regexp_parser *parser);
static unsigned char regexp_pattern_next(const struct mylite_regexp_parser *parser);
static void regexp_parser_set_error(struct mylite_regexp_parser *parser, unsigned int code,
                                    const char *message);
static void regexp_node_free(struct mylite_regexp_node *node);
static int regexp_search(const struct mylite_regexp_node *node,
                         const struct mylite_regexp_match_context *context, bool *out_match);
static int regexp_search_match(const struct mylite_regexp_node *node,
                               const struct mylite_regexp_match_context *context,
                               size_t start_offset, bool *out_found,
                               struct mylite_regexp_match *out_match);
static int regexp_match_node(const struct mylite_regexp_node *node,
                             const struct mylite_regexp_match_context *context, size_t position,
                             struct mylite_regexp_position_list *out_positions);
static int regexp_match_alternation(const struct mylite_regexp_node *node,
                                    const struct mylite_regexp_match_context *context,
                                    size_t position,
                                    struct mylite_regexp_position_list *out_positions);
static int regexp_match_literal_node(const struct mylite_regexp_node *node,
                                     const struct mylite_regexp_match_context *context,
                                     size_t position,
                                     struct mylite_regexp_position_list *out_positions);
static int regexp_match_dot_node(const struct mylite_regexp_match_context *context, size_t position,
                                 struct mylite_regexp_position_list *out_positions);
static int regexp_match_class_node(const struct mylite_regexp_node *node,
                                   const struct mylite_regexp_match_context *context,
                                   size_t position,
                                   struct mylite_regexp_position_list *out_positions);
static int regexp_match_anchor_start_node(const struct mylite_regexp_match_context *context,
                                          size_t position,
                                          struct mylite_regexp_position_list *out_positions);
static int regexp_match_anchor_end_node(const struct mylite_regexp_match_context *context,
                                        size_t position,
                                        struct mylite_regexp_position_list *out_positions);
static int regexp_match_sequence(const struct mylite_regexp_node *node,
                                 const struct mylite_regexp_match_context *context,
                                 size_t child_index, size_t position,
                                 struct mylite_regexp_position_list *out_positions);
static int regexp_match_repeat_then_rest(const struct mylite_regexp_node *repeat,
                                         const struct mylite_regexp_node *sequence,
                                         const struct mylite_regexp_match_context *context,
                                         size_t next_child_index,
                                         struct mylite_regexp_repeat_match_state state,
                                         struct mylite_regexp_position_list *out_positions);
static int regexp_match_repeat_positions(const struct mylite_regexp_node *repeat,
                                         const struct mylite_regexp_match_context *context,
                                         size_t position, size_t count,
                                         struct mylite_regexp_position_list *out_positions);
static bool regexp_literal_matches(unsigned char value, unsigned char literal,
                                   struct mylite_regexp_options options);
static bool regexp_dot_matches(unsigned char value, struct mylite_regexp_options options);
static bool regexp_class_matches(const struct mylite_regexp_class *class_value, unsigned char value,
                                 struct mylite_regexp_options options);
static bool regexp_class_item_matches(struct mylite_regexp_class_item item, unsigned char value,
                                      struct mylite_regexp_options options);
static bool regexp_anchor_start_matches(size_t position,
                                        const struct mylite_regexp_match_context *context);
static bool regexp_anchor_end_matches(size_t position,
                                      const struct mylite_regexp_match_context *context);
static int regexp_append_position(struct mylite_regexp_position_list *list, size_t position);
static bool regexp_position_list_append(struct mylite_regexp_position_list *list, size_t position);
static void regexp_position_list_deinit(struct mylite_regexp_position_list *list);
static unsigned char regexp_case_fold(unsigned char value);
static bool regexp_is_digit(unsigned char value);
static bool regexp_is_alpha(unsigned char value);
static bool regexp_is_alnum(unsigned char value);
static bool regexp_is_word(unsigned char value);
static bool regexp_is_space(unsigned char value);
static bool regexp_is_lower(unsigned char value);
static bool regexp_is_upper(unsigned char value);
static bool regexp_is_xdigit(unsigned char value);
static bool regexp_is_blank(unsigned char value);
static bool regexp_ascii_name_equals(const char *name, size_t name_length, const char *expected);
static size_t regexp_cstring_length(const char *text);

int mylite_regexp_match(const char *value, size_t value_length, const char *pattern,
                        size_t pattern_length, struct mylite_regexp_options options,
                        bool *out_match, struct mylite_regexp_error *out_error)
{
    struct mylite_regexp_parser parser = {
        .pattern = pattern == NULL ? "" : pattern,
        .length = pattern == NULL ? 0U : pattern_length,
    };
    struct mylite_regexp_match_context context = {
        .value = value == NULL ? "" : value,
        .length = value == NULL ? 0U : value_length,
        .options = options,
    };
    struct mylite_regexp_node *node = NULL;
    int status = MYLITE_REGEXP_OK;

    if (out_match == NULL) {
        return MYLITE_REGEXP_NOMEM;
    }
    if (out_error != NULL) {
        *out_error = (struct mylite_regexp_error){0};
    }
    *out_match = false;

    node = parse_regexp_pattern(&parser);
    if (node == NULL) {
        if (parser.error_message != NULL) {
            if (out_error != NULL) {
                *out_error = (struct mylite_regexp_error){
                    .code = parser.error_code == 0U ? MYLITE_REGEXP_ERROR_CODE : parser.error_code,
                    .message = parser.error_message,
                };
            }
            return MYLITE_REGEXP_PATTERN_ERROR;
        }
        return MYLITE_REGEXP_NOMEM;
    }

    status = regexp_search(node, &context, out_match);
    regexp_node_free(node);
    return status;
}

int mylite_regexp_find(const char *value, size_t value_length, const char *pattern,
                       size_t pattern_length, size_t start_offset, size_t occurrence,
                       struct mylite_regexp_options options, bool *out_found,
                       struct mylite_regexp_match *out_match, struct mylite_regexp_error *out_error)
{
    struct mylite_regexp_parser parser = {
        .pattern = pattern == NULL ? "" : pattern,
        .length = pattern == NULL ? 0U : pattern_length,
    };
    struct mylite_regexp_match_context context = {
        .value = value == NULL ? "" : value,
        .length = value == NULL ? 0U : value_length,
        .options = options,
    };
    struct mylite_regexp_node *node = NULL;
    size_t search_offset = start_offset;
    size_t requested = occurrence == 0U ? 1U : occurrence;
    int status = MYLITE_REGEXP_OK;

    if (out_found == NULL || out_match == NULL) {
        return MYLITE_REGEXP_NOMEM;
    }
    if (out_error != NULL) {
        *out_error = (struct mylite_regexp_error){0};
    }
    *out_found = false;
    *out_match = (struct mylite_regexp_match){0};

    node = parse_regexp_pattern(&parser);
    if (node == NULL) {
        if (parser.error_message != NULL) {
            if (out_error != NULL) {
                *out_error = (struct mylite_regexp_error){
                    .code = parser.error_code == 0U ? MYLITE_REGEXP_ERROR_CODE : parser.error_code,
                    .message = parser.error_message,
                };
            }
            return MYLITE_REGEXP_PATTERN_ERROR;
        }
        return MYLITE_REGEXP_NOMEM;
    }

    for (size_t found_count = 0U; search_offset <= context.length;) {
        struct mylite_regexp_match match = {0};
        bool found = false;

        status = regexp_search_match(node, &context, search_offset, &found, &match);
        if (status != MYLITE_REGEXP_OK || !found) {
            break;
        }
        ++found_count;
        if (found_count >= requested) {
            *out_found = true;
            *out_match = match;
            break;
        }
        search_offset = match.end > match.start ? match.end : match.start + 1U;
    }

    regexp_node_free(node);
    return status;
}

static struct mylite_regexp_node *parse_regexp_pattern(struct mylite_regexp_parser *parser)
{
    struct mylite_regexp_node *node = NULL;

    if (parser->length == 0U) {
        regexp_parser_set_error(parser, MYLITE_REGEXP_ERROR_ILLEGAL_ARGUMENT,
                                "Illegal argument to a regular expression.");
        return NULL;
    }
    node = parse_regexp_alternation(parser, false);
    if (node == NULL) {
        return NULL;
    }
    if (!regexp_pattern_at_end(parser)) {
        regexp_parser_set_error(parser, MYLITE_REGEXP_ERROR_CODE,
                                "Mismatched parenthesis in regular expression.");
        regexp_node_free(node);
        return NULL;
    }
    return node;
}

static struct mylite_regexp_node *parse_regexp_alternation(struct mylite_regexp_parser *parser,
                                                           bool stop_at_right_parenthesis)
{
    struct mylite_regexp_node *first = parse_regexp_sequence(parser, stop_at_right_parenthesis);
    struct mylite_regexp_node *alternation = NULL;

    if (first == NULL) {
        return NULL;
    }
    if (regexp_pattern_at_end(parser) || regexp_pattern_current(parser) != '|') {
        return first;
    }

    alternation = regexp_make_node(MYLITE_REGEXP_NODE_ALTERNATION);
    if (alternation == NULL) {
        regexp_node_free(first);
        return NULL;
    }
    if (!regexp_node_list_append(&alternation->children, first)) {
        regexp_node_free(first);
        regexp_node_free(alternation);
        return NULL;
    }

    while (!regexp_pattern_at_end(parser) && regexp_pattern_current(parser) == '|') {
        struct mylite_regexp_node *branch = NULL;

        ++parser->offset;
        branch = parse_regexp_sequence(parser, stop_at_right_parenthesis);
        if (branch == NULL) {
            regexp_node_free(alternation);
            return NULL;
        }
        if (!regexp_node_list_append(&alternation->children, branch)) {
            regexp_node_free(branch);
            regexp_node_free(alternation);
            return NULL;
        }
    }
    return alternation;
}

static struct mylite_regexp_node *parse_regexp_sequence(struct mylite_regexp_parser *parser,
                                                        bool stop_at_right_parenthesis)
{
    struct mylite_regexp_node *sequence = regexp_make_node(MYLITE_REGEXP_NODE_SEQUENCE);

    if (sequence == NULL) {
        return NULL;
    }
    while (!regexp_pattern_at_end(parser)) {
        struct mylite_regexp_node *child = NULL;
        unsigned char current = regexp_pattern_current(parser);

        if (current == '|' || (stop_at_right_parenthesis && current == ')')) {
            break;
        }
        if (!stop_at_right_parenthesis && current == ')') {
            regexp_parser_set_error(parser, MYLITE_REGEXP_ERROR_CODE,
                                    "Mismatched parenthesis in regular expression.");
            regexp_node_free(sequence);
            return NULL;
        }
        child = parse_regexp_repeat(parser);
        if (child == NULL) {
            regexp_node_free(sequence);
            return NULL;
        }
        if (!regexp_node_list_append(&sequence->children, child)) {
            regexp_node_free(child);
            regexp_node_free(sequence);
            return NULL;
        }
    }
    if (sequence->children.count == 0U) {
        regexp_node_free(sequence);
        return regexp_make_empty();
    }
    if (sequence->children.count == 1U) {
        struct mylite_regexp_node *only_child = sequence->children.items[0];

        free((void *)sequence->children.items);
        free(sequence);
        return only_child;
    }
    return sequence;
}

static struct mylite_regexp_node *parse_regexp_repeat(struct mylite_regexp_parser *parser)
{
    struct mylite_regexp_node *atom = parse_regexp_atom(parser);
    struct mylite_regexp_repeat_bounds bounds = {0};
    size_t minimum = 0U;
    size_t maximum = 0U;
    bool has_quantifier = false;

    if (atom == NULL) {
        return NULL;
    }
    if (regexp_pattern_at_end(parser)) {
        return atom;
    }

    switch (regexp_pattern_current(parser)) {
    case '*':
        minimum = 0U;
        maximum = SIZE_MAX;
        has_quantifier = true;
        ++parser->offset;
        break;
    case '+':
        minimum = 1U;
        maximum = SIZE_MAX;
        has_quantifier = true;
        ++parser->offset;
        break;
    case '?':
        minimum = 0U;
        maximum = 1U;
        has_quantifier = true;
        ++parser->offset;
        break;
    case '{':
        if (!parse_regexp_repeat_bounds(parser, &bounds)) {
            regexp_node_free(atom);
            return NULL;
        }
        minimum = bounds.minimum;
        maximum = bounds.maximum;
        has_quantifier = bounds.matched;
        break;
    default:
        break;
    }

    if (!has_quantifier) {
        return atom;
    }
    if (!regexp_pattern_at_end(parser) && regexp_pattern_current(parser) == '?') {
        ++parser->offset;
    }
    return regexp_make_repeat(atom, minimum, maximum);
}

static struct mylite_regexp_node *parse_regexp_atom(struct mylite_regexp_parser *parser)
{
    unsigned char current = 0U;

    if (regexp_pattern_at_end(parser)) {
        regexp_parser_set_error(parser, MYLITE_REGEXP_ERROR_CODE,
                                "Missing regular expression operand.");
        return NULL;
    }

    current = regexp_pattern_current(parser);
    ++parser->offset;
    switch (current) {
    case '^':
        return regexp_make_node(MYLITE_REGEXP_NODE_ANCHOR_START);
    case '$':
        return regexp_make_node(MYLITE_REGEXP_NODE_ANCHOR_END);
    case '.':
        return regexp_make_node(MYLITE_REGEXP_NODE_DOT);
    case '(':
        return parse_regexp_group(parser);
    case '[':
        return parse_regexp_class(parser);
    case '\\':
        return parse_regexp_escape(parser);
    case '*':
    case '+':
    case '?':
        regexp_parser_set_error(parser, MYLITE_REGEXP_ERROR_CODE,
                                "Nothing to repeat in regular expression.");
        return NULL;
    default:
        return regexp_make_literal(current);
    }
}

static struct mylite_regexp_node *parse_regexp_group(struct mylite_regexp_parser *parser)
{
    struct mylite_regexp_node *node = parse_regexp_alternation(parser, true);

    if (node == NULL) {
        return NULL;
    }
    if (regexp_pattern_at_end(parser) || regexp_pattern_current(parser) != ')') {
        regexp_parser_set_error(parser, MYLITE_REGEXP_ERROR_CODE,
                                "Mismatched parenthesis in regular expression.");
        regexp_node_free(node);
        return NULL;
    }
    ++parser->offset;
    return node;
}

static struct mylite_regexp_node *parse_regexp_escape(struct mylite_regexp_parser *parser)
{
    unsigned char escaped = 0U;

    if (regexp_pattern_at_end(parser)) {
        regexp_parser_set_error(parser, MYLITE_REGEXP_ERROR_CODE,
                                "Trailing backslash in regular expression.");
        return NULL;
    }
    escaped = regexp_pattern_current(parser);
    ++parser->offset;

    switch (escaped) {
    case 'd':
        return regexp_make_class_escape(MYLITE_REGEXP_CLASS_DIGIT);
    case 'D':
        return regexp_make_class_escape(MYLITE_REGEXP_CLASS_NOT_DIGIT);
    case 'w':
        return regexp_make_class_escape(MYLITE_REGEXP_CLASS_WORD);
    case 'W':
        return regexp_make_class_escape(MYLITE_REGEXP_CLASS_NOT_WORD);
    case 's':
        return regexp_make_class_escape(MYLITE_REGEXP_CLASS_SPACE);
    case 'S':
        return regexp_make_class_escape(MYLITE_REGEXP_CLASS_NOT_SPACE);
    case 'n':
        return regexp_make_literal('\n');
    case 'r':
        return regexp_make_literal('\r');
    case 't':
        return regexp_make_literal('\t');
    default:
        return regexp_make_literal(escaped);
    }
}

static struct mylite_regexp_node *parse_regexp_class(struct mylite_regexp_parser *parser)
{
    struct mylite_regexp_node *node = regexp_make_node(MYLITE_REGEXP_NODE_CLASS);

    if (node == NULL) {
        return NULL;
    }
    if (!regexp_pattern_at_end(parser) && regexp_pattern_current(parser) == '^') {
        node->character_class.negated = true;
        ++parser->offset;
    }

    while (!regexp_pattern_at_end(parser)) {
        struct mylite_regexp_class_item item = {0};

        if (regexp_pattern_current(parser) == ']' && node->character_class.count > 0U) {
            ++parser->offset;
            return node;
        }
        if (!parse_regexp_class_item(parser, &item)) {
            regexp_node_free(node);
            return NULL;
        }
        if (regexp_class_item_is_literal(item) && !regexp_pattern_at_end(parser) &&
            regexp_pattern_current(parser) == '-' && regexp_pattern_has_next(parser) &&
            regexp_pattern_next(parser) != ']') {
            struct mylite_regexp_class_item last = {0};
            size_t save = parser->offset;

            ++parser->offset;
            if (!parse_regexp_class_item(parser, &last)) {
                regexp_node_free(node);
                return NULL;
            }
            if (regexp_class_item_is_literal(last)) {
                item = (struct mylite_regexp_class_item){
                    .kind = MYLITE_REGEXP_CLASS_RANGE,
                    .first = item.first,
                    .last = last.first,
                };
            } else {
                parser->offset = save;
            }
        }
        if (!regexp_class_append(&node->character_class, item)) {
            regexp_node_free(node);
            return NULL;
        }
    }

    regexp_parser_set_error(parser, MYLITE_REGEXP_ERROR_CODE,
                            "Missing closing bracket in regular expression.");
    regexp_node_free(node);
    return NULL;
}

static bool parse_regexp_repeat_bounds(struct mylite_regexp_parser *parser,
                                       struct mylite_regexp_repeat_bounds *out_bounds)
{
    size_t save = parser->offset;
    size_t minimum = 0U;
    size_t maximum = 0U;

    *out_bounds = (struct mylite_regexp_repeat_bounds){0};
    ++parser->offset;
    if (regexp_pattern_at_end(parser) || !regexp_is_digit(regexp_pattern_current(parser))) {
        parser->offset = save;
        return true;
    }
    if (!parse_regexp_count(parser, &minimum)) {
        return false;
    }
    if (!regexp_pattern_at_end(parser) && regexp_pattern_current(parser) == '}') {
        ++parser->offset;
        *out_bounds = (struct mylite_regexp_repeat_bounds){
            .minimum = minimum,
            .maximum = minimum,
            .matched = true,
        };
        return true;
    }
    if (regexp_pattern_at_end(parser) || regexp_pattern_current(parser) != ',') {
        regexp_parser_set_error(parser, MYLITE_REGEXP_ERROR_CODE,
                                "Invalid repetition count in regular expression.");
        return false;
    }
    ++parser->offset;
    if (!regexp_pattern_at_end(parser) && regexp_pattern_current(parser) == '}') {
        ++parser->offset;
        *out_bounds = (struct mylite_regexp_repeat_bounds){
            .minimum = minimum,
            .maximum = SIZE_MAX,
            .matched = true,
        };
        return true;
    }
    if (!parse_regexp_count(parser, &maximum)) {
        return false;
    }
    if (regexp_pattern_at_end(parser) || regexp_pattern_current(parser) != '}') {
        regexp_parser_set_error(parser, MYLITE_REGEXP_ERROR_CODE,
                                "Invalid repetition count in regular expression.");
        return false;
    }
    if (maximum < minimum) {
        regexp_parser_set_error(parser, MYLITE_REGEXP_ERROR_CODE,
                                "Invalid repetition count in regular expression.");
        return false;
    }
    ++parser->offset;
    *out_bounds = (struct mylite_regexp_repeat_bounds){
        .minimum = minimum,
        .maximum = maximum,
        .matched = true,
    };
    return true;
}

static bool parse_regexp_count(struct mylite_regexp_parser *parser, size_t *out_count)
{
    size_t value = 0U;
    bool saw_digit = false;

    while (!regexp_pattern_at_end(parser) && regexp_is_digit(regexp_pattern_current(parser))) {
        unsigned char digit = (unsigned char)(regexp_pattern_current(parser) - '0');

        if (value > (SIZE_MAX - digit) / regexp_decimal_radix) {
            regexp_parser_set_error(parser, MYLITE_REGEXP_ERROR_CODE,
                                    "Invalid repetition count in regular expression.");
            return false;
        }
        value = (value * regexp_decimal_radix) + digit;
        saw_digit = true;
        ++parser->offset;
    }
    if (!saw_digit) {
        regexp_parser_set_error(parser, MYLITE_REGEXP_ERROR_CODE,
                                "Invalid repetition count in regular expression.");
        return false;
    }
    *out_count = value;
    return true;
}

static bool parse_regexp_class_item(struct mylite_regexp_parser *parser,
                                    struct mylite_regexp_class_item *out_item)
{
    bool matched_posix = false;
    unsigned char current = 0U;

    if (parse_regexp_posix_class(parser, out_item, &matched_posix)) {
        if (matched_posix) {
            return true;
        }
    } else {
        return false;
    }

    if (regexp_pattern_at_end(parser)) {
        regexp_parser_set_error(parser, MYLITE_REGEXP_ERROR_CODE,
                                "Missing closing bracket in regular expression.");
        return false;
    }
    current = regexp_pattern_current(parser);
    ++parser->offset;
    if (current == '\\') {
        return parse_regexp_class_escape(parser, out_item);
    }
    *out_item = (struct mylite_regexp_class_item){
        .kind = MYLITE_REGEXP_CLASS_LITERAL,
        .first = current,
        .last = current,
    };
    return true;
}

static bool parse_regexp_class_escape(struct mylite_regexp_parser *parser,
                                      struct mylite_regexp_class_item *out_item)
{
    unsigned char escaped = 0U;

    if (regexp_pattern_at_end(parser)) {
        regexp_parser_set_error(parser, MYLITE_REGEXP_ERROR_CODE,
                                "Trailing backslash in regular expression.");
        return false;
    }
    escaped = regexp_pattern_current(parser);
    ++parser->offset;
    switch (escaped) {
    case 'd':
        out_item->kind = MYLITE_REGEXP_CLASS_DIGIT;
        return true;
    case 'D':
        out_item->kind = MYLITE_REGEXP_CLASS_NOT_DIGIT;
        return true;
    case 'w':
        out_item->kind = MYLITE_REGEXP_CLASS_WORD;
        return true;
    case 'W':
        out_item->kind = MYLITE_REGEXP_CLASS_NOT_WORD;
        return true;
    case 's':
        out_item->kind = MYLITE_REGEXP_CLASS_SPACE;
        return true;
    case 'S':
        out_item->kind = MYLITE_REGEXP_CLASS_NOT_SPACE;
        return true;
    case 'n':
        escaped = '\n';
        break;
    case 'r':
        escaped = '\r';
        break;
    case 't':
        escaped = '\t';
        break;
    default:
        break;
    }
    *out_item = (struct mylite_regexp_class_item){
        .kind = MYLITE_REGEXP_CLASS_LITERAL,
        .first = escaped,
        .last = escaped,
    };
    return true;
}

static bool parse_regexp_posix_class(struct mylite_regexp_parser *parser,
                                     struct mylite_regexp_class_item *out_item, bool *out_matched)
{
    size_t name_start = parser->offset + 2U;
    size_t name_end = name_start;

    *out_matched = false;
    if (parser->offset + 3U >= parser->length || parser->pattern[parser->offset] != '[' ||
        parser->pattern[parser->offset + 1U] != ':') {
        return true;
    }
    while (name_end + 1U < parser->length &&
           !(parser->pattern[name_end] == ':' && parser->pattern[name_end + 1U] == ']')) {
        ++name_end;
    }
    if (name_end + 1U >= parser->length) {
        regexp_parser_set_error(parser, MYLITE_REGEXP_ERROR_CODE,
                                "Invalid character class in regular expression.");
        return false;
    }
    if (!regexp_posix_class_item(parser->pattern + name_start, name_end - name_start, out_item)) {
        regexp_parser_set_error(parser, MYLITE_REGEXP_ERROR_CODE,
                                "Invalid character class in regular expression.");
        return false;
    }
    parser->offset = name_end + 2U;
    *out_matched = true;
    return true;
}

static bool regexp_posix_class_item(const char *name, size_t name_length,
                                    struct mylite_regexp_class_item *out_item)
{
    if (regexp_ascii_name_equals(name, name_length, "digit")) {
        out_item->kind = MYLITE_REGEXP_CLASS_DIGIT;
        return true;
    }
    if (regexp_ascii_name_equals(name, name_length, "alpha")) {
        out_item->kind = MYLITE_REGEXP_CLASS_ALPHA;
        return true;
    }
    if (regexp_ascii_name_equals(name, name_length, "alnum")) {
        out_item->kind = MYLITE_REGEXP_CLASS_ALNUM;
        return true;
    }
    if (regexp_ascii_name_equals(name, name_length, "space")) {
        out_item->kind = MYLITE_REGEXP_CLASS_SPACE;
        return true;
    }
    if (regexp_ascii_name_equals(name, name_length, "lower")) {
        out_item->kind = MYLITE_REGEXP_CLASS_LOWER;
        return true;
    }
    if (regexp_ascii_name_equals(name, name_length, "upper")) {
        out_item->kind = MYLITE_REGEXP_CLASS_UPPER;
        return true;
    }
    if (regexp_ascii_name_equals(name, name_length, "xdigit")) {
        out_item->kind = MYLITE_REGEXP_CLASS_XDIGIT;
        return true;
    }
    if (regexp_ascii_name_equals(name, name_length, "blank")) {
        out_item->kind = MYLITE_REGEXP_CLASS_BLANK;
        return true;
    }
    return false;
}

static struct mylite_regexp_node *regexp_make_empty(void)
{
    return regexp_make_node(MYLITE_REGEXP_NODE_EMPTY);
}

static struct mylite_regexp_node *regexp_make_node(enum mylite_regexp_node_kind kind)
{
    struct mylite_regexp_node *node = calloc(1U, sizeof(*node));

    if (node != NULL) {
        node->kind = kind;
    }
    return node;
}

static struct mylite_regexp_node *regexp_make_literal(unsigned char literal)
{
    struct mylite_regexp_node *node = regexp_make_node(MYLITE_REGEXP_NODE_LITERAL);

    if (node != NULL) {
        node->literal = literal;
    }
    return node;
}

static struct mylite_regexp_node *
regexp_make_class_escape(enum mylite_regexp_class_item_kind item_kind)
{
    struct mylite_regexp_node *node = regexp_make_node(MYLITE_REGEXP_NODE_CLASS);

    if (node == NULL) {
        return NULL;
    }
    if (!regexp_class_append(&node->character_class,
                             (struct mylite_regexp_class_item){.kind = item_kind})) {
        regexp_node_free(node);
        return NULL;
    }
    return node;
}

static struct mylite_regexp_node *regexp_make_repeat(struct mylite_regexp_node *child,
                                                     size_t minimum, size_t maximum)
{
    struct mylite_regexp_node *node = regexp_make_node(MYLITE_REGEXP_NODE_REPEAT);

    if (node == NULL) {
        regexp_node_free(child);
        return NULL;
    }
    node->repeat = (struct mylite_regexp_repeat){
        .child = child,
        .minimum = minimum,
        .maximum = maximum,
    };
    return node;
}

static bool regexp_node_list_append(struct mylite_regexp_node_list *list,
                                    struct mylite_regexp_node *node)
{
    if (list->count == list->capacity) {
        size_t new_capacity = list->capacity == 0U ? 4U : list->capacity * 2U;
        struct mylite_regexp_node **new_items = (struct mylite_regexp_node **)realloc(
            (void *)list->items, new_capacity * sizeof(*list->items));

        if (new_items == NULL) {
            return false;
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }
    list->items[list->count++] = node;
    return true;
}

static bool regexp_class_append(struct mylite_regexp_class *class_value,
                                struct mylite_regexp_class_item item)
{
    if (class_value->count == class_value->capacity) {
        size_t new_capacity = class_value->capacity == 0U ? 4U : class_value->capacity * 2U;
        struct mylite_regexp_class_item *new_items =
            realloc(class_value->items, new_capacity * sizeof(*class_value->items));

        if (new_items == NULL) {
            return false;
        }
        class_value->items = new_items;
        class_value->capacity = new_capacity;
    }
    class_value->items[class_value->count++] = item;
    return true;
}

static bool regexp_class_item_is_literal(struct mylite_regexp_class_item item)
{
    return item.kind == MYLITE_REGEXP_CLASS_LITERAL;
}

static bool regexp_pattern_at_end(const struct mylite_regexp_parser *parser)
{
    return parser->offset >= parser->length;
}

static unsigned char regexp_pattern_current(const struct mylite_regexp_parser *parser)
{
    return (unsigned char)parser->pattern[parser->offset];
}

static bool regexp_pattern_has_next(const struct mylite_regexp_parser *parser)
{
    return parser->offset + 1U < parser->length;
}

static unsigned char regexp_pattern_next(const struct mylite_regexp_parser *parser)
{
    return (unsigned char)parser->pattern[parser->offset + 1U];
}

static void regexp_parser_set_error(struct mylite_regexp_parser *parser, unsigned int code,
                                    const char *message)
{
    if (parser->error_message == NULL) {
        parser->error_code = code;
        parser->error_message = message;
    }
}

static void regexp_node_free(struct mylite_regexp_node *node)
{
    if (node == NULL) {
        return;
    }
    for (size_t index = 0U; index < node->children.count; ++index) {
        regexp_node_free(node->children.items[index]);
    }
    regexp_node_free(node->repeat.child);
    free((void *)node->children.items);
    free(node->character_class.items);
    free(node);
}

static int regexp_search(const struct mylite_regexp_node *node,
                         const struct mylite_regexp_match_context *context, bool *out_match)
{
    struct mylite_regexp_match match = {0};
    bool found = false;
    int status = regexp_search_match(node, context, 0U, &found, &match);

    if (status == MYLITE_REGEXP_OK) {
        *out_match = found;
    }
    return status;
}

static int regexp_search_match(const struct mylite_regexp_node *node,
                               const struct mylite_regexp_match_context *context,
                               size_t start_offset, bool *out_found,
                               struct mylite_regexp_match *out_match)
{
    *out_found = false;
    *out_match = (struct mylite_regexp_match){0};

    for (size_t position = start_offset; position <= context->length;) {
        struct mylite_regexp_position_list positions = {0};
        int status = regexp_match_node(node, context, position, &positions);

        if (status != MYLITE_REGEXP_OK) {
            regexp_position_list_deinit(&positions);
            return status;
        }
        if (positions.count > 0U) {
            size_t end = positions.items[0];

            regexp_position_list_deinit(&positions);
            *out_found = true;
            *out_match = (struct mylite_regexp_match){.start = position, .end = end};
            return MYLITE_REGEXP_OK;
        }
        regexp_position_list_deinit(&positions);
        if (position == context->length) {
            break;
        }
        ++position;
    }
    return MYLITE_REGEXP_OK;
}

static int regexp_match_node(const struct mylite_regexp_node *node,
                             const struct mylite_regexp_match_context *context, size_t position,
                             struct mylite_regexp_position_list *out_positions)
{
    if (node == NULL) {
        return MYLITE_REGEXP_NOMEM;
    }

    switch (node->kind) {
    case MYLITE_REGEXP_NODE_EMPTY:
        return regexp_append_position(out_positions, position);
    case MYLITE_REGEXP_NODE_SEQUENCE:
        return regexp_match_sequence(node, context, 0U, position, out_positions);
    case MYLITE_REGEXP_NODE_ALTERNATION:
        return regexp_match_alternation(node, context, position, out_positions);
    case MYLITE_REGEXP_NODE_REPEAT:
        return regexp_match_repeat_positions(node, context, position, 0U, out_positions);
    case MYLITE_REGEXP_NODE_LITERAL:
        return regexp_match_literal_node(node, context, position, out_positions);
    case MYLITE_REGEXP_NODE_DOT:
        return regexp_match_dot_node(context, position, out_positions);
    case MYLITE_REGEXP_NODE_CLASS:
        return regexp_match_class_node(node, context, position, out_positions);
    case MYLITE_REGEXP_NODE_ANCHOR_START:
        return regexp_match_anchor_start_node(context, position, out_positions);
    case MYLITE_REGEXP_NODE_ANCHOR_END:
        return regexp_match_anchor_end_node(context, position, out_positions);
    }
    return MYLITE_REGEXP_NOMEM;
}

static int regexp_match_alternation(const struct mylite_regexp_node *node,
                                    const struct mylite_regexp_match_context *context,
                                    size_t position,
                                    struct mylite_regexp_position_list *out_positions)
{
    for (size_t index = 0U; index < node->children.count; ++index) {
        int status =
            regexp_match_node(node->children.items[index], context, position, out_positions);

        if (status != MYLITE_REGEXP_OK) {
            return status;
        }
    }
    return MYLITE_REGEXP_OK;
}

static int regexp_match_literal_node(const struct mylite_regexp_node *node,
                                     const struct mylite_regexp_match_context *context,
                                     size_t position,
                                     struct mylite_regexp_position_list *out_positions)
{
    if (position >= context->length) {
        return MYLITE_REGEXP_OK;
    }
    if (!regexp_literal_matches((unsigned char)context->value[position], node->literal,
                                context->options)) {
        return MYLITE_REGEXP_OK;
    }
    return regexp_append_position(out_positions, position + 1U);
}

static int regexp_match_dot_node(const struct mylite_regexp_match_context *context, size_t position,
                                 struct mylite_regexp_position_list *out_positions)
{
    if (position >= context->length) {
        return MYLITE_REGEXP_OK;
    }
    if (!regexp_dot_matches((unsigned char)context->value[position], context->options)) {
        return MYLITE_REGEXP_OK;
    }
    return regexp_append_position(out_positions, position + 1U);
}

static int regexp_match_class_node(const struct mylite_regexp_node *node,
                                   const struct mylite_regexp_match_context *context,
                                   size_t position,
                                   struct mylite_regexp_position_list *out_positions)
{
    if (position >= context->length) {
        return MYLITE_REGEXP_OK;
    }
    if (!regexp_class_matches(&node->character_class, (unsigned char)context->value[position],
                              context->options)) {
        return MYLITE_REGEXP_OK;
    }
    return regexp_append_position(out_positions, position + 1U);
}

static int regexp_match_anchor_start_node(const struct mylite_regexp_match_context *context,
                                          size_t position,
                                          struct mylite_regexp_position_list *out_positions)
{
    if (!regexp_anchor_start_matches(position, context)) {
        return MYLITE_REGEXP_OK;
    }
    return regexp_append_position(out_positions, position);
}

static int regexp_match_anchor_end_node(const struct mylite_regexp_match_context *context,
                                        size_t position,
                                        struct mylite_regexp_position_list *out_positions)
{
    if (!regexp_anchor_end_matches(position, context)) {
        return MYLITE_REGEXP_OK;
    }
    return regexp_append_position(out_positions, position);
}

static int regexp_match_sequence(const struct mylite_regexp_node *node,
                                 const struct mylite_regexp_match_context *context,
                                 size_t child_index, size_t position,
                                 struct mylite_regexp_position_list *out_positions)
{
    const struct mylite_regexp_node *child = NULL;
    struct mylite_regexp_position_list child_positions = {0};
    int status = MYLITE_REGEXP_OK;

    if (child_index >= node->children.count) {
        return regexp_append_position(out_positions, position);
    }
    child = node->children.items[child_index];
    if (child->kind == MYLITE_REGEXP_NODE_REPEAT) {
        return regexp_match_repeat_then_rest(child, node, context, child_index + 1U,
                                             (struct mylite_regexp_repeat_match_state){
                                                 .position = position,
                                                 .count = 0U,
                                             },
                                             out_positions);
    }

    status = regexp_match_node(child, context, position, &child_positions);
    if (status != MYLITE_REGEXP_OK) {
        regexp_position_list_deinit(&child_positions);
        return status;
    }
    for (size_t index = 0U; index < child_positions.count; ++index) {
        status = regexp_match_sequence(node, context, child_index + 1U,
                                       child_positions.items[index], out_positions);
        if (status != MYLITE_REGEXP_OK) {
            break;
        }
    }
    regexp_position_list_deinit(&child_positions);
    return status;
}

static int regexp_match_repeat_then_rest(const struct mylite_regexp_node *repeat,
                                         const struct mylite_regexp_node *sequence,
                                         const struct mylite_regexp_match_context *context,
                                         size_t next_child_index,
                                         struct mylite_regexp_repeat_match_state state,
                                         struct mylite_regexp_position_list *out_positions)
{
    struct mylite_regexp_position_list child_positions = {0};
    int status = MYLITE_REGEXP_OK;

    if (state.count != repeat->repeat.maximum) {
        status = regexp_match_node(repeat->repeat.child, context, state.position, &child_positions);
        if (status != MYLITE_REGEXP_OK) {
            regexp_position_list_deinit(&child_positions);
            return status;
        }
        for (size_t index = 0U; index < child_positions.count; ++index) {
            size_t next_position = child_positions.items[index];

            if (next_position == state.position) {
                continue;
            }
            status = regexp_match_repeat_then_rest(repeat, sequence, context, next_child_index,
                                                   (struct mylite_regexp_repeat_match_state){
                                                       .position = next_position,
                                                       .count = state.count + 1U,
                                                   },
                                                   out_positions);
            if (status != MYLITE_REGEXP_OK) {
                break;
            }
        }
        regexp_position_list_deinit(&child_positions);
        if (status != MYLITE_REGEXP_OK) {
            return status;
        }
    }
    if (state.count >= repeat->repeat.minimum) {
        return regexp_match_sequence(sequence, context, next_child_index, state.position,
                                     out_positions);
    }
    return MYLITE_REGEXP_OK;
}

static int regexp_match_repeat_positions(const struct mylite_regexp_node *repeat,
                                         const struct mylite_regexp_match_context *context,
                                         size_t position, size_t count,
                                         struct mylite_regexp_position_list *out_positions)
{
    struct mylite_regexp_position_list child_positions = {0};
    int status = MYLITE_REGEXP_OK;

    if (count != repeat->repeat.maximum) {
        status = regexp_match_node(repeat->repeat.child, context, position, &child_positions);
        if (status != MYLITE_REGEXP_OK) {
            regexp_position_list_deinit(&child_positions);
            return status;
        }
        for (size_t index = 0U; index < child_positions.count; ++index) {
            size_t next_position = child_positions.items[index];

            if (next_position == position) {
                continue;
            }
            status = regexp_match_repeat_positions(repeat, context, next_position, count + 1U,
                                                   out_positions);
            if (status != MYLITE_REGEXP_OK) {
                break;
            }
        }
        regexp_position_list_deinit(&child_positions);
        if (status != MYLITE_REGEXP_OK) {
            return status;
        }
    }
    if (count >= repeat->repeat.minimum && !regexp_position_list_append(out_positions, position)) {
        return MYLITE_REGEXP_NOMEM;
    }
    return MYLITE_REGEXP_OK;
}

static bool regexp_literal_matches(unsigned char value, unsigned char literal,
                                   struct mylite_regexp_options options)
{
    if (!options.case_sensitive) {
        value = regexp_case_fold(value);
        literal = regexp_case_fold(literal);
    }
    if (value == literal) {
        return true;
    }
    return false;
}

static bool regexp_dot_matches(unsigned char value, struct mylite_regexp_options options)
{
    if (options.dot_matches_newline) {
        return true;
    }
    if (value != '\n') {
        return true;
    }
    return false;
}

static bool regexp_class_matches(const struct mylite_regexp_class *class_value, unsigned char value,
                                 struct mylite_regexp_options options)
{
    bool matched = false;

    for (size_t index = 0U; index < class_value->count; ++index) {
        if (regexp_class_item_matches(class_value->items[index], value, options)) {
            matched = true;
            break;
        }
    }
    if (class_value->negated) {
        if (matched) {
            return false;
        }
        return true;
    }
    return matched;
}

static bool regexp_class_item_matches(struct mylite_regexp_class_item item, unsigned char value,
                                      struct mylite_regexp_options options)
{
    unsigned char first = item.first;
    unsigned char last = item.last;
    unsigned char compare_value = value;

    if (!options.case_sensitive) {
        first = regexp_case_fold(first);
        last = regexp_case_fold(last);
        compare_value = regexp_case_fold(compare_value);
    }

    switch (item.kind) {
    case MYLITE_REGEXP_CLASS_LITERAL:
        if (compare_value == first) {
            return true;
        }
        return false;
    case MYLITE_REGEXP_CLASS_RANGE:
        if (first <= last) {
            if (compare_value >= first && compare_value <= last) {
                return true;
            }
            return false;
        }
        if (compare_value >= last && compare_value <= first) {
            return true;
        }
        return false;
    case MYLITE_REGEXP_CLASS_DIGIT:
        return regexp_is_digit(value);
    case MYLITE_REGEXP_CLASS_NOT_DIGIT:
        if (regexp_is_digit(value)) {
            return false;
        }
        return true;
    case MYLITE_REGEXP_CLASS_WORD:
        return regexp_is_word(value);
    case MYLITE_REGEXP_CLASS_NOT_WORD:
        if (regexp_is_word(value)) {
            return false;
        }
        return true;
    case MYLITE_REGEXP_CLASS_SPACE:
        return regexp_is_space(value);
    case MYLITE_REGEXP_CLASS_NOT_SPACE:
        if (regexp_is_space(value)) {
            return false;
        }
        return true;
    case MYLITE_REGEXP_CLASS_ALPHA:
        return regexp_is_alpha(value);
    case MYLITE_REGEXP_CLASS_ALNUM:
        return regexp_is_alnum(value);
    case MYLITE_REGEXP_CLASS_LOWER:
        if (options.case_sensitive) {
            return regexp_is_lower(value);
        }
        return regexp_is_alpha(value);
    case MYLITE_REGEXP_CLASS_UPPER:
        if (options.case_sensitive) {
            return regexp_is_upper(value);
        }
        return regexp_is_alpha(value);
    case MYLITE_REGEXP_CLASS_XDIGIT:
        return regexp_is_xdigit(value);
    case MYLITE_REGEXP_CLASS_BLANK:
        return regexp_is_blank(value);
    }
    return false;
}

static bool regexp_anchor_start_matches(size_t position,
                                        const struct mylite_regexp_match_context *context)
{
    if (position == 0U) {
        return true;
    }
    if (!context->options.multiline || position > context->length) {
        return false;
    }
    if (context->value[position - 1U] == '\n') {
        return true;
    }
    return false;
}

static bool regexp_anchor_end_matches(size_t position,
                                      const struct mylite_regexp_match_context *context)
{
    if (position == context->length) {
        return true;
    }
    if (!context->options.multiline || position >= context->length) {
        return false;
    }
    if (context->value[position] == '\n') {
        return true;
    }
    return false;
}

static int regexp_append_position(struct mylite_regexp_position_list *list, size_t position)
{
    if (!regexp_position_list_append(list, position)) {
        return MYLITE_REGEXP_NOMEM;
    }
    return MYLITE_REGEXP_OK;
}

static bool regexp_position_list_append(struct mylite_regexp_position_list *list, size_t position)
{
    if (list->count == list->capacity) {
        size_t new_capacity = list->capacity == 0U ? 4U : list->capacity * 2U;
        size_t *new_items = realloc(list->items, new_capacity * sizeof(*list->items));

        if (new_items == NULL) {
            return false;
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }
    list->items[list->count++] = position;
    return true;
}

static void regexp_position_list_deinit(struct mylite_regexp_position_list *list)
{
    free(list->items);
    *list = (struct mylite_regexp_position_list){0};
}

static unsigned char regexp_case_fold(unsigned char value)
{
    return value >= 'A' && value <= 'Z' ? (unsigned char)(value - 'A' + 'a') : value;
}

static bool regexp_is_digit(unsigned char value)
{
    if (value >= '0' && value <= '9') {
        return true;
    }
    return false;
}

static bool regexp_is_alpha(unsigned char value)
{
    if (value >= 'A' && value <= 'Z') {
        return true;
    }
    if (value >= 'a' && value <= 'z') {
        return true;
    }
    return false;
}

static bool regexp_is_alnum(unsigned char value)
{
    if (regexp_is_alpha(value)) {
        return true;
    }
    return regexp_is_digit(value);
}

static bool regexp_is_word(unsigned char value)
{
    if (regexp_is_alnum(value)) {
        return true;
    }
    if (value == '_') {
        return true;
    }
    return false;
}

static bool regexp_is_space(unsigned char value)
{
    if (value == ' ' || value == '\f' || value == '\n' || value == '\r' || value == '\t') {
        return true;
    }
    if (value == '\v') {
        return true;
    }
    return false;
}

static bool regexp_is_lower(unsigned char value)
{
    if (value >= 'a' && value <= 'z') {
        return true;
    }
    return false;
}

static bool regexp_is_upper(unsigned char value)
{
    if (value >= 'A' && value <= 'Z') {
        return true;
    }
    return false;
}

static bool regexp_is_xdigit(unsigned char value)
{
    if (regexp_is_digit(value) || (value >= 'A' && value <= 'F')) {
        return true;
    }
    if (value >= 'a' && value <= 'f') {
        return true;
    }
    return false;
}

static bool regexp_is_blank(unsigned char value)
{
    if (value == ' ') {
        return true;
    }
    if (value == '\t') {
        return true;
    }
    return false;
}

static bool regexp_ascii_name_equals(const char *name, size_t name_length, const char *expected)
{
    size_t expected_length = regexp_cstring_length(expected);

    if (name_length != expected_length) {
        return false;
    }
    for (size_t index = 0U; index < name_length; ++index) {
        if (regexp_case_fold((unsigned char)name[index]) !=
            regexp_case_fold((unsigned char)expected[index])) {
            return false;
        }
    }
    return true;
}

static size_t regexp_cstring_length(const char *text)
{
    size_t length = 0U;

    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

// NOLINTEND(misc-no-recursion)
