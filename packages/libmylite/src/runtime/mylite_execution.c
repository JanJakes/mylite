#if defined(__linux__) && !defined(_XOPEN_SOURCE)
#  define _XOPEN_SOURCE 700 /* NOLINT(bugprone-reserved-identifier): POSIX feature macro. */
#endif

#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_connection.h"
#include "mylite_convert_tz.h"
#include "mylite_date_format.h"
#include "mylite_date_interval_second.h"
#include "mylite_datediff.h"
#include "mylite_diagnostics.h"
#include "mylite_digest.h"
#include "mylite_dynamic_string.h"
#include "mylite_execution_ast_internal.h"
#include "mylite_execution_catalog.h"
#include "mylite_execution_connection_lifecycle.h"
#include "mylite_execution_diagnostics.h"
#include "mylite_execution_dml_numeric.h"
#include "mylite_execution_loaded_catalog.h"
#include "mylite_execution_plan_types.h"
#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_charset_collation.h"
#include "mylite_execution_scalar_numeric.h"
#include "mylite_execution_scalar_regexp.h"
#include "mylite_execution_scalar_string_position.h"
#include "mylite_execution_scalar_string_transform.h"
#include "mylite_execution_scalar_temporal_format.h"
#include "mylite_execution_show_filter.h"
#include "mylite_execution_sql_normalization.h"
#include "mylite_execution_sqlite_internal.h"
#include "mylite_execution_statement_transaction.h"
#include "mylite_execution_system_variables.h"
#include "mylite_execution_text_internal.h"
#include "mylite_integer_arithmetic.h"
#include "mylite_json.h"
#include "mylite_json_internal.h"
#include "mylite_lexer.h"
#include "mylite_mysql_error_codes.h"
#include "mylite_mysql_server_identity.h"
#include "mylite_named_locks.h"
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
            .modes = lexer_modes_for_session_sql_mode(&database->session),
        },
        &parse_result
    ));
