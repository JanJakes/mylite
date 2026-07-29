#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "mysqli_extension.h"
#include "php_mylite_native_value.h"

#include <stdlib.h>
#include <time.h>

static bool mylite_mysqli_execute_sql(
    mylite_mysqli_link *link,
    const char *sql,
    size_t sql_length,
    zval *out_result
);
static bool mylite_mysqli_execute_cursor_sql(
    mylite_mysqli_link *link,
    const char *sql,
    size_t sql_length,
    zval *out_result
);
static bool mylite_mysqli_execute_buffered_cursor_sql(
    mylite_mysqli_link *link,
    const char *sql,
    size_t sql_length,
    zval *out_result
);
static bool mylite_mysqli_prepare_pending_cursor_sql(
    mylite_mysqli_link *link,
    const char *sql,
    size_t sql_length
);
static bool mylite_mysqli_link_take_pending_cursor_result(
    mylite_mysqli_link *link,
    bool unbuffered,
    zval *out_result
);
static bool mylite_mysqli_stmt_require_ready(mylite_mysqli_stmt *stmt, mylite_mysqli_link *link);
static void mylite_mysqli_link_set_direct_pending(mylite_mysqli_link *link);
static void mylite_mysqli_link_set_prepared_owner(
    mylite_mysqli_link *link,
    mylite_mysqli_stmt *stmt
);
static void mylite_mysqli_link_release_owner(mylite_mysqli_link *link, const void *owner);
static void mylite_mysqli_link_cancel_active_owner(mylite_mysqli_link *link);
static void mylite_mysqli_result_attach_connection(
    mylite_mysqli_result *result,
    mylite_mysqli_link *link
);
static void mylite_mysqli_result_release_connection(mylite_mysqli_result *result);
static bool mylite_mysqli_link_check_ready(mylite_mysqli_link *link, bool report_error);
static bool mylite_mysqli_execute_transaction_control_statement(
    mylite_mysqli_link *link,
    const char *sql,
    size_t sql_length,
    zval *out_result,
    bool *out_handled
);
static uint64_t mylite_mysqli_profile_now_ns(void);
static uint64_t mylite_mysqli_profile_elapsed_ns(uint64_t start_ns);
static void mylite_mysqli_profile_record(
    const char *sql,
    size_t sql_length,
    bool bridge,
    bool error,
    uint64_t execute_ns,
    uint64_t buffer_ns,
    size_t row_count,
    size_t column_count
);
static mylite_mysqli_profile_slot *mylite_mysqli_profile_slot_for(
    const char *sql,
    size_t sql_length
);
static void mylite_mysqli_profile_normalize_sql(
    const char *sql,
    size_t sql_length,
    char out_sql[MYLITE_MYSQLI_PROFILE_SQL_LENGTH]
);
static bool mylite_mysqli_reject_oversized_packet(mylite_mysqli_link *link, size_t sql_length);
static bool mylite_mysqli_buffer_result(
    mylite_mysqli_link *link,
    const mylite_result *source,
    zval *out_result
);
static bool mylite_mysqli_create_cursor_result(
    mylite_mysqli_link *link,
    mylite_stmt *native_stmt,
    zval *out_result
);
static bool mylite_mysqli_buffer_cursor_result(
    mylite_mysqli_link *link,
    mylite_stmt *native_stmt,
    zval *out_result,
    bool finalize_statement,
    bool current_row_pending,
    bool native_types
);
static void mylite_mysqli_capture_result_warnings(
    mylite_mysqli_link *link,
    const mylite_result *result
);
static void mylite_mysqli_capture_stmt_warnings(
    mylite_mysqli_link *link,
    mylite_mysqli_stmt *stmt,
    const mylite_stmt *native_stmt
);
static void mylite_mysqli_clear_warning_records(
    struct mylite_diagnostic **records,
    size_t *record_count
);
static bool mylite_mysqli_initialize_warning_object(
    const struct mylite_diagnostic *records,
    size_t record_count,
    zval *out_warning
);
static void mylite_mysqli_update_warning_properties(mylite_mysqli_warning *warning);
static bool mylite_mysqli_result_reserve_rows(
    mylite_mysqli_link *link,
    mylite_mysqli_result *result,
    uint32_t row_capacity
);
static void mylite_mysqli_fill_field_from_stmt(
    mylite_mysqli_field *field,
    const mylite_stmt *native_stmt,
    uint32_t column
);
static void mylite_mysqli_link_clear_pending_result(mylite_mysqli_link *link);
static void mylite_mysqli_link_clear_last_result(mylite_mysqli_link *link);
static void mylite_mysqli_result_close_cursor(mylite_mysqli_result *result);
static void mylite_mysqli_result_clear_current_row(mylite_mysqli_result *result);
static int mylite_mysqli_result_read_cursor_row(mylite_mysqli_result *result);
static int mylite_mysqli_result_next_row(mylite_mysqli_result *result, zval **out_row_values);
static int mylite_mysqli_stmt_copy_current_row(mylite_mysqli_stmt *stmt);
static bool mylite_mysqli_stmt_buffer_current_result(mylite_mysqli_stmt *stmt);
static void mylite_mysqli_stmt_discard_current_result(
    mylite_mysqli_stmt *stmt,
    bool reset_native_statement
);
static zend_string *mylite_mysqli_column_string(const char *value);
static int mylite_mysqli_column_type(enum mylite_result_column_type type);
static bool mylite_mysqli_bind_statement_parameters(mylite_mysqli_stmt *stmt, zval *params);
static bool mylite_mysqli_initialize_execute_packet_size(
    mylite_mysqli_link *link,
    size_t parameter_count,
    size_t *out_packet_size
);
static bool mylite_mysqli_bind_native_value(
    mylite_mysqli_link *link,
    mylite_stmt *native_stmt,
    size_t index,
    zval *value,
    char type,
    const zend_string *long_data,
    size_t *packet_size
);
static bool mylite_mysqli_add_execute_packet_value(
    mylite_mysqli_link *link,
    size_t *packet_size,
    size_t value_size,
    bool has_length_prefix
);
static size_t mylite_mysqli_length_encoded_integer_size(size_t value);
static void mylite_mysqli_set_packet_too_large_error(mylite_mysqli_link *link);
static bool mylite_mysqli_capture_stmt_status(
    mylite_mysqli_stmt *stmt,
    mylite_mysqli_link *link,
    int status,
    const char *fallback
);
static bool mylite_mysqli_capture_link_status(
    mylite_mysqli_link *link,
    int status,
    const char *fallback
);
static void mylite_mysqli_clear_stmt_long_data(mylite_mysqli_stmt *stmt);
static void mylite_mysqli_clear_stmt_bindings(mylite_mysqli_stmt *stmt);
static bool mylite_mysqli_sql_may_stream_select(const char *sql, size_t length);
static bool mylite_mysqli_sql_keyword_at(
    const char *sql,
    size_t length,
    size_t offset,
    const char *keyword
);
static bool mylite_mysqli_match_transaction_control_statement(
    const char *sql,
    size_t sql_length,
    enum mylite_transaction_control_statement *out_statement
);
static bool mylite_mysqli_consume_statement_end(const char **cursor, const char *end);
static bool mylite_mysqli_set_link_info(mylite_mysqli_link *link, const char *info);
static const char *mylite_mysqli_skip_space(const char *cursor, const char *end);
static bool mylite_mysqli_consume_keyword(
    const char **cursor,
    const char *end,
    const char *keyword
);
static bool mylite_mysqli_is_identifier_char(unsigned char ch);
static bool mylite_mysqli_is_line_comment_terminator(char ch);
static zend_string *mylite_mysqli_resolve_path(
    const char *host,
    size_t host_length,
    const char *database,
    size_t database_length,
    const char *socket,
    size_t socket_length,
    bool *out_memory,
    bool *out_use_database
);
static const char *mylite_mysqli_host_path(
    const char *host,
    size_t host_length,
    size_t *out_path_length
);
static bool mylite_mysqli_is_path_like(const char *value, size_t length);
static bool mylite_mysqli_is_local_path(const char *value, size_t length);
static void mylite_mysqli_clear_error(mylite_mysqli_link *link);
static void mylite_mysqli_clear_stmt_error(mylite_mysqli_stmt *stmt);
static int mylite_mysqli_error_from_status(int status, const char **out_sqlstate);
static void mylite_mysqli_set_connect_error(
    mylite_mysqli_link *link,
    int error_code,
    const char *sqlstate,
    const char *message
);
static void mylite_mysqli_set_global_connect_error(int error_code, const char *message);

zend_object *mylite_mysqli_link_create(zend_class_entry *class_entry) {
    mylite_mysqli_link *link = zend_object_alloc(sizeof(*link), class_entry);

    zend_object_std_init(&link->std, class_entry);
    object_properties_init(&link->std, class_entry);
    link->std.handlers = &mylite_mysqli_link_handlers;
    link->error = zend_empty_string;
    zend_string_addref(link->error);
    memcpy(link->sqlstate, "00000", sizeof(link->sqlstate));
    link->affected_rows = -1;
    ZVAL_UNDEF(&link->last_result);
    link->pending_stmt = NULL;
    link->pending_sql = NULL;
    link->warning_records = NULL;
    link->pending_execute_ns = 0U;
    link->warning_record_count = 0U;
    link->state = MYLITE_MYSQLI_CONNECTION_READY;
    link->active_owner = NULL;
    mylite_mysqli_update_link_properties(link);
    return &link->std;
}

void mylite_mysqli_link_free(zend_object *object) {
    mylite_mysqli_link *link = mylite_mysqli_link_from_obj(object);

    mylite_mysqli_link_cancel_active_owner(link);
    mylite_mysqli_link_clear_pending_result(link);
    mylite_mysqli_link_clear_last_result(link);
    if (link->database != NULL) {
        mylite_close(link->database);
    }
    if (link->path != NULL) {
        zend_string_release(link->path);
    }
    zend_string_release(link->error);
    if (link->info != NULL) {
        zend_string_release(link->info);
    }
    mylite_mysqli_clear_warning_records(&link->warning_records, &link->warning_record_count);
    zend_object_std_dtor(&link->std);
}

zend_object *mylite_mysqli_result_create(zend_class_entry *class_entry) {
    mylite_mysqli_result *result = zend_object_alloc(sizeof(*result), class_entry);

    zend_object_std_init(&result->std, class_entry);
    object_properties_init(&result->std, class_entry);
    result->std.handlers = &mylite_mysqli_result_handlers;
    ZVAL_UNDEF(&result->link);
    mylite_mysqli_update_result_properties(result);
    return &result->std;
}

void mylite_mysqli_result_free(zend_object *object) {
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(object);

    for (uint32_t column = 0; column < result->column_count; column++) {
        mylite_mysqli_field *field = result->fields == NULL ? NULL : &result->fields[column];

        if (field == NULL) {
            continue;
        }
        if (field->name != NULL) {
            zend_string_release(field->name);
        }
        if (field->schema != NULL) {
            zend_string_release(field->schema);
        }
        if (field->table != NULL) {
            zend_string_release(field->table);
        }
        if (field->origin_table != NULL) {
            zend_string_release(field->origin_table);
        }
        if (field->origin_name != NULL) {
            zend_string_release(field->origin_name);
        }
    }
    mylite_mysqli_result_close_cursor(result);
    if (!result->unbuffered) {
        size_t value_count = (size_t)result->row_count * result->column_count;

        for (size_t index = 0; index < value_count; index++) {
            zval_ptr_dtor(&result->values[index]);
        }
    }
    efree(result->fields);
    efree(result->values);
    zend_object_std_dtor(&result->std);
}

zend_object *mylite_mysqli_stmt_create(zend_class_entry *class_entry) {
    mylite_mysqli_stmt *stmt = zend_object_alloc(sizeof(*stmt), class_entry);

    zend_object_std_init(&stmt->std, class_entry);
    object_properties_init(&stmt->std, class_entry);
    stmt->std.handlers = &mylite_mysqli_stmt_handlers;
    stmt->error = zend_empty_string;
    zend_string_addref(stmt->error);
    stmt->affected_rows = -1;
    memcpy(stmt->sqlstate, "00000", sizeof(stmt->sqlstate));
    ZVAL_UNDEF(&stmt->link);
    ZVAL_UNDEF(&stmt->result);
    stmt->native_stmt = NULL;
    stmt->long_data = NULL;
    stmt->warning_records = NULL;
    stmt->warning_record_count = 0U;
    stmt->current_row_pending = false;
    mylite_mysqli_update_stmt_properties(stmt);
    return &stmt->std;
}

void mylite_mysqli_stmt_free(zend_object *object) {
    mylite_mysqli_stmt *stmt = mylite_mysqli_stmt_from_obj(object);

    (void)mylite_mysqli_stmt_close_internal(stmt);
    if (!Z_ISUNDEF(stmt->link)) {
        zval_ptr_dtor(&stmt->link);
    }
    if (stmt->bound_results != NULL) {
        for (uint32_t index = 0; index < stmt->bound_result_count; index++) {
            zval_ptr_dtor(&stmt->bound_results[index]);
        }
        efree(stmt->bound_results);
    }
    zend_string_release(stmt->error);
    zend_object_std_dtor(&stmt->std);
}

zend_object *mylite_mysqli_warning_create(zend_class_entry *class_entry) {
    mylite_mysqli_warning *warning = zend_object_alloc(sizeof(*warning), class_entry);

    zend_object_std_init(&warning->std, class_entry);
    object_properties_init(&warning->std, class_entry);
    warning->records = NULL;
    warning->record_count = 0U;
    warning->index = 0U;
    warning->std.handlers = &mylite_mysqli_warning_handlers;
    return &warning->std;
}

void mylite_mysqli_warning_free(zend_object *object) {
    mylite_mysqli_warning *warning = mylite_mysqli_warning_from_obj(object);

    mylite_mysqli_clear_warning_records(&warning->records, &warning->record_count);
    zend_object_std_dtor(object);
}

mylite_mysqli_link *mylite_mysqli_link_from_obj(zend_object *object) {
    return (mylite_mysqli_link *)((char *)object - XtOffsetOf(mylite_mysqli_link, std));
}

mylite_mysqli_result *mylite_mysqli_result_from_obj(zend_object *object) {
    return (mylite_mysqli_result *)((char *)object - XtOffsetOf(mylite_mysqli_result, std));
}

mylite_mysqli_stmt *mylite_mysqli_stmt_from_obj(zend_object *object) {
    return (mylite_mysqli_stmt *)((char *)object - XtOffsetOf(mylite_mysqli_stmt, std));
}

mylite_mysqli_warning *mylite_mysqli_warning_from_obj(zend_object *object) {
    return (mylite_mysqli_warning *)((char *)object - XtOffsetOf(mylite_mysqli_warning, std));
}

bool mylite_mysqli_link_get_warnings(mylite_mysqli_link *link, zval *out_warning) {
    if (link == NULL || link->database == NULL) {
        return false;
    }
    return mylite_mysqli_initialize_warning_object(
        link->warning_records,
        link->warning_record_count,
        out_warning
    );
}

bool mylite_mysqli_stmt_get_warnings(mylite_mysqli_stmt *stmt, zval *out_warning) {
    if (stmt == NULL || stmt->native_stmt == NULL) {
        return false;
    }
    return mylite_mysqli_initialize_warning_object(
        stmt->warning_records,
        stmt->warning_record_count,
        out_warning
    );
}

bool mylite_mysqli_warning_next_internal(mylite_mysqli_warning *warning) {
    if (warning == NULL || warning->index >= warning->record_count ||
        warning->index + 1U >= warning->record_count) {
        return false;
    }

    ++warning->index;
    mylite_mysqli_update_warning_properties(warning);
    return true;
}

