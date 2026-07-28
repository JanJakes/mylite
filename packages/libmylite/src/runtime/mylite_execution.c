#if defined(__linux__) && !defined(_XOPEN_SOURCE)
#  define _XOPEN_SOURCE 700 /* NOLINT(bugprone-reserved-identifier): POSIX feature macro. */
#endif

#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_catalog_internal.h"
#include "mylite_catalog_string_pool.h"
#include "mylite_collation.h"
#include "mylite_connection.h"
#include "mylite_convert_tz.h"
#include "mylite_date_format.h"
#include "mylite_date_interval_second.h"
#include "mylite_datediff.h"
#include "mylite_diagnostics.h"
#include "mylite_digest.h"
#include "mylite_dynamic_string.h"
#include "mylite_execution_ast_internal.h"
#include "mylite_execution_bound_statement.h"
#include "mylite_execution_catalog.h"
#include "mylite_execution_completion.h"
#include "mylite_execution_connection_lifecycle.h"
#include "mylite_execution_ddl_sql_lowering.h"
#include "mylite_execution_ddl_sql_lowering_support.h"
#include "mylite_execution_diagnostics.h"
#include "mylite_execution_dml_numeric.h"
#include "mylite_execution_information_schema_join_plan.h"
#include "mylite_execution_information_schema_plan.h"
#include "mylite_execution_information_schema_plan_support.h"
#include "mylite_execution_information_schema_predicate.h"
#include "mylite_execution_information_schema_predicate_support.h"
#include "mylite_execution_information_schema_result.h"
#include "mylite_execution_information_schema_result_support.h"
#include "mylite_execution_information_schema_values.h"
#include "mylite_execution_loaded_catalog.h"
#include "mylite_execution_metadata_setup_metrics.h"
#include "mylite_execution_parameter_binding.h"
#include "mylite_execution_plan_types.h"
#include "mylite_execution_result_rows.h"
#include "mylite_execution_row_scalar_sql.h"
#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_charset_collation.h"
#include "mylite_execution_scalar_numeric.h"
#include "mylite_execution_scalar_regexp.h"
#include "mylite_execution_scalar_string_position.h"
#include "mylite_execution_scalar_string_transform.h"
#include "mylite_execution_scalar_temporal_format.h"
#include "mylite_execution_select_analysis.h"
#include "mylite_execution_select_order_plan.h"
#include "mylite_execution_select_order_support.h"
#include "mylite_execution_session_programs.h"
#include "mylite_execution_session_programs_support.h"
#include "mylite_execution_session_system_variables.h"
#include "mylite_execution_session_system_variables_support.h"
#include "mylite_execution_set.h"
#include "mylite_execution_set_support.h"
#include "mylite_execution_show_filter.h"
#include "mylite_execution_sql_lowering_support.h"
#include "mylite_execution_sql_normalization.h"
#include "mylite_execution_sqlite_internal.h"
#include "mylite_execution_statement_transaction.h"
#include "mylite_execution_system_variables.h"
#include "mylite_execution_table_maintenance.h"
#include "mylite_execution_table_maintenance_support.h"
#include "mylite_execution_text_internal.h"
#include "mylite_execution_transaction_control.h"
#include "mylite_execution_transaction_control_support.h"
#include "mylite_execution_value.h"
#include "mylite_execution_values.h"
#include "mylite_execution_values_support.h"
#include "mylite_integer_arithmetic.h"
#include "mylite_json.h"
#include "mylite_json_internal.h"
#include "mylite_lexer.h"
#include "mylite_mysql_error_codes.h"
#include "mylite_mysql_server_identity.h"
#include "mylite_named_locks.h"
#include "mylite_numeric_locale.h"
#include "mylite_parser.h"
#include "mylite_period_functions.h"
#ifdef MYLITE_ENABLE_PROFILING
#  include "mylite_profile_internal.h"
#endif
#include "mylite_rand.h"
#include "mylite_regexp.h"
#include "mylite_result.h"
#include "mylite_spatial.h"
#include "mylite_sqlite_registration.h"
#include "mylite_statement_completion.h"
#include "mylite_statement_context.h"
#include "mylite_string_base64.h"
#include "mylite_string_bitmask.h"
#include "mylite_string_case.h"
#include "mylite_string_char.h"
#include "mylite_string_codepoint.h"
#include "mylite_string_concat.h"
#include "mylite_string_insert.h"
#include "mylite_string_padding.h"
#include "mylite_string_quote.h"
#include "mylite_string_replace.h"
#include "mylite_string_reverse.h"
#include "mylite_string_search.h"
#include "mylite_string_soundex.h"
#include "mylite_string_substring_index.h"
#include "mylite_string_trim.h"
#include "mylite_string_unhex.h"
#include "mylite_sys_functions.h"
#include "mylite_system_functions.h"
#include "mylite_temporal_arithmetic.h"
#include "mylite_temporal_constructor.h"
#include "mylite_temporal_extract.h"
#include "mylite_timediff.h"
#include "mylite_timestamp_function.h"
#include "mylite_timestampdiff.h"
#include "mylite_unix_timestamp.h"
#include "mylite_uuid.h"
#include "mylite_weight_string.h"
#include "sqlite3.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#endif

#include "mylite_execution_declarations_00_constants.inc"
#include "mylite_execution_declarations_01_types.inc"
#include "mylite_execution_declarations_10_statement.inc"
#include "mylite_execution_declarations_20_statement_ddl.inc"
#include "mylite_execution_declarations_30_select_metadata.inc"
#include "mylite_execution_declarations_40_information_schema_show.inc"
#include "mylite_execution_declarations_50_ddl_planning.inc"
#include "mylite_execution_declarations_60_dml_query.inc"
#include "mylite_execution_declarations_70_predicate_dml.inc"
#include "mylite_execution_declarations_80_row_scalar.inc"
#include "mylite_execution_declarations_90_sql_builder.inc"
#include "mylite_execution_declarations_99_sqlite_binding.inc"

int mylite_execute(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    mylite_result **out_result
) {
    struct mylite_statement_context context;
    struct mylite_sql_parse_result parse_result;
    struct mylite_execution_normalized_sql normalized_sql;
    const struct mylite_sql_ast_node *statement = NULL;
    int64_t completed_row_count = -1;
    size_t statement_count = 0U;
    bool preserve_diagnostics_snapshot = false;
    bool completed_statement_is_select = false;
#ifdef MYLITE_ENABLE_PROFILING
    uint64_t profile_statement_started_ns = 0U;
    uint64_t profile_phase_started_ns = 0U;
#endif
    int rc = MYLITE_OK;

    if (out_result == NULL) {
        if (database != NULL) {
            mylite_diagnostics_set_error(
                mylite_connection_diagnostics(database),
                MYLITE_MISUSE,
                "HY000",
                mylite_diagnostics_misuse_message()
            );
        }
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    if (database == NULL || sql == NULL) {
        if (database != NULL) {
            mylite_diagnostics_set_error(
                mylite_connection_diagnostics(database),
                MYLITE_MISUSE,
                "HY000",
                mylite_diagnostics_misuse_message()
            );
        }
        return MYLITE_MISUSE;
    }
    rc = reject_command_with_active_cursor(database);
    if (rc != MYLITE_OK) {
        return rc;
    }

#ifdef MYLITE_ENABLE_PROFILING
    mylite_profile_enter_api(database);
    profile_statement_started_ns = mylite_profile_now_ns();
#endif
    if (database->session.statement_id != UINT64_MAX) {
        ++database->session.statement_id;
    }
    normalized_sql = (struct mylite_execution_normalized_sql){0};
#ifdef MYLITE_ENABLE_PROFILING
    profile_phase_started_ns = mylite_profile_now_ns();
#endif
    rc = mylite_execution_normalize_mysql_compat_sql(database, sql, sql_size, &normalized_sql);
#ifdef MYLITE_ENABLE_PROFILING
    mylite_profile_record_normalization(database, profile_phase_started_ns);
#endif
    if (rc != MYLITE_OK) {
#ifdef MYLITE_ENABLE_PROFILING
        mylite_profile_record_statement(database, profile_statement_started_ns);
#endif
        return rc;
    }

    mylite_statement_context_init(&context);
    rc = mylite_statement_context_begin(
        &context,
        database,
        normalized_sql.sql,
        normalized_sql.sql_size
    );
    if (rc != MYLITE_OK) {
        mylite_execution_normalized_sql_deinit(&normalized_sql);
        mylite_statement_context_deinit(&context);
#ifdef MYLITE_ENABLE_PROFILING
        mylite_profile_record_statement(database, profile_statement_started_ns);
#endif
        return rc;
    }
    mylite_statement_context_set_previous_row_count(&context, database->session.previous_row_count);
    mylite_statement_context_set_previous_found_rows(&context, database->session.found_rows);

#ifdef MYLITE_ENABLE_PROFILING
    profile_phase_started_ns = mylite_profile_now_ns();
#endif
    rc = status_from_parse_status(mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = normalized_sql.sql,
            .length = normalized_sql.sql_size,
            .modes = mylite_execution_lexer_modes_for_session_sql_mode(&database->session),
        },
        &parse_result
    ));
#ifdef MYLITE_ENABLE_PROFILING
    mylite_profile_record_parse(
        database,
        profile_phase_started_ns,
        parse_result.retry_callback_count,
        parse_result.retry_handled_count
    );
#endif
    if (rc != MYLITE_OK) {
        rc = finish_parse_failure(database, &parse_result, rc);
        mylite_sql_parse_result_deinit(&parse_result);
        (void)mylite_statement_context_end(&context, rc);
        mylite_statement_context_deinit(&context);
        mylite_execution_normalized_sql_deinit(&normalized_sql);
#ifdef MYLITE_ENABLE_PROFILING
        mylite_profile_record_statement(database, profile_statement_started_ns);
#endif
        return rc;
    }

    rc = script_statement_count(parse_result.root, &statement_count);
    if (rc == MYLITE_OK && statement_count == 0U) {
        rc = execute_empty_statement(database, out_result);
    } else if (rc == MYLITE_OK && statement_count == 1U) {
        statement = child_at(parse_result.root, 0U);
        rc = execute_parsed_statement(database, &context, statement, out_result);
    } else if (rc == MYLITE_OK) {
        set_multi_statement_parse_error(database, parse_result.root);
        rc = MYLITE_ERROR;
    }

    if (rc == MYLITE_OK) {
        completed_row_count = row_count_for_completed_statement(statement, *out_result);
        preserve_diagnostics_snapshot = statement_preserves_diagnostics_snapshot(statement);
        completed_statement_is_select = statement_result_is_select(statement, *out_result);
    }
    mylite_sql_parse_result_deinit(&parse_result);
    if (rc != MYLITE_OK) {
        rc = finish_failed_statement(database, NULL, rc, out_result);
    } else {
        rc = finish_completed_statement(
            database,
            NULL,
            completed_statement_is_select,
            completed_row_count,
            preserve_diagnostics_snapshot,
            out_result
        );
    }
    (void)mylite_statement_context_end(&context, rc);
    mylite_statement_context_deinit(&context);
    mylite_execution_normalized_sql_deinit(&normalized_sql);
#ifdef MYLITE_ENABLE_PROFILING
    mylite_profile_record_statement(database, profile_statement_started_ns);
#endif
    mylite_connection_publish_processlist_session(database);

    return rc;
}

int mylite_execute_transaction_control(
    mylite_db *database,
    enum mylite_transaction_control_statement statement,
    mylite_result **out_result
) {
    struct mylite_statement_context context;
    const char *sql = NULL;
    int64_t completed_row_count = 0;
#ifdef MYLITE_ENABLE_PROFILING
    uint64_t profile_statement_started_ns = 0U;
#endif
    int rc = MYLITE_OK;

    if (out_result == NULL) {
        if (database != NULL) {
            mylite_diagnostics_set_error(
                mylite_connection_diagnostics(database),
                MYLITE_MISUSE,
                "HY000",
                mylite_diagnostics_misuse_message()
            );
        }
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    if (database == NULL) {
        return MYLITE_MISUSE;
    }
    rc = reject_command_with_active_cursor(database);
    if (rc != MYLITE_OK) {
        return rc;
    }

    switch (statement) {
    case MYLITE_TRANSACTION_CONTROL_START:
        sql = "START TRANSACTION";
        break;
    case MYLITE_TRANSACTION_CONTROL_COMMIT:
        sql = "COMMIT";
        break;
    case MYLITE_TRANSACTION_CONTROL_ROLLBACK:
        sql = "ROLLBACK";
        break;
    default:
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_MISUSE,
            "HY000",
            mylite_diagnostics_misuse_message()
        );
        return MYLITE_MISUSE;
    }

#ifdef MYLITE_ENABLE_PROFILING
    mylite_profile_enter_api(database);
    profile_statement_started_ns = mylite_profile_now_ns();
#endif
    if (database->session.statement_id != UINT64_MAX) {
        ++database->session.statement_id;
    }
    mylite_statement_context_init(&context);
    rc = mylite_statement_context_begin(&context, database, sql, strlen(sql));
    if (rc != MYLITE_OK) {
        mylite_statement_context_deinit(&context);
#ifdef MYLITE_ENABLE_PROFILING
        mylite_profile_record_statement(database, profile_statement_started_ns);
#endif
        return rc;
    }
    mylite_statement_context_set_previous_row_count(&context, database->session.previous_row_count);
    mylite_statement_context_set_previous_found_rows(&context, database->session.found_rows);
    database->session.active_statement_time = (int64_t)mylite_statement_context_time(&context);

    switch (statement) {
    case MYLITE_TRANSACTION_CONTROL_START:
        rc = mylite_execution_execute_start_transaction_with_characteristics(
            database,
            NULL,
            out_result
        );
        break;
    case MYLITE_TRANSACTION_CONTROL_COMMIT:
        rc = mylite_execution_execute_commit_with_chain(database, false, out_result);
        break;
    case MYLITE_TRANSACTION_CONTROL_ROLLBACK:
        rc = mylite_execution_execute_rollback_with_chain(database, false, out_result);
        break;
    default:
        rc = MYLITE_MISUSE;
        break;
    }

    if (rc != MYLITE_OK) {
        rc = finish_failed_statement(database, NULL, rc, out_result);
    } else {
        completed_row_count = mylite_result_affected_rows(*out_result);
        rc = finish_completed_statement(
            database,
            NULL,
            false,
            completed_row_count,
            false,
            out_result
        );
    }
    (void)mylite_statement_context_end(&context, rc);
    mylite_statement_context_deinit(&context);
#ifdef MYLITE_ENABLE_PROFILING
    mylite_profile_record_statement(database, profile_statement_started_ns);
#endif
    mylite_connection_publish_processlist_session(database);
    return rc;
}

int mylite_prepare(mylite_db *database, const char *sql, size_t sql_size, mylite_stmt **out_stmt) {
    return prepare_statement(database, sql, sql_size, false, out_stmt);
}

int mylite_prepare_buffered(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    mylite_stmt **out_stmt
) {
    return prepare_statement(database, sql, sql_size, true, out_stmt);
}

static int prepare_statement(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    bool buffered_results,
    mylite_stmt **out_stmt
) {
    mylite_stmt *stmt = NULL;
#ifdef MYLITE_ENABLE_PROFILING
    uint64_t profile_statement_started_ns = 0U;
#endif
    int rc = MYLITE_OK;

    if (out_stmt == NULL) {
        if (database != NULL) {
            mylite_diagnostics_set_error(
                mylite_connection_diagnostics(database),
                MYLITE_MISUSE,
                "HY000",
                mylite_diagnostics_misuse_message()
            );
        }
        return MYLITE_MISUSE;
    }
    *out_stmt = NULL;
    if (database == NULL || sql == NULL) {
        if (database != NULL) {
            mylite_diagnostics_set_error(
                mylite_connection_diagnostics(database),
                MYLITE_MISUSE,
                "HY000",
                mylite_diagnostics_misuse_message()
            );
        }
        return MYLITE_MISUSE;
    }
    rc = reject_command_with_active_cursor(database);
    if (rc != MYLITE_OK) {
        return rc;
    }

#ifdef MYLITE_ENABLE_PROFILING
    mylite_profile_enter_api(database);
    profile_statement_started_ns = mylite_profile_now_ns();
#endif
    stmt = calloc(1U, sizeof(*stmt));
    if (stmt == NULL) {
        set_nomem_error(database);
#ifdef MYLITE_ENABLE_PROFILING
        mylite_profile_record_statement(database, profile_statement_started_ns);
#endif
        return MYLITE_NOMEM;
    }
    mylite_statement_completion_init(&stmt->completion);
    stmt->metadata_context = result_column_metadata_context_init();
    stmt->buffered_results = buffered_results;
    stmt->has_selected_schema = database->session.has_selected_schema;
    (void)snprintf(
        stmt->selected_schema,
        sizeof(stmt->selected_schema),
        "%s",
        database->session.selected_schema
    );
    (void)snprintf(
        stmt->character_set_client,
        sizeof(stmt->character_set_client),
        "%s",
        database->session.character_set_client
    );
    (void)snprintf(
        stmt->character_set_connection,
        sizeof(stmt->character_set_connection),
        "%s",
        database->session.character_set_connection
    );
    (void)snprintf(
        stmt->collation_connection,
        sizeof(stmt->collation_connection),
        "%s",
        database->session.collation_connection
    );
    rc = prepare_cursor_select_statement(database, sql, sql_size, stmt);
    if (rc != MYLITE_OK) {
        destroy_cursor_statement(stmt);
#ifdef MYLITE_ENABLE_PROFILING
        mylite_profile_record_statement(database, profile_statement_started_ns);
#endif
        return rc;
    }

    register_live_statement(stmt);
    *out_stmt = stmt;
#ifdef MYLITE_ENABLE_PROFILING
    mylite_profile_record_statement(database, profile_statement_started_ns);
#endif
    return MYLITE_OK;
}

