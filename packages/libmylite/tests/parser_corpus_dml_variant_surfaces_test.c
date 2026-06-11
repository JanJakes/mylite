#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_delete_modifier_grammar(void);
static int test_dml_variant_placeholders(void);
static int test_dml_variant_malformed_tails(void);
static int expect_statement_kind(struct expected_statement expected);
static int parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_delete_modifier_grammar();
    failures += test_dml_variant_placeholders();
    failures += test_dml_variant_malformed_tails();

    return failures == 0 ? 0 : 1;
}

static int test_delete_modifier_grammar(void) {
    int failures = 0;

    failures += expect_statement_kind((struct expected_statement){
        .sql = "DELETE LOW_PRIORITY FROM t WHERE id = 1",
        .kind = MYLITE_SQL_AST_DELETE_STATEMENT,
    });
    failures += expect_statement_kind((struct expected_statement){
        .sql = "DELETE QUICK FROM t ORDER BY id LIMIT 1",
        .kind = MYLITE_SQL_AST_DELETE_STATEMENT,
    });
    failures += expect_statement_kind((struct expected_statement){
        .sql = "DELETE LOW_PRIORITY QUICK FROM t",
        .kind = MYLITE_SQL_AST_DELETE_STATEMENT,
    });

    return failures;
}

static int test_dml_variant_placeholders(void) {
    static const struct expected_statement placeholders[] = {
        {.sql = "DELETE IGNORE FROM t WHERE id = 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "DELETE LOW_PRIORITY IGNORE FROM t WHERE id = 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "DELETE t FROM t WHERE id = 1", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT
        },
        {.sql = "DELETE LOW_PRIORITY t FROM t WHERE id = 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "DELETE LOW_PRIORITY QUICK t FROM t WHERE id = 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "DELETE FROM a USING t AS a WHERE a.id = 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "DELETE LOW_PRIORITY FROM a USING t AS a WHERE a.id = 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "DELETE LOW_PRIORITY QUICK FROM a USING t AS a WHERE a.id = 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "DELETE t.*, u.* FROM t, u WHERE t.id = u.id",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "DELETE FROM t ORDER BY id, v DESC LIMIT 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "DELETE FROM t WHERE (@a:=id) ORDER BY id LIMIT 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "UPDATE IGNORE t, u SET t.v = u.v WHERE t.id = u.id",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "UPDATE t LEFT JOIN u USING(id) SET t.v = u.v",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "UPDATE t SET v = 10 ORDER BY id, v DESC LIMIT 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "INSERT INTO t (id, v) VALUES (id, v)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "INSERT INTO t SET dt = '2007-03-23 13:49:38', da = dt",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "INSERT INTO t VALUES (1) ON DUPLICATE KEY UPDATE v = v",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "INSERT INTO t VALUES() AS n ON DUPLICATE KEY UPDATE v = 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "INSERT INTO t VALUES(1, 10) AS n",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "INSERT INTO t VALUES(1, 10) AS n(id_alias, v_alias) "
                "ON DUPLICATE KEY UPDATE v = n.v_alias",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "INSERT INTO t SELECT * FROM t AS source "
                "ON DUPLICATE KEY UPDATE t.v = source.v",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "REPLACE INTO t SELECT id, v FROM u UNION ALL SELECT id, v FROM s",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
    };
    int failures = 0;

    failures += expect_statement_kind((struct expected_statement){
        .sql = "UPDATE t SET v = 10 ORDER BY id LIMIT 1",
        .kind = MYLITE_SQL_AST_UPDATE_STATEMENT,
    });
    for (size_t index = 0U; index < sizeof(placeholders) / sizeof(placeholders[0]); ++index) {
        failures += expect_statement_kind(placeholders[index]);
    }
    return failures;
}

static int test_dml_variant_malformed_tails(void) {
    int failures = 0;

    failures +=
        parse_status("DELETE IGNORE FROM", MYLITE_SQL_PARSE_SYNTAX_ERROR, "incomplete delete");
    failures += parse_status(
        "DELETE IGNORE t",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "delete ignore without source"
    );
    failures += parse_status(
        "DELETE QUICK LOW_PRIORITY FROM t",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "out-of-order delete modifiers"
    );
    failures += parse_status(
        "DELETE LOW_PRIORITY IGNORE QUICK FROM t",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "delete quick after ignore"
    );
    failures +=
        parse_status("DELETE t FROM", MYLITE_SQL_PARSE_SYNTAX_ERROR, "incomplete joined delete");
    failures += parse_status(
        "UPDATE IGNORE t, SET v = 1",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete joined update source"
    );
    failures += parse_status(
        "INSERT INTO t (id, v) VALUES (id,)",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete insert identifier value list"
    );
    failures += parse_status(
        "INSERT INTO t VALUES(1) AS",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete insert row alias"
    );
    failures += parse_status(
        "INSERT INTO t VALUES(1) AS n(",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete insert row alias column list"
    );
    failures += parse_status(
        "REPLACE INTO t SELECT id FROM",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete replace select source"
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
