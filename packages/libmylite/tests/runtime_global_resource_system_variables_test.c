#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    scalar_column_count = 38,
    show_variable_column_count = 2,
    show_variable_row_count = 19,
    global_noop_column_count = 15,
    warning_column_count = 3,
    diagnostic_context_capacity = 512,
    mysql_error_parse = 1064,
    mysql_error_global_variable_scope = 1229,
    mysql_error_session_variable_only = 1238,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_global_resource_values_and_show_rows(void);
static int test_global_resource_set_and_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_with_warnings(
    mylite_db *database,
    const char *sql,
    size_t warning_count
);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

static const char *const global_resource_variables[] = {
    "ngram_token_size",
    "offline_mode",
    "persist_only_admin_x509_subject",
    "persist_sensitive_variables_in_plaintext",
    "persisted_globals_load",
    "protocol_compression_algorithms",
    "schema_definition_cache",
    "stored_program_cache",
    "stored_program_definition_cache",
    "sync_binlog",
    "table_definition_cache",
    "table_encryption_privilege_check",
    "table_open_cache",
    "table_open_cache_instances",
    "tablespace_definition_cache",
    "temptable_max_mmap",
    "temptable_use_mmap",
    "thread_cache_size",
    "thread_stack",
};

int main(void) {
    int failures = 0;

    failures += test_global_resource_values_and_show_rows();
    failures += test_global_resource_set_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_global_resource_values_and_show_rows(void) {
    static const char *const scalar_values[] = {
        "2",
        "2",
        "0",
        "0",
        "",
        "",
        "1",
        "1",
        "1",
        "1",
        "zlib,zstd,uncompressed",
        "zlib,zstd,uncompressed",
        "256",
        "256",
        "256",
        "256",
        "256",
        "256",
        "1",
        "1",
        "2000",
        "2000",
        "0",
        "0",
        "4000",
        "4000",
        "16",
        "16",
        "256",
        "256",
        "0",
        "0",
        "0",
        "0",
        "9",
        "9",
        "1048576",
        "1048576",
    };
    static const char *const show_rows[] = {
        "ngram_token_size",
        "2",
        "offline_mode",
        "OFF",
        "persist_only_admin_x509_subject",
        "",
        "persist_sensitive_variables_in_plaintext",
        "ON",
        "persisted_globals_load",
        "ON",
        "protocol_compression_algorithms",
        "zlib,zstd,uncompressed",
        "schema_definition_cache",
        "256",
        "stored_program_cache",
        "256",
        "stored_program_definition_cache",
        "256",
        "sync_binlog",
        "1",
        "table_definition_cache",
        "2000",
        "table_encryption_privilege_check",
        "OFF",
        "table_open_cache",
        "4000",
        "table_open_cache_instances",
        "16",
        "tablespace_definition_cache",
        "256",
        "temptable_max_mmap",
        "0",
        "temptable_use_mmap",
        "OFF",
        "thread_cache_size",
        "9",
        "thread_stack",
        "1048576",
    };
    struct expected_sql_error global_only_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable",
    };
    mylite_db *database = NULL;
    char sql[diagnostic_context_capacity];
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open resource db");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@ngram_token_size, @@GLOBAL.ngram_token_size, "
                   "@@offline_mode, @@GLOBAL.offline_mode, "
                   "@@persist_only_admin_x509_subject, "
                   "@@GLOBAL.persist_only_admin_x509_subject, "
                   "@@persist_sensitive_variables_in_plaintext, "
                   "@@GLOBAL.persist_sensitive_variables_in_plaintext, "
                   "@@persisted_globals_load, @@GLOBAL.persisted_globals_load, "
                   "@@protocol_compression_algorithms, "
                   "@@GLOBAL.protocol_compression_algorithms, "
                   "@@schema_definition_cache, @@GLOBAL.schema_definition_cache, "
                   "@@stored_program_cache, @@GLOBAL.stored_program_cache, "
                   "@@stored_program_definition_cache, "
                   "@@GLOBAL.stored_program_definition_cache, "
                   "@@sync_binlog, @@GLOBAL.sync_binlog, "
                   "@@table_definition_cache, @@GLOBAL.table_definition_cache, "
                   "@@table_encryption_privilege_check, "
                   "@@GLOBAL.table_encryption_privilege_check, "
                   "@@table_open_cache, @@GLOBAL.table_open_cache, "
                   "@@table_open_cache_instances, @@GLOBAL.table_open_cache_instances, "
                   "@@tablespace_definition_cache, @@GLOBAL.tablespace_definition_cache, "
                   "@@temptable_max_mmap, @@GLOBAL.temptable_max_mmap, "
                   "@@temptable_use_mmap, @@GLOBAL.temptable_use_mmap, "
                   "@@thread_cache_size, @@GLOBAL.thread_cache_size, "
                   "@@thread_stack, @@GLOBAL.thread_stack",
            .values = scalar_values,
            .column_count = scalar_column_count,
            .row_count = 1U,
            .context = "global resource scalar values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW VARIABLES WHERE Variable_name IN "
                   "('ngram_token_size','offline_mode',"
                   "'persist_only_admin_x509_subject',"
                   "'persist_sensitive_variables_in_plaintext','persisted_globals_load',"
                   "'protocol_compression_algorithms','schema_definition_cache',"
                   "'stored_program_cache','stored_program_definition_cache','sync_binlog',"
                   "'table_definition_cache','table_encryption_privilege_check',"
                   "'table_open_cache','table_open_cache_instances',"
                   "'tablespace_definition_cache','temptable_max_mmap',"
                   "'temptable_use_mmap','thread_cache_size','thread_stack')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "global resource SHOW VARIABLES rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW GLOBAL VARIABLES WHERE Variable_name IN "
                   "('ngram_token_size','offline_mode',"
                   "'persist_only_admin_x509_subject',"
                   "'persist_sensitive_variables_in_plaintext','persisted_globals_load',"
                   "'protocol_compression_algorithms','schema_definition_cache',"
                   "'stored_program_cache','stored_program_definition_cache','sync_binlog',"
                   "'table_definition_cache','table_encryption_privilege_check',"
                   "'table_open_cache','table_open_cache_instances',"
                   "'tablespace_definition_cache','temptable_max_mmap',"
                   "'temptable_use_mmap','thread_cache_size','thread_stack')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "global resource SHOW GLOBAL VARIABLES rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW SESSION VARIABLES WHERE Variable_name IN "
                   "('ngram_token_size','offline_mode',"
                   "'persist_only_admin_x509_subject',"
                   "'persist_sensitive_variables_in_plaintext','persisted_globals_load',"
                   "'protocol_compression_algorithms','schema_definition_cache',"
                   "'stored_program_cache','stored_program_definition_cache','sync_binlog',"
                   "'table_definition_cache','table_encryption_privilege_check',"
                   "'table_open_cache','table_open_cache_instances',"
                   "'tablespace_definition_cache','temptable_max_mmap',"
                   "'temptable_use_mmap','thread_cache_size','thread_stack')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "global resource SHOW SESSION VARIABLES rows",
        }
    );

    for (size_t index = 0U;
         index < sizeof(global_resource_variables) / sizeof(global_resource_variables[0]);
         ++index) {
        int written =
            snprintf(sql, sizeof(sql), "SELECT @@SESSION.%s", global_resource_variables[index]);

        if (written < 0 || (size_t)written >= sizeof(sql)) {
            failures += 1;
        } else {
            failures += execute_error(database, sql, global_only_read);
        }
        written = snprintf(sql, sizeof(sql), "SELECT @@LOCAL.%s", global_resource_variables[index]);
        if (written < 0 || (size_t)written >= sizeof(sql)) {
            failures += 1;
        } else {
            failures += execute_error(database, sql, global_only_read);
        }
    }

    mylite_close(database);
    return failures;
}