size_t mylite_stmt_parameter_count(const mylite_stmt *stmt) {
    return stmt == NULL ? 0U : stmt->parameter_count;
}

const char *mylite_stmt_info(const mylite_stmt *stmt) {
    return stmt == NULL ? NULL : mylite_result_info(stmt->completed_result);
}

const char *mylite_execution_bound_statement_character_set_client(const mylite_stmt *stmt) {
    return stmt == NULL ? NULL : stmt->character_set_client;
}

const char *mylite_execution_bound_statement_character_set_connection(const mylite_stmt *stmt) {
    return stmt == NULL ? NULL : stmt->character_set_connection;
}

const char *mylite_execution_bound_statement_collation_connection(const mylite_stmt *stmt) {
    return stmt == NULL ? NULL : stmt->collation_connection;
}

int mylite_stmt_bind_null(mylite_stmt *stmt, size_t index) {
    int rc = validate_stmt_binding_index(stmt, index);

    if (rc == MYLITE_OK) {
        stmt->bindings[index].type = MYLITE_STMT_BINDING_NULL;
        stmt->bindings[index].size = 0U;
    }
    return rc;
}

int mylite_stmt_bind_int64(mylite_stmt *stmt, size_t index, int64_t value) {
    int rc = validate_stmt_binding_index(stmt, index);

    if (rc == MYLITE_OK) {
        stmt->bindings[index].type = MYLITE_STMT_BINDING_INT64;
        stmt->bindings[index].scalar.int64_value = value;
        stmt->bindings[index].size = 0U;
    }
    return rc;
}

int mylite_stmt_bind_uint64(mylite_stmt *stmt, size_t index, uint64_t value) {
    int rc = validate_stmt_binding_index(stmt, index);

    if (rc == MYLITE_OK) {
        stmt->bindings[index].type = MYLITE_STMT_BINDING_UINT64;
        stmt->bindings[index].scalar.uint64_value = value;
        stmt->bindings[index].size = 0U;
    }
    return rc;
}

int mylite_stmt_bind_double(mylite_stmt *stmt, size_t index, double value) {
    int rc = validate_stmt_binding_index(stmt, index);

    if (rc == MYLITE_OK) {
        stmt->bindings[index].type = MYLITE_STMT_BINDING_DOUBLE;
        stmt->bindings[index].scalar.double_value = value;
        stmt->bindings[index].size = 0U;
    }
    return rc;
}

int mylite_stmt_bind_text(mylite_stmt *stmt, size_t index, const char *value, size_t value_size) {
    const struct stmt_bytes_binding_request request = {
        .index = index,
        .type = MYLITE_STMT_BINDING_TEXT,
        .value = value,
        .value_size = value_size,
    };

    return bind_stmt_bytes(stmt, &request);
}

int mylite_stmt_bind_blob(mylite_stmt *stmt, size_t index, const void *value, size_t value_size) {
    const struct stmt_bytes_binding_request request = {
        .index = index,
        .type = MYLITE_STMT_BINDING_BLOB,
        .value = value,
        .value_size = value_size,
    };

    return bind_stmt_bytes(stmt, &request);
}

int mylite_stmt_clear_bindings(mylite_stmt *stmt) {
    if (stmt == NULL || stmt->database == NULL || stmt->current_row_available) {
        if (stmt != NULL && stmt->database != NULL) {
            mylite_diagnostics_set_error(
                mylite_connection_diagnostics(stmt->database),
                MYLITE_MISUSE,
                "HY000",
                mylite_diagnostics_misuse_message()
            );
        }
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; index < stmt->parameter_count; ++index) {
        stmt->bindings[index].type = MYLITE_STMT_BINDING_UNBOUND;
        stmt->bindings[index].size = 0U;
    }
    return MYLITE_OK;
}

int mylite_stmt_reset(mylite_stmt *stmt) {
    if (stmt == NULL || stmt->database == NULL) {
        return MYLITE_MISUSE;
    }
    if (stmt->database->active_cursor != NULL && stmt->database->active_cursor != stmt) {
        return reject_command_with_active_cursor(stmt->database);
    }
    return reset_cursor_execution(stmt);
}

int64_t mylite_stmt_affected_rows(const mylite_stmt *stmt) {
    return stmt == NULL ? -1 : stmt->completion.affected_rows;
}

size_t mylite_stmt_buffered_row_count(const mylite_stmt *stmt) {
    if (stmt == NULL || !stmt->buffered_results || stmt->metadata_result == NULL) {
        return 0U;
    }
    return mylite_result_row_count(stmt->metadata_result);
}

uint64_t mylite_stmt_insert_id(const mylite_stmt *stmt) {
    return stmt == NULL ? 0U : stmt->completion.insert_id;
}

int mylite_stmt_errcode(const mylite_stmt *stmt) {
    return stmt == NULL ? MYLITE_MISUSE : mylite_diagnostics_errcode(&stmt->completion.diagnostics);
}

const char *mylite_stmt_sqlstate(const mylite_stmt *stmt) {
    return stmt == NULL ? mylite_diagnostics_misuse_sqlstate()
                        : mylite_diagnostics_sqlstate(&stmt->completion.diagnostics);
}

const char *mylite_stmt_errmsg(const mylite_stmt *stmt) {
    if (stmt == NULL) {
        return mylite_diagnostics_misuse_message();
    }
    if (mylite_diagnostics_errcode(&stmt->completion.diagnostics) == MYLITE_OK) {
        return "";
    }
    return mylite_diagnostics_errmsg(&stmt->completion.diagnostics);
}

size_t mylite_stmt_warning_count(const mylite_stmt *stmt) {
    return stmt == NULL ? 0U
                        : mylite_diagnostics_warning_total_count(&stmt->completion.diagnostics);
}

size_t mylite_stmt_warning_record_count(const mylite_stmt *stmt) {
    return stmt == NULL ? 0U : mylite_diagnostics_warning_count(&stmt->completion.diagnostics);
}

int mylite_stmt_warning_at(
    const mylite_stmt *stmt,
    size_t index,
    struct mylite_diagnostic *out_diagnostic
) {
    if (stmt == NULL) {
        if (out_diagnostic != NULL) {
            *out_diagnostic = (struct mylite_diagnostic){0};
        }
        return MYLITE_MISUSE;
    }
    return mylite_diagnostics_copy_warning_at(&stmt->completion.diagnostics, index, out_diagnostic);
}

static int validate_stmt_binding_index(mylite_stmt *stmt, size_t index) {
    if (stmt == NULL || stmt->database == NULL || stmt->current_row_available ||
        index >= stmt->parameter_count) {
        if (stmt != NULL && stmt->database != NULL) {
            mylite_diagnostics_set_error(
                mylite_connection_diagnostics(stmt->database),
                MYLITE_MISUSE,
                "HY000",
                mylite_diagnostics_misuse_message()
            );
        }
        return MYLITE_MISUSE;
    }
    return MYLITE_OK;
}

static int bind_stmt_bytes(mylite_stmt *stmt, const struct stmt_bytes_binding_request *request) {
    struct mylite_stmt_binding *binding = NULL;
    unsigned char *bytes = NULL;
    int rc = request == NULL ? MYLITE_MISUSE : validate_stmt_binding_index(stmt, request->index);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (request->value == NULL && request->value_size != 0U) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(stmt->database),
            MYLITE_MISUSE,
            "HY000",
            mylite_diagnostics_misuse_message()
        );
        return MYLITE_MISUSE;
    }

    binding = &stmt->bindings[request->index];
    if (request->value_size > binding->capacity) {
        bytes = malloc(request->value_size);
        if (bytes == NULL) {
            set_nomem_error(stmt->database);
            return MYLITE_NOMEM;
        }
        if (request->value_size != 0U) {
            memcpy(bytes, request->value, request->value_size);
        }
        free(binding->bytes);
        binding->bytes = bytes;
        binding->capacity = request->value_size;
    } else if (request->value_size != 0U) {
        memmove(binding->bytes, request->value, request->value_size);
    }
    binding->type = request->type;
    binding->size = request->value_size;
    return MYLITE_OK;
}

static int reset_cursor_execution(mylite_stmt *stmt) {
    struct mylite_db *database = stmt->database;
    int rc = reject_poisoned_connection(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!stmt->execution_started) {
        stmt->current_row_available = false;
        stmt->done = false;
        return MYLITE_OK;
    }
    if (stmt->sqlite_statement != NULL) {
        rc = finish_cursor_sqlite_statement(stmt, MYLITE_OK);
    }
    rc = rollback_statement_transaction(database, &stmt->read_transaction, rc);
    if (database->active_cursor == stmt) {
        database->active_cursor = NULL;
    }
    if (stmt->has_context) {
        (void)mylite_statement_context_end(&stmt->context, rc);
        mylite_statement_context_deinit(&stmt->context);
        stmt->has_context = false;
    }
    if (mylite_statement_result_may_have_rows(stmt->result_capability)) {
        clear_cursor_select_plan_resources(stmt, rc);
    } else if (stmt->completed_result != NULL) {
        mylite_result_free(stmt->reusable_command_result);
        stmt->reusable_command_result = stmt->completed_result;
        mylite_result_reset_command(stmt->reusable_command_result);
        stmt->completed_result = NULL;
    }
    stmt->row_count = 0U;
    mylite_statement_completion_reset(&stmt->completion);
    stmt->materialized_row_index = 0U;
    stmt->current_materialized_row = 0U;
    stmt->current_row_available = false;
    stmt->execution_started = false;
    stmt->done = false;
    return rc;
}

static int start_cursor_execution(mylite_stmt *stmt) {
    struct mylite_db *database = stmt->database;
    mylite_stmt *saved_active_bound_statement = database->active_bound_statement;
    bool saved_has_selected_schema = database->session.has_selected_schema;
    bool streams_rows = stmt->result_capability == MYLITE_STATEMENT_RESULT_STREAMING_QUERY;
    char saved_selected_schema[MYLITE_SESSION_SCHEMA_CAPACITY] = {0};
    int rc = validate_stmt_bindings_complete(stmt);

    if (rc != MYLITE_OK) {
        return rc;
    }
    stmt->execution_started = true;
    (void)snprintf(
        saved_selected_schema,
        sizeof(saved_selected_schema),
        "%s",
        database->session.selected_schema
    );
    database->session.has_selected_schema = stmt->has_selected_schema;
    (void)snprintf(
        database->session.selected_schema,
        sizeof(database->session.selected_schema),
        "%s",
        stmt->selected_schema
    );
    if (rc == MYLITE_OK && database->session.statement_id != UINT64_MAX) {
        ++database->session.statement_id;
    }
    if (rc == MYLITE_OK) {
        stmt->statement_id = database->session.statement_id;
        mylite_statement_context_init(&stmt->context);
        rc = mylite_statement_context_begin(
            &stmt->context,
            database,
            stmt->normalized_sql.sql,
            stmt->normalized_sql.sql_size
        );
    }
    if (rc == MYLITE_OK) {
        stmt->has_context = true;
        mylite_statement_context_set_previous_row_count(
            &stmt->context,
            database->session.previous_row_count
        );
        mylite_statement_context_set_previous_found_rows(
            &stmt->context,
            database->session.found_rows
        );
        database->session.active_statement_time =
            (int64_t)mylite_statement_context_time(&stmt->context);
        if (!streams_rows) {
            rc = execute_prepared_materialized_statement(stmt);
        } else if (stmt->statement != NULL) {
            rc = mylite_execution_prepare_statement_transaction_boundary(database, stmt->statement);
        }
    }
    if (rc == MYLITE_OK && streams_rows) {
        rc = begin_read_statement_transaction(database, &stmt->read_transaction);
    }
    if (rc == MYLITE_OK && streams_rows) {
        database->active_bound_statement = stmt;
        rc = prepare_cursor_select_execution(stmt);
        database->active_bound_statement = saved_active_bound_statement;
    }
    if (rc == MYLITE_OK && streams_rows && !stmt->has_materialized_rows) {
        database->active_cursor = stmt;
    }
    if (rc != MYLITE_OK && streams_rows) {
        mylite_result *result = NULL;

        rc = rollback_statement_transaction(database, &stmt->read_transaction, rc);
        rc = finish_failed_statement(database, &stmt->completion, rc, &result);
        if (stmt->has_context) {
            (void)mylite_statement_context_end(&stmt->context, rc);
            mylite_statement_context_deinit(&stmt->context);
            stmt->has_context = false;
        }
        stmt->done = true;
    }
    database->session.has_selected_schema = saved_has_selected_schema;
    (void)snprintf(
        database->session.selected_schema,
        sizeof(database->session.selected_schema),
        "%s",
        saved_selected_schema
    );
    return rc;
}

static int execute_prepared_materialized_statement(mylite_stmt *stmt) {
    struct mylite_db *database = stmt->database;
    mylite_stmt *saved_active_bound_statement = database->active_bound_statement;
    mylite_result *result = NULL;
    int64_t completed_row_count = -1;
    bool returns_rows = mylite_statement_result_may_have_rows(stmt->result_capability);
    bool preserve_diagnostics_snapshot = false;
    int rc = MYLITE_OK;

    database->active_bound_statement = stmt;
    rc = execute_parsed_statement(database, &stmt->context, stmt->statement, &result);
    database->active_bound_statement = saved_active_bound_statement;
    if (rc == MYLITE_OK) {
        if (mylite_statement_result_is_dynamic(stmt->result_capability)) {
            returns_rows = result != NULL && mylite_result_column_count(result) != 0U;
        }
        completed_row_count = row_count_for_completed_statement(stmt->statement, result);
        preserve_diagnostics_snapshot = statement_preserves_diagnostics_snapshot(stmt->statement);
        rc = finish_completed_statement(
            database,
            &stmt->completion,
            statement_result_is_select(stmt->statement, result),
            completed_row_count,
            preserve_diagnostics_snapshot,
            &result
        );
    } else {
        rc = finish_failed_statement(database, &stmt->completion, rc, &result);
    }
    if (rc == MYLITE_OK && returns_rows) {
        stmt->metadata_result = result;
        stmt->has_materialized_rows = true;
        result = NULL;
    } else if (rc == MYLITE_OK) {
        stmt->completed_result = result;
        result = NULL;
    }
    mylite_result_free(result);
    if (stmt->has_context) {
        (void)mylite_statement_context_end(&stmt->context, rc);
        mylite_statement_context_deinit(&stmt->context);
        stmt->has_context = false;
    }
    stmt->done = rc != MYLITE_OK || !returns_rows;
    mylite_connection_publish_processlist_session(database);
    return rc;
}

static int validate_stmt_bindings_complete(mylite_stmt *stmt) {
    for (size_t index = 0U; index < stmt->parameter_count; ++index) {
        if (stmt->bindings[index].type == MYLITE_STMT_BINDING_UNBOUND) {
            mylite_diagnostics_set_error(
                mylite_connection_diagnostics(stmt->database),
                MYLITE_MISUSE,
                "HY000",
                "No data supplied for one or more prepared statement parameters"
            );
            return MYLITE_MISUSE;
        }
    }
    return MYLITE_OK;
}

static int copy_active_stmt_parameter_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *parameter,
    struct planned_value *out_value
) {
    const struct mylite_stmt_binding *binding = NULL;
    size_t index = 0U;

    if (database == NULL || parameter == NULL || out_value == NULL ||
        parameter->kind != MYLITE_SQL_AST_PARAMETER || database->active_bound_statement == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct planned_value){0};
    index = mylite_sql_ast_node_parameter_index(parameter);
    if (index >= database->active_bound_statement->parameter_count) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    binding = &database->active_bound_statement->bindings[index];
    out_value->is_external_parameter = true;
    out_value->external_parameter_index = index;
    out_value->external_binding = binding;
    if (binding->type == MYLITE_STMT_BINDING_UNBOUND &&
        database->active_bound_statement->analyzing_unbound_parameters) {
        return MYLITE_OK;
    }
    switch (binding->type) {
    case MYLITE_STMT_BINDING_NULL:
        out_value->is_null = true;
        return MYLITE_OK;
    case MYLITE_STMT_BINDING_INT64:
        out_value->integer = binding->scalar.int64_value;
        return MYLITE_OK;
    case MYLITE_STMT_BINDING_UINT64:
        if (binding->scalar.uint64_value <= (uint64_t)INT64_MAX) {
            out_value->integer = (int64_t)binding->scalar.uint64_value;
            return MYLITE_OK;
        }
        {
            char text[integer_text_capacity];
            int written = snprintf(text, sizeof(text), "%" PRIu64, binding->scalar.uint64_value);

            if (written <= 0 || (size_t)written >= sizeof(text)) {
                return MYLITE_ERROR;
            }
            out_value->text = malloc((size_t)written);
            if (out_value->text == NULL) {
                set_nomem_error(database);
                return MYLITE_NOMEM;
            }
            memcpy(out_value->text, text, (size_t)written);
            out_value->is_text = true;
            out_value->text_length = (size_t)written;
            return MYLITE_OK;
        }
    case MYLITE_STMT_BINDING_DOUBLE:
        out_value->is_real = true;
        out_value->real = binding->scalar.double_value;
        return MYLITE_OK;
    case MYLITE_STMT_BINDING_TEXT:
    case MYLITE_STMT_BINDING_BLOB:
        if (binding->size != 0U) {
            out_value->text = malloc(binding->size);
            if (out_value->text == NULL) {
                set_nomem_error(database);
                return MYLITE_NOMEM;
            }
            memcpy(out_value->text, binding->bytes, binding->size);
        }
        out_value->is_text = binding->type == MYLITE_STMT_BINDING_TEXT;
        out_value->is_blob = binding->type == MYLITE_STMT_BINDING_BLOB;
        out_value->text_length = binding->size;
        return MYLITE_OK;
    case MYLITE_STMT_BINDING_UNBOUND:
        break;
    }

    return MYLITE_MISUSE;
}

