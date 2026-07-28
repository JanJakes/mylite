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

#include "php_mylite_native_value.h"

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
    mylite_stmt *native;
    unsigned native_errno;
    zend_string *errmsg;
    bool pending_row;
    bool ready_for_execute;
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
static bool pdo_mylite_set_attribute(pdo_dbh_t *dbh, zend_long attr, zval *value);
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
static int pdo_mylite_stmt_get_column_meta(pdo_stmt_t *stmt, zend_long column, zval *return_value);
static const char *pdo_mylite_column_native_type(enum mylite_result_column_type type);
static enum pdo_param_type pdo_mylite_column_pdo_type(enum mylite_result_column_type type);
static void pdo_mylite_add_column_flags(zval *return_value, uint32_t native_flags);
static int pdo_mylite_stmt_param_hook(
    pdo_stmt_t *stmt,
    struct pdo_bound_param_data *parameter,
    enum pdo_param_event event_type
);
static int pdo_mylite_stmt_cursor_closer(pdo_stmt_t *stmt);
static int pdo_mylite_bind_parameter(
    pdo_stmt_t *stmt,
    pdo_mylite_stmt *statement_data,
    struct pdo_bound_param_data *parameter
);
static int pdo_mylite_error(pdo_dbh_t *dbh, pdo_stmt_t *stmt, int status, const char *fallback);
static int pdo_mylite_open_error(
    pdo_dbh_t *dbh,
    int status,
    const struct mylite_open_diagnostic *diagnostic
);
static void pdo_mylite_clear_error(pdo_dbh_t *dbh, pdo_stmt_t *stmt);
static void pdo_mylite_replace_error_record(
    unsigned *target_errno,
    zend_string **target_message,
    unsigned native_errno,
    const char *message
);
static void pdo_mylite_update_status(pdo_dbh_t *dbh, const mylite_result *result);
static void pdo_mylite_update_statement_status(pdo_dbh_t *dbh, const mylite_stmt *statement);
static zend_string *pdo_mylite_quote_string(
    const char *value,
    size_t length,
    bool no_backslash_escapes
);
static zend_string *pdo_mylite_resolve_path(pdo_dbh_t *dbh);