static int test_global_resource_set_and_diagnostics(void) {
    static const char *const global_noop_values[] = {
        "0",
        "zlib,zstd,uncompressed",
        "256",
        "256",
        "256",
        "1",
        "2000",
        "0",
        "4000",
        "256",
        "0",
        "9",
        "0",
        "0",
        "0",
    };
    static const char *const read_only_assignments[] = {
        "SET GLOBAL ngram_token_size = DEFAULT",
        "SET GLOBAL persist_only_admin_x509_subject = DEFAULT",
        "SET GLOBAL persist_sensitive_variables_in_plaintext = DEFAULT",
        "SET GLOBAL persisted_globals_load = DEFAULT",
        "SET GLOBAL table_open_cache_instances = DEFAULT",
        "SET GLOBAL thread_stack = DEFAULT",
    };
    static const char *const session_global_only_assignments[] = {
        "SET offline_mode = DEFAULT",
        "SET protocol_compression_algorithms = DEFAULT",
        "SET schema_definition_cache = DEFAULT",
        "SET stored_program_cache = DEFAULT",
        "SET stored_program_definition_cache = DEFAULT",
        "SET sync_binlog = DEFAULT",
        "SET table_definition_cache = DEFAULT",
        "SET table_encryption_privilege_check = DEFAULT",
        "SET table_open_cache = DEFAULT",
        "SET tablespace_definition_cache = DEFAULT",
        "SET temptable_max_mmap = DEFAULT",
        "SET temptable_use_mmap = DEFAULT",
        "SET thread_cache_size = DEFAULT",
    };
    struct expected_sql_error global_only_set = {
        .code = mysql_error_global_variable_scope,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable and should be set with SET GLOBAL",
    };
    struct expected_sql_error read_only = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a read only variable",
    };
    struct expected_sql_error unsupported_global_set = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "fixed no-op",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open resource SET db");

    failures += execute_statement_ok(database, "SET GLOBAL offline_mode = OFF");
    failures += execute_statement_ok(database, "SET GLOBAL offline_mode = DEFAULT");
    failures +=
        execute_statement_ok(database, "SET GLOBAL protocol_compression_algorithms = DEFAULT");
    failures += execute_statement_ok(
        database,
        "SET GLOBAL protocol_compression_algorithms = 'zlib,zstd,uncompressed'"
    );
    failures += execute_statement_ok(database, "SET GLOBAL schema_definition_cache = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL schema_definition_cache = 256");
    failures += execute_statement_ok(database, "SET GLOBAL stored_program_cache = DEFAULT");
    failures +=
        execute_statement_ok(database, "SET GLOBAL stored_program_definition_cache = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL sync_binlog = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL table_definition_cache = DEFAULT");
    failures +=
        execute_statement_ok(database, "SET GLOBAL table_encryption_privilege_check = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL table_encryption_privilege_check = OFF");
    failures += execute_statement_ok(database, "SET GLOBAL table_open_cache = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL tablespace_definition_cache = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL temptable_max_mmap = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL thread_cache_size = DEFAULT");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@GLOBAL.offline_mode, @@GLOBAL.protocol_compression_algorithms, "
                   "@@GLOBAL.schema_definition_cache, @@GLOBAL.stored_program_cache, "
                   "@@GLOBAL.stored_program_definition_cache, @@GLOBAL.sync_binlog, "
                   "@@GLOBAL.table_definition_cache, @@GLOBAL.table_encryption_privilege_check, "
                   "@@GLOBAL.table_open_cache, @@GLOBAL.tablespace_definition_cache, "
                   "@@GLOBAL.temptable_max_mmap, @@GLOBAL.thread_cache_size, "
                   "@@warning_count, @@error_count, ROW_COUNT()",
            .values = global_noop_values,
            .column_count = global_noop_column_count,
            .row_count = 1U,
            .context = "global resource no-op SET readback",
        }
    );

    failures +=
        execute_statement_with_warnings(database, "SET GLOBAL temptable_use_mmap = DEFAULT", 1U);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = (const char *[]){"Warning",
                                       "1287",
                                       "'temptable_use_mmap' is deprecated and will be removed in "
                                       "a future release."},
            .column_count = warning_column_count,
            .row_count = 1U,
            .context = "temptable_use_mmap SET warning",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){.sql = "SELECT @@GLOBAL.temptable_use_mmap, @@warning_count",
                                .values = (const char *[]){"0", "1"},
                                .column_count = 2U,
                                .row_count = 1U,
                                .context = "temptable_use_mmap warning count"}
    );

    for (size_t index = 0U;
         index < sizeof(read_only_assignments) / sizeof(read_only_assignments[0]);
         ++index) {
        failures += execute_error(database, read_only_assignments[index], read_only);
    }
    for (size_t index = 0U; index < sizeof(session_global_only_assignments) /
                                        sizeof(session_global_only_assignments[0]);
         ++index) {
        failures +=
            execute_error(database, session_global_only_assignments[index], global_only_set);
    }

    failures += execute_error(database, "SET GLOBAL offline_mode = ON", unsupported_global_set);
    failures += execute_error(
        database,
        "SET GLOBAL protocol_compression_algorithms = 'uncompressed'",
        unsupported_global_set
    );
    failures +=
        execute_error(database, "SET GLOBAL schema_definition_cache = 257", unsupported_global_set);
    failures += execute_error(
        database,
        "SET GLOBAL table_encryption_privilege_check = ON",
        unsupported_global_set
    );
    failures +=
        execute_error(database, "SET GLOBAL temptable_use_mmap = ON", unsupported_global_set);

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }

    return 0;
}

static int execute_statement_with_warnings(
    mylite_db *database,
    const char *sql,
    size_t warning_count
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures != 0) {
        mylite_result_free(result);
        return failures;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += expect_int64(mylite_result_affected_rows(result), 0, "statement affected rows");
    failures +=
        expect_size(mylite_result_warning_count(result), warning_count, "statement warning count");
    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    return execute_statement_with_warnings(database, sql, 0U);
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    char context[diagnostic_context_capacity];
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    snprintf(context, sizeof(context), "%s error code", sql);
    failures += expect_int(mylite_errcode(database), expected.code, context);
    snprintf(context, sizeof(context), "%s SQLSTATE", sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, context);
    snprintf(context, sizeof(context), "%s error message", sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, context);
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (failures != 0) {
        mylite_result_free(result);
        return failures;
    }
    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    mylite_result_free(result);

    return failures;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    return expect_text(actual, expected, context);
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

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_true(int condition, const char *context) {
    if (condition) {
        return 0;
    }
    fprintf(stderr, "%s: expected true\n", context);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "(null)" : expected,
        actual == NULL ? "(null)" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "(null)" : actual,
        needle == NULL ? "(null)" : needle
    );
    return 1;
}
