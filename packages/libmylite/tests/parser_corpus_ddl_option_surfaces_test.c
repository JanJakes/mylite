#include "parser_test_support.h"

static int test_table_option_placeholders(void);
static int test_column_attribute_placeholders(void);
static int test_alter_table_multi_action_placeholders(void);
static int parse_ok(const char *sql);

int main(void) {
    int failures = 0;

    failures += test_table_option_placeholders();
    failures += test_column_attribute_placeholders();
    failures += test_alter_table_multi_action_placeholders();

    return failures == 0 ? 0 : 1;
}

static int test_table_option_placeholders(void) {
    static const char *const forms[] = {
        "CREATE TABLE option_t (id INT) TABLESPACE innodb_file_per_table STORAGE DISK "
        "ENGINE=InnoDB",
        "CREATE TABLE merge_t (id INT) ENGINE=MERGE UNION=(base_a,base_b) INSERT_METHOD=NO",
        "CREATE TABLE merge_space_t (id INT) ENGINE=MRG_MYISAM UNION (base_a) "
        "INSERT_METHOD=FIRST",
        "CREATE TABLE empty_merge_t (id INT) ENGINE=MERGE UNION=()",
        "CREATE TEMPORARY TABLE empty_temp_merge_t (id INT) ENGINE=MERGE UNION ()",
        "CREATE TABLE storage_t (id INT) INSERT_METHOD=LAST STORAGE MEMORY",
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *table_options = NULL;
    int failures = 0;

    for (size_t index = 0U; index < sizeof(forms) / sizeof(forms[0]); ++index) {
        failures += parse_ok(forms[index]);
    }

    failures += parser_test_parse_sql(
        "CREATE TABLE option_nodes (id INT) TABLESPACE innodb_file_per_table STORAGE DISK "
        "UNION=(base_a,base_b) INSERT_METHOD=LAST",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_options = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_node(
        table_options,
        MYLITE_SQL_AST_TABLE_OPTION_LIST,
        "placeholder table option list"
    );
    failures += parser_test_expect_child_count(table_options, 4U, "placeholder table option count");
    failures += parser_test_expect_node(
        parser_test_child_at(table_options, 0U),
        MYLITE_SQL_AST_TABLE_TABLESPACE_OPTION,
        "tablespace option"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(table_options, 1U),
        MYLITE_SQL_AST_TABLE_STORAGE_OPTION,
        "storage option"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(table_options, 2U),
        MYLITE_SQL_AST_TABLE_UNION_OPTION,
        "union option"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(table_options, 3U),
        MYLITE_SQL_AST_TABLE_INSERT_METHOD_OPTION,
        "insert method option"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(table_options, 1U), 0U),
        "DISK",
        "storage value"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(table_options, 3U), 0U),
        "LAST",
        "insert method value"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_column_attribute_placeholders(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *attribute = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "CREATE TABLE column_attrs (id INT VISIBLE, hidden_col INT INVISIBLE, "
        "g GEOMETRY SRID 0)",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(columns, 3U, "column attribute count");

    column = parser_test_child_at(columns, 0U);
    attribute = parser_test_child_at(column, 2U);
    failures += parser_test_expect_node(
        attribute,
        MYLITE_SQL_AST_COLUMN_VISIBILITY_ATTRIBUTE,
        "visible column attribute"
    );
    failures += parser_test_expect_column_visibility(
        attribute,
        MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE,
        "visible column attribute payload"
    );

    column = parser_test_child_at(columns, 1U);
    attribute = parser_test_child_at(column, 2U);
    failures += parser_test_expect_node(
        attribute,
        MYLITE_SQL_AST_COLUMN_VISIBILITY_ATTRIBUTE,
        "invisible column attribute"
    );
    failures += parser_test_expect_column_visibility(
        attribute,
        MYLITE_SQL_AST_COLUMN_VISIBILITY_INVISIBLE,
        "invisible column attribute payload"
    );

    column = parser_test_child_at(columns, 2U);
    attribute = parser_test_child_at(column, 2U);
    failures += parser_test_expect_node(
        attribute,
        MYLITE_SQL_AST_COLUMN_SRID_ATTRIBUTE,
        "SRID column attribute"
    );
    failures +=
        parser_test_expect_span_text(parser_test_child_at(attribute, 0U), "0", "SRID value");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_ok("ALTER TABLE t ADD COLUMN hidden_col INT INVISIBLE, ADD COLUMN v INT");
    failures += parse_ok("ALTER TABLE t MODIFY hidden_col INT VISIBLE, CHANGE v v2 INT INVISIBLE");

    return failures;
}

static int test_alter_table_multi_action_placeholders(void) {
    static const char *const forms[] = {
        "ALTER TABLE t ADD COLUMN d INT, RENAME TO t2",
        "ALTER TABLE t RENAME COLUMN d TO e, ADD COLUMN f INT",
        "ALTER TABLE t RENAME INDEX ix_c TO ix_c2, DISABLE KEYS",
        "ALTER TABLE t ALTER INDEX ix_c INVISIBLE, ENABLE KEYS",
        "ALTER TABLE t ALTER COLUMN hidden_col SET VISIBLE, COMMENT='x'",
        "ALTER TABLE t ADD FULLTEXT KEY ft_title (title), DROP CHECK chk_title",
        "ALTER TABLE t ADD SPATIAL INDEX sg (g), DROP FOREIGN KEY fk_t_parent",
        "ALTER TABLE t ADD COLUMN x INT, TABLESPACE innodb_file_per_table",
        "ALTER TABLE t ADD COLUMN y INT, STORAGE DISK",
        "ALTER TABLE t ADD COLUMN z INT, UNION=(base_a,base_b)",
        "ALTER TABLE t UNION=()",
        "ALTER TABLE t ENGINE=InnoDB, DROP COLUMN d",
        "ALTER TABLE t ENGINE='InnoDB', MODIFY dl CHAR(64)",
        "ALTER TABLE t ENGINE=MyISAM, ADD COLUMN c2 INT",
        "ALTER TABLE t ENGINE='InnoDB', ALTER my_row_id SET INVISIBLE",
        "ALTER TABLE parent ENGINE=InnoDB, RENAME TO parent0",
        ("ALTER TABLE t AVG_ROW_LENGTH=0 CHECKSUM=0 COMMENT='' MIN_ROWS=0 "
         "MAX_ROWS=0 PACK_KEYS=DEFAULT DELAY_KEY_WRITE=0 ROW_FORMAT=DEFAULT"),
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *actions = NULL;
    int failures = 0;

    for (size_t index = 0U; index < sizeof(forms) / sizeof(forms[0]); ++index) {
        failures += parse_ok(forms[index]);
    }

    failures += parser_test_parse_sql(
        "ALTER TABLE t ADD COLUMN d INT, RENAME TO t2",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    actions = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT,
        "rename multi-action statement"
    );
    failures += parser_test_expect_child_count(actions, 2U, "rename multi-action count");
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 1U),
        MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT,
        "rename multi-action placeholder"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE t ENGINE=InnoDB, DROP COLUMN d",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(result.root, 0U),
        MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT,
        "raw leading table option multi-action placeholder"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(result.root, 0U),
        0U,
        "raw leading table option multi-action child count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE t ADD COLUMN d INT, TABLESPACE innodb_file_per_table",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    actions = parser_test_child_at(parser_test_child_at(result.root, 0U), 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 1U),
        MYLITE_SQL_AST_ALTER_TABLE_STORAGE_STATISTICS_STATEMENT,
        "table option multi-action placeholder"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int parse_ok(const char *sql) {
    struct mylite_sql_parse_result result;
    int failures = parser_test_parse_sql(sql, MYLITE_SQL_PARSE_OK, &result);

    if (failures != 0) {
        (void)parser_test_expect_true(0, sql);
    }
    mylite_sql_parse_result_deinit(&result);
    return failures;
}