bool mylite_mysqli_connect_link(
    mylite_mysqli_link *link,
    const char *host,
    size_t host_length,
    const char *database,
    size_t database_length,
    const char *socket,
    size_t socket_length,
    zend_long port,
    bool port_is_null,
    zend_long flags
) {
    static const zend_long default_port = 3306;
    static const zend_long supported_client_flags = MYLITE_MYSQLI_CLIENT_FOUND_ROWS;
    struct mylite_open_diagnostic diagnostic;
    bool memory = false;
    bool use_database = false;
    zend_string *path = NULL;
    int status = MYLITE_OK;

    mylite_mysqli_clear_error(link);
    if ((!port_is_null && port != 0 && port != default_port) ||
        (flags & ~supported_client_flags) != 0) {
        mylite_mysqli_set_connect_error(
            link,
            MYLITE_MYSQLI_ERROR_UNSUPPORTED,
            "42000",
            "non-default network ports and unsupported client flags are not supported by the "
            "embedded driver"
        );
        return false;
    }
    path = mylite_mysqli_resolve_path(
        host,
        host_length,
        database,
        database_length,
        socket,
        socket_length,
        &memory,
        &use_database
    );
    if (path == NULL) {
        mylite_mysqli_set_connect_error(link, MYLITE_MYSQLI_ERROR_CLIENT, "HY000", "out of memory");
        return false;
    }
    if (memchr(ZSTR_VAL(path), '\0', ZSTR_LEN(path)) != NULL) {
        zend_string_release(path);
        mylite_mysqli_set_connect_error(
            link,
            MYLITE_MYSQLI_ERROR_CONNECTION,
            "HY000",
            "MyLite database paths do not support NUL bytes"
        );
        return false;
    }

    if (link->database != NULL) {
        mylite_mysqli_link_cancel_active_owner(link);
        mylite_mysqli_link_clear_pending_result(link);
        mylite_mysqli_link_clear_last_result(link);
        mylite_close(link->database);
        link->database = NULL;
    }
    link->connected = false;
    if (link->path != NULL) {
        zend_string_release(link->path);
    }
    link->path = zend_string_copy(path);

    status = memory ? mylite_open_memory_with_diagnostic(&link->database, &diagnostic)
                    : mylite_open_with_size_and_diagnostic(
                          ZSTR_VAL(path),
                          ZSTR_LEN(path),
                          &link->database,
                          &diagnostic
                      );
    zend_string_release(path);
    if (status != MYLITE_OK) {
        mylite_mysqli_set_connect_error(
            link,
            MYLITE_MYSQLI_ERROR_CONNECTION,
            diagnostic.sqlstate,
            diagnostic.message
        );
        return false;
    }
    status = mylite_set_client_found_rows(
        link->database,
        (flags & MYLITE_MYSQLI_CLIENT_FOUND_ROWS) != 0
    );
    if (status != MYLITE_OK) {
        mylite_close(link->database);
        link->database = NULL;
        mylite_mysqli_set_connect_error(
            link,
            MYLITE_MYSQLI_ERROR_CLIENT,
            "HY000",
            "could not configure client found-row behavior"
        );
        return false;
    }

    link->connected = true;
    mylite_mysqli_set_global_connect_error(0, "");
    if (use_database && database != NULL) {
        zend_string *quoted = mylite_mysqli_quote_identifier(database, database_length);
        zend_string *sql = zend_strpprintf(0, "USE %s", ZSTR_VAL(quoted));
        bool ok = mylite_mysqli_link_real_query(link, ZSTR_VAL(sql), ZSTR_LEN(sql));

        zend_string_release(sql);
        zend_string_release(quoted);
        if (!ok) {
            mylite_mysqli_set_global_connect_error(link->error_code, ZSTR_VAL(link->error));
            mylite_close(link->database);
            link->database = NULL;
            link->connected = false;
            mylite_mysqli_update_link_properties(link);
            return false;
        }
    }

    mylite_mysqli_update_link_properties(link);
    return true;
}

static void mylite_mysqli_set_connect_error(
    mylite_mysqli_link *link,
    int error_code,
    const char *sqlstate,
    const char *message
) {
    mylite_mysqli_set_error(link, error_code, sqlstate, message);
    mylite_mysqli_set_global_connect_error(link->error_code, ZSTR_VAL(link->error));
    mylite_mysqli_update_link_status_properties(link);
    mylite_mysqli_report_link_error(link);
}

bool mylite_mysqli_close_link(mylite_mysqli_link *link) {
    if (link->database != NULL) {
        int status = MYLITE_OK;

        mylite_mysqli_link_cancel_active_owner(link);
        mylite_mysqli_link_clear_pending_result(link);
        mylite_mysqli_link_clear_last_result(link);
        status = mylite_close_checked(link->database);
        if (status != MYLITE_OK) {
            (void
            )mylite_mysqli_capture_link_status(link, status, "could not close MyLite database");
            mylite_mysqli_update_link_properties(link);
            return false;
        }
        link->database = NULL;
    }
    link->connected = false;
    mylite_mysqli_update_link_properties(link);
    return true;
}

bool mylite_mysqli_link_query(
    mylite_mysqli_link *link,
    const char *sql,
    size_t sql_length,
    zend_long result_mode,
    zval *out_result
) {
    if (result_mode != MYLITE_MYSQLI_STORE_RESULT && result_mode != MYLITE_MYSQLI_USE_RESULT) {
        mylite_mysqli_set_error(
            link,
            MYLITE_MYSQLI_ERROR_UNSUPPORTED,
            "42000",
            "asynchronous and unknown mysqli query result modes are not supported"
        );
        mylite_mysqli_report_link_error(link);
        return false;
    }
    const bool use_cursor = result_mode == MYLITE_MYSQLI_USE_RESULT &&
                            mylite_mysqli_sql_may_stream_select(sql, sql_length);
    const bool use_buffered_cursor = result_mode == MYLITE_MYSQLI_STORE_RESULT &&
                                     mylite_mysqli_sql_may_stream_select(sql, sql_length);

    if (!mylite_mysqli_link_require_ready(link)) {
        return false;
    }
    if (use_cursor) {
        if (!mylite_mysqli_execute_cursor_sql(link, sql, sql_length, out_result)) {
            return false;
        }
    } else if (use_buffered_cursor) {
        if (!mylite_mysqli_execute_buffered_cursor_sql(link, sql, sql_length, out_result)) {
            return false;
        }
    } else if (!mylite_mysqli_execute_sql(link, sql, sql_length, out_result)) {
        return false;
    }

    mylite_mysqli_link_clear_last_result(link);
    if (Z_TYPE_P(out_result) == IS_OBJECT) {
        if (result_mode == MYLITE_MYSQLI_USE_RESULT) {
            mylite_mysqli_result_attach_connection(
                mylite_mysqli_result_from_obj(Z_OBJ_P(out_result)),
                link
            );
        } else {
            ZVAL_COPY(&link->last_result, out_result);
        }
    }
    return true;
}

bool mylite_mysqli_link_autocommit(mylite_mysqli_link *link, bool enable) {
    static const char disable_sql[] = "SET autocommit = 0";
    static const char enable_sql[] = "SET autocommit = 1";
    const char *sql = enable ? enable_sql : disable_sql;
    size_t sql_length = enable ? sizeof(enable_sql) - 1U : sizeof(disable_sql) - 1U;

    return mylite_mysqli_link_real_query(link, sql, sql_length);
}

bool mylite_mysqli_link_real_query(mylite_mysqli_link *link, const char *sql, size_t sql_length) {
    zval result;
    bool ok = true;

    if (!mylite_mysqli_link_require_ready(link)) {
        return false;
    }
    mylite_mysqli_link_clear_last_result(link);
    if (mylite_mysqli_sql_may_stream_select(sql, sql_length)) {
        if (mylite_mysqli_prepare_pending_cursor_sql(link, sql, sql_length)) {
            mylite_mysqli_link_set_direct_pending(link);
            return true;
        }
        return false;
    }

    ok = mylite_mysqli_execute_sql(link, sql, sql_length, &result);
    if (!ok) {
        return false;
    }

    if (Z_TYPE(result) == IS_OBJECT) {
        ZVAL_COPY_VALUE(&link->last_result, &result);
        mylite_mysqli_link_set_direct_pending(link);
    } else {
        zval_ptr_dtor(&result);
    }
    return true;
}

bool mylite_mysqli_link_store_result(mylite_mysqli_link *link, zval *out_result) {
    ZVAL_UNDEF(out_result);
    if (link->state != MYLITE_MYSQLI_CONNECTION_DIRECT_PENDING) {
        return false;
    }
    if (link->pending_stmt != NULL) {
        return mylite_mysqli_link_take_pending_cursor_result(link, false, out_result);
    }
    if (Z_TYPE(link->last_result) == IS_OBJECT) {
        ZVAL_COPY(out_result, &link->last_result);
        mylite_mysqli_link_release_owner(link, NULL);
        mylite_mysqli_clear_error(link);
        mylite_mysqli_update_link_status_properties(link);
        return true;
    }
    return false;
}

bool mylite_mysqli_link_use_result(mylite_mysqli_link *link, zval *out_result) {
    ZVAL_UNDEF(out_result);
    if (link->state != MYLITE_MYSQLI_CONNECTION_DIRECT_PENDING) {
        return false;
    }
    if (link->pending_stmt != NULL) {
        return mylite_mysqli_link_take_pending_cursor_result(link, true, out_result);
    }
    if (Z_TYPE(link->last_result) == IS_OBJECT) {
        ZVAL_COPY(out_result, &link->last_result);
        mylite_mysqli_link_clear_last_result(link);
        mylite_mysqli_result_attach_connection(
            mylite_mysqli_result_from_obj(Z_OBJ_P(out_result)),
            link
        );
        mylite_mysqli_clear_error(link);
        mylite_mysqli_update_link_status_properties(link);
        return true;
    }
    return false;
}

bool mylite_mysqli_stmt_prepare_internal(
    mylite_mysqli_stmt *stmt,
    const char *sql,
    size_t sql_length
) {
    mylite_mysqli_link *link = NULL;
    mylite_stmt *native_stmt = NULL;
    int status = MYLITE_OK;

    if (Z_ISUNDEF(stmt->link) || Z_TYPE(stmt->link) != IS_OBJECT) {
        mylite_mysqli_set_stmt_error(
            stmt,
            MYLITE_MYSQLI_ERROR_CLIENT,
            "HY000",
            "statement has no connection"
        );
        mylite_mysqli_report_stmt_error(stmt);
        return false;
    }
    link = mylite_mysqli_link_from_obj(Z_OBJ(stmt->link));
    if (link->database == NULL) {
        mylite_mysqli_set_stmt_error(
            stmt,
            MYLITE_MYSQLI_ERROR_CONNECTION,
            "HY000",
            "mysqli object is not connected"
        );
        mylite_mysqli_report_stmt_error(stmt);
        return false;
    }
    if (!mylite_mysqli_stmt_require_ready(stmt, link)) {
        return false;
    }
    if (link->state == MYLITE_MYSQLI_CONNECTION_PREPARED_UNBUFFERED && link->active_owner == stmt) {
        mylite_mysqli_stmt_discard_current_result(stmt, true);
    }

    mylite_mysqli_clear_error(link);
    status = mylite_prepare(link->database, sql, sql_length, &native_stmt);
    if (status != MYLITE_OK) {
        return mylite_mysqli_capture_stmt_status(stmt, link, status, "could not prepare statement");
    }
    if (mylite_stmt_parameter_count(native_stmt) > (size_t)UINT32_MAX) {
        (void)mylite_stmt_finalize(native_stmt);
        mylite_mysqli_set_stmt_error(
            stmt,
            MYLITE_MYSQLI_ERROR_CLIENT,
            "HY000",
            "statement has too many parameters"
        );
        mylite_mysqli_report_stmt_error(stmt);
        return false;
    }

    if (stmt->native_stmt != NULL) {
        (void)mylite_stmt_finalize(stmt->native_stmt);
    }
    stmt->native_stmt = native_stmt;
    mylite_mysqli_clear_stmt_bindings(stmt);
    if (!Z_ISUNDEF(stmt->result)) {
        zval_ptr_dtor(&stmt->result);
        ZVAL_UNDEF(&stmt->result);
    }
    if (stmt->query != NULL) {
        zend_string_release(stmt->query);
    }
    stmt->query = zend_string_init(sql, sql_length, false);
    stmt->param_count = (uint32_t)mylite_stmt_parameter_count(native_stmt);
    if (stmt->param_count != 0U) {
        stmt->long_data = ecalloc(stmt->param_count, sizeof(*stmt->long_data));
    }
    stmt->affected_rows = -1;
    stmt->insert_id = 0;
    stmt->num_rows = 0;
    stmt->field_count = (zend_long)mylite_stmt_column_count(native_stmt);
    mylite_mysqli_clear_warning_records(&stmt->warning_records, &stmt->warning_record_count);
    mylite_mysqli_clear_stmt_error(stmt);
    mylite_mysqli_update_stmt_properties(stmt);
    return true;
}

bool mylite_mysqli_stmt_execute_internal(mylite_mysqli_stmt *stmt, zval *params) {
    mylite_mysqli_link *link = NULL;
    size_t column_count = 0U;
    int status = MYLITE_OK;

    if (Z_ISUNDEF(stmt->link) || Z_TYPE(stmt->link) != IS_OBJECT || stmt->native_stmt == NULL) {
        mylite_mysqli_set_stmt_error(
            stmt,
            MYLITE_MYSQLI_ERROR_CLIENT,
            "HY000",
            "statement is not prepared"
        );
        mylite_mysqli_report_stmt_error(stmt);
        return false;
    }

    link = mylite_mysqli_link_from_obj(Z_OBJ(stmt->link));
    if (link->database == NULL) {
        mylite_mysqli_set_stmt_error(
            stmt,
            MYLITE_MYSQLI_ERROR_CONNECTION,
            "HY000",
            "mysqli object is not connected"
        );
        mylite_mysqli_report_stmt_error(stmt);
        return false;
    }
    if (!mylite_mysqli_stmt_require_ready(stmt, link)) {
        return false;
    }
    mylite_mysqli_stmt_discard_current_result(
        stmt,
        link->state == MYLITE_MYSQLI_CONNECTION_PREPARED_UNBUFFERED && link->active_owner == stmt
    );

    mylite_mysqli_clear_error(link);
    mylite_mysqli_link_clear_last_result(link);
    status = mylite_stmt_reset(stmt->native_stmt);
    if (status != MYLITE_OK) {
        return mylite_mysqli_capture_stmt_status(stmt, link, status, "could not reset statement");
    }
    mylite_mysqli_capture_stmt_warnings(link, stmt, stmt->native_stmt);
    if (params != NULL) {
        status = mylite_stmt_clear_bindings(stmt->native_stmt);
        if (status != MYLITE_OK) {
            return mylite_mysqli_capture_stmt_status(
                stmt,
                link,
                status,
                "could not clear statement bindings"
            );
        }
    }
    if (!mylite_mysqli_bind_statement_parameters(stmt, params)) {
        mylite_mysqli_report_stmt_error(stmt);
        return false;
    }
    mylite_mysqli_clear_stmt_long_data(stmt);

    status = mylite_stmt_step(stmt->native_stmt);
    if (status != MYLITE_ROW && status != MYLITE_DONE) {
        (void)mylite_mysqli_capture_stmt_status(stmt, link, status, "could not execute statement");
        return false;
    }

    column_count = mylite_stmt_column_count(stmt->native_stmt);
    if (column_count != 0U) {
        stmt->current_row_pending = status == MYLITE_ROW;
        stmt->num_rows = 0;
        stmt->field_count = (zend_long)column_count;
        stmt->affected_rows = -1;
        stmt->insert_id = 0;
        link->field_count = (zend_long)column_count;
        link->warning_count = 0;
        link->affected_rows = -1;
        link->insert_id = 0;
        if (!mylite_mysqli_set_link_info(link, NULL)) {
            mylite_mysqli_stmt_discard_current_result(stmt, true);
            mylite_mysqli_set_error(link, MYLITE_MYSQLI_ERROR_CLIENT, "HY000", "out of memory");
            mylite_mysqli_set_stmt_error(
                stmt,
                MYLITE_MYSQLI_ERROR_CLIENT,
                "HY000",
                "out of memory"
            );
            mylite_mysqli_report_stmt_error(stmt);
            return false;
        }
        mylite_mysqli_link_set_prepared_owner(link, stmt);
    } else {
        stmt->current_row_pending = false;
        stmt->num_rows = 0;
        stmt->field_count = 0;
        stmt->affected_rows = (zend_long)mylite_stmt_affected_rows(stmt->native_stmt);
        stmt->insert_id = (zend_long)mylite_stmt_insert_id(stmt->native_stmt);
        link->field_count = 0;
        link->warning_count = 0;
        link->affected_rows = stmt->affected_rows;
        link->insert_id = stmt->insert_id;
        if (!mylite_mysqli_set_link_info(link, mylite_stmt_info(stmt->native_stmt))) {
            mylite_mysqli_set_error(link, MYLITE_MYSQLI_ERROR_CLIENT, "HY000", "out of memory");
            mylite_mysqli_set_stmt_error(
                stmt,
                MYLITE_MYSQLI_ERROR_CLIENT,
                "HY000",
                "out of memory"
            );
            mylite_mysqli_report_stmt_error(stmt);
            return false;
        }
    }
    mylite_mysqli_capture_stmt_warnings(link, stmt, stmt->native_stmt);
    mylite_mysqli_update_link_status_properties(link);
    mylite_mysqli_clear_stmt_error(stmt);
    mylite_mysqli_update_stmt_properties(stmt);
    return true;
}

int mylite_mysqli_stmt_fetch_internal(mylite_mysqli_stmt *stmt) {
    mylite_mysqli_link *link = NULL;
    int status = MYLITE_DONE;

    if (stmt->native_stmt == NULL || Z_ISUNDEF(stmt->link) || Z_TYPE(stmt->link) != IS_OBJECT) {
        return MYLITE_ERROR;
    }
    if (Z_TYPE(stmt->result) == IS_OBJECT) {
        mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ(stmt->result));

        if (result->unbuffered || result->cursor >= result->row_count) {
            return result->unbuffered ? MYLITE_ERROR : MYLITE_DONE;
        }
        if (stmt->bound_result_count != result->column_count) {
            mylite_mysqli_set_stmt_error(
                stmt,
                MYLITE_MYSQLI_ERROR_CLIENT,
                "HY000",
                "result variable count mismatch"
            );
            mylite_mysqli_report_stmt_error(stmt);
            return MYLITE_ERROR;
        }
        for (uint32_t column = 0; column < result->column_count; column++) {
            zval *target = &stmt->bound_results[column];
            zval *target_value = target;
            zval *source = &result->values[result->cursor * result->column_count + column];
            zval copy;

            ZVAL_COPY(&copy, source);
            ZVAL_DEREF(target_value);
            zval_ptr_dtor(target_value);
            ZVAL_COPY_VALUE(target_value, &copy);
        }
        result->cursor++;
        return MYLITE_ROW;
    }

    link = mylite_mysqli_link_from_obj(Z_OBJ(stmt->link));
    if (link->state != MYLITE_MYSQLI_CONNECTION_PREPARED_UNBUFFERED || link->active_owner != stmt) {
        return MYLITE_ERROR;
    }
    if (stmt->bound_result_count != (uint32_t)stmt->field_count) {
        mylite_mysqli_set_stmt_error(
            stmt,
            MYLITE_MYSQLI_ERROR_CLIENT,
            "HY000",
            "result variable count mismatch"
        );
        mylite_mysqli_report_stmt_error(stmt);
        return MYLITE_ERROR;
    }

    status = stmt->current_row_pending ? MYLITE_ROW : mylite_stmt_step(stmt->native_stmt);
    if (status == MYLITE_ROW) {
        stmt->current_row_pending = false;
        if (mylite_mysqli_stmt_copy_current_row(stmt) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        stmt->num_rows++;
        mylite_mysqli_update_stmt_properties(stmt);
        return MYLITE_ROW;
    }
    if (status == MYLITE_DONE) {
        mylite_mysqli_capture_stmt_warnings(link, stmt, stmt->native_stmt);
        mylite_mysqli_link_release_owner(link, stmt);
        mylite_mysqli_update_stmt_properties(stmt);
        return MYLITE_DONE;
    }

    (void)mylite_mysqli_capture_stmt_status(stmt, link, status, "could not fetch statement row");
    mylite_mysqli_stmt_discard_current_result(stmt, true);
    return MYLITE_ERROR;
}

