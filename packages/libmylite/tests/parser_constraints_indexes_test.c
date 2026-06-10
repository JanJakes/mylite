#include "parser_test_support.h"

static int test_create_table_primary_key_statements(void);
static int test_create_table_foreign_key_statements(void);
static int test_create_index_statements(void);
static int test_drop_index_statements(void);
static int test_alter_table_add_primary_key_statements(void);
static int test_alter_table_add_index_statements(void);
static int test_alter_table_add_foreign_key_statements(void);
static int test_alter_table_drop_foreign_key_statements(void);
static int test_alter_table_drop_index_statements(void);
static int test_alter_table_rename_index_statements(void);
static int test_alter_table_index_visibility_statements(void);
static int test_alter_table_check_constraint_statements(void);
static int test_alter_table_drop_primary_key_statements(void);
static int test_alter_table_auto_increment_option_statements(void);

int main(void) {
    int failures = 0;

    failures += test_create_table_primary_key_statements();
    failures += test_create_table_foreign_key_statements();
    failures += test_create_index_statements();
    failures += test_drop_index_statements();
    failures += test_alter_table_add_primary_key_statements();
    failures += test_alter_table_add_index_statements();
    failures += test_alter_table_add_foreign_key_statements();
    failures += test_alter_table_drop_foreign_key_statements();
    failures += test_alter_table_drop_index_statements();
    failures += test_alter_table_rename_index_statements();
    failures += test_alter_table_index_visibility_statements();
    failures += test_alter_table_check_constraint_statements();
    failures += test_alter_table_drop_primary_key_statements();
    failures += test_alter_table_auto_increment_option_statements();

    return failures == 0 ? 0 : 1;
}