static int copy_active_stmt_parameter_cell(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *parameter,
    struct session_scalar_cell *out_cell
) {
    static const char empty_value[] = "";
    const struct mylite_stmt_binding *binding = NULL;
    size_t index = 0U;
    int written = 0;

    if (database == NULL || parameter == NULL || out_cell == NULL ||
        parameter->kind != MYLITE_SQL_AST_PARAMETER || database->active_bound_statement == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    index = mylite_sql_ast_node_parameter_index(parameter);
    if (index >= database->active_bound_statement->parameter_count) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    binding = &database->active_bound_statement->bindings[index];
    database->active_bound_statement->parameter_plan_reusable = false;
    if (binding->type == MYLITE_STMT_BINDING_UNBOUND &&
        database->active_bound_statement->analyzing_unbound_parameters) {
        out_cell->value = "0";
        return MYLITE_OK;
    }
    switch (binding->type) {
    case MYLITE_STMT_BINDING_NULL:
        return MYLITE_OK;
    case MYLITE_STMT_BINDING_INT64:
        written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRId64,
            binding->scalar.int64_value
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    case MYLITE_STMT_BINDING_UINT64:
        written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRIu64,
            binding->scalar.uint64_value
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    case MYLITE_STMT_BINDING_DOUBLE:
        if (mylite_execution_format_double_text(
                database,
                binding->scalar.double_value,
                "prepared statement parameter",
                out_cell->double_text,
                sizeof(out_cell->double_text)
            ) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->double_text;
        return MYLITE_OK;
    case MYLITE_STMT_BINDING_TEXT:
    case MYLITE_STMT_BINDING_BLOB:
        out_cell->value = binding->size == 0U ? empty_value : (const char *)binding->bytes;
        out_cell->value_size = binding->size;
        out_cell->has_value_size = true;
        return MYLITE_OK;
    case MYLITE_STMT_BINDING_UNBOUND:
        break;
    }
    return MYLITE_MISUSE;
}

int mylite_stmt_step(mylite_stmt *stmt) {
    int sqlite_rc = SQLITE_OK;
#ifdef MYLITE_ENABLE_PROFILING
    uint64_t profile_step_started_ns = 0U;
#endif
    int rc = MYLITE_OK;

    if (stmt == NULL || stmt->database == NULL) {
        return MYLITE_MISUSE;
    }
#ifdef MYLITE_ENABLE_PROFILING
    mylite_profile_enter_api(stmt->database);
    profile_step_started_ns = mylite_profile_now_ns();
#endif
    rc = reject_poisoned_connection(stmt->database);
    if (rc != MYLITE_OK) {
#ifdef MYLITE_ENABLE_PROFILING
        mylite_profile_record_cursor_step(stmt->database, profile_step_started_ns, false, 0U);
#endif
        return rc;
    }
    if (!stmt->execution_started && !stmt->has_materialized_rows) {
        if (stmt->done) {
#ifdef MYLITE_ENABLE_PROFILING
            mylite_profile_record_cursor_step(stmt->database, profile_step_started_ns, false, 0U);
#endif
            return MYLITE_DONE;
        }
        rc = start_cursor_execution(stmt);
        if (rc != MYLITE_OK) {
#ifdef MYLITE_ENABLE_PROFILING
            mylite_profile_record_cursor_step(stmt->database, profile_step_started_ns, false, 0U);
#endif
            return rc;
        }
    }
    if (!stmt->execution_started && stmt->has_materialized_rows) {
        stmt->execution_started = true;
    }
    if (stmt->done) {
#ifdef MYLITE_ENABLE_PROFILING
        mylite_profile_record_cursor_step(stmt->database, profile_step_started_ns, false, 0U);
#endif
        return MYLITE_DONE;
    }

    stmt->current_row_available = false;
    if (stmt->has_materialized_rows) {
        if (stmt->materialized_row_index >= mylite_result_row_count(stmt->metadata_result)) {
            stmt->done = true;
            if (stmt->completion.published) {
#ifdef MYLITE_ENABLE_PROFILING
                mylite_profile_record_cursor_step(
                    stmt->database,
                    profile_step_started_ns,
                    false,
                    0U
                );
#endif
                return MYLITE_DONE;
            }
            rc = set_materialized_cursor_found_row_count(stmt);
            if (rc != MYLITE_OK) {
#ifdef MYLITE_ENABLE_PROFILING
                mylite_profile_record_cursor_step(
                    stmt->database,
                    profile_step_started_ns,
                    false,
                    0U
                );
#endif
                return rc;
            }
            rc = finish_cursor_statement(stmt, true);
#ifdef MYLITE_ENABLE_PROFILING
            mylite_profile_record_cursor_step(stmt->database, profile_step_started_ns, false, 0U);
#endif
            return rc == MYLITE_OK ? MYLITE_DONE : rc;
        }
        stmt->current_materialized_row = stmt->materialized_row_index;
        ++stmt->materialized_row_index;
        ++stmt->row_count;
        stmt->current_row_available = true;
#ifdef MYLITE_ENABLE_PROFILING
        {
            size_t value_bytes = 0U;

            for (size_t column_index = 0U;
                 column_index < mylite_result_column_count(stmt->metadata_result);
                 ++column_index) {
                value_bytes += mylite_result_value_size(
                    stmt->metadata_result,
                    stmt->current_materialized_row,
                    column_index
                );
            }
            mylite_profile_record_cursor_step(
                stmt->database,
                profile_step_started_ns,
                true,
                value_bytes
            );
        }
#endif
        return MYLITE_ROW;
    }

    sqlite_rc = sqlite3_step(stmt->sqlite_statement);
    if (sqlite_rc == SQLITE_ROW) {
        rc = mylite_execution_read_sqlite_result_row(
            stmt->database,
            stmt->sqlite_statement,
            mylite_result_column_count(stmt->metadata_result),
            stmt->analyzed_select.plan.columns,
            stmt->analyzed_select.plan.column_count,
            &stmt->row_storage
        );
        if (rc != MYLITE_OK) {
            rc = finish_failed_cursor_statement(stmt, rc);
#ifdef MYLITE_ENABLE_PROFILING
            mylite_profile_record_cursor_step(stmt->database, profile_step_started_ns, false, 0U);
#endif
            return rc;
        }
        ++stmt->row_count;
        stmt->current_row_available = true;
#ifdef MYLITE_ENABLE_PROFILING
        {
            size_t value_bytes = 0U;

            for (size_t column_index = 0U;
                 column_index < mylite_result_column_count(stmt->metadata_result);
                 ++column_index) {
                if (!stmt->row_storage.values[column_index].is_null) {
                    value_bytes += stmt->row_storage.values[column_index].size;
                }
            }
            mylite_profile_record_cursor_step(
                stmt->database,
                profile_step_started_ns,
                true,
                value_bytes
            );
        }
#endif
        return MYLITE_ROW;
    }
    if (sqlite_rc != SQLITE_DONE) {
        rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        rc = finish_failed_cursor_statement(stmt, rc);
#ifdef MYLITE_ENABLE_PROFILING
        mylite_profile_record_cursor_step(stmt->database, profile_step_started_ns, false, 0U);
#endif
        return rc;
    }

    rc = finish_cursor_sqlite_statement(stmt, MYLITE_OK);
    stmt->done = true;
    if (rc == MYLITE_OK) {
        rc = set_cursor_found_row_count(stmt);
    }
    if (rc == MYLITE_OK) {
        rc = finish_cursor_statement(stmt, true);
    }
#ifdef MYLITE_ENABLE_PROFILING
    mylite_profile_record_cursor_step(stmt->database, profile_step_started_ns, false, 0U);
#endif
    return rc == MYLITE_OK ? MYLITE_DONE : rc;
}

int mylite_stmt_finalize(mylite_stmt *stmt) {
#ifdef MYLITE_ENABLE_PROFILING
    mylite_db *profile_database = NULL;
    uint64_t profile_finalize_started_ns = 0U;
#endif
    int rc = MYLITE_OK;

    if (stmt == NULL) {
        return MYLITE_OK;
    }
#ifdef MYLITE_ENABLE_PROFILING
    profile_database = stmt->database;
    if (profile_database != NULL) {
        mylite_profile_enter_api(profile_database);
        profile_finalize_started_ns = mylite_profile_now_ns();
    }
#endif
    if (stmt->database == NULL) {
        destroy_cursor_statement(stmt);
        return MYLITE_OK;
    }
    if (stmt->sqlite_statement != NULL) {
        rc = finish_cursor_sqlite_statement(stmt, MYLITE_OK);
    }
    if (rc == MYLITE_OK && stmt->execution_started && !stmt->done && !stmt->completion.published) {
        stmt->completion.found_rows = (uint64_t)stmt->row_count;
        rc = finish_cursor_statement(stmt, false);
    }
    destroy_cursor_statement(stmt);
#ifdef MYLITE_ENABLE_PROFILING
    if (profile_database != NULL) {
        mylite_profile_record_cursor_finalize(profile_database, profile_finalize_started_ns);
    }
#endif
    return rc;
}

size_t mylite_stmt_column_count(const mylite_stmt *stmt) {
    if (stmt == NULL || stmt->metadata_result == NULL) {
        return 0U;
    }
    return mylite_result_column_count(stmt->metadata_result);
}

const char *mylite_stmt_column_name(const mylite_stmt *stmt, size_t column_index) {
    if (stmt == NULL || stmt->metadata_result == NULL) {
        return NULL;
    }
    return mylite_result_column_name(stmt->metadata_result, column_index);
}

const char *mylite_stmt_column_schema_name(const mylite_stmt *stmt, size_t column_index) {
    if (stmt == NULL || stmt->metadata_result == NULL) {
        return NULL;
    }
    return mylite_result_column_schema_name(stmt->metadata_result, column_index);
}

const char *mylite_stmt_column_table_name(const mylite_stmt *stmt, size_t column_index) {
    if (stmt == NULL || stmt->metadata_result == NULL) {
        return NULL;
    }
    return mylite_result_column_table_name(stmt->metadata_result, column_index);
}

const char *mylite_stmt_column_origin_schema_name(const mylite_stmt *stmt, size_t column_index) {
    if (stmt == NULL || stmt->metadata_result == NULL) {
        return NULL;
    }
    return mylite_result_column_origin_schema_name(stmt->metadata_result, column_index);
}

const char *mylite_stmt_column_origin_table_name(const mylite_stmt *stmt, size_t column_index) {
    if (stmt == NULL || stmt->metadata_result == NULL) {
        return NULL;
    }
    return mylite_result_column_origin_table_name(stmt->metadata_result, column_index);
}

const char *mylite_stmt_column_origin_name(const mylite_stmt *stmt, size_t column_index) {
    if (stmt == NULL || stmt->metadata_result == NULL) {
        return NULL;
    }
    return mylite_result_column_origin_name(stmt->metadata_result, column_index);
}

enum mylite_result_column_type mylite_stmt_column_type(
    const mylite_stmt *stmt,
    size_t column_index
) {
    if (stmt == NULL || stmt->metadata_result == NULL) {
        return MYLITE_RESULT_COLUMN_TYPE_UNKNOWN;
    }
    return mylite_result_column_type(stmt->metadata_result, column_index);
}

uint32_t mylite_stmt_column_flags(const mylite_stmt *stmt, size_t column_index) {
    if (stmt == NULL || stmt->metadata_result == NULL) {
        return 0U;
    }
    return mylite_result_column_flags(stmt->metadata_result, column_index);
}

uint32_t mylite_stmt_column_charset_id(const mylite_stmt *stmt, size_t column_index) {
    if (stmt == NULL || stmt->metadata_result == NULL) {
        return 0U;
    }
    return mylite_result_column_charset_id(stmt->metadata_result, column_index);
}

uint32_t mylite_stmt_column_collation_id(const mylite_stmt *stmt, size_t column_index) {
    if (stmt == NULL || stmt->metadata_result == NULL) {
        return 0U;
    }
    return mylite_result_column_collation_id(stmt->metadata_result, column_index);
}

uint64_t mylite_stmt_column_display_length(const mylite_stmt *stmt, size_t column_index) {
    if (stmt == NULL || stmt->metadata_result == NULL) {
        return 0U;
    }
    return mylite_result_column_display_length(stmt->metadata_result, column_index);
}

uint16_t mylite_stmt_column_decimals(const mylite_stmt *stmt, size_t column_index) {
    if (stmt == NULL || stmt->metadata_result == NULL) {
        return 0U;
    }
    return mylite_result_column_decimals(stmt->metadata_result, column_index);
}

int mylite_stmt_column_nullable(const mylite_stmt *stmt, size_t column_index) {
    if (stmt == NULL || stmt->metadata_result == NULL) {
        return 1;
    }
    return mylite_result_column_nullable(stmt->metadata_result, column_index);
}

int mylite_stmt_value_is_null(const mylite_stmt *stmt, size_t column_index) {
    if (stmt == NULL || !stmt->current_row_available ||
        column_index >= mylite_stmt_column_count(stmt)) {
        return 1;
    }
    if (stmt->has_materialized_rows) {
        return mylite_result_value_is_null(
            stmt->metadata_result,
            stmt->current_materialized_row,
            column_index
        );
    }
    return stmt->row_storage.values[column_index].is_null ? 1 : 0;
}

const char *mylite_stmt_value_text(const mylite_stmt *stmt, size_t column_index) {
    return (const char *)mylite_stmt_value_bytes(stmt, column_index);
}

const void *mylite_stmt_value_bytes(const mylite_stmt *stmt, size_t column_index) {
    const struct mylite_result_cell *cell = NULL;

    if (stmt == NULL || !stmt->current_row_available ||
        column_index >= mylite_stmt_column_count(stmt)) {
        return NULL;
    }
    if (stmt->has_materialized_rows) {
        return mylite_result_value_bytes(
            stmt->metadata_result,
            stmt->current_materialized_row,
            column_index
        );
    }
    cell = &stmt->row_storage.values[column_index];
    return cell->is_null ? NULL : cell->bytes;
}

size_t mylite_stmt_value_size(const mylite_stmt *stmt, size_t column_index) {
    const struct mylite_result_cell *cell = NULL;

    if (stmt == NULL || !stmt->current_row_available ||
        column_index >= mylite_stmt_column_count(stmt)) {
        return 0U;
    }
    if (stmt->has_materialized_rows) {
        return mylite_result_value_size(
            stmt->metadata_result,
            stmt->current_materialized_row,
            column_index
        );
    }
    cell = &stmt->row_storage.values[column_index];
    return cell->is_null ? 0U : cell->size;
}

static int prepare_cursor_select_statement(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    mylite_stmt *stmt
) {
    size_t statement_count = 0U;
#ifdef MYLITE_ENABLE_PROFILING
    uint64_t profile_phase_started_ns = 0U;
#endif
    int rc = MYLITE_OK;

    stmt->database = database;
    if (database->session.statement_id != UINT64_MAX) {
        ++database->session.statement_id;
    }
    stmt->statement_id = database->session.statement_id;

#ifdef MYLITE_ENABLE_PROFILING
    profile_phase_started_ns = mylite_profile_now_ns();
#endif
    rc =
        mylite_execution_normalize_mysql_compat_sql(database, sql, sql_size, &stmt->normalized_sql);
#ifdef MYLITE_ENABLE_PROFILING
    mylite_profile_record_normalization(database, profile_phase_started_ns);
#endif
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (stmt->normalized_sql.owned_sql == NULL) {
        char *owned_sql = NULL;

        if (stmt->normalized_sql.sql_size == SIZE_MAX) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        owned_sql = malloc(stmt->normalized_sql.sql_size + 1U);
        if (owned_sql == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        memcpy(owned_sql, stmt->normalized_sql.sql, stmt->normalized_sql.sql_size);
        owned_sql[stmt->normalized_sql.sql_size] = '\0';
        stmt->normalized_sql.sql = owned_sql;
        stmt->normalized_sql.owned_sql = owned_sql;
    }
    stmt->has_normalized_sql = true;

    mylite_statement_context_init(&stmt->context);
    rc = mylite_statement_context_begin(
        &stmt->context,
        database,
        stmt->normalized_sql.sql,
        stmt->normalized_sql.sql_size
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    stmt->has_context = true;
    mylite_statement_context_set_previous_row_count(
        &stmt->context,
        database->session.previous_row_count
    );
    mylite_statement_context_set_previous_found_rows(&stmt->context, database->session.found_rows);

#ifdef MYLITE_ENABLE_PROFILING
    profile_phase_started_ns = mylite_profile_now_ns();
#endif
    stmt->parse_modes = mylite_execution_lexer_modes_for_session_sql_mode(&database->session);
    rc = status_from_parse_status(mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = stmt->normalized_sql.sql,
            .length = stmt->normalized_sql.sql_size,
            .modes = stmt->parse_modes,
            .allow_parameters = true,
        },
        &stmt->parse_result
    ));
#ifdef MYLITE_ENABLE_PROFILING
    mylite_profile_record_parse(
        database,
        profile_phase_started_ns,
        stmt->parse_result.retry_callback_count,
        stmt->parse_result.retry_handled_count
    );
#endif
    stmt->has_parse_result = true;
    if (rc != MYLITE_OK) {
        rc = finish_parse_failure(database, &stmt->parse_result, rc);
        (void)mylite_statement_context_end(&stmt->context, rc);
        mylite_statement_context_deinit(&stmt->context);
        stmt->has_context = false;
        return rc;
    }

    rc = script_statement_count(stmt->parse_result.root, &statement_count);
    if (rc == MYLITE_OK && statement_count != 1U) {
        set_unsupported_error(database, "prepare supports exactly one statement");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        stmt->statement = child_at(stmt->parse_result.root, 0U);
        if (!prepared_statement_kind_is_supported(stmt->statement)) {
            set_unsupported_error(database, "statement type is not supported by prepare");
            rc = MYLITE_ERROR;
        } else {
            stmt->result_capability = mylite_statement_result_capability(stmt->statement);
        }
    }
    if (rc == MYLITE_OK && stmt->parse_result.parameter_count != 0U) {
        if (stmt->parse_result.parameter_count > SIZE_MAX / sizeof(*stmt->bindings)) {
            set_nomem_error(database);
            rc = MYLITE_NOMEM;
        } else {
            stmt->bindings = calloc(stmt->parse_result.parameter_count, sizeof(*stmt->bindings));
            if (stmt->bindings == NULL) {
                set_nomem_error(database);
                rc = MYLITE_NOMEM;
            } else {
                stmt->parameter_count = stmt->parse_result.parameter_count;
            }
        }
    }
    if (rc == MYLITE_OK &&
        (stmt->parameter_count != 0U ||
         stmt->result_capability != MYLITE_STATEMENT_RESULT_STREAMING_QUERY ||
         stmt->buffered_results) &&
        prepared_statement_requires_object_validation(stmt->statement)) {
        rc = validate_prepared_statement_objects(stmt);
    }
    if (rc == MYLITE_OK && (stmt->parameter_count != 0U ||
                            stmt->result_capability != MYLITE_STATEMENT_RESULT_STREAMING_QUERY ||
                            stmt->buffered_results)) {
        (void)mylite_statement_context_end(&stmt->context, MYLITE_OK);
        mylite_statement_context_deinit(&stmt->context);
        stmt->has_context = false;
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        database->session.active_statement_time =
            (int64_t)mylite_statement_context_time(&stmt->context);
        rc = mylite_execution_prepare_statement_transaction_boundary(database, stmt->statement);
    }
    if (rc == MYLITE_OK) {
        rc = begin_prepare_transaction(database, &stmt->read_transaction);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_cursor_select_metadata(stmt);
    }
    if (rc == MYLITE_OK) {
        rc = commit_statement_transaction(database, &stmt->read_transaction);
    }
    if (rc != MYLITE_OK) {
        mylite_result *result = NULL;

        rc = rollback_statement_transaction(database, &stmt->read_transaction, rc);
        rc = finish_failed_statement(database, &stmt->completion, rc, &result);
        (void)mylite_statement_context_end(&stmt->context, rc);
        mylite_statement_context_deinit(&stmt->context);
        stmt->has_context = false;
    } else if (stmt->has_context) {
        (void)mylite_statement_context_end(&stmt->context, MYLITE_OK);
        mylite_statement_context_deinit(&stmt->context);
        stmt->has_context = false;
    }
    return rc;
}

static bool prepared_statement_kind_is_supported(const struct mylite_sql_ast_node *statement) {
    if (statement == NULL) {
        return false;
    }
    if (mylite_execution_prepared_statement_disallows_statement(statement)) {
        return false;
    }
    return statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT ||
           select_statement_into_list(statement) == NULL;
}

static bool prepared_statement_requires_object_validation(
    const struct mylite_sql_ast_node *statement
) {
    if (statement == NULL) {
        return false;
    }
    switch (statement->kind) {
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_INSERT_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SELECT_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_SET_STATEMENT:
    case MYLITE_SQL_AST_DELETE_STATEMENT:
    case MYLITE_SQL_AST_JOINED_DELETE_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_JOINED_UPDATE_STATEMENT:
        return true;
    default:
        return false;
    }
}

static int validate_prepared_statement_objects(mylite_stmt *stmt) {
    struct mylite_db *database = stmt->database;
    mylite_stmt *saved_active_bound_statement = database->active_bound_statement;
    bool unsupported = false;
    int rc = begin_prepare_transaction(database, &stmt->read_transaction);

    if (rc != MYLITE_OK) {
        return rc;
    }

    stmt->analyzing_unbound_parameters = true;
    database->active_bound_statement = stmt;
    database->cursor_plan_attempt_active = true;
    database->cursor_plan_attempt_unsupported = false;
    switch (stmt->statement->kind) {
    case MYLITE_SQL_AST_SELECT_STATEMENT:
        rc = validate_prepared_select_statement_objects(stmt);
        break;
    case MYLITE_SQL_AST_INSERT_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT: {
        struct planned_insert plan = {0};

        rc = plan_insert(database, stmt->statement, &plan);
        planned_insert_deinit(&plan);
        break;
    }
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_SET_STATEMENT: {
        struct planned_insert plan = {0};

        rc = plan_insert_set(database, stmt->statement, &plan);
        planned_insert_deinit(&plan);
        break;
    }
    case MYLITE_SQL_AST_INSERT_SELECT_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT: {
        struct planned_insert_select plan = {0};

        rc = plan_insert_select(database, stmt->statement, &plan);
        planned_insert_select_deinit(&plan);
        break;
    }
    case MYLITE_SQL_AST_DELETE_STATEMENT:
    case MYLITE_SQL_AST_JOINED_DELETE_STATEMENT: {
        struct planned_delete plan = {0};

        rc = plan_delete(database, stmt->statement, &plan);
        planned_delete_deinit(&plan);
        break;
    }
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_JOINED_UPDATE_STATEMENT: {
        struct planned_update plan = {0};

        rc = plan_update(database, stmt->statement, &plan);
        planned_update_deinit(&plan);
        break;
    }
    default:
        rc = MYLITE_ERROR;
        break;
    }
    unsupported = rc != MYLITE_OK && database->cursor_plan_attempt_unsupported;
    database->cursor_plan_attempt_active = false;
    database->cursor_plan_attempt_unsupported = false;
    database->active_bound_statement = saved_active_bound_statement;
    stmt->analyzing_unbound_parameters = false;

    if (rc == MYLITE_OK) {
        rc = commit_statement_transaction(database, &stmt->read_transaction);
    } else {
        rc = rollback_statement_transaction(database, &stmt->read_transaction, rc);
        if (rc != MYLITE_NOMEM &&
            stmt->result_capability == MYLITE_STATEMENT_RESULT_STREAMING_QUERY && unsupported) {
            mylite_diagnostics_clear_condition(mylite_connection_diagnostics(database));
            rc = MYLITE_OK;
        }
    }
    return rc;
}

static int validate_prepared_select_statement_objects(mylite_stmt *stmt) {
    struct mylite_db *database = stmt->database;
    const char *argument_count_error_function =
        select_statement_argument_count_error_function(stmt->statement);
    struct information_schema_join_compat_plan join_plan = {0};
    struct information_schema_query information_schema_query = {0};
    struct planned_select plan = {0};
    bool targets_information_schema = false;
    int rc = MYLITE_OK;

    if (argument_count_error_function != NULL) {
        set_native_function_parameter_count_error(database, argument_count_error_function);
        return MYLITE_ERROR;
    }
    rc = select_statement_targets_information_schema(
        database,
        stmt->statement,
        &targets_information_schema
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!targets_information_schema) {
        rc = plan_select(database, stmt->statement, true, &plan);
        planned_select_deinit(&plan);
        return rc;
    }

    rc = mylite_execution_information_schema_join_plan_analyze(
        database,
        stmt->statement,
        &join_plan
    );
    if (rc == MYLITE_OK && join_plan.kind == INFORMATION_SCHEMA_JOIN_COMPAT_NONE) {
        rc = resolve_information_schema_query(database, stmt->statement, &information_schema_query);
    }
    mylite_execution_information_schema_join_plan_deinit(&join_plan);
    information_schema_query_deinit(&information_schema_query);
    return rc;
}

static int prepare_cursor_select_metadata(mylite_stmt *stmt) {
    enum cursor_plan_attempt_result plan_result = CURSOR_PLAN_ATTEMPT_FAILED;
    bool uses_materialized_dispatch = false;
    int rc = ensure_prepared_statement_parse_tree(stmt);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = select_statement_uses_materialized_cursor_dispatch(
        stmt->database,
        stmt->statement,
        &uses_materialized_dispatch
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (uses_materialized_dispatch) {
        return prepare_cursor_materialized_select_statement(stmt);
    }
    plan_result = prepare_cursor_select_plan(stmt, false, &rc);
    if (plan_result == CURSOR_PLAN_ATTEMPT_READY || plan_result == CURSOR_PLAN_ATTEMPT_FAILED) {
        return rc;
    }

    clear_cursor_select_plan_resources(stmt, rc);
    mylite_diagnostics_clear_condition(mylite_connection_diagnostics(stmt->database));
    return prepare_cursor_materialized_select_statement(stmt);
}

static int prepare_cursor_select_execution(mylite_stmt *stmt) {
    enum cursor_plan_attempt_result plan_result = CURSOR_PLAN_ATTEMPT_FAILED;
    bool uses_materialized_dispatch = false;
    int rc = MYLITE_OK;

    if (stmt->buffered_results) {
        return prepare_cursor_materialized_select_statement(stmt);
    }
    if (stmt->analyzed_select.analysis.valid) {
        plan_result = prepare_cursor_select_plan(stmt, true, &rc);
        if (plan_result == CURSOR_PLAN_ATTEMPT_READY || plan_result == CURSOR_PLAN_ATTEMPT_FAILED) {
            return rc;
        }

        clear_cursor_select_plan_resources(stmt, rc);
        mylite_diagnostics_clear_condition(mylite_connection_diagnostics(stmt->database));
        return prepare_cursor_materialized_select_statement(stmt);
    }
    rc = ensure_prepared_statement_parse_tree(stmt);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = select_statement_uses_materialized_cursor_dispatch(
        stmt->database,
        stmt->statement,
        &uses_materialized_dispatch
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (uses_materialized_dispatch) {
        return prepare_cursor_materialized_select_statement(stmt);
    }
    plan_result = prepare_cursor_select_plan(stmt, true, &rc);
    if (plan_result == CURSOR_PLAN_ATTEMPT_READY) {
        return rc;
    }
    if (plan_result == CURSOR_PLAN_ATTEMPT_FAILED) {
        return rc;
    }

    clear_cursor_select_plan_resources(stmt, rc);
    mylite_diagnostics_clear_condition(mylite_connection_diagnostics(stmt->database));
    return prepare_cursor_materialized_select_statement(stmt);
}

static int select_statement_uses_materialized_cursor_dispatch(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool *out_uses_materialized_dispatch
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = {0};
    char table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = {0};
    bool matches = false;
    int rc = MYLITE_OK;

    if (out_uses_materialized_dispatch == NULL) {
        return MYLITE_MISUSE;
    }
    *out_uses_materialized_dispatch = false;

    rc = select_statement_targets_information_schema(database, statement, &matches);
    if (rc == MYLITE_OK && !matches) {
        rc = select_statement_targets_mysql_data_dictionary_table(
            database,
            statement,
            table_name,
            sizeof(table_name),
            &matches
        );
    }
    if (rc == MYLITE_OK && !matches) {
        rc = select_statement_targets_absent_mysql_enterprise_table(
            database,
            statement,
            table_name,
            sizeof(table_name),
            &matches
        );
    }
    if (rc == MYLITE_OK && !matches) {
        rc = select_statement_targets_mysql_system_table(database, statement, &matches);
    }
    if (rc == MYLITE_OK && !matches) {
        rc = select_statement_targets_absent_builtin_schema_table(
            database,
            statement,
            schema_name,
            sizeof(schema_name),
            table_name,
            sizeof(table_name),
            &matches
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_uses_materialized_dispatch = matches;
    return MYLITE_OK;
}

static int prepare_cursor_materialized_select_statement(mylite_stmt *stmt) {
    mylite_result *result = NULL;
    int rc = ensure_prepared_statement_parse_tree(stmt);

    if (rc == MYLITE_OK) {
        rc = execute_select_statement_in_read_transaction(
            stmt->database,
            &stmt->context,
            stmt->statement,
            true,
            &result
        );
    }

    if (rc == MYLITE_OK) {
        stmt->metadata_result = result;
        stmt->has_materialized_rows = true;
        rc = commit_statement_transaction(stmt->database, &stmt->read_transaction);
    }
    if (rc == MYLITE_OK) {
        rc = set_materialized_cursor_found_row_count(stmt);
    }
    if (rc == MYLITE_OK) {
        mylite_diagnostics_clear_condition(mylite_connection_diagnostics(stmt->database));
        rc = mylite_statement_completion_capture(
            &stmt->completion,
            stmt->database,
            &(const struct mylite_statement_completion_values){
                .status = MYLITE_OK,
                .row_count = -1,
                .affected_rows = mylite_result_affected_rows(stmt->metadata_result),
                .found_rows = stmt->completion.found_rows,
                .insert_id = mylite_result_insert_id(stmt->metadata_result),
                .warning_count = mylite_result_warning_count(stmt->metadata_result),
                .updates_found_rows = true,
            }
        );
    }
    if (rc == MYLITE_OK) {
        mylite_result_set_warning_count(stmt->metadata_result, stmt->completion.warning_count);
    } else if (rc == MYLITE_NOMEM) {
        set_nomem_error(stmt->database);
    }
    if (rc == MYLITE_OK && stmt->buffered_results) {
        rc = publish_buffered_statement_completion(stmt);
    }
    return rc;
}

static int publish_buffered_statement_completion(mylite_stmt *stmt) {
    struct mylite_db *database = stmt->database;
    int rc = MYLITE_OK;

    stmt->completion.row_count = -1;
    rc = mylite_statement_completion_publish(&stmt->completion, database);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    if (stmt->has_context) {
        (void)mylite_statement_context_end(&stmt->context, rc);
        mylite_statement_context_deinit(&stmt->context);
        stmt->has_context = false;
    }
    if (rc == MYLITE_OK) {
        mylite_execution_clear_select_consumed_next_transaction_characteristics(database);
    }
    mylite_connection_publish_processlist_session(database);
    return rc;
}

static enum cursor_plan_attempt_result prepare_cursor_select_plan(
    mylite_stmt *stmt,
    bool prepare_execution,
    int *out_rc
) {
    struct mylite_db *database = stmt->database;
    struct planned_select *plan = &stmt->analyzed_select.plan;
#ifdef MYLITE_ENABLE_PROFILING
    uint64_t profile_lowering_started_ns = 0U;
#endif
    bool unsupported = false;
    bool create_metadata = false;
    int rc = MYLITE_OK;

    if (stmt->analyzed_select.analysis.valid &&
        !prepared_select_analysis_matches_current_state(stmt)) {
        clear_cursor_select_plan_resources(stmt, MYLITE_OK);
    }
    database->cursor_plan_attempt_active = true;
    database->cursor_plan_attempt_unsupported = false;
    rc = analyze_prepared_select(stmt);
    unsupported = rc != MYLITE_OK && database->cursor_plan_attempt_unsupported;
    database->cursor_plan_attempt_active = false;
    database->cursor_plan_attempt_unsupported = false;

    create_metadata = rc == MYLITE_OK && stmt->metadata_result == NULL;
    if (create_metadata) {
        rc = mylite_result_create(&stmt->metadata_result);
    }
    if (rc == MYLITE_OK && create_metadata) {
        rc = load_result_column_metadata_context(database, plan, &stmt->metadata_context);
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && create_metadata && column_index < plan->column_count;
         ++column_index) {
        rc = append_select_result_column(
            database,
            stmt->metadata_result,
            plan,
            &stmt->metadata_context,
            column_index
        );
    }
    if (rc == MYLITE_OK && prepare_execution &&
        stmt->analyzed_select.analysis.lowered_sql == NULL) {
#ifdef MYLITE_ENABLE_PROFILING
        profile_lowering_started_ns = mylite_profile_now_ns();
#endif
        rc = build_select_sql(plan, &stmt->analyzed_select.analysis.lowered_sql);
#ifdef MYLITE_ENABLE_PROFILING
        mylite_profile_record_select_lowering(database, profile_lowering_started_ns, false);
#endif
#ifdef MYLITE_ENABLE_PROFILING
    } else if (rc == MYLITE_OK && prepare_execution) {
        mylite_profile_record_select_lowering(database, 0U, true);
#endif
    }
    if (rc == MYLITE_OK && prepare_execution) {
        rc = prepare_cached_sqlite_statement(
            database,
            stmt->analyzed_select.analysis.lowered_sql,
            &stmt->sqlite_statement
        );
        if (rc == MYLITE_OK) {
            stmt->sqlite_statement_is_cached = true;
        }
    }
    if (rc == MYLITE_OK && prepare_execution) {
        rc = bind_select_parameters(stmt->sqlite_statement, plan);
    }
    if (rc != MYLITE_OK && stmt->sqlite_statement != NULL) {
        rc = finish_cursor_sqlite_statement(stmt, rc);
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (rc != MYLITE_OK &&
               mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
        set_physical_sqlite_row_error(database);
        rc = MYLITE_ERROR;
    }
    *out_rc = rc;
    if (rc == MYLITE_OK) {
        if (stmt->analyzed_select.analysis.parameter_values_are_reusable) {
            release_prepared_statement_parse_tree(stmt);
        }
        return CURSOR_PLAN_ATTEMPT_READY;
    }
    return unsupported ? CURSOR_PLAN_ATTEMPT_UNSUPPORTED : CURSOR_PLAN_ATTEMPT_FAILED;
}

static int analyze_prepared_select(mylite_stmt *stmt) {
    struct mylite_analyzed_select_plan *analyzed = &stmt->analyzed_select;
    struct mylite_select_analysis_state *analysis = &analyzed->analysis;
    struct mylite_db *database = stmt->database;
    struct mylite_select_analysis_session_key session_key = {0};
    struct mylite_select_analysis_generation_key generation_key = {0};
    bool parameter_contexts_are_reusable = false;
#ifdef MYLITE_ENABLE_PROFILING
    uint64_t profile_plan_started_ns = 0U;
#endif
    int rc = MYLITE_OK;

    if (analysis->valid && !prepared_select_analysis_matches_current_state(stmt)) {
        deinit_analyzed_select_plan(analyzed);
    }
    if (analysis->valid) {
#ifdef MYLITE_ENABLE_PROFILING
        mylite_profile_record_select_plan(database, 0U, true);
#endif
        return MYLITE_OK;
    }

    session_key = current_select_analysis_session_key(database);
    generation_key = (struct mylite_select_analysis_generation_key){
        .catalog = database->session.catalog_generation,
        .sqlite_schema = database->session.sqlite_schema_generation,
    };
    rc = ensure_prepared_statement_parse_tree(stmt);
    if (rc != MYLITE_OK) {
        return rc;
    }

#ifdef MYLITE_ENABLE_PROFILING
    profile_plan_started_ns = mylite_profile_now_ns();
#endif
    rc = mylite_execution_select_parameters_are_plan_reusable(
        stmt->statement,
        &parameter_contexts_are_reusable
    );
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    stmt->parameter_plan_reusable = parameter_contexts_are_reusable;
    if (rc == MYLITE_OK) {
        rc = plan_select(database, stmt->statement, true, &analyzed->plan);
    }
#ifdef MYLITE_ENABLE_PROFILING
    mylite_profile_record_select_plan(database, profile_plan_started_ns, false);
#endif
    if (rc != MYLITE_OK) {
        planned_select_deinit(&analyzed->plan);
        return rc;
    }
    rc = mylite_execution_select_analysis_capture(
        analysis,
        stmt->bindings,
        stmt->parameter_count,
        session_key,
        generation_key,
        stmt->parameter_plan_reusable
    );
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
        deinit_analyzed_select_plan(analyzed);
        return rc;
    }
    return MYLITE_OK;
}

static bool prepared_select_analysis_matches_current_state(const mylite_stmt *stmt) {
    const struct mylite_db *database = stmt == NULL ? NULL : stmt->database;

    if (database == NULL) {
        return false;
    }
    return mylite_execution_select_analysis_matches(
        &stmt->analyzed_select.analysis,
        stmt->bindings,
        stmt->parameter_count,
        current_select_analysis_session_key(database),
        (struct mylite_select_analysis_generation_key){
            .catalog = database->session.catalog_generation,
            .sqlite_schema = database->session.sqlite_schema_generation,
        }
    );
}

static int ensure_prepared_statement_parse_tree(mylite_stmt *stmt) {
    size_t statement_count = 0U;
#ifdef MYLITE_ENABLE_PROFILING
    uint64_t profile_parse_started_ns = 0U;
#endif
    int rc = MYLITE_OK;

    if (stmt == NULL || stmt->database == NULL || !stmt->has_normalized_sql) {
        return MYLITE_MISUSE;
    }
    if (stmt->statement != NULL) {
        return MYLITE_OK;
    }

#ifdef MYLITE_ENABLE_PROFILING
    profile_parse_started_ns = mylite_profile_now_ns();
#endif
    rc = status_from_parse_status(mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = stmt->normalized_sql.sql,
            .length = stmt->normalized_sql.sql_size,
            .modes = stmt->parse_modes,
            .allow_parameters = true,
        },
        &stmt->parse_result
    ));
#ifdef MYLITE_ENABLE_PROFILING
    mylite_profile_record_parse(
        stmt->database,
        profile_parse_started_ns,
        stmt->parse_result.retry_callback_count,
        stmt->parse_result.retry_handled_count
    );
#endif
    stmt->has_parse_result = true;
    if (rc != MYLITE_OK) {
        rc = finish_parse_failure(stmt->database, &stmt->parse_result, rc);
        release_prepared_statement_parse_tree(stmt);
        return rc;
    }

    rc = script_statement_count(stmt->parse_result.root, &statement_count);
    if (rc == MYLITE_OK && statement_count == 1U) {
        stmt->statement = child_at(stmt->parse_result.root, 0U);
    }
    if (rc != MYLITE_OK || statement_count != 1U || stmt->statement == NULL ||
        mylite_statement_result_capability(stmt->statement) !=
            MYLITE_STATEMENT_RESULT_STREAMING_QUERY ||
        stmt->parse_result.parameter_count != stmt->parameter_count) {
        set_parse_error(stmt->database, &stmt->parse_result);
        release_prepared_statement_parse_tree(stmt);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static void release_prepared_statement_parse_tree(mylite_stmt *stmt) {
    if (stmt == NULL || !stmt->has_parse_result) {
        return;
    }

    mylite_sql_parse_result_deinit(&stmt->parse_result);
    stmt->parse_result = (struct mylite_sql_parse_result){0};
    stmt->statement = NULL;
    stmt->has_parse_result = false;
}

static struct mylite_select_analysis_session_key current_select_analysis_session_key(
    const struct mylite_db *database
) {
    const char *sql_auto_is_null = NULL;

    if (database == NULL) {
        return (struct mylite_select_analysis_session_key){0};
    }
    sql_auto_is_null = mylite_execution_session_system_variable_override_value(
        database,
        MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL
    );
    return (struct mylite_select_analysis_session_key){
        .sql_mode = database->session.sql_mode,
        .last_insert_id = database->session.last_insert_id,
        .time_zone_offset_minutes = database->session.time_zone_offset_minutes,
        .sql_auto_is_null = sql_auto_is_null != NULL && strcmp(sql_auto_is_null, "1") == 0,
    };
}

static void deinit_analyzed_select_plan(struct mylite_analyzed_select_plan *analyzed) {
    if (analyzed == NULL) {
        return;
    }

    planned_select_deinit(&analyzed->plan);
    mylite_execution_select_analysis_deinit(&analyzed->analysis);
    *analyzed = (struct mylite_analyzed_select_plan){0};
}

static int finish_cursor_sqlite_statement(mylite_stmt *stmt, int rc) {
    if (stmt->sqlite_statement != NULL) {
        if (stmt->sqlite_statement_is_cached) {
            rc = finish_cached_sqlite_statement(stmt->database, stmt->sqlite_statement, rc);
        } else {
            rc = finalize_sqlite_statement(stmt->sqlite_statement, rc);
        }
        stmt->sqlite_statement = NULL;
        stmt->sqlite_statement_is_cached = false;
    }
    return rc;
}

static void clear_cursor_select_plan_resources(mylite_stmt *stmt, int rc) {
    (void)finish_cursor_sqlite_statement(stmt, rc);
    mylite_execution_result_row_storage_deinit(&stmt->row_storage);
    mylite_result_free(stmt->metadata_result);
    stmt->metadata_result = NULL;
    mylite_result_free(stmt->completed_result);
    stmt->completed_result = NULL;
    mylite_result_free(stmt->reusable_command_result);
    stmt->reusable_command_result = NULL;
    stmt->has_materialized_rows = false;
    result_column_metadata_context_deinit(&stmt->metadata_context);
    stmt->metadata_context = result_column_metadata_context_init();
}

static int finish_cursor_statement(mylite_stmt *stmt, bool exhausted) {
    struct mylite_db *database = stmt->database;
    bool is_current_statement = stmt->statement_id == database->session.statement_id;
    int rc = commit_statement_transaction(database, &stmt->read_transaction);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!exhausted) {
        stmt->completion.found_rows = (uint64_t)stmt->row_count;
    }
    if (!stmt->completion.captured) {
        mylite_diagnostics_clear_condition(mylite_connection_diagnostics(database));
        rc = mylite_statement_completion_capture(
            &stmt->completion,
            database,
            &(const struct mylite_statement_completion_values){
                .status = MYLITE_OK,
                .row_count = -1,
                .affected_rows = mylite_result_affected_rows(stmt->metadata_result),
                .found_rows = stmt->completion.found_rows,
                .insert_id = mylite_result_insert_id(stmt->metadata_result),
                .warning_count = mylite_result_warning_count(stmt->metadata_result),
                .updates_found_rows = true,
            }
        );
    } else {
        stmt->completion.row_count = -1;
        stmt->completion.found_rows =
            exhausted ? stmt->completion.found_rows : (uint64_t)stmt->row_count;
    }
    if (rc == MYLITE_OK && is_current_statement) {
        rc = mylite_statement_completion_publish(&stmt->completion, database);
    }
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        mylite_statement_completion_publish_failure_fallback(database);
        return rc;
    }
    if (stmt->has_context) {
        (void)mylite_statement_context_end(&stmt->context, MYLITE_OK);
        mylite_statement_context_deinit(&stmt->context);
        stmt->has_context = false;
    }
    if (is_current_statement) {
        mylite_execution_clear_select_consumed_next_transaction_characteristics(database);
    }
    if (database->active_cursor == stmt) {
        database->active_cursor = NULL;
    }
    return MYLITE_OK;
}

static int finish_failed_cursor_statement(mylite_stmt *stmt, int rc) {
    struct mylite_db *database = stmt->database;
    mylite_result *result = NULL;

    rc = finish_cursor_sqlite_statement(stmt, rc);
    rc = rollback_statement_transaction(database, &stmt->read_transaction, rc);
    if (database->active_cursor == stmt) {
        database->active_cursor = NULL;
    }
    stmt->done = true;
    rc = finish_failed_statement(database, &stmt->completion, rc, &result);
    if (stmt->has_context) {
        (void)mylite_statement_context_end(&stmt->context, rc);
        mylite_statement_context_deinit(&stmt->context);
        stmt->has_context = false;
    }
    mylite_connection_publish_processlist_session(database);
    return rc;
}

int mylite_execution_prepare_connection_close(struct mylite_db *database) {
    int rc = MYLITE_OK;

    if (database == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_execution_detach_connection_statements(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (database->session.user_transaction_active) {
        rc =
            normalize_sqlite_control_rc(database, execute_sqlite_control_sql(database, "ROLLBACK"));
        if (rc == MYLITE_OK) {
            mylite_catalog_invalidate_descriptor_cache(database);
            database->session.user_transaction_active = false;
            mylite_execution_clear_active_transaction_characteristics(database);
            mylite_execution_clear_user_savepoints(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_reconcile_persistent_auto_increment_high_waters(database);
    }
    if (rc == MYLITE_OK) {
        mylite_execution_clear_persistent_auto_increment_high_waters(database);
    }
    return rc;
}

int mylite_execution_detach_connection_statements(struct mylite_db *database) {
    int rc = MYLITE_OK;

    if (database == NULL) {
        return MYLITE_MISUSE;
    }

    while (database->live_statements != NULL) {
        mylite_stmt *stmt = database->live_statements;

        database->live_statements = stmt->next_live_statement;
        stmt->next_live_statement = NULL;
        rc = detach_cursor_statement(stmt, rc);
    }
    database->active_cursor = NULL;
    return rc;
}

static void register_live_statement(mylite_stmt *stmt) {
    if (stmt == NULL || stmt->database == NULL) {
        return;
    }

    stmt->next_live_statement = stmt->database->live_statements;
    stmt->database->live_statements = stmt;
}

static void unregister_live_statement(mylite_stmt *stmt) {
    mylite_stmt **current = NULL;

    if (stmt == NULL || stmt->database == NULL) {
        return;
    }

    current = &stmt->database->live_statements;
    while (*current != NULL) {
        if (*current == stmt) {
            *current = stmt->next_live_statement;
            stmt->next_live_statement = NULL;
            return;
        }
        current = &(*current)->next_live_statement;
    }
}

static int detach_cursor_statement(mylite_stmt *stmt, int rc) {
    if (stmt == NULL || stmt->database == NULL) {
        return rc;
    }

    if (stmt->database->active_cursor == stmt) {
        stmt->database->active_cursor = NULL;
    }
    rc = release_cursor_statement_resources(stmt, rc);
    stmt->database = NULL;
    stmt->current_row_available = false;
    stmt->done = true;
    return rc;
}

static int release_cursor_statement_resources(mylite_stmt *stmt, int rc) {
    if (stmt->sqlite_statement != NULL) {
        rc = finish_cursor_sqlite_statement(stmt, rc);
    }
    rc = rollback_statement_transaction(stmt->database, &stmt->read_transaction, rc);
    deinit_analyzed_select_plan(&stmt->analyzed_select);
    deinit_analyzed_dml_plan(&stmt->analyzed_dml);
    mylite_execution_result_row_storage_deinit(&stmt->row_storage);
    result_column_metadata_context_deinit(&stmt->metadata_context);
    mylite_result_free(stmt->metadata_result);
    stmt->metadata_result = NULL;
    mylite_result_free(stmt->completed_result);
    stmt->completed_result = NULL;
    mylite_result_free(stmt->reusable_command_result);
    stmt->reusable_command_result = NULL;
    if (stmt->has_parse_result) {
        mylite_sql_parse_result_deinit(&stmt->parse_result);
        stmt->has_parse_result = false;
    }
    if (stmt->has_context) {
        if (rc == MYLITE_OK) {
            rc = mylite_statement_context_end(&stmt->context, MYLITE_OK);
        }
        mylite_statement_context_deinit(&stmt->context);
        stmt->has_context = false;
    }
    if (stmt->has_normalized_sql) {
        mylite_execution_normalized_sql_deinit(&stmt->normalized_sql);
        stmt->has_normalized_sql = false;
    }
    release_stmt_bindings(stmt);
    return rc;
}

static void release_stmt_bindings(mylite_stmt *stmt) {
    if (stmt == NULL) {
        return;
    }
    for (size_t index = 0U; index < stmt->parameter_count; ++index) {
        free(stmt->bindings[index].bytes);
    }
    free(stmt->bindings);
    stmt->bindings = NULL;
    stmt->parameter_count = 0U;
}

static void destroy_cursor_statement(mylite_stmt *stmt) {
    if (stmt == NULL) {
        return;
    }
    unregister_live_statement(stmt);
    if (stmt->database != NULL) {
        if (stmt->database->active_cursor == stmt) {
            stmt->database->active_cursor = NULL;
        }
        (void)release_cursor_statement_resources(stmt, MYLITE_OK);
    }
    mylite_statement_completion_deinit(&stmt->completion);
    free(stmt);
}

static int set_materialized_cursor_found_row_count(mylite_stmt *stmt) {
    if (mylite_result_has_found_row_count(stmt->metadata_result)) {
        stmt->completion.found_rows = mylite_result_found_row_count(stmt->metadata_result);
        return MYLITE_OK;
    }
    if (mylite_result_row_count(stmt->metadata_result) > UINT64_MAX) {
        set_runtime_error(stmt->database, "SELECT found-row count is out of range");
        return MYLITE_ERROR;
    }
    stmt->completion.found_rows = (uint64_t)mylite_result_row_count(stmt->metadata_result);
    return MYLITE_OK;
}

static int set_cursor_found_row_count(mylite_stmt *stmt) {
    int64_t count = 0;
    uint64_t found_row_count = 0U;
    int rc = MYLITE_OK;

    if (stmt->analyzed_select.plan.calc_found_rows) {
        rc = read_select_found_row_count(stmt->database, &stmt->analyzed_select.plan, &count);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (count < 0) {
            set_runtime_error(stmt->database, "invalid SQL_CALC_FOUND_ROWS count");
            return MYLITE_ERROR;
        }
        stmt->completion.found_rows = (uint64_t)count;
        return append_sql_calc_found_rows_deprecation_warning(stmt->database);
    }

    rc = found_row_count_for_select_limit_envelope(
        stmt->database,
        &stmt->analyzed_select.plan,
        stmt->row_count,
        &found_row_count
    );
    if (rc == MYLITE_OK) {
        stmt->completion.found_rows = found_row_count;
    }
    return rc;
}

static int finish_parse_failure(
    struct mylite_db *database,
    const struct mylite_sql_parse_result *parse_result,
    int parse_rc
) {
    struct mylite_statement_completion completion;
    int rc = parse_rc;
    int completion_rc = MYLITE_OK;

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else {
        set_parse_error(database, parse_result);
    }
    mylite_statement_completion_init(&completion);
    completion_rc = mylite_statement_completion_capture(
        &completion,
        database,
        &(const struct mylite_statement_completion_values){
            .status = rc,
            .row_count = -1,
            .affected_rows = -1,
            .found_rows = database->session.found_rows,
            .warning_count =
                mylite_diagnostics_warning_total_count(mylite_connection_diagnostics(database)),
        }
    );
    if (completion_rc == MYLITE_OK) {
        completion_rc = mylite_statement_completion_publish(&completion, database);
    }
    mylite_statement_completion_deinit(&completion);
    if (completion_rc != MYLITE_OK) {
        set_nomem_error(database);
        mylite_statement_completion_publish_failure_fallback(database);
    }
    return completion_rc == MYLITE_OK ? rc : completion_rc;
}

const struct mylite_sql_ast_node *mylite_execution_unwrap_parenthesized_expression(
    const struct mylite_sql_ast_node *expression
) {
    return unwrap_parenthesized_expression(expression);
}

int mylite_execution_parse_unsigned_integer_literal(
    const struct mylite_sql_source_span *span,
    uint64_t *out_value
) {
    return parse_unsigned_integer_literal(span, out_value);
}

bool mylite_execution_is_scalar_arithmetic_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    return is_scalar_arithmetic_projection_expression(expression);
}

bool mylite_execution_is_scalar_bitwise_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    return is_scalar_bitwise_projection_expression(expression);
}

int mylite_execution_evaluate_scalar_arithmetic_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
) {
    return evaluate_scalar_arithmetic_expression(database, expression, out_value);
}

int mylite_execution_evaluate_scalar_bitwise_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
) {
    return evaluate_scalar_bitwise_expression(database, expression, out_value);
}

int mylite_execution_evaluate_bit_count_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
) {
    return evaluate_bit_count_operand(database, expression, out_value);
}

int mylite_execution_accumulate_staged_division_by_zero_warnings(
    struct mylite_db *database,
    size_t staged_count,
    size_t *inout_warning_count
) {
    return accumulate_staged_division_by_zero_warnings(database, staged_count, inout_warning_count);
}

int mylite_execution_accumulate_staged_warning_count(
    struct mylite_db *database,
    size_t staged_count,
    size_t *inout_warning_count
) {
    return accumulate_staged_warning_count(database, staged_count, inout_warning_count);
}

int mylite_execution_append_division_by_zero_warnings(
    struct mylite_db *database,
    size_t warning_count
) {
    return append_division_by_zero_warnings(database, warning_count);
}

int mylite_execution_decode_sql_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    const char *unsupported_message,
    const char *nul_message,
    char **out_text,
    size_t *out_text_length
) {
    return decode_sql_string_literal(
        database,
        literal_node,
        unsupported_message,
        nul_message,
        out_text,
        out_text_length
    );
}

int mylite_execution_current_timestamp_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    return current_timestamp_scalar_value(database, out_cell);
}

int mylite_execution_validate_temporal_fractional_precision(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_execution_temporal_fractional_precision_context context
) {
    enum { temporal_fractional_precision_max = 6 };
    struct mylite_sql_source_span precision_span = {0};
    uint64_t precision = 0U;

    if (expression == NULL || !mylite_sql_ast_node_has_temporal_fractional_precision(expression)) {
        return MYLITE_OK;
    }

    precision_span = mylite_sql_ast_node_temporal_fractional_precision_span(expression);
    if (parse_unsigned_integer_literal(&precision_span, &precision) != MYLITE_OK) {
        set_unsupported_error(
            database,
            "temporal fractional precision supports only integer values"
        );
        return MYLITE_ERROR;
    }
    if (precision > temporal_fractional_precision_max) {
        set_temporal_precision_too_big_error(
            database,
            context.subject_name == NULL ? "temporal" : context.subject_name,
            precision
        );
        return MYLITE_ERROR;
    }
    if (precision != 0U) {
        set_unsupported_error(
            database,
            context.unsupported_message == NULL ? "fractional temporal precision is not supported"
                                                : context.unsupported_message
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_supported_temporal_fractional_precision(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *subject_name,
    uint64_t *out_precision
) {
    enum { temporal_fractional_precision_max = 6 };
    struct mylite_sql_source_span precision_span = {0};
    uint64_t precision = 0U;

    if (out_precision == NULL) {
        return MYLITE_MISUSE;
    }
    *out_precision = 0U;
    if (!mylite_sql_ast_node_has_temporal_fractional_precision(expression)) {
        return MYLITE_OK;
    }

    precision_span = mylite_sql_ast_node_temporal_fractional_precision_span(expression);
    if (parse_unsigned_integer_literal(&precision_span, &precision) != MYLITE_OK) {
        set_unsupported_error(
            database,
            "temporal fractional precision supports only integer values"
        );
        return MYLITE_ERROR;
    }
    if (precision > temporal_fractional_precision_max) {
        set_temporal_precision_too_big_error(
            database,
            subject_name == NULL ? "temporal" : subject_name,
            precision
        );
        return MYLITE_ERROR;
    }
    *out_precision = precision;

    return MYLITE_OK;
}

int mylite_execution_sysdate_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return sysdate_scalar_value(database, expression, out_cell);
}

int mylite_execution_current_date_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    return current_date_scalar_value(database, out_cell);
}

int mylite_execution_current_time_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    return current_time_scalar_value(database, out_cell);
}

int mylite_execution_utc_date_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    return utc_date_scalar_value(database, out_cell);
}

