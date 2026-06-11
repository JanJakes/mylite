#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_executable_ddl_aliases(void);
static int test_ddl_residual_placeholders(void);
static int test_ddl_residual_malformed_tails(void);
static int expect_statement_kind(struct expected_statement expected);
static int parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_executable_ddl_aliases();
    failures += test_ddl_residual_placeholders();
    failures += test_ddl_residual_malformed_tails();

    return failures == 0 ? 0 : 1;
}

static int test_executable_ddl_aliases(void) {
    static const struct expected_statement statements[] = {
        {
            .sql = "CREATE TABLE type_aliases ("
                   "d DOUBLE PRECISION(42,12), r REAL(42,12), "
                   "f FLOAT(58,0) SIGNED, fw FLOAT(10.3), "
                   "y YEAR UNSIGNED, y4 YEAR(4) UNSIGNED, "
                   "vb VARCHAR(10) BYTE, lb LONG BYTE)",
            .kind = MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        },
        {
            .sql = "CREATE TABLE duplicate_defaults ("
                   "a INT DEFAULT 1 DEFAULT 2, b INT DEFAULT NULL DEFAULT 5, "
                   "c INT DEFAULT 6 DEFAULT NULL, d CHAR(4) DEFAULT 'a' DEFAULT 'b')",
            .kind = MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        },
        {
            .sql = "CREATE INDEX e_index TYPE btree ON t1(e)",
            .kind = MYLITE_SQL_AST_CREATE_INDEX_STATEMENT,
        },
        {
            .sql = "CREATE INDEX m_index ON t1(m) TYPE btree",
            .kind = MYLITE_SQL_AST_CREATE_INDEX_STATEMENT,
        },
        {
            .sql = "ALTER TABLE t1 CHARACTER SET binary",
            .kind = MYLITE_SQL_AST_ALTER_TABLE_DEFAULT_CHARSET_COLLATION_STATEMENT,
        },
        {
            .sql = "ALTER TABLE t1 CONVERT TO CHARACTER SET DEFAULT COLLATE utf8mb4_bin",
            .kind = MYLITE_SQL_AST_ALTER_TABLE_CONVERT_CHARACTER_SET_STATEMENT,
        },
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_kind(statements[index]);
    }
    return failures;
}

static int test_ddl_residual_placeholders(void) {
    static const struct expected_statement statements[] = {
        {
            .sql = "CREATE TABLE ft_parser (a TEXT, FULLTEXT(a) WITH PARSER simple_parser)",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        },
        {
            .sql = "ALTER TABLE ft_parser ADD FULLTEXT(a) WITH PARSER simple_parser",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        },
        {
            .sql = "CREATE FULLTEXT INDEX ft_a ON ft_parser(a) WITH PARSER simple_parser",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        },
        {
            .sql = "CREATE TABLE generated_residual ("
                   "pk INT NOT NULL AUTO_INCREMENT, c INT NOT NULL, "
                   "g INT GENERATED ALWAYS AS ((c + c)) VIRTUAL NOT NULL, "
                   "PRIMARY KEY (pk)) ENGINE=InnoDB AUTO_INCREMENT=30 DEFAULT CHARSET=utf8mb4",
            .kind = MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        },
        {
            .sql = "CREATE TABLE fk_default ("
                   "a INT, FOREIGN KEY (a) REFERENCES parent(a) ON DELETE SET DEFAULT)",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        },
        {
            .sql = "ALTER TABLE t ADD COLUMN new_col INT, ORDER BY payoutid,bandid",
            .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        },
        {
            .sql = "ALTER TABLE t MODIFY COLUMN c1 FLOAT(10.3), DROP CHECK t_chk_1, "
                   "ADD CONSTRAINT CHECK(c1 > 10.1) ENFORCED",
            .kind = MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT,
        },
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_kind(statements[index]);
    }
    return failures;
}

static int test_ddl_residual_malformed_tails(void) {
    int failures = 0;

    failures += parse_status(
        "CREATE TABLE bad_generated (c INT, g INT GENERATED ALWAYS AS () VIRTUAL NOT NULL) "
        "ENGINE=InnoDB",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "empty generated expression"
    );
    failures += parse_status(
        "CREATE TABLE bad_generated (c INT, g INT GENERATED ALWAYS AS (c +) VIRTUAL NOT NULL) "
        "ENGINE=InnoDB",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete generated expression"
    );
    failures += parse_status(
        "CREATE TABLE bad_fulltext (a TEXT, FULLTEXT(a) WITH PARSER)",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete fulltext parser"
    );
    failures += parse_status(
        "CREATE TABLE bad_fk (a INT, FOREIGN KEY (a) REFERENCES parent(a) ON DELETE SET)",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete foreign key action"
    );
    failures += parse_status(
        "ALTER TABLE t ADD COLUMN new_col INT, ORDER BY",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete alter table order by action"
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
