#include "mylite_catalog.h"

#include "mylite_catalog_internal.h"

#include "mylite_connection.h"
#include "mylite_sqlite_registration.h"
#include "sqlite3.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    catalog_table_key_block_size_eight = 8,
    catalog_table_key_block_size_sixteen = 16,
    catalog_table_stats_sample_pages_max = 65535,
};

enum catalog_table_insert_bind_index {
    catalog_table_insert_schema_id_bind = 1,
    catalog_table_insert_name_bind = 2,
    catalog_table_insert_kind_bind = 3,
    catalog_table_insert_physical_name_bind = 4,
    catalog_table_insert_auto_increment_next_bind = 5,
    catalog_table_insert_default_charset_bind = 6,
    catalog_table_insert_default_collation_bind = 7,
    catalog_table_insert_comment_bind = 8,
    catalog_table_insert_created_time_bind = 9,
    catalog_table_insert_updated_time_bind = 10,
    catalog_table_insert_generation_bind = 11,
};

enum catalog_table_insert_in_mutation_bind_index {
    catalog_table_insert_in_mutation_table_id_bind = 1,
    catalog_table_insert_in_mutation_schema_id_bind = 2,
    catalog_table_insert_in_mutation_name_bind = 3,
    catalog_table_insert_in_mutation_kind_bind = 4,
    catalog_table_insert_in_mutation_physical_name_bind = 5,
    catalog_table_insert_in_mutation_auto_increment_next_bind = 6,
    catalog_table_insert_in_mutation_auto_increment_status_bind = 7,
    catalog_table_insert_in_mutation_default_charset_bind = 8,
    catalog_table_insert_in_mutation_default_collation_bind = 9,
    catalog_table_insert_in_mutation_comment_bind = 10,
    catalog_table_insert_in_mutation_row_format_bind = 11,
    catalog_table_insert_in_mutation_key_block_size_bind = 12,
    catalog_table_insert_in_mutation_pack_keys_bind = 13,
    catalog_table_insert_in_mutation_checksum_bind = 14,
    catalog_table_insert_in_mutation_stats_persistent_bind = 15,
    catalog_table_insert_in_mutation_stats_auto_recalc_bind = 16,
    catalog_table_insert_in_mutation_stats_sample_pages_bind = 17,
    catalog_table_insert_in_mutation_min_rows_bind = 18,
    catalog_table_insert_in_mutation_max_rows_bind = 19,
    catalog_table_insert_in_mutation_avg_row_length_bind = 20,
    catalog_table_insert_in_mutation_delay_key_write_bind = 21,
    catalog_table_insert_in_mutation_created_time_bind = 22,
    catalog_table_insert_in_mutation_updated_time_bind = 23,
    catalog_table_insert_in_mutation_generation_bind = 24,
};

enum catalog_view_insert_bind_index {
    catalog_view_insert_table_id_bind = 1,
    catalog_view_insert_view_definition_bind = 2,
    catalog_view_insert_show_create_sql_bind = 3,
    catalog_view_insert_check_option_bind = 4,
    catalog_view_insert_is_updatable_bind = 5,
    catalog_view_insert_definer_bind = 6,
    catalog_view_insert_security_type_bind = 7,
    catalog_view_insert_character_set_client_bind = 8,
    catalog_view_insert_collation_connection_bind = 9,
    catalog_view_insert_source_schema_id_bind = 10,
    catalog_view_insert_source_table_id_bind = 11,
    catalog_view_insert_source_schema_name_bind = 12,
    catalog_view_insert_source_table_name_bind = 13,
    catalog_view_insert_generation_bind = 14,
};

enum catalog_table_storage_statistics_update_bind_index {
    catalog_table_storage_statistics_update_row_format_bind = 1,
    catalog_table_storage_statistics_update_key_block_size_bind = 2,
    catalog_table_storage_statistics_update_pack_keys_bind = 3,
    catalog_table_storage_statistics_update_checksum_bind = 4,
    catalog_table_storage_statistics_update_stats_persistent_bind = 5,
    catalog_table_storage_statistics_update_stats_auto_recalc_bind = 6,
    catalog_table_storage_statistics_update_stats_sample_pages_bind = 7,
    catalog_table_storage_statistics_update_min_rows_bind = 8,
    catalog_table_storage_statistics_update_max_rows_bind = 9,
    catalog_table_storage_statistics_update_avg_row_length_bind = 10,
    catalog_table_storage_statistics_update_delay_key_write_bind = 11,
    catalog_table_storage_statistics_update_generation_bind = 12,
    catalog_table_storage_statistics_update_table_id_bind = 13,
};

enum catalog_column_reorder_offset_bind_index {
    catalog_column_reorder_offset_bind = 1,
    catalog_column_reorder_offset_table_id_bind = 2,
};

enum catalog_column_reorder_bind_index {
    catalog_column_reorder_ordinal_bind = 1,
    catalog_column_reorder_version_increment_bind = 2,
    catalog_column_reorder_generation_bind = 3,
    catalog_column_reorder_table_id_bind = 4,
    catalog_column_reorder_column_id_bind = 5,
};

struct catalog_view_values {
    const char *view_definition;
    const char *show_create_sql;
    const char *check_option;
    const char *is_updatable;
    const char *definer;
    const char *security_type;
    const char *character_set_client;
    const char *collation_connection;
    int64_t source_schema_id;
    int64_t source_table_id;
    const char *source_schema_name;
    const char *source_table_name;
};