int mylite_execution_utc_time_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    return utc_time_scalar_value(database, out_cell);
}

int mylite_execution_utc_timestamp_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    return utc_timestamp_scalar_value(database, out_cell);
}

int mylite_execution_system_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return system_variable_value(database, expression, out_cell);
}

int mylite_execution_decode_sql_string_literal_with_policy(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    const char *unsupported_message,
    const char *nul_message,
    bool allow_nul,
    char **out_text,
    size_t *out_text_length
) {
    return decode_sql_string_literal_with_policy(
        database,
        literal_node,
        unsupported_message,
        nul_message,
        allow_nul,
        out_text,
        out_text_length
    );
}

int mylite_execution_decode_binary_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_bytes,
    size_t *out_byte_count
) {
    return decode_binary_hex_literal(database, literal_node, out_bytes, out_byte_count);
}

int mylite_execution_normalize_decimal_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    bool is_negative,
    char *buffer,
    size_t buffer_size
) {
    return normalize_decimal_integer_literal(database, span, is_negative, buffer, buffer_size);
}

int mylite_execution_format_uint64(
    struct mylite_db *database,
    uint64_t value,
    char *buffer,
    size_t buffer_size
) {
    return format_uint64(database, value, buffer, buffer_size);
}

int mylite_execution_cast_binary_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return cast_binary_value(database, expression, out_cell);
}

