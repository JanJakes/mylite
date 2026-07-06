// clang-format off
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <php.h>
#include <Zend/zend_smart_str.h>
#include <ext/pdo/php_pdo_driver.h>
#include <ext/pdo/php_pdo_error.h>
#include <ext/standard/info.h>
// clang-format on

#include <mylite/mylite.h>

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PHP_PDO_MYLITE_EXT_VERSION "0.1.0"

typedef struct pdo_mylite_db_handle {
    mylite_db *db;
    unsigned native_errno;
    zend_string *errmsg;
    zend_string *last_insert_id;
} pdo_mylite_db_handle;

typedef struct pdo_mylite_stmt {
    pdo_mylite_db_handle *handle;
    zend_string *sql;
    mylite_result *result;
    mylite_stmt *native_stmt;
    size_t native_column_count;
    size_t cursor;
    size_t current_row;
} pdo_mylite_stmt;

static int pdo_mylite_handle_factory(pdo_dbh_t *dbh, zval *driver_options);
static void pdo_mylite_handle_closer(pdo_dbh_t *dbh);
static bool pdo_mylite_handle_preparer(
    pdo_dbh_t *dbh,
    zend_string *sql,
    pdo_stmt_t *stmt,
    zval *driver_options
);
static zend_long pdo_mylite_handle_doer(pdo_dbh_t *dbh, const zend_string *sql);
static zend_string *pdo_mylite_handle_quoter(
    pdo_dbh_t *dbh,
    const zend_string *unquoted,
    enum pdo_param_type paramtype
);
static bool pdo_mylite_handle_begin(pdo_dbh_t *dbh);
static bool pdo_mylite_handle_commit(pdo_dbh_t *dbh);
static bool pdo_mylite_handle_rollback(pdo_dbh_t *dbh);
static zend_string *pdo_mylite_last_insert_id(pdo_dbh_t *dbh, const zend_string *name);
static void pdo_mylite_fetch_error(pdo_dbh_t *dbh, pdo_stmt_t *stmt, zval *info);
static int pdo_mylite_get_attribute(pdo_dbh_t *dbh, zend_long attr, zval *return_value);
static int pdo_mylite_stmt_dtor(pdo_stmt_t *stmt);
static int pdo_mylite_stmt_execute(pdo_stmt_t *stmt);
static int pdo_mylite_stmt_fetch(
    pdo_stmt_t *stmt,
    enum pdo_fetch_orientation orientation,
    zend_long offset
);
static int pdo_mylite_stmt_describe(pdo_stmt_t *stmt, int column);
static int pdo_mylite_stmt_get_col(
    pdo_stmt_t *stmt,
    int column,
    zval *result,
    enum pdo_param_type *type
);
static int pdo_mylite_stmt_cursor_closer(pdo_stmt_t *stmt);
static int pdo_mylite_error(pdo_dbh_t *dbh, pdo_stmt_t *stmt, int status, const char *fallback);
static void pdo_mylite_clear_error(pdo_dbh_t *dbh, pdo_stmt_t *stmt);
static void pdo_mylite_stmt_release_storage(pdo_mylite_stmt *statement_data);
static bool pdo_mylite_sql_may_stream_select(const char *sql, size_t length);
static bool pdo_mylite_sql_keyword_at(
    const char *sql,
    size_t length,
    size_t offset,
    const char *keyword
);
static bool pdo_mylite_is_identifier_byte(unsigned char ch);
static void pdo_mylite_update_status(pdo_dbh_t *dbh, const mylite_result *result);
static zend_string *pdo_mylite_quote_string(const char *value, size_t length);
static char *pdo_mylite_resolve_path(pdo_dbh_t *dbh);

static const struct pdo_dbh_methods pdo_mylite_dbh_methods = {
    pdo_mylite_handle_closer,
    pdo_mylite_handle_preparer,
    pdo_mylite_handle_doer,
    pdo_mylite_handle_quoter,
    pdo_mylite_handle_begin,
    pdo_mylite_handle_commit,
    pdo_mylite_handle_rollback,
    NULL,
    pdo_mylite_last_insert_id,
    pdo_mylite_fetch_error,
    pdo_mylite_get_attribute,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
#if PHP_VERSION_ID >= 80400
    ,
    NULL
#endif
};

