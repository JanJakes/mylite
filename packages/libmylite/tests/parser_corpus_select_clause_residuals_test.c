#include "parser_test_support.h"

static int test_select_clause_residuals(void);
static int parse_ok(const char *sql);

int main(void) {
    int failures = 0;

    failures += test_select_clause_residuals();

    return failures == 0 ? 0 : 1;
}

static int test_select_clause_residuals(void) {
    int failures = 0;

    failures += parse_ok("SELECT 0 LIMIT 0");
    failures += parse_ok("SELECT 1 AS a LIMIT 1, 10");
    failures += parse_ok("SELECT 1 AS a LIMIT 1 OFFSET 2");
    failures += parse_ok("SELECT 9 FROM DUAL WHERE 1 LIMIT 1");
    failures += parse_ok("SELECT id FROM t1 FOR SHARE OF t1 FOR UPDATE OF t2");
    failures += parse_ok("SELECT t1.id, t2.id FROM t1 JOIN t2 ON t1.id = t2.id "
                         "FOR SHARE OF t1 NOWAIT FOR UPDATE OF t2 SKIP LOCKED");
    failures += parse_ok("SELECT id FROM t1 LOCK IN SHARE MODE FOR UPDATE");
    failures += parse_ok("SELECT id FROM t1 ORDER BY NULL");
    failures += parse_ok("SELECT id FROM t1 ORDER BY 'a' DESC");
    failures += parse_ok("SELECT id FROM t1 ORDER BY @rank");
    failures += parse_ok("SELECT a,b FROM t1 GROUP BY a,b HAVING b='hello'");
    failures += parse_ok("SELECT t1.a AS t1c1, t2.a AS t2c1 "
                         "FROM t1 JOIN t2 ON t1.id=t2.id HAVING t1c1 = t2c1");
    failures += parse_ok("SELECT t1.a AS t1c1, t2.a AS t2c1 "
                         "FROM t1 JOIN t2 ON t1.id=t2.id HAVING t1c1 != t2c1");
    failures += parse_ok("SELECT a FROM t1 GROUP BY a HAVING a IN (10,20)");
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