int mylite_execution_convert_binary_type_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return convert_binary_type_value(database, expression, out_cell);
}

int mylite_execution_convert_using_binary_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return convert_using_binary_value(database, expression, out_cell);
}

int mylite_execution_convert_using_charset_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return convert_using_charset_value(database, expression, out_cell);
}

int mylite_execution_collate_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return collate_expression_value(database, expression, out_cell);
}

int mylite_execution_scalar_convert_charset_info_for_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_convert_charset_info *out_info
) {
    return scalar_convert_charset_info_for_expression(database, expression, out_info);
}

int mylite_execution_scalar_convert_charset_info_by_name(
    struct mylite_db *database,
    const char *charset_name,
    struct scalar_convert_charset_info *out_info
) {
    return scalar_convert_charset_info_by_name(database, charset_name, out_info);
}

int mylite_execution_rand_seed_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    uint32_t *out_seed
) {
    return rand_seed_value(database, expression, out_seed);
}

int64_t mylite_execution_current_timestamp_epoch(const struct mylite_db *database) {
    return current_timestamp_epoch(database);
}

int mylite_execution_date_add_set_unknown_identifier_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_source_span *span = expression == NULL ? NULL : &expression->span;
    const char *source = NULL;
    size_t source_size = 0U;
    size_t destination_index = 0U;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

    if (span == NULL || span->text == NULL || span->length == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    source = span->text;
    source_size = span->length;
    if (source[0] != '`' && source[0] != '"') {
        if (source_size >= sizeof(column_name)) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        memcpy(column_name, source, source_size);
        column_name[source_size] = '\0';
        set_unknown_column_error(database, column_name);
        return MYLITE_ERROR;
    }
    if (source_size < 2U || source[source_size - 1U] != source[0]) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    for (size_t source_index = 1U; source_index + 1U < source_size; ++source_index) {
        if (destination_index + 1U >= sizeof(column_name)) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        if (source[source_index] == source[0] && source[source_index + 1U] == source[0]) {
            column_name[destination_index] = source[0];
            ++source_index;
        } else {
            column_name[destination_index] = source[source_index];
        }
        ++destination_index;
    }
    if (destination_index == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    column_name[destination_index] = '\0';
    set_unknown_column_error(database, column_name);
    return MYLITE_ERROR;
}

size_t mylite_execution_temporal_constructor_function_argument_count(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return temporal_constructor_function_argument_count(ast_kind);
}

const char *mylite_execution_temporal_constructor_function_name(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return temporal_constructor_function_name(ast_kind);
}

bool mylite_execution_is_temporal_constructor_function_kind(enum mylite_sql_ast_node_kind ast_kind
) {
    return is_temporal_constructor_function_kind(ast_kind);
}

int mylite_execution_copy_identifier_name_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_node,
    char *destination,
    size_t destination_size,
    const char *identifier_kind,
    const char *nul_message
) {
    return copy_table_option_name_text(
        database,
        name_node,
        destination,
        destination_size,
        (struct table_option_name_policy){
            .identifier_kind = identifier_kind,
            .nul_message = nul_message,
        }
    );
}

