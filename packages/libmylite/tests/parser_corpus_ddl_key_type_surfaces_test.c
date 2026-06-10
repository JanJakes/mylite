#include "parser_test_support.h"

#include <stddef.h>

static int test_inline_key_attribute(void);
static int test_primary_prefix_part(void);
static int test_index_key_block_size_option(void);
static int test_zerofill_placeholders(void);
static int parse_ok(const char *sql);

int main(void) {
    int failures = 0;

    failures += test_inline_key_attribute();
    failures += test_primary_prefix_part();
    failures += test_index_key_block_size_option();
    failures += test_zerofill_placeholders();

    return failures == 0 ? 0 : 1;
}

static int test_inline_key_attribute(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *items = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *attribute = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "CREATE TABLE inline_key (id INT KEY, v INT)",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    items = parser_test_child_at(statement, 1U);
    column = parser_test_child_at(items, 0U);
    attribute = parser_test_child_at(column, 2U);
    failures += parser_test_expect_node(
        attribute,
        MYLITE_SQL_AST_INLINE_PRIMARY_KEY,
        "inline KEY attribute"
    );
    failures += parser_test_expect_span_text(attribute, "KEY", "inline KEY span");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_primary_prefix_part(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *items = NULL;
    const struct mylite_sql_ast_node *primary_key = NULL;
    const struct mylite_sql_ast_node *key_parts = NULL;
    const struct mylite_sql_ast_node *part = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "CREATE TABLE primary_prefix (name VARCHAR(100), PRIMARY KEY (name(10) DESC))",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    items = parser_test_child_at(statement, 1U);
    primary_key = parser_test_child_at(items, 1U);
    key_parts = parser_test_child_at(primary_key, 0U);
    part = parser_test_child_at(key_parts, 0U);
    failures += parser_test_expect_node(
        part,
        MYLITE_SQL_AST_SECONDARY_INDEX_PART,
        "primary prefix key part"
    );
    failures += parser_test_expect_child_count(part, 3U, "primary prefix key part children");
    failures += parser_test_expect_span_text(
        parser_test_child_at(part, 0U),
        "name",
        "primary prefix column"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(part, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "primary prefix length"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(part, 1U),
        "10",
        "primary prefix length span"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(part, 2U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "primary prefix direction"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_ok("ALTER TABLE t ADD PRIMARY KEY (name(10), code)");

    return failures;
}

static int test_index_key_block_size_option(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *items = NULL;
    const struct mylite_sql_ast_node *index = NULL;
    const struct mylite_sql_ast_node *options = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "CREATE TABLE keyed (a INT, KEY k (a) KEY_BLOCK_SIZE=1024)",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    items = parser_test_child_at(statement, 1U);
    index = parser_test_child_at(items, 1U);
    options = parser_test_child_at(index, 2U);
    failures += parser_test_expect_node(
        options,
        MYLITE_SQL_AST_INDEX_OPTION_LIST,
        "index key_block_size option list"
    );
    failures += parser_test_expect_child_count(options, 1U, "index key_block_size option count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(options, 0U),
        "KEY_BLOCK_SIZE",
        "index key_block_size option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_ok("CREATE UNIQUE INDEX u ON keyed (a) KEY_BLOCK_SIZE 128");
    failures += parse_ok("ALTER TABLE keyed ADD KEY k2 (a) KEY_BLOCK_SIZE=256");

    return failures;
}

static int test_zerofill_placeholders(void) {
    static const char *const forms[] = {
        "CREATE TABLE zint (a INT ZEROFILL)",
        "CREATE TABLE zdec (a DECIMAL(5,2) ZEROFILL)",
        "ALTER TABLE zint ADD COLUMN b FLOAT ZEROFILL",
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    for (size_t index = 0U; index < sizeof(forms) / sizeof(forms[0]); ++index) {
        failures += parser_test_parse_sql(forms[index], MYLITE_SQL_PARSE_OK, &result);
        statement = parser_test_child_at(result.root, 0U);
        failures += parser_test_expect_node(
            statement,
            MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
            forms[index]
        );
        mylite_sql_parse_result_deinit(&result);
    }

    return failures;
}

static int parse_ok(const char *sql) {
    struct mylite_sql_parse_result result;
    int failures = parser_test_parse_sql(sql, MYLITE_SQL_PARSE_OK, &result);

    if (failures != 0) {
        failures += parser_test_expect_true(0, sql);
    }
    mylite_sql_parse_result_deinit(&result);
    return failures;
}