bool mylite_mysqli_stmt_get_result_internal(mylite_mysqli_stmt *stmt, zval *out_result) {
    ZVAL_UNDEF(out_result);
    if (!mylite_mysqli_stmt_store_result_internal(stmt) || Z_TYPE(stmt->result) != IS_OBJECT) {
        return false;
    }
    ZVAL_COPY(out_result, &stmt->result);
    return true;
}

bool mylite_mysqli_stmt_result_metadata_internal(mylite_mysqli_stmt *stmt, zval *out_result) {
    size_t column_count = 0U;

    ZVAL_UNDEF(out_result);
    if (stmt->native_stmt == NULL) {
        return false;
    }
    column_count = mylite_stmt_column_count(stmt->native_stmt);
    if (column_count == 0U || column_count > (size_t)UINT32_MAX) {
        return false;
    }

    object_init_ex(out_result, mylite_mysqli_result_ce);
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ_P(out_result));

    result->column_count = (uint32_t)column_count;
    result->fields = ecalloc(result->column_count, sizeof(*result->fields));
    for (uint32_t column = 0; column < result->column_count; column++) {
        mylite_mysqli_fill_field_from_stmt(&result->fields[column], stmt->native_stmt, column);
    }
    mylite_mysqli_update_result_properties(result);
    return true;
}

bool mylite_mysqli_stmt_store_result_internal(mylite_mysqli_stmt *stmt) {
    if (Z_TYPE(stmt->result) == IS_OBJECT) {
        return true;
    }
    if (stmt->native_stmt == NULL || Z_ISUNDEF(stmt->link) || Z_TYPE(stmt->link) != IS_OBJECT) {
        return false;
    }

    mylite_mysqli_link *link = mylite_mysqli_link_from_obj(Z_OBJ(stmt->link));

    if (link->state != MYLITE_MYSQLI_CONNECTION_PREPARED_UNBUFFERED || link->active_owner != stmt) {
        return stmt->field_count == 0;
    }
    return mylite_mysqli_stmt_buffer_current_result(stmt);
}

void mylite_mysqli_stmt_free_result_internal(mylite_mysqli_stmt *stmt) {
    mylite_mysqli_stmt_discard_current_result(stmt, true);
    stmt->num_rows = 0;
    mylite_mysqli_update_stmt_properties(stmt);
}

bool mylite_mysqli_stmt_reset_internal(mylite_mysqli_stmt *stmt) {
    if (stmt->native_stmt == NULL || Z_ISUNDEF(stmt->link) || Z_TYPE(stmt->link) != IS_OBJECT) {
        mylite_mysqli_set_stmt_error(
            stmt,
            MYLITE_MYSQLI_ERROR_CLIENT,
            "HY000",
            "statement is not prepared"
        );
        mylite_mysqli_report_stmt_error(stmt);
        return false;
    }

    mylite_mysqli_link *link = mylite_mysqli_link_from_obj(Z_OBJ(stmt->link));
    const int status = mylite_stmt_reset(stmt->native_stmt);
    if (status != MYLITE_OK) {
        return mylite_mysqli_capture_stmt_status(stmt, link, status, "could not reset statement");
    }
    mylite_mysqli_capture_stmt_warnings(link, stmt, stmt->native_stmt);
    mylite_mysqli_link_release_owner(link, stmt);
    stmt->current_row_pending = false;
    if (!Z_ISUNDEF(stmt->result)) {
        zval_ptr_dtor(&stmt->result);
        ZVAL_UNDEF(&stmt->result);
    }
    mylite_mysqli_clear_stmt_long_data(stmt);
    stmt->num_rows = 0;
    stmt->affected_rows = -1;
    mylite_mysqli_clear_stmt_error(stmt);
    mylite_mysqli_update_stmt_properties(stmt);
    return true;
}

bool mylite_mysqli_stmt_close_internal(mylite_mysqli_stmt *stmt) {
    mylite_mysqli_stmt_discard_current_result(stmt, false);
    if (stmt->native_stmt != NULL) {
        (void)mylite_stmt_finalize(stmt->native_stmt);
        stmt->native_stmt = NULL;
    }
    mylite_mysqli_clear_stmt_bindings(stmt);
    if (stmt->query != NULL) {
        zend_string_release(stmt->query);
        stmt->query = NULL;
    }
    stmt->param_count = 0U;
    stmt->field_count = 0;
    stmt->num_rows = 0;
    stmt->affected_rows = -1;
    mylite_mysqli_clear_warning_records(&stmt->warning_records, &stmt->warning_record_count);
    mylite_mysqli_update_stmt_properties(stmt);
    return true;
}

bool mylite_mysqli_stmt_send_long_data_internal(
    mylite_mysqli_stmt *stmt,
    zend_long parameter,
    const char *data,
    size_t data_length
) {
    if (stmt->native_stmt == NULL || parameter < 0 || (uint64_t)parameter >= stmt->param_count) {
        mylite_mysqli_set_stmt_error(
            stmt,
            MYLITE_MYSQLI_ERROR_CLIENT,
            "HY000",
            "invalid statement parameter"
        );
        mylite_mysqli_report_stmt_error(stmt);
        return false;
    }

    zend_string *current = stmt->long_data[parameter];
    const size_t current_length = current == NULL ? 0U : ZSTR_LEN(current);
    zend_string *combined = zend_string_safe_alloc(1U, current_length, data_length, false);
    if (current_length != 0U) {
        memcpy(ZSTR_VAL(combined), ZSTR_VAL(current), current_length);
    }
    if (data_length != 0U) {
        memcpy(ZSTR_VAL(combined) + current_length, data, data_length);
    }
    ZSTR_VAL(combined)[current_length + data_length] = '\0';
    if (current != NULL) {
        zend_string_release(current);
    }
    stmt->long_data[parameter] = combined;
    return true;
}

bool mylite_mysqli_link_execute_query(
    mylite_mysqli_link *link,
    const char *sql,
    size_t sql_length,
    zval *params,
    zval *out_result
) {
    mylite_stmt *native_stmt = NULL;
    size_t packet_size = 0U;
    int status = MYLITE_OK;

    if (params == NULL) {
        return mylite_mysqli_link_query(
            link,
            sql,
            sql_length,
            MYLITE_MYSQLI_STORE_RESULT,
            out_result
        );
    }
    if (link->database == NULL) {
        mylite_mysqli_set_error(
            link,
            MYLITE_MYSQLI_ERROR_CONNECTION,
            "HY000",
            "mysqli object is not connected"
        );
        mylite_mysqli_report_link_error(link);
        return false;
    }
    if (!mylite_mysqli_link_require_ready(link)) {
        return false;
    }

    mylite_mysqli_clear_error(link);
    status = mylite_prepare(link->database, sql, sql_length, &native_stmt);
    if (status != MYLITE_OK) {
        return mylite_mysqli_capture_link_status(link, status, "could not prepare statement");
    }
    const size_t parameter_count = mylite_stmt_parameter_count(native_stmt);
    if (parameter_count != zend_hash_num_elements(Z_ARRVAL_P(params))) {
        (void)mylite_stmt_finalize(native_stmt);
        mylite_mysqli_set_error(
            link,
            MYLITE_MYSQLI_ERROR_CLIENT,
            "HY000",
            "parameter count mismatch"
        );
        mylite_mysqli_report_link_error(link);
        return false;
    }
    if (!mylite_mysqli_initialize_execute_packet_size(link, parameter_count, &packet_size)) {
        (void)mylite_stmt_finalize(native_stmt);
        mylite_mysqli_report_link_error(link);
        return false;
    }
    for (size_t index = 0U; index < parameter_count; ++index) {
        zval *value = zend_hash_index_find(Z_ARRVAL_P(params), index);

        if (value == NULL || !mylite_mysqli_bind_native_value(
                                 link,
                                 native_stmt,
                                 index,
                                 value,
                                 '\0',
                                 NULL,
                                 &packet_size
                             )) {
            (void)mylite_stmt_finalize(native_stmt);
            mylite_mysqli_report_link_error(link);
            return false;
        }
    }

    mylite_mysqli_link_clear_last_result(link);
    if (!mylite_mysqli_buffer_cursor_result(link, native_stmt, out_result, true, false, true)) {
        return false;
    }
    if (Z_TYPE_P(out_result) == IS_OBJECT) {
        ZVAL_COPY(&link->last_result, out_result);
    }
    mylite_mysqli_update_link_status_properties(link);
    return true;
}

static bool mylite_mysqli_execute_sql(
    mylite_mysqli_link *link,
    const char *sql,
    size_t sql_length,
    zval *out_result
) {
    mylite_result *source = NULL;
    const char *sqlstate = "HY000";
    uint64_t execute_start_ns = 0U;
    uint64_t execute_ns = 0U;
    uint64_t buffer_start_ns = 0U;
    uint64_t buffer_ns = 0U;
    size_t column_count = 0U;
    size_t row_count = 0U;
    bool profile_enabled = MYLITE_MYSQLI_G(profile_enabled);
    bool handled_transaction_control = false;
    int status = MYLITE_OK;

    ZVAL_UNDEF(out_result);
    if (link->database == NULL) {
        mylite_mysqli_set_error(
            link,
            MYLITE_MYSQLI_ERROR_CONNECTION,
            "HY000",
            "mysqli object is not connected"
        );
        mylite_mysqli_report_link_error(link);
        return false;
    }

    mylite_mysqli_clear_error(link);
    if (mylite_mysqli_reject_oversized_packet(link, sql_length)) {
        return false;
    }
    if (mylite_mysqli_execute_transaction_control_statement(
            link,
            sql,
            sql_length,
            out_result,
            &handled_transaction_control
        )) {
        return true;
    }
    if (handled_transaction_control) {
        return false;
    }
    if (profile_enabled) {
        execute_start_ns = mylite_mysqli_profile_now_ns();
    }
    status = mylite_execute(link->database, sql, sql_length, &source);
    if (profile_enabled) {
        execute_ns = mylite_mysqli_profile_elapsed_ns(execute_start_ns);
    }
    if (status != MYLITE_OK) {
        int error_code = mylite_errcode(link->database);

        if (profile_enabled) {
            mylite_mysqli_profile_record(sql, sql_length, false, true, execute_ns, 0U, 0U, 0U);
        }
        if (error_code == 0) {
            error_code = mylite_mysqli_error_from_status(status, &sqlstate);
        } else {
            sqlstate = mylite_sqlstate(link->database);
        }
        mylite_mysqli_set_error(link, error_code, sqlstate, mylite_errmsg(link->database));
        mylite_mysqli_report_link_error(link);
        return false;
    }

    row_count = mylite_result_row_count(source);
    column_count = mylite_result_column_count(source);
    if (profile_enabled) {
        buffer_start_ns = mylite_mysqli_profile_now_ns();
    }
    if (!mylite_mysqli_buffer_result(link, source, out_result)) {
        if (profile_enabled) {
            buffer_ns = mylite_mysqli_profile_elapsed_ns(buffer_start_ns);
            mylite_mysqli_profile_record(
                sql,
                sql_length,
                false,
                true,
                execute_ns,
                buffer_ns,
                row_count,
                column_count
            );
        }
        mylite_result_free(source);
        return false;
    }
    if (profile_enabled) {
        buffer_ns = mylite_mysqli_profile_elapsed_ns(buffer_start_ns);
        mylite_mysqli_profile_record(
            sql,
            sql_length,
            false,
            false,
            execute_ns,
            buffer_ns,
            row_count,
            column_count
        );
    }
    mylite_result_free(source);
    mylite_mysqli_update_link_status_properties(link);
    return true;
}

static bool mylite_mysqli_execute_transaction_control_statement(
    mylite_mysqli_link *link,
    const char *sql,
    size_t sql_length,
    zval *out_result,
    bool *out_handled
) {
    enum mylite_transaction_control_statement statement = MYLITE_TRANSACTION_CONTROL_START;
    mylite_result *source = NULL;
    const char *sqlstate = "HY000";
    uint64_t execute_start_ns = 0U;
    uint64_t execute_ns = 0U;
    uint64_t buffer_start_ns = 0U;
    uint64_t buffer_ns = 0U;
    size_t column_count = 0U;
    size_t row_count = 0U;
    bool profile_enabled = MYLITE_MYSQLI_G(profile_enabled);
    int status = MYLITE_OK;

    *out_handled = false;
    if (!mylite_mysqli_match_transaction_control_statement(sql, sql_length, &statement)) {
        return false;
    }
    *out_handled = true;

    if (profile_enabled) {
        execute_start_ns = mylite_mysqli_profile_now_ns();
    }
    status = mylite_execute_transaction_control(link->database, statement, &source);
    if (profile_enabled) {
        execute_ns = mylite_mysqli_profile_elapsed_ns(execute_start_ns);
    }
    if (status != MYLITE_OK) {
        int error_code = mylite_errcode(link->database);

        if (profile_enabled) {
            mylite_mysqli_profile_record(sql, sql_length, false, true, execute_ns, 0U, 0U, 0U);
        }
        if (error_code == 0) {
            error_code = mylite_mysqli_error_from_status(status, &sqlstate);
        } else {
            sqlstate = mylite_sqlstate(link->database);
        }
        mylite_mysqli_set_error(link, error_code, sqlstate, mylite_errmsg(link->database));
        mylite_mysqli_report_link_error(link);
        return false;
    }

    row_count = mylite_result_row_count(source);
    column_count = mylite_result_column_count(source);
    if (profile_enabled) {
        buffer_start_ns = mylite_mysqli_profile_now_ns();
    }
    if (!mylite_mysqli_buffer_result(link, source, out_result)) {
        if (profile_enabled) {
            buffer_ns = mylite_mysqli_profile_elapsed_ns(buffer_start_ns);
            mylite_mysqli_profile_record(
                sql,
                sql_length,
                false,
                true,
                execute_ns,
                buffer_ns,
                row_count,
                column_count
            );
        }
        mylite_result_free(source);
        return false;
    }
    if (profile_enabled) {
        buffer_ns = mylite_mysqli_profile_elapsed_ns(buffer_start_ns);
        mylite_mysqli_profile_record(
            sql,
            sql_length,
            false,
            false,
            execute_ns,
            buffer_ns,
            row_count,
            column_count
        );
    }
    mylite_result_free(source);
    mylite_mysqli_update_link_status_properties(link);
    return true;
}

static bool mylite_mysqli_execute_cursor_sql(
    mylite_mysqli_link *link,
    const char *sql,
    size_t sql_length,
    zval *out_result
) {
    mylite_stmt *native_stmt = NULL;
    const char *sqlstate = "HY000";
    uint64_t execute_start_ns = 0U;
    uint64_t execute_ns = 0U;
    size_t column_count = 0U;
    bool profile_enabled = MYLITE_MYSQLI_G(profile_enabled);
    int status = MYLITE_OK;

    ZVAL_UNDEF(out_result);
    if (link->database == NULL) {
        mylite_mysqli_set_error(
            link,
            MYLITE_MYSQLI_ERROR_CONNECTION,
            "HY000",
            "mysqli object is not connected"
        );
        mylite_mysqli_report_link_error(link);
        return false;
    }

    mylite_mysqli_clear_error(link);
    if (mylite_mysqli_reject_oversized_packet(link, sql_length)) {
        return false;
    }
    if (profile_enabled) {
        execute_start_ns = mylite_mysqli_profile_now_ns();
    }
    status = mylite_prepare(link->database, sql, sql_length, &native_stmt);
    if (profile_enabled) {
        execute_ns = mylite_mysqli_profile_elapsed_ns(execute_start_ns);
    }
    if (status != MYLITE_OK) {
        int error_code = mylite_errcode(link->database);

        if (profile_enabled) {
            mylite_mysqli_profile_record(sql, sql_length, false, true, execute_ns, 0U, 0U, 0U);
        }
        if (error_code == 0) {
            error_code = mylite_mysqli_error_from_status(status, &sqlstate);
        } else {
            sqlstate = mylite_sqlstate(link->database);
        }
        mylite_mysqli_set_error(link, error_code, sqlstate, mylite_errmsg(link->database));
        mylite_mysqli_report_link_error(link);
        return false;
    }

    column_count = mylite_stmt_column_count(native_stmt);
    if (!mylite_mysqli_create_cursor_result(link, native_stmt, out_result)) {
        if (profile_enabled) {
            mylite_mysqli_profile_record(
                sql,
                sql_length,
                false,
                true,
                execute_ns,
                0U,
                0U,
                column_count
            );
        }
        if (native_stmt != NULL) {
            (void)mylite_stmt_finalize(native_stmt);
        }
        return false;
    }
    if (profile_enabled) {
        mylite_mysqli_profile_record(
            sql,
            sql_length,
            false,
            false,
            execute_ns,
            0U,
            0U,
            column_count
        );
    }
    mylite_mysqli_update_link_status_properties(link);
    return true;
}