const char *mylite_execution_national_character_set_name(void) {
    return national_character_set_name;
}

const char *mylite_execution_national_collation_name(void) {
    return national_collation_name;
}

void mylite_execution_set_parse_error(struct mylite_db *database) {
    set_parse_error(database, NULL);
}

void mylite_execution_set_unsupported_error(struct mylite_db *database, const char *message) {
    set_unsupported_error(database, message);
}

int mylite_execution_format_approximate_result_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    double value,
    char *buffer,
    size_t buffer_size
) {
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    int rc = MYLITE_OK;

    if (column != NULL && column_descriptor_is_approximate(column)) {
        rc = approximate_type_info_for_logical_type(column->logical_type, &info);
    }
    if (rc == MYLITE_OK) {
        rc = format_approximate_value_text(
            database,
            value,
            &info,
            "approximate result",
            buffer,
            buffer_size
        );
    }
    return rc;
}

void mylite_execution_set_native_function_parameter_count_error(
    struct mylite_db *database,
    const char *function_name
) {
    set_native_function_parameter_count_error(database, function_name);
}

void mylite_execution_set_scalar_division_unsupported_error(struct mylite_db *database) {
    set_scalar_division_unsupported_error(database);
}

void mylite_execution_set_abs_signed_minimum_overflow_error(struct mylite_db *database) {
    set_abs_signed_minimum_overflow_error(database);
}

void mylite_execution_set_abs_unsupported_error(struct mylite_db *database) {
    set_abs_unsupported_error(database);
}

void mylite_execution_set_sign_unsupported_error(struct mylite_db *database) {
    set_sign_unsupported_error(database);
}

void mylite_execution_set_rounding_unsupported_error(struct mylite_db *database) {
    set_rounding_unsupported_error(database);
}

int mylite_execution_set_rounding_signed_overflow_error(struct mylite_db *database) {
    return set_rounding_signed_overflow_error(database);
}

void mylite_execution_set_sqrt_unsupported_error(struct mylite_db *database) {
    set_sqrt_unsupported_error(database);
}

void mylite_execution_set_angle_conversion_unsupported_error(struct mylite_db *database) {
    set_angle_conversion_unsupported_error(database);
}

void mylite_execution_set_inverse_trig_unsupported_error(struct mylite_db *database) {
    set_inverse_trig_unsupported_error(database);
}

void mylite_execution_set_direct_trig_unsupported_error(struct mylite_db *database) {
    set_direct_trig_unsupported_error(database);
}

void mylite_execution_set_atan_unsupported_error(struct mylite_db *database) {
    set_atan_unsupported_error(database);
}

void mylite_execution_set_exp_log_power_unsupported_error(struct mylite_db *database) {
    set_exp_log_power_unsupported_error(database);
}

void mylite_execution_set_format_unsupported_error(struct mylite_db *database) {
    set_format_unsupported_error(database);
}

void mylite_execution_set_truncate_unsupported_error(struct mylite_db *database) {
    set_truncate_unsupported_error(database);
}

void mylite_execution_set_base_conversion_unsupported_error(struct mylite_db *database) {
    set_base_conversion_unsupported_error(database);
}

void mylite_execution_set_bit_count_unsupported_error(struct mylite_db *database) {
    set_bit_count_unsupported_error(database);
}

void mylite_execution_set_crc32_unsupported_error(struct mylite_db *database) {
    set_crc32_unsupported_error(database);
}

void mylite_execution_set_hex_unsupported_error(struct mylite_db *database) {
    set_hex_unsupported_error(database);
}

void mylite_execution_set_invalid_json_function_text_error(
    struct mylite_db *database,
    size_t position
) {
    set_invalid_json_function_text_error(database, position);
}

int mylite_execution_append_invalid_json_value_warning(
    struct mylite_db *database,
    const struct mylite_json_normalize_result *result
) {
    return append_invalid_json_value_warning(database, result);
}

void mylite_execution_set_invalid_json_path_error(struct mylite_db *database, size_t position) {
    set_invalid_json_path_error(database, position);
}

void mylite_execution_set_json_path_not_allowed_error(struct mylite_db *database) {
    set_json_path_not_allowed_error(database);
}

void mylite_execution_set_json_path_not_array_cell_error(struct mylite_db *database) {
    set_json_path_not_array_cell_error(database);
}

void mylite_execution_set_invalid_json_data_type_error(
    struct mylite_db *database,
    const char *function_name
) {
    set_invalid_json_data_type_error(database, function_name);
}

void mylite_execution_set_invalid_json_one_or_all_error(struct mylite_db *database) {
    set_invalid_json_one_or_all_error(database);
}

void mylite_execution_set_invalid_json_one_or_all_function_error(
    struct mylite_db *database,
    const char *function_name
) {
    set_invalid_json_one_or_all_function_error(database, function_name);
}

void mylite_execution_set_incorrect_arguments_to_escape_error(struct mylite_db *database) {
    set_incorrect_arguments_to_escape_error(database);
}

void mylite_execution_set_json_unquote_incorrect_type_error(struct mylite_db *database) {
    set_json_unquote_incorrect_type_error(database);
}

void mylite_execution_set_json_quote_incorrect_type_error(struct mylite_db *database) {
    set_json_quote_incorrect_type_error(database);
}

void mylite_execution_set_json_binary_charset_error(struct mylite_db *database) {
    set_json_binary_charset_error(database);
}

void mylite_execution_set_json_null_member_name_error(struct mylite_db *database) {
    set_json_null_member_name_error(database);
}

bool mylite_execution_text_equals_ascii_case_insensitive(const char *left, const char *right) {
    return text_equals_ascii_case_insensitive(left, right);
}

bool mylite_execution_column_descriptor_is_time(
    const struct mylite_catalog_column_descriptor *column
) {
    return column_descriptor_is_time(column);
}

int mylite_execution_active_stmt_parameter_cell(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *parameter,
    struct session_scalar_cell *out_cell
) {
    return copy_active_stmt_parameter_cell(database, parameter, out_cell);
}

bool mylite_execution_active_stmt_is_analyzing_unbound_parameters(const struct mylite_db *database
) {
    return database != NULL && database->active_bound_statement != NULL &&
           database->active_bound_statement->analyzing_unbound_parameters;
}

int mylite_execution_information_schema_copy_select_item_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *alias,
    char **out_text
) {
    return copy_select_item_alias_text(database, alias, out_text);
}

int mylite_execution_information_schema_copy_aggregate_label(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_text
) {
    return copy_aggregate_result_column_name(database, span, out_text);
}

int mylite_execution_information_schema_make_scalar_result_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
) {
    return make_scalar_result_column_descriptor(database, expression, label, out_descriptor);
}

int mylite_execution_information_schema_copy_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value,
    enum mylite_execution_information_schema_predicate_value_kind kind,
    struct information_schema_predicate_value *out_value
) {
    if (value != NULL && value->kind == MYLITE_SQL_AST_PARAMETER) {
        struct session_scalar_cell cell = {0};
        size_t value_size = 0U;
        int rc = copy_active_stmt_parameter_cell(database, value, &cell);

        *out_value = (struct information_schema_predicate_value){0};
        if (rc != MYLITE_OK || cell.value == NULL) {
            out_value->is_null = rc == MYLITE_OK;
            return rc;
        }
        value_size = cell.has_value_size ? cell.value_size : strlen(cell.value);
        if (memchr(cell.value, '\0', value_size) != NULL) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA predicate parameters do not support NUL bytes"
            );
            return MYLITE_ERROR;
        }
        out_value->text = malloc(value_size + 1U);
        if (out_value->text == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        memcpy(out_value->text, cell.value, value_size);
        out_value->text[value_size] = '\0';
        out_value->is_numeric = !cell.has_value_size;
        if (kind == MYLITE_EXECUTION_INFORMATION_SCHEMA_PREDICATE_LIKE_PATTERN &&
            !text_value_is_supported_string_key(out_value->text, value_size)) {
            free(out_value->text);
            *out_value = (struct information_schema_predicate_value){0};
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA WHERE LIKE pattern parameters support only ASCII text"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }
    if (kind == MYLITE_EXECUTION_INFORMATION_SCHEMA_PREDICATE_LITERAL) {
        return information_schema_predicate_literal_value_text(database, value, out_value);
    }
    if (kind == MYLITE_EXECUTION_INFORMATION_SCHEMA_PREDICATE_LIKE_PATTERN) {
        return information_schema_predicate_like_pattern_text(database, value, out_value);
    }
    return information_schema_predicate_value_text(database, value, out_value);
}

int mylite_execution_information_schema_predicate_escape_character(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    char *out_escape_character
) {
    return like_escape_character_from_predicate(database, predicate, out_escape_character);
}

int mylite_execution_information_schema_auto_increment_predicate_value(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    struct table_status_values status = {.auto_increment = NULL};

    return table_status_auto_increment_predicate_value(
        database,
        table,
        &status,
        buffer,
        buffer_size,
        out_value
    );
}

struct mylite_result_column_descriptor mylite_execution_information_schema_unknown_result_column_descriptor(
    const char *label
) {
    return unknown_result_column_descriptor(label);
}

int mylite_execution_information_schema_populate_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor
) {
    struct result_column_metadata_context metadata_context = result_column_metadata_context_init();

    return populate_select_result_column_descriptor(
        database,
        NULL,
        column,
        &metadata_context,
        descriptor
    );
}

