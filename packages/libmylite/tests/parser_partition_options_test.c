#include "parser_test_support.h"

static int test_create_table_partition_options(void);
static int test_create_table_partition_rejections(void);
static int parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_create_table_partition_options();
    failures += test_create_table_partition_rejections();

    return failures == 0 ? 0 : 1;
}

static int test_create_table_partition_options(void) {
    static const char *const statements[] = {
        "CREATE TABLE t1 (id INT, purchased DATE) "
        "PARTITION BY RANGE (YEAR(purchased)) ("
        "PARTITION p0 VALUES LESS THAN (1990), "
        "PARTITION p1 VALUES LESS THAN MAXVALUE)",
        "CREATE TABLE t2 (id INT, region INT) "
        "PARTITION BY LIST (region) ("
        "PARTITION p0 VALUES IN (1, 2), "
        "PARTITION p1 VALUES IN (3, 4))",
        "CREATE TABLE t3 (id INT, name VARCHAR(32)) "
        "PARTITION BY HASH (id) PARTITIONS 4",
        "CREATE TABLE t4 (id INT, name VARCHAR(32), KEY name_key (name)) "
        "PARTITION BY LINEAR KEY ALGORITHM=1 (name) PARTITIONS 3",
        "CREATE TEMPORARY TABLE t5 (id INT, group_id INT) "
        "PARTITION BY RANGE (id) SUBPARTITION BY HASH (group_id) SUBPARTITIONS 2 ("
        "PARTITION p0 VALUES LESS THAN (100), "
        "PARTITION p1 VALUES LESS THAN MAXVALUE)",
    };
    struct mylite_sql_parse_result result;
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += parser_test_parse_sql(statements[index], MYLITE_SQL_PARSE_OK, &result);
        failures += parser_test_expect_node(
            parser_test_child_at(result.root, 0U),
            index == 4U ? MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_STATEMENT
                        : MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
            statements[index]
        );
        mylite_sql_parse_result_deinit(&result);
    }

    return failures;
}

static int test_create_table_partition_rejections(void) {
    int failures = 0;

    failures += parse_status(
        "CREATE TABLE t1 (id INT) PARTITION BY RANGE",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete partition method"
    );
    failures += parse_status(
        "CREATE TABLE t1 (id INT) PARTITION BY RANGE (id) AS SELECT 1",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "partitioned create table select remains unsupported"
    );
    failures += parse_status(
        "SELECT * FROM t1 PARTITION (p0)",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "query partition selection remains unsupported"
    );

    return failures;
}

static int parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
) {
    struct mylite_sql_parse_result result;
    int failures = parser_test_parse_sql(sql, expected_status, &result);

    if (failures != 0) {
        (void)parser_test_expect_true(0, context);
    }
    mylite_sql_parse_result_deinit(&result);
    return failures;
}
