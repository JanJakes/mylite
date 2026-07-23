#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_collation_utf8mb4_0900_ai_ci_id = 255,
    mysql_error_native_function_arity = 1582,
    mysql_roles_graphml_utf8mb4_display_length = 201326592,
    mysql_approximate_decimals = 31,
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

static int test_scalar_and_dual_roles_graphml(void);
static int test_do_and_table_backed_roles_graphml(void);
static int test_roles_graphml_diagnostics(void);
static int test_roles_graphml_metadata(void);
static int setup_database(mylite_db **out_database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_graph_value(const char *value, const char *context);
static int expect_column_metadata(
    const mylite_result *result,
    size_t column,
    struct expected_column_metadata expected
);

int main(void) {
    int failures = 0;

    failures += test_scalar_and_dual_roles_graphml();
    failures += test_do_and_table_backed_roles_graphml();
    failures += test_roles_graphml_diagnostics();
    failures += test_roles_graphml_metadata();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_and_dual_roles_graphml(void) {
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = setup_database(&database);

    failures += execute_ok(database, "SELECT ROLES_GRAPHML() AS graph", &result);
    if (result != NULL) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 1U, "scalar columns");
        failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, "scalar rows");
        failures += mylite_test_expect_text(
            mylite_result_column_name(result, 0U),
            "graph",
            "scalar column"
        );
        failures += expect_graph_value(mylite_result_value_text(result, 0U, 0U), "scalar graph");
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT ROLES_GRAPHML() AS graph FROM DUAL", &result);
    if (result != NULL) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 1U, "dual columns");
        failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, "dual rows");
        failures += expect_graph_value(mylite_result_value_text(result, 0U, 0U), "dual graph");
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT CHARSET(ROLES_GRAPHML()) AS cs, COLLATION(ROLES_GRAPHML()) AS co, "
        "COERCIBILITY(ROLES_GRAPHML()) AS coercibility",
        &result
    );
    if (result != NULL) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 3U, "charset columns");
        failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, "charset rows");
        failures +=
            mylite_test_expect_text(mylite_result_value_text(result, 0U, 0U), "utf8mb3", "charset");
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, 0U, 1U),
            "utf8mb3_general_ci",
            "collation"
        );
        failures +=
            mylite_test_expect_text(mylite_result_value_text(result, 0U, 2U), "3", "coercibility");
    }
    mylite_result_free(result);

    mylite_close(database);
    return failures;
}

static int test_do_and_table_backed_roles_graphml(void) {
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = setup_database(&database);

    failures += execute_ok(database, "DO ROLES_GRAPHML()", &result);
    if (result != NULL) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, "do columns");
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "do rows");
        failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, "do warnings");
        failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, "do affected");
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "CREATE TABLE roles_probe(id INT)", NULL);
    failures += execute_ok(database, "INSERT INTO roles_probe VALUES (1),(2)", NULL);
    failures += execute_ok(
        database,
        "SELECT id, ROLES_GRAPHML() AS graph FROM roles_probe ORDER BY id",
        &result
    );
    if (result != NULL) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 2U, "row columns");
        failures += mylite_test_expect_size(mylite_result_row_count(result), 2U, "row rows");
        failures +=
            mylite_test_expect_text(mylite_result_value_text(result, 0U, 0U), "1", "row id 1");
        failures +=
            mylite_test_expect_text(mylite_result_value_text(result, 1U, 0U), "2", "row id 2");
        failures += expect_graph_value(mylite_result_value_text(result, 0U, 1U), "row graph 1");
        failures += expect_graph_value(mylite_result_value_text(result, 1U, 1U), "row graph 2");
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT id, CHARSET(ROLES_GRAPHML()) AS cs, COLLATION(ROLES_GRAPHML()) AS co, "
        "COERCIBILITY(ROLES_GRAPHML()) AS coercibility FROM roles_probe ORDER BY id",
        &result
    );
    if (result != NULL) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 4U, "row charset columns");
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 2U, "row charset rows");
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, 0U, 0U),
            "1",
            "row charset id 1"
        );
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, 0U, 1U),
            "utf8mb3",
            "row charset"
        );
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, 0U, 2U),
            "utf8mb3_general_ci",
            "row collation"
        );
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, 0U, 3U),
            "3",
            "row coercibility"
        );
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, 1U, 0U),
            "2",
            "row charset id 2"
        );
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, 1U, 1U),
            "utf8mb3",
            "row charset"
        );
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, 1U, 2U),
            "utf8mb3_general_ci",
            "row collation"
        );
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, 1U, 3U),
            "3",
            "row coercibility"
        );
    }
    mylite_result_free(result);

    mylite_close(database);
    return failures;
}

