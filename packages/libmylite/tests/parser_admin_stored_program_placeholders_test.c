#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_admin_noop_statement_forms(void);
static int test_unsupported_stored_program_statement_forms(void);
static int test_call_argument_forms(void);
static int expect_statement_kind(struct expected_statement expected);
static int expect_parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_admin_noop_statement_forms();
    failures += test_unsupported_stored_program_statement_forms();
    failures += test_call_argument_forms();

    return failures == 0 ? 0 : 1;
}

static int test_admin_noop_statement_forms(void) {
    static const struct expected_statement statements[] = {
        {
            .sql = "CREATE USER 'u'@'%' IDENTIFIED BY 'p'",
            .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT,
        },
        {.sql = "ALTER USER 'u'@'%' ACCOUNT LOCK", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "DROP USER IF EXISTS 'u'@'%'", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "CREATE ROLE r", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "DROP ROLE IF EXISTS r", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "RENAME USER 'u'@'%' TO 'v'@'%'", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "GRANT SELECT ON *.* TO 'u'@'%'", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {
            .sql = "REVOKE SELECT ON *.* FROM 'u'@'%'",
            .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT,
        },
        {.sql = "SET PASSWORD FOR 'u'@'%' = 'x'", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "SET ROLE DEFAULT", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {
            .sql = "SET DEFAULT ROLE ALL TO 'u'@'%'",
            .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT,
        },
        {.sql = "SET PERSIST max_connections = 200", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {
            .sql = "SET PERSIST_ONLY innodb_monitor_enable = 'latch'",
            .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT,
        },
        {.sql = "RESET MASTER", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "RESET PERSIST max_connections", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "FLUSH PRIVILEGES", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {
            .sql = "PURGE BINARY LOGS TO 'bin.000001'",
            .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT,
        },
        {.sql = "KILL QUERY @thread_id", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "CACHE INDEX t USE key_cache", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "LOAD INDEX INTO CACHE t", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "SHOW CREATE USER 'u'@'%'", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "ALTER INSTANCE RELOAD TLS", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "RESTART", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "SHUTDOWN", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_kind(statements[index]);
    }
    failures += expect_statement_kind((struct expected_statement){
        .sql = "SET @@PERSIST.max_connections = 200",
        .kind = MYLITE_SQL_AST_SET_STATEMENT,
    });
    failures += expect_parse_status(
        "GRANT USAGE ON *.* TO 'u'@'%'; SELECT 1",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "admin placeholder rejects multiple statements"
    );
    return failures;
}

static int test_unsupported_stored_program_statement_forms(void) {
    static const struct expected_statement statements[] = {
        {
            .sql = "CREATE DEFINER=`root`@`%` PROCEDURE p(IN x INT) BEGIN SELECT x; END",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT,
        },
        {
            .sql = "CREATE FUNCTION f() RETURNS INT RETURN 1",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT,
        },
        {
            .sql = "CREATE TRIGGER tr BEFORE INSERT ON t FOR EACH ROW SET NEW.id = 1",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT,
        },
        {
            .sql = "CREATE EVENT e ON SCHEDULE EVERY 1 DAY DO SELECT 1",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT,
        },
        {
            .sql = "ALTER PROCEDURE p COMMENT 'x'",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT,
        },
        {
            .sql = "ALTER FUNCTION f SQL SECURITY INVOKER",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT,
        },
        {.sql = "ALTER EVENT e DISABLE", .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT
        },
        {
            .sql = "DROP FUNCTION IF EXISTS f",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT,
        },
        {
            .sql = "DROP TRIGGER IF EXISTS tr",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT,
        },
        {.sql = "DROP EVENT IF EXISTS e",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT},
        {.sql = "SHOW CREATE FUNCTION f",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT},
        {.sql = "SHOW CREATE TRIGGER tr",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT},
        {.sql = "SHOW CREATE EVENT e", .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT},
        {.sql = "CALL mtr.p(OUT @arg)", .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT
        },
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_kind(statements[index]);
    }
    return failures;
}

static int test_call_argument_forms(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures +=
        parser_test_parse_sql("CALL mtr.add_suppression('x')", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_CALL_STATEMENT, "call with argument");
    failures += parser_test_expect_child_count(statement, 2U, "call argument child count");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int expect_statement_kind(struct expected_statement expected) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(expected.sql, MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(statement, expected.kind, expected.sql);
    failures += parser_test_expect_child_count(
        statement,
        expected.kind == MYLITE_SQL_AST_SET_STATEMENT ? 1U : 0U,
        expected.sql
    );
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
