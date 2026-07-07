#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "mysqli_extension.h"

#include <stdlib.h>
#include <time.h>

static bool mylite_mysqli_execute_sql(
    mylite_mysqli_link *link,
    const char *sql,
    size_t sql_length,
    zval *out_result
);
static bool mylite_mysqli_execute_bridge_statement(
    mylite_mysqli_link *link,
    const char *sql,
    size_t sql_length,
    zval *out_result
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
static zend_string *mylite_mysqli_column_string(const char *value);
static int mylite_mysqli_column_type(enum mylite_result_column_type type);
static zend_string *mylite_mysqli_param_to_sql(zval *value);
static uint32_t mylite_mysqli_count_markers(const char *sql, size_t sql_length);
static bool mylite_mysqli_match_autocommit_off_assignment(const char *sql, size_t sql_length);
static void mylite_mysqli_set_ok_result(mylite_mysqli_link *link, zval *out_result);
static bool mylite_mysqli_set_link_info(mylite_mysqli_link *link, const char *info);
static const char *mylite_mysqli_skip_space(const char *cursor, const char *end);
static bool mylite_mysqli_consume_keyword(
    const char **cursor,
    const char *end,
    const char *keyword
);
static bool mylite_mysqli_consume_autocommit_identifier(const char **cursor, const char *end);
static bool mylite_mysqli_is_identifier_char(unsigned char ch);
static bool mylite_mysqli_is_line_comment_terminator(char ch);
static bool mylite_mysqli_is_dash_comment_start(const char *sql, size_t sql_length, size_t index);
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
    mylite_mysqli_update_link_properties(link);
    return &link->std;
}

void mylite_mysqli_link_free(zend_object *object) {
    mylite_mysqli_link *link = mylite_mysqli_link_from_obj(object);

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
    if (!Z_ISUNDEF(link->last_result)) {
        zval_ptr_dtor(&link->last_result);
    }
    zend_object_std_dtor(&link->std);
}

zend_object *mylite_mysqli_result_create(zend_class_entry *class_entry) {
    mylite_mysqli_result *result = zend_object_alloc(sizeof(*result), class_entry);

    zend_object_std_init(&result->std, class_entry);
    object_properties_init(&result->std, class_entry);
    result->std.handlers = &mylite_mysqli_result_handlers;
    mylite_mysqli_update_result_properties(result);
    return &result->std;
}

void mylite_mysqli_result_free(zend_object *object) {
    mylite_mysqli_result *result = mylite_mysqli_result_from_obj(object);

    for (uint32_t column = 0; column < result->column_count; column++) {
        if (result->names != NULL && result->names[column] != NULL) {
            zend_string_release(result->names[column]);
        }
        if (result->schemas != NULL && result->schemas[column] != NULL) {
            zend_string_release(result->schemas[column]);
        }
        if (result->tables != NULL && result->tables[column] != NULL) {
            zend_string_release(result->tables[column]);
        }
        if (result->origin_tables != NULL && result->origin_tables[column] != NULL) {
            zend_string_release(result->origin_tables[column]);
        }
        if (result->origin_names != NULL && result->origin_names[column] != NULL) {
            zend_string_release(result->origin_names[column]);
        }
    }
    for (uint32_t index = 0; index < result->row_count * result->column_count; index++) {
        zval_ptr_dtor(&result->values[index]);
    }
    efree(result->names);
    efree(result->schemas);
    efree(result->tables);
    efree(result->origin_tables);
    efree(result->origin_names);
    efree(result->types);
    efree(result->flags);
    efree(result->lengths);
    efree(result->max_lengths);
    efree(result->decimals);
    efree(result->charsets);
    efree(result->nullable);
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
    mylite_mysqli_update_stmt_properties(stmt);
    return &stmt->std;
}