static const struct pdo_stmt_methods pdo_mylite_stmt_methods = {
    pdo_mylite_stmt_dtor,
    pdo_mylite_stmt_execute,
    pdo_mylite_stmt_fetch,
    pdo_mylite_stmt_describe,
    pdo_mylite_stmt_get_col,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    pdo_mylite_stmt_cursor_closer
};

static const pdo_driver_t pdo_mylite_driver = {
    PDO_DRIVER_HEADER(mylite),
    pdo_mylite_handle_factory
};

static const zend_module_dep pdo_mylite_deps[] = {ZEND_MOD_REQUIRED("pdo")
                                                      ZEND_MOD_REQUIRED("mylite") ZEND_MOD_END};

PHP_MINIT_FUNCTION(pdo_mylite) {
    return php_pdo_register_driver(&pdo_mylite_driver);
}

PHP_MSHUTDOWN_FUNCTION(pdo_mylite) {
    php_pdo_unregister_driver(&pdo_mylite_driver);
    return SUCCESS;
}

PHP_MINFO_FUNCTION(pdo_mylite) {
    php_info_print_table_start();
    php_info_print_table_row(2, "pdo_mylite support", "enabled");
    php_info_print_table_row(2, "pdo_mylite version", PHP_PDO_MYLITE_EXT_VERSION);
    php_info_print_table_end();
}

zend_module_entry pdo_mylite_module_entry = {
    STANDARD_MODULE_HEADER_EX,
    NULL,
    pdo_mylite_deps,
    "pdo_mylite",
    NULL,
    PHP_MINIT(pdo_mylite),
    PHP_MSHUTDOWN(pdo_mylite),
    NULL,
    NULL,
    PHP_MINFO(pdo_mylite),
    PHP_PDO_MYLITE_EXT_VERSION,
    STANDARD_MODULE_PROPERTIES,
};

#ifdef COMPILE_DL_PDO_MYLITE
ZEND_GET_MODULE(pdo_mylite)
#endif

static int pdo_mylite_handle_factory(pdo_dbh_t *dbh, zval *driver_options) {
    (void)driver_options;
    int ok = 0;
    pdo_mylite_db_handle *handle = pecalloc(1, sizeof(*handle), dbh->is_persistent);
    dbh->driver_data = handle;
    handle->last_insert_id = zend_string_init("0", 1, false);
    memcpy(dbh->error_code, "00000", sizeof("00000"));

    if (dbh->is_persistent) {
        pdo_mylite_error(
            dbh,
            NULL,
            MYLITE_MISUSE,
            "persistent MyLite PDO connections are not supported"
        );
        goto cleanup;
    }

    char *path = pdo_mylite_resolve_path(dbh);
    if (path == NULL || path[0] == '\0') {
        efree(path);
        pdo_mylite_error(dbh, NULL, MYLITE_MISUSE, "MyLite PDO DSN requires a path");
        goto cleanup;
    }

    int status = MYLITE_OK;
    if (strcmp(path, ":memory:") == 0) {
        status = mylite_open_memory(&handle->db);
    } else {
        status = mylite_open(path, &handle->db);
    }
    efree(path);
    if (status != MYLITE_OK) {
        pdo_mylite_error(dbh, NULL, status, "could not open MyLite database");
        goto cleanup;
    }

    dbh->alloc_own_columns = true;
    dbh->max_escaped_char_length = 2;
    ok = 1;

cleanup:
    dbh->methods = &pdo_mylite_dbh_methods;
    return ok;
}

static void pdo_mylite_handle_closer(pdo_dbh_t *dbh) {
    pdo_mylite_db_handle *handle = (pdo_mylite_db_handle *)dbh->driver_data;
    if (handle == NULL) {
        return;
    }
    if (handle->db != NULL) {
        mylite_close(handle->db);
        handle->db = NULL;
    }
    if (handle->errmsg != NULL) {
        zend_string_release(handle->errmsg);
    }
    if (handle->last_insert_id != NULL) {
        zend_string_release(handle->last_insert_id);
    }
    pefree(handle, dbh->is_persistent);
    dbh->driver_data = NULL;
}

