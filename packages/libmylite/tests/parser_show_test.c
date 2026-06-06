#include "parser_test_support.h"

static int test_show_columns_introspection_statements(void);
static int test_show_triggers_empty_introspection_statements(void);
static int test_show_events_empty_introspection_statements(void);
static int test_show_open_tables_empty_introspection_statements(void);
static int test_show_routine_status_empty_introspection_statements(void);
static int test_limited_stored_procedure_statements(void);
static int test_show_processlist_introspection_statements(void);
static int test_show_plugins_metadata_statement(void);
static int test_show_engine_status_statement(void);
static int test_show_grants_statement(void);
static int test_show_privileges_statement(void);
static int test_show_binary_log_metadata_statements(void);
static int test_show_replica_metadata_statements(void);
static int test_show_warnings_diagnostics_statements(void);
static int test_show_errors_diagnostics_statements(void);
static int test_show_index_empty_introspection_statements(void);
static int test_show_variables_statement(void);
static int test_show_status_statement(void);

int main(void) {
    int failures = 0;

    failures += test_show_columns_introspection_statements();
    failures += test_show_triggers_empty_introspection_statements();
    failures += test_show_events_empty_introspection_statements();
    failures += test_show_open_tables_empty_introspection_statements();
    failures += test_show_routine_status_empty_introspection_statements();
    failures += test_limited_stored_procedure_statements();
    failures += test_show_processlist_introspection_statements();
    failures += test_show_plugins_metadata_statement();
    failures += test_show_engine_status_statement();
    failures += test_show_grants_statement();
    failures += test_show_privileges_statement();
    failures += test_show_binary_log_metadata_statements();
    failures += test_show_replica_metadata_statements();
    failures += test_show_warnings_diagnostics_statements();
    failures += test_show_errors_diagnostics_statements();
    failures += test_show_index_empty_introspection_statements();
    failures += test_show_variables_statement();
    failures += test_show_status_statement();

    return failures == 0 ? 0 : 1;
}