static int bind_catalog_table_insert_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    int64_t auto_increment_next,
    const struct mylite_catalog_table_descriptor_input *values,
    uint64_t generation
);
static int bind_catalog_table_insert_identity_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    const struct mylite_catalog_table_descriptor_input *values,
    int64_t auto_increment_next
);
static int bind_catalog_table_insert_default_values(
    sqlite3_stmt *statement,
    const struct mylite_catalog_table_descriptor_input *values
);
static int bind_catalog_table_insert_storage_statistics_values(
    sqlite3_stmt *statement,
    const struct mylite_catalog_table_descriptor_input *values
);
static int bind_catalog_table_insert_lifecycle_values(
    sqlite3_stmt *statement,
    const struct mylite_catalog_table_descriptor_input *values,
    uint64_t generation
);
static int bind_catalog_view_insert_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    const struct catalog_view_values *values,
    uint64_t generation
);
static int offset_catalog_column_ordinals_for_reorder(
    struct mylite_db *database,
    const struct mylite_catalog_column_reorder *reorder
);
static int apply_catalog_column_reorder(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct mylite_catalog_column_reorder *reorder,
    size_t column_index
);
static int validate_catalog_column_reorder_request(
    const struct mylite_catalog_column_reorder *reorder
);
static int text_equals_ascii_case_insensitive(const char *left, const char *right);
static int text_has_ascii_case_insensitive_prefix(const char *text, const char *prefix);
static char ascii_lower(unsigned char byte);

int mylite_catalog_allocate_table_id_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t *out_table_id
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (out_table_id != NULL) {
        *out_table_id = 0;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_table_id == NULL) {
        return MYLITE_MISUSE;
    }

    return mylite_catalog_read_next_table_id(database->sqlite, out_table_id);
}

int mylite_catalog_allocate_index_id_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t *out_index_id
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (out_index_id != NULL) {
        *out_index_id = 0;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_index_id == NULL) {
        return MYLITE_MISUSE;
    }

    return mylite_catalog_read_next_index_id(database->sqlite, out_index_id);
}

int mylite_catalog_allocate_foreign_key_id_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t *out_foreign_key_id
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (out_foreign_key_id != NULL) {
        *out_foreign_key_id = 0;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_foreign_key_id == NULL) {
        return MYLITE_MISUSE;
    }

    return mylite_catalog_read_next_foreign_key_id(database->sqlite, out_foreign_key_id);
}

int mylite_catalog_allocate_check_constraint_id_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t *out_check_constraint_id
) {
    int rc = MYLITE_OK;

    if (out_check_constraint_id != NULL) {
        *out_check_constraint_id = 0;
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_active_mutation(mutation);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_check_constraint_id == NULL) {
        return MYLITE_MISUSE;
    }

    return mylite_catalog_read_next_check_constraint_id(database->sqlite, out_check_constraint_id);
}

int mylite_catalog_insert_table_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id, // NOLINT(bugprone-easily-swappable-parameters)
    int64_t schema_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_table_kind kind, // NOLINT(bugprone-easily-swappable-parameters)
    int64_t auto_increment_next,
    int64_t auto_increment_status,
    const char *default_charset,
    const char *default_collation,
    const char *comment,
    const char *row_format_option,
    int64_t key_block_size,
    int64_t pack_keys,
    int64_t checksum,
    int64_t stats_persistent,
    int64_t stats_auto_recalc,
    int64_t stats_sample_pages,
    int64_t min_rows,
    int64_t max_rows,
    int64_t avg_row_length,
    int64_t delay_key_write,
    int64_t created_time_utc_epoch,
    int64_t updated_time_utc_epoch,
    struct mylite_catalog_table_descriptor *out_table
) {
    const struct mylite_catalog_table_descriptor_input descriptor = {
        .schema_id = schema_id,
        .name = name,
        .physical_name = physical_name,
        .kind = kind,
        .auto_increment_status = auto_increment_status,
        .default_charset = default_charset,
        .default_collation = default_collation,
        .comment = comment,
        .row_format_option = row_format_option,
        .key_block_size = key_block_size,
        .pack_keys = pack_keys,
        .checksum = checksum,
        .stats_persistent = stats_persistent,
        .stats_auto_recalc = stats_auto_recalc,
        .stats_sample_pages = stats_sample_pages,
        .min_rows = min_rows,
        .max_rows = max_rows,
        .avg_row_length = avg_row_length,
        .delay_key_write = delay_key_write,
        .created_time_utc_epoch = created_time_utc_epoch,
        .updated_time_utc_epoch = updated_time_utc_epoch,
    };
    struct mylite_catalog_schema_descriptor schema = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_table != NULL) {
        *out_table = (struct mylite_catalog_table_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (auto_increment_next <= 0 || auto_increment_status < 0) {
        return MYLITE_ERROR;
    }
    rc = mylite_catalog_validate_table_descriptor_input(&descriptor);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_read_schema_by_id_from_sqlite(database->sqlite, schema_id, &schema);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_tables "
        "(table_id, schema_id, name, kind, physical_name, auto_increment_next, "
        "auto_increment_status, default_charset, default_collation, comment, row_format_option, "
        "key_block_size, pack_keys, checksum, stats_persistent, stats_auto_recalc, "
        "stats_sample_pages, min_rows, max_rows, avg_row_length, delay_key_write, "
        "fulltext_doc_id_initialized, created_time_utc_epoch, "
        "updated_time_utc_epoch, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, "
        "?17, ?18, ?19, ?20, ?21, 0, ?22, ?23, 1, ?24, ?24)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_catalog_table_insert_values(
            statement,
            table_id,
            auto_increment_next,
            &descriptor,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_table != NULL) {
        return mylite_catalog_read_table_by_id_from_sqlite(database->sqlite, table_id, out_table);
    }

    return MYLITE_OK;
}

