#include "parser_test_support.h"

static int test_set_fixed_system_variable_statement(void);
static int test_sql_prepared_statement_lifecycle(void);
static int test_set_transaction_statement(void);
static int test_insert_select_statement(void);
static int test_insert_modifier_statements(void);
static int test_insert_on_duplicate_key_update_statement(void);
static int test_replace_select_statement(void);
static int test_replace_modifier_statements(void);
static int test_load_data_infile_statement(void);
static int test_delete_statement(void);
static int test_update_statement(void);
static int test_transaction_control_statements(void);
static int test_table_lock_statements(void);

int main(void) {
    int failures = 0;

    failures += test_set_fixed_system_variable_statement();
    failures += test_sql_prepared_statement_lifecycle();
    failures += test_set_transaction_statement();
    failures += test_insert_select_statement();
    failures += test_insert_modifier_statements();
    failures += test_insert_on_duplicate_key_update_statement();
    failures += test_replace_select_statement();
    failures += test_replace_modifier_statements();
    failures += test_load_data_infile_statement();
    failures += test_delete_statement();
    failures += test_update_statement();
    failures += test_transaction_control_statements();
    failures += test_table_lock_statements();

    return failures == 0 ? 0 : 1;
}

static int test_set_fixed_system_variable_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *assignment_list = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    const struct mylite_sql_ast_node *target = NULL;
    const struct mylite_sql_ast_node *value = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SET autocommit = 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    assignment_list = parser_test_child_at(statement, 0U);
    assignment = parser_test_child_at(assignment_list, 0U);
    target = parser_test_child_at(assignment, 0U);
    value = parser_test_child_at(assignment, 1U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_SET_STATEMENT, "set statement");
    failures += parser_test_expect_child_count(statement, 1U, "set statement children");
    failures += parser_test_expect_node(
        assignment_list,
        MYLITE_SQL_AST_SET_ASSIGNMENT_LIST,
        "set assignment list"
    );
    failures += parser_test_expect_child_count(assignment_list, 1U, "set assignment count");
    failures +=
        parser_test_expect_node(assignment, MYLITE_SQL_AST_SET_ASSIGNMENT, "set assignment");
    failures += parser_test_expect_child_count(assignment, 2U, "set assignment children");
    failures += parser_test_expect_node(
        target,
        MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_TARGET,
        "set system variable target"
    );
    failures += parser_test_expect_child_count(target, 1U, "set target unscoped child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(target, 0U),
        "autocommit",
        "set unscoped name"
    );
    failures += parser_test_expect_literal(
        value,
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "set integer fixed value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SET SESSION `autocommit` = ON;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 0U), 0U);
    target = parser_test_child_at(assignment, 0U);
    value = parser_test_child_at(assignment, 1U);
    failures += parser_test_expect_child_count(target, 2U, "set target scoped child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(target, 0U),
        "SESSION",
        "set target session scope"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(target, 1U),
        "`autocommit`",
        "set quoted name"
    );
    failures +=
        parser_test_expect_literal(value, MYLITE_SQL_AST_LITERAL_TRUE, "set ON fixed value");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SET LOCAL sql_warnings = OFF;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 0U), 0U);
    target = parser_test_child_at(assignment, 0U);
    value = parser_test_child_at(assignment, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(target, 0U),
        "LOCAL",
        "set target local scope"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(target, 1U),
        "sql_warnings",
        "set local name"
    );
    failures +=
        parser_test_expect_literal(value, MYLITE_SQL_AST_LITERAL_FALSE, "set OFF fixed value");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SET @@session.sql_mode = DEFAULT;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 0U), 0U);
    target = parser_test_child_at(assignment, 0U);
    value = parser_test_child_at(assignment, 1U);
    failures +=
        parser_test_expect_child_count(target, 1U, "set system variable target child count");
    failures += parser_test_expect_node(
        parser_test_child_at(target, 0U),
        MYLITE_SQL_AST_SYSTEM_VARIABLE,
        "set system variable token target"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(target, 0U),
        "@@session.sql_mode",
        "set @@ target"
    );
    failures +=
        parser_test_expect_node(value, MYLITE_SQL_AST_SET_DEFAULT_VALUE, "set default value");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SET sql_mode = "
        "'ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,"
        "ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 0U), 0U);
    failures += parser_test_expect_literal(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "set sql_mode string value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SET time_zone = SYSTEM;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 0U), 0U);
    value = parser_test_child_at(assignment, 1U);
    failures +=
        parser_test_expect_node(value, MYLITE_SQL_AST_IDENTIFIER, "set time_zone SYSTEM value");
    failures += parser_test_expect_span_text(value, "SYSTEM", "set time_zone SYSTEM span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SET time_zone = UTC;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 0U), 0U);
    value = parser_test_child_at(assignment, 1U);
    failures +=
        parser_test_expect_node(value, MYLITE_SQL_AST_IDENTIFIER, "set time_zone UTC value");
    failures += parser_test_expect_span_text(value, "UTC", "set time_zone UTC span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SET transaction_isolation = SERIALIZABLE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 0U), 0U);
    value = parser_test_child_at(assignment, 1U);
    failures += parser_test_expect_node(
        value,
        MYLITE_SQL_AST_IDENTIFIER,
        "set transaction_isolation SERIALIZABLE value"
    );
    failures += parser_test_expect_span_text(
        value,
        "SERIALIZABLE",
        "set transaction_isolation SERIALIZABLE span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SET time_zone = NULL;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 0U), 0U);
    value = parser_test_child_at(assignment, 1U);
    failures +=
        parser_test_expect_literal(value, MYLITE_SQL_AST_LITERAL_NULL, "set time_zone NULL value");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SET autocommit = 1, sql_notes = 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    assignment_list = parser_test_child_at(statement, 0U);
    failures +=
        parser_test_expect_child_count(assignment_list, 2U, "set assignment list supports commas");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SET @name = 1, @other := @@sql_mode;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    assignment_list = parser_test_child_at(statement, 0U);
    assignment = parser_test_child_at(assignment_list, 0U);
    failures +=
        parser_test_expect_child_count(assignment_list, 2U, "user variable assignment count");
    failures += parser_test_expect_node(
        parser_test_child_at(assignment, 0U),
        MYLITE_SQL_AST_USER_VARIABLE,
        "user target"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(assignment, 0U),
        "@name",
        "user target span"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "user value"
    );
    assignment = parser_test_child_at(assignment_list, 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(assignment, 0U),
        MYLITE_SQL_AST_USER_VARIABLE,
        "second user target"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_SYSTEM_VARIABLE,
        "user system value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SET @d = DEFAULT;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SET app.autocommit = 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_sql_prepared_statement_lifecycle(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *using_list = NULL;
    int failures = 0;

    failures +=
        parser_test_parse_sql("PREPARE stmt FROM 'SELECT ?';", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_PREPARE_STATEMENT, "prepare statement");
    failures += parser_test_expect_child_count(statement, 2U, "prepare child count");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(statement, 0U), "stmt", "prepare name");
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "prepare source"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("PREPARE `MiXeD` FROM @sql;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "`MiXeD`",
        "quoted prepare name"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_USER_VARIABLE,
        "prepare user source"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("EXECUTE stmt;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_EXECUTE_STATEMENT, "execute statement");
    failures += parser_test_expect_child_count(statement, 1U, "execute child count");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(statement, 0U), "stmt", "execute name");
    failures += parser_test_expect_true(
        parser_test_child_at(statement, 1U) == NULL,
        "execute without using list"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("EXECUTE stmt USING @a, @`b-c`;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    using_list = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        using_list,
        MYLITE_SQL_AST_EXECUTE_USING_LIST,
        "execute using list"
    );
    failures += parser_test_expect_child_count(using_list, 2U, "execute using count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(using_list, 0U),
        "@a",
        "execute first variable"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(using_list, 1U),
        "@`b-c`",
        "execute second variable"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DEALLOCATE PREPARE stmt;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_DEALLOCATE_PREPARE_STATEMENT,
        "deallocate prepare statement"
    );
    failures += parser_test_expect_child_count(statement, 1U, "deallocate child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "stmt",
        "deallocate name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DROP PREPARE stmt;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_DEALLOCATE_PREPARE_STATEMENT,
        "drop prepare statement"
    );
    failures += parser_test_expect_span_text(statement, "DROP PREPARE stmt", "drop prepare span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ?;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("PREPARE stmt FROM SELECT;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("EXECUTE stmt USING 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("DEALLOCATE stmt;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_set_transaction_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *characteristics = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SET TRANSACTION ISOLATION LEVEL READ COMMITTED;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    characteristics = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SET_TRANSACTION_STATEMENT,
        "set transaction statement"
    );
    failures += parser_test_expect_child_count(statement, 1U, "set transaction child count");
    failures += parser_test_expect_span_text(
        statement,
        "SET TRANSACTION ISOLATION LEVEL READ COMMITTED",
        "set transaction span"
    );
    failures += parser_test_expect_node(
        characteristics,
        MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST,
        "set transaction characteristic list"
    );
    failures +=
        parser_test_expect_child_count(characteristics, 1U, "set transaction isolation count");
    failures += parser_test_expect_node(
        parser_test_child_at(characteristics, 0U),
        MYLITE_SQL_AST_TRANSACTION_ISOLATION_READ_COMMITTED,
        "set transaction read committed"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(characteristics, 0U),
        "READ COMMITTED",
        "read committed span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SET SESSION TRANSACTION READ ONLY;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    characteristics = parser_test_child_at(statement, 1U);
    failures +=
        parser_test_expect_child_count(statement, 2U, "set session transaction child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "SESSION",
        "set transaction scope"
    );
    failures += parser_test_expect_child_count(characteristics, 1U, "set transaction access count");
    failures += parser_test_expect_node(
        parser_test_child_at(characteristics, 0U),
        MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_ONLY,
        "set transaction read only"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(characteristics, 0U),
        "READ ONLY",
        "read only span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SET GLOBAL TRANSACTION READ WRITE, ISOLATION LEVEL SERIALIZABLE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    characteristics = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "GLOBAL",
        "set global scope"
    );
    failures += parser_test_expect_child_count(characteristics, 2U, "set transaction mixed count");
    failures += parser_test_expect_node(
        parser_test_child_at(characteristics, 0U),
        MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_WRITE,
        "set transaction read write"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(characteristics, 1U),
        MYLITE_SQL_AST_TRANSACTION_ISOLATION_SERIALIZABLE,
        "set transaction serializable"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "set session transaction isolation level repeatable read, read write;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    characteristics = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(characteristics, 0U),
        MYLITE_SQL_AST_TRANSACTION_ISOLATION_REPEATABLE_READ,
        "set transaction repeatable read"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(characteristics, 1U),
        MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_WRITE,
        "set transaction lower read write"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SET LOCAL TRANSACTION READ WRITE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SET TRANSACTION;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SET TRANSACTION READ WRITE, READ ONLY;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SET TRANSACTION ISOLATION LEVEL READ COMMITTED, ISOLATION LEVEL SERIALIZABLE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SET TRANSACTION ISOLATION LEVEL READ-COMMITTED;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_insert_select_statement(void) {
    enum { insert_select_duplicate_modifier_child_count = 6 };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "INSERT INTO app.simple_lifecycle (id, amount) "
        "SELECT id, amount FROM app.source_lifecycle WHERE id >= 1 ORDER BY amount DESC LIMIT 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SELECT_STATEMENT,
        "insert select statement"
    );
    failures +=
        parser_test_expect_child_count(statement, 3U, "insert select statement child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.simple_lifecycle",
        "insert select target"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_IDENTIFIER_LIST,
        "insert select target columns"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(statement, 1U),
        2U,
        "insert select target count"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "insert source"
    );
    failures += parser_test_expect_node(
        parser_test_first_child_kind(
            parser_test_child_at(statement, 2U),
            MYLITE_SQL_AST_ORDER_BY_CLAUSE
        ),
        MYLITE_SQL_AST_ORDER_BY_CLAUSE,
        "insert select order clause"
    );
    failures += parser_test_expect_node(
        parser_test_first_child_kind(
            parser_test_child_at(statement, 2U),
            MYLITE_SQL_AST_LIMIT_CLAUSE
        ),
        MYLITE_SQL_AST_LIMIT_CLAUSE,
        "insert select limit clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT simple_lifecycle SELECT * FROM source_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SELECT_STATEMENT,
        "insert select no into"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "simple_lifecycle",
        "insert select target"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(statement, 1U),
        0U,
        "insert select implicit columns"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO app.simple_lifecycle (id, amount) "
        "SELECT 1, 'ok' FROM DUAL WHERE NOT EXISTS (SELECT 1 FROM app.guard WHERE id = 1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SELECT_STATEMENT,
        "insert select dual where statement"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "dual source"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(statement, 2U), 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "insert select dual source"
    );
    failures += parser_test_expect_node(
        parser_test_first_child_kind(
            parser_test_child_at(statement, 2U),
            MYLITE_SQL_AST_WHERE_CLAUSE
        ),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "insert select dual where clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO app.simple_lifecycle (id, amount) "
        "SELECT 1, 'ok' FROM DUAL WHERE 1 = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SELECT_STATEMENT,
        "insert select dual scalar where statement"
    );
    failures += parser_test_expect_node(
        parser_test_first_child_kind(
            parser_test_child_at(statement, 2U),
            MYLITE_SQL_AST_WHERE_CLAUSE
        ),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "insert select dual scalar where clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO app.simple_lifecycle (id, amount) "
        "SELECT id, amount FROM app.source_a UNION ALL SELECT id, amount FROM app.source_b;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SELECT_STATEMENT,
        "insert select union source statement"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT,
        "insert select union source"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO app.simple_lifecycle (id, amount) "
        "SELECT id, amount FROM app.source_lifecycle "
        "ON DUPLICATE KEY UPDATE amount = VALUES(amount), id = DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SELECT_STATEMENT,
        "insert select duplicate statement"
    );
    failures +=
        parser_test_expect_child_count(statement, 4U, "insert select duplicate child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE,
        "insert select duplicate clause"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(parser_test_child_at(statement, 3U), 0U),
        2U,
        "insert select duplicate assignment count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT HIGH_PRIORITY IGNORE INTO app.simple_lifecycle (id) "
        "SELECT id FROM app.source_lifecycle "
        "ON DUPLICATE KEY UPDATE id = VALUES(id);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SELECT_STATEMENT,
        "insert select duplicate modifiers"
    );
    failures += parser_test_expect_child_count(
        statement,
        insert_select_duplicate_modifier_child_count,
        "insert select duplicate modifiers child count"
    );
    failures += parser_test_expect_node(
        parser_test_first_child_kind(statement, MYLITE_SQL_AST_INSERT_HIGH_PRIORITY_MODIFIER),
        MYLITE_SQL_AST_INSERT_HIGH_PRIORITY_MODIFIER,
        "insert select duplicate priority modifier"
    );
    failures += parser_test_expect_node(
        parser_test_first_child_kind(statement, MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER),
        MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER,
        "insert select duplicate ignore modifier"
    );
    failures += parser_test_expect_node(
        parser_test_first_child_kind(statement, MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE),
        MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE,
        "insert select duplicate modifier clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO app.simple_lifecycle (id, amount) "
        "SELECT 1, 2 FROM DUAL ON DUPLICATE KEY UPDATE amount = VALUES(amount);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "insert select duplicate dual source"
    );
    failures += parser_test_expect_node(
        parser_test_first_child_kind(statement, MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE),
        MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE,
        "insert select duplicate dual clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO app.simple_lifecycle (id, amount) "
        "SELECT id, amount FROM app.source_a UNION ALL SELECT id, amount FROM app.source_b "
        "ON DUPLICATE KEY UPDATE amount = VALUES(amount);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT,
        "insert select duplicate union source"
    );
    failures += parser_test_expect_node(
        parser_test_first_child_kind(statement, MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE),
        MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE,
        "insert select duplicate union clause"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_insert_modifier_statements(void) {
    const size_t priority_ignore_child_count = 5U;
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "INSERT LOW_PRIORITY INTO app.simple_lifecycle (id) VALUES (1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_STATEMENT,
        "low priority insert values"
    );
    failures +=
        parser_test_expect_child_count(statement, 4U, "low priority insert values child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_LOW_PRIORITY_MODIFIER,
        "low priority insert values modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT HIGH_PRIORITY simple_lifecycle VALUES (2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_STATEMENT,
        "high priority insert values"
    );
    failures +=
        parser_test_expect_child_count(statement, 4U, "high priority insert values child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_HIGH_PRIORITY_MODIFIER,
        "high priority insert values modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT IGNORE INTO app.simple_lifecycle (id) VALUES (3);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "ignore insert values");
    failures += parser_test_expect_child_count(statement, 4U, "ignore insert values child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER,
        "ignore insert values modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT LOW_PRIORITY IGNORE INTO app.simple_lifecycle (id) VALUES (4);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_STATEMENT,
        "low priority ignore values"
    );
    failures += parser_test_expect_child_count(
        statement,
        priority_ignore_child_count,
        "low priority ignore values child count"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_LOW_PRIORITY_MODIFIER,
        "low priority ignore values priority modifier"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 4U),
        MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER,
        "low priority ignore values ignore modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("INSERT simple_lifecycle VALUES (3);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_STATEMENT,
        "insert values no into"
    );
    failures += parser_test_expect_child_count(statement, 3U, "insert values no into child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "simple_lifecycle",
        "insert values target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT DELAYED INTO app.simple_lifecycle SET id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SET_STATEMENT,
        "delayed insert set"
    );
    failures += parser_test_expect_child_count(statement, 3U, "delayed insert set child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_INSERT_DELAYED_MODIFIER,
        "delayed insert set modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT DELAYED IGNORE INTO app.simple_lifecycle SET id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SET_STATEMENT,
        "delayed ignore insert set"
    );
    failures +=
        parser_test_expect_child_count(statement, 4U, "delayed ignore insert set child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_INSERT_DELAYED_MODIFIER,
        "delayed ignore insert set delayed modifier"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER,
        "delayed ignore insert set ignore modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO app.simple_lifecycle (id) VALUES (DEFAULT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "insert default");
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_DML_DEFAULT_VALUE,
        "insert values default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO app.simple_lifecycle (id, expires_at, nullable_at) VALUES "
        "(UNIX_TIMESTAMP(), UNIX_TIMESTAMP() + +60, UNIX_TIMESTAMP() - NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_STATEMENT,
        "insert unix timestamp values"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION,
        "insert unix timestamp bare"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 1U),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "insert unix timestamp plus"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 1U),
            0U
        ),
        MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION,
        "insert unix timestamp plus source"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(
            parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 1U),
            1U
        ),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "insert unix timestamp plus signed delta"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 2U),
        MYLITE_SQL_AST_OPERATOR_SUBTRACT,
        "insert unix timestamp null subtract"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 2U),
            1U
        ),
        MYLITE_SQL_AST_LITERAL_NULL,
        "insert unix timestamp null delta"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO app.simple_lifecycle SET id = DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SET_STATEMENT,
        "insert set default"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 0U), 1U),
        MYLITE_SQL_AST_DML_DEFAULT_VALUE,
        "insert set default value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO app.simple_lifecycle SET id = UNIX_TIMESTAMP() - -1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SET_STATEMENT,
        "insert set unix timestamp"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 0U), 1U),
        MYLITE_SQL_AST_OPERATOR_SUBTRACT,
        "insert set unix timestamp subtract"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(
            parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 0U), 1U),
            1U
        ),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "insert set unix timestamp negative delta"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT HIGH_PRIORITY INTO app.simple_lifecycle (id) SELECT id FROM app.source_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SELECT_STATEMENT,
        "high priority insert select"
    );
    failures +=
        parser_test_expect_child_count(statement, 4U, "high priority insert select child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_HIGH_PRIORITY_MODIFIER,
        "high priority insert select modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT IGNORE INTO app.simple_lifecycle (id) SELECT id FROM app.source_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SELECT_STATEMENT,
        "ignore insert select"
    );
    failures += parser_test_expect_child_count(statement, 4U, "ignore insert select child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER,
        "ignore insert select modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT LOW_PRIORITY IGNORE INTO app.simple_lifecycle (id) "
        "SELECT id FROM app.source_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SELECT_STATEMENT,
        "low priority ignore insert select"
    );
    failures += parser_test_expect_child_count(
        statement,
        priority_ignore_child_count,
        "low priority ignore insert select child count"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_LOW_PRIORITY_MODIFIER,
        "low priority ignore insert select priority modifier"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 4U),
        MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER,
        "low priority ignore insert select ignore modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT HIGH_PRIORITY IGNORE simple_lifecycle SELECT * FROM source_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SELECT_STATEMENT,
        "high priority ignore insert select"
    );
    failures += parser_test_expect_child_count(
        statement,
        priority_ignore_child_count,
        "high priority ignore insert select child count"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_HIGH_PRIORITY_MODIFIER,
        "high priority ignore insert select priority modifier"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 4U),
        MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER,
        "high priority ignore insert select ignore modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT DELAYED simple_lifecycle SELECT * FROM source_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SELECT_STATEMENT,
        "delayed insert select"
    );
    failures += parser_test_expect_child_count(statement, 4U, "delayed insert select child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_DELAYED_MODIFIER,
        "delayed insert select modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT DELAYED IGNORE simple_lifecycle SELECT * FROM source_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SELECT_STATEMENT,
        "delayed ignore insert select"
    );
    failures += parser_test_expect_child_count(
        statement,
        priority_ignore_child_count,
        "delayed ignore insert select child count"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_DELAYED_MODIFIER,
        "delayed ignore insert select delayed modifier"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 4U),
        MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER,
        "delayed ignore insert select ignore modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_insert_on_duplicate_key_update_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *clause = NULL;
    const struct mylite_sql_ast_node *assignments = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    const struct mylite_sql_ast_node *value = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "INSERT INTO app.simple_lifecycle (id, amount) VALUES (1, 2) "
        "ON DUPLICATE KEY UPDATE amount = VALUES(amount);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    clause = parser_test_child_at(statement, 3U);
    assignments = parser_test_child_at(clause, 0U);
    assignment = parser_test_child_at(assignments, 0U);
    value = parser_test_child_at(assignment, 1U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "duplicate insert");
    failures += parser_test_expect_child_count(statement, 4U, "duplicate insert child count");
    failures += parser_test_expect_node(
        clause,
        MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE,
        "duplicate insert clause"
    );
    failures += parser_test_expect_node(
        assignments,
        MYLITE_SQL_AST_INSERT_DUPLICATE_ASSIGNMENT_LIST,
        "duplicate assignment list"
    );
    failures += parser_test_expect_child_count(assignments, 1U, "duplicate assignment count");
    failures += parser_test_expect_node(
        assignment,
        MYLITE_SQL_AST_INSERT_DUPLICATE_ASSIGNMENT,
        "duplicate assignment"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(assignment, 0U),
        "amount",
        "duplicate assignment target"
    );
    failures +=
        parser_test_expect_node(value, MYLITE_SQL_AST_INSERT_VALUES_REFERENCE, "values reference");
    failures += parser_test_expect_span_text(
        parser_test_child_at(value, 0U),
        "amount",
        "values reference column"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT DELAYED simple_lifecycle SET id = 1, amount = 2 "
        "ON DUPLICATE KEY UPDATE amount = DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    clause = parser_test_child_at(statement, 3U);
    assignments = parser_test_child_at(clause, 0U);
    assignment = parser_test_child_at(assignments, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SET_STATEMENT,
        "duplicate insert set"
    );
    failures += parser_test_expect_child_count(statement, 4U, "duplicate insert set child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_INSERT_DELAYED_MODIFIER,
        "duplicate insert set modifier"
    );
    failures += parser_test_expect_node(
        clause,
        MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE,
        "duplicate insert set clause"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_DML_DEFAULT_VALUE,
        "duplicate default value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO app.simple_lifecycle (id, amount) VALUES (1, 2) "
        "ON DUPLICATE KEY UPDATE amount = UNIX_TIMESTAMP() + 30;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    clause = parser_test_child_at(statement, 3U);
    assignments = parser_test_child_at(clause, 0U);
    assignment = parser_test_child_at(assignments, 0U);
    failures += parser_test_expect_operator(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "duplicate unix timestamp assignment"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(assignment, 1U), 0U),
        MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION,
        "duplicate unix timestamp assignment source"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO app.simple_lifecycle (id, amount) VALUES (1, 2) "
        "ON DUPLICATE KEY UPDATE amount = amount + 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    clause = parser_test_child_at(statement, 3U);
    assignments = parser_test_child_at(clause, 0U);
    assignment = parser_test_child_at(assignments, 0U);
    failures += parser_test_expect_operator(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "duplicate arithmetic add assignment"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(assignment, 1U), 0U),
        "amount",
        "duplicate arithmetic source"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(assignment, 1U), 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "duplicate arithmetic literal"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO app.simple_lifecycle (id, amount) VALUES (1, 2) "
        "ON DUPLICATE KEY UPDATE amount = amount - 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    clause = parser_test_child_at(statement, 3U);
    assignments = parser_test_child_at(clause, 0U);
    assignment = parser_test_child_at(assignments, 0U);
    failures += parser_test_expect_operator(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_OPERATOR_SUBTRACT,
        "duplicate arithmetic subtract assignment"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO simple_lifecycle VALUES (1) "
        "ON DUPLICATE KEY UPDATE simple_lifecycle.id = VALUES(simple_lifecycle.id), id = 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    clause = parser_test_child_at(statement, 3U);
    assignments = parser_test_child_at(clause, 0U);
    failures += parser_test_expect_child_count(assignments, 2U, "duplicate wider assignment count");
    assignment = parser_test_child_at(assignments, 0U);
    value = parser_test_child_at(assignment, 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(assignment, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "duplicate qualified target"
    );
    failures += parser_test_expect_node(
        value,
        MYLITE_SQL_AST_INSERT_VALUES_REFERENCE,
        "qualified values ref"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(value, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "duplicate qualified values column"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_replace_select_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "REPLACE INTO app.simple_lifecycle (id, amount) "
        "SELECT id, amount FROM app.source_lifecycle WHERE id >= 1 ORDER BY amount DESC LIMIT 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT,
        "replace select statement"
    );
    failures +=
        parser_test_expect_child_count(statement, 3U, "replace select statement child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.simple_lifecycle",
        "replace select target"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_IDENTIFIER_LIST,
        "replace select target columns"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(statement, 1U),
        2U,
        "replace select target count"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "replace source"
    );
    failures += parser_test_expect_node(
        parser_test_first_child_kind(
            parser_test_child_at(statement, 2U),
            MYLITE_SQL_AST_ORDER_BY_CLAUSE
        ),
        MYLITE_SQL_AST_ORDER_BY_CLAUSE,
        "replace select order clause"
    );
    failures += parser_test_expect_node(
        parser_test_first_child_kind(
            parser_test_child_at(statement, 2U),
            MYLITE_SQL_AST_LIMIT_CLAUSE
        ),
        MYLITE_SQL_AST_LIMIT_CLAUSE,
        "replace select limit clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE simple_lifecycle SELECT * FROM source_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT,
        "replace select no into"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "simple_lifecycle",
        "replace select target"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(statement, 1U),
        0U,
        "replace select implicit columns"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("REPLACE INTO t SELECT 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT,
        "replace select no source"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_replace_modifier_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "REPLACE LOW_PRIORITY INTO app.simple_lifecycle (id) VALUES (1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT,
        "low priority replace values"
    );
    failures +=
        parser_test_expect_child_count(statement, 4U, "low priority replace values child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 3U),
        MYLITE_SQL_AST_REPLACE_LOW_PRIORITY_MODIFIER,
        "low priority replace values modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE DELAYED simple_lifecycle VALUES (2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT,
        "delayed replace values"
    );
    failures += parser_test_expect_child_count(statement, 4U, "delayed replace values child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 3U),
        MYLITE_SQL_AST_REPLACE_DELAYED_MODIFIER,
        "delayed replace values modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE DELAYED INTO app.simple_lifecycle SET id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_REPLACE_SET_STATEMENT,
        "delayed replace set"
    );
    failures += parser_test_expect_child_count(statement, 3U, "delayed replace set child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_REPLACE_DELAYED_MODIFIER,
        "delayed replace set modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE INTO app.simple_lifecycle (id) VALUES (DEFAULT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT,
        "replace default"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_DML_DEFAULT_VALUE,
        "replace values default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE INTO app.simple_lifecycle SET id = DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_REPLACE_SET_STATEMENT,
        "replace set default"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 0U), 1U),
        MYLITE_SQL_AST_DML_DEFAULT_VALUE,
        "replace set default value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE LOW_PRIORITY INTO app.simple_lifecycle (id) SELECT id FROM app.source_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT,
        "low priority replace select"
    );
    failures +=
        parser_test_expect_child_count(statement, 4U, "low priority replace select child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 3U),
        MYLITE_SQL_AST_REPLACE_LOW_PRIORITY_MODIFIER,
        "low priority replace select modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE DELAYED simple_lifecycle SELECT * FROM source_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT,
        "delayed replace select"
    );
    failures += parser_test_expect_child_count(statement, 4U, "delayed replace select child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 3U),
        MYLITE_SQL_AST_REPLACE_DELAYED_MODIFIER,
        "delayed replace select modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_load_data_infile_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *ignore_lines = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "LOAD DATA INFILE '/tmp/posts.tsv' INTO TABLE posts;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_LOAD_DATA_INFILE_STATEMENT,
        "load data statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "load data statement child count");
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "load data file literal"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "posts",
        "load data target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "LOAD DATA INFILE '/tmp/posts.tsv' INTO TABLE app.posts IGNORE 1 LINES (id, body);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    ignore_lines = parser_test_child_at(statement, 2U);
    columns = parser_test_child_at(statement, 3U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_LOAD_DATA_INFILE_STATEMENT,
        "load data full"
    );
    failures += parser_test_expect_child_count(statement, 4U, "load data full child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "app.posts",
        "load data qualified"
    );
    failures += parser_test_expect_literal(
        ignore_lines,
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "load data ignore"
    );
    failures += parser_test_expect_span_text(ignore_lines, "1", "load data ignore text");
    failures +=
        parser_test_expect_node(columns, MYLITE_SQL_AST_IDENTIFIER_LIST, "load data columns");
    failures += parser_test_expect_child_count(columns, 2U, "load data column count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(columns, 0U),
        "id",
        "load data first column"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(columns, 1U),
        "body",
        "load data second column"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "LOAD DATA LOCAL INFILE '/tmp/posts.tsv' INTO TABLE posts;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_child_count(statement, 3U, "load data local child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_LOAD_DATA_LOCAL_MODIFIER,
        "load data local modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "LOAD DATA INFILE '/tmp/posts.tsv' INTO TABLE posts (id) IGNORE 1 LINES;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "LOAD DATA INFILE '/tmp/posts.tsv' INTO TABLE posts IGNORE +1 LINES;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "LOAD DATA INFILE '/tmp/posts.tsv' INTO TABLE posts FIELDS TERMINATED BY ',';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_delete_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *assignment_list = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures +=
        parser_test_parse_sql("DELETE FROM app.simple_lifecycle;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_DELETE_STATEMENT, "delete statement");
    failures += parser_test_expect_child_count(statement, 1U, "delete statement target only");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.simple_lifecycle",
        "delete target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DELETE FROM simple_lifecycle WHERE id = +1 ORDER BY nn DESC LIMIT 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    where_clause = parser_test_child_at(statement, 1U);
    order_clause = parser_test_child_at(statement, 2U);
    limit_clause = parser_test_child_at(statement, 3U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_DELETE_STATEMENT, "qualified delete");
    failures += parser_test_expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "delete where");
    failures += parser_test_expect_operator(
        parser_test_child_at(where_clause, 0U),
        MYLITE_SQL_AST_OPERATOR_EQUAL,
        "delete where operator"
    );
    failures +=
        parser_test_expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "delete order");
    failures += parser_test_expect_span_text(
        parser_test_child_at(order_clause, 0U),
        "nn",
        "delete order key"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "delete order direction"
    );
    failures += parser_test_expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "delete limit");
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "2",
        "delete limit row count"
    );
    failures += parser_test_expect_true(
        parser_test_child_at(limit_clause, 1U) == NULL,
        "delete limit has no offset"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DELETE FROM simple_lifecycle WHERE id = FALSE LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    where_clause = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(where_clause, 0U), 1U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "delete false predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DELETE FROM simple_lifecycle ORDER BY simple_lifecycle.id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    failures += parser_test_expect_node(
        parser_test_child_at(order_clause, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "delete qualified order key"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DELETE FROM simple_lifecycle LIMIT 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    limit_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures +=
        parser_test_expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "delete simple limit");
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "0",
        "delete simple limit row count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DELETE l FROM lefts AS l JOIN rights AS r ON l.k = r.k WHERE r.id IS NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    where_clause = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_JOINED_DELETE_STATEMENT,
        "joined delete statement"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "l",
        "joined delete target"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_FROM_JOIN,
        "joined delete join"
    );
    failures +=
        parser_test_expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "joined delete where");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DELETE a, b FROM wp_options a, wp_options b WHERE a.id = b.id;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    where_clause = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_TABLE_NAME_LIST,
        "multi-target joined delete target list"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(statement, 0U), 1U),
        "b",
        "multi-target joined delete second target"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_FROM_JOIN,
        "multi-target joined delete comma sources"
    );
    failures += parser_test_expect_node(
        where_clause,
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "multi-target joined delete where"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DELETE l FROM lefts AS l STRAIGHT_JOIN rights AS r ON l.k = r.k "
        "WHERE r.id IS NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_JOINED_DELETE_STATEMENT,
        "straight joined delete statement"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_join_kind(parser_test_child_at(statement, 1U)) ==
            MYLITE_SQL_AST_JOIN_KIND_INNER,
        "straight joined delete kind"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DELETE FROM app.lefts USING app.lefts LEFT JOIN app.rights ON lefts.k = rights.k "
        "WHERE rights.id IS NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_JOINED_DELETE_STATEMENT,
        "using joined delete statement"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.lefts",
        "using joined target"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_join_kind(parser_test_child_at(statement, 1U)) ==
            MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER,
        "using joined delete left join kind"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DELETE l FROM lefts AS l JOIN rights AS r ON l.k = r.k ORDER BY l.id;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DELETE l FROM lefts AS l JOIN rights AS r ON l.k = r.k LIMIT 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE lefts AS l JOIN rights AS r ON l.k = r.k SET l.v = 7 "
        "WHERE r.id IS NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    assignment_list = parser_test_child_at(statement, 1U);
    assignment = parser_test_child_at(assignment_list, 0U);
    where_clause = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_JOINED_UPDATE_STATEMENT,
        "joined update statement"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_FROM_JOIN,
        "joined update join"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_join_kind(parser_test_child_at(statement, 0U)) ==
            MYLITE_SQL_AST_JOIN_KIND_INNER,
        "joined update join kind"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(assignment, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "joined update qualified assignment target"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(assignment, 0U),
        "l.v",
        "joined update assignment"
    );
    failures +=
        parser_test_expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "joined update where");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE lefts AS l STRAIGHT_JOIN rights AS r ON l.k = r.k SET l.v = 7 "
        "WHERE r.id IS NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_JOINED_UPDATE_STATEMENT,
        "straight joined update statement"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_join_kind(parser_test_child_at(statement, 0U)) ==
            MYLITE_SQL_AST_JOIN_KIND_INNER,
        "straight joined update kind"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE app.lefts AS l LEFT JOIN app.rights AS r ON l.k = r.k "
        "SET r.w = NULL ORDER BY r.id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 1U), 0U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_join_kind(parser_test_child_at(statement, 0U)) ==
            MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER,
        "joined update left join kind"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "joined update NULL assignment"
    );
    failures += parser_test_expect_node(
        order_clause,
        MYLITE_SQL_AST_ORDER_BY_CLAUSE,
        "joined update order"
    );
    failures +=
        parser_test_expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "joined update limit");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE lefts JOIN rights SET lefts.v = 1 WHERE rights.id = 9;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_JOINED_UPDATE_STATEMENT,
        "joined update without ON statement"
    );
    failures += parser_test_expect_true(
        parser_test_child_at(parser_test_child_at(statement, 0U), 2U) == NULL,
        "joined update without ON has no join predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE lefts l JOIN rights r ON l.k = r.k SET l.v = 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_update_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *assignment_list = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    const struct mylite_sql_ast_node *from_table = NULL;
    const struct mylite_sql_ast_node *hint_list = NULL;
    const struct mylite_sql_ast_node *hint = NULL;
    const struct mylite_sql_ast_node *name_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "UPDATE app.simple_lifecycle SET amount = +1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    assignment_list = parser_test_child_at(statement, 1U);
    assignment = parser_test_child_at(assignment_list, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_UPDATE_STATEMENT, "update statement");
    failures += parser_test_expect_child_count(statement, 2U, "update statement required children");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.simple_lifecycle",
        "update target"
    );
    failures += parser_test_expect_node(
        assignment_list,
        MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST,
        "update assignment list"
    );
    failures += parser_test_expect_child_count(assignment_list, 1U, "update single assignment");
    failures += parser_test_expect_node(
        assignment,
        MYLITE_SQL_AST_UPDATE_ASSIGNMENT,
        "update assignment node"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(assignment, 0U),
        "amount",
        "update assignment target"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "update positive assignment value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE app.simple_lifecycle FORCE KEY FOR ORDER BY (PRIMARY, k_amount) "
        "SET amount = 2 WHERE id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_table = parser_test_child_at(statement, 0U);
    assignment_list = parser_test_child_at(statement, 1U);
    where_clause = parser_test_child_at(statement, 2U);
    hint_list = parser_test_child_at(from_table, 1U);
    hint = parser_test_child_at(hint_list, 0U);
    name_list = parser_test_child_at(hint, 1U);
    failures +=
        parser_test_expect_node(from_table, MYLITE_SQL_AST_FROM_TABLE, "update hinted target");
    failures += parser_test_expect_child_count(from_table, 2U, "update hinted target child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(from_table, 0U),
        "app.simple_lifecycle",
        "update hinted table"
    );
    failures +=
        parser_test_expect_node(hint_list, MYLITE_SQL_AST_INDEX_HINT_LIST, "update hint list");
    failures +=
        parser_test_expect_node(hint, MYLITE_SQL_AST_FORCE_INDEX_HINT, "update force key hint");
    failures += parser_test_expect_node(
        parser_test_child_at(hint, 0U),
        MYLITE_SQL_AST_INDEX_HINT_FOR_ORDER_BY,
        "update force order scope"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(name_list, 0U),
        "PRIMARY",
        "update primary hint name"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(name_list, 1U),
        "k_amount",
        "update second hint name"
    );
    failures += parser_test_expect_node(
        assignment_list,
        MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST,
        "update hinted assignment list"
    );
    failures +=
        parser_test_expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "update hinted where");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE simple_lifecycle USE INDEX () SET amount = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_table = parser_test_child_at(statement, 0U);
    hint = parser_test_child_at(parser_test_child_at(from_table, 1U), 0U);
    name_list = parser_test_child_at(hint, 0U);
    failures += parser_test_expect_child_count(name_list, 0U, "update empty use index names");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE simple_lifecycle FORCE INDEX () SET amount = 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "UPDATE simple_lifecycle IGNORE INDEX () SET amount = 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("UPDATE counters SET n = n + 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    assignment_list = parser_test_child_at(statement, 1U);
    assignment = parser_test_child_at(assignment_list, 0U);
    failures += parser_test_expect_operator(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "update arithmetic add assignment"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(assignment, 1U), 0U),
        "n",
        "update arithmetic source"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(assignment, 1U), 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "update arithmetic literal"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("UPDATE counters SET n = n - 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    assignment_list = parser_test_child_at(statement, 1U);
    assignment = parser_test_child_at(assignment_list, 0U);
    failures += parser_test_expect_operator(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_OPERATOR_SUBTRACT,
        "update arithmetic subtract assignment"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE simple_lifecycle SET amount = UNIX_TIMESTAMP();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 1U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION,
        "update unix timestamp assignment"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE simple_lifecycle SET amount = UNIX_TIMESTAMP() + -90;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 1U), 0U);
    failures += parser_test_expect_operator(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "update unix timestamp plus assignment"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(assignment, 1U), 0U),
        MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION,
        "update unix timestamp plus source"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(assignment, 1U), 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "update unix timestamp negative delta"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE simple_lifecycle SET amount = UNIX_TIMESTAMP(1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE simple_lifecycle SET amount = NULL WHERE id = +1 ORDER BY nn DESC LIMIT 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    assignment_list = parser_test_child_at(statement, 1U);
    assignment = parser_test_child_at(assignment_list, 0U);
    where_clause = parser_test_child_at(statement, 2U);
    order_clause = parser_test_child_at(statement, 3U);
    limit_clause = parser_test_child_at(statement, 4U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_UPDATE_STATEMENT, "qualified update");
    failures += parser_test_expect_literal(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "update NULL assignment value"
    );
    failures += parser_test_expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "update where");
    failures += parser_test_expect_operator(
        parser_test_child_at(where_clause, 0U),
        MYLITE_SQL_AST_OPERATOR_EQUAL,
        "update where operator"
    );
    failures +=
        parser_test_expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "update order");
    failures += parser_test_expect_span_text(
        parser_test_child_at(order_clause, 0U),
        "nn",
        "update order key"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "update order direction"
    );
    failures += parser_test_expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "update limit");
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "2",
        "update limit row count"
    );
    failures += parser_test_expect_true(
        parser_test_child_at(limit_clause, 1U) == NULL,
        "update limit has no offset"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE simple_lifecycle SET simple_lifecycle.amount = -1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    assignment_list = parser_test_child_at(statement, 1U);
    assignment = parser_test_child_at(assignment_list, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(assignment, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "update qualified assignment target"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "update negative assignment value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE simple_lifecycle SET amount = 1, nn = 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    assignment_list = parser_test_child_at(statement, 1U);
    failures +=
        parser_test_expect_child_count(assignment_list, 2U, "update multiple assignment list");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE simple_lifecycle SET amount = 1 LIMIT 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    limit_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures +=
        parser_test_expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "update simple limit");
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit_clause, 0U),
        "0",
        "update simple limit row count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE simple_lifecycle SET amount = FALSE WHERE id = TRUE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 1U), 0U);
    where_clause = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_literal(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "update false assignment value"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(where_clause, 0U), 1U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "update true predicate value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE simple_lifecycle SET amount = DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 1U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_DML_DEFAULT_VALUE,
        "update default assignment value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE simple_lifecycle SET amount = 'text';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 1U), 0U);
    failures += parser_test_expect_literal(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "update string assignment value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE target SET value = (SELECT source_value FROM source WHERE source_id = 1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 1U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_SCALAR_SUBQUERY,
        "update scalar subquery assignment value"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(assignment, 1U),
        "(SELECT source_value FROM source WHERE source_id = 1)",
        "update scalar subquery assignment span"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(assignment, 1U), 0U),
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "update scalar subquery inner select"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_transaction_control_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *characteristics = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("START TRANSACTION;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_START_TRANSACTION_STATEMENT,
        "start transaction statement"
    );
    failures += parser_test_expect_child_count(statement, 0U, "start transaction children");
    failures +=
        parser_test_expect_span_text(statement, "START TRANSACTION", "start transaction span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("START TRANSACTION READ WRITE;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    characteristics = parser_test_child_at(statement, 0U);
    failures +=
        parser_test_expect_child_count(statement, 1U, "start transaction read write children");
    failures += parser_test_expect_span_text(
        statement,
        "START TRANSACTION READ WRITE",
        "start transaction read write span"
    );
    failures += parser_test_expect_node(
        characteristics,
        MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST,
        "start transaction characteristic list"
    );
    failures +=
        parser_test_expect_child_count(characteristics, 1U, "start transaction read write count");
    failures += parser_test_expect_node(
        parser_test_child_at(characteristics, 0U),
        MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_WRITE,
        "start transaction read write"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "START TRANSACTION WITH CONSISTENT SNAPSHOT, READ ONLY;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    characteristics = parser_test_child_at(statement, 0U);
    failures +=
        parser_test_expect_child_count(characteristics, 2U, "start transaction mixed count");
    failures += parser_test_expect_node(
        parser_test_child_at(characteristics, 0U),
        MYLITE_SQL_AST_TRANSACTION_CONSISTENT_SNAPSHOT,
        "start transaction consistent snapshot"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(characteristics, 1U),
        MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_ONLY,
        "start transaction read only"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(characteristics, 0U),
        "WITH CONSISTENT SNAPSHOT",
        "consistent snapshot span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "START TRANSACTION READ ONLY, READ ONLY, WITH CONSISTENT SNAPSHOT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    characteristics = parser_test_child_at(statement, 0U);
    failures +=
        parser_test_expect_child_count(characteristics, 3U, "start transaction repeated count");
    failures += parser_test_expect_node(
        parser_test_child_at(characteristics, 0U),
        MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_ONLY,
        "start transaction first repeated read only"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(characteristics, 1U),
        MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_ONLY,
        "start transaction second repeated read only"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(characteristics, 2U),
        MYLITE_SQL_AST_TRANSACTION_CONSISTENT_SNAPSHOT,
        "start transaction repeated snapshot"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("BEGIN;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_START_TRANSACTION_STATEMENT, "begin");
    failures += parser_test_expect_span_text(statement, "BEGIN", "begin span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("BEGIN IMMEDIATE;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_START_TRANSACTION_STATEMENT,
        "begin immediate"
    );
    failures += parser_test_expect_span_text(statement, "BEGIN IMMEDIATE", "begin immediate span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("begin immediate;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_START_TRANSACTION_STATEMENT,
        "lower begin immediate"
    );
    failures +=
        parser_test_expect_span_text(statement, "begin immediate", "lower begin immediate span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("BEGIN WORK;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_START_TRANSACTION_STATEMENT,
        "begin work"
    );
    failures += parser_test_expect_span_text(statement, "BEGIN WORK", "begin work span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("COMMIT;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_COMMIT_STATEMENT, "commit");
    failures += parser_test_expect_span_text(statement, "COMMIT", "commit span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("COMMIT WORK;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_COMMIT_STATEMENT, "commit work");
    failures += parser_test_expect_span_text(statement, "COMMIT WORK", "commit work span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("ROLLBACK;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_ROLLBACK_STATEMENT, "rollback");
    failures += parser_test_expect_span_text(statement, "ROLLBACK", "rollback span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("ROLLBACK WORK;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_ROLLBACK_STATEMENT, "rollback work");
    failures += parser_test_expect_span_text(statement, "ROLLBACK WORK", "rollback work span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SAVEPOINT sp;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_SAVEPOINT_STATEMENT, "savepoint");
    failures += parser_test_expect_child_count(statement, 1U, "savepoint children");
    failures += parser_test_expect_span_text(statement, "SAVEPOINT sp", "savepoint span");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(statement, 0U), "sp", "savepoint name");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("ROLLBACK TO sp;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT,
        "rollback to savepoint"
    );
    failures += parser_test_expect_child_count(statement, 1U, "rollback to savepoint children");
    failures +=
        parser_test_expect_span_text(statement, "ROLLBACK TO sp", "rollback to savepoint span");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "sp",
        "rollback to savepoint name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("ROLLBACK WORK TO SAVEPOINT `sp ace`;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT,
        "rollback work to savepoint"
    );
    failures += parser_test_expect_span_text(
        statement,
        "ROLLBACK WORK TO SAVEPOINT `sp ace`",
        "rollback work to savepoint span"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "`sp ace`",
        "quoted savepoint name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("RELEASE SAVEPOINT sp;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT,
        "release savepoint"
    );
    failures += parser_test_expect_child_count(statement, 1U, "release savepoint children");
    failures +=
        parser_test_expect_span_text(statement, "RELEASE SAVEPOINT sp", "release savepoint span");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "sp",
        "release savepoint name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE transaction ("
        "begin INT, immediate INT, commit INT, rollback INT, work INT, savepoint INT, "
        "isolation INT, level INT, committed INT, uncommitted INT, repeatable INT, "
        "serializable INT, only INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "transaction keywords as identifiers"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "transaction",
        "transaction table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "START TRANSACTION ISOLATION LEVEL READ COMMITTED;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("BEGIN READ ONLY;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("BEGIN DEFERRED;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("BEGIN EXCLUSIVE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("BEGIN TRANSACTION;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "BEGIN IMMEDIATE TRANSACTION;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("END TRANSACTION;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("COMMIT AND CHAIN;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("COMMIT TRANSACTION;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("ROLLBACK RELEASE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("ROLLBACK TRANSACTION;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("ROLLBACK TO 'sp';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("RELEASE sp;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_table_lock_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *targets = NULL;
    const struct mylite_sql_ast_node *target = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("LOCK TABLES t READ;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    targets = parser_test_child_at(statement, 0U);
    target = parser_test_child_at(targets, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_LOCK_TABLES_STATEMENT, "lock read");
    failures += parser_test_expect_span_text(statement, "LOCK TABLES t READ", "lock read span");
    failures +=
        parser_test_expect_node(targets, MYLITE_SQL_AST_LOCK_TABLE_TARGET_LIST, "lock target list");
    failures += parser_test_expect_child_count(targets, 1U, "lock read target count");
    failures +=
        parser_test_expect_node(target, MYLITE_SQL_AST_LOCK_TABLE_TARGET, "lock read target");
    failures += parser_test_expect_child_count(target, 2U, "lock read target children");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(target, 0U), "t", "lock read table");
    failures += parser_test_expect_node(
        parser_test_child_at(target, 1U),
        MYLITE_SQL_AST_LOCK_TABLE_READ_LOCK,
        "lock read mode"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("LOCK TABLE t WRITE;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    targets = parser_test_child_at(statement, 0U);
    target = parser_test_child_at(targets, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_LOCK_TABLES_STATEMENT, "lock write");
    failures += parser_test_expect_span_text(statement, "LOCK TABLE t WRITE", "lock write span");
    failures += parser_test_expect_child_count(target, 2U, "lock write target children");
    failures += parser_test_expect_node(
        parser_test_child_at(target, 1U),
        MYLITE_SQL_AST_LOCK_TABLE_WRITE_LOCK,
        "lock write mode"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "LOCK TABLES app.t AS reader READ LOCAL, u writer WRITE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    targets = parser_test_child_at(statement, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_LOCK_TABLES_STATEMENT, "multi lock");
    failures += parser_test_expect_child_count(targets, 2U, "multi lock target count");
    target = parser_test_child_at(targets, 0U);
    failures += parser_test_expect_child_count(target, 3U, "first multi lock target children");
    failures += parser_test_expect_span_text(
        parser_test_child_at(target, 0U),
        "app.t",
        "first multi lock table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(target, 1U),
        "reader",
        "first multi lock alias"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(target, 2U),
        MYLITE_SQL_AST_LOCK_TABLE_READ_LOCAL_LOCK,
        "first multi lock mode"
    );
    target = parser_test_child_at(targets, 1U);
    failures += parser_test_expect_child_count(target, 3U, "second multi lock target children");
    failures += parser_test_expect_span_text(
        parser_test_child_at(target, 0U),
        "u",
        "second multi lock table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(target, 1U),
        "writer",
        "second multi lock alias"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(target, 2U),
        MYLITE_SQL_AST_LOCK_TABLE_WRITE_LOCK,
        "second multi lock mode"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("UNLOCK TABLES;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_UNLOCK_TABLES_STATEMENT, "unlock tables");
    failures += parser_test_expect_child_count(statement, 0U, "unlock tables children");
    failures += parser_test_expect_span_text(statement, "UNLOCK TABLES", "unlock tables span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("UNLOCK TABLE;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_UNLOCK_TABLES_STATEMENT, "unlock table");
    failures += parser_test_expect_span_text(statement, "UNLOCK TABLE", "unlock table span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("LOCK TABLES t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "LOCK TABLES t READ LOCAL LOCAL;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "LOCK TABLES t LOW_PRIORITY WRITE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("UNLOCK;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("LOCK INSTANCE FOR BACKUP;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}