void mylite_mysqli_stmt_free(zend_object *object) {
    mylite_mysqli_stmt *stmt = mylite_mysqli_stmt_from_obj(object);

    if (!Z_ISUNDEF(stmt->link)) {
        zval_ptr_dtor(&stmt->link);
    }
    if (!Z_ISUNDEF(stmt->result)) {
        zval_ptr_dtor(&stmt->result);
    }
    if (stmt->bound_params != NULL) {
        for (uint32_t index = 0; index < stmt->bound_param_count; index++) {
            zval_ptr_dtor(&stmt->bound_params[index]);
        }
        efree(stmt->bound_params);
    }
    if (stmt->bound_results != NULL) {
        for (uint32_t index = 0; index < stmt->bound_result_count; index++) {
            zval_ptr_dtor(&stmt->bound_results[index]);
        }
        efree(stmt->bound_results);
    }
    if (stmt->query != NULL) {
        zend_string_release(stmt->query);
    }
    if (stmt->types != NULL) {
        zend_string_release(stmt->types);
    }
    zend_string_release(stmt->error);
    zend_object_std_dtor(&stmt->std);
}

zend_object *mylite_mysqli_warning_create(zend_class_entry *class_entry) {
    mylite_mysqli_warning *warning = zend_object_alloc(sizeof(*warning), class_entry);

    zend_object_std_init(&warning->std, class_entry);
    object_properties_init(&warning->std, class_entry);
    warning->std.handlers = &mylite_mysqli_warning_handlers;
    return &warning->std;
}