static int test_roles_graphml_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = setup_database(&database);

    failures += execute_error(
        database,
        "SELECT ROLES_GRAPHML(NULL)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'ROLES_GRAPHML'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ROLES_GRAPHML('a','b')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'ROLES_GRAPHML'",
        }
    );
    failures += execute_error(
        database,
        "DO ROLES_GRAPHML(1)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'ROLES_GRAPHML'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CHARSET(ROLES_GRAPHML(NULL))",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'ROLES_GRAPHML'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COERCIBILITY(ROLES_GRAPHML(1))",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'ROLES_GRAPHML'",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_roles_graphml_metadata(void) {
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = setup_database(&database);

    failures += execute_ok(database, "SELECT ROLES_GRAPHML() AS graph", &result);
    if (result != NULL) {
        failures += expect_column_metadata(
            result,
            0U,
            (struct expected_column_metadata){
                .type = MYLITE_RESULT_COLUMN_TYPE_BLOB,
                .flags = 0U,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length = mysql_roles_graphml_utf8mb4_display_length,
                .decimals = mysql_approximate_decimals,
                .nullable = 1,
                .context = "scalar metadata",
            }
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "CREATE TABLE roles_probe(id INT)", NULL);
    failures += execute_ok(database, "INSERT INTO roles_probe VALUES (1)", NULL);
    failures += execute_ok(database, "SELECT ROLES_GRAPHML() AS graph FROM roles_probe", &result);
    if (result != NULL) {
        failures += expect_column_metadata(
            result,
            0U,
            (struct expected_column_metadata){
                .type = MYLITE_RESULT_COLUMN_TYPE_BLOB,
                .flags = 0U,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length = mysql_roles_graphml_utf8mb4_display_length,
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

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(out_database),
        MYLITE_OK,
        "open database"
    );
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
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
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
}

static int expect_graph_value(const char *value, const char *context) {
    int failures = 0;

    failures +=
        mylite_test_expect_contains(value, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>", context);
    failures += mylite_test_expect_contains(
        value,
        "<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns\"",
        context
    );
    failures +=
        mylite_test_expect_contains(value, "<graph id=\"G\" edgedefault=\"directed\"", context);
    failures += mylite_test_expect_contains(value, "<data key=\"key1\">`root`@`%`</data>", context);
    failures += mylite_test_expect_contains(value, "</graphml>", context);
    return failures;
}

static int expect_column_metadata(
    const mylite_result *result,
    size_t column,
    struct expected_column_metadata expected
) {
    int failures = 0;

    failures += mylite_test_expect_int(
        (int)mylite_result_column_type(result, column),
        (int)expected.type,
        expected.context
    );
    failures += mylite_test_expect_uint32(
        mylite_result_column_flags(result, column),
        expected.flags,
        expected.context
    );
    failures += mylite_test_expect_uint32(
        mylite_result_column_charset_id(result, column),
        expected.charset_id,
        expected.context
    );
    failures += mylite_test_expect_uint32(
        mylite_result_column_collation_id(result, column),
        expected.collation_id,
        expected.context
    );
    failures += mylite_test_expect_uint64(
        mylite_result_column_display_length(result, column),
        expected.display_length,
        expected.context
    );
    failures += mylite_test_expect_uint16(
        mylite_result_column_decimals(result, column),
        expected.decimals,
        expected.context
    );
    failures += mylite_test_expect_int(
        mylite_result_column_nullable(result, column),
        expected.nullable,
        expected.context
    );
    return failures;
}