static bool mylite_mysqli_execute_buffered_cursor_sql(
    mylite_mysqli_link *link,
    const char *sql,
    size_t sql_length,
    zval *out_result
) {
    mylite_stmt *native_stmt = NULL;
    const char *sqlstate = "HY000";
    uint64_t execute_start_ns = 0U;
    uint64_t execute_ns = 0U;
    uint64_t buffer_start_ns = 0U;
    uint64_t buffer_ns = 0U;
    size_t column_count = 0U;
    size_t row_count = 0U;
    bool profile_enabled = MYLITE_MYSQLI_G(profile_enabled);
    int status = MYLITE_OK;

    ZVAL_UNDEF(out_result);
    if (link->database == NULL) {
        mylite_mysqli_set_error(
            link,
            MYLITE_MYSQLI_ERROR_CONNECTION,
            "HY000",
            "mysqli object is not connected"
        );
        mylite_mysqli_report_link_error(link);
        return false;
    }

    mylite_mysqli_clear_error(link);
    if (mylite_mysqli_reject_oversized_packet(link, sql_length)) {
        return false;
    }
    if (profile_enabled) {
        execute_start_ns = mylite_mysqli_profile_now_ns();
    }
    status = mylite_prepare(link->database, sql, sql_length, &native_stmt);
    if (profile_enabled) {
        execute_ns = mylite_mysqli_profile_elapsed_ns(execute_start_ns);
    }
    if (status != MYLITE_OK) {
        int error_code = mylite_errcode(link->database);

        if (profile_enabled) {
            mylite_mysqli_profile_record(sql, sql_length, false, true, execute_ns, 0U, 0U, 0U);
        }
        if (error_code == 0) {
            error_code = mylite_mysqli_error_from_status(status, &sqlstate);
        } else {
            sqlstate = mylite_sqlstate(link->database);
        }
        mylite_mysqli_set_error(link, error_code, sqlstate, mylite_errmsg(link->database));
        mylite_mysqli_report_link_error(link);
        return false;
    }

    column_count = mylite_stmt_column_count(native_stmt);
    if (profile_enabled) {
        buffer_start_ns = mylite_mysqli_profile_now_ns();
    }
    if (!mylite_mysqli_buffer_cursor_result(link, native_stmt, out_result, true, false, false)) {
        if (profile_enabled) {
            buffer_ns = mylite_mysqli_profile_elapsed_ns(buffer_start_ns);
            mylite_mysqli_profile_record(
                sql,
                sql_length,
                false,
                true,
                execute_ns,
                buffer_ns,
                0U,
                column_count
            );
        }
        return false;
    }
    if (Z_TYPE_P(out_result) == IS_OBJECT) {
        row_count = mylite_mysqli_result_from_obj(Z_OBJ_P(out_result))->row_count;
    }
    if (profile_enabled) {
        buffer_ns = mylite_mysqli_profile_elapsed_ns(buffer_start_ns);
        mylite_mysqli_profile_record(
            sql,
            sql_length,
            false,
            false,
            execute_ns,
            buffer_ns,
            row_count,
            column_count
        );
    }
    mylite_mysqli_update_link_status_properties(link);
    return true;
}

static bool mylite_mysqli_prepare_pending_cursor_sql(
    mylite_mysqli_link *link,
    const char *sql,
    size_t sql_length
) {
    mylite_stmt *native_stmt = NULL;
    uint64_t execute_start_ns = 0U;
    uint64_t execute_ns = 0U;
    size_t column_count = 0U;
    int status = MYLITE_OK;

    if (link->database == NULL) {
        mylite_mysqli_set_error(
            link,
            MYLITE_MYSQLI_ERROR_CONNECTION,
            "HY000",
            "mysqli object is not connected"
        );
        mylite_mysqli_report_link_error(link);
        return false;
    }

    mylite_mysqli_clear_error(link);
    if (mylite_mysqli_reject_oversized_packet(link, sql_length)) {
        return false;
    }

    if (MYLITE_MYSQLI_G(profile_enabled)) {
        execute_start_ns = mylite_mysqli_profile_now_ns();
    }
    status = mylite_prepare(link->database, sql, sql_length, &native_stmt);
    if (MYLITE_MYSQLI_G(profile_enabled)) {
        execute_ns = mylite_mysqli_profile_elapsed_ns(execute_start_ns);
    }
    if (status != MYLITE_OK) {
        if (MYLITE_MYSQLI_G(profile_enabled)) {
            mylite_mysqli_profile_record(sql, sql_length, false, true, execute_ns, 0U, 0U, 0U);
        }
        if (native_stmt != NULL) {
            (void)mylite_stmt_finalize(native_stmt);
        }
        return mylite_mysqli_capture_link_status(link, status, "could not prepare statement");
    }

    column_count = mylite_stmt_column_count(native_stmt);
    if (column_count > (size_t)UINT32_MAX) {
        mylite_mysqli_set_error(
            link,
            MYLITE_MYSQLI_ERROR_CLIENT,
            "HY000",
            "result set has too many columns"
        );
        mylite_mysqli_report_link_error(link);
        (void)mylite_stmt_finalize(native_stmt);
        return false;
    }

    link->field_count = (zend_long)column_count;
    link->warning_count = 0;
    link->affected_rows = -1;
    link->insert_id = 0;
    if (!mylite_mysqli_set_link_info(link, NULL)) {
        mylite_mysqli_set_error(link, MYLITE_MYSQLI_ERROR_CLIENT, "HY000", "out of memory");
        mylite_mysqli_report_link_error(link);
        (void)mylite_stmt_finalize(native_stmt);
        return false;
    }

    link->pending_stmt = native_stmt;
    link->pending_sql = zend_string_init(sql, sql_length, false);
    link->pending_execute_ns = execute_ns;
    mylite_mysqli_update_link_status_properties(link);
    return true;
}

static bool mylite_mysqli_link_take_pending_cursor_result(
    mylite_mysqli_link *link,
    bool unbuffered,
    zval *out_result
) {
    mylite_stmt *native_stmt = link->pending_stmt;
    zend_string *sql = link->pending_sql;
    uint64_t execute_ns = link->pending_execute_ns;
    uint64_t buffer_start_ns = 0U;
    uint64_t buffer_ns = 0U;
    size_t column_count = 0U;
    size_t row_count = 0U;
    bool ok = false;

    ZVAL_UNDEF(out_result);
    if (native_stmt == NULL) {
        return false;
    }

    link->pending_stmt = NULL;
    link->pending_sql = NULL;
    link->pending_execute_ns = 0U;
    column_count = mylite_stmt_column_count(native_stmt);
    if (unbuffered) {
        ok = mylite_mysqli_create_cursor_result(link, native_stmt, out_result);
        if (!ok) {
            (void)mylite_stmt_finalize(native_stmt);
        }
    } else {
        if (MYLITE_MYSQLI_G(profile_enabled)) {
            buffer_start_ns = mylite_mysqli_profile_now_ns();
        }
        ok = mylite_mysqli_buffer_cursor_result(link, native_stmt, out_result, true, false, false);
        if (MYLITE_MYSQLI_G(profile_enabled)) {
            buffer_ns = mylite_mysqli_profile_elapsed_ns(buffer_start_ns);
        }
        if (ok && Z_TYPE_P(out_result) == IS_OBJECT) {
            row_count = mylite_mysqli_result_from_obj(Z_OBJ_P(out_result))->row_count;
        }
    }

    if (MYLITE_MYSQLI_G(profile_enabled) && sql != NULL) {
        mylite_mysqli_profile_record(
            ZSTR_VAL(sql),
            ZSTR_LEN(sql),
            false,
            !ok,
            execute_ns,
            buffer_ns,
            row_count,
            column_count
        );
    }
    if (sql != NULL) {
        zend_string_release(sql);
    }
    if (!ok) {
        mylite_mysqli_link_release_owner(link, NULL);
        return false;
    }

    mylite_mysqli_clear_error(link);
    mylite_mysqli_link_clear_last_result(link);
    if (Z_TYPE_P(out_result) == IS_OBJECT) {
        if (unbuffered) {
            mylite_mysqli_result_attach_connection(
                mylite_mysqli_result_from_obj(Z_OBJ_P(out_result)),
                link
            );
        } else {
            ZVAL_COPY(&link->last_result, out_result);
            mylite_mysqli_link_release_owner(link, NULL);
        }
    } else {
        mylite_mysqli_link_release_owner(link, NULL);
    }
    mylite_mysqli_update_link_status_properties(link);
    return true;
}

static uint64_t mylite_mysqli_profile_now_ns(void) {
    if (!MYLITE_MYSQLI_G(profile_enabled)) {
        return 0U;
    }
#if defined(CLOCK_MONOTONIC)
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
        return (uint64_t)now.tv_sec * 1000000000ULL + (uint64_t)now.tv_nsec;
    }
#endif
    return 0U;
}

static uint64_t mylite_mysqli_profile_elapsed_ns(uint64_t start_ns) {
    uint64_t end_ns = 0U;

    if (start_ns == 0U) {
        return 0U;
    }
    end_ns = mylite_mysqli_profile_now_ns();
    return end_ns >= start_ns ? end_ns - start_ns : 0U;
}

static void mylite_mysqli_profile_record(
    const char *sql,
    size_t sql_length,
    bool bridge,
    bool error,
    uint64_t execute_ns,
    uint64_t buffer_ns,
    size_t row_count,
    size_t column_count
) {
    mylite_mysqli_profile_slot *slot = NULL;
    uint64_t cell_count = 0U;

    if (!MYLITE_MYSQLI_G(profile_enabled)) {
        return;
    }

    if (column_count != 0U && row_count > UINT64_MAX / column_count) {
        cell_count = UINT64_MAX;
    } else {
        cell_count = (uint64_t)row_count * (uint64_t)column_count;
    }

    MYLITE_MYSQLI_G(profile_calls)++;
    MYLITE_MYSQLI_G(profile_execute_ns) += execute_ns;
    MYLITE_MYSQLI_G(profile_buffer_ns) += buffer_ns;
    MYLITE_MYSQLI_G(profile_rows) += (uint64_t)row_count;
    MYLITE_MYSQLI_G(profile_cells) += cell_count;
    if (bridge) {
        MYLITE_MYSQLI_G(profile_bridge_calls)++;
    }
    if (error) {
        MYLITE_MYSQLI_G(profile_errors)++;
    }
    if (column_count > 0U) {
        MYLITE_MYSQLI_G(profile_result_sets)++;
    }

    slot = mylite_mysqli_profile_slot_for(sql, sql_length);
    slot->calls++;
    slot->execute_ns += execute_ns;
    slot->buffer_ns += buffer_ns;
    slot->rows += (uint64_t)row_count;
    slot->cells += cell_count;
    if (bridge) {
        slot->bridge_calls++;
    }
    if (error) {
        slot->errors++;
    }
    if (column_count > 0U) {
        slot->result_sets++;
    }
}

static mylite_mysqli_profile_slot *mylite_mysqli_profile_slot_for(
    const char *sql,
    size_t sql_length
) {
    char normalized[MYLITE_MYSQLI_PROFILE_SQL_LENGTH];
    mylite_mysqli_profile_slot *fallback =
        &MYLITE_MYSQLI_G(profile_slots)[MYLITE_MYSQLI_PROFILE_SLOT_COUNT - 1U];

    mylite_mysqli_profile_normalize_sql(sql, sql_length, normalized);
    for (uint32_t index = 0U; index < MYLITE_MYSQLI_PROFILE_SLOT_COUNT - 1U; index++) {
        mylite_mysqli_profile_slot *slot = &MYLITE_MYSQLI_G(profile_slots)[index];

        if (slot->sql[0] == '\0') {
            snprintf(slot->sql, sizeof(slot->sql), "%s", normalized);
            return slot;
        }
        if (strcmp(slot->sql, normalized) == 0) {
            return slot;
        }
    }

    if (fallback->sql[0] == '\0') {
        snprintf(fallback->sql, sizeof(fallback->sql), "%s", "<other>");
    }
    return fallback;
}

static void mylite_mysqli_profile_normalize_sql(
    const char *sql,
    size_t sql_length,
    char out_sql[MYLITE_MYSQLI_PROFILE_SQL_LENGTH]
) {
    size_t out_index = 0U;
    bool pending_space = false;

    for (size_t index = 0U; index < sql_length && out_index + 1U < MYLITE_MYSQLI_PROFILE_SQL_LENGTH;
         index++) {
        unsigned char ch = (unsigned char)sql[index];

        if (isspace(ch)) {
            pending_space = out_index > 0U;
            continue;
        }
        if (pending_space && out_index + 1U < MYLITE_MYSQLI_PROFILE_SQL_LENGTH) {
            out_sql[out_index++] = ' ';
            pending_space = false;
        }
        if (ch == '\'' || ch == '"' || ch == '`') {
            unsigned char quote = ch;

            out_sql[out_index++] = '?';
            while (index + 1U < sql_length) {
                index++;
                ch = (unsigned char)sql[index];
                if (ch == '\\' && index + 1U < sql_length) {
                    index++;
                    continue;
                }
                if (ch == quote) {
                    break;
                }
            }
            continue;
        }
        if (isdigit(ch)) {
            out_sql[out_index++] = '?';
            while (index + 1U < sql_length && isdigit((unsigned char)sql[index + 1U])) {
                index++;
            }
            continue;
        }
        out_sql[out_index++] = (char)tolower(ch);
    }

    while (out_index > 0U && out_sql[out_index - 1U] == ' ') {
        out_index--;
    }
    out_sql[out_index] = '\0';
}

static bool mylite_mysqli_reject_oversized_packet(mylite_mysqli_link *link, size_t sql_length) {
    if (sql_length <= (size_t)MYLITE_MYSQLI_MAX_ALLOWED_PACKET) {
        return false;
    }

    mylite_mysqli_set_packet_too_large_error(link);
    mylite_mysqli_report_link_error(link);
    return true;
}

static bool mylite_mysqli_buffer_result(
    mylite_mysqli_link *link,
    const mylite_result *source,
    zval *out_result
) {
    size_t column_count = mylite_result_column_count(source);
    size_t row_count = mylite_result_row_count(source);

    if (column_count > (size_t)UINT32_MAX || row_count > (size_t)UINT32_MAX ||
        (column_count != 0U && row_count > SIZE_MAX / column_count)) {
        mylite_mysqli_set_error(
            link,
            MYLITE_MYSQLI_ERROR_CLIENT,
            "HY000",
            "result set is too large"
        );
        mylite_mysqli_report_link_error(link);
        return false;
    }

    link->field_count = (zend_long)column_count;
    mylite_mysqli_capture_result_warnings(link, source);
    if (!mylite_mysqli_set_link_info(link, mylite_result_info(source))) {
        mylite_mysqli_set_error(link, MYLITE_MYSQLI_ERROR_CLIENT, "HY000", "out of memory");
        mylite_mysqli_report_link_error(link);
        return false;
    }
    if (column_count == 0U) {
        link->affected_rows = (zend_long)mylite_result_affected_rows(source);
        link->insert_id = (zend_long)mylite_result_insert_id(source);
        ZVAL_TRUE(out_result);
        return true;
    }

    object_init_ex(out_result, mylite_mysqli_result_ce);
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ_P(out_result));
    result->column_count = (uint32_t)column_count;
    result->row_count = (uint32_t)row_count;
    result->row_capacity = result->row_count;
    result->fields = ecalloc(result->column_count, sizeof(*result->fields));
    if (row_count > 0U) {
        result->values = ecalloc(row_count * column_count, sizeof(zval));
    }

    for (uint32_t column = 0; column < result->column_count; column++) {
        mylite_mysqli_field *field = &result->fields[column];
        const char *name = mylite_result_column_name(source, column);
        const char *schema = mylite_result_column_schema_name(source, column);
        const char *table = mylite_result_column_table_name(source, column);
        const char *origin_table = mylite_result_column_origin_table_name(source, column);
        const char *origin_name = mylite_result_column_origin_name(source, column);
        uint32_t collation_id = mylite_result_column_collation_id(source, column);

        field->name = mylite_mysqli_column_string(name);
        field->schema = mylite_mysqli_column_string(schema);
        field->table = mylite_mysqli_column_string(table);
        field->origin_table = mylite_mysqli_column_string(origin_table);
        field->origin_name = mylite_mysqli_column_string(origin_name);
        field->type = mylite_mysqli_column_type(mylite_result_column_type(source, column));
        field->flags = mylite_result_column_flags(source, column);
        field->length = mylite_result_column_display_length(source, column);
        field->decimals = mylite_result_column_decimals(source, column);
        field->charset = collation_id == 0U ? 255U : collation_id;
        field->nullable = mylite_result_column_nullable(source, column) != 0;
    }

    for (uint32_t row = 0; row < result->row_count; row++) {
        for (uint32_t column = 0; column < result->column_count; column++) {
            zval *value = &result->values[(size_t)row * result->column_count + column];
            const void *bytes = mylite_result_value_bytes(source, row, column);
            size_t byte_count = mylite_result_value_size(source, row, column);

            if (mylite_result_value_is_null(source, row, column)) {
                ZVAL_NULL(value);
            } else {
                if (byte_count > result->fields[column].max_length) {
                    result->fields[column].max_length = byte_count;
                }
                ZVAL_STRINGL(value, bytes == NULL ? "" : (const char *)bytes, byte_count);
            }
        }
    }

    link->affected_rows = -1;
    link->insert_id = (zend_long)mylite_result_insert_id(source);
    mylite_mysqli_update_result_properties(result);
    return true;
}

