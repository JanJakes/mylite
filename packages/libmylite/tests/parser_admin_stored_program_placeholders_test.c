#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_admin_noop_statement_forms(void);
static int test_unsupported_stored_program_statement_forms(void);
static int test_call_argument_forms(void);
static int test_flush_script_form(void);
static int test_removed_flush_forms_rejected(void);
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
    failures += test_flush_script_form();
    failures += test_removed_flush_forms_rejected();

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
        {.sql = "RESET BINARY LOGS AND GTIDS", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "RESET REPLICA ALL FOR CHANNEL ''", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "RESET PERSIST max_connections", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "FLUSH PRIVILEGES", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "FLUSH STATUS", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "FLUSH OPTIMIZER_COSTS", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "FLUSH USER_RESOURCES", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "FLUSH LOGS", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "FLUSH BINARY LOGS", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "FLUSH ENGINE LOGS", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "FLUSH ERROR LOGS", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "FLUSH GENERAL LOGS", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "FLUSH SLOW LOGS", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "FLUSH RELAY LOGS", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "FLUSH RELAY LOGS FOR CHANNEL channel1", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT
        },
        {.sql = "FLUSH TABLE", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "FLUSH TABLES", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "FLUSH TABLES mysql.events", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "FLUSH TABLE export FOR EXPORT", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "FLUSH TABLES wp_options, wp_posts WITH READ LOCK",
         .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "FLUSH TABLE WITH READ LOCK", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "FLUSH TABLES WITH READ LOCK", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {
            .sql = "PURGE BINARY LOGS TO 'bin.000001'",
            .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT,
        },
        {
            .sql = "PURGE BINARY LOGS BEFORE '2000-01-01 00:00:00'",
            .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT,
        },
        {.sql = "BINLOG 'AAAA'", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {
            .sql = "CHANGE REPLICATION FILTER REPLICATE_DO_DB = (wp)",
            .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT,
        },
        {
            .sql = "CHANGE REPLICATION FILTER REPLICATE_WILD_DO_TABLE = ('wp.%') "
                   "FOR CHANNEL ''",
            .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT,
        },
        {
            .sql = "START REPLICA IO_THREAD, SQL_THREAD FOR CHANNEL ''",
            .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT,
        },
        {
            .sql = "STOP REPLICA SQL_THREAD FOR CHANNEL ''",
            .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT,
        },
        {
            .sql = "START GROUP_REPLICATION USER='u', PASSWORD='p', "
                   "DEFAULT_AUTH='mysql_native_password'",
            .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT,
        },
        {.sql = "STOP GROUP_REPLICATION", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "KILL QUERY @thread_id", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "CACHE INDEX t USE key_cache", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "LOAD INDEX INTO CACHE t", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
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
    failures +=
        expect_parse_status("START SLAVE", MYLITE_SQL_PARSE_SYNTAX_ERROR, "removed START SLAVE");
    failures +=
        expect_parse_status("STOP SLAVE", MYLITE_SQL_PARSE_SYNTAX_ERROR, "removed STOP SLAVE");
    failures += expect_parse_status(
        "CHANGE MASTER TO MASTER_HOST='h'",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "removed CHANGE MASTER"
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
            .sql = "SET sql_mode = default; CREATE PROCEDURE p() BEGIN DECLARE y INT; END",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT,
        },
        {
            .sql = ("CREATE PROCEDURE body_features() BEGIN "
                    "DECLARE done BOOL DEFAULT FALSE; "
                    "DECLARE v INT DEFAULT 0; "
                    "DECLARE no_more_rows CONDITION FOR SQLSTATE '02000'; "
                    "DECLARE cur CURSOR FOR SELECT id FROM t; "
                    "DECLARE CONTINUE HANDLER FOR no_more_rows SET done = TRUE; "
                    "body_label: BEGIN "
                    "IF v = 0 THEN SET v = 1; ELSEIF v = 1 THEN SET v = 2; "
                    "ELSE SET v = 3; END IF; "
                    "CASE v WHEN 1 THEN SET v = 2; ELSE SET v = 4; END CASE; "
                    "loop_label: LOOP SET v = v + 1; "
                    "IF v > 4 THEN LEAVE loop_label; END IF; "
                    "ITERATE loop_label; END LOOP loop_label; "
                    "REPEAT SET v = v - 1; UNTIL v = 0 END REPEAT; "
                    "WHILE v < 1 DO SET v = v + 1; END WHILE; "
                    "OPEN cur; FETCH cur INTO v; CLOSE cur; "
                    "END body_label; END"),
            .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT,
        },
        {
            .sql = "DROP PROCEDURE IF EXISTS p; CREATE PROCEDURE p() SELECT 1; CALL p()",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT,
        },
        {
            .sql = "CREATE FUNCTION f() RETURNS INT RETURN 1",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT,
        },
        {
            .sql = "CREATE FUNCTION f_body() RETURNS INT DETERMINISTIC NO SQL BEGIN RETURN 1; END",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT,
        },
        {
            .sql = "CREATE FUNCTION udf_i RETURNS INTEGER SONAME 'missing_udf.so'",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT,
        },
        {
            .sql = "CREATE FUNCTION IF NOT EXISTS udf_s RETURNS STRING SONAME 'missing_udf.so'",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT,
        },
        {
            .sql = "CREATE AGGREGATE FUNCTION udf_r RETURNS REAL SONAME 'missing_udf.so'",
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
        {
            .sql = "ALTER DEFINER=mysqltest_u1@localhost EVENT e1 ON SCHEDULE EVERY 1 HOUR",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT,
        },
        {.sql = "ALTER EVENT e DISABLE", .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT
        },
        {
            .sql = "SIGNAL SQLSTATE '01000'",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT,
        },
        {.sql = "RESIGNAL", .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT},
        {
            .sql = "DROP FUNCTION IF EXISTS f",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT,
        },
        {.sql = "DROP FUNCTION udf_i", .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT},
        {
            .sql = "DROP TRIGGER IF EXISTS tr",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT,
        },
        {.sql = "DROP EVENT IF EXISTS e",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT},
        {.sql = "CALL mtr.p(OUT @arg)", .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT
        },
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_kind(statements[index]);
    }
    failures +=
        expect_parse_status("END IF", MYLITE_SQL_PARSE_SYNTAX_ERROR, "body-only END IF fragment");
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

static int test_flush_script_form(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *first = NULL;
    const struct mylite_sql_ast_node *second = NULL;
    int failures =
        parser_test_parse_sql("FLUSH TABLES;\nUNLOCK TABLES", MYLITE_SQL_PARSE_OK, &result);

    failures += parser_test_expect_child_count(result.root, 2U, "flush/unlock script count");
    first = parser_test_child_at(result.root, 0U);
    second = parser_test_child_at(result.root, 1U);
    failures += parser_test_expect_node(first, MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT, "flush tables");
    failures += parser_test_expect_span_text(first, "FLUSH TABLES", "flush tables statement span");
    failures +=
        parser_test_expect_node(second, MYLITE_SQL_AST_UNLOCK_TABLES_STATEMENT, "unlock tables");
    failures += parser_test_expect_span_text(second, "UNLOCK TABLES", "unlock tables span");
    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_removed_flush_forms_rejected(void) {
    int failures = 0;

    failures +=
        expect_parse_status("FLUSH HOSTS", MYLITE_SQL_PARSE_SYNTAX_ERROR, "removed FLUSH HOSTS");
    failures += expect_parse_status(
        "FLUSH QUERY CACHE",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "removed FLUSH QUERY CACHE"
    );
    failures += expect_parse_status(
        "FLUSH TABLES, STATUS",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "FLUSH TABLES cannot be mixed with flush options"
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