static const struct pdo_dbh_methods pdo_mylite_dbh_methods = {
    pdo_mylite_handle_closer,
    pdo_mylite_handle_preparer,
    pdo_mylite_handle_doer,
    pdo_mylite_handle_quoter,
    pdo_mylite_handle_begin,
    pdo_mylite_handle_commit,
    pdo_mylite_handle_rollback,
    pdo_mylite_set_attribute,
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
    pdo_mylite_stmt_param_hook,
    NULL,
    NULL,
    pdo_mylite_stmt_get_column_meta,
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
    struct mylite_open_diagnostic diagnostic;
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

    zend_string *path = pdo_mylite_resolve_path(dbh);
    if (path == NULL || ZSTR_LEN(path) == 0U) {
        if (path != NULL) {
            zend_string_release(path);
        }
        pdo_mylite_error(dbh, NULL, MYLITE_MISUSE, "MyLite PDO DSN requires a path");
        goto cleanup;
    }
    if (memchr(ZSTR_VAL(path), '\0', ZSTR_LEN(path)) != NULL) {
        zend_string_release(path);
        pdo_mylite_error(dbh, NULL, MYLITE_MISUSE, "MyLite PDO paths do not support NUL bytes");
        goto cleanup;
    }

    int status = MYLITE_OK;
    if (ZSTR_LEN(path) == strlen(":memory:") &&
        memcmp(ZSTR_VAL(path), ":memory:", ZSTR_LEN(path)) == 0) {
        status = mylite_open_memory_with_diagnostic(&handle->db, &diagnostic);
    } else {
        status = mylite_open_with_size_and_diagnostic(
            ZSTR_VAL(path),
            ZSTR_LEN(path),
            &handle->db,
            &diagnostic
        );
    }
    zend_string_release(path);
    if (status != MYLITE_OK) {
        pdo_mylite_open_error(dbh, status, &diagnostic);
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
    pdo_mylite_db_handle *handle = (pdo_mylite_db_handle *)dbh->driver_data;
    pdo_mylite_stmt *statement_data = ecalloc(1, sizeof(*statement_data));
    zend_string *rewritten_sql = NULL;
    zend_string *prepared_sql = sql;

    if (pdo_attr_lval(driver_options, PDO_ATTR_CURSOR, PDO_CURSOR_FWDONLY) != PDO_CURSOR_FWDONLY) {
        pdo_mylite_error(
            dbh,
            NULL,
            MYLITE_MISUSE,
            "scrollable cursors are not supported by the MyLite PDO driver"
        );
        efree(statement_data);
        return false;
    }

    statement_data->handle = handle;
    stmt->driver_data = statement_data;
    stmt->methods = &pdo_mylite_stmt_methods;
    stmt->supports_placeholders = PDO_PLACEHOLDER_POSITIONAL;

    const int rewrite_status = pdo_parse_params(stmt, sql, &rewritten_sql);
    if (rewrite_status < 0) {
        efree(statement_data);
        stmt->driver_data = NULL;
        return false;
    }
    if (rewrite_status > 0) {
        prepared_sql = rewritten_sql;
    }
    const int status = mylite_prepare_buffered(
        handle->db,
        ZSTR_VAL(prepared_sql),
        ZSTR_LEN(prepared_sql),
        &statement_data->native
    );
    if (rewritten_sql != NULL) {
        zend_string_release(rewritten_sql);
    }
    if (status != MYLITE_OK) {
        pdo_mylite_error(dbh, NULL, status, "could not prepare MyLite statement");
        efree(statement_data);
        stmt->driver_data = NULL;
        return false;
    }
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
    (void)paramtype;
    pdo_mylite_db_handle *handle = (pdo_mylite_db_handle *)dbh->driver_data;
    const bool no_backslash_escapes = mylite_session_no_backslash_escapes(handle->db) > 0;

    return pdo_mylite_quote_string(ZSTR_VAL(unquoted), ZSTR_LEN(unquoted), no_backslash_escapes);
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

static bool pdo_mylite_set_attribute(pdo_dbh_t *dbh, zend_long attr, zval *value) {
    (void)dbh;
    return attr == PDO_ATTR_EMULATE_PREPARES && !zend_is_true(value);
}

static zend_string *pdo_mylite_last_insert_id(pdo_dbh_t *dbh, const zend_string *name) {
    pdo_mylite_db_handle *handle = (pdo_mylite_db_handle *)dbh->driver_data;
    (void)name;
    return zend_string_copy(handle->last_insert_id);
}

static void pdo_mylite_fetch_error(pdo_dbh_t *dbh, pdo_stmt_t *stmt, zval *info) {
    pdo_mylite_db_handle *handle = (pdo_mylite_db_handle *)dbh->driver_data;
    pdo_mylite_stmt *statement_data = stmt == NULL ? NULL : (pdo_mylite_stmt *)stmt->driver_data;
    unsigned native_errno = 0U;
    zend_string *message = NULL;

    if (statement_data != NULL) {
        native_errno = statement_data->native_errno;
        message = statement_data->errmsg;
    } else if (handle != NULL) {
        native_errno = handle->native_errno;
        message = handle->errmsg;
    }

    if (native_errno == 0U || message == NULL) {
        return;
    }
    add_next_index_long(info, (zend_long)native_errno);
    add_next_index_str(info, zend_string_copy(message));
}

static int pdo_mylite_get_attribute(pdo_dbh_t *dbh, zend_long attr, zval *return_value) {
    (void)dbh;
    switch (attr) {
    case PDO_ATTR_CLIENT_VERSION:
        ZVAL_STRING(return_value, mylite_version());
        return 1;
    case PDO_ATTR_SERVER_VERSION:
        ZVAL_STRING(return_value, mylite_server_version());
        return 1;
    case PDO_ATTR_DRIVER_NAME:
        ZVAL_STRING(return_value, "mylite");
        return 1;
    case PDO_ATTR_EMULATE_PREPARES:
        ZVAL_FALSE(return_value);
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
    if (statement_data->native != NULL) {
        (void)mylite_stmt_finalize(statement_data->native);
        statement_data->native = NULL;
    }
    if (statement_data->errmsg != NULL) {
        zend_string_release(statement_data->errmsg);
    }
    efree(statement_data);
    stmt->driver_data = NULL;
    return 1;
}

static int pdo_mylite_stmt_execute(pdo_stmt_t *stmt) {
    pdo_mylite_stmt *statement_data = (pdo_mylite_stmt *)stmt->driver_data;
    pdo_dbh_t *dbh = stmt->dbh;
    int status = MYLITE_OK;

    if (!statement_data->ready_for_execute) {
        status = mylite_stmt_reset(statement_data->native);
        if (status == MYLITE_OK) {
            status = mylite_stmt_clear_bindings(statement_data->native);
        }
    }
    statement_data->ready_for_execute = false;
    statement_data->pending_row = false;
    if (status != MYLITE_OK) {
        pdo_mylite_error(dbh, stmt, status, "could not reset MyLite statement");
        return 0;
    }

    status = mylite_stmt_step(statement_data->native);
    if (status != MYLITE_ROW && status != MYLITE_DONE) {
        pdo_mylite_error(dbh, stmt, status, "could not execute MyLite statement");
        return 0;
    }
    statement_data->pending_row = status == MYLITE_ROW;
    pdo_mylite_clear_error(dbh, stmt);
    pdo_mylite_update_statement_status(dbh, statement_data->native);
    const size_t column_count = mylite_stmt_column_count(statement_data->native);
    if (column_count > (size_t)INT_MAX) {
        pdo_mylite_error(dbh, stmt, MYLITE_ERROR, "result set has too many columns");
        return 0;
    }
    php_pdo_stmt_set_column_count(stmt, (int)column_count);
    if (column_count > 0U) {
        const size_t row_count = mylite_stmt_buffered_row_count(statement_data->native);

        stmt->row_count = row_count > (size_t)ZEND_LONG_MAX ? ZEND_LONG_MAX : (zend_long)row_count;
    } else {
        const int64_t affected_rows = mylite_stmt_affected_rows(statement_data->native);

        stmt->row_count = affected_rows < 0 ? 0 : (zend_long)affected_rows;
    }
    return 1;
}

static int pdo_mylite_stmt_fetch(
    pdo_stmt_t *stmt,
    enum pdo_fetch_orientation orientation,
    zend_long offset
) {
    pdo_mylite_stmt *statement_data = (pdo_mylite_stmt *)stmt->driver_data;

    if (orientation != PDO_FETCH_ORI_NEXT || offset != 0) {
        pdo_mylite_error(
            stmt->dbh,
            stmt,
            MYLITE_MISUSE,
            "fetch orientation is not supported by the MyLite PDO driver"
        );
        return 0;
    }

    if (statement_data->pending_row) {
        statement_data->pending_row = false;
        return 1;
    }
    const int status = mylite_stmt_step(statement_data->native);
    if (status == MYLITE_DONE) {
        return 0;
    }
    if (status != MYLITE_ROW) {
        pdo_mylite_error(stmt->dbh, stmt, status, "could not fetch MyLite row");
        return 0;
    }
    return 1;
}

static int pdo_mylite_stmt_describe(pdo_stmt_t *stmt, int column) {
    pdo_mylite_stmt *statement_data = (pdo_mylite_stmt *)stmt->driver_data;
    if (column < 0 || (size_t)column >= mylite_stmt_column_count(statement_data->native)) {
        return 0;
    }
    const char *name = mylite_stmt_column_name(statement_data->native, (size_t)column);
    stmt->columns[column].name =
        zend_string_init(name == NULL ? "" : name, name == NULL ? 0U : strlen(name), false);
    const uint64_t display_length =
        mylite_stmt_column_display_length(statement_data->native, (size_t)column);
    stmt->columns[column].maxlen = (size_t)display_length;
    if ((uint64_t)stmt->columns[column].maxlen != display_length) {
        stmt->columns[column].maxlen = SIZE_MAX;
    }
    stmt->columns[column].precision =
        (zend_ulong)mylite_stmt_column_decimals(statement_data->native, (size_t)column);
    return 1;
}

static int pdo_mylite_stmt_get_col(
    pdo_stmt_t *stmt,
    int column,
    zval *result,
    enum pdo_param_type *type
) {
    pdo_mylite_stmt *statement_data = (pdo_mylite_stmt *)stmt->driver_data;
    if (column < 0 || (size_t)column >= mylite_stmt_column_count(statement_data->native)) {
        ZVAL_NULL(result);
        return 0;
    }

    if (mylite_stmt_value_is_null(statement_data->native, (size_t)column)) {
        ZVAL_NULL(result);
        if (type != NULL) {
            *type = PDO_PARAM_NULL;
        }
    } else {
        const void *bytes = mylite_stmt_value_bytes(statement_data->native, (size_t)column);
        mylite_php_native_value_to_zval(
            mylite_stmt_column_type(statement_data->native, (size_t)column),
            bytes,
            mylite_stmt_value_size(statement_data->native, (size_t)column),
            stmt->dbh->stringify != 0,
            result
        );
        if (type != NULL) {
            *type = Z_TYPE_P(result) == IS_LONG ? PDO_PARAM_INT : PDO_PARAM_STR;
        }
    }
    return 1;
}

static int pdo_mylite_stmt_get_column_meta(pdo_stmt_t *stmt, zend_long column, zval *return_value) {
    pdo_mylite_stmt *statement_data = (pdo_mylite_stmt *)stmt->driver_data;

    if (column < 0 || (size_t)column >= mylite_stmt_column_count(statement_data->native)) {
        return 0;
    }
    const size_t column_index = (size_t)column;
    const enum mylite_result_column_type native_type =
        mylite_stmt_column_type(statement_data->native, column_index);
    const char *table = mylite_stmt_column_table_name(statement_data->native, column_index);

    array_init(return_value);
    add_assoc_string(
        return_value,
        "native_type",
        (char *)pdo_mylite_column_native_type(native_type)
    );
    add_assoc_long(return_value, "pdo_type", (zend_long)pdo_mylite_column_pdo_type(native_type));
    pdo_mylite_add_column_flags(
        return_value,
        mylite_stmt_column_flags(statement_data->native, column_index)
    );
    add_assoc_string(return_value, "table", (char *)(table == NULL ? "" : table));
    return 1;
}

static const char *pdo_mylite_column_native_type(enum mylite_result_column_type type) {
    switch (type) {
    case MYLITE_RESULT_COLUMN_TYPE_DECIMAL:
        return "DECIMAL";
    case MYLITE_RESULT_COLUMN_TYPE_TINY:
        return "TINY";
    case MYLITE_RESULT_COLUMN_TYPE_SHORT:
        return "SHORT";
    case MYLITE_RESULT_COLUMN_TYPE_LONG:
        return "LONG";
    case MYLITE_RESULT_COLUMN_TYPE_FLOAT:
        return "FLOAT";
    case MYLITE_RESULT_COLUMN_TYPE_DOUBLE:
        return "DOUBLE";
    case MYLITE_RESULT_COLUMN_TYPE_NULL:
        return "NULL";
    case MYLITE_RESULT_COLUMN_TYPE_TIMESTAMP:
        return "TIMESTAMP";
    case MYLITE_RESULT_COLUMN_TYPE_LONGLONG:
        return "LONGLONG";
    case MYLITE_RESULT_COLUMN_TYPE_INT24:
        return "INT24";
    case MYLITE_RESULT_COLUMN_TYPE_DATE:
        return "DATE";
    case MYLITE_RESULT_COLUMN_TYPE_TIME:
        return "TIME";
    case MYLITE_RESULT_COLUMN_TYPE_DATETIME:
        return "DATETIME";
    case MYLITE_RESULT_COLUMN_TYPE_YEAR:
        return "YEAR";
    case MYLITE_RESULT_COLUMN_TYPE_VARCHAR:
        return "VARCHAR";
    case MYLITE_RESULT_COLUMN_TYPE_BIT:
        return "BIT";
    case MYLITE_RESULT_COLUMN_TYPE_JSON:
        return "JSON";
    case MYLITE_RESULT_COLUMN_TYPE_NEWDECIMAL:
        return "NEWDECIMAL";
    case MYLITE_RESULT_COLUMN_TYPE_BLOB:
        return "BLOB";
    case MYLITE_RESULT_COLUMN_TYPE_VAR_STRING:
        return "VAR_STRING";
    case MYLITE_RESULT_COLUMN_TYPE_STRING:
        return "STRING";
    case MYLITE_RESULT_COLUMN_TYPE_GEOMETRY:
        return "GEOMETRY";
    case MYLITE_RESULT_COLUMN_TYPE_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

static enum pdo_param_type pdo_mylite_column_pdo_type(enum mylite_result_column_type type) {
    switch (type) {
    case MYLITE_RESULT_COLUMN_TYPE_TINY:
    case MYLITE_RESULT_COLUMN_TYPE_SHORT:
    case MYLITE_RESULT_COLUMN_TYPE_LONG:
    case MYLITE_RESULT_COLUMN_TYPE_LONGLONG:
    case MYLITE_RESULT_COLUMN_TYPE_INT24:
    case MYLITE_RESULT_COLUMN_TYPE_YEAR:
    case MYLITE_RESULT_COLUMN_TYPE_BIT:
        return PDO_PARAM_INT;
    default:
        return PDO_PARAM_STR;
    }
}

static void pdo_mylite_add_column_flags(zval *return_value, uint32_t native_flags) {
    zval flags;

    array_init(&flags);
    if ((native_flags & MYLITE_RESULT_COLUMN_FLAG_NOT_NULL) != 0U) {
        add_next_index_string(&flags, "not_null");
    }
    if ((native_flags & MYLITE_RESULT_COLUMN_FLAG_PRI_KEY) != 0U) {
        add_next_index_string(&flags, "primary_key");
    }
    if ((native_flags & MYLITE_RESULT_COLUMN_FLAG_UNIQUE_KEY) != 0U) {
        add_next_index_string(&flags, "unique_key");
    }
    if ((native_flags & MYLITE_RESULT_COLUMN_FLAG_MULTIPLE_KEY) != 0U) {
        add_next_index_string(&flags, "multiple_key");
    }
    if ((native_flags & MYLITE_RESULT_COLUMN_FLAG_BLOB) != 0U) {
        add_next_index_string(&flags, "blob");
    }
    add_assoc_zval(return_value, "flags", &flags);
}

static int pdo_mylite_stmt_param_hook(
    pdo_stmt_t *stmt,
    struct pdo_bound_param_data *parameter,
    enum pdo_param_event event_type
) {
    pdo_mylite_stmt *statement_data = (pdo_mylite_stmt *)stmt->driver_data;

    if (event_type == PDO_PARAM_EVT_EXEC_POST) {
        statement_data->ready_for_execute = false;
        return 1;
    }
    if (!parameter->is_param || event_type != PDO_PARAM_EVT_EXEC_PRE) {
        return 1;
    }
    if (!statement_data->ready_for_execute) {
        int status = mylite_stmt_reset(statement_data->native);

        if (status == MYLITE_OK) {
            status = mylite_stmt_clear_bindings(statement_data->native);
        }
        if (status != MYLITE_OK) {
            pdo_mylite_error(stmt->dbh, stmt, status, "could not reset MyLite statement");
            return 0;
        }
        statement_data->ready_for_execute = true;
        statement_data->pending_row = false;
    }
    return pdo_mylite_bind_parameter(stmt, statement_data, parameter);
}

static int pdo_mylite_stmt_cursor_closer(pdo_stmt_t *stmt) {
    pdo_mylite_stmt *statement_data = (pdo_mylite_stmt *)stmt->driver_data;
    if (statement_data == NULL || statement_data->native == NULL) {
        return 1;
    }
    const int status = mylite_stmt_reset(statement_data->native);
    statement_data->pending_row = false;
    statement_data->ready_for_execute = false;
    if (status != MYLITE_OK) {
        pdo_mylite_error(stmt->dbh, stmt, status, "could not close MyLite cursor");
        return 0;
    }
    return 1;
}

static int pdo_mylite_bind_parameter(
    pdo_stmt_t *stmt,
    pdo_mylite_stmt *statement_data,
    struct pdo_bound_param_data *parameter
) {
    if (parameter->paramno < 0 ||
        (uint64_t)parameter->paramno >= mylite_stmt_parameter_count(statement_data->native)) {
        pdo_mylite_error(stmt->dbh, stmt, MYLITE_MISUSE, "invalid MyLite parameter index");
        return 0;
    }

    zval *value = &parameter->parameter;
    const size_t index = (size_t)parameter->paramno;
    const enum pdo_param_type type = PDO_PARAM_TYPE(parameter->param_type);
    int status = MYLITE_OK;

    ZVAL_DEREF(value);
    if (Z_TYPE_P(value) == IS_NULL || type == PDO_PARAM_NULL) {
        status = mylite_stmt_bind_null(statement_data->native, index);
    } else {
        switch (type) {
        case PDO_PARAM_BOOL:
            status = mylite_stmt_bind_int64(statement_data->native, index, zend_is_true(value));
            break;
        case PDO_PARAM_INT:
            status = mylite_stmt_bind_int64(statement_data->native, index, zval_get_long(value));
            break;
        case PDO_PARAM_LOB: {
            zend_string *bytes = NULL;

            if (Z_TYPE_P(value) == IS_RESOURCE) {
                php_stream *stream = NULL;
                php_stream_from_zval_no_verify(stream, value);
                if (stream != NULL) {
                    bytes = php_stream_copy_to_mem(stream, PHP_STREAM_COPY_ALL, false);
                }
            } else {
                bytes = zval_get_string(value);
            }
            if (bytes == NULL) {
                pdo_mylite_error(stmt->dbh, stmt, MYLITE_ERROR, "could not read LOB parameter");
                return 0;
            }
            status = mylite_stmt_bind_blob(
                statement_data->native,
                index,
                ZSTR_VAL(bytes),
                ZSTR_LEN(bytes)
            );
            zend_string_release(bytes);
            break;
        }
        case PDO_PARAM_STR:
        default: {
            zend_string *text = zval_get_string(value);

            if (UNEXPECTED(EG(exception) != NULL)) {
                return 0;
            }
            status = mylite_stmt_bind_text(
                statement_data->native,
                index,
                ZSTR_VAL(text),
                ZSTR_LEN(text)
            );
            zend_string_release(text);
            break;
        }
        }
    }
    if (status != MYLITE_OK) {
        pdo_mylite_error(stmt->dbh, stmt, status, "could not bind MyLite parameter");
        return 0;
    }
    return 1;
}

static int pdo_mylite_error(pdo_dbh_t *dbh, pdo_stmt_t *stmt, int status, const char *fallback) {
    pdo_mylite_db_handle *handle = (pdo_mylite_db_handle *)dbh->driver_data;
    pdo_mylite_stmt *statement_data = stmt == NULL ? NULL : (pdo_mylite_stmt *)stmt->driver_data;
    pdo_error_type *pdo_error = stmt != NULL ? &stmt->error_code : &dbh->error_code;
    const bool has_statement_diagnostic = statement_data != NULL &&
                                          statement_data->native != NULL &&
                                          mylite_stmt_errcode(statement_data->native) != MYLITE_OK;
    const bool has_connection_diagnostic =
        handle != NULL && handle->db != NULL && mylite_errcode(handle->db) != MYLITE_OK;
    const char *sqlstate = "HY000";
    const char *message = fallback;
    unsigned native_errno = (unsigned)status;

    if (has_statement_diagnostic) {
        sqlstate = mylite_stmt_sqlstate(statement_data->native);
        message = mylite_stmt_errmsg(statement_data->native);
        native_errno = (unsigned)mylite_stmt_errcode(statement_data->native);
    } else if (has_connection_diagnostic) {
        sqlstate = mylite_sqlstate(handle->db);
        message = mylite_errmsg(handle->db);
        native_errno = (unsigned)mylite_errcode(handle->db);
    }

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
    if (statement_data != NULL) {
        pdo_mylite_replace_error_record(
            &statement_data->native_errno,
            &statement_data->errmsg,
            native_errno,
            message
        );
    } else if (handle != NULL) {
        pdo_mylite_replace_error_record(
            &handle->native_errno,
            &handle->errmsg,
            native_errno,
            message
        );
    }
    if (stmt == NULL && dbh->methods == NULL) {
        pdo_throw_exception(
            native_errno,
            handle != NULL && handle->errmsg != NULL ? ZSTR_VAL(handle->errmsg) : (char *)message,
            pdo_error
        );
    }
    return status;
}

static int pdo_mylite_open_error(
    pdo_dbh_t *dbh,
    int status,
    const struct mylite_open_diagnostic *diagnostic
) {
    pdo_mylite_db_handle *handle = (pdo_mylite_db_handle *)dbh->driver_data;
    const char *message = diagnostic == NULL ? NULL : diagnostic->message;
    const char *sqlstate = diagnostic == NULL ? "HY000" : diagnostic->sqlstate;
    unsigned native_errno =
        diagnostic == NULL ? (unsigned)status : (unsigned)diagnostic->error_code;
    size_t sqlstate_size = strlen(sqlstate);

    if (message == NULL || message[0] == '\0') {
        message = "could not open MyLite database";
    }
    if (native_errno == 0U) {
        native_errno = (unsigned)status;
    }
    if (sqlstate_size >= sizeof(dbh->error_code)) {
        sqlstate_size = sizeof(dbh->error_code) - 1U;
    }
    memcpy(dbh->error_code, sqlstate, sqlstate_size);
    dbh->error_code[sqlstate_size] = '\0';
    if (handle != NULL) {
        pdo_mylite_replace_error_record(
            &handle->native_errno,
            &handle->errmsg,
            native_errno,
            message
        );
    }
    if (dbh->methods == NULL) {
        pdo_throw_exception(
            native_errno,
            handle != NULL && handle->errmsg != NULL ? ZSTR_VAL(handle->errmsg) : (char *)message,
            &dbh->error_code
        );
    }
    return status;
}

static void pdo_mylite_clear_error(pdo_dbh_t *dbh, pdo_stmt_t *stmt) {
    pdo_error_type *pdo_error = stmt != NULL ? &stmt->error_code : &dbh->error_code;
    pdo_mylite_db_handle *handle = (pdo_mylite_db_handle *)dbh->driver_data;
    pdo_mylite_stmt *statement_data = stmt == NULL ? NULL : (pdo_mylite_stmt *)stmt->driver_data;

    memcpy(*pdo_error, "00000", sizeof("00000"));
    if (statement_data != NULL) {
        pdo_mylite_replace_error_record(
            &statement_data->native_errno,
            &statement_data->errmsg,
            0U,
            NULL
        );
    } else if (handle != NULL) {
        pdo_mylite_replace_error_record(&handle->native_errno, &handle->errmsg, 0U, NULL);
    }
}

static void pdo_mylite_replace_error_record(
    unsigned *target_errno,
    zend_string **target_message,
    unsigned native_errno,
    const char *message
) {
    if (target_errno == NULL || target_message == NULL) {
        return;
    }

    *target_errno = native_errno;
    if (*target_message != NULL) {
        zend_string_release(*target_message);
        *target_message = NULL;
    }
    if (message != NULL) {
        *target_message = zend_string_init(message, strlen(message), false);
    }
}

static void pdo_mylite_update_status(pdo_dbh_t *dbh, const mylite_result *result) {
    pdo_mylite_db_handle *handle = (pdo_mylite_db_handle *)dbh->driver_data;
    zend_string_release(handle->last_insert_id);
    handle->last_insert_id = zend_u64_to_str(mylite_result_insert_id(result));
}

static void pdo_mylite_update_statement_status(pdo_dbh_t *dbh, const mylite_stmt *statement) {
    pdo_mylite_db_handle *handle = (pdo_mylite_db_handle *)dbh->driver_data;
    zend_string_release(handle->last_insert_id);
    handle->last_insert_id = zend_u64_to_str(mylite_stmt_insert_id(statement));
}

static zend_string *pdo_mylite_quote_string(
    const char *value,
    size_t length,
    bool no_backslash_escapes
) {
    smart_str quoted = {0};

    smart_str_appendc(&quoted, '\'');
    for (size_t index = 0U; index < length; ++index) {
        if (no_backslash_escapes) {
            if (value[index] == '\'') {
                smart_str_appendc(&quoted, '\'');
            }
            smart_str_appendc(&quoted, value[index]);
            continue;
        }
        switch (value[index]) {
        case '\0':
            smart_str_appendl(&quoted, "\\0", 2);
            break;
        case '\n':
            smart_str_appendl(&quoted, "\\n", 2);
            break;
        case '\r':
            smart_str_appendl(&quoted, "\\r", 2);
            break;
        case '\\':
        case '\'':
        case '"':
            smart_str_appendc(&quoted, '\\');
            smart_str_appendc(&quoted, value[index]);
            break;
        case '\032':
            smart_str_appendl(&quoted, "\\Z", 2);
            break;
        default:
            smart_str_appendc(&quoted, value[index]);
            break;
        }
    }
    smart_str_appendc(&quoted, '\'');
    smart_str_0(&quoted);
    return quoted.s;
}

static zend_string *pdo_mylite_resolve_path(pdo_dbh_t *dbh) {
    static const char dsn_prefix[] = "mylite:";
    static const char prefix[] = "path=";
    const char *source = dbh->data_source;
    size_t source_length = dbh->data_source_len;
    zend_execute_data *execute_data = EG(current_execute_data);

    if (execute_data != NULL && ZEND_CALL_NUM_ARGS(execute_data) >= 1U) {
        zval *dsn = ZEND_CALL_ARG(execute_data, 1);

        ZVAL_DEREF(dsn);
        if (Z_TYPE_P(dsn) == IS_STRING && Z_STRLEN_P(dsn) >= sizeof(dsn_prefix) - 1U &&
            memcmp(Z_STRVAL_P(dsn), dsn_prefix, sizeof(dsn_prefix) - 1U) == 0) {
            source = Z_STRVAL_P(dsn) + sizeof(dsn_prefix) - 1U;
            source_length = Z_STRLEN_P(dsn) - (sizeof(dsn_prefix) - 1U);
        }
    }

    if (source == NULL || source_length == 0U) {
        return zend_string_init("", 0U, false);
    }
    if (source_length >= sizeof(prefix) - 1U && memcmp(source, prefix, sizeof(prefix) - 1U) == 0) {
        return zend_string_init(
            source + sizeof(prefix) - 1U,
            source_length - (sizeof(prefix) - 1U),
            false
        );
    }
    return zend_string_init(source, source_length, false);
}