static bool mylite_mysqli_create_cursor_result(
    mylite_mysqli_link *link,
    mylite_stmt *native_stmt,
    zval *out_result
) {
    size_t column_count = mylite_stmt_column_count(native_stmt);

    if (column_count > (size_t)UINT32_MAX) {
        mylite_mysqli_set_error(
            link,
            MYLITE_MYSQLI_ERROR_CLIENT,
            "HY000",
            "result set has too many columns"
        );
        mylite_mysqli_report_link_error(link);
        return false;
    }

    link->field_count = (zend_long)column_count;
    link->warning_count = 0;
    link->affected_rows = -1;
    link->insert_id = 0;
    if (!mylite_mysqli_set_link_info(link, NULL)) {
        mylite_mysqli_set_error(link, MYLITE_MYSQLI_ERROR_CLIENT, "HY000", "out of memory");
        mylite_mysqli_report_link_error(link);
        return false;
    }
    mylite_mysqli_capture_stmt_warnings(link, NULL, native_stmt);
    if (column_count == 0U) {
        (void)mylite_stmt_finalize(native_stmt);
        ZVAL_TRUE(out_result);
        return true;
    }

    object_init_ex(out_result, mylite_mysqli_result_ce);
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ_P(out_result));
    result->column_count = (uint32_t)column_count;
    result->row_capacity = 1U;
    result->unbuffered = true;
    result->native_stmt = native_stmt;
    result->fields = ecalloc(result->column_count, sizeof(*result->fields));
    result->values = ecalloc(result->column_count, sizeof(zval));
    for (uint32_t column = 0; column < result->column_count; column++) {
        ZVAL_UNDEF(&result->values[column]);
        mylite_mysqli_fill_field_from_stmt(&result->fields[column], native_stmt, column);
    }

    mylite_mysqli_update_result_properties(result);
    return true;
}

static bool mylite_mysqli_buffer_cursor_result(
    mylite_mysqli_link *link,
    mylite_stmt *native_stmt,
    zval *out_result,
    bool finalize_statement,
    bool current_row_pending,
    bool native_types
) {
    int status = current_row_pending ? MYLITE_ROW : mylite_stmt_step(native_stmt);
    size_t column_count = mylite_stmt_column_count(native_stmt);

    mylite_mysqli_clear_error(link);
    if (status != MYLITE_ROW && status != MYLITE_DONE) {
        if (finalize_statement) {
            (void)mylite_stmt_finalize(native_stmt);
        }
        return mylite_mysqli_capture_link_status(link, status, "could not execute statement");
    }

    if (column_count > (size_t)UINT32_MAX) {
        mylite_mysqli_set_error(
            link,
            MYLITE_MYSQLI_ERROR_CLIENT,
            "HY000",
            "result set has too many columns"
        );
        mylite_mysqli_report_link_error(link);
        if (finalize_statement) {
            (void)mylite_stmt_finalize(native_stmt);
        }
        return false;
    }

    link->field_count = (zend_long)column_count;
    link->warning_count = 0;
    link->affected_rows = -1;
    link->insert_id = 0;
    if (!mylite_mysqli_set_link_info(link, NULL)) {
        mylite_mysqli_set_error(link, MYLITE_MYSQLI_ERROR_CLIENT, "HY000", "out of memory");
        mylite_mysqli_report_link_error(link);
        if (finalize_statement) {
            (void)mylite_stmt_finalize(native_stmt);
        }
        return false;
    }
    if (column_count == 0U) {
        link->affected_rows = (zend_long)mylite_stmt_affected_rows(native_stmt);
        link->insert_id = (zend_long)mylite_stmt_insert_id(native_stmt);
        mylite_mysqli_capture_stmt_warnings(link, NULL, native_stmt);
        if (!mylite_mysqli_set_link_info(link, mylite_stmt_info(native_stmt))) {
            mylite_mysqli_set_error(link, MYLITE_MYSQLI_ERROR_CLIENT, "HY000", "out of memory");
            mylite_mysqli_report_link_error(link);
            if (finalize_statement) {
                (void)mylite_stmt_finalize(native_stmt);
            }
            return false;
        }
        if (finalize_statement) {
            status = mylite_stmt_finalize(native_stmt);
            if (status != MYLITE_OK) {
                return mylite_mysqli_capture_link_status(
                    link,
                    status,
                    "could not finalize statement"
                );
            }
        }
        ZVAL_TRUE(out_result);
        return true;
    }

    object_init_ex(out_result, mylite_mysqli_result_ce);
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(Z_OBJ_P(out_result));
    result->column_count = (uint32_t)column_count;
    result->fields = ecalloc(result->column_count, sizeof(*result->fields));
    for (uint32_t column = 0; column < result->column_count; column++) {
        mylite_mysqli_fill_field_from_stmt(&result->fields[column], native_stmt, column);
    }

    while (status == MYLITE_ROW) {
        if (result->row_count == result->row_capacity) {
            uint32_t next_capacity = 16U;

            if (result->row_capacity > UINT32_MAX / 2U) {
                mylite_mysqli_set_error(
                    link,
                    MYLITE_MYSQLI_ERROR_CLIENT,
                    "HY000",
                    "result set is too large"
                );
                mylite_mysqli_report_link_error(link);
                if (finalize_statement) {
                    (void)mylite_stmt_finalize(native_stmt);
                }
                zval_ptr_dtor(out_result);
                ZVAL_UNDEF(out_result);
                return false;
            }
            if (result->row_capacity != 0U) {
                next_capacity = result->row_capacity * 2U;
            }
            if (!mylite_mysqli_result_reserve_rows(link, result, next_capacity)) {
                if (finalize_statement) {
                    (void)mylite_stmt_finalize(native_stmt);
                }
                zval_ptr_dtor(out_result);
                ZVAL_UNDEF(out_result);
                return false;
            }
        }
        for (uint32_t column = 0; column < result->column_count; column++) {
            const void *bytes = mylite_stmt_value_bytes(native_stmt, column);
            size_t byte_count = mylite_stmt_value_size(native_stmt, column);
            zval *value =
                &result->values[(size_t)result->row_count * result->column_count + column];

            if (mylite_stmt_value_is_null(native_stmt, column)) {
                ZVAL_NULL(value);
            } else {
                if (byte_count > result->fields[column].max_length) {
                    result->fields[column].max_length = byte_count;
                }
                if (native_types) {
                    mylite_php_native_value_to_zval(
                        mylite_stmt_column_type(native_stmt, column),
                        bytes,
                        byte_count,
                        false,
                        value
                    );
                } else {
                    ZVAL_STRINGL(value, bytes == NULL ? "" : (const char *)bytes, byte_count);
                }
            }
        }
        result->row_count++;
        status = mylite_stmt_step(native_stmt);
    }

    if (status != MYLITE_DONE) {
        if (finalize_statement) {
            (void)mylite_stmt_finalize(native_stmt);
        }
        zval_ptr_dtor(out_result);
        ZVAL_UNDEF(out_result);
        return mylite_mysqli_capture_link_status(link, status, "could not execute statement");
    }
    link->affected_rows = -1;
    link->insert_id = (zend_long)mylite_stmt_insert_id(native_stmt);
    mylite_mysqli_capture_stmt_warnings(link, NULL, native_stmt);
    if (finalize_statement) {
        status = mylite_stmt_finalize(native_stmt);
        if (status != MYLITE_OK) {
            zval_ptr_dtor(out_result);
            ZVAL_UNDEF(out_result);
            return mylite_mysqli_capture_link_status(link, status, "could not finalize statement");
        }
    }

    mylite_mysqli_update_result_properties(result);
    return true;
}

static void mylite_mysqli_capture_result_warnings(
    mylite_mysqli_link *link,
    const mylite_result *result
) {
    size_t record_count = result == NULL ? 0U : mylite_result_warning_record_count(result);

    mylite_mysqli_clear_warning_records(&link->warning_records, &link->warning_record_count);
    link->warning_count = result == NULL ? 0 : (zend_long)mylite_result_warning_count(result);
    if (record_count == 0U) {
        return;
    }

    link->warning_records = safe_emalloc(record_count, sizeof(*link->warning_records), 0U);
    for (size_t index = 0U; index < record_count; ++index) {
        if (mylite_result_warning_at(result, index, &link->warning_records[index]) != MYLITE_OK) {
            mylite_mysqli_clear_warning_records(
                &link->warning_records,
                &link->warning_record_count
            );
            link->warning_count = 0;
            return;
        }
        memcpy(
            link->warning_records[index].sqlstate,
            "HY000",
            sizeof(link->warning_records[index].sqlstate)
        );
        link->warning_record_count = index + 1U;
    }
}

static void mylite_mysqli_capture_stmt_warnings(
    mylite_mysqli_link *link,
    mylite_mysqli_stmt *stmt,
    const mylite_stmt *native_stmt
) {
    size_t record_count = native_stmt == NULL ? 0U : mylite_stmt_warning_record_count(native_stmt);

    mylite_mysqli_clear_warning_records(&link->warning_records, &link->warning_record_count);
    if (stmt != NULL) {
        mylite_mysqli_clear_warning_records(&stmt->warning_records, &stmt->warning_record_count);
    }
    link->warning_count =
        native_stmt == NULL ? 0 : (zend_long)mylite_stmt_warning_count(native_stmt);
    if (record_count == 0U) {
        return;
    }

    link->warning_records = safe_emalloc(record_count, sizeof(*link->warning_records), 0U);
    for (size_t index = 0U; index < record_count; ++index) {
        if (mylite_stmt_warning_at(native_stmt, index, &link->warning_records[index]) !=
            MYLITE_OK) {
            mylite_mysqli_clear_warning_records(
                &link->warning_records,
                &link->warning_record_count
            );
            link->warning_count = 0;
            return;
        }
        memcpy(
            link->warning_records[index].sqlstate,
            "HY000",
            sizeof(link->warning_records[index].sqlstate)
        );
        link->warning_record_count = index + 1U;
    }
    if (stmt != NULL) {
        stmt->warning_records = safe_emalloc(record_count, sizeof(*stmt->warning_records), 0U);
        memcpy(
            stmt->warning_records,
            link->warning_records,
            record_count * sizeof(*stmt->warning_records)
        );
        stmt->warning_record_count = record_count;
    }
}

static void mylite_mysqli_clear_warning_records(
    struct mylite_diagnostic **records,
    size_t *record_count
) {
    if (records == NULL || record_count == NULL) {
        return;
    }

    efree(*records);
    *records = NULL;
    *record_count = 0U;
}

static bool mylite_mysqli_initialize_warning_object(
    const struct mylite_diagnostic *records,
    size_t record_count,
    zval *out_warning
) {
    if (records == NULL || record_count == 0U || out_warning == NULL) {
        return false;
    }

    object_init_ex(out_warning, mylite_mysqli_warning_ce);
    mylite_mysqli_warning *warning = mylite_mysqli_warning_from_obj(Z_OBJ_P(out_warning));

    warning->records = safe_emalloc(record_count, sizeof(*warning->records), 0U);
    memcpy(warning->records, records, record_count * sizeof(*warning->records));
    warning->record_count = record_count;
    warning->index = 0U;
    mylite_mysqli_update_warning_properties(warning);
    return true;
}

static void mylite_mysqli_update_warning_properties(mylite_mysqli_warning *warning) {
    const struct mylite_diagnostic *record = NULL;

    if (warning == NULL || warning->records == NULL || warning->index >= warning->record_count) {
        return;
    }
    record = &warning->records[warning->index];
    zend_update_property_string(
        mylite_mysqli_warning_ce,
        &warning->std,
        "message",
        strlen("message"),
        record->message
    );
    zend_update_property_string(
        mylite_mysqli_warning_ce,
        &warning->std,
        "sqlstate",
        strlen("sqlstate"),
        record->sqlstate
    );
    zend_update_property_long(
        mylite_mysqli_warning_ce,
        &warning->std,
        "errno",
        strlen("errno"),
        record->error_code
    );
}

static bool mylite_mysqli_result_reserve_rows(
    mylite_mysqli_link *link,
    mylite_mysqli_result *result,
    uint32_t row_capacity
) {
    size_t old_value_count = 0U;
    size_t new_value_count = 0U;

    if (row_capacity < result->row_count ||
        (result->column_count != 0U && (size_t)row_capacity > SIZE_MAX / result->column_count)) {
        mylite_mysqli_set_error(
            link,
            MYLITE_MYSQLI_ERROR_CLIENT,
            "HY000",
            "result set is too large"
        );
        mylite_mysqli_report_link_error(link);
        return false;
    }

    old_value_count = (size_t)result->row_capacity * result->column_count;
    new_value_count = (size_t)row_capacity * result->column_count;
    result->values = result->values == NULL
                         ? safe_emalloc(new_value_count, sizeof(zval), 0)
                         : safe_erealloc(result->values, new_value_count, sizeof(zval), 0);
    memset(&result->values[old_value_count], 0, (new_value_count - old_value_count) * sizeof(zval));
    result->row_capacity = row_capacity;
    return true;
}

static void mylite_mysqli_fill_field_from_stmt(
    mylite_mysqli_field *field,
    const mylite_stmt *native_stmt,
    uint32_t column
) {
    uint32_t collation_id = mylite_stmt_column_collation_id(native_stmt, column);

    field->name = mylite_mysqli_column_string(mylite_stmt_column_name(native_stmt, column));
    field->schema =
        mylite_mysqli_column_string(mylite_stmt_column_schema_name(native_stmt, column));
    field->table = mylite_mysqli_column_string(mylite_stmt_column_table_name(native_stmt, column));
    field->origin_table =
        mylite_mysqli_column_string(mylite_stmt_column_origin_table_name(native_stmt, column));
    field->origin_name =
        mylite_mysqli_column_string(mylite_stmt_column_origin_name(native_stmt, column));
    field->type = mylite_mysqli_column_type(mylite_stmt_column_type(native_stmt, column));
    field->flags = mylite_stmt_column_flags(native_stmt, column);
    field->length = mylite_stmt_column_display_length(native_stmt, column);
    field->decimals = mylite_stmt_column_decimals(native_stmt, column);
    field->charset = collation_id == 0U ? 255U : collation_id;
    field->nullable = mylite_stmt_column_nullable(native_stmt, column) != 0;
}

bool mylite_mysqli_link_require_ready(mylite_mysqli_link *link) {
    return mylite_mysqli_link_check_ready(link, true);
}

bool mylite_mysqli_link_require_ready_without_report(mylite_mysqli_link *link) {
    return mylite_mysqli_link_check_ready(link, false);
}

static bool mylite_mysqli_link_check_ready(mylite_mysqli_link *link, bool report_error) {
    static const char message[] = "Commands out of sync; you can't run this command now";

    if (link->state == MYLITE_MYSQLI_CONNECTION_READY) {
        return true;
    }
    mylite_mysqli_set_error(link, MYLITE_MYSQLI_ERROR_COMMANDS_OUT_OF_SYNC, "HY000", message);
    mylite_mysqli_update_link_status_properties(link);
    if (report_error) {
        mylite_mysqli_report_link_error(link);
    }
    return false;
}

static bool mylite_mysqli_stmt_require_ready(mylite_mysqli_stmt *stmt, mylite_mysqli_link *link) {
    static const char message[] = "Commands out of sync; you can't run this command now";

    if (link->state == MYLITE_MYSQLI_CONNECTION_READY ||
        (link->state == MYLITE_MYSQLI_CONNECTION_PREPARED_UNBUFFERED && link->active_owner == stmt
        )) {
        return true;
    }
    mylite_mysqli_set_error(link, MYLITE_MYSQLI_ERROR_COMMANDS_OUT_OF_SYNC, "HY000", message);
    mylite_mysqli_set_stmt_error(stmt, MYLITE_MYSQLI_ERROR_COMMANDS_OUT_OF_SYNC, "HY000", message);
    mylite_mysqli_update_link_status_properties(link);
    mylite_mysqli_update_stmt_properties(stmt);
    mylite_mysqli_report_stmt_error(stmt);
    return false;
}

static void mylite_mysqli_link_set_direct_pending(mylite_mysqli_link *link) {
    link->state = MYLITE_MYSQLI_CONNECTION_DIRECT_PENDING;
    link->active_owner = NULL;
}

static void mylite_mysqli_link_set_prepared_owner(
    mylite_mysqli_link *link,
    mylite_mysqli_stmt *stmt
) {
    link->state = MYLITE_MYSQLI_CONNECTION_PREPARED_UNBUFFERED;
    link->active_owner = stmt;
}

static void mylite_mysqli_link_release_owner(mylite_mysqli_link *link, const void *owner) {
    if ((link->state == MYLITE_MYSQLI_CONNECTION_DIRECT_PENDING && owner == NULL) ||
        (link->active_owner != NULL && link->active_owner == owner)) {
        link->state = MYLITE_MYSQLI_CONNECTION_READY;
        link->active_owner = NULL;
    }
}

static void mylite_mysqli_link_cancel_active_owner(mylite_mysqli_link *link) {
    if (link->state == MYLITE_MYSQLI_CONNECTION_DIRECT_UNBUFFERED && link->active_owner != NULL) {
        mylite_mysqli_result_close_cursor(link->active_owner);
        return;
    }
    if (link->state == MYLITE_MYSQLI_CONNECTION_PREPARED_UNBUFFERED && link->active_owner != NULL) {
        mylite_mysqli_stmt_discard_current_result(link->active_owner, true);
    }
}

