#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_executable_constraint_syntax(void);
static int test_extended_option_placeholders(void);
static int test_malformed_extended_option_tails(void);
static int expect_statement_kind(struct expected_statement expected);
static int parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_executable_constraint_syntax();
    failures += test_extended_option_placeholders();
    failures += test_malformed_extended_option_tails();

    return failures == 0 ? 0 : 1;
}

static int test_executable_constraint_syntax(void) {
    int failures = 0;

    failures += expect_statement_kind((struct expected_statement){
        .sql = "CREATE TABLE bare_pk (id INT, CONSTRAINT PRIMARY KEY (id));",
        .kind = MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
    });
    failures += expect_statement_kind((struct expected_statement){
        .sql = "ALTER TABLE bare_pk ADD CONSTRAINT PRIMARY KEY (id);",
        .kind = MYLITE_SQL_AST_ALTER_TABLE_ADD_PRIMARY_KEY_STATEMENT,
    });
    failures += expect_statement_kind((struct expected_statement){
        .sql = "CREATE TABLE bare_fk (pid INT, CONSTRAINT FOREIGN KEY (pid) "
               "REFERENCES parent(id));",
        .kind = MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
    });
    failures += expect_statement_kind((struct expected_statement){
        .sql = "ALTER TABLE bare_fk ADD CONSTRAINT FOREIGN KEY (pid) REFERENCES parent(id);",
        .kind = MYLITE_SQL_AST_ALTER_TABLE_ADD_FOREIGN_KEY_STATEMENT,
    });
    failures += expect_statement_kind((struct expected_statement){
        .sql = "ALTER TABLE checked ALTER CONSTRAINT positive NOT ENFORCED;",
        .kind = MYLITE_SQL_AST_ALTER_TABLE_ALTER_CHECK_STATEMENT,
    });
    failures += expect_statement_kind((struct expected_statement){
        .sql = "ALTER TABLE checked ALTER CONSTRAINT positive ENFORCED;",
        .kind = MYLITE_SQL_AST_ALTER_TABLE_ALTER_CHECK_STATEMENT,
    });
    failures += expect_statement_kind((struct expected_statement){
        .sql = "ALTER TABLE checked ALTER CONSTRAINT positive NOT ENFORCED, "
               "ALTER CONSTRAINT positive ENFORCED;",
        .kind = MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT,
    });

    return failures;
}

static int test_extended_option_placeholders(void) {
    static const char column_format_placeholder_sql[] =
        "CREATE TABLE column_format_t (a INT COLUMN_FORMAT DYNAMIC, b INT COLUMN_FORMAT FIXED, c "
        "INT COLUMN_FORMAT DEFAULT);";
    static const char alter_check_interval_placeholder_sql[] =
        "ALTER TABLE check_in_t ADD COLUMN c11 YEAR, ADD CONSTRAINT CHECK (c11 > '2007-01-01' + "
        "INTERVAL +1 YEAR);";
    static const char *const placeholders[] = {
        "CREATE TABLE encrypted_t (id INT) ENCRYPTION='N';",
        "ALTER TABLE encrypted_t ENCRYPTION='Y';",
        "CREATE TABLE secondary_t (id INT) SECONDARY_ENGINE=myisam;",
        "ALTER TABLE secondary_t SECONDARY_ENGINE MOCK;",
        "CREATE TABLE directory_t (id INT) DATA DIRECTORY='/tmp' INDEX DIRECTORY='/tmp';",
        "CREATE TABLE column_storage_t (a INT STORAGE DISK, b INT STORAGE MEMORY);",
        "ALTER TABLE column_storage_t ADD COLUMN c INT STORAGE MEMORY;",
        column_format_placeholder_sql,
        "ALTER TABLE column_format_t MODIFY COLUMN a INT COLUMN_FORMAT FIXED;",
        "CREATE TABLE not_secondary_t (d DATE NOT SECONDARY);",
        "CREATE TABLE check_in_t (f1 INT CHECK (f1 IN (10, 20)));",
        "ALTER TABLE check_in_t ADD CONSTRAINT CHECK (f1 NOT IN (1, 2, 3));",
        alter_check_interval_placeholder_sql,
        "CREATE TABLE check_logical_t (a INT CHECK ((a > 0) && (a < 10)));",
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(placeholders) / sizeof(placeholders[0]); ++index) {
        failures += expect_statement_kind((struct expected_statement){
            .sql = placeholders[index],
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        });
    }

    return failures;
}

static int test_malformed_extended_option_tails(void) {
    int failures = 0;

    failures += parse_status(
        "CREATE TABLE bad_encryption (id INT) ENCRYPTION =",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete encryption option"
    );
    failures += parse_status(
        "CREATE TABLE bad_storage (id INT STORAGE)",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete storage attribute"
    );
    failures += parse_status(
        "CREATE TABLE bad_column_format (id INT COLUMN_FORMAT)",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete column format attribute"
    );
    failures += parse_status(
        "ALTER TABLE checked ALTER CONSTRAINT",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete alter constraint"
    );
    failures += parse_status(
        "CREATE TABLE bad_check (id INT CHECK (id IN))",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete check IN expression"
    );
    failures += parse_status(
        "CREATE TABLE bad_check (id INT CHECK (id IN (, 1)))",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "leading comma check IN expression"
    );
    failures += parse_status(
        "CREATE TABLE bad_check (id INT CHECK (id IN (1,)))",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "trailing comma check IN expression"
    );
    failures += parse_status(
        "CREATE TABLE bad_check (id INT CHECK (id IN (1,,2)))",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "doubled comma check IN expression"
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
    if (expected.kind == MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT) {
        failures += parser_test_expect_child_count(statement, 0U, expected.sql);
    }
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