static int test_create_table_primary_key_statements(void) {
    enum {
        fulltext_item_count = 7U,
        fulltext_key_item_index = 3U,
        fulltext_index_item_index = 4U,
        fulltext_named_item_index = 5U,
        fulltext_unnamed_item_index = 6U,
    };

    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *items = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *primary_key = NULL;
    const struct mylite_sql_ast_node *key_parts = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "CREATE TABLE inline_pk (id INT PRIMARY KEY, amount BIGINT NOT NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    items = parser_test_child_at(statement, 1U);
    column = parser_test_child_at(items, 0U);
    primary_key = parser_test_child_at(column, 2U);
    failures += parser_test_expect_node(
        items,
        MYLITE_SQL_AST_COLUMN_DEFINITION_LIST,
        "inline pk item list"
    );
    failures += parser_test_expect_child_count(items, 2U, "inline pk item count");
    failures +=
        parser_test_expect_node(column, MYLITE_SQL_AST_COLUMN_DEFINITION, "inline pk column");
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "id",
        "inline pk column name"
    );
    failures +=
        parser_test_expect_node(primary_key, MYLITE_SQL_AST_INLINE_PRIMARY_KEY, "inline pk marker");
    failures += parser_test_expect_span_text(primary_key, "PRIMARY KEY", "inline pk marker span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE inline_pk_full (id INT NOT NULL DEFAULT +1 PRIMARY KEY);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(parser_test_child_at(statement, 1U), 0U);
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "inline pk not null"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(column, 3U),
        MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE,
        "inline pk default"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(column, 4U),
        MYLITE_SQL_AST_INLINE_PRIMARY_KEY,
        "inline pk final"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE auto_inline (id INT AUTO_INCREMENT PRIMARY KEY, amount INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    column =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 1U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT,
        "inline auto increment marker"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(column, 3U),
        MYLITE_SQL_AST_INLINE_PRIMARY_KEY,
        "inline auto increment primary key"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE auto_table_pk (id INT NOT NULL AUTO_INCREMENT, PRIMARY KEY (id));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    column =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 1U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(column, 3U),
        MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT,
        "table pk auto increment marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE auto_secondary_key (id INT AUTO_INCREMENT, KEY(id), v INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    items = parser_test_child_at(statement, 1U);
    column = parser_test_child_at(items, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT,
        "secondary key auto increment marker"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(items, 1U),
        MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION,
        "secondary key auto increment index"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE auto_secondary_unique (id INT AUTO_INCREMENT UNIQUE, v INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    column =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 1U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT,
        "secondary unique auto increment marker"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(column, 3U),
        MYLITE_SQL_AST_INLINE_UNIQUE_KEY,
        "secondary unique auto increment key"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE auto_option (id INT PRIMARY KEY AUTO_INCREMENT) AUTO_INCREMENT=7;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    items = parser_test_child_at(statement, 1U);
    column = parser_test_child_at(items, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_INLINE_PRIMARY_KEY,
        "auto option primary key before auto increment"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(column, 3U),
        MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT,
        "auto option column marker"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_TABLE_OPTION_LIST,
        "auto increment table option list"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(statement, 2U), 0U),
        MYLITE_SQL_AST_TABLE_AUTO_INCREMENT_OPTION,
        "auto increment table option"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "auto increment table option value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE table_pk (id INT, PRIMARY KEY (id));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    items = parser_test_child_at(statement, 1U);
    primary_key = parser_test_child_at(items, 1U);
    key_parts = parser_test_child_at(primary_key, 0U);
    failures += parser_test_expect_child_count(items, 2U, "table pk item count");
    failures +=
        parser_test_expect_node(primary_key, MYLITE_SQL_AST_PRIMARY_KEY_DEFINITION, "table pk");
    failures += parser_test_expect_span_text(primary_key, "PRIMARY KEY (id)", "table pk span");
    failures +=
        parser_test_expect_node(key_parts, MYLITE_SQL_AST_PRIMARY_KEY_PART_LIST, "table pk parts");
    failures += parser_test_expect_child_count(key_parts, 1U, "table pk part count");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(key_parts, 0U), "id", "table pk part");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE parser_named_pk_index (id INT, PRIMARY KEY idx (id), KEY idx (id));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    items = parser_test_child_at(statement, 1U);
    primary_key = parser_test_child_at(items, 1U);
    key_parts = parser_test_child_at(primary_key, 0U);
    failures += parser_test_expect_child_count(primary_key, 1U, "named table pk child count");
    failures +=
        parser_test_expect_span_text(primary_key, "PRIMARY KEY idx (id)", "named table pk span");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(key_parts, 0U), "id", "named pk part");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(items, 2U), 0U),
        "idx",
        "secondary key reuses ignored primary key name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE parser_composite_pk (a INT, b INT, PRIMARY KEY (a, `b`));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    primary_key =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 1U), 2U);
    key_parts = parser_test_child_at(primary_key, 0U);
    failures += parser_test_expect_child_count(key_parts, 2U, "composite pk parser part count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "a",
        "composite pk first part"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 1U), 0U),
        "`b`",
        "composite pk second part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE parser_qualified_pk (id INT, PRIMARY KEY (parser_qualified_pk.id));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE parser_qualified_pk_desc (id INT, PRIMARY KEY (parser_qualified_pk_desc.id "
        "DESC));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE secondary_keys (id INT PRIMARY KEY, v INT, KEY k_v (v), INDEX (id));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    items = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(items, 4U, "secondary key item count");
    failures += parser_test_expect_node(
        parser_test_child_at(items, 2U),
        MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION,
        "named secondary key"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(items, 2U), 0U),
        "k_v",
        "secondary key name"
    );
    key_parts = parser_test_child_at(parser_test_child_at(items, 2U), 1U);
    failures += parser_test_expect_node(
        key_parts,
        MYLITE_SQL_AST_SECONDARY_INDEX_PART_LIST,
        "secondary key parts"
    );
    failures += parser_test_expect_child_count(key_parts, 1U, "secondary key part count");
    failures += parser_test_expect_node(
        parser_test_child_at(key_parts, 0U),
        MYLITE_SQL_AST_SECONDARY_INDEX_PART,
        "secondary key part"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "v",
        "secondary key part"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(items, 3U),
        MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION,
        "unnamed secondary index"
    );
    key_parts = parser_test_child_at(parser_test_child_at(items, 3U), 0U);
    failures += parser_test_expect_child_count(key_parts, 1U, "unnamed secondary key part count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "id",
        "unnamed secondary key part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE parser_composite_secondary (a INT, b INT, KEY k_ab (a, b));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    key_parts =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 1U), 2U);
    key_parts = parser_test_child_at(key_parts, 1U);
    failures +=
        parser_test_expect_child_count(key_parts, 2U, "composite secondary parser part count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "a",
        "composite secondary first part"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 1U), 0U),
        "b",
        "composite secondary second part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_named_pk (id INT, CONSTRAINT pk PRIMARY KEY (id));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    items = parser_test_child_at(statement, 1U);
    primary_key = parser_test_child_at(items, 1U);
    failures += parser_test_expect_node(
        primary_key,
        MYLITE_SQL_AST_PRIMARY_KEY_DEFINITION,
        "named primary constraint"
    );
    failures += parser_test_expect_span_text(
        primary_key,
        "PRIMARY KEY (id)",
        "named primary constraint span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unique_keys (id INT UNIQUE, n INT UNIQUE KEY, UNIQUE KEY u_id (id), "
        "UNIQUE INDEX (n));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    items = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(items, 4U, "unique key item count");
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(items, 0U), 2U),
        MYLITE_SQL_AST_INLINE_UNIQUE_KEY,
        "inline unique attribute"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(items, 1U), 2U),
        MYLITE_SQL_AST_INLINE_UNIQUE_KEY,
        "inline unique key attribute"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(items, 2U),
        MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION,
        "named unique key"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(items, 2U), 0U),
        "u_id",
        "unique key name"
    );
    key_parts = parser_test_child_at(parser_test_child_at(items, 2U), 1U);
    failures += parser_test_expect_node(
        key_parts,
        MYLITE_SQL_AST_SECONDARY_INDEX_PART_LIST,
        "unique key parts"
    );
    failures += parser_test_expect_child_count(key_parts, 1U, "unique key part count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "id",
        "unique key part"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(items, 3U),
        MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION,
        "unnamed unique index"
    );
    key_parts = parser_test_child_at(parser_test_child_at(items, 3U), 0U);
    failures += parser_test_expect_child_count(key_parts, 1U, "unnamed unique key part count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "n",
        "unnamed unique key part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE named_unique_constraints ("
        "a INT, b INT, c VARCHAR(20), body TEXT, "
        "CONSTRAINT uq_a UNIQUE (a), "
        "CONSTRAINT uq_b UNIQUE KEY (b DESC), "
        "CONSTRAINT UNIQUE INDEX uq_c (c(3), body(2)));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    items = parser_test_child_at(statement, 1U);

    enum {
        named_unique_constraint_item_count = 7U,
        named_unique_key_item_index = 5U,
        named_unique_explicit_item_index = 6U,
    };

    failures += parser_test_expect_child_count(
        items,
        named_unique_constraint_item_count,
        "named unique constraint item count"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(items, 4U),
        MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION,
        "named unique constraint definition"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(items, 4U), 0U),
        "uq_a",
        "named unique name"
    );
    key_parts = parser_test_child_at(parser_test_child_at(items, 4U), 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "a",
        "named unique key part"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(items, named_unique_key_item_index), 0U),
        "uq_b",
        "named unique key name"
    );
    key_parts = parser_test_child_at(parser_test_child_at(items, named_unique_key_item_index), 1U);
    failures += parser_test_expect_order_direction(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "named unique desc key part"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(items, named_unique_explicit_item_index), 0U),
        "uq_c",
        "constraint unique index explicit name"
    );
    key_parts =
        parser_test_child_at(parser_test_child_at(items, named_unique_explicit_item_index), 1U);
    failures +=
        parser_test_expect_child_count(key_parts, 2U, "constraint unique prefix part count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 1U),
        "3",
        "constraint unique first prefix"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 1U), 1U),
        "2",
        "constraint unique second prefix"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE explicit_named_unique (a INT, CONSTRAINT ignored UNIQUE KEY visible (a));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    items = parser_test_child_at(parser_test_child_at(result.root, 0U), 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(items, 1U), 0U),
        "visible",
        "constraint unique explicit visible index name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE explicit_no_keyword_name (a INT, CONSTRAINT c UNIQUE visible (a));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    items = parser_test_child_at(parser_test_child_at(result.root, 0U), 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(items, 1U), 0U),
        "visible",
        "constraint unique no-keyword explicit visible index name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE secondary_prefix (id INT, name VARCHAR(20), KEY k_name (name(4)));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    key_parts =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 1U), 2U);
    key_parts = parser_test_child_at(key_parts, 1U);
    failures += parser_test_expect_child_count(key_parts, 1U, "secondary prefix parser part count");
    failures += parser_test_expect_node(
        parser_test_child_at(key_parts, 0U),
        MYLITE_SQL_AST_SECONDARY_INDEX_PART,
        "secondary prefix key part"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "name",
        "secondary prefix key part column"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 1U),
        "4",
        "secondary prefix key part length"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE secondary_desc (id INT, v VARCHAR(20), KEY k (id DESC), "
        "KEY k_v (v(4) ASC));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    key_parts =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 1U), 2U);
    key_parts = parser_test_child_at(key_parts, 1U);
    failures += parser_test_expect_order_direction(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "secondary desc key part"
    );
    key_parts =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 1U), 3U);
    key_parts = parser_test_child_at(key_parts, 1U);
    failures += parser_test_expect_order_direction(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 2U),
        MYLITE_SQL_AST_ORDER_DIRECTION_ASC,
        "secondary prefix asc key part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE fulltext_keys (id INT, body TEXT, title VARCHAR(20), "
        "FULLTEXT KEY ft_body (body), FULLTEXT INDEX ft_title (title), "
        "FULLTEXT ft_named (body(10)), FULLTEXT (title DESC));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    items = parser_test_child_at(parser_test_child_at(result.root, 0U), 1U);
    failures += parser_test_expect_child_count(items, fulltext_item_count, "fulltext item count");
    failures += parser_test_expect_node(
        parser_test_child_at(items, fulltext_key_item_index),
        MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION,
        "fulltext key definition"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(items, fulltext_key_item_index), 0U),
        "ft_body",
        "fulltext key name"
    );
    key_parts = parser_test_child_at(parser_test_child_at(items, fulltext_key_item_index), 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "body",
        "fulltext key part"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(items, fulltext_index_item_index),
        MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION,
        "fulltext index definition"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(items, fulltext_index_item_index), 0U),
        "ft_title",
        "fulltext index name"
    );
    key_parts = parser_test_child_at(parser_test_child_at(items, fulltext_index_item_index), 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "title",
        "fulltext index part"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(items, fulltext_named_item_index),
        MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION,
        "named fulltext definition"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(items, fulltext_named_item_index), 0U),
        "ft_named",
        "named fulltext name"
    );
    key_parts = parser_test_child_at(parser_test_child_at(items, fulltext_named_item_index), 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 1U),
        "10",
        "fulltext ignored prefix"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(items, fulltext_unnamed_item_index),
        MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION,
        "unnamed fulltext definition"
    );
    key_parts = parser_test_child_at(parser_test_child_at(items, fulltext_unnamed_item_index), 0U);
    failures += parser_test_expect_order_direction(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "fulltext explicit order parser part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE fulltext_no_semicolon (body TEXT, FULLTEXT KEY ft_body (body))",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_constraint_fulltext (body TEXT, "
        "CONSTRAINT c FULLTEXT KEY ft_body (body));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_secondary_using (id INT, KEY k USING BTREE (id));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    items = parser_test_child_at(parser_test_child_at(result.root, 0U), 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(items, 1U), 1U),
        MYLITE_SQL_AST_INDEX_TYPE_OPTION,
        "secondary index type option"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(items, 1U), 1U), 0U),
        "BTREE",
        "secondary index type option name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_pk_prefix (id INT, PRIMARY KEY (id(4)));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE pk_order (id INT, PRIMARY KEY (id DESC));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    primary_key =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 1U), 1U);
    key_parts = parser_test_child_at(primary_key, 0U);
    failures += parser_test_expect_order_direction(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "primary key desc parser part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE pk_using (id INT, PRIMARY KEY USING BTREE (id) COMMENT 'pk');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    primary_key =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 1U), 1U);
    key_parts = parser_test_child_at(primary_key, 0U);
    failures +=
        parser_test_expect_node(key_parts, MYLITE_SQL_AST_PRIMARY_KEY_PART_LIST, "pk using parts");
    failures += parser_test_expect_node(
        parser_test_child_at(primary_key, 1U),
        MYLITE_SQL_AST_INDEX_TYPE_OPTION,
        "primary key type option"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(primary_key, 2U),
        MYLITE_SQL_AST_INDEX_OPTION_LIST,
        "primary key option list"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE UNIQUE INDEX k_complex ON create_idx (v ASC, id DESC) "
        "USING BTREE COMMENT 'Test comment' ALGORITHM INPLACE LOCK SHARED;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_UNIQUE_INDEX_STATEMENT,
        "create index online options"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_create_table_foreign_key_statements(void) {
    enum {
        named_indexed_fk_child_count = 6,
        named_indexed_fk_action_index = 5,
    };

    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *items = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *foreign_key = NULL;
    const struct mylite_sql_ast_node *actions = NULL;
    const struct mylite_sql_ast_node *child_parts = NULL;
    const struct mylite_sql_ast_node *parent_parts = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "CREATE TABLE child (id INT, parent_id INT, CONSTRAINT fk_child_parent "
        "FOREIGN KEY (parent_id) REFERENCES parent (id));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    items = parser_test_child_at(statement, 1U);
    foreign_key = parser_test_child_at(items, 2U);
    failures += parser_test_expect_node(
        foreign_key,
        MYLITE_SQL_AST_FOREIGN_KEY_DEFINITION,
        "named create foreign key definition"
    );
    failures +=
        parser_test_expect_child_count(foreign_key, 4U, "named create foreign key child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(foreign_key, 0U),
        "fk_child_parent",
        "fk name"
    );
    child_parts = parser_test_child_at(foreign_key, 1U);
    parent_parts = parser_test_child_at(foreign_key, 3U);
    failures += parser_test_expect_node(
        child_parts,
        MYLITE_SQL_AST_FOREIGN_KEY_PART_LIST,
        "fk child part list"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(child_parts, 0U),
        "parent_id",
        "fk child part"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(foreign_key, 2U),
        "parent",
        "fk parent table"
    );
    failures += parser_test_expect_node(
        parent_parts,
        MYLITE_SQL_AST_FOREIGN_KEY_PART_LIST,
        "fk parent part list"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parent_parts, 0U),
        "id",
        "fk parent part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE child (parent_id INT, FOREIGN KEY (`parent_id`) REFERENCES app.parent "
        "(`id`));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    foreign_key =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 1U), 1U);
    failures +=
        parser_test_expect_child_count(foreign_key, 3U, "unnamed create foreign key child count");
    child_parts = parser_test_child_at(foreign_key, 0U);
    parent_parts = parser_test_child_at(foreign_key, 2U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(child_parts, 0U),
        "`parent_id`",
        "quoted fk child part"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(foreign_key, 1U),
        "app.parent",
        "qualified fk parent"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parent_parts, 0U),
        "`id`",
        "quoted fk parent part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE child (parent_id INT, FOREIGN KEY parent_idx (`parent_id`) "
        "REFERENCES app.parent (`id`));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    foreign_key =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 1U), 1U);
    failures +=
        parser_test_expect_child_count(foreign_key, 4U, "indexed create foreign key child count");
    failures += parser_test_expect_node(
        parser_test_child_at(foreign_key, 0U),
        MYLITE_SQL_AST_FOREIGN_KEY_INDEX_NAME,
        "fk index name wrapper"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(foreign_key, 0U), 0U),
        "parent_idx",
        "fk index name"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(foreign_key, 2U),
        "app.parent",
        "indexed fk parent"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE child (a INT, b INT, FOREIGN KEY (a, b) REFERENCES parent (a, b));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    foreign_key =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 1U), 2U);
    child_parts = parser_test_child_at(foreign_key, 0U);
    parent_parts = parser_test_child_at(foreign_key, 2U);
    failures += parser_test_expect_child_count(child_parts, 2U, "composite fk child parser parts");
    failures +=
        parser_test_expect_child_count(parent_parts, 2U, "composite fk parent parser parts");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE child (parent_id INT REFERENCES parent (id) ON DELETE CASCADE, other INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    items = parser_test_child_at(statement, 1U);
    column = parser_test_child_at(items, 0U);
    failures +=
        parser_test_expect_child_count(column, 2U, "ignored inline reference column child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "parent_id",
        "ignored inline reference column name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE child (parent_id INT, FOREIGN KEY (parent_id) REFERENCES parent);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE child (parent_id INT, FOREIGN KEY (parent_id) REFERENCES parent (id) "
        "ON DELETE CASCADE);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    foreign_key =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 1U), 1U);
    failures +=
        parser_test_expect_child_count(foreign_key, 4U, "create foreign key action child count");
    actions = parser_test_child_at(foreign_key, 3U);
    failures +=
        parser_test_expect_node(actions, MYLITE_SQL_AST_FOREIGN_KEY_ACTION_LIST, "fk action list");
    failures += parser_test_expect_child_count(actions, 1U, "fk action count");
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 0U),
        MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_CASCADE,
        "delete cascade action"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE child (parent_id INT, CONSTRAINT fk FOREIGN KEY idx_parent (parent_id) "
        "REFERENCES parent (id) ON UPDATE RESTRICT ON DELETE NO ACTION);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    foreign_key =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 1U), 1U);
    failures += parser_test_expect_child_count(
        foreign_key,
        named_indexed_fk_child_count,
        "named indexed create fk action child count"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(foreign_key, 0U),
        "fk",
        "named indexed fk name"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(foreign_key, 1U),
        MYLITE_SQL_AST_FOREIGN_KEY_INDEX_NAME,
        "named indexed fk index"
    );
    actions = parser_test_child_at(foreign_key, named_indexed_fk_action_index);
    failures += parser_test_expect_child_count(actions, 2U, "named indexed fk action count");
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 0U),
        MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_RESTRICT,
        "update restrict action"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 1U),
        MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_NO_ACTION,
        "delete no action action"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE child (parent_id INT, FOREIGN KEY (parent_id) REFERENCES parent (id) "
        "ON DELETE SET NULL ON UPDATE SET NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    foreign_key =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 1U), 1U);
    actions = parser_test_child_at(foreign_key, 3U);
    failures += parser_test_expect_child_count(actions, 2U, "set null fk action count");
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 0U),
        MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_SET_NULL,
        "delete set null action"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 1U),
        MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_SET_NULL,
        "update set null action"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE child (parent_id INT, FOREIGN KEY (parent_id) REFERENCES parent (id) "
        "ON DELETE SET DEFAULT);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_create_index_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *key_parts = NULL;
    int failures = 0;

    failures +=
        parser_test_parse_sql("CREATE INDEX k_v ON create_idx (v);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_CREATE_INDEX_STATEMENT, "create index");
    failures += parser_test_expect_child_count(statement, 3U, "create index child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "k_v",
        "create index name"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "create_idx",
        "create index table"
    );
    key_parts = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_child_count(key_parts, 1U, "create index part count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "v",
        "create index part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE UNIQUE INDEX `u_v` ON app.create_idx (`v`);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_UNIQUE_INDEX_STATEMENT,
        "create unique index"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "`u_v`",
        "create unique index name"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "app.create_idx",
        "create unique index table"
    );
    key_parts = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "`v`",
        "create unique index part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE INDEX k_multi ON create_idx (v, id);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    key_parts = parser_test_child_at(parser_test_child_at(result.root, 0U), 2U);
    failures +=
        parser_test_expect_child_count(key_parts, 2U, "create composite index parser part count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 1U), 0U),
        "id",
        "create composite index part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE INDEX ON create_idx (v);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE INDEX IF NOT EXISTS k_v ON create_idx (v);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE INDEX k_v ON create_idx (create_idx.v);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE INDEX k_v ON create_idx (v(4));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    key_parts = parser_test_child_at(parser_test_child_at(result.root, 0U), 2U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "v",
        "create index prefix column"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 1U),
        "4",
        "create index prefix length"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE INDEX k_v ON create_idx (v DESC);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    key_parts = parser_test_child_at(parser_test_child_at(result.root, 0U), 2U);
    failures += parser_test_expect_order_direction(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "create index desc part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE FULLTEXT INDEX ft_v ON create_idx (v(4));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_FULLTEXT_INDEX_STATEMENT,
        "create fulltext index"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "ft_v",
        "create fulltext index name"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "create_idx",
        "create fulltext index table"
    );
    key_parts = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "v",
        "create fulltext part"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 1U),
        "4",
        "create fulltext prefix"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE FULLTEXT KEY k_v ON create_idx (v);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE INDEX k_v USING BTREE ON create_idx (v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_child_count(statement, 4U, "create index type child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_INDEX_TYPE_OPTION,
        "create index type option"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(statement, 1U), 0U),
        "BTREE",
        "create index type option name"
    );
    key_parts = parser_test_child_at(statement, 3U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "v",
        "create index type part"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_drop_index_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("DROP INDEX k_v ON drop_idx;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_DROP_INDEX_STATEMENT, "drop index");
    failures += parser_test_expect_child_count(statement, 2U, "drop index child count");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(statement, 0U), "k_v", "drop index name");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "drop_idx",
        "drop index table"
    );
    failures +=
        parser_test_expect_span_text(statement, "DROP INDEX k_v ON drop_idx", "drop index span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("DROP INDEX `k_v` ON app.drop_idx;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "`k_v`",
        "quoted drop index name"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "app.drop_idx",
        "qualified drop index table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("DROP INDEX `PRIMARY` ON drop_idx;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "`PRIMARY`",
        "quoted drop primary name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("DROP INDEX PRIMARY ON drop_idx;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "PRIMARY",
        "unquoted drop primary name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("DROP KEY k_v ON drop_idx;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DROP INDEX IF EXISTS k_v ON drop_idx;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DROP INDEX k_v ON drop_idx ALGORITHM=INPLACE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DROP INDEX k_v;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_add_primary_key_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *primary_key = NULL;
    const struct mylite_sql_ast_node *key_parts = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "ALTER TABLE add_pk ADD PRIMARY KEY (id);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_PRIMARY_KEY_STATEMENT,
        "alter add primary key statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "alter add primary key child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "add_pk",
        "alter add primary key table"
    );
    primary_key = parser_test_child_at(statement, 1U);
    key_parts = parser_test_child_at(primary_key, 0U);
    failures += parser_test_expect_node(
        primary_key,
        MYLITE_SQL_AST_PRIMARY_KEY_DEFINITION,
        "alter add primary key definition"
    );
    failures +=
        parser_test_expect_span_text(primary_key, "PRIMARY KEY (id)", "alter add primary key span");
    failures += parser_test_expect_child_count(key_parts, 1U, "alter add primary key part count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(key_parts, 0U),
        "id",
        "alter add primary key part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_pk ADD PRIMARY KEY (id), ALGORITHM=COPY, LOCK=EXCLUSIVE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY,
        "alter add primary key algorithm option"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_EXCLUSIVE,
        "alter add primary key lock option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.add_pk ADD PRIMARY KEY (`id`);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.add_pk",
        "schema-qualified alter add primary key table"
    );
    primary_key = parser_test_child_at(statement, 1U);
    key_parts = parser_test_child_at(primary_key, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "`id`",
        "quoted alter add primary key part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_pk ADD PRIMARY KEY (id, other);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    key_parts =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 1U), 0U);
    failures +=
        parser_test_expect_child_count(key_parts, 2U, "alter add composite pk parser part count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 1U), 0U),
        "other",
        "alter add composite pk part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_pk ADD PRIMARY KEY (add_pk.id);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_pk ADD PRIMARY KEY (add_pk.id DESC);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_pk ADD PRIMARY KEY (id DESC, other ASC);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    key_parts =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 1U), 0U);
    failures += parser_test_expect_order_direction(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "alter add primary key desc part"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(parser_test_child_at(key_parts, 1U), 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_ASC,
        "alter add primary key asc part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_pk ADD CONSTRAINT pk PRIMARY KEY (id);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_PRIMARY_KEY_STATEMENT,
        "alter add named primary key statement"
    );
    primary_key = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_span_text(
        primary_key,
        "PRIMARY KEY (id)",
        "alter add named primary key span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_pk ADD PRIMARY KEY USING BTREE (id) COMMENT 'pk';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    primary_key = parser_test_child_at(parser_test_child_at(result.root, 0U), 1U);
    key_parts = parser_test_child_at(primary_key, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(primary_key, 1U),
        MYLITE_SQL_AST_INDEX_TYPE_OPTION,
        "alter add primary key type option"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(primary_key, 2U),
        MYLITE_SQL_AST_INDEX_OPTION_LIST,
        "alter add primary key option list"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "id",
        "alter pk option part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_pk ADD PRIMARY KEY (id), ADD KEY k_v (v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_add_index_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *secondary_index = NULL;
    const struct mylite_sql_ast_node *key_parts = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "ALTER TABLE add_idx ADD INDEX k_v (v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT,
        "alter add index statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "alter add index child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "add_idx",
        "alter add index table"
    );
    secondary_index = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        secondary_index,
        MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION,
        "alter add index definition"
    );
    failures +=
        parser_test_expect_span_text(secondary_index, "INDEX k_v (v)", "alter add index span");
    failures += parser_test_expect_span_text(
        parser_test_child_at(secondary_index, 0U),
        "k_v",
        "alter add index name"
    );
    key_parts = parser_test_child_at(secondary_index, 1U);
    failures += parser_test_expect_child_count(key_parts, 1U, "alter add index part count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "v",
        "alter add index part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_idx ADD INDEX k_v (v), ALGORITHM=INPLACE, LOCK=NONE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE,
        "alter add index algorithm option"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_NONE,
        "alter add index lock option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.add_idx ADD KEY (`v`);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.add_idx",
        "schema-qualified alter add index table"
    );
    secondary_index = parser_test_child_at(statement, 1U);
    key_parts = parser_test_child_at(secondary_index, 0U);
    failures += parser_test_expect_child_count(
        secondary_index,
        1U,
        "unnamed alter add index definition child count"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "`v`",
        "quoted alter add index part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_idx ADD KEY k_multi (v, id);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    key_parts =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 1U), 1U);
    failures += parser_test_expect_child_count(
        key_parts,
        2U,
        "alter add composite index parser part count"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 1U), 0U),
        "id",
        "alter add composite index part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_idx ADD KEY k_qualified (add_idx.v);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_idx ADD UNIQUE KEY u_v (v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT,
        "alter add unique statement"
    );
    secondary_index = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        secondary_index,
        MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION,
        "alter add unique definition"
    );
    failures += parser_test_expect_span_text(
        secondary_index,
        "UNIQUE KEY u_v (v)",
        "alter add unique span"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(secondary_index, 0U),
        "u_v",
        "alter add unique name"
    );
    key_parts = parser_test_child_at(secondary_index, 1U);
    failures += parser_test_expect_child_count(key_parts, 1U, "alter add unique part count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "v",
        "alter add unique part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_idx ADD UNIQUE (`v`);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    secondary_index = parser_test_child_at(parser_test_child_at(result.root, 0U), 1U);
    failures += parser_test_expect_node(
        secondary_index,
        MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION,
        "unnamed alter add unique definition"
    );
    failures +=
        parser_test_expect_child_count(secondary_index, 1U, "unnamed alter add unique child count");
    key_parts = parser_test_child_at(secondary_index, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "`v`",
        "unnamed alter add unique part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_idx ADD UNIQUE INDEX u_v (v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    secondary_index = parser_test_child_at(parser_test_child_at(result.root, 0U), 1U);
    failures += parser_test_expect_span_text(
        secondary_index,
        "UNIQUE INDEX u_v (v)",
        "alter add unique index keyword span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_idx ADD FULLTEXT KEY ft_v (v(4));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT,
        "alter add fulltext statement"
    );
    secondary_index = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        secondary_index,
        MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION,
        "alter add fulltext definition"
    );
    failures += parser_test_expect_span_text(
        secondary_index,
        "FULLTEXT KEY ft_v (v(4))",
        "alter add fulltext span"
    );
    key_parts = parser_test_child_at(secondary_index, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "v",
        "alter add fulltext part"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 1U),
        "4",
        "alter add fulltext prefix"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_idx ADD CONSTRAINT uq_v UNIQUE (v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT,
        "alter add constraint unique statement"
    );
    secondary_index = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        secondary_index,
        MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION,
        "alter add constraint unique definition"
    );
    failures += parser_test_expect_span_text(
        secondary_index,
        "UNIQUE (v)",
        "alter add constraint unique span"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(secondary_index, 0U),
        "uq_v",
        "alter add constraint unique name"
    );
    key_parts = parser_test_child_at(secondary_index, 1U);
    failures +=
        parser_test_expect_child_count(key_parts, 1U, "alter add constraint unique part count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "v",
        "alter add constraint unique part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_idx ADD CONSTRAINT uq_key UNIQUE KEY visible (v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    secondary_index = parser_test_child_at(parser_test_child_at(result.root, 0U), 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(secondary_index, 0U),
        "visible",
        "alter add constraint unique explicit index name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_idx ADD CONSTRAINT UNIQUE (v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    secondary_index = parser_test_child_at(parser_test_child_at(result.root, 0U), 1U);
    failures += parser_test_expect_child_count(
        secondary_index,
        1U,
        "alter add constraint unnamed unique child count"
    );
    key_parts = parser_test_child_at(secondary_index, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "v",
        "alter add constraint unnamed unique part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_idx ADD CONSTRAINT uq_key UNIQUE KEY (v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    secondary_index = parser_test_child_at(parser_test_child_at(result.root, 0U), 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(secondary_index, 0U),
        "uq_key",
        "alter add constraint unique key name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_idx ADD CONSTRAINT uq_key UNIQUE visible (v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    secondary_index = parser_test_child_at(parser_test_child_at(result.root, 0U), 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(secondary_index, 0U),
        "visible",
        "alter add constraint unique no-keyword explicit index name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_idx ADD CONSTRAINT c FULLTEXT KEY ft_v (v);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_idx ADD INDEX k_v USING BTREE (v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    secondary_index = parser_test_child_at(parser_test_child_at(result.root, 0U), 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(secondary_index, 1U),
        MYLITE_SQL_AST_INDEX_TYPE_OPTION,
        "alter add index type option"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(secondary_index, 1U), 0U),
        "BTREE",
        "alter add index type option name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_idx ADD INDEX k_v (v(4));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    secondary_index = parser_test_child_at(parser_test_child_at(result.root, 0U), 1U);
    key_parts = parser_test_child_at(secondary_index, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 0U),
        "v",
        "alter add index prefix column"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 1U),
        "4",
        "alter add index prefix length"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_idx ADD INDEX k_v (v DESC);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    secondary_index = parser_test_child_at(parser_test_child_at(result.root, 0U), 1U);
    key_parts = parser_test_child_at(secondary_index, 1U);
    failures += parser_test_expect_order_direction(
        parser_test_child_at(parser_test_child_at(key_parts, 0U), 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "alter add index desc part"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_add_foreign_key_statements(void) {
    enum {
        alter_fk_action_child_count = 5,
        alter_fk_action_list_index = 4,
    };

    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *foreign_key = NULL;
    const struct mylite_sql_ast_node *actions = NULL;
    const struct mylite_sql_ast_node *child_parts = NULL;
    const struct mylite_sql_ast_node *parent_parts = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "ALTER TABLE child ADD CONSTRAINT fk_child_parent FOREIGN KEY (parent_id) "
        "REFERENCES parent (id);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_FOREIGN_KEY_STATEMENT,
        "alter add foreign key statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "alter add foreign key child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "child",
        "alter fk child table"
    );
    foreign_key = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        foreign_key,
        MYLITE_SQL_AST_FOREIGN_KEY_DEFINITION,
        "alter fk definition"
    );
    failures += parser_test_expect_child_count(foreign_key, 4U, "alter fk definition child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(foreign_key, 0U),
        "fk_child_parent",
        "alter fk name"
    );
    child_parts = parser_test_child_at(foreign_key, 1U);
    parent_parts = parser_test_child_at(foreign_key, 3U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(child_parts, 0U),
        "parent_id",
        "alter fk child part"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(foreign_key, 2U),
        "parent",
        "alter fk parent table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parent_parts, 0U),
        "id",
        "alter fk parent part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE child ADD FOREIGN KEY (parent_id) REFERENCES parent (id), "
        "ALGORITHM=INPLACE, LOCK=NONE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE,
        "alter add foreign key algorithm option"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_NONE,
        "alter add foreign key lock option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.child ADD CONSTRAINT `fk` FOREIGN KEY (`parent_id`) "
        "REFERENCES app.parent (`id`);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    foreign_key = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.child",
        "qualified alter fk child"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(foreign_key, 0U),
        "`fk`",
        "quoted alter fk name"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(foreign_key, 1U), 0U),
        "`parent_id`",
        "quoted child"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(foreign_key, 2U),
        "app.parent",
        "qualified parent"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(foreign_key, 3U), 0U),
        "`id`",
        "quoted parent"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE child ADD FOREIGN KEY (parent_id) REFERENCES parent (id);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    foreign_key = parser_test_child_at(statement, 1U);
    failures +=
        parser_test_expect_child_count(foreign_key, 3U, "unnamed alter fk definition child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "child",
        "unnamed alter fk child table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(foreign_key, 1U),
        "parent",
        "unnamed alter fk parent"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE child ADD FOREIGN KEY idx_parent (parent_id) REFERENCES parent (id);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    foreign_key = parser_test_child_at(statement, 1U);
    failures +=
        parser_test_expect_child_count(foreign_key, 4U, "indexed alter fk definition child count");
    failures += parser_test_expect_node(
        parser_test_child_at(foreign_key, 0U),
        MYLITE_SQL_AST_FOREIGN_KEY_INDEX_NAME,
        "indexed alter fk index node"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(foreign_key, 0U), 0U),
        "idx_parent",
        "alter fk index"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE child ADD CONSTRAINT fk FOREIGN KEY (parent_id) REFERENCES parent (id) "
        "ON UPDATE CASCADE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    foreign_key = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(
        foreign_key,
        alter_fk_action_child_count,
        "alter fk action child count"
    );
    actions = parser_test_child_at(foreign_key, alter_fk_action_list_index);
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 0U),
        MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_CASCADE,
        "alter update cascade action"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE child ADD CONSTRAINT fk FOREIGN KEY (parent_id) REFERENCES parent (id) "
        "ON UPDATE SET DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_drop_foreign_key_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "ALTER TABLE child DROP FOREIGN KEY fk_child_parent;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_FOREIGN_KEY_STATEMENT,
        "alter drop foreign key statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "alter drop foreign key child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "child",
        "alter drop fk table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "fk_child_parent",
        "alter drop fk name"
    );
    failures += parser_test_expect_span_text(
        statement,
        "ALTER TABLE child DROP FOREIGN KEY fk_child_parent",
        "alter drop fk span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.child DROP FOREIGN KEY `MiXeD_FK`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.child",
        "qualified alter drop fk table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "`MiXeD_FK`",
        "quoted alter drop fk name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE child DROP FOREIGN KEY IF EXISTS fk_child_parent;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE child DROP FOREIGN KEY fk_child_parent, DROP INDEX fk_child_parent;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE child DROP FOREIGN KEY fk_child_parent, ALGORITHM=INPLACE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE,
        "alter drop foreign key algorithm option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE child DROP CONSTRAINT fk_child_parent;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_CONSTRAINT_STATEMENT,
        "alter drop constraint foreign key statement"
    );
    failures +=
        parser_test_expect_child_count(statement, 2U, "alter drop constraint fk child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "child",
        "alter drop constraint fk table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "fk_child_parent",
        "alter drop constraint fk name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE child DROP CONSTRAINT IF EXISTS fk_child_parent;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_drop_index_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures +=
        parser_test_parse_sql("ALTER TABLE drop_idx DROP INDEX k_v;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_INDEX_STATEMENT,
        "alter drop index statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "alter drop index child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "drop_idx",
        "alter drop index table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "k_v",
        "alter drop index name"
    );
    failures += parser_test_expect_span_text(
        statement,
        "ALTER TABLE drop_idx DROP INDEX k_v",
        "alter drop index span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.drop_idx DROP KEY `k_v`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.drop_idx",
        "schema-qualified alter drop index table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "`k_v`",
        "quoted alter drop key name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE drop_idx DROP INDEX `PRIMARY`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "`PRIMARY`",
        "quoted primary drop name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE drop_idx DROP INDEX PRIMARY;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE drop_idx DROP INDEX k_v, ALGORITHM=INPLACE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE,
        "alter drop index algorithm option"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_rename_index_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX k_old TO k_new;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_RENAME_INDEX_STATEMENT,
        "alter rename index statement"
    );
    failures += parser_test_expect_child_count(statement, 3U, "alter rename index child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "rename_idx",
        "alter rename index table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "k_old",
        "alter rename old name"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 2U),
        "k_new",
        "alter rename new name"
    );
    failures += parser_test_expect_span_text(
        statement,
        "ALTER TABLE rename_idx RENAME INDEX k_old TO k_new",
        "alter rename index span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.rename_idx RENAME KEY `k_old` TO `k_new`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.rename_idx",
        "schema-qualified alter rename index table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "`k_old`",
        "quoted rename old name"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 2U),
        "`k_new`",
        "quoted rename new name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX `PRIMARY` TO `renamed`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "`PRIMARY`",
        "quoted primary old name"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 2U),
        "`renamed`",
        "quoted renamed name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX k_old TO `PRIMARY`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 2U),
        "`PRIMARY`",
        "quoted primary new name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX PRIMARY TO k_new;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX k_old TO PRIMARY;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX rename_idx.k_old TO k_new;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX k_old TO rename_idx.k_new;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX k_old k_new;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX k_old TO k_new, RENAME INDEX k2 TO k3;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX k_old TO k_new, ALGORITHM=INPLACE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE,
        "alter rename index algorithm option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX k_old TO k_new, LOCK=NONE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_NONE,
        "alter rename index lock option"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_index_visibility_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "ALTER TABLE idx_visibility ALTER INDEX k_v INVISIBLE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_INDEX_VISIBILITY_STATEMENT,
        "alter index invisible statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "alter index invisible child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "idx_visibility",
        "alter index table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "k_v",
        "alter index name"
    );
    failures += parser_test_expect_column_visibility(
        statement,
        MYLITE_SQL_AST_COLUMN_VISIBILITY_INVISIBLE,
        "alter index invisible payload"
    );
    failures += parser_test_expect_span_text(
        statement,
        "ALTER TABLE idx_visibility ALTER INDEX k_v INVISIBLE",
        "alter index invisible span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.idx_visibility ALTER INDEX `k_v` VISIBLE, ALGORITHM=COPY, LOCK=NONE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.idx_visibility",
        "schema-qualified alter index visibility table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "`k_v`",
        "quoted alter index name"
    );
    failures += parser_test_expect_column_visibility(
        statement,
        MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE,
        "alter index visible payload"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY,
        "alter index visibility algorithm option"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_NONE,
        "alter index visibility lock option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE idx_visibility ALTER INDEX `PRIMARY` INVISIBLE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "`PRIMARY`",
        "quoted primary index"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE idx_visibility ALTER INDEX PRIMARY INVISIBLE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE idx_visibility ALTER KEY k_v INVISIBLE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE idx_visibility ALTER INDEX idx_visibility.k_v INVISIBLE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE idx_visibility ALTER INDEX k_v INVISIBLE, ALTER INDEX k2 VISIBLE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_check_constraint_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *check = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "ALTER TABLE checked ADD CHECK (a > 0);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    check = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_CHECK_STATEMENT,
        "alter add check statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "alter add check child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "checked",
        "alter add check table"
    );
    failures +=
        parser_test_expect_node(check, MYLITE_SQL_AST_CHECK_CONSTRAINT_DEFINITION, "added CHECK");
    failures += parser_test_expect_span_text(
        statement,
        "ALTER TABLE checked ADD CHECK (a > 0)",
        "add span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.checked ADD CONSTRAINT positive CHECK ((a > 0) AND (b IS NOT NULL)) "
        "NOT ENFORCED;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    check = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.checked",
        "schema-qualified alter add check table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(check, 1U),
        "positive",
        "explicit alter CHECK name"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(check, 2U),
        MYLITE_SQL_AST_CHECK_ENFORCEMENT_NOT_ENFORCED,
        "alter CHECK not enforced"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.checked ADD CONSTRAINT positive CHECK (a > 0), ALGORITHM=COPY;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_CHECK_STATEMENT,
        "alter add check algorithm statement"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY,
        "alter add check algorithm option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE checked DROP CHECK positive;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_CHECK_STATEMENT,
        "alter drop check statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "alter drop check child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "positive",
        "alter drop check name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE checked DROP CHECK positive, ALGORITHM=INPLACE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_CHECK_STATEMENT,
        "alter drop check algorithm statement"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE,
        "alter drop check algorithm option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE checked ALTER CHECK positive NOT ENFORCED;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ALTER_CHECK_STATEMENT,
        "alter alter check statement"
    );
    failures += parser_test_expect_child_count(statement, 3U, "alter alter check child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_CHECK_ENFORCEMENT_NOT_ENFORCED,
        "alter check not enforced"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE checked ALTER CHECK positive ENFORCED;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_CHECK_ENFORCEMENT_ENFORCED,
        "alter check enforced"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE checked ALTER CHECK positive ENFORCED, ALGORITHM=COPY;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ALTER_CHECK_STATEMENT,
        "alter check algorithm statement"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY,
        "alter check algorithm option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE checked DROP CONSTRAINT positive;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_CONSTRAINT_STATEMENT,
        "alter drop constraint check statement"
    );
    failures +=
        parser_test_expect_child_count(statement, 2U, "alter drop constraint check child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "checked",
        "alter drop constraint table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "positive",
        "alter drop constraint name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE checked ALTER CHECK positive;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE checked ADD CHECK (a > 0), ADD CHECK (b > 0);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_drop_primary_key_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "ALTER TABLE drop_pk DROP PRIMARY KEY;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_PRIMARY_KEY_STATEMENT,
        "alter drop primary key statement"
    );
    failures += parser_test_expect_child_count(statement, 1U, "alter drop primary key child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "drop_pk",
        "alter drop primary table"
    );
    failures += parser_test_expect_span_text(
        statement,
        "ALTER TABLE drop_pk DROP PRIMARY KEY",
        "alter drop pk span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE drop_pk DROP PRIMARY KEY, ALGORITHM=COPY, LOCK=EXCLUSIVE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY,
        "alter drop primary key algorithm option"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_EXCLUSIVE,
        "alter drop primary key lock option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.drop_pk DROP PRIMARY KEY;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.drop_pk",
        "schema-qualified alter drop primary key table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE drop_pk DROP PRIMARY KEY, ADD KEY k_v (v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE drop_pk DROP INDEX PRIMARY;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE drop_pk DROP CONSTRAINT `PRIMARY`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_CONSTRAINT_STATEMENT,
        "alter drop constraint primary statement"
    );
    failures +=
        parser_test_expect_child_count(statement, 2U, "alter drop constraint primary child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "`PRIMARY`",
        "alter drop constraint primary quoted name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE drop_pk DROP CONSTRAINT `PRIMARY`, ALGORITHM=COPY;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY,
        "alter drop constraint algorithm option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE drop_pk DROP CONSTRAINT PRIMARY;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE drop_pk DROP CONSTRAINT `PRIMARY`, DROP INDEX k_v;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_auto_increment_option_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *option = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "ALTER TABLE auto_counter AUTO_INCREMENT=10;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    option = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_AUTO_INCREMENT_STATEMENT,
        "alter table auto increment statement"
    );
    failures +=
        parser_test_expect_child_count(statement, 2U, "alter table auto increment child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "auto_counter",
        "alter auto increment table"
    );
    failures += parser_test_expect_node(
        option,
        MYLITE_SQL_AST_TABLE_AUTO_INCREMENT_OPTION,
        "alter auto option"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(option, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "alter auto value"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(option, 0U),
        "10",
        "alter auto value span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.auto_counter AUTO_INCREMENT 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    option = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.auto_counter",
        "schema-qualified alter auto increment table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(option, 0U),
        "0",
        "alter auto increment zero value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE auto_counter AUTO_INCREMENT=-1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE auto_counter AUTO_INCREMENT=+1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE auto_counter AUTO_INCREMENT='10';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE auto_counter AUTO_INCREMENT=NULL;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE auto_counter AUTO_INCREMENT=1.5;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE auto_counter AUTO_INCREMENT=10, ADD COLUMN other INT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}