static bool pdo_mylite_handle_preparer(
    pdo_dbh_t *dbh,
    zend_string *sql,
    pdo_stmt_t *stmt,
    zval *driver_options
) {
    (void)driver_options;
    pdo_mylite_db_handle *handle = (pdo_mylite_db_handle *)dbh->driver_data;
    pdo_mylite_stmt *statement_data = ecalloc(1, sizeof(*statement_data));
    statement_data->handle = handle;
    statement_data->sql = zend_string_copy(sql);
    stmt->driver_data = statement_data;
    stmt->methods = &pdo_mylite_stmt_methods;
    stmt->supports_placeholders = PDO_PLACEHOLDER_NONE;
    return true;
}

static zend_long pdo_mylite_handle_doer(pdo_dbh_t *dbh, const zend_string *sql) {
    pdo_mylite_db_handle *handle = (pdo_mylite_db_handle *)dbh->driver_data;
    mylite_result *result = NULL;
    const int status = mylite_execute(handle->db, ZSTR_VAL(sql), ZSTR_LEN(sql), &result);
    if (status != MYLITE_OK) {
        pdo_mylite_error(dbh, NULL, status, "could not execute MyLite SQL");
        return -1;
    }
    pdo_mylite_clear_error(dbh, NULL);
    pdo_mylite_update_status(dbh, result);
    const int64_t affected_rows = mylite_result_affected_rows(result);
    mylite_result_free(result);
    return affected_rows < 0 ? 0 : (zend_long)affected_rows;
}

static zend_string *pdo_mylite_handle_quoter(
    pdo_dbh_t *dbh,
    const zend_string *unquoted,
    enum pdo_param_type paramtype
) {
    (void)dbh;
    (void)paramtype;
    zend_string *escaped = pdo_mylite_quote_string(ZSTR_VAL(unquoted), ZSTR_LEN(unquoted));
    zend_string *quoted = zend_strpprintf(0, "'%s'", ZSTR_VAL(escaped));
    zend_string_release(escaped);
    return quoted;
}

static bool pdo_mylite_handle_begin(pdo_dbh_t *dbh) {
    zend_string *sql = zend_string_init("START TRANSACTION", strlen("START TRANSACTION"), false);
    const bool ok = pdo_mylite_handle_doer(dbh, sql) >= 0;
    zend_string_release(sql);
    return ok;
}

static bool pdo_mylite_handle_commit(pdo_dbh_t *dbh) {
    zend_string *sql = zend_string_init("COMMIT", strlen("COMMIT"), false);
    const bool ok = pdo_mylite_handle_doer(dbh, sql) >= 0;
    zend_string_release(sql);
    return ok;
}

static bool pdo_mylite_handle_rollback(pdo_dbh_t *dbh) {
    zend_string *sql = zend_string_init("ROLLBACK", strlen("ROLLBACK"), false);
    const bool ok = pdo_mylite_handle_doer(dbh, sql) >= 0;
    zend_string_release(sql);
    return ok;
}

static zend_string *pdo_mylite_last_insert_id(pdo_dbh_t *dbh, const zend_string *name) {
    (void)name;
    pdo_mylite_db_handle *handle = (pdo_mylite_db_handle *)dbh->driver_data;
    return zend_string_copy(handle->last_insert_id);
}

static void pdo_mylite_fetch_error(pdo_dbh_t *dbh, pdo_stmt_t *stmt, zval *info) {
    (void)stmt;
    pdo_mylite_db_handle *handle = (pdo_mylite_db_handle *)dbh->driver_data;
    if (handle == NULL || handle->native_errno == 0U || handle->errmsg == NULL) {
        return;
    }
    add_next_index_long(info, (zend_long)handle->native_errno);
    add_next_index_str(info, zend_string_copy(handle->errmsg));
}

static int pdo_mylite_get_attribute(pdo_dbh_t *dbh, zend_long attr, zval *return_value) {
    (void)dbh;
    switch (attr) {
    case PDO_ATTR_CLIENT_VERSION:
    case PDO_ATTR_SERVER_VERSION:
        ZVAL_STRING(return_value, mylite_version());
        return 1;
    case PDO_ATTR_DRIVER_NAME:
        ZVAL_STRING(return_value, "mylite");
        return 1;
    default:
        return 0;
    }
}

