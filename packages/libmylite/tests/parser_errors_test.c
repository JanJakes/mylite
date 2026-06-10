#include "parser_test_support.h"

#include <stdio.h>
#include <string.h>

static int test_syntax_errors(void);
static int test_lexer_errors(void);

int main(void) {
    int failures = 0;

    failures += test_syntax_errors();
    failures += test_lexer_errors();

    return failures == 0 ? 0 : 1;
}

static int test_syntax_errors(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SELECT FROM DUAL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    if (result.error_token.text == NULL || result.error_token.length != strlen("FROM") ||
        memcmp(result.error_token.text, "FROM", strlen("FROM")) != 0) {
        fprintf(stderr, "expected syntax error token FROM\n");
        failures = 1;
    }
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT 1 +;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT INTERVAL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT INTEGER;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT 1 WHERE TRUE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT 1, * FROM DUAL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT DATABASE(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT SCHEMA(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT USER(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT CURRENT_USER(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT SESSION_USER(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT SYSTEM_USER(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT SESSION_USER ();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT SYSTEM_USER ();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT SESSION_USER/**/();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT SYSTEM_USER/**/();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT DATABASE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT SCHEMA;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT DATABASE() LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT USER() LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT CURRENT_USER LIMIT 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT MOD();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT MOD(5);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT MOD(5,2,1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT DIV(5,2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT 5 DIV;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT DIV 2;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT 1=;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT =1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT 1<=>;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT 1 AND;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT AND 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT 1 OR;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT 1 XOR;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT NOT;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT 1&&1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT 1||0;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE DATABASE IF EXISTS app;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE DATABASE app DEFAULT ENCRYPTION='N';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SET NAMES;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SET NAMES NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SET NAMES utf8mb4, latin1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SET CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SET NAMES ?;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SET NAMES CONCAT('utf8', 'mb4');",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE DATABASE a.b;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DROP DATABASE IF NOT EXISTS app;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW DATABASES LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW DATABASES LIKE NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW DATABASES LIKE N'app%';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW DATABASES WHERE Database = 'app';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW DATABASES LIKE 'app%' WHERE `Database` = 'app';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW FULL DATABASES;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW FULL TABLE STATUS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW EXTENDED TABLE STATUS;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW TABLE STATUS FULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW TABLE STATUS LIKE 'a%' WHERE Name = 't';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW TABLE STATUS WHERE Name = 't' ORDER BY Name;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW TABLE STATUS LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW TABLE STATUS LIKE NULL;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW TABLE STATUS LIKE N't%';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW TABLE STATUS LIKE 't%' FROM app;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW CHARACTER SETS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW CHARSETS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW COLLATIONS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW CHARACTER SET LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW CHARACTER SET LIKE NULL;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW CHARACTER SET LIKE N'utf8mb4';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW CHARACTER SET LIKE _utf8mb4'utf8mb4';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW COLLATION LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW COLLATION LIKE NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW COLLATION LIKE N'utf8mb4_0900_ai_ci';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW COLLATION LIKE _utf8mb4'utf8mb4_0900_ai_ci';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW CREATE DATABASE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW CREATE DATABASE IF NOT EXISTS app;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW CREATE DATABASE IF EXISTS app;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW CREATE DATABASE app LIKE 'a%';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW CREATE DATABASE app DEFAULT CHARACTER SET utf8mb4;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW CREATE DATABASE app.db;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW ENGINES LIKE 'InnoDB';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW ENGINES WHERE Engine = 'InnoDB';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW FULL ENGINES;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW PLUGINS LIKE 'InnoDB';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW PLUGINS WHERE Name = 'InnoDB';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW FULL PLUGINS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW PLUGINS FROM mysql;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW ENGINE InnoDB MUTEX;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW ENGINE InnoDB LOGS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW ENGINE InnoDB STATUS LIKE '%';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW FULL ENGINE InnoDB STATUS;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW EXTENDED FULL COLUMNS FROM t;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW CREATE TABLE t LIKE 't';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW CREATE TABLE t WHERE Table = 't';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW CREATE TABLE t FROM app;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW CREATE TEMPORARY TABLE t;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW FULL CREATE TABLE t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW EXTENDED COLUMNS FROM t;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW COLUMNS FROM t LIKE 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW COLUMNS FROM t LIKE 'i%' WHERE Field = 'id';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DESCRIBE t id;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DESCRIBE SELECT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("EXPLAIN t id;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("EXPLAIN t 'i%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("EXPLAIN t FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "EXPLAIN FORMAT=DEFAULT SELECT 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "EXPLAIN FORMAT=JSON ANALYZE SELECT 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("EXPLAIN FOR CONNECTION 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("EXPLAIN EXTENDED SELECT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "EXPLAIN PARTITIONS SELECT 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("CREATE TABLE t ();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE a.b.c (id INT);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE t (id VARCHAR);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE t (id INT DEFAULT (NULL));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_first_child_kind(
                parser_test_child_at(
                    parser_test_child_at(parser_test_child_at(result.root, 0U), 1U),
                    0U
                ),
                MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE
            ),
            0U
        ),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized NULL default"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_first_child_kind(
                    parser_test_child_at(
                        parser_test_child_at(parser_test_child_at(result.root, 0U), 1U),
                        0U
                    ),
                    MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE
                ),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_NULL,
        "parenthesized NULL default literal"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE t (id INT DEFAULT (1 + 2));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_first_child_kind(
                    parser_test_child_at(
                        parser_test_child_at(parser_test_child_at(result.root, 0U), 1U),
                        0U
                    ),
                    MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE
                ),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "parenthesized expression default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE t (id INT DEFAULT '5');", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(
                parser_test_child_at(
                    parser_test_child_at(parser_test_child_at(result.root, 0U), 1U),
                    0U
                ),
                MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE
            ),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_STRING,
        "string integer default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE t (id INT DEFAULT 1.5);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE t (b BINARY(2) DEFAULT 0x1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(
                parser_test_child_at(
                    parser_test_child_at(parser_test_child_at(result.root, 0U), 1U),
                    0U
                ),
                MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE
            ),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_HEX,
        "hex binary default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE t (id INT DEFAULT 1 + 2);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE t (id INT DEFAULT NULL NULL);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE t (id INT DEFAULT NULL DEFAULT NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE t (id INT DEFAULT NULL NOT NULL);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE t (id INT(+1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE t (id INT) ENGINE=DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE t (id INT) DEFAULT CHARSET=DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE t (id INT) COLLATE=DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE t (id INT) DEFAULT CHARACTER SET;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE t (id INT) ENGINE=InnoDB, DEFAULT CHARSET=utf8mb4;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE t (id INT) COMMENT=123;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE t (id INT) COMMENT=abc;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE t (id INT) COMMENT=NULL;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE IF EXISTS t (id INT);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE IF NOT t (id INT);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DROP TABLE IF NOT EXISTS t;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "TRUNCATE TABLE IF EXISTS t;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("TRUNCATE TABLE a, b;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "TRUNCATE TABLE t WHERE id = 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("TRUNCATE TABLE t LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "TRUNCATE TEMPORARY TABLE t;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "RENAME TABLE old_name new_name;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "RENAME TABLE old_name TO new_name,;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name RENAME TABLE new_name;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name RENAME new_name, ADD COLUMN added INT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name RENAME new_name, RENAME final_name;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name ADD COLUMN added INT AFTER old_name.id;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name ADD COLUMN old_name.added INT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name ADD COLUMN added VARCHAR;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_signed_unsigned (c INT SIGNED UNSIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_unsigned_signed (c INT UNSIGNED SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_repeated_signed (c INT SIGNED SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_repeated_unsigned (c INT UNSIGNED UNSIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_alias_zerofill (c INT2 ZEROFILL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        "alias zerofill placeholder"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_alias_signed_unsigned (c INT1 SIGNED UNSIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_alias_unsigned_signed (c INT1 UNSIGNED SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_alias_repeated_signed (c INT1 SIGNED SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_alias_repeated_unsigned (c INT1 UNSIGNED UNSIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_decimal_signed (c DECIMAL SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_width_minus (c INT(-1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_width_empty (c INT());",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_width_decimal (c INT(1.0));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_width_string (c INT('1'));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_width_hex (c INT(0x1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_width_bit (c INT(b'1'));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_width_parameter (c INT(?));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_width_expression (c INT(1+1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_width_after_unsigned (c INT UNSIGNED(1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_alias_width_signed (c INT1(+1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_zerofill (c MEDIUMINT ZEROFILL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        "integer zerofill placeholder"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_int5 (c INT5);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_bool_width (c BOOL(1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_boolean_width (c BOOLEAN(1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_bool_signed (c BOOL SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_bool_unsigned (c BOOL UNSIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_bool_zerofill (c BOOL ZEROFILL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        "bool zerofill placeholder"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_bool_signed_unsigned (c BOOL SIGNED UNSIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name DROP COLUMN old_name.added;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name DROP COLUMN added, DROP COLUMN other;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("ALTER TABLE old_name DROP INDEX idx;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name DROP FOREIGN KEY fk;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name DROP COLUMN IF EXISTS added;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name DROP (added);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name DROP COLUMN added FIRST;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name DROP COLUMN added, ALGORITHM=INSTANT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT,
        "alter drop column algorithm option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name DROP COLUMN added, LOCK=DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_DEFAULT,
        "alter drop column lock option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name RENAME old_col TO new_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_name.old_col TO new_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_col TO old_name.new_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_col new_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_col TO new_col FIRST;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_col TO new_col, RENAME COLUMN other TO final;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_col TO new_col, ALGORITHM=INSTANT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT,
        "alter rename column algorithm option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_col TO new_col, LOCK=DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_DEFAULT,
        "alter rename column lock option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name MODIFY old_name.old_col BIGINT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name MODIFY old_col BIGINT AFTER old_name.other_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name MODIFY old_col BIGINT, MODIFY other_col BIGINT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name MODIFY old_col VARCHAR;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name MODIFY old_col BIGINT, ALGORITHM=INSTANT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name MODIFY old_col BIGINT, LOCK=DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name CHANGE old_name.old_col new_col BIGINT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name CHANGE old_col old_name.new_col BIGINT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name CHANGE old_col new_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name CHANGE old_col new_col BIGINT AFTER old_name.other_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name CHANGE old_col new_col BIGINT, CHANGE other_col final_col BIGINT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name CHANGE old_col new_col VARCHAR;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name CHANGE old_col new_col BIGINT, ALGORITHM=INSTANT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name CHANGE old_col new_col BIGINT, LOCK=DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name ALTER COLUMN old_name.old_col SET DEFAULT 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name ALTER old_name.old_col SET DEFAULT 1, ALTER other DROP DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name ALTER old_col SET DEFAULT (1 + 1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 2U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "alter set parenthesized expression default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name ALTER old_col SET DEFAULT '1';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "alter set string default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name ALTER COLUMN old_name.old_col DROP DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name ALTER COLUMN old_name.old_col SET INVISIBLE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name ALTER old_col SET INVISIBLE, ALTER other SET VISIBLE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name ALTER old_col SET HIDDEN;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE t (id INT INVISIBLE);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE t ADD COLUMN v INT INVISIBLE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("INSERT INTO t VALUES (1.5);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO t VALUES (+TRUE);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("INSERT INTO t VALUE ROW();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO t VALUES ROW(1), (2);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO t VALUES (1), ROW(2);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("REPLACE INTO t VALUES (1.5);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE INTO t VALUE ROW(1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE INTO t VALUES ROW(1), (2);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE INTO t VALUES (1), ROW(2);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE LOW_PRIORITY DELAYED INTO t VALUES (1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT LOW_PRIORITY HIGH_PRIORITY INTO t VALUES (1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT HIGH_PRIORITY LOW_PRIORITY INTO t VALUES (1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT LOW_PRIORITY DELAYED INTO t VALUES (1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT IGNORE LOW_PRIORITY INTO t VALUES (1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT IGNORE LOW_PRIORITY INTO t SELECT id FROM src;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO LOW_PRIORITY t VALUES (1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE HIGH_PRIORITY INTO t VALUES (1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE INTO LOW_PRIORITY t VALUES (1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE INTO t TABLE source;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE INTO t (app.id) VALUES (1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("REPLACE INTO t VALUES (?);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE INTO t VALUES ((SELECT 1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("REPLACE INTO t VALUES (1.5);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("REPLACE INTO t VALUES (1e0);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("REPLACE INTO t VALUES (0x1);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("REPLACE INTO t VALUES (b'1');", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("REPLACE INTO t VALUES (DEFAULT(id));", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("REPLACE INTO t SET id = 1.5;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("REPLACE INTO t SET id = DEFAULT(id);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("REPLACE INTO t SET id = ?;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE INTO t SET id = (SELECT 1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("REPLACE INTO t SET id = 1.5;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("REPLACE INTO t SET id = 1e0;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("REPLACE INTO t SET id = 0x1;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("REPLACE INTO t SET id = b'1';", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE DELAYED LOW_PRIORITY INTO t SET id = 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE HIGH_PRIORITY INTO t SET id = 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT id FROM t WHERE id + 1 = 2;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT id FROM t WHERE id = '1';", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT DEFAULT(id), DEFAULT(t.id) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT DEFAULT(1) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t WHERE id XOR nn = 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT id FROM t WHERE ! (id = 1);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t WHERE ABS(id) = 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT id FROM t WHERE id = b'1';", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t JOIN other USING (id);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t WHERE id = 1 ORDER BY nn, id;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t WHERE id = 1 LIMIT +1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t LIMIT TRUE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("DELETE FROM t LIMIT +1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("DELETE FROM t LIMIT FALSE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DELETE FROM t LIMIT 1 OFFSET 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("DELETE FROM t LIMIT 1, 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DELETE FROM t ORDER BY id, nn;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DELETE LOW_PRIORITY FROM t;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("UPDATE t SET id = 1.5;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("UPDATE t SET id = 1 + 2;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("UPDATE t SET id = (1 + 2);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("UPDATE t SET id = 2 + 3 * 4;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("UPDATE t SET id = (2 + 3) * 4;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("UPDATE t SET id = -FALSE;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("UPDATE t SET id = TRUE + 1;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("UPDATE t SET id = NULL + 1;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("UPDATE t SET id = 2 * 3;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("UPDATE t SET id = 1 / 2;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("UPDATE t SET id = other;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("UPDATE t SET id = DEFAULT(id);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE t SET id = 1 LIMIT +1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE t SET id = 1 LIMIT FALSE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE t SET id = 1 LIMIT 1 OFFSET 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE t SET id = 1 LIMIT 1, 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE t SET id = 1 ORDER BY id, nn LIMIT 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("UPDATE LOW_PRIORITY t SET id = 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        parser_test_first_child_kind(statement, MYLITE_SQL_AST_UPDATE_LOW_PRIORITY_MODIFIER),
        MYLITE_SQL_AST_UPDATE_LOW_PRIORITY_MODIFIER,
        "update low priority modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("UPDATE IGNORE t SET id = 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        parser_test_first_child_kind(statement, MYLITE_SQL_AST_UPDATE_IGNORE_MODIFIER),
        MYLITE_SQL_AST_UPDATE_IGNORE_MODIFIER,
        "update ignore modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE LOW_PRIORITY IGNORE t SET id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        parser_test_first_child_kind(statement, MYLITE_SQL_AST_UPDATE_LOW_PRIORITY_MODIFIER),
        MYLITE_SQL_AST_UPDATE_LOW_PRIORITY_MODIFIER,
        "update low priority ignore low priority modifier"
    );
    failures += parser_test_expect_node(
        parser_test_first_child_kind(statement, MYLITE_SQL_AST_UPDATE_IGNORE_MODIFIER),
        MYLITE_SQL_AST_UPDATE_IGNORE_MODIFIER,
        "update low priority ignore modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE IGNORE LOW_PRIORITY t SET id = 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("UPDATE t AS x SET id = 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "WITH c AS (SELECT id FROM t) UPDATE t SET id = 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_lexer_errors(void) {
    struct mylite_sql_parse_result result;
    int failures = 0;

    failures +=
        parser_test_parse_sql("SELECT 'unterminated", MYLITE_SQL_PARSE_LEXER_ERROR, &result);
    if (result.error_token.error != MYLITE_SQL_LEXER_ERROR_UNTERMINATED_STRING) {
        fprintf(
            stderr,
            "expected unterminated string lexer error, got %s\n",
            mylite_sql_lexer_error_name(result.error_token.error)
        );
        failures = 1;
    }

    mylite_sql_parse_result_deinit(&result);
    return failures;
}
