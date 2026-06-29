#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_utility_noop_statement_forms(void);
static int test_unsupported_utility_statement_forms(void);
static int expect_statement_kind(struct expected_statement expected);
static int expect_parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_utility_noop_statement_forms();
    failures += test_unsupported_utility_statement_forms();

    return failures == 0 ? 0 : 1;
}

static int test_utility_noop_statement_forms(void) {
    static const struct expected_statement statements[] = {
        {
            .sql = "ANALYZE TABLE foo UPDATE HISTOGRAM ON col1 WITH 10 BUCKETS",
            .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT,
        },
        {
            .sql = "ANALYZE TABLE foo DROP HISTOGRAM ON col1",
            .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT,
        },
        {
            .sql = "INSTALL COMPONENT 'file://component_validate_password'",
            .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT,
        },
        {
            .sql = "UNINSTALL COMPONENT 'file://component_validate_password'",
            .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT,
        },
        {.sql = "INSTALL PLUGIN example SONAME 'ha_example.so'",
         .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT},
        {.sql = "UNINSTALL PLUGIN example", .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT},
        {
            .sql = "CREATE SPATIAL REFERENCE SYSTEM 2004326 NAME 'Copy of WGS 84' "
                   "ORGANIZATION 'EPSG' IDENTIFIED BY 2004326 DEFINITION 'GEOGCS[]'",
            .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT,
        },
        {
            .sql = "CREATE OR REPLACE SPATIAL REFERENCE SYSTEM 2004326 "
                   "NAME 'Copy of WGS 84' DEFINITION 'GEOGCS[]'",
            .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT,
        },
        {
            .sql = "CREATE SPATIAL REFERENCE SYSTEM IF NOT EXISTS 2004326 "
                   "NAME 'Copy of WGS 84' DEFINITION 'GEOGCS[]'",
            .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT,
        },
        {
            .sql = "DROP SPATIAL REFERENCE SYSTEM IF EXISTS 2004326",
            .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT,
        },
        {
            .sql = "DROP SPATIAL REFERENCE SYSTEM 2004326",
            .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT,
        },
        {
            .sql = "CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENGINE=InnoDB",
            .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT,
        },
        {.sql = "ALTER TABLESPACE ts1 RENAME TO ts2", .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT
        },
        {.sql = "DROP TABLESPACE ts1", .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT},
        {
            .sql = "CREATE LOGFILE GROUP lg1 ADD UNDOFILE 'undo.dat' ENGINE=InnoDB",
            .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT,
        },
        {
            .sql = "ALTER LOGFILE GROUP lg1 ADD UNDOFILE 'undo2.dat' ENGINE=NDB",
            .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT,
        },
        {.sql = "DROP LOGFILE GROUP lg1 ENGINE=NDB", .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT},
        {.sql = "SET GLOBAL max_allowed_packet=4*1024",
         .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT},
        {.sql = "SET @@GLOBAL.max_allowed_packet=1024*1024",
         .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT},
        {.sql = "SET SESSION max_points_in_geometry=4*1024*1024",
         .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT},
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_kind(statements[index]);
    }
    failures += expect_parse_status(
        "INSTALL COMPONENT 'file://c'; SELECT 1",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "utility no-op rejects multiple statements"
    );
    return failures;
}

static int test_unsupported_utility_statement_forms(void) {
    static const struct expected_statement statements[] = {
        {.sql = "XA START 'xid'", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "XA RECOVER", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "HANDLER t1 OPEN", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "HANDLER t1 READ FIRST WHERE id > 1 LIMIT 5",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "GET DIAGNOSTICS @n = NUMBER", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT
        },
        {.sql = "GET DIAGNOSTICS CONDITION 1 @errno = MYSQL_ERRNO",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SHOW PROFILE CPU FOR QUERY 15 LIMIT 2 OFFSET 2",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SHOW PROFILES", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "LOAD DATA INFILE 'tmp.txt' IGNORE INTO TABLE t1 FIELDS TERMINATED BY ','",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SHOW PROCEDURE CODE p", .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT
        },
        {.sql = "SHOW FUNCTION CODE f", .kind = MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT
        },
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_kind(statements[index]);
    }
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