static void mylite_mysqli_result_attach_connection(
    mylite_mysqli_result *result,
    mylite_mysqli_link *link
) {
    if (result->connection_owner) {
        mylite_mysqli_result_release_connection(result);
    }
    ZVAL_OBJ_COPY(&result->link, &link->std);
    result->connection_owner = true;
    link->state = MYLITE_MYSQLI_CONNECTION_DIRECT_UNBUFFERED;
    link->active_owner = result;
}

static void mylite_mysqli_result_release_connection(mylite_mysqli_result *result) {
    if (!result->connection_owner || Z_ISUNDEF(result->link) || Z_TYPE(result->link) != IS_OBJECT) {
        return;
    }

    mylite_mysqli_link *link = mylite_mysqli_link_from_obj(Z_OBJ(result->link));

    mylite_mysqli_link_release_owner(link, result);
    result->connection_owner = false;
    zval_ptr_dtor(&result->link);
    ZVAL_UNDEF(&result->link);
}

static void mylite_mysqli_link_clear_pending_result(mylite_mysqli_link *link) {
    if (link->pending_stmt != NULL) {
        (void)mylite_stmt_finalize(link->pending_stmt);
        link->pending_stmt = NULL;
    }
    if (link->pending_sql != NULL) {
        zend_string_release(link->pending_sql);
        link->pending_sql = NULL;
    }
    link->pending_execute_ns = 0U;
    mylite_mysqli_link_release_owner(link, NULL);
}

static void mylite_mysqli_link_clear_last_result(mylite_mysqli_link *link) {
    if (Z_ISUNDEF(link->last_result)) {
        return;
    }
    if (Z_TYPE(link->last_result) == IS_OBJECT &&
        instanceof_function(Z_OBJCE(link->last_result), mylite_mysqli_result_ce)) {
        mylite_mysqli_result_close_cursor(mylite_mysqli_result_from_obj(Z_OBJ(link->last_result)));
    }
    zval_ptr_dtor(&link->last_result);
    ZVAL_UNDEF(&link->last_result);
}

void mylite_mysqli_result_discard(mylite_mysqli_result *result) {
    mylite_mysqli_result_close_cursor(result);
    mylite_mysqli_update_result_properties(result);
}

static void mylite_mysqli_result_close_cursor(mylite_mysqli_result *result) {
    if (result == NULL) {
        return;
    }
    if (result->unbuffered) {
        mylite_mysqli_result_clear_current_row(result);
        if (result->native_stmt != NULL) {
            (void)mylite_stmt_finalize(result->native_stmt);
            result->native_stmt = NULL;
        }
        result->unbuffered_finished = true;
    }
    mylite_mysqli_result_release_connection(result);
}

static void mylite_mysqli_result_clear_current_row(mylite_mysqli_result *result) {
    if (!result->current_row_valid || result->values == NULL) {
        return;
    }
    for (uint32_t column = 0; column < result->column_count; column++) {
        zval_ptr_dtor(&result->values[column]);
        ZVAL_UNDEF(&result->values[column]);
    }
    result->current_row_valid = false;
}

static int mylite_mysqli_result_read_cursor_row(mylite_mysqli_result *result) {
    int status = MYLITE_DONE;

    if (!result->unbuffered || result->native_stmt == NULL || result->unbuffered_finished) {
        return MYLITE_DONE;
    }

    mylite_mysqli_result_clear_current_row(result);
    status = mylite_stmt_step(result->native_stmt);
    if (status == MYLITE_ROW) {
        for (uint32_t column = 0; column < result->column_count; column++) {
            const void *bytes = mylite_stmt_value_bytes(result->native_stmt, column);
            size_t byte_count = mylite_stmt_value_size(result->native_stmt, column);
            zval *value = &result->values[column];

            if (mylite_stmt_value_is_null(result->native_stmt, column)) {
                ZVAL_NULL(value);
            } else {
                if (byte_count > result->fields[column].max_length) {
                    result->fields[column].max_length = byte_count;
                }
                ZVAL_STRINGL(value, bytes == NULL ? "" : (const char *)bytes, byte_count);
            }
        }
        result->current_row_valid = true;
        result->cursor++;
        result->row_count = result->cursor;
        return MYLITE_ROW;
    }

    result->unbuffered_finished = true;
    if (status == MYLITE_DONE && !Z_ISUNDEF(result->link) && Z_TYPE(result->link) == IS_OBJECT) {
        mylite_mysqli_link *link = mylite_mysqli_link_from_obj(Z_OBJ(result->link));

        mylite_mysqli_capture_stmt_warnings(link, NULL, result->native_stmt);
        mylite_mysqli_update_link_status_properties(link);
    }
    if (result->native_stmt != NULL) {
        int finalize_status = mylite_stmt_finalize(result->native_stmt);

        result->native_stmt = NULL;
        if (status == MYLITE_DONE && finalize_status != MYLITE_OK) {
            status = finalize_status;
        }
    }
    mylite_mysqli_result_release_connection(result);
    mylite_mysqli_update_result_properties(result);
    return status;
}

static zend_string *mylite_mysqli_column_string(const char *value) {
    if (value == NULL || value[0] == '\0') {
        return zend_string_copy(zend_empty_string);
    }
    return zend_string_init(value, strlen(value), false);
}

static int mylite_mysqli_column_type(enum mylite_result_column_type type) {
    if (type == MYLITE_RESULT_COLUMN_TYPE_UNKNOWN) {
        return MYLITE_MYSQLI_FIELD_TYPE_VAR_STRING;
    }
    return (int)type;
}

void mylite_mysqli_result_fetch(mylite_mysqli_result *result, int mode, zval *return_value) {
    uint32_t element_count = 0U;
    zval *row_values = NULL;
    int status = mylite_mysqli_result_next_row(result, &row_values);

    if (status == MYLITE_DONE) {
        RETURN_NULL();
    }
    if (status != MYLITE_ROW) {
        RETURN_FALSE;
    }

    if ((mode & MYLITE_MYSQLI_NUM) != 0) {
        element_count += result->column_count;
    }
    if ((mode & MYLITE_MYSQLI_ASSOC) != 0) {
        element_count += result->column_count;
    }
    array_init_size(return_value, element_count);
    for (uint32_t column = 0; column < result->column_count; column++) {
        zval *source = &row_values[column];
        zval value;

        if ((mode & MYLITE_MYSQLI_NUM) != 0) {
            ZVAL_COPY(&value, source);
            add_next_index_zval(return_value, &value);
        }
        if ((mode & MYLITE_MYSQLI_ASSOC) != 0) {
            ZVAL_COPY(&value, source);
            add_assoc_zval_ex(
                return_value,
                ZSTR_VAL(result->fields[column].name),
                ZSTR_LEN(result->fields[column].name),
                &value
            );
        }
    }
}

void mylite_mysqli_result_fetch_object(mylite_mysqli_result *result, zval *return_value) {
    zval *row_values = NULL;
    int status = mylite_mysqli_result_next_row(result, &row_values);
    HashTable *properties = NULL;

    if (status == MYLITE_DONE) {
        RETURN_NULL();
    }
    if (status != MYLITE_ROW) {
        RETURN_FALSE;
    }

    object_init(return_value);
    properties = Z_OBJPROP_P(return_value);
    zend_hash_extend(properties, result->column_count, false);
    for (uint32_t column = 0; column < result->column_count; column++) {
        zval value;

        ZVAL_COPY(&value, &row_values[column]);
        zend_hash_update(properties, result->fields[column].name, &value);
    }
}

void mylite_mysqli_result_fetch_column(
    mylite_mysqli_result *result,
    zend_long column,
    zval *return_value
) {
    zval *row_values = NULL;
    int status = 0;

    if (column < 0 || (uint32_t)column >= result->column_count) {
        RETURN_FALSE;
    }
    status = mylite_mysqli_result_next_row(result, &row_values);
    if (status == MYLITE_DONE) {
        RETURN_NULL();
    }
    if (status != MYLITE_ROW) {
        RETURN_FALSE;
    }

    ZVAL_COPY(return_value, &row_values[(uint32_t)column]);
}

void mylite_mysqli_result_fetch_field(
    mylite_mysqli_result *result,
    uint32_t index,
    zval *return_value
) {
    mylite_mysqli_field *field = &result->fields[index];

    object_init(return_value);
    add_property_str(return_value, "name", zend_string_copy(field->name));
    add_property_str(
        return_value,
        "orgname",
        field->origin_name == NULL ? ZSTR_EMPTY_ALLOC() : zend_string_copy(field->origin_name)
    );
    add_property_str(
        return_value,
        "table",
        field->table == NULL ? ZSTR_EMPTY_ALLOC() : zend_string_copy(field->table)
    );
    add_property_str(
        return_value,
        "orgtable",
        field->origin_table == NULL ? ZSTR_EMPTY_ALLOC() : zend_string_copy(field->origin_table)
    );
    add_property_str(
        return_value,
        "db",
        field->schema == NULL ? ZSTR_EMPTY_ALLOC() : zend_string_copy(field->schema)
    );
    add_property_string(return_value, "catalog", "def");
    add_property_string(return_value, "def", "");
    add_property_long(return_value, "flags", (zend_long)field->flags);
    add_property_long(return_value, "type", field->type);
    add_property_long(return_value, "length", (zend_long)field->length);
    add_property_long(return_value, "max_length", (zend_long)field->max_length);
    add_property_long(return_value, "decimals", (zend_long)field->decimals);
    add_property_long(return_value, "charsetnr", (zend_long)field->charset);
}

static int mylite_mysqli_result_next_row(mylite_mysqli_result *result, zval **out_row_values) {
#if PHP_VERSION_ID < 80400
    zval retained_link;
    bool report_retained_error =
        result->connection_owner && !Z_ISUNDEF(result->link) && Z_TYPE(result->link) == IS_OBJECT;

    ZVAL_UNDEF(&retained_link);
    if (report_retained_error) {
        ZVAL_COPY(&retained_link, &result->link);
    }
#endif
    int status = MYLITE_DONE;

    if (result->unbuffered) {
        status = mylite_mysqli_result_read_cursor_row(result);
        if (status == MYLITE_ROW) {
            *out_row_values = result->values;
        }
    } else if (result->cursor >= result->row_count) {
        mylite_mysqli_result_release_connection(result);
    } else {
        *out_row_values = &result->values[(size_t)result->cursor * result->column_count];
        result->cursor++;
        status = MYLITE_ROW;
    }

#if PHP_VERSION_ID < 80400
    if (report_retained_error) {
        mylite_mysqli_link *link = mylite_mysqli_link_from_obj(Z_OBJ(retained_link));

        if (link->error_code != 0) {
            mylite_mysqli_report_link_error(link);
        }
        zval_ptr_dtor(&retained_link);
    }
#endif
    return status;
}

static int mylite_mysqli_stmt_copy_current_row(mylite_mysqli_stmt *stmt) {
    for (uint32_t column = 0; column < (uint32_t)stmt->field_count; column++) {
        zval *target = &stmt->bound_results[column];
        zval *target_value = target;

        ZVAL_DEREF(target_value);
        zval_ptr_dtor(target_value);
        if (mylite_stmt_value_is_null(stmt->native_stmt, column)) {
            ZVAL_NULL(target_value);
        } else {
            const void *bytes = mylite_stmt_value_bytes(stmt->native_stmt, column);
            size_t byte_count = mylite_stmt_value_size(stmt->native_stmt, column);

            mylite_php_native_value_to_zval(
                mylite_stmt_column_type(stmt->native_stmt, column),
                bytes,
                byte_count,
                false,
                target_value
            );
        }
    }
    return MYLITE_OK;
}

static bool mylite_mysqli_stmt_buffer_current_result(mylite_mysqli_stmt *stmt) {
    mylite_mysqli_link *link = mylite_mysqli_link_from_obj(Z_OBJ(stmt->link));
    zval result;

    if (!mylite_mysqli_buffer_cursor_result(
            link,
            stmt->native_stmt,
            &result,
            false,
            stmt->current_row_pending,
            true
        )) {
        mylite_mysqli_set_stmt_error(stmt, link->error_code, link->sqlstate, ZSTR_VAL(link->error));
        mylite_mysqli_report_stmt_error(stmt);
        mylite_mysqli_stmt_discard_current_result(stmt, true);
        return false;
    }

    stmt->current_row_pending = false;
    mylite_mysqli_link_release_owner(link, stmt);
    if (!Z_ISUNDEF(stmt->result)) {
        zval_ptr_dtor(&stmt->result);
        ZVAL_UNDEF(&stmt->result);
    }
    if (Z_TYPE(result) == IS_OBJECT) {
        mylite_mysqli_result *result_object = mylite_mysqli_result_from_obj(Z_OBJ(result));

        stmt->num_rows = result_object->row_count;
        stmt->field_count = result_object->column_count;
        ZVAL_COPY_VALUE(&stmt->result, &result);
    } else {
        zval_ptr_dtor(&result);
        stmt->num_rows = 0;
        stmt->field_count = 0;
    }
    stmt->affected_rows = link->affected_rows;
    stmt->insert_id = link->insert_id;
    mylite_mysqli_capture_stmt_warnings(link, stmt, stmt->native_stmt);
    mylite_mysqli_clear_stmt_error(stmt);
    mylite_mysqli_update_link_status_properties(link);
    mylite_mysqli_update_stmt_properties(stmt);
    return true;
}

static void mylite_mysqli_stmt_discard_current_result(
    mylite_mysqli_stmt *stmt,
    bool reset_native_statement
) {
    if (!Z_ISUNDEF(stmt->link) && Z_TYPE(stmt->link) == IS_OBJECT) {
        mylite_mysqli_link *link = mylite_mysqli_link_from_obj(Z_OBJ(stmt->link));

        mylite_mysqli_link_release_owner(link, stmt);
    }
    stmt->current_row_pending = false;
    if (reset_native_statement && stmt->native_stmt != NULL) {
        (void)mylite_stmt_reset(stmt->native_stmt);
    }
    if (!Z_ISUNDEF(stmt->result)) {
        zval_ptr_dtor(&stmt->result);
        ZVAL_UNDEF(&stmt->result);
    }
}

static bool mylite_mysqli_bind_statement_parameters(mylite_mysqli_stmt *stmt, zval *params) {
    mylite_mysqli_link *link = mylite_mysqli_link_from_obj(Z_OBJ(stmt->link));
    const uint32_t supplied_count =
        params == NULL ? stmt->bound_param_count : zend_hash_num_elements(Z_ARRVAL_P(params));
    size_t packet_size = 0U;

    if (supplied_count != stmt->param_count) {
        mylite_mysqli_set_stmt_error(
            stmt,
            MYLITE_MYSQLI_ERROR_CLIENT,
            "HY000",
            "parameter count mismatch"
        );
        return false;
    }
    if (!mylite_mysqli_initialize_execute_packet_size(link, supplied_count, &packet_size)) {
        mylite_mysqli_set_stmt_error(stmt, link->error_code, link->sqlstate, ZSTR_VAL(link->error));
        return false;
    }
    for (uint32_t index = 0U; index < supplied_count; ++index) {
        zval *value = params == NULL ? &stmt->bound_params[index]
                                     : zend_hash_index_find(Z_ARRVAL_P(params), index);
        const char type =
            params == NULL && stmt->types != NULL ? ZSTR_VAL(stmt->types)[index] : '\0';
        const zend_string *long_data =
            params != NULL || stmt->long_data == NULL ? NULL : stmt->long_data[index];

        if (value == NULL || !mylite_mysqli_bind_native_value(
                                 link,
                                 stmt->native_stmt,
                                 index,
                                 value,
                                 type,
                                 long_data,
                                 &packet_size
                             )) {
            if (link->error_code == MYLITE_MYSQLI_ERROR_NONE) {
                mylite_mysqli_set_error(
                    link,
                    MYLITE_MYSQLI_ERROR_CLIENT,
                    "HY000",
                    "invalid statement parameter"
                );
            }
            mylite_mysqli_set_stmt_error(
                stmt,
                link->error_code,
                link->sqlstate,
                ZSTR_VAL(link->error)
            );
            return false;
        }
    }
    return true;
}

static bool mylite_mysqli_initialize_execute_packet_size(
    mylite_mysqli_link *link,
    size_t parameter_count,
    size_t *out_packet_size
) {
    enum {
        command_and_statement_header_size = 10,
        new_parameter_bindings_flag_size = 1,
        parameter_type_size = 2,
    };

    size_t null_bitmap_size = 0U;
    size_t type_bytes = 0U;
    size_t packet_size = 0U;

    if (out_packet_size == NULL || parameter_count > (SIZE_MAX - 7U)) {
        mylite_mysqli_set_packet_too_large_error(link);
        return false;
    }
    null_bitmap_size = (parameter_count + 7U) / 8U;
    if (parameter_count > SIZE_MAX / parameter_type_size) {
        mylite_mysqli_set_packet_too_large_error(link);
        return false;
    }
    type_bytes = parameter_count * parameter_type_size;
    if (null_bitmap_size >
            SIZE_MAX - command_and_statement_header_size - new_parameter_bindings_flag_size ||
        type_bytes > SIZE_MAX - command_and_statement_header_size -
                         new_parameter_bindings_flag_size - null_bitmap_size) {
        mylite_mysqli_set_packet_too_large_error(link);
        return false;
    }
    packet_size = command_and_statement_header_size + new_parameter_bindings_flag_size +
                  null_bitmap_size + type_bytes;
    if (packet_size > (size_t)MYLITE_MYSQLI_MAX_ALLOWED_PACKET) {
        mylite_mysqli_set_packet_too_large_error(link);
        return false;
    }

    *out_packet_size = packet_size;
    return true;
}