uint64_t mylite_execution_information_schema_character_set_max_bytes_per_character(
    const char *character_set_name
) {
    return character_set_max_bytes_per_character(character_set_name);
}

uint64_t mylite_execution_information_schema_result_collation_max_bytes_per_character(
    const char *collation_name
) {
    return result_metadata_collation_max_bytes_per_character(collation_name);
}

uint64_t mylite_execution_information_schema_result_display_length_cap(uint64_t display_length) {
    return result_metadata_display_length_cap(display_length);
}

uint32_t mylite_execution_information_schema_result_collation_id(const char *collation_name) {
    return result_metadata_collation_id(collation_name);
}

bool mylite_execution_select_order_source_context_is_joined(
    const struct select_source_context *source_context
) {
    return select_source_context_is_joined(source_context);
}

enum mylite_execution_select_order_expression_support mylite_execution_select_order_expression_support(
    const struct mylite_sql_ast_node *expression
) {
    enum planned_row_scalar_conversion_kind conversion_kind = PLANNED_ROW_SCALAR_CONVERSION_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL ||
        (!row_scalar_expression_is_context_expression_attempt(expression, false) &&
         !row_scalar_expression_is_predicate_value_context_attempt(expression) &&
         expression->kind != MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION &&
         expression->kind != MYLITE_SQL_AST_COLLATE_EXPRESSION &&
         (expression->kind != MYLITE_SQL_AST_COMPARISON_PREDICATE ||
          mylite_sql_ast_node_operator(expression) != MYLITE_SQL_AST_OPERATOR_LIKE))) {
        return MYLITE_EXECUTION_SELECT_ORDER_EXPRESSION_UNSUPPORTED;
    }

    conversion_kind = row_scalar_conversion_kind_from_expression(expression);
    if (conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_CHAR ||
        conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_SIGNED ||
        conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_UNSIGNED ||
        row_scalar_expression_contains_integer_arithmetic_attempt(expression)) {
        return MYLITE_EXECUTION_SELECT_ORDER_EXPRESSION_JOINED;
    }
    return MYLITE_EXECUTION_SELECT_ORDER_EXPRESSION_SINGLE_SOURCE;
}

int mylite_execution_select_order_plan_field_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    return plan_row_scalar_field_expression_with_context(
        database,
        order_key,
        true,
        source_context,
        table_columns,
        table_column_count,
        COLUMN_REFERENCE_ORDER,
        "SELECT ORDER BY FIELD() supports only descriptor search columns",
        out_expression
    );
}

int mylite_execution_select_order_plan_rand_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    struct planned_row_scalar_expression *out_expression
) {
    return plan_row_scalar_rand_expression(
        database,
        order_key,
        false,
        NULL,
        NULL,
        0U,
        out_expression
    );
}

int mylite_execution_select_order_plan_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    return plan_row_scalar_expression(
        database,
        order_key,
        true,
        source_context,
        NULL,
        table_columns,
        table_column_count,
        out_expression
    );
}

void mylite_execution_select_order_row_scalar_expression_deinit(
    struct planned_row_scalar_expression *expression
) {
    planned_row_scalar_expression_deinit(expression);
}

int mylite_execution_select_order_copy_identifier_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char **out_alias
) {
    return copy_select_item_identifier_alias_text(database, column_node, out_alias);
}

int mylite_execution_select_order_resolve_column_pointer(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor **out_column,
    size_t *out_source_index
) {
    return resolve_descriptor_column_reference_pointer_with_source_index(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_ORDER,
        "ORDER BY supports only unqualified descriptor columns",
        table_columns,
        table_column_count,
        out_column,
        out_source_index
    );
}

int mylite_execution_select_order_parse_ordinal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    size_t column_count,
    size_t *out_ordinal
) {
    return parse_select_order_ordinal(database, order_key, column_count, out_ordinal);
}

int mylite_execution_select_order_validate_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    struct integer_column_range range = {0};

    if (column_descriptor_is_set(column)) {
        set_unsupported_error(database, "ORDER BY does not yet support SET columns");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_string_family(column) || column_descriptor_is_date(column) ||
        column_descriptor_is_time(column) || column_descriptor_is_datetime(column) ||
        column_descriptor_is_timestamp(column) || column_descriptor_is_year(column) ||
        column_descriptor_is_bit(column)) {
        return MYLITE_OK;
    }
    return integer_range_for_column(
        database,
        column,
        "ORDER BY supports only integer, BIT, YEAR, DATE, TIME, DATETIME, TIMESTAMP, or "
        "nonbinary string descriptor columns",
        &range
    );
}

int mylite_execution_select_order_convert_limit_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    int64_t *out_value
) {
    return convert_limit_integer_literal(database, literal, out_value);
}

int mylite_execution_ddl_integer_range_for_logical_type(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    struct integer_column_range *out_range
) {
    return integer_range_for_logical_type(
        database,
        (struct integer_logical_type_range_request){
            .logical_type = logical_type,
            .unsupported_message = unsupported_message,
        },
        out_range
    );
}

int mylite_execution_ddl_append_numbered_parameter(
    struct mylite_dynamic_string *string,
    size_t parameter_index
) {
    return append_numbered_parameter(string, parameter_index);
}

int mylite_execution_ddl_append_size_literal(struct mylite_dynamic_string *string, size_t value) {
    return append_size_literal(string, value);
}

int mylite_execution_ddl_append_uint64_literal(
    struct mylite_dynamic_string *string,
    uint64_t value
) {
    return append_uint64_literal(string, value);
}

bool mylite_execution_ddl_planned_secondary_index_is_fulltext(
    const struct planned_secondary_index *index
) {
    return planned_secondary_index_is_fulltext(index);
}

bool mylite_execution_ddl_planned_secondary_index_is_spatial(
    const struct planned_secondary_index *index
) {
    return planned_secondary_index_is_spatial(index);
}

bool mylite_execution_ddl_planned_column_is_char_or_varchar(const struct planned_column *column) {
    return planned_column_is_char_or_varchar(column);
}

bool mylite_execution_ddl_planned_column_is_string_family(const struct planned_column *column) {
    return planned_column_is_string_family(column);
}

bool mylite_execution_ddl_column_descriptor_is_string_family(
    const struct mylite_catalog_column_descriptor *column
) {
    return column_descriptor_is_string_family(column);
}

bool mylite_execution_ddl_column_descriptor_is_char_or_varchar(
    const struct mylite_catalog_column_descriptor *column
) {
    return column_descriptor_is_char_or_varchar(column);
}

bool mylite_execution_ddl_text_equals_ascii_case_insensitive(const char *left, const char *right) {
    return text_equals_ascii_case_insensitive(left, right);
}

bool mylite_execution_ddl_loaded_index_part_requires_string_key_validation(
    const struct loaded_index_part *part
) {
    return loaded_index_part_requires_string_key_validation(part);
}

int mylite_execution_ddl_append_alter_table_add_column_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    return append_alter_table_add_column_default(database, string, plan);
}

int mylite_execution_append_json_table_source_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_source *source,
    size_t source_index,
    size_t *next_parameter
) {
    return append_json_table_source_sql(string, source, source_index, next_parameter);
}

bool mylite_execution_column_descriptor_uses_string_key_collation(
    const struct mylite_catalog_column_descriptor *column,
    bool include_text_family
) {
    return column_descriptor_uses_string_key_collation(column, include_text_family);
}

int mylite_execution_append_string_key_collation_sql(struct mylite_dynamic_string *string) {
    return append_string_key_collation_sql(string);
}

int mylite_execution_append_mysql_quoted_text(
    struct mylite_dynamic_string *string,
    const char *text
) {
    return append_mysql_quoted_text(string, text);
}

bool mylite_execution_text_value_is_supported_string_key(const char *text, size_t text_length) {
    return text_value_is_supported_string_key(text, text_length);
}

const char *mylite_execution_scalar_pi_text(void) {
    return scalar_pi_text;
}

int mylite_execution_scalar_subquery_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return scalar_subquery_value(database, expression, out_cell);
}

int mylite_execution_scalar_rand_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return rand_function_value(database, expression, out_cell);
}

int mylite_execution_literal_projection_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return literal_projection_value(database, expression, out_cell);
}

int mylite_execution_format_session_scalar_uint64_value(
    struct mylite_db *database,
    uint64_t value,
    struct session_scalar_cell *out_cell
) {
    return format_session_scalar_uint64_value(database, value, out_cell);
}

int mylite_execution_validate_utf8_text(
    const char *text,
    size_t text_length,
    size_t *out_character_count
) {
    return validate_utf8_text(text, text_length, out_character_count);
}

int mylite_execution_utf8_sequence_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
) {
    return utf8_sequence_width(text, text_length, index, out_width);
}

bool mylite_execution_is_session_scalar_expression(const struct mylite_sql_ast_node *expression) {
    return is_session_scalar_expression(expression);
}

int mylite_execution_session_user_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct session_scalar_cell *out_cell
) {
    return mylite_execution_set_session_user_variable_value(database, node, out_cell);
}

int mylite_execution_session_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return session_scalar_value(database, expression, out_cell);
}

int mylite_execution_set_unknown_column_reference_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = collect_column_reference_parts(database, expression, parts, &part_count);

    if (rc == MYLITE_OK) {
        rc = format_column_reference_name(
            database,
            parts,
            part_count,
            column_name,
            sizeof(column_name)
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    set_unknown_column_error(database, column_name);
    return MYLITE_ERROR;
}

int mylite_execution_values_resolve_column_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char *column_name,
    size_t column_name_size,
    char *display_name,
    size_t display_name_size,
    size_t *out_part_count
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (column_name == NULL || column_name_size == 0U || display_name == NULL ||
        display_name_size == 0U || out_part_count == NULL) {
        return MYLITE_MISUSE;
    }
    column_name[0] = '\0';
    display_name[0] = '\0';
    *out_part_count = 0U;
    rc = collect_column_reference_parts(database, column_node, parts, &part_count);
    if (rc == MYLITE_OK && part_count != 0U) {
        int written = snprintf(column_name, column_name_size, "%s", parts[0]);

        if (written < 0 || (size_t)written >= column_name_size) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = format_column_reference_name(
            database,
            parts,
            part_count,
            display_name,
            display_name_size
        );
    }
    if (rc == MYLITE_OK) {
        *out_part_count = part_count;
    }
    return rc;
}

uint32_t mylite_execution_table_maintenance_result_collation_id(const char *collation_name) {
    return result_metadata_collation_id(collation_name);
}

int mylite_execution_table_maintenance_resolve_target_names(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *schema_name,
    size_t schema_name_size,
    char *table_name,
    size_t table_name_size
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int schema_written = 0;
    int table_written = 0;
    int rc = reject_builtin_schema_write_target(database, node);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = collect_identifier_parts(node, parts, table_name_part_capacity, &part_count, database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U) {
        if (database == NULL || !database->session.has_selected_schema) {
            set_no_database_error(database);
            return MYLITE_ERROR;
        }
        schema_written =
            snprintf(schema_name, schema_name_size, "%s", database->session.selected_schema);
        table_written = snprintf(table_name, table_name_size, "%s", parts[0]);
    } else if (part_count == 2U) {
        schema_written = snprintf(schema_name, schema_name_size, "%s", parts[0]);
        table_written = snprintf(table_name, table_name_size, "%s", parts[1]);
    } else {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (schema_written < 0 || (size_t)schema_written >= schema_name_size || table_written < 0 ||
        (size_t)table_written >= table_name_size) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

int mylite_execution_transaction_collect_identifier_parts(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t capacity,
    size_t *out_part_count
) {
    return collect_identifier_parts(node, parts, capacity, out_part_count, database);
}

int mylite_execution_set_copy_table_option_name_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size,
    const char *identifier_kind,
    const char *nul_message
) {
    return copy_table_option_name_text(
        database,
        node,
        destination,
        destination_size,
        (struct table_option_name_policy){
            .identifier_kind = identifier_kind,
            .nul_message = nul_message,
        }
    );
}

int mylite_execution_session_program_start_cursor_execution(mylite_stmt *statement) {
    return start_cursor_execution(statement);
}

int mylite_execution_session_program_finish_parse_failure(
    struct mylite_db *database,
    const struct mylite_sql_parse_result *parse_result,
    int parse_rc
) {
    return finish_parse_failure(database, parse_result, parse_rc);
}

int mylite_execution_session_program_execute_empty_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    return execute_empty_statement(database, out_result);
}

int mylite_execution_session_program_validate_alter_table_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    return validate_alter_table_algorithm_lock_options(database, statement);
}

int mylite_execution_session_program_execute_non_prepared_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_non_prepared_statement(database, context, statement, out_result);
}

int mylite_execution_session_program_execute_parsed_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_parsed_statement(database, context, statement, out_result);
}

bool mylite_execution_session_program_statement_result_is_select(
    const struct mylite_sql_ast_node *statement,
    const mylite_result *result
) {
    return statement_result_is_select(statement, result);
}

int mylite_execution_session_program_finish_failed_statement(
    struct mylite_db *database,
    struct mylite_statement_completion *completion,
    int rc,
    mylite_result **out_result
) {
    return finish_failed_statement(database, completion, rc, out_result);
}

int mylite_execution_session_program_finish_completed_statement(
    struct mylite_db *database,
    struct mylite_statement_completion *completion,
    bool completed_statement_is_select,
    int64_t completed_row_count,
    bool preserve_diagnostics_snapshot,
    mylite_result **out_result
) {
    return finish_completed_statement(
        database,
        completion,
        completed_statement_is_select,
        completed_row_count,
        preserve_diagnostics_snapshot,
        out_result
    );
}

int mylite_execution_session_program_resolve_selected_schema(
    struct mylite_db *database,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    return resolve_selected_schema(database, out_schema);
}

int mylite_execution_session_program_collect_identifier_parts(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t capacity,
    size_t *out_part_count
) {
    return collect_identifier_parts(node, parts, capacity, out_part_count, database);
}

int mylite_execution_session_program_resolve_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution
) {
    return resolve_table_name(database, node, out_resolution);
}

int mylite_execution_set_decode_table_option_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char **out_name,
    size_t *out_name_length,
    const char *identifier_kind,
    const char *nul_message
) {
    return decode_table_option_string_literal(
        database,
        node,
        out_name,
        out_name_length,
        (struct table_option_name_policy){
            .identifier_kind = identifier_kind,
            .nul_message = nul_message,
        }
    );
}

int mylite_execution_set_append_utf8_alias_warning(struct mylite_db *database) {
    return append_utf8_alias_warning(database);
}

int mylite_execution_set_append_utf8mb3_deprecation_warning(struct mylite_db *database) {
    return append_utf8mb3_deprecation_warning(database);
}

int mylite_execution_transaction_execute_physical_create_table(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const char *physical_name,
    bool temporary
) {
    return execute_physical_create_table(database, plan, physical_name, temporary);
}

int mylite_execution_transaction_execute_physical_drop_table(
    struct mylite_db *database,
    const char *physical_name
) {
    return execute_physical_drop_table(database, physical_name);
}

void mylite_execution_transaction_planned_column_from_catalog_descriptor(
    const struct mylite_catalog_column_descriptor *column,
    const struct mylite_sql_ast_node *default_node,
    struct planned_column *out_column
) {
    planned_column_from_catalog_descriptor(column, default_node, out_column);
}

void mylite_execution_transaction_planned_create_table_deinit(struct planned_create_table *plan) {
    planned_create_table_deinit(plan);
}

int mylite_execution_transaction_reserve_primary_key_parts(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t required_capacity
) {
    return reserve_planned_primary_key_parts(database, plan, required_capacity);
}

int mylite_execution_transaction_reserve_secondary_index_parts(
    struct mylite_db *database,
    struct planned_secondary_index *index,
    size_t required_capacity
) {
    return reserve_planned_secondary_index_parts(database, index, required_capacity);
}

int mylite_execution_transaction_reserve_secondary_indexes(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t required_capacity
) {
    return reserve_planned_secondary_indexes(database, plan, required_capacity);
}

int mylite_execution_transaction_resolve_lock_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution,
    struct mylite_catalog_table_descriptor *out_table
) {
    int rc = reject_builtin_schema_write_target(database, node);

    if (rc == MYLITE_OK) {
        rc = resolve_visible_table_reference(database, node, out_resolution, out_table);
    }
    return rc;
}

void mylite_execution_transaction_set_system_variable_value_error(
    struct mylite_db *database,
    const char *variable_name,
    const char *value
) {
    mylite_execution_set_system_variable_value_error(database, variable_name, value);
}

void mylite_execution_set_illegal_mix_of_collations_error(
    struct mylite_db *database,
    const char *first_collation,
    const char *second_collation,
    const char *operation
) {
    set_illegal_mix_of_collations_error(database, first_collation, second_collation, operation);
}