static int bind_catalog_table_insert_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    int64_t auto_increment_next,
    const struct mylite_catalog_table_descriptor_input *values,
    uint64_t generation
) {
    int rc =
        bind_catalog_table_insert_identity_values(statement, table_id, values, auto_increment_next);

    if (rc == MYLITE_OK) {
        rc = bind_catalog_table_insert_default_values(statement, values);
    }
    if (rc == MYLITE_OK) {
        rc = bind_catalog_table_insert_storage_statistics_values(statement, values);
    }
    if (rc == MYLITE_OK) {
        rc = bind_catalog_table_insert_lifecycle_values(statement, values, generation);
    }
    return rc;
}

static int bind_catalog_table_insert_identity_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    const struct mylite_catalog_table_descriptor_input *values,
    int64_t auto_increment_next
) {
    int rc = values == NULL ? MYLITE_MISUSE
                            : mylite_catalog_bind_i64(
                                  statement,
                                  catalog_table_insert_in_mutation_table_id_bind,
                                  table_id
                              );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_schema_id_bind,
            values->schema_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_table_insert_in_mutation_name_bind,
            values->name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_kind_bind,
            (int64_t)values->kind
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_table_insert_in_mutation_physical_name_bind,
            values->physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_auto_increment_next_bind,
            auto_increment_next
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_auto_increment_status_bind,
            values->auto_increment_status
        );
    }
    return rc;
}

static int bind_catalog_table_insert_default_values(
    sqlite3_stmt *statement,
    const struct mylite_catalog_table_descriptor_input *values
) {
    int rc = values == NULL ? MYLITE_MISUSE
                            : mylite_catalog_bind_text(
                                  statement,
                                  catalog_table_insert_in_mutation_default_charset_bind,
                                  values->default_charset
                              );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_table_insert_in_mutation_default_collation_bind,
            values->default_collation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_table_insert_in_mutation_comment_bind,
            values->comment
        );
    }
    return rc;
}

static int bind_catalog_table_insert_storage_statistics_values(
    sqlite3_stmt *statement,
    const struct mylite_catalog_table_descriptor_input *values
) {
    int rc = values == NULL ? MYLITE_MISUSE
                            : mylite_catalog_bind_text(
                                  statement,
                                  catalog_table_insert_in_mutation_row_format_bind,
                                  values->row_format_option
                              );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_key_block_size_bind,
            values->key_block_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_pack_keys_bind,
            values->pack_keys
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_checksum_bind,
            values->checksum
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_stats_persistent_bind,
            values->stats_persistent
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_stats_auto_recalc_bind,
            values->stats_auto_recalc
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_stats_sample_pages_bind,
            values->stats_sample_pages
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_min_rows_bind,
            values->min_rows
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_max_rows_bind,
            values->max_rows
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_avg_row_length_bind,
            values->avg_row_length
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_delay_key_write_bind,
            values->delay_key_write
        );
    }
    return rc;
}

static int bind_catalog_table_insert_lifecycle_values(
    sqlite3_stmt *statement,
    const struct mylite_catalog_table_descriptor_input *values,
    uint64_t generation
) {
    int rc = values == NULL ? MYLITE_MISUSE
                            : mylite_catalog_bind_i64(
                                  statement,
                                  catalog_table_insert_in_mutation_created_time_bind,
                                  values->created_time_utc_epoch
                              );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_updated_time_bind,
            values->updated_time_utc_epoch
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(
            statement,
            catalog_table_insert_in_mutation_generation_bind,
            generation
        );
    }
    return rc;
}

int mylite_catalog_insert_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t ordinal_position,
    const char *name,
    const char *logical_type,
    const char *physical_type,
    bool is_nullable,
    bool is_visible,
    bool is_auto_increment,
    enum mylite_catalog_column_default_kind default_kind,
    int64_t default_integer,
    const char *default_text,
    bool on_update_current_timestamp,
    const char *character_set_name,
    const char *collation_name,
    const char *comment,
    bool is_generated,
    enum mylite_catalog_generated_column_kind generated_kind,
    const char *generation_expression,
    const char *sqlite_generation_expression,
    struct mylite_catalog_column_descriptor *out_column
) {
    const struct mylite_catalog_column_values values = {
        .name = name,
        .logical_type = logical_type,
        .physical_type = physical_type,
        .is_nullable = is_nullable,
        .is_visible = is_visible,
        .is_auto_increment = is_auto_increment,
        .default_kind = default_kind,
        .default_integer = default_integer,
        .default_text = default_text,
        .on_update_current_timestamp = on_update_current_timestamp,
        .character_set_name = character_set_name,
        .collation_name = collation_name,
        .comment = comment == NULL ? "" : comment,
        .is_generated = is_generated,
        .generated_kind = generated_kind,
        .generation_expression = generation_expression == NULL ? "" : generation_expression,
        .sqlite_generation_expression =
            sqlite_generation_expression == NULL ? "" : sqlite_generation_expression,
    };
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_column != NULL) {
        *out_column = (struct mylite_catalog_column_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_ordinal(ordinal_position);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_column_values(&values, false);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_columns "
        "(table_id, ordinal_position, name, logical_type, physical_type, is_nullable, "
        "is_visible, is_auto_increment, default_kind, default_integer, default_text, "
        "on_update_current_timestamp, character_set_name, collation_name, comment, "
        "is_generated, generated_kind, generation_expression, sqlite_generation_expression, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, "
        "?16, ?17, ?18, ?19, 1, ?20, ?20)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_column_insert_values(
            statement,
            table_id,
            ordinal_position,
            &values,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_column != NULL) {
        return mylite_catalog_read_column_by_name_from_sqlite(
            database->sqlite,
            table_id,
            name,
            out_column
        );
    }

    return MYLITE_OK;
}

