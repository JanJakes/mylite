#include "parser_test_support.h"

static int test_table_partition_selection_statements(void);
static int test_table_partition_selection_rejections(void);
static int parse_ok(
    const char *sql,
    enum mylite_sql_ast_node_kind expected_statement_kind,
    const char *context
);
static int parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_table_partition_selection_statements();
    failures += test_table_partition_selection_rejections();

    return failures == 0 ? 0 : 1;
}

static int test_table_partition_selection_statements(void) {
    static const struct {
        const char *sql;
        enum mylite_sql_ast_node_kind kind;
        const char *context;
    } statements[] = {
        {"SELECT * FROM sales PARTITION (p0)",
         MYLITE_SQL_AST_SELECT_STATEMENT,
         "select source partition"},
        {"SELECT * FROM sales PARTITION (p0, p1) AS s USE INDEX (idx_region)",
         MYLITE_SQL_AST_SELECT_STATEMENT,
         "select source partition before alias and hints"},
        {"SELECT id INTO @selected_id FROM sales PARTITION (p0)",
         MYLITE_SQL_AST_SELECT_STATEMENT,
         "select into source partition"},
        {"SELECT * FROM sales PARTITION (p0) AS s JOIN regions PARTITION (p1) AS r "
         "ON s.region = r.id",
         MYLITE_SQL_AST_SELECT_STATEMENT,
         "joined select source partitions"},
        {"INSERT INTO sales PARTITION (p0) (id, region) VALUES (1, 3)",
         MYLITE_SQL_AST_INSERT_STATEMENT,
         "insert values target partition"},
        {"INSERT IGNORE INTO sales PARTITION (p0) SET id = 1, region = 3",
         MYLITE_SQL_AST_INSERT_SET_STATEMENT,
         "insert set target partition"},
        {"INSERT INTO sales PARTITION (p0) SELECT id, region FROM source PARTITION (p1)",
         MYLITE_SQL_AST_INSERT_SELECT_STATEMENT,
         "insert select target and source partitions"},
        {"REPLACE INTO sales PARTITION (p0) VALUES (1, 3)",
         MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT,
         "replace values target partition"},
        {"REPLACE INTO sales PARTITION (p0) SET id = 1, region = 3",
         MYLITE_SQL_AST_REPLACE_SET_STATEMENT,
         "replace set target partition"},
        {"REPLACE INTO sales PARTITION (p0) SELECT id, region FROM source PARTITION (p1)",
         MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT,
         "replace select target and source partitions"},
        {"UPDATE sales PARTITION (p0) USE INDEX (idx_region) SET region = 4 WHERE id = 1",
         MYLITE_SQL_AST_UPDATE_STATEMENT,
         "update target partition before hints"},
        {"UPDATE sales PARTITION (p0) AS s JOIN regions PARTITION (p1) AS r "
         "ON s.region = r.id SET s.region = 4",
         MYLITE_SQL_AST_JOINED_UPDATE_STATEMENT,
         "joined update source partitions"},
        {"DELETE FROM sales PARTITION (p0) AS s WHERE id = 1",
         MYLITE_SQL_AST_DELETE_STATEMENT,
         "delete target partition before alias"},
        {"DELETE s FROM sales PARTITION (p0) AS s JOIN regions PARTITION (p1) AS r "
         "ON s.region = r.id",
         MYLITE_SQL_AST_JOINED_DELETE_STATEMENT,
         "joined delete source partitions"},
        {"LOAD DATA INFILE '/tmp/sales.tsv' INTO TABLE sales PARTITION (p0) "
         "IGNORE 1 LINES (id, region)",
         MYLITE_SQL_AST_LOAD_DATA_INFILE_STATEMENT,
         "load data target partition"},
        {"LOAD DATA LOCAL INFILE '/tmp/sales.tsv' INTO TABLE sales PARTITION (p0)",
         MYLITE_SQL_AST_LOAD_DATA_INFILE_STATEMENT,
         "load data local target partition"},
    };

    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures +=
            parse_ok(statements[index].sql, statements[index].kind, statements[index].context);
    }

    return failures;
}

static int test_table_partition_selection_rejections(void) {
    int failures = 0;

    failures += parse_status(
        "SELECT * FROM sales PARTITION ()",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "empty partition list rejected"
    );
    failures += parse_status(
        "SELECT * FROM sales PARTITION p0",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "unparenthesized partition list rejected"
    );
    failures += parse_status(
        "INSERT INTO sales PARTITION () VALUES (1)",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "empty insert target partition list rejected"
    );

    return failures;
}

static int parse_ok(
    const char *sql,
    enum mylite_sql_ast_node_kind expected_statement_kind,
    const char *context
) {
    struct mylite_sql_parse_result result;
    int failures = parser_test_parse_sql(sql, MYLITE_SQL_PARSE_OK, &result);

    failures += parser_test_expect_node(
        parser_test_child_at(result.root, 0U),
        expected_statement_kind,
        context
    );
    mylite_sql_parse_result_deinit(&result);
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