#ifdef MYLITE_ENABLE_PROFILING
    mylite_profile_record_parse(database, profile_phase_started_ns);
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
        rc = finish_failed_statement(database, rc, out_result);
    } else {
        rc = finish_completed_statement(
            database,
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
        rc = execute_start_transaction_statement_with_characteristics(database, NULL, out_result);
        break;
    case MYLITE_TRANSACTION_CONTROL_COMMIT:
        rc = execute_commit_statement_with_chain(database, false, out_result);
        break;
    case MYLITE_TRANSACTION_CONTROL_ROLLBACK:
        rc = execute_rollback_statement_with_chain(database, false, out_result);
        break;
    default:
        rc = MYLITE_MISUSE;
        break;
    }

    if (rc != MYLITE_OK) {
        rc = finish_failed_statement(database, rc, out_result);
    } else {
        completed_row_count = mylite_result_affected_rows(*out_result);
        rc = finish_completed_statement(database, false, completed_row_count, false, out_result);
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
    stmt->metadata_context = result_column_metadata_context_init();
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

int mylite_stmt_step(mylite_stmt *stmt) {
    int sqlite_rc = SQLITE_OK;
#ifdef MYLITE_ENABLE_PROFILING
    uint64_t profile_step_started_ns = 0U;
#endif
    int rc = MYLITE_OK;

    if (stmt == NULL || stmt->database == NULL) {
        return MYLITE_MISUSE;
    }
    rc = reject_poisoned_connection(stmt->database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (stmt->sqlite_statement == NULL && !stmt->has_materialized_rows) {
        return stmt->done ? MYLITE_DONE : MYLITE_MISUSE;
    }
    if (stmt->done) {
        return MYLITE_DONE;
    }

#ifdef MYLITE_ENABLE_PROFILING
    mylite_profile_enter_api(stmt->database);
    profile_step_started_ns = mylite_profile_now_ns();
#endif
    stmt->current_row_available = false;
    if (stmt->has_materialized_rows) {
        if (stmt->materialized_row_index >= mylite_result_row_count(stmt->metadata_result)) {
            stmt->done = true;
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
        rc = read_selected_sqlite_row(
            stmt->database,
            stmt->sqlite_statement,
            mylite_result_column_count(stmt->metadata_result),
            stmt->select_plan.columns,
            stmt->select_plan.column_count,
            &stmt->row_storage
        );
        if (rc != MYLITE_OK) {
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
        rc = finish_cursor_sqlite_statement(stmt, rc);
        rc = rollback_statement_transaction(stmt->database, &stmt->read_transaction, rc);
        if (stmt->database->active_cursor == stmt) {
            stmt->database->active_cursor = NULL;
        }
        stmt->done = true;
        mylite_result *result = NULL;

        rc = finish_failed_statement(stmt->database, rc, &result);
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
    if (rc == MYLITE_OK && !stmt->done) {
        stmt->found_row_count = (uint64_t)stmt->row_count;
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
    rc = status_from_parse_status(mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = stmt->normalized_sql.sql,
            .length = stmt->normalized_sql.sql_size,
            .modes = lexer_modes_for_session_sql_mode(&database->session),
        },
        &stmt->parse_result
    ));
#ifdef MYLITE_ENABLE_PROFILING
    mylite_profile_record_parse(database, profile_phase_started_ns);
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
        set_unsupported_error(database, "cursor prepare supports exactly one SELECT statement");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        stmt->statement = child_at(stmt->parse_result.root, 0U);
        if (stmt->statement == NULL || stmt->statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT ||
            select_statement_into_list(stmt->statement) != NULL) {
            set_unsupported_error(database, "cursor prepare supports only SELECT statements");
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        database->session.active_statement_time =
            (int64_t)mylite_statement_context_time(&stmt->context);
        rc = prepare_statement_transaction_boundary(database, stmt->statement);
    }
    if (rc == MYLITE_OK) {
        rc = begin_read_statement_transaction(database, &stmt->read_transaction);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_cursor_select_plan(stmt);
        if (rc != MYLITE_OK && rc != MYLITE_NOMEM) {
            clear_cursor_select_plan_resources(stmt, rc);
            mylite_diagnostics_clear_condition(mylite_connection_diagnostics(database));
            rc = prepare_cursor_materialized_select_statement(stmt);
        }
    }
    if (rc == MYLITE_OK && !stmt->has_materialized_rows) {
        database->active_cursor = stmt;
    }
    if (rc != MYLITE_OK) {
        mylite_result *result = NULL;

        rc = rollback_statement_transaction(database, &stmt->read_transaction, rc);
        rc = finish_failed_statement(database, rc, &result);
        (void)mylite_statement_context_end(&stmt->context, rc);
        mylite_statement_context_deinit(&stmt->context);
        stmt->has_context = false;
    }
    return rc;
}

static int prepare_cursor_materialized_select_statement(mylite_stmt *stmt) {
    mylite_result *result = NULL;
    int rc = execute_select_statement_in_read_transaction(
        stmt->database,
        &stmt->context,
        stmt->statement,
        true,
        &result
    );

    if (rc == MYLITE_OK) {
        stmt->metadata_result = result;
        stmt->has_materialized_rows = true;
        rc = commit_statement_transaction(stmt->database, &stmt->read_transaction);
    }
    return rc;
}

static int prepare_cursor_select_plan(mylite_stmt *stmt) {
    struct mylite_db *database = stmt->database;
    int rc = plan_select(database, stmt->statement, true, &stmt->select_plan);

    if (rc == MYLITE_OK) {
        stmt->has_select_plan = true;
        rc = mylite_result_create(&stmt->metadata_result);
    }
    if (rc == MYLITE_OK) {
        rc = load_result_column_metadata_context(
            database,
            &stmt->select_plan,
            &stmt->metadata_context
        );
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < stmt->select_plan.column_count;
         ++column_index) {
        rc = append_select_result_column(
            database,
            stmt->metadata_result,
            &stmt->select_plan,
            &stmt->metadata_context,
            column_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = build_select_sql(&stmt->select_plan, &stmt->sqlite_sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_cached_sqlite_statement(database, stmt->sqlite_sql, &stmt->sqlite_statement);
        if (rc == MYLITE_OK) {
            stmt->sqlite_statement_is_cached = true;
        }
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_parameters(stmt->sqlite_statement, &stmt->select_plan);
    }
    if (rc != MYLITE_OK && stmt->sqlite_statement != NULL) {
        rc = finish_cursor_sqlite_statement(stmt, rc);
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (
        rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK
    ) {
        set_physical_sqlite_row_error(database);
        rc = MYLITE_ERROR;
    }
    return rc;
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
    free(stmt->sqlite_sql);
    stmt->sqlite_sql = NULL;
    sqlite_result_row_storage_deinit(&stmt->row_storage);
    if (stmt->has_select_plan) {
        planned_select_deinit(&stmt->select_plan);
        stmt->has_select_plan = false;
    }
    mylite_result_free(stmt->metadata_result);
    stmt->metadata_result = NULL;
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
        stmt->found_row_count = (uint64_t)stmt->row_count;
    }
    if (is_current_statement) {
        if (stmt->row_count > (size_t)INT64_MAX) {
            set_runtime_error(database, "SELECT row count is out of range");
            return MYLITE_ERROR;
        }
        mylite_diagnostics_clear_condition(mylite_connection_diagnostics(database));
        rc = snapshot_current_diagnostics(database);
        if (rc != MYLITE_OK) {
            database->session.previous_row_count = -1;
            return rc;
        }
        database->session.previous_row_count = (int64_t)stmt->row_count;
        database->session.found_rows = stmt->found_row_count;
    }
    if (stmt->has_context) {
        (void)mylite_statement_context_end(&stmt->context, MYLITE_OK);
        mylite_statement_context_deinit(&stmt->context);
        stmt->has_context = false;
    }
    if (is_current_statement) {
        clear_select_consumed_next_transaction_characteristics(database);
    }
    if (database->active_cursor == stmt) {
        database->active_cursor = NULL;
    }
    return MYLITE_OK;
}

int mylite_execution_prepare_connection_close(struct mylite_db *database) {
    int rc = MYLITE_OK;

    if (database == NULL) {
        return MYLITE_MISUSE;
    }
    mylite_execution_detach_connection_statements(database);
    if (database->session.user_transaction_active) {
        rc =
            normalize_sqlite_control_rc(database, execute_sqlite_control_sql(database, "ROLLBACK"));
        if (rc == MYLITE_OK) {
            mylite_catalog_invalidate_descriptor_cache(database);
            database->session.user_transaction_active = false;
            clear_active_transaction_characteristics(database);
            clear_user_savepoints(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = reconcile_persistent_auto_increment_high_waters(database);
    }
    if (rc == MYLITE_OK) {
        clear_persistent_auto_increment_high_waters(database);
    }
    return rc;
}

void mylite_execution_detach_connection_statements(struct mylite_db *database) {
    if (database == NULL) {
        return;
    }

    while (database->live_statements != NULL) {
        mylite_stmt *stmt = database->live_statements;

        database->live_statements = stmt->next_live_statement;
        stmt->next_live_statement = NULL;
        detach_cursor_statement(stmt);
    }
    database->active_cursor = NULL;
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

static void detach_cursor_statement(mylite_stmt *stmt) {
    if (stmt == NULL || stmt->database == NULL) {
        return;
    }

    if (stmt->database->active_cursor == stmt) {
        stmt->database->active_cursor = NULL;
    }
    release_cursor_statement_resources(stmt);
    stmt->database = NULL;
    stmt->current_row_available = false;
    stmt->done = true;
}

static void release_cursor_statement_resources(mylite_stmt *stmt) {
    if (stmt->sqlite_statement != NULL) {
        (void)finish_cursor_sqlite_statement(stmt, MYLITE_OK);
    }
    (void)rollback_statement_transaction(stmt->database, &stmt->read_transaction, MYLITE_OK);
    free(stmt->sqlite_sql);
    stmt->sqlite_sql = NULL;
    sqlite_result_row_storage_deinit(&stmt->row_storage);
    result_column_metadata_context_deinit(&stmt->metadata_context);
    if (stmt->has_select_plan) {
        planned_select_deinit(&stmt->select_plan);
        stmt->has_select_plan = false;
    }
    mylite_result_free(stmt->metadata_result);
    stmt->metadata_result = NULL;
    if (stmt->has_parse_result) {
        mylite_sql_parse_result_deinit(&stmt->parse_result);
        stmt->has_parse_result = false;
    }
    if (stmt->has_context) {
        (void)mylite_statement_context_end(&stmt->context, MYLITE_OK);
        mylite_statement_context_deinit(&stmt->context);
        stmt->has_context = false;
    }
    if (stmt->has_normalized_sql) {
        mylite_execution_normalized_sql_deinit(&stmt->normalized_sql);
        stmt->has_normalized_sql = false;
    }
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
        release_cursor_statement_resources(stmt);
    }
    free(stmt);
}

static int set_materialized_cursor_found_row_count(mylite_stmt *stmt) {
    if (mylite_result_has_found_row_count(stmt->metadata_result)) {
        stmt->found_row_count = mylite_result_found_row_count(stmt->metadata_result);
        return MYLITE_OK;
    }
    if (mylite_result_row_count(stmt->metadata_result) > UINT64_MAX) {
        set_runtime_error(stmt->database, "SELECT found-row count is out of range");
        return MYLITE_ERROR;
    }
    stmt->found_row_count = (uint64_t)mylite_result_row_count(stmt->metadata_result);
    return MYLITE_OK;
}

static int set_cursor_found_row_count(mylite_stmt *stmt) {
    int64_t count = 0;
    uint64_t found_row_count = 0U;
    int rc = MYLITE_OK;

    if (stmt->select_plan.calc_found_rows) {
        rc = read_select_found_row_count(stmt->database, &stmt->select_plan, &count);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (count < 0) {
            set_runtime_error(stmt->database, "invalid SQL_CALC_FOUND_ROWS count");
            return MYLITE_ERROR;
        }
        stmt->found_row_count = (uint64_t)count;
        return append_sql_calc_found_rows_deprecation_warning(stmt->database);
    }

    rc = found_row_count_for_select_limit_envelope(
        stmt->database,
        &stmt->select_plan,
        stmt->row_count,
        &found_row_count
    );
    if (rc == MYLITE_OK) {
        stmt->found_row_count = found_row_count;
    }
    return rc;
}

static int finish_parse_failure(
    struct mylite_db *database,
    const struct mylite_sql_parse_result *parse_result,
    int parse_rc
) {
    int rc = parse_rc;
    int snapshot_rc = MYLITE_OK;

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else {
        set_parse_error(database, parse_result);
    }
    database->session.previous_row_count = -1;

    snapshot_rc = snapshot_current_diagnostics(database);
    return snapshot_rc == MYLITE_OK ? rc : snapshot_rc;
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

int mylite_execution_scalar_hex_numeric_runtime_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value,
    bool *out_handled
) {
    enum mylite_execution_system_variable_kind variable = MYLITE_EXECUTION_SYSTEM_VARIABLE_NONE;
    int64_t timestamp = 0;
    int rc = MYLITE_OK;

    if (out_value == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_bitwise_value){.is_null = false, .integer = 0U};
    *out_handled = true;
    if (expression == NULL) {
        *out_handled = false;
        return MYLITE_OK;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION:
        out_value->integer = database->session.connection_id;
        return MYLITE_OK;
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
        out_value->integer = (uint64_t)database->session.previous_row_count;
        return MYLITE_OK;
    case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION:
        out_value->integer = database->session.found_rows;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
        out_value->integer = database->session.last_insert_id;
        return MYLITE_OK;
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
        rc = resolve_session_system_variable(database, expression, &variable);
        if (rc != MYLITE_OK) {
            return rc;
        }
        break;
    default:
        *out_handled = false;
        return MYLITE_OK;
    }

    switch (variable) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_INCREMENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_OFFSET:
        out_value->integer = auto_increment_step_system_variable_value(
            database,
            variable,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WAIT_TIMEOUT:
        out_value->integer = timeout_system_variable_value(
            database,
            variable,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN:
        if (system_variable_expression_has_global_scope(expression)) {
            out_value->integer = group_concat_max_len_default_value;
        } else {
            out_value->integer = database->session.group_concat_max_len;
        }
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INFORMATION_SCHEMA_STATS_EXPIRY:
        if (system_variable_expression_has_global_scope(expression)) {
            out_value->integer = information_schema_stats_expiry_default_value;
        } else {
            out_value->integer = database->session.information_schema_stats_expiry;
        }
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ERROR_COUNT:
        if (system_variable_expression_has_global_scope(expression)) {
            out_value->integer = max_error_count_default_value;
        } else {
            out_value->integer = database->session.max_error_count;
        }
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS:
        out_value->integer = foreign_key_checks_system_variable_uint64_value(
            database,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BIG_TABLES:
        out_value->integer = big_tables_system_variable_uint64_value(
            database,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    default:
        break;
    }

    if (mylite_execution_system_variable_is_boolean_session_placeholder(variable)) {
        out_value->integer = boolean_session_placeholder_system_variable_uint64_value(
            database,
            variable,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    }

    switch (variable) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOCOMMIT:
        out_value->integer = system_variable_expression_has_global_scope(expression) ||
                                     database->session.autocommit_enabled
                                 ? 1U
                                 : 0U;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN:
        out_value->integer = 1U;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_GENERATE_INVISIBLE_PRIMARY_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REPLICA_SKIP_COUNTER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REQUIRE_PRIMARY_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER:
        out_value->integer = 0U;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID:
        out_value->integer = server_id_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID_BITS:
        out_value->integer = server_id_bits_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PORT:
        out_value->integer = port_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_VERSION:
        out_value->integer = protocol_version_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SELECT_LIMIT:
        out_value->integer = sql_select_limit_system_variable_value(
            database,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TIMESTAMP:
        timestamp = current_timestamp_epoch(database);
        out_value->integer = (uint64_t)timestamp;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WARNING_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ERROR_COUNT:
        return diagnostics_count_system_variable_value(
            system_variable_count_diagnostics(database),
            variable,
            &out_value->integer
        );
    default:
        break;
    }

    *out_handled = false;
    return MYLITE_OK;
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

bool mylite_execution_is_temporal_constructor_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
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
    return session_user_variable_value(database, node, out_cell);
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

#include "mylite_execution_statement_entry.inc"

#include "mylite_execution_explain_statement.inc"

#include "mylite_execution_statement_session_handlers.inc"

#include "mylite_execution_prepared_statement_execution.inc"

#include "mylite_execution_transaction_characteristics.inc"

#include "mylite_execution_statement_transaction_boundaries.inc"

#include "mylite_execution_transaction_statements.inc"

#include "mylite_execution_lock_tables.inc"

#include "mylite_execution_statement_implicit_commits.inc"

#include "mylite_execution_session_savepoints.inc"

#include "mylite_execution_set_connection_charset.inc"

#include "mylite_execution_set_assignments.inc"

#include "mylite_execution_admin_placeholders.inc"

#include "mylite_execution_prepared_statement_support.inc"

#include "mylite_execution_stored_procedures.inc"

#include "mylite_execution_set_session_snapshot.inc"

#include "mylite_execution_set_system_variable_dispatch.inc"

#include "mylite_execution_set_boolean_variables.inc"

#include "mylite_execution_set_numeric_transaction_variables.inc"

#include "mylite_execution_set_limit_size_expiry_variables.inc"

#include "mylite_execution_set_m_session_limit_system_variables.inc"

#include "mylite_execution_set_o_optimizer_system_variables.inc"

#include "mylite_execution_set_timeout_variables.inc"

#include "mylite_execution_set_last_insert_id_variables.inc"

#include "mylite_execution_set_remaining_system_variables.inc"

#include "mylite_execution_set_jl_system_variables.inc"

#include "mylite_execution_set_myisam_system_variables.inc"

#include "mylite_execution_set_innodb_core_system_variables.inc"

#include "mylite_execution_set_innodb_storage_system_variables.inc"

#include "mylite_execution_set_connection_system_variables.inc"

#include "mylite_execution_set_binary_log_system_variables.inc"

#include "mylite_execution_set_replication_global_system_variables.inc"

#include "mylite_execution_set_bootstrap_system_variables.inc"

#include "mylite_execution_set_compatibility_system_variables.inc"

#include "mylite_execution_set_internal_session_system_variables.inc"

#include "mylite_execution_set_resource_tuning_system_variables.inc"

#include "mylite_execution_set_sql_mode_timestamp_time_zone.inc"

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

#include "mylite_execution_information_schema_join_plan.inc"
#include "mylite_execution_information_schema_join_compat.inc"

#include "mylite_execution_mysql_system_query_dispatch.inc"

#include "mylite_execution_mysql_system_performance_schema_setup_metrics_rows.inc"

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

#include "mylite_execution_information_schema_result_rows.inc"

#include "mylite_execution_information_schema_predicate_validation.inc"

#include "mylite_execution_information_schema_predicate_evaluation.inc"

#include "mylite_execution_information_schema_predicate_comparison.inc"

#include "mylite_execution_information_schema_predicate_values.inc"

#include "mylite_execution_information_schema_query_planning.inc"

#include "mylite_execution_information_schema_compare_format_helpers.inc"

#include "mylite_execution_information_schema_descriptor_metadata.inc"

#include "mylite_execution_table_maintenance_queries.inc"

#include "mylite_execution_show_tables_status_general.inc"

#include "mylite_execution_show_charset_variables_status.inc"

#include "mylite_execution_show_variables_where_eval.inc"

#include "mylite_execution_show_schema_objects_processlist_privileges.inc"

#include "mylite_execution_show_replication_metadata.inc"

#include "mylite_execution_show_diagnostics_output.inc"

#include "mylite_execution_show_columns_indexes.inc"

#include "mylite_execution_show_create.inc"

#include "mylite_execution_result_completion.inc"

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

#include "mylite_execution_values_statement.inc"

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

#include "mylite_execution_scalar_system_variables.inc"

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

#include "mylite_execution_select_order_planning.inc"

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

#include "mylite_execution_sql_builder_create_table_index_helpers.inc"

#include "mylite_execution_sql_builder_drop_alter_add_column_index.inc"

#include "mylite_execution_sql_builder_alter_column_defaults.inc"

#include "mylite_execution_sql_builder_alter_modify_copy.inc"

#include "mylite_execution_sql_builder_alter_order_force_rename_truncate.inc"

#include "mylite_execution_insert_sql_builders.inc"

#include "mylite_execution_select_sql_builders.inc"

#include "mylite_execution_row_scalar_sql_core.inc"

#include "mylite_execution_row_scalar_sql_functions.inc"

#include "mylite_execution_row_scalar_sql_json_control.inc"

#include "mylite_execution_aggregate_predicate_sql_builders.inc"

#include "mylite_execution_dml_sql_builders.inc"

#include "mylite_execution_sqlite_write_statements.inc"

#include "mylite_execution_insert_duplicate_write_helpers.inc"

#include "mylite_execution_update_unique_key_write_conflicts.inc"

#include "mylite_execution_foreign_key_write_validation.inc"

#include "mylite_execution_unique_key_write_lookup.inc"

#include "mylite_execution_key_tuple_formatting.inc"

#include "mylite_execution_row_scalar_select_parameter_binding.inc"

#include "mylite_execution_count_having_select.inc"

#include "mylite_execution_count_expression_aggregate.inc"

#include "mylite_execution_row_scalar_expression_parameter_dispatch.inc"

#include "mylite_execution_row_scalar_window_parameter_binding.inc"

#include "mylite_execution_row_scalar_conversion_parameter_binding.inc"

#include "mylite_execution_row_scalar_arithmetic_parameter_binding.inc"

#include "mylite_execution_row_scalar_temporal_string_parameter_binding.inc"

#include "mylite_execution_row_scalar_string_regexp_parameter_binding.inc"

#include "mylite_execution_row_scalar_json_parameter_binding.inc"

#include "mylite_execution_row_scalar_control_flow_parameter_binding.inc"

#include "mylite_execution_row_scalar_encoding_uuid_char_parameter_binding.inc"

#include "mylite_execution_predicate_dml_parameter_binding.inc"

#include "mylite_execution_sqlite_result_extraction.inc"
