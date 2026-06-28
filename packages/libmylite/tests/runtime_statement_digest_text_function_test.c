#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_collation_utf8mb4_0900_ai_ci_id = 255,
    mysql_error_native_function_arity = 1582,
    mysql_error_statement_digest_parse = 3676,
    mysql_statement_digest_text_utf8mb4_display_length = 268435456,
    mysql_approximate_decimals = 31,
    scalar_result_column_count = 9,
    scalar_null_digest_column = 5,
    scalar_charset_column = 6,
    scalar_collation_column = 7,
    scalar_coercibility_column = 8,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_column_metadata {
    enum mylite_result_column_type type;
    uint32_t flags;
    uint32_t charset_id;
    uint32_t collation_id;
    uint64_t display_length;
    uint16_t decimals;
    int nullable;
    const char *context;
};

static int test_statement_digest_text_scalar(void);
static int test_statement_digest_text_row_contexts(void);
static int test_statement_digest_text_diagnostics(void);
static int test_statement_digest_text_metadata(void);
static int setup_database(mylite_db **out_database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_result_shape(
    const mylite_result *result,
    size_t column_count,
    size_t row_count,
    const char *context
);
static int expect_column_metadata(
    const mylite_result *result,
    size_t column,
    struct expected_column_metadata expected
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_uint16(uint16_t actual, uint16_t expected, const char *context);
static int expect_uint32(uint32_t actual, uint32_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_statement_digest_text_scalar();
    failures += test_statement_digest_text_row_contexts();
    failures += test_statement_digest_text_diagnostics();
    failures += test_statement_digest_text_metadata();

    return failures == 0 ? 0 : 1;
}

static int test_statement_digest_text_scalar(void) {
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = setup_database(&database);

    failures += execute_ok(
        database,
        "SELECT STATEMENT_DIGEST_TEXT('select 1') AS simple, "
        "STATEMENT_DIGEST_TEXT('select 1;') AS semicolon, "
        "STATEMENT_DIGEST_TEXT('select /*x*/ 1 from dual') AS comments, "
        "STATEMENT_DIGEST_TEXT('select * from t where id in (1,2,3)') AS in_list, "
        "STATEMENT_DIGEST_TEXT('select a+1,b>=2,c<>3,d<=>null "
        "from t where e like ''x%'' or f is not null') AS expr, "
        "STATEMENT_DIGEST_TEXT(NULL) AS null_digest, "
        "CHARSET(STATEMENT_DIGEST_TEXT('select 1')) AS charset_name, "
        "COLLATION(STATEMENT_DIGEST_TEXT('select 1')) AS collation_name, "
        "COERCIBILITY(STATEMENT_DIGEST_TEXT('select 1')) AS coercibility",
        &result
    );
    if (result != NULL) {
        failures += expect_result_shape(result, scalar_result_column_count, 1U, "scalar shape");
        failures +=
            expect_text(mylite_result_value_text(result, 0U, 0U), "SELECT ?", "simple digest");
        failures +=
            expect_text(mylite_result_value_text(result, 0U, 1U), "SELECT ? ;", "semicolon digest");
        failures += expect_text(
            mylite_result_value_text(result, 0U, 2U),
            "SELECT ? FROM DUAL",
            "comment digest"
        );
        failures += expect_text(
            mylite_result_value_text(result, 0U, 3U),
            "SELECT * FROM `t` WHERE `id` IN (...)",
            "in-list digest"
        );
        failures += expect_text(
            mylite_result_value_text(result, 0U, 4U),
            "SELECT `a` + ? , `b` >= ? , `c` != ? , `d` <=> ? FROM `t` "
            "WHERE `e` LIKE ? OR `f` IS NOT NULL",
            "expression digest"
        );
        failures += expect_text(
            mylite_result_value_text(result, 0U, scalar_null_digest_column),
            NULL,
            "null digest"
        );
        failures += expect_text(
            mylite_result_value_text(result, 0U, scalar_charset_column),
            "utf8mb4",
            "charset"
        );
        failures += expect_text(
            mylite_result_value_text(result, 0U, scalar_collation_column),
            "utf8mb4_0900_ai_ci",
            "collation"
        );
        failures += expect_text(
            mylite_result_value_text(result, 0U, scalar_coercibility_column),
            "4",
            "coercibility"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "DO STATEMENT_DIGEST_TEXT('select 1')", &result);
    if (result != NULL) {
        failures += expect_result_shape(result, 0U, 0U, "do shape");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "do affected rows");
        failures += expect_size(mylite_result_warning_count(result), 0U, "do warnings");
    }
    mylite_result_free(result);

    mylite_close(database);
    return failures;
}

static int test_statement_digest_text_row_contexts(void) {
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = setup_database(&database);

    failures += execute_ok(database, "CREATE TABLE digest_probe(id INT, sql_text TEXT)", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO digest_probe VALUES "
        "(1, 'select 1'), "
        "(2, 'select * from t where id in (1,2,3)'), "
        "(3, NULL)",
        NULL
    );
    failures += execute_ok(
        database,
        "SELECT id, STATEMENT_DIGEST_TEXT(sql_text) AS digest "
        "FROM digest_probe ORDER BY id",
        &result
    );
    if (result != NULL) {
        failures += expect_result_shape(result, 2U, 3U, "row projection shape");
        failures += expect_text(mylite_result_value_text(result, 0U, 0U), "1", "row id 1");
        failures += expect_text(mylite_result_value_text(result, 0U, 1U), "SELECT ?", "row 1");
        failures += expect_text(mylite_result_value_text(result, 1U, 0U), "2", "row id 2");
        failures += expect_text(
            mylite_result_value_text(result, 1U, 1U),
            "SELECT * FROM `t` WHERE `id` IN (...)",
            "row 2"
        );
        failures += expect_text(mylite_result_value_text(result, 2U, 0U), "3", "row id 3");
        failures += expect_text(mylite_result_value_text(result, 2U, 1U), NULL, "row 3");
    }
    mylite_result_free(result);
    result = NULL;

    mylite_close(database);
    return failures;
}

static int test_statement_digest_text_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = setup_database(&database);

    failures += execute_error(
        database,
        "SELECT STATEMENT_DIGEST_TEXT()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'STATEMENT_DIGEST_TEXT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT STATEMENT_DIGEST_TEXT('a','b')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'STATEMENT_DIGEST_TEXT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CHARSET(STATEMENT_DIGEST_TEXT())",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'STATEMENT_DIGEST_TEXT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT STATEMENT_DIGEST_TEXT('select from')",
        (struct expected_sql_error){
            .code = mysql_error_statement_digest_parse,
            .sqlstate = "HY000",
            .message_part = "Could not parse argument to digest function",
        }
    );
    failures += execute_error(
        database,
        "SELECT STATEMENT_DIGEST_TEXT('select ?')",
        (struct expected_sql_error){
            .code = mysql_error_statement_digest_parse,
            .sqlstate = "HY000",
            .message_part = "Could not parse argument to digest function",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_statement_digest_text_metadata(void) {
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = setup_database(&database);

    failures += execute_ok(database, "SELECT STATEMENT_DIGEST_TEXT('select 1') AS digest", &result);
    if (result != NULL) {
        failures += expect_column_metadata(
            result,
            0U,
            (struct expected_column_metadata){
                .type = MYLITE_RESULT_COLUMN_TYPE_BLOB,
                .flags = 0U,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length = mysql_statement_digest_text_utf8mb4_display_length,
                .decimals = mysql_approximate_decimals,
                .nullable = 1,
                .context = "scalar metadata",
            }
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "CREATE TABLE digest_metadata_probe(id INT)", NULL);
    failures += execute_ok(database, "INSERT INTO digest_metadata_probe VALUES (1)", NULL);
    failures += execute_ok(
        database,
        "SELECT STATEMENT_DIGEST_TEXT('select 1') AS digest FROM digest_metadata_probe",
        &result
    );
    if (result != NULL) {
        failures += expect_column_metadata(
            result,
            0U,
            (struct expected_column_metadata){
                .type = MYLITE_RESULT_COLUMN_TYPE_BLOB,
                .flags = 0U,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length = mysql_statement_digest_text_utf8mb4_display_length,
                .decimals = mysql_approximate_decimals,
                .nullable = 1,
                .context = "row metadata",
            }
        );
    }
    mylite_result_free(result);

    mylite_close(database);
    return failures;
}

static int setup_database(mylite_db **out_database) {
    int failures = 0;

    failures += expect_int(mylite_test_open_temporary(out_database), MYLITE_OK, "open database");
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "SET NAMES utf8mb4", NULL);
    }
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *local_result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &local_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got rc=%d sqlstate=%s message=%s\n",
            sql,
            rc,
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    if (out_result != NULL) {
        *out_result = local_result;
    } else {
        mylite_result_free(local_result);
    }
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    mylite_result_free(result);
    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
}

static int expect_result_shape(
    const mylite_result *result,
    size_t column_count,
    size_t row_count,
    const char *context
) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), column_count, context);
    failures += expect_size(mylite_result_row_count(result), row_count, context);
    return failures;
}