void mylite_execution_set_unknown_collation_error(
    struct mylite_db *database,
    const char *collation_name
) {
    set_unknown_collation_error(database, collation_name);
}

void mylite_execution_set_collation_not_valid_for_charset_error(
    struct mylite_db *database,
    const char *collation_name,
    const char *charset_name
) {
    set_collation_not_valid_for_charset_error(database, collation_name, charset_name);
}

void mylite_execution_set_nomem_error(struct mylite_db *database) {
    set_nomem_error(database);
}

void mylite_execution_set_runtime_error(struct mylite_db *database, const char *message) {
    set_runtime_error(database, message);
}

void mylite_execution_set_regexp_illegal_argument_error(struct mylite_db *database) {
    set_regexp_illegal_argument_error(database);
}

void mylite_execution_set_regexp_error(struct mylite_db *database, const char *message) {
    set_regexp_error(database, message);
}

void mylite_execution_set_regexp_character_range_error(
    struct mylite_db *database,
    const char *message
) {
    set_regexp_character_range_error(database, message);
}

void mylite_execution_session_scalar_cell_deinit(struct session_scalar_cell *cell) {
    session_scalar_cell_deinit(cell);
}

static int system_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return mylite_execution_session_system_variable_value(database, expression, out_cell);
}

static int database_character_set_system_variable_value(
    struct mylite_db *database,
    bool global_scope,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    return mylite_execution_session_database_character_set_system_variable_value(
        database,
        global_scope,
        buffer,
        buffer_size,
        out_value
    );
}

static int database_collation_system_variable_value(
    struct mylite_db *database,
    bool global_scope,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    return mylite_execution_session_database_collation_system_variable_value(
        database,
        global_scope,
        buffer,
        buffer_size,
        out_value
    );
}

static int format_session_scalar_uint64_value(
    struct mylite_db *database,
    uint64_t value,
    struct session_scalar_cell *out_cell
) {
    return mylite_execution_session_format_scalar_uint64_value(database, value, out_cell);
}

static int resolve_session_system_variable(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_execution_system_variable_kind *out_kind
) {
    return mylite_execution_session_resolve_system_variable(database, expression, out_kind);
}

static bool foreign_key_checks_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
) {
    return mylite_execution_session_foreign_key_checks_value(database, global_scope);
}

static bool session_sql_mode_has(const struct mylite_session_state *session, uint64_t mode) {
    return mylite_execution_session_sql_mode_has(session, mode);
}

static unsigned int lexer_modes_for_statement(const struct mylite_db *database) {
    return mylite_execution_lexer_modes_for_statement(database);
}

static int append_system_variable_read_warning(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
) {
    return mylite_execution_session_append_system_variable_read_warning(database, kind);
}

static int show_system_variable_value(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    bool global_scope,
    char *integer_buffer,
    size_t integer_buffer_size,
    const char **out_value
) {
    return mylite_execution_session_show_system_variable_value(
        database,
        kind,
        global_scope,
        integer_buffer,
        integer_buffer_size,
        out_value
    );
}

const char *mylite_execution_session_system_variable_override_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
) {
    return mylite_execution_set_session_system_variable_override_value(database, kind);
}

int mylite_execution_session_previous_diagnostics_condition_count(
    const struct mylite_diagnostics *diagnostics,
    uint64_t *out_count
) {
    return previous_diagnostics_condition_count(diagnostics, out_count);
}

int mylite_execution_session_previous_diagnostics_error_count(
    const struct mylite_diagnostics *diagnostics,
    uint64_t *out_count
) {
    return previous_diagnostics_error_count(diagnostics, out_count);
}

int mylite_execution_session_resolve_schema_name(
    struct mylite_db *database,
    const char *schema_name,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    return resolve_schema_name(database, schema_name, out_schema);
}

const char *mylite_execution_session_system_variable_override_show_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
) {
    return mylite_execution_set_session_system_variable_override_show_value(database, kind);
}

const char *mylite_execution_session_myisam_stats_method_text(
    enum mylite_session_myisam_stats_method value
) {
    return mylite_execution_myisam_stats_method_text(value);
}

const char *mylite_execution_session_transaction_isolation_text(
    enum mylite_transaction_isolation isolation
) {
    return mylite_execution_transaction_isolation_value_text(isolation);
}

const char *mylite_execution_session_transaction_read_only_scalar_text(
    enum mylite_transaction_access_mode access_mode
) {
    return mylite_execution_transaction_read_only_scalar_text(access_mode);
}

const char *mylite_execution_session_transaction_read_only_show_text(
    enum mylite_transaction_access_mode access_mode
) {
    return mylite_execution_transaction_read_only_show_text(access_mode);
}

bool mylite_execution_session_sql_mode_token_matches(
    const char *text,
    size_t length,
    const char *expected
) {
    return mylite_execution_set_sql_mode_token_matches(text, length, expected);
}

#include "mylite_execution_statement_entry.inc"

#include "mylite_execution_explain_statement.inc"

#include "mylite_execution_statement_session_handlers.inc"

#include "mylite_execution_admin_placeholders.inc"

#include "mylite_execution_ddl_create_table_statements.inc"

#include "mylite_execution_ddl_create_view_statements.inc"

#include "mylite_execution_ddl_create_schema_index_statements.inc"

#include "mylite_execution_ddl_drop_existence_statements.inc"

#include "mylite_execution_ddl_table_action_statements.inc"

#include "mylite_execution_ddl_alter_table_index_constraint_statements.inc"

#include "mylite_execution_ddl_alter_table_column_statements.inc"

#include "mylite_execution_ddl_alter_table_schema_option_statements.inc"

#include "mylite_execution_ddl_alter_table_maintenance_statements.inc"

#include "mylite_execution_dml_statements.inc"

#include "mylite_execution_metadata_queries.inc"

#include "mylite_execution_select_into_user_variables.inc"

// clang-format off
#include "mylite_execution_information_schema_join_compat.inc"
// clang-format on

#include "mylite_execution_mysql_system_query_dispatch.inc"

#include "mylite_execution_mysql_system_sys_auto_increment_rows.inc"

#include "mylite_execution_mysql_system_sys_statistics_rows.inc"

#include "mylite_execution_mysql_system_sys_table_index_health_rows.inc"

#include "mylite_execution_mysql_system_sys_object_overview_rows.inc"

#include "mylite_execution_mysql_system_innodb_stats_rows.inc"

#include "mylite_execution_information_schema_query_execution.inc"

#include "mylite_execution_information_schema_system_dispatch_rows.inc"

#include "mylite_execution_information_schema_catalog_dispatch_rows.inc"

#include "mylite_execution_information_schema_row_helpers.inc"

#include "mylite_execution_information_schema_static_core_rows.inc"

#include "mylite_execution_information_schema_static_storage_rows.inc"

#include "mylite_execution_information_schema_builtin_table_status_helpers.inc"

#include "mylite_execution_information_schema_base_table_status_rows.inc"

#include "mylite_execution_information_schema_columns_system_rows.inc"

#include "mylite_execution_information_schema_columns_base_rows.inc"

#include "mylite_execution_information_schema_innodb_virtual_rows.inc"

#include "mylite_execution_information_schema_innodb_column_rows.inc"

#include "mylite_execution_information_schema_innodb_table_rows.inc"

#include "mylite_execution_information_schema_innodb_index_rows.inc"

#include "mylite_execution_information_schema_innodb_foreign_rows.inc"

#include "mylite_execution_information_schema_constraint_rows.inc"

#include "mylite_execution_information_schema_key_constraint_rows.inc"

#include "mylite_execution_information_schema_statistics_rows.inc"

#include "mylite_execution_information_schema_predicate_values.inc"

#include "mylite_execution_information_schema_descriptor_metadata.inc"

#include "mylite_execution_show_tables_status_general.inc"

#include "mylite_execution_show_charset_variables_status.inc"

#include "mylite_execution_show_variables_where_eval.inc"

#include "mylite_execution_show_schema_objects_processlist_privileges.inc"

#include "mylite_execution_show_replication_metadata.inc"

#include "mylite_execution_show_diagnostics_output.inc"

#include "mylite_execution_show_columns_indexes.inc"

#include "mylite_execution_show_create.inc"

#include "mylite_execution_create_table_planning_core.inc"

#include "mylite_execution_create_table_column_default_charset.inc"

#include "mylite_execution_create_table_item_validation.inc"

#include "mylite_execution_primary_key_definition_planning.inc"

#include "mylite_execution_create_table_secondary_index_planning.inc"

#include "mylite_execution_create_table_foreign_key_planning.inc"

#include "mylite_execution_create_table_check_constraint_planning.inc"

#include "mylite_execution_create_table_generated_expression_rendering.inc"

#include "mylite_execution_check_expression_rendering.inc"

#include "mylite_execution_create_table_constraints.inc"

#include "mylite_execution_create_table_variants.inc"

#include "mylite_execution_table_options_planning.inc"

#include "mylite_execution_create_table_execution.inc"

#include "mylite_execution_schema_table_admin.inc"

#include "mylite_execution_alter_table_add_column.inc"

#include "mylite_execution_alter_table_add_index.inc"

#include "mylite_execution_alter_table_foreign_key_index.inc"

#include "mylite_execution_alter_table_check_constraints.inc"

#include "mylite_execution_alter_table_drop_rename_column.inc"

#include "mylite_execution_alter_table_modify_column_entry.inc"

#include "mylite_execution_alter_table_default_visibility_options.inc"

#include "mylite_execution_alter_table_charset_conversion_options.inc"

#include "mylite_execution_alter_table_table_option_actions.inc"

#include "mylite_execution_alter_table_modify_column_resolution.inc"

#include "mylite_execution_alter_table_modify_column_execution.inc"

#include "mylite_execution_alter_table_check_rebuild_sql.inc"

#include "mylite_execution_alter_table_rename_check_constraints.inc"

#include "mylite_execution_load_data_planning.inc"

#include "mylite_execution_dml_planning.inc"

#include "mylite_execution_insert_execution.inc"

#include "mylite_execution_insert_select_planning.inc"

#include "mylite_execution_insert_select_table_execution.inc"

#include "mylite_execution_insert_select_row_scalar_execution.inc"

#include "mylite_execution_insert_select_table_rows.inc"

#include "mylite_execution_insert_select_value_materialization.inc"

#include "mylite_execution_insert_select_validation_core.inc"

#include "mylite_execution_insert_select_string_validation.inc"

#include "mylite_execution_insert_select_type_validation.inc"

#include "mylite_execution_update_planning.inc"

#include "mylite_execution_update_execution.inc"

#include "mylite_execution_select_planning_core.inc"

#include "mylite_execution_select_json_table_source.inc"

#include "mylite_execution_grouped_aggregate_entry.inc"

#include "mylite_execution_grouped_aggregate_source_planning.inc"

#include "mylite_execution_grouped_aggregate_group_columns.inc"

#include "mylite_execution_grouped_aggregate_projection_columns.inc"

#include "mylite_execution_grouped_aggregate_function_planning.inc"

#include "mylite_execution_grouped_aggregate_having_planning.inc"

#include "mylite_execution_grouped_aggregate_literal_conversion.inc"

#include "mylite_execution_grouped_aggregate_order_planning.inc"

#include "mylite_execution_select_execution.inc"

#include "mylite_execution_aggregate_execution.inc"

#include "mylite_execution_scalar_projection_classification.inc"

#include "mylite_execution_scalar_projection_select_execution.inc"

#include "mylite_execution_scalar_result_metadata.inc"

#include "mylite_execution_session_scalar_result_helpers.inc"

#include "mylite_execution_session_scalar_warnings.inc"

#include "mylite_execution_scalar_projection_argument_diagnostics.inc"

#include "mylite_execution_scalar.inc"

#include "mylite_execution_scalar_string_core.inc"

#include "mylite_execution_scalar_temporal_core.inc"

#include "mylite_execution_scalar_string_extended.inc"

#include "mylite_execution_scalar_misc.inc"

#include "mylite_execution_spatial_functions.inc"

#include "mylite_execution_scalar_conversion.inc"

#include "mylite_execution_scalar_temporal_format.inc"

#include "mylite_execution_scalar_bitwise_eval.inc"

#include "mylite_execution_scalar_logical_eval.inc"

#include "mylite_execution_scalar_comparison_eval.inc"

#include "mylite_execution_scalar_arithmetic_eval.inc"

#include "mylite_execution_scalar_diagnostic_helpers.inc"

#include "mylite_execution_scalar_control_case_entry.inc"

#include "mylite_execution_scalar_control_if_eval.inc"

#include "mylite_execution_scalar_literal_projection.inc"

#include "mylite_execution_scalar_control_validation.inc"

#include "mylite_execution_scalar_projection.inc"

#include "mylite_execution_delete_planning.inc"

#include "mylite_execution_column_plan_entry.inc"

#include "mylite_execution_column_default_finalization.inc"

#include "mylite_execution_column_default_text.inc"

#include "mylite_execution_column_default_integer_eval.inc"

#include "mylite_execution_column_type_mapping.inc"

#include "mylite_execution_column_type_predicates.inc"

#include "mylite_execution_column_descriptor_parsing.inc"

#include "mylite_execution_column_row_size_validation.inc"

#include "mylite_execution_column_key_modify_validation.inc"

#include "mylite_execution_descriptor_helpers.inc"

#include "mylite_execution_insert_row_planning.inc"

#include "mylite_execution_insert_value_conversion.inc"

#include "mylite_execution_dml_default_values.inc"

#include "mylite_execution_dml_integer_conversion.inc"

#include "mylite_execution_dml_enum_set_conversion.inc"

#include "mylite_execution_dml_string_binary_conversion.inc"

#include "mylite_execution_dml_decimal_approx_conversion.inc"

#include "mylite_execution_dml_temporal_defaults.inc"

#include "mylite_execution_dml_value_helpers.inc"

#include "mylite_execution_dml_string_validation.inc"

#include "mylite_execution_dml_implicit_values.inc"

#include "mylite_execution_row_scalar_select_items.inc"

#include "mylite_execution_query_planning.inc"

#include "mylite_execution_row_scalar_numeric_planning.inc"

#include "mylite_execution_row_scalar_string_basic_planning.inc"

#include "mylite_execution_row_scalar_string_shape_planning.inc"

#include "mylite_execution_row_scalar_string_bitmask_search_planning.inc"

#include "mylite_execution_row_scalar_string_edit_planning.inc"

#include "mylite_execution_row_scalar_string_transform_planning.inc"

#include "mylite_execution_row_scalar_string_compare_set_planning.inc"

#include "mylite_execution_row_scalar_string_regexp_planning.inc"

#include "mylite_execution_row_scalar_json_planning.inc"

#include "mylite_execution_row_scalar_binary_value_planning.inc"

#include "mylite_execution_row_scalar_char_charset_planning.inc"

#include "mylite_execution_row_scalar_control_flow_planning.inc"

#include "mylite_execution_row_scalar_conversion_value_planning.inc"

#include "mylite_execution_row_scalar_concat_planning.inc"

#include "mylite_execution_row_scalar_temporal_format_planning.inc"

#include "mylite_execution_row_scalar_temporal_interval_extract_planning.inc"

#include "mylite_execution_row_scalar_temporal_conversion_planning.inc"

#include "mylite_execution_row_scalar_temporal_period_timezone_weight_planning.inc"

#include "mylite_execution_row_scalar_temporal_diff_planning.inc"

#include "mylite_execution_row_scalar_temporal_timestamp_planning.inc"

#include "mylite_execution_row_scalar_misc_planning.inc"

#include "mylite_execution_select_column_planning.inc"

#include "mylite_execution_select_predicate_entry.inc"

#include "mylite_execution_select_predicate_leaf_comparison.inc"

#include "mylite_execution_select_predicate_temporal_extract.inc"

#include "mylite_execution_select_predicate_string_functions.inc"

#include "mylite_execution_select_predicate_json_regexp_functions.inc"

#include "mylite_execution_select_predicate_subquery_correlation.inc"

#include "mylite_execution_select_predicate_special_in.inc"

#include "mylite_execution_select_predicate_work_helpers.inc"

#include "mylite_execution_select_predicate_value_conversion.inc"

#include "mylite_execution_select_predicate_temporal_literals.inc"

#include "mylite_execution_safe_updates_planning.inc"

#include "mylite_execution_update_planning_helpers.inc"

#include "mylite_execution_show_result_metadata.inc"

#include "mylite_execution_show_tables_helpers.inc"

#include "mylite_execution_show_table_status_rows_helpers.inc"

#include "mylite_execution_show_table_status_where_helpers.inc"

#include "mylite_execution_show_columns_helpers.inc"

#include "mylite_execution_show_index_rows_helpers.inc"

#include "mylite_execution_show_index_where_helpers.inc"

#include "mylite_execution_show_column_display_helpers.inc"

#include "mylite_execution_show_databases_helpers.inc"

#include "mylite_execution_show_table_status_count_helpers.inc"

#include "mylite_execution_sql_builder_alter_column_defaults.inc"

#include "mylite_execution_sql_builder_alter_modify_copy.inc"

#include "mylite_execution_sqlite_write_statements.inc"

#include "mylite_execution_insert_duplicate_write_helpers.inc"

#include "mylite_execution_update_unique_key_write_conflicts.inc"

#include "mylite_execution_foreign_key_write_validation.inc"

#include "mylite_execution_unique_key_write_lookup.inc"

#include "mylite_execution_key_tuple_formatting.inc"

#include "mylite_execution_count_having_select.inc"

#include "mylite_execution_count_expression_aggregate.inc"