static int test_show_columns_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SHOW COLUMNS FROM numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT,
        "show columns statement"
    );
    failures += parser_test_expect_child_count(statement, 1U, "show columns child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "numbers",
        "show columns table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW COLUMNS IN app.numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT,
        "show columns in statement"
    );
    failures += parser_test_expect_child_count(statement, 1U, "show columns in child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.numbers",
        "show columns qualified table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW FIELDS FROM numbers IN app;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT,
        "show fields statement"
    );
    failures +=
        parser_test_expect_child_count(statement, 2U, "show fields explicit schema child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "numbers",
        "show fields table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "app",
        "show fields schema"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW FIELDS IN numbers FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT,
        "show fields in from statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "show fields in from child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "numbers",
        "show fields in table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "app",
        "show fields from schema"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW COLUMNS FROM app.numbers FROM other;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT,
        "show columns qualified explicit schema"
    );
    failures += parser_test_expect_child_count(
        statement,
        2U,
        "show columns qualified explicit child count"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.numbers",
        "show columns qualified table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "other",
        "show columns trailing schema"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW COLUMNS FROM numbers LIKE 'i%';", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT,
        "show columns like"
    );
    failures += parser_test_expect_child_count(statement, 2U, "show columns like child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "numbers",
        "show columns like table"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "columns like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "'i%'",
        "show columns like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW COLUMNS FROM numbers WHERE Field = 'id';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT,
        "show columns where"
    );
    failures += parser_test_expect_child_count(statement, 2U, "show columns where child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "numbers",
        "show columns where table"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "columns where clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW FULL COLUMNS FROM numbers LIKE 'i%';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_FULL_COLUMNS_STATEMENT,
        "show full columns"
    );
    failures += parser_test_expect_child_count(statement, 2U, "show full columns like child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "numbers",
        "show full columns table"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "full columns like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "'i%'",
        "full columns like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW FULL FIELDS IN app.numbers FROM other;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_FULL_COLUMNS_STATEMENT,
        "show full fields"
    );
    failures += parser_test_expect_child_count(statement, 2U, "show full fields child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.numbers",
        "full fields table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "other",
        "full fields schema"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW FULL FIELDS IN app.numbers FROM other WHERE Collation IS NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_FULL_COLUMNS_STATEMENT,
        "show full fields where"
    );
    failures += parser_test_expect_child_count(statement, 3U, "show full fields where child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.numbers",
        "full fields where table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "other",
        "full fields where schema"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "full fields where"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW FIELDS FROM app.numbers FROM other LIKE 'i\\_1';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT,
        "show fields qualified like"
    );
    failures +=
        parser_test_expect_child_count(statement, 3U, "show fields qualified like child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.numbers",
        "fields like table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "other",
        "fields like schema"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "fields like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 2U),
        "'i\\_1'",
        "fields like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DESCRIBE app.numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "describe table");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.numbers",
        "describe target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DESC numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "desc table");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(statement, 0U), "numbers", "desc target");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("EXPLAIN app.numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "explain table");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.numbers",
        "explain target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW CREATE TABLE app.numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT,
        "show create table"
    );
    failures += parser_test_expect_child_count(statement, 1U, "show create child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.numbers",
        "show create target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE columns (fields INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "columns table name"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "columns",
        "columns identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_triggers_empty_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SHOW TRIGGERS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT, "show triggers");
    failures += parser_test_expect_child_count(statement, 0U, "show triggers child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW FULL TRIGGERS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT,
        "show full triggers"
    );
    failures += parser_test_expect_child_count(statement, 0U, "show full triggers child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW TRIGGERS FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT,
        "show triggers from"
    );
    failures += parser_test_expect_child_count(statement, 1U, "show triggers from child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app",
        "show triggers schema"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW TRIGGERS IN app LIKE 'account\\_%';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT,
        "show triggers in like"
    );
    failures += parser_test_expect_child_count(statement, 2U, "show triggers in like child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app",
        "show triggers like schema"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "triggers like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "'account\\_%'",
        "triggers like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW FULL TRIGGERS LIKE 'account';", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT,
        "show full triggers like"
    );
    failures +=
        parser_test_expect_child_count(statement, 1U, "show full triggers like child count");
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "full triggers like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "'account'",
        "full triggers like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE triggers (full INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "triggers table");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "triggers",
        "triggers identifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 0U), 0U),
        "full",
        "full identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW TRIGGER FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW EXTENDED TRIGGERS FROM app;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW TRIGGERS FROM app WHERE `Table` = 'account';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW TRIGGERS FROM app LIKE 'account' WHERE `Table` = 'account';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_events_empty_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SHOW EVENTS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT, "show events");
    failures += parser_test_expect_child_count(statement, 0U, "show events child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW EVENTS FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT,
        "show events from"
    );
    failures += parser_test_expect_child_count(statement, 1U, "show events from child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app",
        "show events schema"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW EVENTS IN app LIKE 'daily\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT,
        "show events in like"
    );
    failures += parser_test_expect_child_count(statement, 2U, "show events in like child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app",
        "show events like schema"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "events like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "'daily\\_%'",
        "events like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW EVENTS LIKE 'daily%';", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT,
        "show events like"
    );
    failures += parser_test_expect_child_count(statement, 1U, "show events like child count");
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "events bare like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "'daily%'",
        "events bare like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE events (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "events table");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "events",
        "events identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW FULL EVENTS FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW EXTENDED EVENTS FROM app;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW EVENT FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW EVENTS FROM app WHERE Name = 'daily_event';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW EVENTS FROM app LIKE 'daily%' WHERE Name = 'daily_event';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_open_tables_empty_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SHOW OPEN TABLES;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT,
        "show open tables"
    );
    failures += parser_test_expect_child_count(statement, 0U, "show open tables child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW OPEN TABLES FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT,
        "show open tables from"
    );
    failures += parser_test_expect_child_count(statement, 1U, "show open tables from child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app",
        "show open tables schema"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW OPEN TABLES IN app LIKE 'open\\_%';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT,
        "show open tables in like"
    );
    failures +=
        parser_test_expect_child_count(statement, 2U, "show open tables in like child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app",
        "show open tables like schema"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "open tables like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "'open\\_%'",
        "open tables like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW OPEN TABLES LIKE 'open%';", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT,
        "show open tables like"
    );
    failures += parser_test_expect_child_count(statement, 1U, "show open tables like child count");
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "open tables bare like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "'open%'",
        "open tables bare like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("CREATE TABLE open (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "open table");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "open",
        "open identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW FULL OPEN TABLES FROM app;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW EXTENDED OPEN TABLES FROM app;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW OPEN TABLES FROM app WHERE `Table` = 'open_table';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW OPEN TABLES FROM app LIKE 'open%' WHERE `Table` = 'open_table';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW OPEN TABLES FROM app ORDER BY `Table`;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW OPEN TABLES FROM app LIMIT 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW OPEN TABLES FROM app.extra;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW OPEN TABLES FROM app LIKE 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW OPEN TABLES FROM app LIKE NULL;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_routine_status_empty_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SHOW PROCEDURE STATUS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_PROCEDURE_STATUS_STATEMENT,
        "show procedure status"
    );
    failures += parser_test_expect_child_count(statement, 0U, "show procedure status child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW PROCEDURE STATUS LIKE 'routine\\_%';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_PROCEDURE_STATUS_STATEMENT,
        "show procedure status like"
    );
    failures +=
        parser_test_expect_child_count(statement, 1U, "show procedure status like child count");
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "procedure status like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "'routine\\_%'",
        "procedure like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW FUNCTION STATUS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_FUNCTION_STATUS_STATEMENT,
        "show function status"
    );
    failures += parser_test_expect_child_count(statement, 0U, "show function status child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW FUNCTION STATUS LIKE 'routine%';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_FUNCTION_STATUS_STATEMENT,
        "show function status like"
    );
    failures +=
        parser_test_expect_child_count(statement, 1U, "show function status like child count");
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "function status like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "'routine%'",
        "function like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE status (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "status table");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "status",
        "status identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW PROCEDURE STATUS FROM app;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW FUNCTION STATUS IN app;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW FULL PROCEDURE STATUS;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW EXTENDED FUNCTION STATUS;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW PROCEDURE STATUS WHERE Name = 'routine_proc';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW PROCEDURE STATUS LIKE 'routine%' WHERE Name = 'routine_proc';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW PROCEDURE STATUS ORDER BY Name;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW FUNCTION STATUS LIMIT 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW PROCEDURE STATUS LIKE 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW FUNCTION STATUS LIKE NULL;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW PROCEDURE STATUS LIKE N'routine';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW FUNCTION STATUS LIKE _utf8mb4'routine';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_limited_stored_procedure_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "CREATE PROCEDURE app.sync_proc() BEGIN SELECT ID FROM posts LIMIT 1; END;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_PROCEDURE_STATEMENT,
        "create procedure"
    );
    failures += parser_test_expect_child_count(statement, 2U, "create procedure child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.sync_proc",
        "create procedure name"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "create procedure select body"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW CREATE PROCEDURE app.sync_proc;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_CREATE_PROCEDURE_STATEMENT,
        "show create procedure"
    );
    failures += parser_test_expect_child_count(statement, 1U, "show create procedure child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.sync_proc",
        "show create procedure name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DROP PROCEDURE IF EXISTS app.sync_proc;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_DROP_PROCEDURE_STATEMENT,
        "drop procedure"
    );
    failures += parser_test_expect_child_count(statement, 2U, "drop procedure child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.sync_proc",
        "drop procedure name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("CALL app.sync_proc;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_CALL_STATEMENT, "call");
    failures += parser_test_expect_child_count(statement, 1U, "call child count");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(statement, 0U), "app.sync_proc", "call");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("CALL app.sync_proc();", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_CALL_STATEMENT, "call parens");
    failures += parser_test_expect_child_count(statement, 1U, "call parens child count");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_processlist_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SHOW PROCESSLIST;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_PROCESSLIST_STATEMENT,
        "show processlist"
    );
    failures += parser_test_expect_child_count(statement, 0U, "show processlist child count");
    failures +=
        parser_test_expect_span_text(statement, "SHOW PROCESSLIST", "show processlist span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW FULL PROCESSLIST;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_FULL_PROCESSLIST_STATEMENT,
        "show full processlist"
    );
    failures += parser_test_expect_child_count(statement, 0U, "show full processlist child count");
    failures += parser_test_expect_span_text(
        statement,
        "SHOW FULL PROCESSLIST",
        "show full processlist span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW FULL /* keep */ PROCESSLIST;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_FULL_PROCESSLIST_STATEMENT,
        "show full processlist with comment"
    );
    failures += parser_test_expect_span_text(
        statement,
        "SHOW FULL /* keep */ PROCESSLIST",
        "show full processlist commented span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE processlist (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "processlist table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "processlist",
        "processlist identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW PROCESSLIST LIKE 'root%';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW PROCESSLIST WHERE Id > 0;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW PROCESSLIST ORDER BY Id;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW PROCESSLIST LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW FULL PROCESSLIST LIMIT 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW PROCESSLIST FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW PROCESSLIST IN app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW EXTENDED PROCESSLIST;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_plugins_metadata_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SHOW PLUGINS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SHOW_PLUGINS_STATEMENT, "show plugins");
    failures += parser_test_expect_child_count(statement, 0U, "show plugins child count");
    failures += parser_test_expect_span_text(statement, "SHOW PLUGINS", "show plugins span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("show plugins;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_PLUGINS_STATEMENT,
        "lowercase show plugins"
    );
    failures += parser_test_expect_child_count(statement, 0U, "lowercase show plugins child count");
    failures +=
        parser_test_expect_span_text(statement, "show plugins", "lowercase show plugins span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE plugins (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "plugins table");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "plugins",
        "plugins identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_engine_status_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SHOW ENGINE InnoDB STATUS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_ENGINE_STATUS_STATEMENT,
        "show engine status"
    );
    failures += parser_test_expect_child_count(statement, 1U, "show engine status child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "show engine status engine name"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "InnoDB",
        "show engine status engine span"
    );
    failures += parser_test_expect_span_text(
        statement,
        "SHOW ENGINE InnoDB STATUS",
        "show engine status span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW ENGINE innodb STATUS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_ENGINE_STATUS_STATEMENT,
        "show lower engine status"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "innodb",
        "show lower engine status engine span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW ENGINE `InnoDB` STATUS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_ENGINE_STATUS_STATEMENT,
        "show quoted engine status"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "`InnoDB`",
        "show quoted engine status engine span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW ENGINE 'InnoDB' STATUS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_ENGINE_STATUS_STATEMENT,
        "show string engine status"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "show string engine status engine"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_grants_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SHOW GRANTS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SHOW_GRANTS_STATEMENT, "show grants");
    failures += parser_test_expect_child_count(statement, 0U, "show grants child count");
    failures += parser_test_expect_span_text(statement, "SHOW GRANTS", "show grants span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("show grants;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_GRANTS_STATEMENT,
        "lowercase show grants"
    );
    failures += parser_test_expect_child_count(statement, 0U, "lowercase show grants child count");
    failures +=
        parser_test_expect_span_text(statement, "show grants", "lowercase show grants span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW GRANTS FOR CURRENT_USER;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_GRANTS_STATEMENT,
        "show grants current user"
    );
    failures +=
        parser_test_expect_child_count(statement, 0U, "show grants current user child count");
    failures += parser_test_expect_span_text(
        statement,
        "SHOW GRANTS FOR CURRENT_USER",
        "show grants current user span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW GRANTS FOR CURRENT_USER();", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_GRANTS_STATEMENT,
        "show grants current user function"
    );
    failures += parser_test_expect_child_count(
        statement,
        0U,
        "show grants current user function child count"
    );
    failures += parser_test_expect_span_text(
        statement,
        "SHOW GRANTS FOR CURRENT_USER()",
        "show grants current user function span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE grants (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "grants table");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "grants",
        "grants identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW GRANTS FOR root;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW GRANTS FOR 'root'@'%';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW GRANTS FOR CURRENT_USER USING 'r';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW GRANTS LIKE 'root%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW GRANTS LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW GRANTS WHERE User = 'root';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_privileges_statement(void) {
    static const char *const unsupported_sql[] = {
        "SHOW PRIVILEGES LIKE 'Select';",
        "SHOW PRIVILEGES WHERE Privilege = 'Select';",
        "SHOW FULL PRIVILEGES;",
        "SHOW PRIVILEGES FROM mysql;",
        "SHOW PRIVILEGES ORDER BY Privilege;",
        "SHOW PRIVILEGES LIMIT 1;",
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SHOW PRIVILEGES;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_PRIVILEGES_STATEMENT,
        "show privileges"
    );
    failures += parser_test_expect_child_count(statement, 0U, "show privileges child count");
    failures += parser_test_expect_span_text(statement, "SHOW PRIVILEGES", "show privileges span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("show privileges;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_PRIVILEGES_STATEMENT,
        "lowercase show privileges"
    );
    failures +=
        parser_test_expect_child_count(statement, 0U, "lowercase show privileges child count");
    failures += parser_test_expect_span_text(
        statement,
        "show privileges",
        "lowercase show privileges span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE privileges (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "privileges table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "privileges",
        "privileges identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    for (size_t index = 0U; index < sizeof(unsupported_sql) / sizeof(unsupported_sql[0]); ++index) {
        failures +=
            parser_test_parse_sql(unsupported_sql[index], MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
        mylite_sql_parse_result_deinit(&result);
    }

    return failures;
}

static int test_show_binary_log_metadata_statements(void) {
    static const char *const unsupported_sql[] = {
        "SHOW BINARY LOG STATUS LIKE '%';",
        "SHOW BINARY LOG STATUS WHERE File IS NOT NULL;",
        "SHOW BINARY LOG STATUS LIMIT 1;",
        "SHOW BINARY LOGS LIKE '%';",
        "SHOW BINARY LOGS WHERE Log_name IS NOT NULL;",
        "SHOW BINARY LOGS LIMIT 1;",
        "SHOW FULL BINARY LOG STATUS;",
        "SHOW FULL BINARY LOGS;",
        "SHOW MASTER STATUS;",
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SHOW BINARY LOG STATUS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_BINARY_LOG_STATUS_STATEMENT,
        "show binary log status"
    );
    failures += parser_test_expect_child_count(statement, 0U, "show binary log status child count");
    failures += parser_test_expect_span_text(
        statement,
        "SHOW BINARY LOG STATUS",
        "show binary log status span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("show binary log status;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_BINARY_LOG_STATUS_STATEMENT,
        "lowercase show binary log status"
    );
    failures += parser_test_expect_child_count(
        statement,
        0U,
        "lowercase show binary log status child count"
    );
    failures += parser_test_expect_span_text(
        statement,
        "show binary log status",
        "lowercase show binary log status"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW BINARY LOGS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_BINARY_LOGS_STATEMENT,
        "show binary logs"
    );
    failures += parser_test_expect_child_count(statement, 0U, "show binary logs child count");
    failures +=
        parser_test_expect_span_text(statement, "SHOW BINARY LOGS", "show binary logs span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("show binary logs;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_BINARY_LOGS_STATEMENT,
        "lowercase show binary logs"
    );
    failures +=
        parser_test_expect_child_count(statement, 0U, "lowercase show binary logs child count");
    failures +=
        parser_test_expect_span_text(statement, "show binary logs", "lowercase show binary logs");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("CREATE TABLE logs (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "logs table");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "logs",
        "logs identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    for (size_t index = 0U; index < sizeof(unsupported_sql) / sizeof(unsupported_sql[0]); ++index) {
        failures +=
            parser_test_parse_sql(unsupported_sql[index], MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
        mylite_sql_parse_result_deinit(&result);
    }

    return failures;
}

static int test_show_replica_metadata_statements(void) {
    static const char *const unsupported_sql[] = {
        "SHOW REPLICA STATUS FOR CHANNEL 'default';",
        "SHOW REPLICA STATUS LIKE '%';",
        "SHOW REPLICA STATUS WHERE Channel_Name = '';",
        "SHOW REPLICA STATUS LIMIT 1;",
        "SHOW FULL REPLICA STATUS;",
        "SHOW REPLICAS LIKE '%';",
        "SHOW REPLICAS WHERE Host IS NOT NULL;",
        "SHOW REPLICAS LIMIT 1;",
        "SHOW FULL REPLICAS;",
        "SHOW SLAVE STATUS;",
        "SHOW SLAVE HOSTS;",
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SHOW REPLICA STATUS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_REPLICA_STATUS_STATEMENT,
        "show replica status"
    );
    failures += parser_test_expect_child_count(statement, 0U, "show replica status child count");
    failures +=
        parser_test_expect_span_text(statement, "SHOW REPLICA STATUS", "show replica status span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("show replica status;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_REPLICA_STATUS_STATEMENT,
        "lowercase show replica status"
    );
    failures +=
        parser_test_expect_child_count(statement, 0U, "lowercase show replica status child count");
    failures += parser_test_expect_span_text(
        statement,
        "show replica status",
        "lowercase show replica status"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW REPLICAS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SHOW_REPLICAS_STATEMENT, "show replicas");
    failures += parser_test_expect_child_count(statement, 0U, "show replicas child count");
    failures += parser_test_expect_span_text(statement, "SHOW REPLICAS", "show replicas span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("show replicas;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_REPLICAS_STATEMENT,
        "lowercase show replicas"
    );
    failures +=
        parser_test_expect_child_count(statement, 0U, "lowercase show replicas child count");
    failures += parser_test_expect_span_text(statement, "show replicas", "lowercase show replicas");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE replica (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "replica table");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "replica",
        "replica identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE replicas (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "replicas table");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "replicas",
        "replicas identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    for (size_t index = 0U; index < sizeof(unsupported_sql) / sizeof(unsupported_sql[0]); ++index) {
        failures +=
            parser_test_parse_sql(unsupported_sql[index], MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
        mylite_sql_parse_result_deinit(&result);
    }

    return failures;
}

static int test_show_warnings_diagnostics_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *limit = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SHOW WARNINGS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT, "show warnings");
    failures += parser_test_expect_child_count(statement, 0U, "show warnings child count");
    failures += parser_test_expect_span_text(statement, "SHOW WARNINGS", "show warnings span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW WARNINGS LIMIT 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    limit = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT,
        "show warnings limit"
    );
    failures += parser_test_expect_child_count(statement, 1U, "show warnings limit child count");
    failures +=
        parser_test_expect_node(limit, MYLITE_SQL_AST_LIMIT_CLAUSE, "show warnings limit clause");
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit, 0U),
        "1",
        "show warnings limit row count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW WARNINGS LIMIT 2, 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    limit = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT,
        "show warnings comma"
    );
    failures += parser_test_expect_child_count(limit, 2U, "show warnings comma child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit, 0U),
        "1",
        "show warnings comma row count"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit, 1U),
        "2",
        "show warnings comma offset"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW WARNINGS LIMIT 1 OFFSET 2;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    limit = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT,
        "show warnings offset"
    );
    failures += parser_test_expect_child_count(limit, 2U, "show warnings offset child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit, 0U),
        "1",
        "show warnings offset row count"
    );
    failures +=
        parser_test_expect_span_text(parser_test_child_at(limit, 1U), "2", "show warnings offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW COUNT(*) WARNINGS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_COUNT_WARNINGS_STATEMENT,
        "show count warnings"
    );
    failures += parser_test_expect_child_count(statement, 0U, "show count warnings child count");
    failures += parser_test_expect_span_text(
        statement,
        "SHOW COUNT(*) WARNINGS",
        "show count warnings span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE warnings (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "warnings table");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "warnings",
        "warnings identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW COUNT (*) WARNINGS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW WARNINGS LIMIT +1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW WARNINGS LIMIT -1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW WARNINGS LIKE 'x';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW WARNINGS WHERE Code = 1287;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW WARNINGS ORDER BY Code;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_errors_diagnostics_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *limit = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SHOW ERRORS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT, "show errors");
    failures += parser_test_expect_child_count(statement, 0U, "show errors child count");
    failures += parser_test_expect_span_text(statement, "SHOW ERRORS", "show errors span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW ERRORS LIMIT 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    limit = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT,
        "show errors limit"
    );
    failures += parser_test_expect_child_count(statement, 1U, "show errors limit child count");
    failures +=
        parser_test_expect_node(limit, MYLITE_SQL_AST_LIMIT_CLAUSE, "show errors limit clause");
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit, 0U),
        "1",
        "show errors limit row count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW ERRORS LIMIT 2, 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    limit = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT,
        "show errors comma"
    );
    failures += parser_test_expect_child_count(limit, 2U, "show errors comma child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit, 0U),
        "1",
        "show errors comma row count"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit, 1U),
        "2",
        "show errors comma offset"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW ERRORS LIMIT 1 OFFSET 2;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    limit = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT,
        "show errors offset"
    );
    failures += parser_test_expect_child_count(limit, 2U, "show errors offset child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(limit, 0U),
        "1",
        "show errors offset row count"
    );
    failures +=
        parser_test_expect_span_text(parser_test_child_at(limit, 1U), "2", "show errors offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW COUNT(*) ERRORS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_COUNT_ERRORS_STATEMENT,
        "show count errors"
    );
    failures += parser_test_expect_child_count(statement, 0U, "show count errors child count");
    failures +=
        parser_test_expect_span_text(statement, "SHOW COUNT(*) ERRORS", "show count errors span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE errors (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "errors table");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "errors",
        "errors identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW COUNT (*) ERRORS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW ERRORS LIMIT +1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW ERRORS LIMIT -1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW ERRORS LIKE 'x';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW ERRORS WHERE Code = 1064;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW ERRORS ORDER BY Code;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_index_empty_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SHOW INDEX FROM numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, "show index");
    failures += parser_test_expect_child_count(statement, 1U, "show index child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "numbers",
        "show index table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW INDEX IN app.numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, "show index in");
    failures += parser_test_expect_child_count(statement, 1U, "show index in child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.numbers",
        "show index in table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW INDEXES FROM numbers FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, "show indexes");
    failures += parser_test_expect_child_count(statement, 2U, "show indexes child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "numbers",
        "show indexes table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "app",
        "show indexes schema"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW KEYS IN app.numbers IN other;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, "show keys");
    failures += parser_test_expect_child_count(statement, 2U, "show keys child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.numbers",
        "show keys table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "other",
        "show keys schema"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE indexes (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "indexes table");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "indexes",
        "indexes identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW INDEX;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW EXTENDED INDEX FROM numbers;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW INDEX FROM numbers WHERE Key_name = 'idx';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, "show index where");
    failures += parser_test_expect_child_count(statement, 2U, "show index where child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "numbers",
        "show index where table"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "show index where"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW KEYS IN app.numbers IN other WHERE `Column_name` IN ('id','v');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, "show keys where");
    failures += parser_test_expect_child_count(statement, 3U, "show keys where child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.numbers",
        "show keys where table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "other",
        "show keys where schema"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "show keys where"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW INDEX FROM numbers WHERE Key_name = 'idx' ORDER BY Key_name;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW INDEX FROM numbers LIKE 'idx';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_variables_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SHOW VARIABLES;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT,
        "show variables"
    );
    failures += parser_test_expect_child_count(statement, 0U, "show variables child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW GLOBAL VARIABLES;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT,
        "show global variables"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "GLOBAL",
        "global variables scope"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW SESSION VARIABLES LIKE 'sql\\_%';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT,
        "show session variables"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "SESSION",
        "session variables scope"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "variables like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "'sql\\_%'",
        "variables like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW LOCAL VARIABLES LIKE 'autocommit';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT,
        "show local variables"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "LOCAL",
        "local variables scope"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW VARIABLES LIKE 'SQL\\_LOG\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT,
        "show variables like"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "bare variables like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "'SQL\\_LOG\\_%'",
        "bare like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW VARIABLES WHERE Variable_name = 'autocommit';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT,
        "show variables where"
    );
    failures += parser_test_expect_child_count(statement, 1U, "show variables where child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "variables where"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW GLOBAL VARIABLES WHERE `Value` = 'ON';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "GLOBAL",
        "global variables where scope"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "scoped variables where"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW VARIABLES LIKE 'a%' WHERE Variable_name = 'autocommit';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW VARIABLES WHERE Variable_name = 'autocommit' ORDER BY Variable_name;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE variables (global INT, session INT, local INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "variables nonreserved identifiers"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "variables",
        "variables table"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_status_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SHOW STATUS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SHOW_STATUS_STATEMENT, "show status");
    failures += parser_test_expect_child_count(statement, 0U, "show status child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW GLOBAL STATUS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_STATUS_STATEMENT,
        "show global status"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "GLOBAL",
        "global status scope"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW SESSION STATUS LIKE 'threads\\_%';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_STATUS_STATEMENT,
        "show session status"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "SESSION",
        "session status scope"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "status like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "'threads\\_%'",
        "status like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW LOCAL STATUS LIKE 'Threads%';", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_STATUS_STATEMENT,
        "show local status"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "LOCAL",
        "local status scope"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW STATUS LIKE 'Threads\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_STATUS_STATEMENT,
        "show status like"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "bare status like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "'Threads\\_%'",
        "bare status like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW STATUS WHERE Variable_name = 'Threads_connected';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW STATUS LIKE 'Threads%' WHERE Variable_name = 'Threads_connected';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW STATUS ORDER BY Variable_name;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW STATUS LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW FULL STATUS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW STATUS LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW STATUS LIKE NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW STATUS LIKE N'threads';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}