static int pdo_mylite_stmt_dtor(pdo_stmt_t *stmt) {
    pdo_mylite_stmt *statement_data = (pdo_mylite_stmt *)stmt->driver_data;
    if (statement_data == NULL) {
        return 1;
    }
    pdo_mylite_stmt_release_storage(statement_data);
    if (statement_data->sql != NULL) {
        zend_string_release(statement_data->sql);
    }
    efree(statement_data);
    stmt->driver_data = NULL;
    return 1;
}

static int pdo_mylite_stmt_execute(pdo_stmt_t *stmt) {
    pdo_mylite_stmt *statement_data = (pdo_mylite_stmt *)stmt->driver_data;
    pdo_dbh_t *dbh = stmt->dbh;
    zend_string *sql =
        stmt->active_query_string != NULL ? stmt->active_query_string : statement_data->sql;

    pdo_mylite_stmt_release_storage(statement_data);
    statement_data->cursor = 0U;
    statement_data->current_row = 0U;

    if (pdo_mylite_sql_may_stream_select(ZSTR_VAL(sql), ZSTR_LEN(sql))) {
        mylite_stmt *native_stmt = NULL;
        const int prepare_status =
            mylite_prepare(statement_data->handle->db, ZSTR_VAL(sql), ZSTR_LEN(sql), &native_stmt);

        if (prepare_status == MYLITE_OK) {
            statement_data->native_stmt = native_stmt;
            statement_data->native_column_count = mylite_stmt_column_count(native_stmt);
            if (statement_data->native_column_count > (size_t)INT_MAX) {
                pdo_mylite_stmt_release_storage(statement_data);
                pdo_mylite_error(dbh, stmt, MYLITE_ERROR, "result set has too many columns");
                return 0;
            }
            pdo_mylite_clear_error(dbh, stmt);
            php_pdo_stmt_set_column_count(stmt, (int)statement_data->native_column_count);
            stmt->row_count = 0;
            return 1;
        }
    }

    const int status = mylite_execute(
        statement_data->handle->db,
        ZSTR_VAL(sql),
        ZSTR_LEN(sql),
        &statement_data->result
    );
    if (status != MYLITE_OK) {
        pdo_mylite_error(dbh, stmt, status, "could not execute MyLite statement");
        return 0;
    }
    pdo_mylite_clear_error(dbh, stmt);
    pdo_mylite_update_status(dbh, statement_data->result);
    php_pdo_stmt_set_column_count(stmt, (int)mylite_result_column_count(statement_data->result));
    const int64_t affected_rows = mylite_result_affected_rows(statement_data->result);
    stmt->row_count = affected_rows < 0 ? 0 : (zend_long)affected_rows;
    return 1;
}

static int pdo_mylite_stmt_fetch(
    pdo_stmt_t *stmt,
    enum pdo_fetch_orientation orientation,
    zend_long offset
) {
    (void)orientation;
    (void)offset;
    pdo_mylite_stmt *statement_data = (pdo_mylite_stmt *)stmt->driver_data;
    if (statement_data->native_stmt != NULL) {
        const int status = mylite_stmt_step(statement_data->native_stmt);

        if (status == MYLITE_ROW) {
            return 1;
        }
        if (status == MYLITE_DONE) {
            return 0;
        }
        pdo_mylite_error(stmt->dbh, stmt, status, "could not fetch MyLite statement row");
        pdo_mylite_stmt_release_storage(statement_data);
        return 0;
    }
    if (statement_data->result == NULL ||
        statement_data->cursor >= mylite_result_row_count(statement_data->result)) {
        return 0;
    }
    statement_data->current_row = statement_data->cursor;
    ++statement_data->cursor;
    return 1;
}

static int pdo_mylite_stmt_describe(pdo_stmt_t *stmt, int column) {
    pdo_mylite_stmt *statement_data = (pdo_mylite_stmt *)stmt->driver_data;
    if (statement_data->native_stmt != NULL) {
        const char *name = NULL;

        if (column < 0 || (size_t)column >= statement_data->native_column_count) {
            return 0;
        }
        name = mylite_stmt_column_name(statement_data->native_stmt, (size_t)column);
        stmt->columns[column].name =
            zend_string_init(name == NULL ? "" : name, name == NULL ? 0U : strlen(name), false);
        stmt->columns[column].maxlen = 0;
        stmt->columns[column].precision = 0;
        return 1;
    }
    if (column < 0 || statement_data->result == NULL ||
        (size_t)column >= mylite_result_column_count(statement_data->result)) {
        return 0;
    }
    const char *name = mylite_result_column_name(statement_data->result, (size_t)column);
    stmt->columns[column].name =
        zend_string_init(name == NULL ? "" : name, name == NULL ? 0U : strlen(name), false);
    stmt->columns[column].maxlen = 0;
    stmt->columns[column].precision = 0;
    return 1;
}

