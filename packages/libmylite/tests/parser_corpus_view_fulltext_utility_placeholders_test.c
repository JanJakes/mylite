#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_unsupported_statement_placeholders(void);
static int test_noop_statement_placeholders(void);
static int test_syntax_errors_remain_syntax_errors(void);
static int expect_statement_kind(struct expected_statement expected);
static int expect_syntax_error(const char *sql);
static int parse_ok(const char *sql);

int main(void) {
    int failures = 0;

    failures += test_unsupported_statement_placeholders();
    failures += test_noop_statement_placeholders();
    failures += test_syntax_errors_remain_syntax_errors();

    return failures == 0 ? 0 : 1;
}

static int test_unsupported_statement_placeholders(void) {
    static const struct expected_statement placeholders[] = {
        {
            .sql = "CREATE VIEW v AS SELECT LPAD('x', 1 NOT IN (0), 1) AS c",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        },
        {
            .sql = "CREATE ALGORITHM=TEMPTABLE VIEW v AS "
                   "SELECT t1.a FROM (t1 JOIN t2 ON t1.a = t2.a)",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        },
        {
            .sql = "ALTER VIEW v AS SELECT RPAD('x', 1 NOT IN (0), 1) AS c",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        },
        {
            .sql = "SELECT * FROM t1 WHERE MATCH a,b AGAINST ('+mysql*' IN BOOLEAN MODE)",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        },
        {
            .sql = "SELECT t1.q, t2.item, MATCH t2.item AGAINST ('sushi' IN BOOLEAN MODE) "
                   "AS x FROM t1, t2 ORDER BY x DESC",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        },
        {
            .sql = "SELECT * FROM t2 WHERE MATCH name AGAINST ('*a*b*c*' IN BOOLEAN MODE)",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        },
        {
            .sql = "UPDATE t1 SET a = 'some test' WHERE MATCH a,b AGAINST ('model')",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        },
        {
            .sql = "DELETE FROM t1 WHERE MATCH a AGAINST ('000000')",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        },
        {
            .sql = "SELECT * INTO OUTFILE 'tmp1.txt' FROM t1",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        },
        {
            .sql = "SELECT title INTO DUMPFILE 'tmp1.bin' FROM t1 LIMIT 1",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        },
        {
            .sql = "LOAD XML INFILE '../../std_data/loadxml.dat' INTO TABLE t1 "
                   "ROWS IDENTIFIED BY '<row>'",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        },
        {
            .sql = "IMPORT TABLE FROM 't1_*.sdi', 't2_*.sdi'",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        },
        {
            .sql = "ALTER TABLE t DISCARD TABLESPACE",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        },
        {
            .sql = "ALTER TABLE t DISCARD PARTITION p2 TABLESPACE",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        },
        {
            .sql = "ALTER TABLE t IMPORT PARTITION p1sp0 TABLESPACE",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        },
        {.sql = "HELP 'function_of_my_dream'", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT
        },
    };
    int failures = 0;

    failures += parse_ok("SELECT MATCH(title, body) AGAINST ('needle') FROM articles");
    failures += parse_ok("CREATE VIEW v AS SELECT id FROM source");
    for (size_t index = 0U; index < sizeof(placeholders) / sizeof(placeholders[0]); ++index) {
        failures += expect_statement_kind(placeholders[index]);
    }
    return failures;
}

static int test_noop_statement_placeholders(void) {
    static const struct expected_statement statements[] = {
        {
            .sql = "LOCK INSTANCE FOR BACKUP",
            .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT,
        },
        {
            .sql = "UNLOCK INSTANCE",
            .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT,
        },
        {
            .sql = "CHANGE REPLICATION SOURCE TO SOURCE_USER='plug_user', "
                   "SOURCE_PASSWORD='plug_user', SOURCE_RETRY_COUNT=0",
            .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT,
        },
        {
            .sql = "CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu' ENGINE InnoDB",
            .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT,
        },
        {
            .sql = "ALTER UNDO TABLESPACE undo_003 SET ACTIVE ENGINE InnoDB",
            .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT,
        },
        {
            .sql = "DROP UNDO TABLESPACE undo_003 ENGINE InnoDB",
            .kind = MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT,
        },
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_kind(statements[index]);
    }
    return failures;
}

static int test_syntax_errors_remain_syntax_errors(void) {
    int failures = 0;

    failures += expect_syntax_error("MATCH a AGAINST ('x')");
    failures += expect_syntax_error("SELECT * FROM t1 WHERE MATCH a AGAINST");
    failures += expect_syntax_error("SELECT 1 INTO OUTFILE");
    failures += expect_syntax_error("CREATE VIEW v AS");
    failures += expect_syntax_error("HELP");
    failures += expect_syntax_error("LOCK INSTANCE");

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

static int expect_syntax_error(const char *sql) {
    struct mylite_sql_parse_result result;
    int failures = parser_test_parse_sql(sql, MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);

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