static int expect_column_metadata(
    const mylite_result *result,
    size_t column,
    struct expected_column_metadata expected
) {
    int failures = 0;

    failures += expect_int(
        (int)mylite_result_column_type(result, column),
        (int)expected.type,
        expected.context
    );
    failures +=
        expect_uint32(mylite_result_column_flags(result, column), expected.flags, expected.context);
    failures += expect_uint32(
        mylite_result_column_charset_id(result, column),
        expected.charset_id,
        expected.context
    );
    failures += expect_uint32(
        mylite_result_column_collation_id(result, column),
        expected.collation_id,
        expected.context
    );
    failures += expect_uint64(
        mylite_result_column_display_length(result, column),
        expected.display_length,
        expected.context
    );
    failures += expect_uint16(
        mylite_result_column_decimals(result, column),
        expected.decimals,
        expected.context
    );
    failures += expect_int(
        mylite_result_column_nullable(result, column),
        expected.nullable,
        expected.context
    );
    return failures;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_uint16(uint16_t actual, uint16_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %u, got %u\n", context, (unsigned)expected, (unsigned)actual);
    return 1;
}

static int expect_uint32(uint32_t actual, uint32_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %u, got %u\n", context, expected, actual);
    return 1;
}

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %llu, got %llu\n",
        context,
        (unsigned long long)expected,
        (unsigned long long)actual
    );
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if ((actual == NULL && expected == NULL) ||
        (actual != NULL && expected != NULL && strcmp(actual, expected) == 0)) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected text to contain [%s], got [%s]\n",
        context,
        needle == NULL ? "NULL" : needle,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}