static int pdo_mylite_stmt_get_col(
    pdo_stmt_t *stmt,
    int column,
    zval *result,
    enum pdo_param_type *type
) {
    (void)type;
    pdo_mylite_stmt *statement_data = (pdo_mylite_stmt *)stmt->driver_data;
    if (statement_data->native_stmt != NULL) {
        const void *bytes = NULL;

        if (column < 0 || (size_t)column >= statement_data->native_column_count) {
            ZVAL_NULL(result);
            return 0;
        }
        bytes = mylite_stmt_value_bytes(statement_data->native_stmt, (size_t)column);
        if (bytes == NULL) {
            ZVAL_NULL(result);
        } else {
            ZVAL_STRINGL(
                result,
                (const char *)bytes,
                mylite_stmt_value_size(statement_data->native_stmt, (size_t)column)
            );
        }
        return 1;
    }
    if (column < 0 || statement_data->result == NULL ||
        (size_t)column >= mylite_result_column_count(statement_data->result)) {
        ZVAL_NULL(result);
        return 0;
    }

    const void *bytes = mylite_result_value_bytes(
        statement_data->result,
        statement_data->current_row,
        (size_t)column
    );
    if (bytes == NULL) {
        ZVAL_NULL(result);
    } else {
        ZVAL_STRINGL(
            result,
            (const char *)bytes,
            mylite_result_value_size(
                statement_data->result,
                statement_data->current_row,
                (size_t)column
            )
        );
    }
    return 1;
}

static int pdo_mylite_stmt_cursor_closer(pdo_stmt_t *stmt) {
    pdo_mylite_stmt *statement_data = (pdo_mylite_stmt *)stmt->driver_data;
    if (statement_data != NULL) {
        pdo_mylite_stmt_release_storage(statement_data);
    }
    return 1;
}

static int pdo_mylite_error(pdo_dbh_t *dbh, pdo_stmt_t *stmt, int status, const char *fallback) {
    pdo_mylite_db_handle *handle = (pdo_mylite_db_handle *)dbh->driver_data;
    pdo_error_type *pdo_error = stmt != NULL ? &stmt->error_code : &dbh->error_code;
    const char *sqlstate =
        handle != NULL && handle->db != NULL ? mylite_sqlstate(handle->db) : "HY000";
    const char *message =
        handle != NULL && handle->db != NULL ? mylite_errmsg(handle->db) : fallback;
    unsigned native_errno = handle != NULL && handle->db != NULL
                                ? (unsigned)mylite_errcode(handle->db)
                                : (unsigned)status;

    if (message == NULL || message[0] == '\0') {
        message = fallback;
    }
    if (native_errno == 0U) {
        native_errno = (unsigned)status;
    }

    {
        size_t sqlstate_size = strlen(sqlstate);

        if (sqlstate_size >= sizeof(*pdo_error)) {
            sqlstate_size = sizeof(*pdo_error) - 1U;
        }
        memcpy(*pdo_error, sqlstate, sqlstate_size);
        (*pdo_error)[sqlstate_size] = '\0';
    }
    if (handle != NULL) {
        handle->native_errno = native_errno;
        if (handle->errmsg != NULL) {
            zend_string_release(handle->errmsg);
        }
        handle->errmsg = zend_string_init(message, strlen(message), false);
    }
    return status;
}

static void pdo_mylite_clear_error(pdo_dbh_t *dbh, pdo_stmt_t *stmt) {
    pdo_error_type *pdo_error = stmt != NULL ? &stmt->error_code : &dbh->error_code;
    pdo_mylite_db_handle *handle = (pdo_mylite_db_handle *)dbh->driver_data;

    memcpy(*pdo_error, "00000", sizeof("00000"));
    if (handle != NULL) {
        handle->native_errno = 0U;
        if (handle->errmsg != NULL) {
            zend_string_release(handle->errmsg);
            handle->errmsg = NULL;
        }
    }
}