void mylite_mysqli_warning_free(zend_object *object) {
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

bool mylite_mysqli_connect_link(
    mylite_mysqli_link *link,
    const char *host,
    size_t host_length,
    const char *database,
    size_t database_length,
    const char *socket,
    size_t socket_length
) {
    bool memory = false;
    bool use_database = false;
    zend_string *path = mylite_mysqli_resolve_path(
        host,
        host_length,
        database,
        database_length,
        socket,
        socket_length,
        &memory,
        &use_database
    );
    int status = MYLITE_OK;

    mylite_mysqli_clear_error(link);
    if (path == NULL) {
        mylite_mysqli_set_error(link, MYLITE_MYSQLI_ERROR_CLIENT, "HY000", "out of memory");
        mylite_mysqli_set_global_connect_error(link->error_code, ZSTR_VAL(link->error));
        mylite_mysqli_report_link_error(link);
        return false;
    }

    if (link->database != NULL) {
        mylite_close(link->database);
        link->database = NULL;
    }
    link->connected = false;
    if (link->path != NULL) {
        zend_string_release(link->path);
    }
    link->path = zend_string_copy(path);

    status =
        memory ? mylite_open_memory(&link->database) : mylite_open(ZSTR_VAL(path), &link->database);
    zend_string_release(path);
    if (status != MYLITE_OK) {
        mylite_mysqli_set_error(
            link,
            MYLITE_MYSQLI_ERROR_CONNECTION,
            "HY000",
            "failed to open MyLite database"
        );
        mylite_mysqli_set_global_connect_error(link->error_code, ZSTR_VAL(link->error));
        mylite_mysqli_report_link_error(link);
        mylite_mysqli_update_link_properties(link);
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

bool mylite_mysqli_link_query(
    mylite_mysqli_link *link,
    const char *sql,
    size_t sql_length,
    zval *out_result
) {
    if (!mylite_mysqli_execute_sql(link, sql, sql_length, out_result)) {
        return false;
    }

    if (Z_TYPE_P(out_result) == IS_OBJECT) {
        if (!Z_ISUNDEF(link->last_result)) {
            zval_ptr_dtor(&link->last_result);
        }
        ZVAL_COPY(&link->last_result, out_result);
    }
    return true;
}

bool mylite_mysqli_link_real_query(mylite_mysqli_link *link, const char *sql, size_t sql_length) {
    zval result;
    bool ok = mylite_mysqli_execute_sql(link, sql, sql_length, &result);

    if (!ok) {
        return false;
    }
    if (!Z_ISUNDEF(link->last_result)) {
        zval_ptr_dtor(&link->last_result);
        ZVAL_UNDEF(&link->last_result);
    }
    if (Z_TYPE(result) == IS_OBJECT) {
        ZVAL_COPY_VALUE(&link->last_result, &result);
    } else {
        zval_ptr_dtor(&result);
    }
    return true;
}

bool mylite_mysqli_stmt_prepare_internal(
    mylite_mysqli_stmt *stmt,
    const char *sql,
    size_t sql_length
) {
    if (stmt->query != NULL) {
        zend_string_release(stmt->query);
    }
    stmt->query = zend_string_init(sql, sql_length, false);
    stmt->param_count = mylite_mysqli_count_markers(sql, sql_length);
    mylite_mysqli_clear_stmt_error(stmt);
    mylite_mysqli_update_stmt_properties(stmt);
    return true;
}

bool mylite_mysqli_stmt_execute_internal(mylite_mysqli_stmt *stmt, zval *params) {
    mylite_mysqli_link *link = NULL;
    zend_string *sql = NULL;
    zval result;

    if (Z_ISUNDEF(stmt->link) || Z_TYPE(stmt->link) != IS_OBJECT || stmt->query == NULL) {
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
    if (params != NULL) {
        sql = mylite_mysqli_interpolate_query(
            zend_string_copy(stmt->query),
            params,
            zend_hash_num_elements(Z_ARRVAL_P(params))
        );
    } else if (stmt->bound_param_count > 0U) {
        zval params_array;

        array_init(&params_array);
        for (uint32_t index = 0; index < stmt->bound_param_count; index++) {
            zval value;

            ZVAL_COPY(&value, &stmt->bound_params[index]);
            add_next_index_zval(&params_array, &value);
        }
        sql = mylite_mysqli_interpolate_query(
            zend_string_copy(stmt->query),
            &params_array,
            stmt->bound_param_count
        );
        zval_ptr_dtor(&params_array);
    } else {
        sql = zend_string_copy(stmt->query);
    }

    if (sql == NULL) {
        mylite_mysqli_set_stmt_error(
            stmt,
            MYLITE_MYSQLI_ERROR_CLIENT,
            "HY000",
            "parameter count mismatch"
        );
        mylite_mysqli_report_stmt_error(stmt);
        return false;
    }

    if (!mylite_mysqli_execute_sql(link, ZSTR_VAL(sql), ZSTR_LEN(sql), &result)) {
        mylite_mysqli_set_stmt_error(stmt, link->error_code, link->sqlstate, ZSTR_VAL(link->error));
        zend_string_release(sql);
        mylite_mysqli_report_stmt_error(stmt);
        return false;
    }
    zend_string_release(sql);

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
    mylite_mysqli_clear_stmt_error(stmt);
    mylite_mysqli_update_stmt_properties(stmt);
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
    if (mylite_mysqli_execute_bridge_statement(link, sql, sql_length, out_result)) {
        if (profile_enabled) {
            mylite_mysqli_profile_record(sql, sql_length, true, false, 0U, 0U, 0U, 0U);
        }
        return true;
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

static bool mylite_mysqli_execute_bridge_statement(
    mylite_mysqli_link *link,
    const char *sql,
    size_t sql_length,
    zval *out_result
) {
    if (!mylite_mysqli_match_autocommit_off_assignment(sql, sql_length)) {
        return false;
    }

    mylite_mysqli_set_ok_result(link, out_result);
    return true;
}

static bool mylite_mysqli_reject_oversized_packet(mylite_mysqli_link *link, size_t sql_length) {
    if (sql_length <= (size_t)MYLITE_MYSQLI_MAX_ALLOWED_PACKET) {
        return false;
    }

    mylite_mysqli_set_error(
        link,
        MYLITE_MYSQLI_ERROR_PACKET_TOO_LARGE,
        "08S01",
        "Got a packet bigger than 'max_allowed_packet' bytes"
    );
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
    link->warning_count = (zend_long)mylite_result_warning_count(source);
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
    result->names = ecalloc(result->column_count, sizeof(zend_string *));
    result->schemas = ecalloc(result->column_count, sizeof(zend_string *));
    result->tables = ecalloc(result->column_count, sizeof(zend_string *));
    result->origin_tables = ecalloc(result->column_count, sizeof(zend_string *));
    result->origin_names = ecalloc(result->column_count, sizeof(zend_string *));
    result->types = ecalloc(result->column_count, sizeof(int));
    result->flags = ecalloc(result->column_count, sizeof(unsigned int));
    result->lengths = ecalloc(result->column_count, sizeof(uint64_t));
    result->max_lengths = ecalloc(result->column_count, sizeof(uint64_t));
    result->decimals = ecalloc(result->column_count, sizeof(unsigned int));
    result->charsets = ecalloc(result->column_count, sizeof(unsigned int));
    result->nullable = ecalloc(result->column_count, sizeof(bool));
    if (row_count > 0U) {
        result->values = ecalloc(row_count * column_count, sizeof(zval));
    }

    for (uint32_t column = 0; column < result->column_count; column++) {
        const char *name = mylite_result_column_name(source, column);
        const char *schema = mylite_result_column_schema_name(source, column);
        const char *table = mylite_result_column_table_name(source, column);
        const char *origin_table = mylite_result_column_origin_table_name(source, column);
        const char *origin_name = mylite_result_column_origin_name(source, column);
        uint32_t collation_id = mylite_result_column_collation_id(source, column);

        result->names[column] = mylite_mysqli_column_string(name);
        result->schemas[column] = mylite_mysqli_column_string(schema);
        result->tables[column] = mylite_mysqli_column_string(table);
        result->origin_tables[column] = mylite_mysqli_column_string(origin_table);
        result->origin_names[column] = mylite_mysqli_column_string(origin_name);
        result->types[column] =
            mylite_mysqli_column_type(mylite_result_column_type(source, column));
        result->flags[column] = mylite_result_column_flags(source, column);
        result->lengths[column] = mylite_result_column_display_length(source, column);
        result->decimals[column] = mylite_result_column_decimals(source, column);
        result->charsets[column] = collation_id == 0U ? 255U : collation_id;
        result->nullable[column] = mylite_result_column_nullable(source, column) != 0;
    }

    for (uint32_t row = 0; row < result->row_count; row++) {
        for (uint32_t column = 0; column < result->column_count; column++) {
            zval *value = &result->values[(size_t)row * result->column_count + column];
            const void *bytes = mylite_result_value_bytes(source, row, column);
            size_t byte_count = mylite_result_value_size(source, row, column);

            if (bytes == NULL) {
                ZVAL_NULL(value);
            } else {
                if (byte_count > result->max_lengths[column]) {
                    result->max_lengths[column] = byte_count;
                }
                ZVAL_STRINGL(value, (const char *)bytes, byte_count);
            }
        }
    }

    link->affected_rows = -1;
    link->insert_id = (zend_long)mylite_result_insert_id(source);
    mylite_mysqli_update_result_properties(result);
    return true;
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

    if (result->cursor >= result->row_count) {
        RETURN_NULL();
    }

    if ((mode & MYLITE_MYSQLI_NUM) != 0) {
        element_count += result->column_count;
    }
    if ((mode & MYLITE_MYSQLI_ASSOC) != 0) {
        element_count += result->column_count;
    }
    array_init_size(return_value, element_count);
    for (uint32_t column = 0; column < result->column_count; column++) {
        zval *source = &result->values[result->cursor * result->column_count + column];
        zval value;

        if ((mode & MYLITE_MYSQLI_NUM) != 0) {
            ZVAL_COPY(&value, source);
            add_next_index_zval(return_value, &value);
        }
        if ((mode & MYLITE_MYSQLI_ASSOC) != 0) {
            ZVAL_COPY(&value, source);
            add_assoc_zval_ex(
                return_value,
                ZSTR_VAL(result->names[column]),
                ZSTR_LEN(result->names[column]),
                &value
            );
        }
    }
    result->cursor++;
}

void mylite_mysqli_result_fetch_column(
    mylite_mysqli_result *result,
    zend_long column,
    zval *return_value
) {
    if (column < 0 || (uint32_t)column >= result->column_count) {
        RETURN_FALSE;
    }
    if (result->cursor >= result->row_count) {
        RETURN_NULL();
    }

    ZVAL_COPY(
        return_value,
        &result->values[result->cursor * result->column_count + (uint32_t)column]
    );
    result->cursor++;
}

void mylite_mysqli_result_fetch_field(
    mylite_mysqli_result *result,
    uint32_t index,
    zval *return_value
) {
    object_init(return_value);
    add_property_str(return_value, "name", zend_string_copy(result->names[index]));
    add_property_str(
        return_value,
        "orgname",
        result->origin_names[index] == NULL ? ZSTR_EMPTY_ALLOC()
                                            : zend_string_copy(result->origin_names[index])
    );
    add_property_str(
        return_value,
        "table",
        result->tables[index] == NULL ? ZSTR_EMPTY_ALLOC() : zend_string_copy(result->tables[index])
    );
    add_property_str(
        return_value,
        "orgtable",
        result->origin_tables[index] == NULL ? ZSTR_EMPTY_ALLOC()
                                             : zend_string_copy(result->origin_tables[index])
    );
    add_property_str(
        return_value,
        "db",
        result->schemas[index] == NULL ? ZSTR_EMPTY_ALLOC()
                                       : zend_string_copy(result->schemas[index])
    );
    add_property_string(return_value, "catalog", "def");
    add_property_string(return_value, "def", "");
    add_property_long(return_value, "flags", (zend_long)result->flags[index]);
    add_property_long(return_value, "type", result->types[index]);
    add_property_long(return_value, "length", (zend_long)result->lengths[index]);
    add_property_long(return_value, "max_length", (zend_long)result->max_lengths[index]);
    add_property_long(return_value, "decimals", (zend_long)result->decimals[index]);
    add_property_long(return_value, "charsetnr", (zend_long)result->charsets[index]);
}

zend_string *mylite_mysqli_interpolate_query(
    zend_string *query,
    zval *params,
    uint32_t param_count
) {
    smart_str sql = {0};
    uint32_t param_index = 0U;
    const char *text = ZSTR_VAL(query);
    size_t length = ZSTR_LEN(query);
    char quote = '\0';
    bool line_comment = false;
    bool block_comment = false;

    for (size_t index = 0U; index < length; index++) {
        char ch = text[index];

        if (line_comment) {
            smart_str_appendc(&sql, ch);
            if (mylite_mysqli_is_line_comment_terminator(ch)) {
                line_comment = false;
            }
            continue;
        }
        if (block_comment) {
            smart_str_appendc(&sql, ch);
            if (ch == '*' && index + 1U < length && text[index + 1U] == '/') {
                index++;
                smart_str_appendc(&sql, text[index]);
                block_comment = false;
            }
            continue;
        }
        if (quote != '\0') {
            smart_str_appendc(&sql, ch);
            if (ch == '\\' && index + 1U < length) {
                index++;
                smart_str_appendc(&sql, text[index]);
            } else if (ch == quote) {
                quote = '\0';
            }
            continue;
        }

        if (ch == '\'' || ch == '"' || ch == '`') {
            quote = ch;
            smart_str_appendc(&sql, ch);
            continue;
        }
        if (ch == '#' || mylite_mysqli_is_dash_comment_start(text, length, index)) {
            line_comment = true;
            smart_str_appendc(&sql, ch);
            if (ch == '-') {
                index++;
                smart_str_appendc(&sql, text[index]);
            }
            continue;
        }
        if (ch == '/' && index + 1U < length && text[index + 1U] == '*') {
            block_comment = true;
            smart_str_appendc(&sql, ch);
            index++;
            smart_str_appendc(&sql, text[index]);
            continue;
        }
        if (ch == '?') {
            zval *parameter = zend_hash_index_find(Z_ARRVAL_P(params), param_index);
            zend_string *literal = NULL;

            if (param_index >= param_count) {
                smart_str_free(&sql);
                zend_string_release(query);
                return NULL;
            }
            if (parameter == NULL) {
                smart_str_free(&sql);
                zend_string_release(query);
                return NULL;
            }
            literal = mylite_mysqli_param_to_sql(parameter);
            smart_str_append(&sql, literal);
            zend_string_release(literal);
            param_index++;
            continue;
        }
        smart_str_appendc(&sql, ch);
    }

    zend_string_release(query);
    if (param_index != param_count) {
        smart_str_free(&sql);
        return NULL;
    }

    smart_str_0(&sql);
    return sql.s;
}

static zend_string *mylite_mysqli_param_to_sql(zval *value) {
    zval copy;
    zval *copy_value = &copy;

    ZVAL_COPY(&copy, value);
    ZVAL_DEREF(copy_value);

    switch (Z_TYPE_P(copy_value)) {
    case IS_NULL:
        zval_ptr_dtor(&copy);
        return zend_string_init("NULL", strlen("NULL"), false);
    case IS_FALSE:
        zval_ptr_dtor(&copy);
        return zend_string_init("0", strlen("0"), false);
    case IS_TRUE:
        zval_ptr_dtor(&copy);
        return zend_string_init("1", strlen("1"), false);
    case IS_LONG: {
        zend_string *result = zend_strpprintf(0, ZEND_LONG_FMT, Z_LVAL_P(copy_value));
        zval_ptr_dtor(&copy);
        return result;
    }
    case IS_DOUBLE: {
        zend_string *result = zend_strpprintf(0, "%.*G", (int)EG(precision), Z_DVAL_P(copy_value));
        zval_ptr_dtor(&copy);
        return result;
    }
    default: {
        zend_string *string_value = zval_get_string(copy_value);
        zend_string *escaped =
            mylite_mysqli_escape_string(ZSTR_VAL(string_value), ZSTR_LEN(string_value));
        zend_string *result = zend_strpprintf(0, "'%s'", ZSTR_VAL(escaped));

        zend_string_release(escaped);
        zend_string_release(string_value);
        zval_ptr_dtor(&copy);
        return result;
    }
    }
}

static uint32_t mylite_mysqli_count_markers(const char *sql, size_t sql_length) {
    uint32_t count = 0U;
    char quote = '\0';
    bool line_comment = false;
    bool block_comment = false;

    for (size_t index = 0U; index < sql_length; index++) {
        char ch = sql[index];

        if (line_comment) {
            if (mylite_mysqli_is_line_comment_terminator(ch)) {
                line_comment = false;
            }
            continue;
        }
        if (block_comment) {
            if (ch == '*' && index + 1U < sql_length && sql[index + 1U] == '/') {
                index++;
                block_comment = false;
            }
            continue;
        }
        if (quote != '\0') {
            if (ch == '\\' && index + 1U < sql_length) {
                index++;
            } else if (ch == quote) {
                quote = '\0';
            }
            continue;
        }
        if (ch == '\'' || ch == '"' || ch == '`') {
            quote = ch;
        } else if (ch == '#' || mylite_mysqli_is_dash_comment_start(sql, sql_length, index)) {
            if (ch == '-') {
                index++;
            }
            line_comment = true;
        } else if (ch == '/' && index + 1U < sql_length && sql[index + 1U] == '*') {
            index++;
            block_comment = true;
        } else if (ch == '?') {
            count++;
        }
    }
    return count;
}

static bool mylite_mysqli_match_autocommit_off_assignment(const char *sql, size_t sql_length) {
    const char *cursor = sql;
    const char *end = sql + sql_length;

    cursor = mylite_mysqli_skip_space(cursor, end);
    if (!mylite_mysqli_consume_keyword(&cursor, end, "SET")) {
        return false;
    }
    cursor = mylite_mysqli_skip_space(cursor, end);
    if (!mylite_mysqli_consume_autocommit_identifier(&cursor, end)) {
        return false;
    }
    cursor = mylite_mysqli_skip_space(cursor, end);
    if (cursor == end || *cursor != '=') {
        return false;
    }
    cursor++;
    cursor = mylite_mysqli_skip_space(cursor, end);
    if (cursor == end || *cursor != '0') {
        return false;
    }
    cursor++;
    cursor = mylite_mysqli_skip_space(cursor, end);
    if (cursor < end && *cursor == ';') {
        cursor++;
        cursor = mylite_mysqli_skip_space(cursor, end);
    }
    return cursor == end;
}

static void mylite_mysqli_set_ok_result(mylite_mysqli_link *link, zval *out_result) {
    link->field_count = 0;
    link->warning_count = 0;
    link->affected_rows = 0;
    link->insert_id = 0;
    if (!mylite_mysqli_set_link_info(link, NULL)) {
        mylite_mysqli_set_error(link, MYLITE_MYSQLI_ERROR_CLIENT, "HY000", "out of memory");
        ZVAL_FALSE(out_result);
        return;
    }
    ZVAL_TRUE(out_result);
    mylite_mysqli_update_link_status_properties(link);
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

static bool mylite_mysqli_consume_autocommit_identifier(const char **cursor, const char *end) {
    static const char name[] = "autocommit";
    size_t length = sizeof(name) - 1U;

    if (*cursor < end && **cursor == '`') {
        (*cursor)++;
        if ((size_t)(end - *cursor) < length ||
            zend_binary_strcasecmp(*cursor, length, name, length) != 0) {
            return false;
        }
        *cursor += length;
        if (*cursor == end || **cursor != '`') {
            return false;
        }
        (*cursor)++;
        return true;
    }
    if ((size_t)(end - *cursor) < length ||
        zend_binary_strcasecmp(*cursor, length, name, length) != 0) {
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

static bool mylite_mysqli_is_dash_comment_start(const char *sql, size_t sql_length, size_t index) {
    if (index + 2U >= sql_length || sql[index] != '-' || sql[index + 1U] != '-') {
        return false;
    }

    unsigned char following = (unsigned char)sql[index + 2U];
    return following <= (unsigned char)' ';
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

        *out_memory =
            path_length == strlen(":memory:") && memcmp(path, ":memory:", path_length) == 0;
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

zend_string *mylite_mysqli_escape_string(const char *value, size_t length) {
    smart_str escaped = {0};

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
    return escaped.s == NULL ? zend_string_init("", 0, false) : escaped.s;
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
    switch (status) {
    case MYLITE_NOMEM:
        *out_sqlstate = "HY000";
        return MYLITE_MYSQLI_ERROR_CLIENT;
    case MYLITE_MISUSE:
        *out_sqlstate = "HY000";
        return MYLITE_MYSQLI_ERROR_CLIENT;
    case MYLITE_ERROR:
        *out_sqlstate = "HY000";
        return MYLITE_MYSQLI_ERROR_EXEC;
    default:
        *out_sqlstate = "HY000";
        return MYLITE_MYSQLI_ERROR_CLIENT;
    }
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
        1
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
        result->row_count
    );
    zend_update_property_long(
        mylite_mysqli_result_ce,
        &result->std,
        "type",
        strlen("type"),
        MYLITE_MYSQLI_STORE_RESULT
    );
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

    if (!MYLITE_MYSQLI_G(profile_enabled) || MYLITE_MYSQLI_G(profile_calls) == 0U) {
        return;
    }

    output = MYLITE_MYSQLI_G(profile_stderr) ? stderr : fopen(MYLITE_MYSQLI_G(profile_path), "a");
    if (output == NULL) {
        output = stderr;
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

    if (output != stderr) {
        fclose(output);
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