static bool mylite_mysqli_bind_native_value(
    mylite_mysqli_link *link,
    mylite_stmt *native_stmt,
    size_t index,
    zval *value,
    char type,
    const zend_string *long_data,
    size_t *packet_size
) {
    int status = MYLITE_OK;

    ZVAL_DEREF(value);
    if (long_data != NULL) {
        if (ZSTR_LEN(long_data) > (size_t)MYLITE_MYSQLI_MAX_ALLOWED_PACKET) {
            mylite_mysqli_set_packet_too_large_error(link);
            return false;
        }
        status =
            mylite_stmt_bind_blob(native_stmt, index, ZSTR_VAL(long_data), ZSTR_LEN(long_data));
    } else if (Z_TYPE_P(value) == IS_NULL) {
        status = mylite_stmt_bind_null(native_stmt, index);
    } else if (type == 'i') {
        if (!mylite_mysqli_add_execute_packet_value(link, packet_size, sizeof(zend_long), false)) {
            return false;
        }
        status = mylite_stmt_bind_int64(native_stmt, index, zval_get_long(value));
    } else if (type == 'd') {
        if (!mylite_mysqli_add_execute_packet_value(link, packet_size, sizeof(double), false)) {
            return false;
        }
        status = mylite_stmt_bind_double(native_stmt, index, zval_get_double(value));
    } else if (type == 's' || type == 'b') {
        zend_string *text = zval_get_string(value);

        if (UNEXPECTED(EG(exception) != NULL)) {
            return false;
        }
        if (!mylite_mysqli_add_execute_packet_value(link, packet_size, ZSTR_LEN(text), true)) {
            zend_string_release(text);
            return false;
        }
        status = type == 'b'
                     ? mylite_stmt_bind_blob(native_stmt, index, ZSTR_VAL(text), ZSTR_LEN(text))
                     : mylite_stmt_bind_text(native_stmt, index, ZSTR_VAL(text), ZSTR_LEN(text));
        zend_string_release(text);
    } else if (type != '\0') {
        mylite_mysqli_set_error(
            link,
            MYLITE_MYSQLI_ERROR_CLIENT,
            "HY000",
            "invalid parameter type"
        );
        return false;
    } else {
        switch (Z_TYPE_P(value)) {
        case IS_FALSE:
        case IS_TRUE:
            if (!mylite_mysqli_add_execute_packet_value(
                    link,
                    packet_size,
                    sizeof(zend_long),
                    false
                )) {
                return false;
            }
            status = mylite_stmt_bind_int64(native_stmt, index, Z_TYPE_P(value) == IS_TRUE ? 1 : 0);
            break;
        case IS_LONG:
            if (!mylite_mysqli_add_execute_packet_value(
                    link,
                    packet_size,
                    sizeof(zend_long),
                    false
                )) {
                return false;
            }
            status = mylite_stmt_bind_int64(native_stmt, index, Z_LVAL_P(value));
            break;
        case IS_DOUBLE:
            if (!mylite_mysqli_add_execute_packet_value(link, packet_size, sizeof(double), false)) {
                return false;
            }
            status = mylite_stmt_bind_double(native_stmt, index, Z_DVAL_P(value));
            break;
        default: {
            zend_string *text = zval_get_string(value);

            if (UNEXPECTED(EG(exception) != NULL)) {
                return false;
            }
            if (!mylite_mysqli_add_execute_packet_value(link, packet_size, ZSTR_LEN(text), true)) {
                zend_string_release(text);
                return false;
            }
            status = mylite_stmt_bind_text(native_stmt, index, ZSTR_VAL(text), ZSTR_LEN(text));
            zend_string_release(text);
            break;
        }
        }
    }

    if (status != MYLITE_OK) {
        const char *sqlstate = "HY000";
        int error_code = link->database == NULL ? 0 : mylite_errcode(link->database);

        if (error_code == 0) {
            error_code = mylite_mysqli_error_from_status(status, &sqlstate);
        } else {
            sqlstate = mylite_sqlstate(link->database);
        }
        mylite_mysqli_set_error(
            link,
            error_code,
            sqlstate,
            link->database == NULL ? "could not bind statement parameter"
                                   : mylite_errmsg(link->database)
        );
        return false;
    }
    return true;
}

static bool mylite_mysqli_add_execute_packet_value(
    mylite_mysqli_link *link,
    size_t *packet_size,
    size_t value_size,
    bool has_length_prefix
) {
    size_t prefix_size =
        has_length_prefix ? mylite_mysqli_length_encoded_integer_size(value_size) : 0U;
    size_t remaining = 0U;

    if (packet_size == NULL || *packet_size > (size_t)MYLITE_MYSQLI_MAX_ALLOWED_PACKET) {
        mylite_mysqli_set_packet_too_large_error(link);
        return false;
    }
    remaining = (size_t)MYLITE_MYSQLI_MAX_ALLOWED_PACKET - *packet_size;
    if (prefix_size > remaining || value_size > remaining - prefix_size) {
        mylite_mysqli_set_packet_too_large_error(link);
        return false;
    }

    *packet_size += prefix_size + value_size;
    return true;
}

static size_t mylite_mysqli_length_encoded_integer_size(size_t value) {
    if (value < 251U) {
        return 1U;
    }
    if (value <= UINT16_MAX) {
        return 3U;
    }
    if (value <= 0xFFFFFFU) {
        return 4U;
    }
    return 9U;
}

static void mylite_mysqli_set_packet_too_large_error(mylite_mysqli_link *link) {
    mylite_mysqli_set_error(
        link,
        MYLITE_MYSQLI_ERROR_PACKET_TOO_LARGE,
        "08S01",
        "Got a packet bigger than 'max_allowed_packet' bytes"
    );
}

static bool mylite_mysqli_capture_stmt_status(
    mylite_mysqli_stmt *stmt,
    mylite_mysqli_link *link,
    int status,
    const char *fallback
) {
    const char *sqlstate = "HY000";
    const char *message = fallback;
    int error_code = link->database == NULL ? 0 : mylite_errcode(link->database);

    if (error_code == 0) {
        error_code = mylite_mysqli_error_from_status(status, &sqlstate);
    } else {
        sqlstate = mylite_sqlstate(link->database);
        message = mylite_errmsg(link->database);
    }
    mylite_mysqli_set_error(link, error_code, sqlstate, message);
    mylite_mysqli_set_stmt_error(stmt, error_code, sqlstate, message);
    mylite_mysqli_report_stmt_error(stmt);
    return false;
}

static bool mylite_mysqli_capture_link_status(
    mylite_mysqli_link *link,
    int status,
    const char *fallback
) {
    const char *sqlstate = "HY000";
    const char *message = fallback;
    int error_code = link->database == NULL ? 0 : mylite_errcode(link->database);

    if (error_code == 0) {
        error_code = mylite_mysqli_error_from_status(status, &sqlstate);
    } else {
        sqlstate = mylite_sqlstate(link->database);
        message = mylite_errmsg(link->database);
    }
    mylite_mysqli_set_error(link, error_code, sqlstate, message);
    mylite_mysqli_report_link_error(link);
    return false;
}

static void mylite_mysqli_clear_stmt_long_data(mylite_mysqli_stmt *stmt) {
    if (stmt->long_data == NULL) {
        return;
    }
    for (uint32_t index = 0U; index < stmt->param_count; ++index) {
        if (stmt->long_data[index] != NULL) {
            zend_string_release(stmt->long_data[index]);
            stmt->long_data[index] = NULL;
        }
    }
}

static void mylite_mysqli_clear_stmt_bindings(mylite_mysqli_stmt *stmt) {
    mylite_mysqli_clear_stmt_long_data(stmt);
    efree(stmt->long_data);
    stmt->long_data = NULL;
    if (stmt->bound_params != NULL) {
        for (uint32_t index = 0U; index < stmt->bound_param_count; ++index) {
            zval_ptr_dtor(&stmt->bound_params[index]);
        }
        efree(stmt->bound_params);
        stmt->bound_params = NULL;
    }
    stmt->bound_param_count = 0U;
    if (stmt->types != NULL) {
        zend_string_release(stmt->types);
        stmt->types = NULL;
    }
}

static bool mylite_mysqli_sql_may_stream_select(const char *sql, size_t length) {
    size_t offset = 0U;

    for (;;) {
        while (offset < length && isspace((unsigned char)sql[offset])) {
            offset++;
        }
        if (offset + 1U < length && sql[offset] == '/' && sql[offset + 1U] == '*') {
            offset += 2U;
            while (offset + 1U < length && !(sql[offset] == '*' && sql[offset + 1U] == '/')) {
                offset++;
            }
            if (offset + 1U >= length) {
                return false;
            }
            offset += 2U;
            continue;
        }
        if (offset < length && sql[offset] == '#') {
            while (offset < length && !mylite_mysqli_is_line_comment_terminator(sql[offset])) {
                offset++;
            }
            continue;
        }
        if (offset + 2U < length && sql[offset] == '-' && sql[offset + 1U] == '-' &&
            isspace((unsigned char)sql[offset + 2U])) {
            offset += 2U;
            while (offset < length && !mylite_mysqli_is_line_comment_terminator(sql[offset])) {
                offset++;
            }
            continue;
        }
        break;
    }

    return mylite_mysqli_sql_keyword_at(sql, length, offset, "select") ||
           mylite_mysqli_sql_keyword_at(sql, length, offset, "with");
}

static bool mylite_mysqli_sql_keyword_at(
    const char *sql,
    size_t length,
    size_t offset,
    const char *keyword
) {
    size_t keyword_length = strlen(keyword);

    if (offset + keyword_length > length) {
        return false;
    }
    for (size_t index = 0U; index < keyword_length; index++) {
        if (tolower((unsigned char)sql[offset + index]) != keyword[index]) {
            return false;
        }
    }
    return offset + keyword_length == length ||
           !mylite_mysqli_is_identifier_char((unsigned char)sql[offset + keyword_length]);
}

static bool mylite_mysqli_match_transaction_control_statement(
    const char *sql,
    size_t sql_length,
    enum mylite_transaction_control_statement *out_statement
) {
    const char *cursor = sql;
    const char *end = sql + sql_length;

    cursor = mylite_mysqli_skip_space(cursor, end);
    if (mylite_mysqli_consume_keyword(&cursor, end, "START")) {
        cursor = mylite_mysqli_skip_space(cursor, end);
        if (!mylite_mysqli_consume_keyword(&cursor, end, "TRANSACTION") ||
            !mylite_mysqli_consume_statement_end(&cursor, end)) {
            return false;
        }
        *out_statement = MYLITE_TRANSACTION_CONTROL_START;
        return true;
    }
    if (mylite_mysqli_consume_keyword(&cursor, end, "BEGIN")) {
        if (!mylite_mysqli_consume_statement_end(&cursor, end)) {
            return false;
        }
        *out_statement = MYLITE_TRANSACTION_CONTROL_START;
        return true;
    }
    if (mylite_mysqli_consume_keyword(&cursor, end, "COMMIT")) {
        if (!mylite_mysqli_consume_statement_end(&cursor, end)) {
            return false;
        }
        *out_statement = MYLITE_TRANSACTION_CONTROL_COMMIT;
        return true;
    }
    if (mylite_mysqli_consume_keyword(&cursor, end, "ROLLBACK")) {
        if (!mylite_mysqli_consume_statement_end(&cursor, end)) {
            return false;
        }
        *out_statement = MYLITE_TRANSACTION_CONTROL_ROLLBACK;
        return true;
    }

    return false;
}

static bool mylite_mysqli_consume_statement_end(const char **cursor, const char *end) {
    *cursor = mylite_mysqli_skip_space(*cursor, end);
    if (*cursor < end && **cursor == ';') {
        (*cursor)++;
        *cursor = mylite_mysqli_skip_space(*cursor, end);
    }
    return *cursor == end;
}

static bool mylite_mysqli_set_link_info(mylite_mysqli_link *link, const char *info) {
    zend_string *new_info = NULL;

    if (info != NULL) {
        new_info = zend_string_init(info, strlen(info), false);
        if (new_info == NULL) {
            return false;
        }
    }
    if (link->info != NULL) {
        zend_string_release(link->info);
    }
    link->info = new_info;
    return true;
}

static const char *mylite_mysqli_skip_space(const char *cursor, const char *end) {
    while (cursor < end && isspace((unsigned char)*cursor)) {
        cursor++;
    }
    return cursor;
}

static bool mylite_mysqli_consume_keyword(
    const char **cursor,
    const char *end,
    const char *keyword
) {
    size_t length = strlen(keyword);

    if ((size_t)(end - *cursor) < length ||
        zend_binary_strcasecmp(*cursor, length, keyword, length) != 0) {
        return false;
    }
    if (*cursor + length < end &&
        mylite_mysqli_is_identifier_char((unsigned char)(*cursor)[length])) {
        return false;
    }
    *cursor += length;
    return true;
}

static bool mylite_mysqli_is_identifier_char(unsigned char ch) {
    return isalnum(ch) || ch == '_' || ch == '$';
}

static bool mylite_mysqli_is_line_comment_terminator(char ch) {
    return ch == '\n' || ch == '\r';
}

static zend_string *mylite_mysqli_resolve_path(
    const char *host,
    size_t host_length,
    const char *database,
    size_t database_length,
    const char *socket,
    size_t socket_length,
    bool *out_memory,
    bool *out_use_database
) {
    static const char prefix[] = "mylite:";
    const char *host_path = NULL;
    size_t host_path_length = 0U;

    *out_memory = true;
    *out_use_database = false;
    if (host != NULL && host_length >= sizeof(prefix) - 1U &&
        memcmp(host, prefix, sizeof(prefix) - 1U) == 0) {
        const char *path = host + sizeof(prefix) - 1U;
        size_t path_length = host_length - (sizeof(prefix) - 1U);

        *out_memory = path_length == 0U || (path_length == strlen(":memory:") &&
                                            memcmp(path, ":memory:", path_length) == 0);
        *out_use_database = database != NULL && database_length > 0U &&
                            !mylite_mysqli_is_path_like(database, database_length);
        return zend_string_init(
            path_length == 0U ? ":memory:" : path,
            path_length == 0U ? strlen(":memory:") : path_length,
            false
        );
    }
    host_path = mylite_mysqli_host_path(host, host_length, &host_path_length);
    if (host_path != NULL) {
        *out_memory = mylite_mysqli_is_local_path(host_path, host_path_length);
        *out_use_database = database != NULL && database_length > 0U &&
                            !mylite_mysqli_is_path_like(database, database_length);
        return zend_string_init(host_path, host_path_length, false);
    }
    if (socket != NULL && socket_length > 0U) {
        *out_memory = mylite_mysqli_is_local_path(socket, socket_length);
        *out_use_database = database != NULL && database_length > 0U &&
                            !mylite_mysqli_is_path_like(database, database_length);
        return zend_string_init(socket, socket_length, false);
    }
    if (database != NULL && mylite_mysqli_is_path_like(database, database_length)) {
        *out_memory = mylite_mysqli_is_local_path(database, database_length);
        return zend_string_init(database, database_length, false);
    }

    return zend_string_init(":memory:", strlen(":memory:"), false);
}

static const char *mylite_mysqli_host_path(
    const char *host,
    size_t host_length,
    size_t *out_path_length
) {
    static const char localhost_prefix[] = "localhost:";
    const char *path = NULL;
    size_t path_length = 0U;

    *out_path_length = 0U;
    if (host == NULL || host_length == 0U) {
        return NULL;
    }
    if (host_length > sizeof(localhost_prefix) - 1U &&
        memcmp(host, localhost_prefix, sizeof(localhost_prefix) - 1U) == 0) {
        path = host + sizeof(localhost_prefix) - 1U;
        path_length = host_length - (sizeof(localhost_prefix) - 1U);
        if (mylite_mysqli_is_path_like(path, path_length)) {
            *out_path_length = path_length;
            return path;
        }
    }
    if (!mylite_mysqli_is_path_like(host, host_length)) {
        return NULL;
    }
    *out_path_length = host_length;
    return host;
}

static bool mylite_mysqli_is_path_like(const char *value, size_t length) {
    static const char suffix[] = ".mylite";

    if (value == NULL || length == 0U) {
        return false;
    }
    if (mylite_mysqli_is_local_path(value, length) || memchr(value, '/', length) != NULL ||
        memchr(value, '\\', length) != NULL || value[0] == '.') {
        return true;
    }
    return length > sizeof(suffix) - 1U &&
           memcmp(value + length - (sizeof(suffix) - 1U), suffix, sizeof(suffix) - 1U) == 0;
}

static bool mylite_mysqli_is_local_path(const char *value, size_t length) {
    return value != NULL && length == strlen(":memory:") && memcmp(value, ":memory:", length) == 0;
}

zend_string *mylite_mysqli_quote_identifier(const char *value, size_t length) {
    smart_str quoted = {0};

    smart_str_appendc(&quoted, '`');
    for (size_t index = 0U; index < length; index++) {
        if (value[index] == '`') {
            smart_str_appendc(&quoted, '`');
        }
        smart_str_appendc(&quoted, value[index]);
    }
    smart_str_appendc(&quoted, '`');
    smart_str_0(&quoted);
    return quoted.s;
}

