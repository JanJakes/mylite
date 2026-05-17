#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
    row_count_text_capacity = 32,
    schema_variable_column_count = 5,
    mysql_error_no_database_selected = 1046,
    mysql_error_database_access_denied = 1044,
    mysql_error_parse = 1064,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_unknown_character_set = 1115,
    mysql_error_collation_not_valid_for_character_set = 1253,
    mysql_error_unknown_collation = 1273,
    mysql_error_conflicting_declarations = 1302,
    mysql_error_database_does_not_exist = 3503,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_single_row_result {
    const char *const *columns;
    const char *const *values;
    size_t column_count;
    const char *context;
};

static int test_schema_default_create_alter_metadata_and_persistence(void);
static int test_binary_defaults_and_independent_handles(void);
static int test_schema_default_diagnostics(void);
static int expect_single_row_result(
    mylite_db *database,
    const char *sql,
    struct expected_single_row_result expected
);
static int expect_result_contains(
    mylite_db *database,
    const char *sql,
    size_t row,
    size_t column,
    const char *needle,
    const char *context
);
static int expect_result_not_contains(
    mylite_db *database,
    const char *sql,
    size_t row,
    size_t column,
    const char *needle,
    const char *context
);
static int expect_row_count(mylite_db *database, int64_t expected, const char *context);
static int expect_statement_warning_count(
    mylite_db *database,
    const char *sql,
    size_t expected_warning_count,
    const char *context
);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);
static int expect_text_not_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_schema_default_create_alter_metadata_and_persistence();
    failures += test_binary_defaults_and_independent_handles();
    failures += test_schema_default_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_schema_default_create_alter_metadata_and_persistence(void) {
    static const char *const variable_columns[] = {
        "DATABASE()",
        "@@character_set_database",
        "@@collation_database",
        "@@global.character_set_database",
        "@@global.collation_database",
    };
    static const char *const unicode_variable_values[] = {
        "app",
        "utf8mb4",
        "utf8mb4_unicode_ci",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const altered_variable_values[] = {
        "app",
        "utf8mb4",
        "utf8mb4_unicode_520_ci",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const schemata_columns[] = {
        "DEFAULT_CHARACTER_SET_NAME",
        "DEFAULT_COLLATION_NAME",
    };
    static const char *const schemata_values[] = {"utf8mb4", "utf8mb4_unicode_ci"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "create_alter") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open create alter file");
    failures += execute_statement_ok(database, "CREATE DATABASE app COLLATE utf8mb4_unicode_ci");
    failures += expect_result_contains(
        database,
        "SHOW CREATE DATABASE app",
        0U,
        1U,
        "DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci",
        "show create unicode schema"
    );
    failures += execute_statement_ok(database, "USE app");
    failures += expect_single_row_result(
        database,
        "SELECT DATABASE(), @@character_set_database, @@collation_database, "
        "@@global.character_set_database, @@global.collation_database",
        (struct expected_single_row_result){
            .columns = variable_columns,
            .values = unicode_variable_values,
            .column_count = schema_variable_column_count,
            .context = "selected schema variables",
        }
    );
    failures += expect_single_row_result(
        database,
        "SELECT DEFAULT_CHARACTER_SET_NAME, DEFAULT_COLLATION_NAME "
        "FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME = 'app'",
        (struct expected_single_row_result){
            .columns = schemata_columns,
            .values = schemata_values,
            .column_count = 2U,
            .context = "information schema schemata defaults",
        }
    );

    failures += execute_statement_ok(database, "CREATE TABLE inherited (v VARCHAR(10))");
    failures += expect_result_contains(
        database,
        "SHOW CREATE TABLE inherited",
        0U,
        1U,
        "COLLATE utf8mb4_unicode_ci",
        "schema default inherited column collation"
    );
    failures += execute_statement_ok(database, "CREATE TABLE cloned LIKE inherited");
    failures += expect_result_contains(
        database,
        "SHOW CREATE TABLE cloned",
        0U,
        1U,
        "COLLATE utf8mb4_unicode_ci",
        "create table like cloned collation"
    );

    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;
    failures +=
        execute_statement_ok(database, "ALTER DATABASE app DEFAULT COLLATE utf8mb4_0900_bin");
    failures += expect_row_count(database, 1, "row count after named alter database");
    failures +=
        execute_statement_ok(database, "ALTER SCHEMA DEFAULT COLLATE utf8mb4_unicode_520_ci");
    failures += expect_single_row_result(
        database,
        "SELECT DATABASE(), @@character_set_database, @@collation_database, "
        "@@global.character_set_database, @@global.collation_database",
        (struct expected_single_row_result){
            .columns = variable_columns,
            .values = altered_variable_values,
            .column_count = schema_variable_column_count,
            .context = "altered selected schema variables",
        }
    );
    session = mylite_connection_session_state(database);
    failures += expect_uint64(
        session->sqlite_schema_generation,
        sqlite_schema_generation,
        "sqlite schema generation after schema default alter"
    );
    failures += expect_uint64(
        session->catalog_generation > catalog_generation ? 1U : 0U,
        1U,
        "catalog generation increases after schema default alter"
    );
    failures += execute_statement_ok(database, "CREATE TABLE after_alter (v VARCHAR(10))");
    failures += expect_result_contains(
        database,
        "SHOW CREATE TABLE app.after_alter",
        0U,
        1U,
        "COLLATE utf8mb4_unicode_520_ci",
        "create table after alter inherits new schema default"
    );
    failures +=
        execute_statement_ok(database, "CREATE TABLE ctas_after_alter AS SELECT v FROM inherited");
    failures += expect_result_contains(
        database,
        "SHOW CREATE TABLE app.ctas_after_alter",
        0U,
        1U,
        "ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci",
        "create table select inherits target schema default"
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after schema default lifecycle"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen create alter file");
    failures += expect_result_contains(
        database,
        "SHOW CREATE DATABASE app",
        0U,
        1U,
        "DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci",
        "reopened show create altered schema"
    );
    failures += expect_result_contains(
        database,
        "SHOW CREATE TABLE app.after_alter",
        0U,
        1U,
        "COLLATE utf8mb4_unicode_520_ci",
        "reopened inherited table collation"
    );
    failures += expect_result_contains(
        database,
        "SHOW CREATE TABLE app.ctas_after_alter",
        0U,
        1U,
        "ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci",
        "reopened create table select target schema default"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_binary_defaults_and_independent_handles(void) {
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += execute_statement_ok(first, "CREATE DATABASE app DEFAULT CHARSET=binary");
    failures += execute_statement_ok(second, "CREATE DATABASE app DEFAULT CHARSET=ascii");

    failures += expect_result_contains(
        first,
        "SHOW CREATE DATABASE app",
        0U,
        1U,
        "DEFAULT CHARACTER SET binary",
        "binary schema show create"
    );
    failures += expect_result_not_contains(
        first,
        "SHOW CREATE DATABASE app",
        0U,
        1U,
        "COLLATE binary",
        "binary schema show create omits collate"
    );
    failures += expect_result_contains(
        second,
        "SHOW CREATE DATABASE app",
        0U,
        1U,
        "DEFAULT CHARACTER SET ascii",
        "ascii schema show create"
    );

    failures += execute_statement_ok(first, "USE app");
    failures += execute_statement_ok(first, "CREATE TABLE binary_inherited (v VARCHAR(10))");
    failures += expect_result_contains(
        first,
        "SHOW CREATE TABLE binary_inherited",
        0U,
        1U,
        "varbinary(10)",
        "binary schema varchar inheritance"
    );
    failures += expect_result_contains(
        first,
        "SHOW CREATE TABLE binary_inherited",
        0U,
        1U,
        "DEFAULT CHARSET=binary",
        "binary schema table default"
    );
    failures += execute_statement_ok(second, "USE app");
    failures += execute_statement_ok(second, "CREATE TABLE ascii_inherited (v VARCHAR(10))");
    failures += expect_result_contains(
        second,
        "SHOW CREATE TABLE ascii_inherited",
        0U,
        1U,
        "DEFAULT CHARSET=ascii",
        "ascii schema table default"
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int test_schema_default_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += execute_statement_ok(database, "CREATE DATABASE app");

    failures += execute_error(
        database,
        "CREATE DATABASE bad CHARACTER SET nosuch_charset",
        (struct expected_sql_error){
            .code = mysql_error_unknown_character_set,
            .sqlstate = "42000",
            .message_part = "Unknown character set: 'nosuch_charset'",
        }
    );
    failures += execute_error(
        database,
        "CREATE DATABASE bad COLLATE nosuch_collation",
        (struct expected_sql_error){
            .code = mysql_error_unknown_collation,
            .sqlstate = "HY000",
            .message_part = "Unknown collation: 'nosuch_collation'",
        }
    );
    failures += execute_error(
        database,
        "CREATE DATABASE bad CHARACTER SET binary COLLATE utf8mb4_bin",
        (struct expected_sql_error){
            .code = mysql_error_collation_not_valid_for_character_set,
            .sqlstate = "42000",
            .message_part = "not valid for CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "CREATE DATABASE bad CHARACTER SET ascii COLLATE utf8mb4_bin",
        (struct expected_sql_error){
            .code = mysql_error_collation_not_valid_for_character_set,
            .sqlstate = "42000",
            .message_part = "not valid for CHARACTER SET 'ascii'",
        }
    );
    failures += execute_error(
        database,
        "CREATE DATABASE bad CHARACTER SET utf8mb4 COLLATE ascii_bin",
        (struct expected_sql_error){
            .code = mysql_error_collation_not_valid_for_character_set,
            .sqlstate = "42000",
            .message_part = "not valid for CHARACTER SET 'utf8mb4'",
        }
    );
    failures += execute_error(
        database,
        "CREATE DATABASE bad CHARACTER SET utf8mb4 CHARACTER SET binary",
        (struct expected_sql_error){
            .code = mysql_error_conflicting_declarations,
            .sqlstate = "HY000",
            .message_part = "Conflicting declarations",
        }
    );
    failures += execute_error(
        database,
        "CREATE DATABASE IF NOT EXISTS app CHARACTER SET nosuch_charset",
        (struct expected_sql_error){
            .code = mysql_error_unknown_character_set,
            .sqlstate = "42000",
            .message_part = "Unknown character set: 'nosuch_charset'",
        }
    );
    failures += execute_error(
        database,
        "CREATE DATABASE IF NOT EXISTS app CHARACTER SET utf8mb4 CHARACTER SET binary",
        (struct expected_sql_error){
            .code = mysql_error_conflicting_declarations,
            .sqlstate = "HY000",
            .message_part = "Conflicting declarations",
        }
    );
    failures += expect_statement_warning_count(
        database,
        "CREATE DATABASE IF NOT EXISTS app COLLATE utf8mb4_bin",
        1U,
        "if not exists no-op warning"
    );
    failures += expect_result_contains(
        database,
        "SHOW CREATE DATABASE app",
        0U,
        1U,
        "DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci",
        "if not exists preserves existing defaults"
    );
    failures += execute_error(
        database,
        "ALTER DATABASE DEFAULT COLLATE utf8mb4_bin",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "ALTER DATABASE missing DEFAULT CHARACTER SET utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_database_does_not_exist,
            .sqlstate = "42Y07",
            .message_part = "Database 'missing' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER DATABASE information_schema DEFAULT COLLATE utf8mb4_bin",
        (struct expected_sql_error){
            .code = mysql_error_database_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied",
        }
    );
    failures += execute_error(
        database,
        "ALTER DATABASE _mylite_catalog DEFAULT COLLATE utf8mb4_bin",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_catalog'",
        }
    );
    failures += execute_error(
        database,
        "ALTER DATABASE app DEFAULT CHARACTER SET DEFAULT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER DATABASE app ENGINE=InnoDB",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int expect_single_row_result(
    mylite_db *database,
    const char *sql,
    struct expected_single_row_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    for (size_t index = 0U; index < expected.column_count; ++index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, index),
            expected.columns[index],
            expected.context
        );
    }
    failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
    for (size_t index = 0U; index < expected.column_count; ++index) {
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, index),
            expected.values[index],
            expected.context
        );
    }
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    mylite_result_free(result);
    return failures;
}

static int expect_result_contains(
    mylite_db *database,
    const char *sql,
    size_t row,
    size_t column,
    const char *needle,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures +=
        expect_text_contains(mylite_result_value_text(result, row, column), needle, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    mylite_result_free(result);
    return failures;
}

static int expect_result_not_contains(
    mylite_db *database,
    const char *sql,
    size_t row,
    size_t column,
    const char *needle,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures +=
        expect_text_not_contains(mylite_result_value_text(result, row, column), needle, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    mylite_result_free(result);
    return failures;
}

static int expect_row_count(mylite_db *database, int64_t expected, const char *context) {
    char expected_text[row_count_text_capacity];
    static const char *const columns[] = {"ROW_COUNT()"};
    const char *values[] = {expected_text};
    int written = snprintf(expected_text, sizeof(expected_text), "%" PRId64, expected);

    if (written < 0 || (size_t)written >= sizeof(expected_text)) {
        fprintf(stderr, "failed to format row count expectation for %s\n", context);
        return 1;
    }
    return expect_single_row_result(
        database,
        "SELECT ROW_COUNT()",
        (struct expected_single_row_result){
            .columns = columns,
            .values = values,
            .column_count = 1U,
            .context = context,
        }
    );
}

static int expect_statement_warning_count(
    mylite_db *database,
    const char *sql,
    size_t expected_warning_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, context);
    failures += expect_size(mylite_result_row_count(result), 0U, context);
    failures += expect_size(mylite_result_warning_count(result), expected_warning_count, context);
    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "expected success for %s, got rc=%d code=%d state=%s message=%s\n",
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

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "expected error for %s, got rc=%d\n", sql, rc);
        mylite_result_free(result);
        return 1;
    }

    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_schema_defaults_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
        return 1;
    }

    return 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    int failures = 0;

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        failures += 1;
    } else if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        failures += 1;
    }
    if (fclose(file) != 0) {
        failures += 1;
    }

    return failures;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %" PRIu64 ", got %" PRIu64 "\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected text %s, got %s\n",
        context,
        expected == NULL ? "(null)" : expected,
        actual == NULL ? "(null)" : actual
    );
    return 1;
}

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected text containing %s, got %s\n",
        context,
        needle == NULL ? "(null)" : needle,
        actual == NULL ? "(null)" : actual
    );
    return 1;
}

static int expect_text_not_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        return 0;
    }
    fprintf(stderr, "%s: expected text not containing %s, got %s\n", context, needle, actual);
    return 1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }

    return 0;
}