static void pdo_mylite_stmt_release_storage(pdo_mylite_stmt *statement_data) {
    if (statement_data->result != NULL) {
        mylite_result_free(statement_data->result);
        statement_data->result = NULL;
    }
    if (statement_data->native_stmt != NULL) {
        (void)mylite_stmt_finalize(statement_data->native_stmt);
        statement_data->native_stmt = NULL;
    }
    statement_data->native_column_count = 0U;
}

static bool pdo_mylite_sql_may_stream_select(const char *sql, size_t length) {
    size_t offset = 0U;

    for (;;) {
        while (offset < length && isspace((unsigned char)sql[offset])) {
            ++offset;
        }
        if (offset + 1U < length && sql[offset] == '/' && sql[offset + 1U] == '*') {
            offset += 2U;
            while (offset + 1U < length && !(sql[offset] == '*' && sql[offset + 1U] == '/')) {
                ++offset;
            }
            if (offset + 1U >= length) {
                return false;
            }
            offset += 2U;
            continue;
        }
        if (offset < length && sql[offset] == '#') {
            while (offset < length && sql[offset] != '\n' && sql[offset] != '\r') {
                ++offset;
            }
            continue;
        }
        if (offset + 2U < length && sql[offset] == '-' && sql[offset + 1U] == '-' &&
            isspace((unsigned char)sql[offset + 2U])) {
            offset += 2U;
            while (offset < length && sql[offset] != '\n' && sql[offset] != '\r') {
                ++offset;
            }
            continue;
        }
        break;
    }

    return pdo_mylite_sql_keyword_at(sql, length, offset, "select") ||
           pdo_mylite_sql_keyword_at(sql, length, offset, "with");
}

static bool pdo_mylite_sql_keyword_at(
    const char *sql,
    size_t length,
    size_t offset,
    const char *keyword
) {
    size_t keyword_length = strlen(keyword);

    if (offset + keyword_length > length) {
        return false;
    }
    for (size_t index = 0U; index < keyword_length; ++index) {
        if (tolower((unsigned char)sql[offset + index]) != keyword[index]) {
            return false;
        }
    }
    return offset + keyword_length == length ||
           !pdo_mylite_is_identifier_byte((unsigned char)sql[offset + keyword_length]);
}

static bool pdo_mylite_is_identifier_byte(unsigned char ch) {
    return isalnum(ch) || ch == '_' || ch == '$';
}

static void pdo_mylite_update_status(pdo_dbh_t *dbh, const mylite_result *result) {
    pdo_mylite_db_handle *handle = (pdo_mylite_db_handle *)dbh->driver_data;
    zend_string_release(handle->last_insert_id);
    handle->last_insert_id = zend_u64_to_str(mylite_result_insert_id(result));
}

static zend_string *pdo_mylite_quote_string(const char *value, size_t length) {
    smart_str escaped = {0};

    for (size_t index = 0U; index < length; ++index) {
        switch (value[index]) {
        case '\0':
            smart_str_appendl(&escaped, "\\0", 2);
            break;
        case '\n':
            smart_str_appendl(&escaped, "\\n", 2);
            break;
        case '\r':
            smart_str_appendl(&escaped, "\\r", 2);
            break;
        case '\\':
        case '\'':
        case '"':
            smart_str_appendc(&escaped, '\\');
            smart_str_appendc(&escaped, value[index]);
            break;
        case '\032':
            smart_str_appendl(&escaped, "\\Z", 2);
            break;
        default:
            smart_str_appendc(&escaped, value[index]);
            break;
        }
    }
    smart_str_0(&escaped);
    return escaped.s == NULL ? zend_string_init("", 0, false) : escaped.s;
}

static char *pdo_mylite_resolve_path(pdo_dbh_t *dbh) {
    const char *source = dbh->data_source;

    if (source == NULL || source[0] == '\0') {
        return estrdup("");
    }
    if (strncmp(source, "path=", strlen("path=")) == 0) {
        return estrdup(source + strlen("path="));
    }
    return estrdup(source);
}