bool mylite_mysqli_escape_string(
    mylite_mysqli_link *link,
    const char *value,
    size_t length,
    zend_string **out_escaped
) {
    smart_str escaped = {0};

    *out_escaped = NULL;
    if (link->database == NULL) {
        mylite_mysqli_set_error(
            link,
            MYLITE_MYSQLI_ERROR_CONNECTION,
            "HY000",
            "mysqli object is not connected"
        );
        mylite_mysqli_report_link_error(link);
        return false;
    }
    if (mylite_session_no_backslash_escapes(link->database) > 0) {
        mylite_mysqli_set_error(
            link,
            MYLITE_MYSQLI_ERROR_INSECURE_API,
            "HY000",
            "mysql_real_escape_string cannot be used with NO_BACKSLASH_ESCAPES sql mode"
        );
        mylite_mysqli_report_link_error(link);
        return false;
    }

    mylite_mysqli_clear_error(link);
    for (size_t index = 0U; index < length; index++) {
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
    *out_escaped = escaped.s == NULL ? zend_string_init("", 0, false) : escaped.s;
    return true;
}

void mylite_mysqli_set_error(
    mylite_mysqli_link *link,
    int error_code,
    const char *sqlstate,
    const char *message
) {
    if (message == NULL || message[0] == '\0') {
        message = "MyLite mysqli error";
    }
    zend_string_release(link->error);
    link->error = zend_string_init(message, strlen(message), false);
    (void)mylite_mysqli_set_link_info(link, NULL);
    link->error_code = error_code;
    memcpy(link->sqlstate, sqlstate, 5U);
    link->sqlstate[5] = '\0';
    mylite_mysqli_update_link_status_properties(link);
}

void mylite_mysqli_set_stmt_error(
    mylite_mysqli_stmt *stmt,
    int error_code,
    const char *sqlstate,
    const char *message
) {
    if (message == NULL || message[0] == '\0') {
        message = "MyLite mysqli statement error";
    }
    zend_string_release(stmt->error);
    stmt->error = zend_string_init(message, strlen(message), false);
    stmt->error_code = error_code;
    memcpy(stmt->sqlstate, sqlstate, 5U);
    stmt->sqlstate[5] = '\0';
    mylite_mysqli_update_stmt_properties(stmt);
}

static void mylite_mysqli_clear_error(mylite_mysqli_link *link) {
    zend_string_release(link->error);
    link->error = zend_empty_string;
    zend_string_addref(link->error);
    link->error_code = 0;
    memcpy(link->sqlstate, "00000", sizeof(link->sqlstate));
}

static void mylite_mysqli_clear_stmt_error(mylite_mysqli_stmt *stmt) {
    zend_string_release(stmt->error);
    stmt->error = zend_empty_string;
    zend_string_addref(stmt->error);
    stmt->error_code = 0;
    memcpy(stmt->sqlstate, "00000", sizeof(stmt->sqlstate));
    mylite_mysqli_update_stmt_properties(stmt);
}

void mylite_mysqli_report_link_error(mylite_mysqli_link *link) {
    if ((MYLITE_MYSQLI_G(report_mode) & MYLITE_MYSQLI_REPORT_STRICT) != 0) {
        zend_object *exception = zend_throw_exception(
            mylite_mysqli_exception_ce,
            ZSTR_VAL(link->error),
            link->error_code
        );

        if (exception != NULL) {
            zend_update_property_string(
                mylite_mysqli_exception_ce,
                exception,
                "sqlstate",
                strlen("sqlstate"),
                link->sqlstate
            );
        }
    } else if ((MYLITE_MYSQLI_G(report_mode) & MYLITE_MYSQLI_REPORT_ERROR) != 0) {
        php_error_docref(NULL, E_WARNING, "%s", ZSTR_VAL(link->error));
    }
}

void mylite_mysqli_report_stmt_error(mylite_mysqli_stmt *stmt) {
    if ((MYLITE_MYSQLI_G(report_mode) & MYLITE_MYSQLI_REPORT_STRICT) != 0) {
        zend_object *exception = zend_throw_exception(
            mylite_mysqli_exception_ce,
            ZSTR_VAL(stmt->error),
            stmt->error_code
        );

        if (exception != NULL) {
            zend_update_property_string(
                mylite_mysqli_exception_ce,
                exception,
                "sqlstate",
                strlen("sqlstate"),
                stmt->sqlstate
            );
        }
    } else if ((MYLITE_MYSQLI_G(report_mode) & MYLITE_MYSQLI_REPORT_ERROR) != 0) {
        php_error_docref(NULL, E_WARNING, "%s", ZSTR_VAL(stmt->error));
    }
}

static int mylite_mysqli_error_from_status(int status, const char **out_sqlstate) {
    *out_sqlstate = "HY000";
    return status == MYLITE_ERROR ? MYLITE_MYSQLI_ERROR_EXEC : MYLITE_MYSQLI_ERROR_CLIENT;
}

void mylite_mysqli_update_link_properties(mylite_mysqli_link *link) {
    zend_update_property_string(
        mylite_mysqli_link_ce,
        &link->std,
        "client_info",
        strlen("client_info"),
        "mylite mysqli " PHP_MYLITE_MYSQLI_VERSION
    );
    zend_update_property_long(
        mylite_mysqli_link_ce,
        &link->std,
        "client_version",
        strlen("client_version"),
        100
    );
    zend_update_property_string(
        mylite_mysqli_link_ce,
        &link->std,
        "host_info",
        strlen("host_info"),
        "MyLite embedded"
    );
    zend_update_property_string(
        mylite_mysqli_link_ce,
        &link->std,
        "server_info",
        strlen("server_info"),
        MYLITE_MYSQLI_SERVER_INFO
    );
    zend_update_property_long(
        mylite_mysqli_link_ce,
        &link->std,
        "server_version",
        strlen("server_version"),
        MYLITE_MYSQLI_SERVER_VERSION
    );
    zend_update_property_long(
        mylite_mysqli_link_ce,
        &link->std,
        "protocol_version",
        strlen("protocol_version"),
        10
    );
    zend_update_property_long(
        mylite_mysqli_link_ce,
        &link->std,
        "thread_id",
        strlen("thread_id"),
        (zend_long)mylite_connection_id(link->database)
    );
    mylite_mysqli_update_link_status_properties(link);
}

void mylite_mysqli_update_link_status_properties(mylite_mysqli_link *link) {
    zend_update_property_long(
        mylite_mysqli_link_ce,
        &link->std,
        "affected_rows",
        strlen("affected_rows"),
        link->affected_rows
    );
    zend_update_property_long(
        mylite_mysqli_link_ce,
        &link->std,
        "connect_errno",
        strlen("connect_errno"),
        MYLITE_MYSQLI_G(connect_errno)
    );
    zend_update_property_string(
        mylite_mysqli_link_ce,
        &link->std,
        "connect_error",
        strlen("connect_error"),
        MYLITE_MYSQLI_G(connect_error)
    );
    zend_update_property_long(
        mylite_mysqli_link_ce,
        &link->std,
        "errno",
        strlen("errno"),
        link->error_code
    );
    zend_update_property_str(
        mylite_mysqli_link_ce,
        &link->std,
        "error",
        strlen("error"),
        zend_string_copy(link->error)
    );
    zend_update_property_long(
        mylite_mysqli_link_ce,
        &link->std,
        "field_count",
        strlen("field_count"),
        link->field_count
    );
    if (link->info == NULL) {
        zend_update_property_null(mylite_mysqli_link_ce, &link->std, "info", strlen("info"));
    } else {
        zend_update_property_str(
            mylite_mysqli_link_ce,
            &link->std,
            "info",
            strlen("info"),
            zend_string_copy(link->info)
        );
    }
    zend_update_property_long(
        mylite_mysqli_link_ce,
        &link->std,
        "insert_id",
        strlen("insert_id"),
        link->insert_id
    );
    zend_update_property_string(
        mylite_mysqli_link_ce,
        &link->std,
        "sqlstate",
        strlen("sqlstate"),
        link->sqlstate
    );
    zend_update_property_long(
        mylite_mysqli_link_ce,
        &link->std,
        "warning_count",
        strlen("warning_count"),
        link->warning_count
    );
}

void mylite_mysqli_update_result_properties(mylite_mysqli_result *result) {
    zend_update_property_long(
        mylite_mysqli_result_ce,
        &result->std,
        "current_field",
        strlen("current_field"),
        result->field_cursor
    );
    zend_update_property_long(
        mylite_mysqli_result_ce,
        &result->std,
        "field_count",
        strlen("field_count"),
        result->column_count
    );
    zend_update_property_null(mylite_mysqli_result_ce, &result->std, "lengths", strlen("lengths"));
    zend_update_property_long(
        mylite_mysqli_result_ce,
        &result->std,
        "num_rows",
        strlen("num_rows"),
        mylite_mysqli_result_num_rows(result)
    );
    zend_update_property_long(
        mylite_mysqli_result_ce,
        &result->std,
        "type",
        strlen("type"),
        result->unbuffered ? MYLITE_MYSQLI_USE_RESULT : MYLITE_MYSQLI_STORE_RESULT
    );
}

zend_long mylite_mysqli_result_num_rows(const mylite_mysqli_result *result) {
    if (result->unbuffered && !result->unbuffered_finished) {
        return 0;
    }
    return (zend_long)result->row_count;
}

void mylite_mysqli_update_stmt_properties(mylite_mysqli_stmt *stmt) {
    zend_update_property_long(
        mylite_mysqli_stmt_ce,
        &stmt->std,
        "affected_rows",
        strlen("affected_rows"),
        stmt->affected_rows
    );
    zend_update_property_long(
        mylite_mysqli_stmt_ce,
        &stmt->std,
        "insert_id",
        strlen("insert_id"),
        stmt->insert_id
    );
    zend_update_property_long(
        mylite_mysqli_stmt_ce,
        &stmt->std,
        "num_rows",
        strlen("num_rows"),
        stmt->num_rows
    );
    zend_update_property_long(
        mylite_mysqli_stmt_ce,
        &stmt->std,
        "param_count",
        strlen("param_count"),
        stmt->param_count
    );
    zend_update_property_long(
        mylite_mysqli_stmt_ce,
        &stmt->std,
        "field_count",
        strlen("field_count"),
        stmt->field_count
    );
    zend_update_property_long(
        mylite_mysqli_stmt_ce,
        &stmt->std,
        "errno",
        strlen("errno"),
        stmt->error_code
    );
    zend_update_property_str(
        mylite_mysqli_stmt_ce,
        &stmt->std,
        "error",
        strlen("error"),
        zend_string_copy(stmt->error)
    );
    zend_update_property_string(
        mylite_mysqli_stmt_ce,
        &stmt->std,
        "sqlstate",
        strlen("sqlstate"),
        stmt->sqlstate
    );
    zend_update_property_long(mylite_mysqli_stmt_ce, &stmt->std, "id", strlen("id"), 0);
}

static void mylite_mysqli_set_global_connect_error(int error_code, const char *message) {
    MYLITE_MYSQLI_G(connect_errno) = error_code;
    snprintf(
        MYLITE_MYSQLI_G(connect_error),
        sizeof(MYLITE_MYSQLI_G(connect_error)),
        "%s",
        message == NULL ? "" : message
    );
}

void mylite_mysqli_flush_profile(void) {
    bool printed[MYLITE_MYSQLI_PROFILE_SLOT_COUNT] = {false};
    FILE *output = NULL;
    FILE *opened_output = NULL;

    if (!MYLITE_MYSQLI_G(profile_enabled) || MYLITE_MYSQLI_G(profile_calls) == 0U) {
        return;
    }

    if (MYLITE_MYSQLI_G(profile_stderr)) {
        output = stderr;
    } else {
        opened_output = fopen(MYLITE_MYSQLI_G(profile_path), "a");
        output = opened_output;
    }
    if (output == NULL) {
        output = stderr;
    }
    if (output == NULL) {
        return;
    }

    fprintf(
        output,
        "mylite_mysqli_profile_calls=%llu\n",
        (unsigned long long)MYLITE_MYSQLI_G(profile_calls)
    );
    fprintf(
        output,
        "mylite_mysqli_profile_errors=%llu\n",
        (unsigned long long)MYLITE_MYSQLI_G(profile_errors)
    );
    fprintf(
        output,
        "mylite_mysqli_profile_bridge_calls=%llu\n",
        (unsigned long long)MYLITE_MYSQLI_G(profile_bridge_calls)
    );
    fprintf(
        output,
        "mylite_mysqli_profile_result_sets=%llu\n",
        (unsigned long long)MYLITE_MYSQLI_G(profile_result_sets)
    );
    fprintf(
        output,
        "mylite_mysqli_profile_rows=%llu\n",
        (unsigned long long)MYLITE_MYSQLI_G(profile_rows)
    );
    fprintf(
        output,
        "mylite_mysqli_profile_cells=%llu\n",
        (unsigned long long)MYLITE_MYSQLI_G(profile_cells)
    );
    fprintf(
        output,
        "mylite_mysqli_profile_execute_ms=%.3f\n",
        (double)MYLITE_MYSQLI_G(profile_execute_ns) / 1000000.0
    );
    fprintf(
        output,
        "mylite_mysqli_profile_buffer_ms=%.3f\n",
        (double)MYLITE_MYSQLI_G(profile_buffer_ns) / 1000000.0
    );
    fprintf(output, "mylite_mysqli_profile_top_sql:\n");

    for (uint32_t rank = 0U; rank < MYLITE_MYSQLI_G(profile_limit); rank++) {
        uint32_t best_index = MYLITE_MYSQLI_PROFILE_SLOT_COUNT;
        uint64_t best_ns = 0U;

        for (uint32_t index = 0U; index < MYLITE_MYSQLI_PROFILE_SLOT_COUNT; index++) {
            const mylite_mysqli_profile_slot *slot = &MYLITE_MYSQLI_G(profile_slots)[index];
            uint64_t total_ns = slot->execute_ns + slot->buffer_ns;

            if (printed[index] || slot->calls == 0U) {
                continue;
            }
            if (best_index == MYLITE_MYSQLI_PROFILE_SLOT_COUNT || total_ns > best_ns) {
                best_index = index;
                best_ns = total_ns;
            }
        }
        if (best_index == MYLITE_MYSQLI_PROFILE_SLOT_COUNT) {
            break;
        }

        const mylite_mysqli_profile_slot *slot = &MYLITE_MYSQLI_G(profile_slots)[best_index];

        printed[best_index] = true;
        fprintf(
            output,
            "%u\tcalls=%llu\terrors=%llu\tbridge=%llu\tresults=%llu\trows=%llu\tcells=%llu\t"
            "execute_ms=%.3f\tbuffer_ms=%.3f\tsql=%s\n",
            rank + 1U,
            (unsigned long long)slot->calls,
            (unsigned long long)slot->errors,
            (unsigned long long)slot->bridge_calls,
            (unsigned long long)slot->result_sets,
            (unsigned long long)slot->rows,
            (unsigned long long)slot->cells,
            (double)slot->execute_ns / 1000000.0,
            (double)slot->buffer_ns / 1000000.0,
            slot->sql
        );
    }
    fprintf(output, "mylite_mysqli_profile_end\n");

    if (opened_output != NULL) {
        fclose(opened_output);
    }

    MYLITE_MYSQLI_G(profile_calls) = 0U;
    MYLITE_MYSQLI_G(profile_errors) = 0U;
    MYLITE_MYSQLI_G(profile_bridge_calls) = 0U;
    MYLITE_MYSQLI_G(profile_result_sets) = 0U;
    MYLITE_MYSQLI_G(profile_rows) = 0U;
    MYLITE_MYSQLI_G(profile_cells) = 0U;
    MYLITE_MYSQLI_G(profile_execute_ns) = 0U;
    MYLITE_MYSQLI_G(profile_buffer_ns) = 0U;
    memset(MYLITE_MYSQLI_G(profile_slots), 0, sizeof(MYLITE_MYSQLI_G(profile_slots)));
}

void mylite_mysqli_init_globals(zend_mylite_mysqli_globals *globals) {
    const char *profile = getenv("MYLITE_MYSQLI_PROFILE");
    const char *profile_limit = getenv("MYLITE_MYSQLI_PROFILE_LIMIT");
    unsigned long parsed_limit = 20UL;

    globals->report_mode = MYLITE_MYSQLI_REPORT_ERROR | MYLITE_MYSQLI_REPORT_STRICT;
    globals->connect_errno = 0;
    globals->connect_error[0] = '\0';
    globals->profile_enabled = profile != NULL && profile[0] != '\0' && strcmp(profile, "0") != 0;
    globals->profile_stderr = false;
    globals->profile_path[0] = '\0';
    globals->profile_limit = 20U;
    globals->profile_calls = 0U;
    globals->profile_errors = 0U;
    globals->profile_bridge_calls = 0U;
    globals->profile_result_sets = 0U;
    globals->profile_rows = 0U;
    globals->profile_cells = 0U;
    globals->profile_execute_ns = 0U;
    globals->profile_buffer_ns = 0U;
    memset(globals->profile_slots, 0, sizeof(globals->profile_slots));

    if (!globals->profile_enabled) {
        return;
    }
    globals->profile_stderr = strcmp(profile, "1") == 0 || strcmp(profile, "stderr") == 0;
    if (!globals->profile_stderr) {
        snprintf(globals->profile_path, sizeof(globals->profile_path), "%s", profile);
    }
    if (profile_limit != NULL && profile_limit[0] != '\0') {
        parsed_limit = strtoul(profile_limit, NULL, 10);
        if (parsed_limit == 0UL) {
            parsed_limit = 20UL;
        }
    }
    if (parsed_limit > MYLITE_MYSQLI_PROFILE_SLOT_COUNT) {
        parsed_limit = MYLITE_MYSQLI_PROFILE_SLOT_COUNT;
    }
    globals->profile_limit = (uint32_t)parsed_limit;
}
