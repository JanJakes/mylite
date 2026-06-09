#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_alter_table_partition_placeholder_forms(void);
static int test_alter_table_partition_incomplete_forms(void);
static int expect_statement_kind(struct expected_statement expected);
static int expect_parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_alter_table_partition_placeholder_forms();
    failures += test_alter_table_partition_incomplete_forms();

    return failures == 0 ? 0 : 1;
}

static int test_alter_table_partition_placeholder_forms(void) {
    static const struct expected_statement statements[] = {
        {.sql = "ALTER TABLE t PARTITION BY HASH (id) PARTITIONS 4",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "ALTER TABLE app.t PARTITION BY HASH (id) PARTITIONS 4",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "ALTER TABLE t REMOVE PARTITIONING",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "ALTER TABLE t ADD PARTITION PARTITIONS 2",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "ALTER TABLE t ADD PARTITION (PARTITION p1 VALUES LESS THAN (10))",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "ALTER TABLE t DROP PARTITION p0",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "ALTER TABLE t REORGANIZE PARTITION p0 INTO "
                "(PARTITION p0 VALUES LESS THAN (10))",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "ALTER TABLE t REBUILD PARTITION p0",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "ALTER TABLE t COALESCE PARTITION 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "ALTER TABLE t TRUNCATE PARTITION p0",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "ALTER TABLE t EXCHANGE PARTITION p0 WITH TABLE staging WITHOUT VALIDATION",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "ALTER TABLE t ANALYZE PARTITION p0",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "ALTER TABLE t CHECK PARTITION p0",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "ALTER TABLE t OPTIMIZE PARTITION p0",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "ALTER TABLE t REPAIR PARTITION p0",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "ALTER TABLE t ALGORITHM = INPLACE, LOCK = SHARED, ADD PARTITION PARTITIONS 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "ALTER TABLE t ADD COLUMN c INT, DROP PARTITION p0",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_kind(statements[index]);
    }
    return failures;
}

static int test_alter_table_partition_incomplete_forms(void) {
    int failures = 0;

    failures += expect_parse_status(
        "ALTER TABLE t ADD PARTITION",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete ADD PARTITION remains syntax error"
    );
    failures += expect_parse_status(
        "ALTER TABLE t PARTITION BY",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete PARTITION BY remains syntax error"
    );
    failures += expect_parse_status(
        "ALTER TABLE t PARTITION p0",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "non-operation PARTITION tail remains syntax error"
    );
    failures += expect_parse_status(
        "ALTER TABLE t ADD PARTITION (PARTITION p0 VALUES LESS THAN (10)",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "unbalanced partition form remains syntax error"
    );
    return failures;
}

static int expect_statement_kind(struct expected_statement expected) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(expected.sql, MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(statement, expected.kind, expected.sql);
    failures += parser_test_expect_child_count(statement, 0U, expected.sql);
    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int expect_parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
) {
    struct mylite_sql_parse_result result;
    int failures = parser_test_parse_sql(sql, expected_status, &result);

    mylite_sql_parse_result_deinit(&result);
    (void)context;
    return failures;
}