int mylite_catalog_insert_view_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const char *view_definition,
    const char *show_create_sql,
    const char *check_option,
    const char *is_updatable,
    const char *definer,
    const char *security_type,
    const char *character_set_client,
    const char *collation_connection,
    int64_t source_schema_id,
    int64_t source_table_id,
    const char *source_schema_name,
    const char *source_table_name,
    struct mylite_catalog_view_descriptor *out_view
) {
    const struct catalog_view_values values = {
        .view_definition = view_definition,
        .show_create_sql = show_create_sql,
        .check_option = check_option,
        .is_updatable = is_updatable,
        .definer = definer,
        .security_type = security_type,
        .character_set_client = character_set_client,
        .collation_connection = collation_connection,
        .source_schema_id = source_schema_id,
        .source_table_id = source_table_id,
        .source_schema_name = source_schema_name,
        .source_table_name = source_table_name,
    };
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_view != NULL) {
        *out_view = (struct mylite_catalog_view_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            view_definition,
            MYLITE_CATALOG_VIEW_DEFINITION_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            show_create_sql,
            MYLITE_CATALOG_VIEW_SHOW_CREATE_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc =
            mylite_catalog_validate_required_name(check_option, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc =
            mylite_catalog_validate_required_name(is_updatable, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(definer, MYLITE_CATALOG_DEFINER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            security_type,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            character_set_client,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            collation_connection,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(source_schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(source_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            source_schema_name,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            source_table_name,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_views "
        "(table_id, view_definition, show_create_sql, check_option, is_updatable, definer, "
        "security_type, character_set_client, collation_connection, source_schema_id, "
        "source_table_id, source_schema_name, source_table_name, descriptor_version, "
        "created_catalog_generation, "
        "updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, 1, ?14, ?14)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_catalog_view_insert_values(
            statement,
            table_id,
            &values,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_view != NULL) {
        return mylite_catalog_read_view_by_table_id_from_sqlite(
            database->sqlite,
            table_id,
            out_view
        );
    }

    return MYLITE_OK;
}

static int bind_catalog_view_insert_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    const struct catalog_view_values *values,
    uint64_t generation
) {
    int rc = values == NULL
                 ? MYLITE_MISUSE
                 : mylite_catalog_bind_i64(statement, catalog_view_insert_table_id_bind, table_id);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_view_insert_view_definition_bind,
            values->view_definition
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_view_insert_show_create_sql_bind,
            values->show_create_sql
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_view_insert_check_option_bind,
            values->check_option
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_view_insert_is_updatable_bind,
            values->is_updatable
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, catalog_view_insert_definer_bind, values->definer);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_view_insert_security_type_bind,
            values->security_type
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_view_insert_character_set_client_bind,
            values->character_set_client
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_view_insert_collation_connection_bind,
            values->collation_connection
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_view_insert_source_schema_id_bind,
            values->source_schema_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_view_insert_source_table_id_bind,
            values->source_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_view_insert_source_schema_name_bind,
            values->source_schema_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_view_insert_source_table_name_bind,
            values->source_table_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, catalog_view_insert_generation_bind, generation);
    }
    return rc;
}

int mylite_catalog_delete_table_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_delete_foreign_keys_for_related_table_from_sqlite(
        database->sqlite,
        table_id
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_delete_check_constraints_for_table_from_sqlite(
            database->sqlite,
            table_id
        );
    }

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_views WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_index_columns WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_columns WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_tables WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_delete_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t column_id,
    int64_t ordinal_position
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_ordinal(ordinal_position);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_columns WHERE table_id = ?1 AND column_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "UPDATE _mylite_catalog_columns "
            "SET ordinal_position = ordinal_position - 1, "
            "descriptor_version = descriptor_version + 1, "
            "updated_catalog_generation = ?1 "
            "WHERE table_id = ?2 AND ordinal_position > ?3",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 1, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, ordinal_position);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_rename_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t column_id,
    const char *name
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET name = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND column_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 4, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_replace_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t column_id,
    const char *name,
    const char *logical_type,
    const char *physical_type,
    bool is_nullable,
    bool is_visible,
    bool is_auto_increment,
    enum mylite_catalog_column_default_kind default_kind,
    int64_t default_integer,
    const char *default_text,
    bool on_update_current_timestamp,
    const char *character_set_name,
    const char *collation_name,
    const char *comment,
    bool is_generated,
    enum mylite_catalog_generated_column_kind generated_kind,
    const char *generation_expression,
    const char *sqlite_generation_expression
) {
    const struct mylite_catalog_column_values values = {
        .name = name,
        .logical_type = logical_type,
        .physical_type = physical_type,
        .is_nullable = is_nullable,
        .is_visible = is_visible,
        .is_auto_increment = is_auto_increment,
        .default_kind = default_kind,
        .default_integer = default_integer,
        .default_text = default_text,
        .on_update_current_timestamp = on_update_current_timestamp,
        .character_set_name = character_set_name,
        .collation_name = collation_name,
        .comment = comment == NULL ? "" : comment,
        .is_generated = is_generated,
        .generated_kind = generated_kind,
        .generation_expression = generation_expression == NULL ? "" : generation_expression,
        .sqlite_generation_expression =
            sqlite_generation_expression == NULL ? "" : sqlite_generation_expression,
    };
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_column_values(&values, true);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET name = ?1, logical_type = ?2, physical_type = ?3, is_nullable = ?4, "
        "is_visible = ?5, is_auto_increment = ?6, default_kind = ?7, default_integer = ?8, "
        "default_text = ?9, on_update_current_timestamp = ?10, character_set_name = ?11, "
        "collation_name = ?12, comment = ?13, is_generated = ?14, generated_kind = ?15, "
        "generation_expression = ?16, sqlite_generation_expression = ?17, "
        "descriptor_version = descriptor_version + 1, updated_catalog_generation = ?18 "
        "WHERE table_id = ?19 AND column_id = ?20",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_column_replace_values(
            statement,
            table_id,
            column_id,
            &values,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_reorder_columns_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct mylite_catalog_column_reorder *reorder
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_catalog_column_reorder_request(reorder);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = offset_catalog_column_ordinals_for_reorder(database, reorder);
    for (size_t index = 0U; rc == MYLITE_OK && index < reorder->column_count; ++index) {
        rc = apply_catalog_column_reorder(database, mutation, reorder, index);
    }

    return rc;
}

static int offset_catalog_column_ordinals_for_reorder(
    struct mylite_db *database,
    const struct mylite_catalog_column_reorder *reorder
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET ordinal_position = ordinal_position + ?1 WHERE table_id = ?2",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_reorder_offset_bind,
            (int64_t)reorder->column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_reorder_offset_table_id_bind,
            reorder->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK &&
        sqlite3_changes64(database->sqlite) != (sqlite3_int64)reorder->column_count) {
        rc = MYLITE_ERROR;
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int apply_catalog_column_reorder(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct mylite_catalog_column_reorder *reorder,
    size_t column_index
) {
    const struct mylite_catalog_column_descriptor *column = &reorder->columns[column_index];
    int64_t final_ordinal = (int64_t)column_index + 1;
    bool ordinal_changed = column->ordinal_position != final_ordinal;
    int64_t increment_descriptor_version =
        (ordinal_changed && column->column_id != reorder->metadata_replaced_column_id) ? 1 : 0;
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (column->table_id != reorder->table_id) {
        return MYLITE_MISUSE;
    }
    rc = mylite_catalog_validate_positive_id(column->column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_ordinal(final_ordinal);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET ordinal_position = ?1, "
        "descriptor_version = descriptor_version + ?2, "
        "updated_catalog_generation = CASE WHEN ?2 <> 0 THEN ?3 "
        "ELSE updated_catalog_generation END "
        "WHERE table_id = ?4 AND column_id = ?5",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_column_reorder_ordinal_bind, final_ordinal);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_reorder_version_increment_bind,
            increment_descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(
            statement,
            catalog_column_reorder_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_reorder_table_id_bind,
            reorder->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_reorder_column_id_bind,
            column->column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int validate_catalog_column_reorder_request(
    const struct mylite_catalog_column_reorder *reorder
) {
    int rc = MYLITE_OK;

    if (reorder == NULL || reorder->columns == NULL || reorder->column_count == 0U ||
        reorder->column_count > (size_t)(INT64_MAX / 2)) {
        return MYLITE_MISUSE;
    }
    rc = mylite_catalog_validate_positive_id(reorder->table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(reorder->metadata_replaced_column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return MYLITE_OK;
}

int mylite_catalog_set_column_visibility_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t column_id,
    bool is_visible
) {
    sqlite3_stmt *statement = NULL;
    int64_t visible_value = 0;
    int rc = mylite_catalog_validate_ready_database(database);

    if (is_visible) {
        visible_value = 1;
    }

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET is_visible = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND column_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, visible_value);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 4, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_delete_schema_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t schema_id
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_delete_foreign_keys_for_schema_from_sqlite(database->sqlite, schema_id);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_delete_check_constraints_for_schema_from_sqlite(
            database->sqlite,
            schema_id
        );
    }

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_index_columns "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_views "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_columns "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_tables WHERE schema_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_schemas WHERE schema_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_update_table_identity_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table
) {
    sqlite3_stmt *statement = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    int rc = MYLITE_OK;

    if (out_table != NULL) {
        *out_table = (struct mylite_catalog_table_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_read_schema_by_id_from_sqlite(database->sqlite, schema_id, &schema);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables "
        "SET schema_id = ?1, name = ?2, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?3 "
        "WHERE table_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 3, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 4, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_table != NULL) {
        return mylite_catalog_read_table_by_id_from_sqlite(database->sqlite, table_id, out_table);
    }

    return MYLITE_OK;
}

int mylite_catalog_update_table_default_charset_collation_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const char *default_charset,
    const char *default_collation,
    struct mylite_catalog_table_descriptor *out_table
) {
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_table != NULL) {
        *out_table = (struct mylite_catalog_table_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(default_charset, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(
        default_collation,
        MYLITE_CATALOG_IDENTIFIER_CAPACITY
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables "
        "SET default_charset = ?1, default_collation = ?2, "
        "descriptor_version = descriptor_version + 1, updated_catalog_generation = ?3 "
        "WHERE table_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, default_charset);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, default_collation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 3, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 4, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_table != NULL) {
        return mylite_catalog_read_table_by_id_from_sqlite(database->sqlite, table_id, out_table);
    }

    return MYLITE_OK;
}

int mylite_catalog_update_table_comment_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const char *comment,
    struct mylite_catalog_table_descriptor *out_table
) {
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_table != NULL) {
        *out_table = (struct mylite_catalog_table_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_optional_name(comment, MYLITE_CATALOG_TABLE_COMMENT_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables "
        "SET comment = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, comment);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_table != NULL) {
        return mylite_catalog_read_table_by_id_from_sqlite(database->sqlite, table_id, out_table);
    }

    return MYLITE_OK;
}

int mylite_catalog_update_table_storage_statistics_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct mylite_catalog_table_descriptor *table,
    struct mylite_catalog_table_descriptor *out_table
) {
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_table != NULL) {
        *out_table = (struct mylite_catalog_table_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (table == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_catalog_validate_positive_id(table->table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_table_descriptor_input(
        &(const struct mylite_catalog_table_descriptor_input){
            .schema_id = table->schema_id,
            .name = table->name,
            .physical_name = table->physical_name,
            .kind = table->kind,
            .auto_increment_status = table->auto_increment_status,
            .default_charset = table->default_charset,
            .default_collation = table->default_collation,
            .comment = table->comment,
            .row_format_option = table->row_format_option,
            .key_block_size = table->key_block_size,
            .pack_keys = table->pack_keys,
            .checksum = table->checksum,
            .stats_persistent = table->stats_persistent,
            .stats_auto_recalc = table->stats_auto_recalc,
            .stats_sample_pages = table->stats_sample_pages,
            .min_rows = table->min_rows,
            .max_rows = table->max_rows,
            .avg_row_length = table->avg_row_length,
            .delay_key_write = table->delay_key_write,
            .created_time_utc_epoch = table->created_time_utc_epoch,
            .updated_time_utc_epoch = table->updated_time_utc_epoch,
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables "
        "SET row_format_option = ?1, key_block_size = ?2, pack_keys = ?3, checksum = ?4, "
        "stats_persistent = ?5, stats_auto_recalc = ?6, stats_sample_pages = ?7, "
        "min_rows = ?8, max_rows = ?9, avg_row_length = ?10, delay_key_write = ?11, "
        "descriptor_version = descriptor_version + 1, updated_catalog_generation = ?12 "
        "WHERE table_id = ?13",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_table_storage_statistics_update_row_format_bind,
            table->row_format_option
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_storage_statistics_update_key_block_size_bind,
            table->key_block_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_storage_statistics_update_pack_keys_bind,
            table->pack_keys
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_storage_statistics_update_checksum_bind,
            table->checksum
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_storage_statistics_update_stats_persistent_bind,
            table->stats_persistent
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_storage_statistics_update_stats_auto_recalc_bind,
            table->stats_auto_recalc
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_storage_statistics_update_stats_sample_pages_bind,
            table->stats_sample_pages
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_storage_statistics_update_min_rows_bind,
            table->min_rows
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_storage_statistics_update_max_rows_bind,
            table->max_rows
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_storage_statistics_update_avg_row_length_bind,
            table->avg_row_length
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_storage_statistics_update_delay_key_write_bind,
            table->delay_key_write
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(
            statement,
            catalog_table_storage_statistics_update_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_storage_statistics_update_table_id_bind,
            table->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_table != NULL) {
        return mylite_catalog_read_table_by_id_from_sqlite(
            database->sqlite,
            table->table_id,
            out_table
        );
    }

    return MYLITE_OK;
}

int mylite_catalog_update_schema_default_charset_collation_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t schema_id,
    const char *default_charset,
    const char *default_collation,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_schema != NULL) {
        *out_schema = (struct mylite_catalog_schema_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(default_charset, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(
        default_collation,
        MYLITE_CATALOG_IDENTIFIER_CAPACITY
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_schemas "
        "SET default_charset = ?1, default_collation = ?2, "
        "descriptor_version = descriptor_version + 1, updated_catalog_generation = ?3 "
        "WHERE schema_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, default_charset);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, default_collation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 3, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 4, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_schema != NULL) {
        return mylite_catalog_read_schema_by_id_from_sqlite(
            database->sqlite,
            schema_id,
            out_schema
        );
    }

    return MYLITE_OK;
}

int mylite_catalog_update_table_auto_increment_next(
    struct mylite_db *database,
    int64_t table_id,
    int64_t auto_increment_next
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (auto_increment_next <= 0) {
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables SET auto_increment_next = ?1 WHERE table_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, auto_increment_next);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        mylite_catalog_invalidate_descriptor_cache(database);
    }
    return rc;
}

int mylite_catalog_update_table_auto_increment_status(
    struct mylite_db *database,
    int64_t table_id,
    int64_t auto_increment_status
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (auto_increment_status < 0) {
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables SET auto_increment_status = ?1 WHERE table_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, auto_increment_status);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        mylite_catalog_invalidate_descriptor_cache(database);
    }
    return rc;
}

int mylite_catalog_update_table_updated_time(
    struct mylite_db *database,
    int64_t table_id,
    int64_t updated_time_utc_epoch
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (updated_time_utc_epoch < 0) {
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables SET updated_time_utc_epoch = ?1 WHERE table_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, updated_time_utc_epoch);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        mylite_catalog_invalidate_descriptor_cache(database);
    }
    return rc;
}

int mylite_catalog_create_schema(
    struct mylite_db *database,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    return mylite_catalog_create_schema_with_defaults(
        database,
        name,
        MYLITE_CATALOG_DEFAULT_TABLE_CHARSET,
        MYLITE_CATALOG_DEFAULT_TABLE_COLLATION,
        out_schema
    );
}

int mylite_catalog_create_schema_with_defaults(
    struct mylite_db *database,
    const char *name,
    const char *default_charset,
    const char *default_collation,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    struct mylite_catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_schema != NULL) {
        *out_schema = (struct mylite_catalog_schema_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(default_charset, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(
        default_collation,
        MYLITE_CATALOG_IDENTIFIER_CAPACITY
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_schemas "
        "(name, default_charset, default_collation, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, 1, ?4, ?4)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, default_charset);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 3, default_collation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 4, generation.next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
        return rc;
    }

    if (out_schema != NULL) {
        return mylite_catalog_read_schema_by_name_from_sqlite(database->sqlite, name, out_schema);
    }

    return MYLITE_OK;
}

int mylite_catalog_delete_schema(struct mylite_db *database, int64_t schema_id) {
    struct mylite_catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_delete_foreign_keys_for_schema_from_sqlite(database->sqlite, schema_id);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_index_columns "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_views "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_columns "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_tables WHERE schema_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_schemas WHERE schema_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
        return rc;
    }

    return MYLITE_OK;
}

int mylite_catalog_create_table(
    struct mylite_db *database,
    int64_t schema_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_table_kind kind,
    const char *default_charset,
    const char *default_collation,
    const char *comment,
    int64_t created_time_utc_epoch,
    int64_t updated_time_utc_epoch,
    struct mylite_catalog_table_descriptor *out_table
) {
    struct mylite_catalog_generation_change generation = {0};
    struct mylite_catalog_schema_descriptor schema = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_table != NULL) {
        *out_table = (struct mylite_catalog_table_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_table_descriptor_input(
        &(const struct mylite_catalog_table_descriptor_input){
            .schema_id = schema_id,
            .name = name,
            .physical_name = physical_name,
            .kind = kind,
            .default_charset = default_charset,
            .default_collation = default_collation,
            .comment = comment,
            .created_time_utc_epoch = created_time_utc_epoch,
            .updated_time_utc_epoch = updated_time_utc_epoch,
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_read_schema_by_id_from_sqlite(database->sqlite, schema_id, &schema);
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_tables "
        "(schema_id, name, kind, physical_name, auto_increment_next, default_charset, "
        "default_collation, comment, fulltext_doc_id_initialized, created_time_utc_epoch, "
        "updated_time_utc_epoch, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, 0, ?9, ?10, 1, ?11, ?11)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_table_insert_schema_id_bind, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, catalog_table_insert_name_bind, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_table_insert_kind_bind, (int64_t)kind);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_table_insert_physical_name_bind,
            physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_table_insert_auto_increment_next_bind, 1);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_table_insert_default_charset_bind,
            default_charset
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_table_insert_default_collation_bind,
            default_collation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, catalog_table_insert_comment_bind, comment);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_created_time_bind,
            created_time_utc_epoch
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_updated_time_bind,
            updated_time_utc_epoch
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(
            statement,
            catalog_table_insert_generation_bind,
            generation.next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
        return rc;
    }

    if (out_table != NULL) {
        return mylite_catalog_read_table_by_name_from_sqlite(
            database->sqlite,
            schema_id,
            name,
            out_table
        );
    }

    return MYLITE_OK;
}

int mylite_catalog_validate_table_descriptor_input(
    const struct mylite_catalog_table_descriptor_input *input
) {
    int rc = input == NULL ? MYLITE_MISUSE : mylite_catalog_validate_positive_id(input->schema_id);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_logical_object_name(
            input->name,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            input->physical_name,
            MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_table_kind(input->kind);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            input->default_charset,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            input->default_collation,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_optional_name(
            input->comment,
            MYLITE_CATALOG_TABLE_COMMENT_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_optional_name(
            input->row_format_option,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc == MYLITE_OK && input->row_format_option != NULL &&
        input->row_format_option[0] != '\0' &&
        !text_equals_ascii_case_insensitive(input->row_format_option, "DYNAMIC") &&
        !text_equals_ascii_case_insensitive(input->row_format_option, "COMPACT") &&
        !text_equals_ascii_case_insensitive(input->row_format_option, "REDUNDANT") &&
        !text_equals_ascii_case_insensitive(input->row_format_option, "COMPRESSED")) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && input->key_block_size != 0 && input->key_block_size != 1 &&
        input->key_block_size != 2 && input->key_block_size != 4 &&
        input->key_block_size != catalog_table_key_block_size_eight &&
        input->key_block_size != catalog_table_key_block_size_sixteen) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && input->auto_increment_status < 0) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK &&
        (input->pack_keys < -1 || input->pack_keys > 1 || input->checksum < 0 ||
         input->checksum > 1 || input->stats_persistent < -1 || input->stats_persistent > 1 ||
         input->stats_auto_recalc < -1 || input->stats_auto_recalc > 1 ||
         input->stats_sample_pages < 0 ||
         input->stats_sample_pages > catalog_table_stats_sample_pages_max ||
         input->delay_key_write < -1 || input->delay_key_write > 1 || input->min_rows < 0 ||
         input->max_rows < 0 || input->avg_row_length < 0)) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK &&
        (input->created_time_utc_epoch < 0 || input->updated_time_utc_epoch < 0)) {
        rc = MYLITE_ERROR;
    }

    return rc;
}

int mylite_catalog_update_table_name(
    struct mylite_db *database,
    int64_t table_id,
    const char *name
) {
    struct mylite_catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables "
        "SET name = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 2, generation.next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
        return rc;
    }

    return MYLITE_OK;
}

int mylite_catalog_delete_table(struct mylite_db *database, int64_t table_id) {
    struct mylite_catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_delete_foreign_keys_for_related_table_from_sqlite(
        database->sqlite,
        table_id
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_delete_check_constraints_for_table_from_sqlite(
            database->sqlite,
            table_id
        );
    }

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_views WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_index_columns WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_columns WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_tables WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
        return rc;
    }

    return MYLITE_OK;
}

int mylite_catalog_create_column(
    struct mylite_db *database,
    int64_t table_id,
    int64_t ordinal_position,
    const char *name,
    const char *logical_type,
    const char *physical_type,
    bool is_nullable,
    enum mylite_catalog_column_default_kind default_kind,
    int64_t default_integer,
    const char *default_text,
    bool on_update_current_timestamp,
    const char *character_set_name,
    const char *collation_name,
    const char *comment,
    bool is_generated,
    enum mylite_catalog_generated_column_kind generated_kind,
    const char *generation_expression,
    const char *sqlite_generation_expression,
    struct mylite_catalog_column_descriptor *out_column
) {
    const struct mylite_catalog_column_values values = {
        .name = name,
        .logical_type = logical_type,
        .physical_type = physical_type,
        .is_nullable = is_nullable,
        .is_visible = true,
        .is_auto_increment = false,
        .default_kind = default_kind,
        .default_integer = default_integer,
        .default_text = default_text,
        .on_update_current_timestamp = on_update_current_timestamp,
        .character_set_name = character_set_name,
        .collation_name = collation_name,
        .comment = comment == NULL ? "" : comment,
        .is_generated = is_generated,
        .generated_kind = generated_kind,
        .generation_expression = generation_expression == NULL ? "" : generation_expression,
        .sqlite_generation_expression =
            sqlite_generation_expression == NULL ? "" : sqlite_generation_expression,
    };
    struct mylite_catalog_generation_change generation = {0};
    struct mylite_catalog_table_descriptor table = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_column != NULL) {
        *out_column = (struct mylite_catalog_column_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_ordinal(ordinal_position);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_column_values(&values, false);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_read_table_by_id_from_sqlite(database->sqlite, table_id, &table);
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_columns "
        "(table_id, ordinal_position, name, logical_type, physical_type, is_nullable, "
        "is_visible, is_auto_increment, default_kind, default_integer, default_text, "
        "on_update_current_timestamp, character_set_name, collation_name, comment, "
        "is_generated, generated_kind, generation_expression, sqlite_generation_expression, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, "
        "?16, ?17, ?18, ?19, 1, ?20, ?20)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_column_insert_values(
            statement,
            table_id,
            ordinal_position,
            &values,
            generation.next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
        return rc;
    }

    if (out_column != NULL) {
        return mylite_catalog_read_column_by_name_from_sqlite(
            database->sqlite,
            table_id,
            name,
            out_column
        );
    }

    return MYLITE_OK;
}

int mylite_catalog_delete_column(struct mylite_db *database, int64_t column_id) {
    struct mylite_catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_columns WHERE column_id = ?1",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
        return rc;
    }

    return MYLITE_OK;
}

bool mylite_catalog_name_is_reserved(const char *name) {
    static const char prefix[] = "_mylite_";

    if (name == NULL) {
        return false;
    }

    return text_has_ascii_case_insensitive_prefix(name, prefix) != 0;
}

static int text_has_ascii_case_insensitive_prefix(const char *text, const char *prefix) {
    size_t index = 0U;

    while (prefix[index] != '\0') {
        if (text[index] == '\0' ||
            ascii_lower((unsigned char)text[index]) != ascii_lower((unsigned char)prefix[index])) {
            return 0;
        }
        ++index;
    }

    return 1;
}

static int text_equals_ascii_case_insensitive(const char *left, const char *right) {
    size_t index = 0U;

    if (left == NULL || right == NULL) {
        return 0;
    }
    while (left[index] != '\0' && right[index] != '\0') {
        if (ascii_lower((unsigned char)left[index]) != ascii_lower((unsigned char)right[index])) {
            return 0;
        }
        ++index;
    }
    return left[index] == '\0' && right[index] == '\0';
}

static char ascii_lower(unsigned char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (char)(byte + ('a' - 'A'));
    }
    return (char)byte;
}
