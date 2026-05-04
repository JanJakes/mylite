#include "mylite_table_ddl.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_span.h"
#include "mylite_table_ddl_alter.h"
#include "mylite_transactions.h"
#include "sqlite3.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int assign_generated_index_name(mylite_db *database, struct mylite_create_table_plan *plan,
                                       size_t index);
static bool create_table_index_name_exists(const struct mylite_create_table_plan *plan,
                                           const char *name, size_t before_index);
static int validate_create_index_plan(mylite_db *database, const char *selected_schema,
                                      struct mylite_index_ddl_plan *plan,
                                      struct mylite_alter_table_model *model);
static int validate_drop_index_plan(mylite_db *database, const char *selected_schema,
                                    struct mylite_index_ddl_plan *plan,
                                    struct mylite_alter_table_model *model);
static int resolve_index_ddl_schema(mylite_db *database, const char *selected_schema,
                                    struct mylite_index_ddl_plan *plan);
static int validate_index_ddl_target(mylite_db *database, const struct mylite_index_ddl_plan *plan);
static size_t alter_table_model_index(const struct mylite_alter_table_model *model,
                                      const char *index_name);
static int set_duplicate_key_name_error(mylite_db *database, const char *index_name);
static int set_drop_index_missing_error(mylite_db *database, const char *index_name);
static int validate_create_index_columns(mylite_db *database,
                                         const struct mylite_alter_table_model *model,
                                         const struct mylite_create_table_index *index);
static int validate_create_index_supported_features(mylite_db *database,
                                                    const struct mylite_index_ddl_plan *plan);
static int validate_create_unique_index_values(mylite_db *database,
                                               const struct mylite_alter_table_model *model,
                                               const struct mylite_create_table_index *index);
static char *build_create_unique_index_duplicate_sql(mylite_db *database,
                                                     const struct mylite_alter_table_model *model,
                                                     const struct mylite_create_table_index *index);
static const char *alter_table_column_physical_name(const struct mylite_alter_table_column *column);
static int apply_create_index_to_model(mylite_db *database, struct mylite_index_ddl_plan *plan,
                                       struct mylite_alter_table_model *model,
                                       const struct mylite_alter_table_index **out_index);
static int apply_drop_index_to_model(mylite_db *database, struct mylite_index_ddl_plan *plan,
                                     struct mylite_alter_table_model *model);
static int create_index_transaction(mylite_db *database,
                                    const struct mylite_alter_table_model *model,
                                    const struct mylite_alter_table_index *index);
static int insert_standalone_index_catalog_rows(mylite_db *database,
                                                const struct mylite_alter_table_model *model,
                                                const struct mylite_alter_table_index *index);
static int insert_standalone_index_catalog_part(mylite_db *database, sqlite3_stmt *insert,
                                                const struct mylite_alter_table_model *model,
                                                const struct mylite_alter_table_index *index,
                                                const struct mylite_alter_table_index_part *part,
                                                size_t part_index);
static int drop_index_transaction(mylite_db *database, const struct mylite_index_ddl_plan *plan);
static int delete_index_catalog_rows(mylite_db *database, const struct mylite_index_ddl_plan *plan);
static int append_create_index_warnings(mylite_db *database,
                                        const struct mylite_alter_table_model *model,
                                        const struct mylite_create_table_index *index);
static int append_index_hash_warning(mylite_db *database);
static int append_index_duplicate_warning(mylite_db *database, const char *index_name);
static int maybe_append_duplicate_index_warning(mylite_db *database,
                                                const struct mylite_alter_table_model *model,
                                                const struct mylite_create_table_index *index);
static bool
alter_table_index_matches_create_index(const struct mylite_alter_table_index *table_index,
                                       const struct mylite_create_table_index *create_index);
static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_table_ddl_assign_generated_index_names(mylite_db *database,
                                                  struct mylite_create_table_plan *plan)
{
    if (database == NULL || plan == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; index < plan->index_count; ++index) {
        int status = assign_generated_index_name(database, plan, index);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int assign_generated_index_name(mylite_db *database, struct mylite_create_table_plan *plan,
                                       size_t index)
{
    struct mylite_create_table_index *table_index = &plan->indexes[index];
    const char *base = NULL;
    unsigned int suffix = 1U;

    if (table_index->name != NULL) {
        return MYLITE_OK;
    }
    if (table_index->part_count == 0U || table_index->parts[0].column_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "Index has no key parts");
        return MYLITE_EXEC_ERROR;
    }

    base = table_index->parts[0].column_name;
    for (;;) {
        char *candidate = mylite_table_ddl_generated_index_name_candidate(base, suffix);

        if (candidate == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        if (!create_table_index_name_exists(plan, candidate, index)) {
            table_index->name = candidate;
            return MYLITE_OK;
        }
        free(candidate);
        ++suffix;
    }
}

char *mylite_table_ddl_generated_index_name_candidate(const char *base, unsigned int suffix)
{
    enum { suffix_buffer_size = 32 };
    char suffix_buffer[suffix_buffer_size];
    size_t candidate_length = strlen(base);
    char *candidate = NULL;

    suffix_buffer[0] = '\0';
    if (suffix > 1U) {
        int written = snprintf(suffix_buffer, sizeof(suffix_buffer), "_%u", suffix);

        if (written < 0) {
            return NULL;
        }
        candidate_length += (size_t)written;
    }

    candidate = malloc(candidate_length + 1U);
    if (candidate == NULL) {
        return NULL;
    }
    (void)snprintf(candidate, candidate_length + 1U, "%s%s", base, suffix_buffer);
    return candidate;
}

static bool create_table_index_name_exists(const struct mylite_create_table_plan *plan,
                                           const char *name, size_t before_index)
{
    for (size_t index = 0U; index < before_index; ++index) {
        if (plan->indexes[index].name != NULL &&
            mylite_ascii_case_equal(plan->indexes[index].name, name)) {
            return true;
        }
    }
    return false;
}

int mylite_table_ddl_execute_create_index_statement(mylite_db *database,
                                                    const char *selected_schema,
                                                    struct mylite_index_ddl_plan *plan)
{
    const struct mylite_alter_table_index *created_index = NULL;
    struct mylite_alter_table_model model = {0};
    int status = MYLITE_OK;

    if (database == NULL || plan == NULL) {
        return MYLITE_MISUSE;
    }

    status = validate_create_index_plan(database, selected_schema, plan, &model);
    if (status == MYLITE_OK) {
        status = validate_create_index_columns(database, &model, &plan->index);
    }
    if (status == MYLITE_OK) {
        status = validate_create_index_supported_features(database, plan);
    }
    if (status == MYLITE_OK && plan->index_class == MYLITE_SQL_AST_INDEX_CLASS_UNIQUE) {
        status = validate_create_unique_index_values(database, &model, &plan->index);
    }
    if (status == MYLITE_OK) {
        status = append_create_index_warnings(database, &model, &plan->index);
    }
    if (status == MYLITE_OK) {
        status = apply_create_index_to_model(database, plan, &model, &created_index);
    }
    if (status == MYLITE_OK) {
        status = create_index_transaction(database, &model, created_index);
    }

    mylite_table_ddl_alter_table_model_deinit(&model);
    return status;
}

int mylite_table_ddl_execute_drop_index_statement(mylite_db *database, const char *selected_schema,
                                                  struct mylite_index_ddl_plan *plan)
{
    struct mylite_alter_table_model model = {0};
    int status = MYLITE_OK;

    if (database == NULL || plan == NULL) {
        return MYLITE_MISUSE;
    }

    status = validate_drop_index_plan(database, selected_schema, plan, &model);
    if (status == MYLITE_OK) {
        status = apply_drop_index_to_model(database, plan, &model);
    }
    if (status == MYLITE_OK) {
        status = drop_index_transaction(database, plan);
    }

    mylite_table_ddl_alter_table_model_deinit(&model);
    return status;
}

static int validate_create_index_plan(mylite_db *database, const char *selected_schema,
                                      struct mylite_index_ddl_plan *plan,
                                      struct mylite_alter_table_model *model)
{
    int status = resolve_index_ddl_schema(database, selected_schema, plan);

    if (status != MYLITE_OK) {
        return status;
    }
    status = validate_index_ddl_target(database, plan);
    if (status != MYLITE_OK) {
        return status;
    }
    status = mylite_table_ddl_load_alter_table_model(database, plan->schema_name, plan->table_name,
                                                     model);
    if (status != MYLITE_OK) {
        return status;
    }
    if (alter_table_model_index(model, plan->index.name) < model->index_count) {
        return set_duplicate_key_name_error(database, plan->index.name);
    }
    return MYLITE_OK;
}

static int validate_drop_index_plan(mylite_db *database, const char *selected_schema,
                                    struct mylite_index_ddl_plan *plan,
                                    struct mylite_alter_table_model *model)
{
    char *canonical_name = NULL;
    size_t index = 0U;
    int status = resolve_index_ddl_schema(database, selected_schema, plan);

    if (status != MYLITE_OK) {
        return status;
    }
    status = validate_index_ddl_target(database, plan);
    if (status != MYLITE_OK) {
        return status;
    }
    status = mylite_table_ddl_load_alter_table_model(database, plan->schema_name, plan->table_name,
                                                     model);
    if (status != MYLITE_OK) {
        return status;
    }

    index = alter_table_model_index(model, plan->index_name);
    if (index == model->index_count) {
        return set_drop_index_missing_error(database, plan->index_name);
    }

    canonical_name = mylite_copy_nonempty_cstring(model->indexes[index].name);
    if (canonical_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    free(plan->index_name);
    plan->index_name = canonical_name;
    return MYLITE_OK;
}

static int resolve_index_ddl_schema(mylite_db *database, const char *selected_schema,
                                    struct mylite_index_ddl_plan *plan)
{
    if (plan->schema_name != NULL) {
        return MYLITE_OK;
    }
    if (selected_schema == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "No database selected");
        return MYLITE_EXEC_ERROR;
    }

    plan->schema_name = mylite_copy_span_text(selected_schema, strlen(selected_schema));
    if (plan->schema_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int validate_index_ddl_target(mylite_db *database, const struct mylite_index_ddl_plan *plan)
{
    struct mylite_schema_presence presence = {false};
    bool exists = false;
    int status = mylite_catalog_schema_exists(database, plan->schema_name, &presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(database, "Unknown database '",
                                                         plan->schema_name, "'");
        return MYLITE_EXEC_ERROR;
    }
    if (presence.is_system) {
        (void)mylite_diagnostics_set_error_message_parts(database, "Access to system schema '",
                                                         plan->schema_name, "' is rejected.");
        return MYLITE_EXEC_ERROR;
    }

    status = mylite_catalog_table_exists(database, plan->schema_name, plan->table_name, &exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        return mylite_diagnostics_set_table_doesnt_exist_error(database, plan->schema_name,
                                                               plan->table_name);
    }
    return MYLITE_OK;
}

static size_t alter_table_model_index(const struct mylite_alter_table_model *model,
                                      const char *index_name)
{
    for (size_t index = 0U; index < model->index_count; ++index) {
        if (mylite_ascii_case_equal(model->indexes[index].name, index_name)) {
            return index;
        }
    }
    return model->index_count;
}

static int set_duplicate_key_name_error(mylite_db *database, const char *index_name)
{
    (void)mylite_diagnostics_set_error_message_parts(database, "Duplicate key name '", index_name,
                                                     "'");
    return MYLITE_EXEC_ERROR;
}

static int set_drop_index_missing_error(mylite_db *database, const char *index_name)
{
    (void)mylite_diagnostics_set_error_message_parts(database, "Can't DROP '", index_name,
                                                     "'; check that column/key exists");
    return MYLITE_EXEC_ERROR;
}

static int validate_create_index_columns(mylite_db *database,
                                         const struct mylite_alter_table_model *model,
                                         const struct mylite_create_table_index *index)
{
    for (size_t part = 0U; part < index->part_count; ++part) {
        const char *column_name = index->parts[part].column_name;

        if (mylite_table_ddl_alter_table_column_index(model, column_name) == model->column_count) {
            (void)mylite_diagnostics_set_error_message_parts(database, "Key column '", column_name,
                                                             "' doesn't exist in table");
            return MYLITE_EXEC_ERROR;
        }
    }
    return MYLITE_OK;
}

static int validate_create_index_supported_features(mylite_db *database,
                                                    const struct mylite_index_ddl_plan *plan)
{
    if (plan->index_class == MYLITE_SQL_AST_INDEX_CLASS_FULLTEXT ||
        plan->index_class == MYLITE_SQL_AST_INDEX_CLASS_SPATIAL) {
        (void)mylite_diagnostics_set_error_message(database, "Unsupported standalone index class");
        return MYLITE_UNSUPPORTED;
    }
    if (plan->index.has_with_parser) {
        (void)mylite_diagnostics_set_error_message(
            database, "WITH PARSER is only supported for FULLTEXT indexes");
        return MYLITE_EXEC_ERROR;
    }
    return MYLITE_OK;
}

static int validate_create_unique_index_values(mylite_db *database,
                                               const struct mylite_alter_table_model *model,
                                               const struct mylite_create_table_index *index)
{
    char *sql = build_create_unique_index_duplicate_sql(database, model, index);
    sqlite3_stmt *select = NULL;
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    rc = sqlite3_step(select);
    sqlite3_finalize(select);
    if (rc == SQLITE_ROW) {
        (void)mylite_diagnostics_set_error_message_parts(database, "Duplicate entry for key '",
                                                         index->name, "'");
        return MYLITE_EXEC_ERROR;
    }
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static char *build_create_unique_index_duplicate_sql(mylite_db *database,
                                                     const struct mylite_alter_table_model *model,
                                                     const struct mylite_create_table_index *index)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_append(sql, "SELECT 1 FROM (SELECT ", (int)strlen("SELECT 1 FROM (SELECT "));
    for (size_t part = 0U; part < index->part_count; ++part) {
        const struct mylite_create_table_key_part *key_part = &index->parts[part];
        size_t column_index =
            mylite_table_ddl_alter_table_column_index(model, key_part->column_name);
        const char *column_name = alter_table_column_physical_name(&model->columns[column_index]);

        if (part != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        if (key_part->has_prefix_length) {
            sqlite3_str_appendf(sql, "substr(\"%w\",1,%llu)", column_name,
                                (unsigned long long)key_part->prefix_length);
        } else {
            sqlite3_str_appendf(sql, "\"%w\"", column_name);
        }
    }
    sqlite3_str_appendf(sql, " FROM \"%w\" WHERE ", model->physical_name);
    for (size_t part = 0U; part < index->part_count; ++part) {
        size_t column_index =
            mylite_table_ddl_alter_table_column_index(model, index->parts[part].column_name);
        const char *column_name = alter_table_column_physical_name(&model->columns[column_index]);

        if (part != 0U) {
            sqlite3_str_append(sql, " AND ", (int)strlen(" AND "));
        }
        sqlite3_str_appendf(sql, "\"%w\" IS NOT NULL", column_name);
    }
    sqlite3_str_append(sql, " GROUP BY ", (int)strlen(" GROUP BY "));
    for (size_t part = 0U; part < index->part_count; ++part) {
        if (part != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "%d", (int)(part + 1U));
    }
    sqlite3_str_append(sql, " HAVING COUNT(*) > 1) LIMIT 1",
                       (int)strlen(" HAVING COUNT(*) > 1) LIMIT 1"));
    return sqlite3_str_finish(sql);
}

static const char *alter_table_column_physical_name(const struct mylite_alter_table_column *column)
{
    if (column->source_name != NULL) {
        return column->source_name;
    }
    return column->name;
}

static int apply_create_index_to_model(mylite_db *database, struct mylite_index_ddl_plan *plan,
                                       struct mylite_alter_table_model *model,
                                       const struct mylite_alter_table_index **out_index)
{
    enum mylite_alter_table_action_kind kind = MYLITE_ALTER_TABLE_ACTION_ADD_SECONDARY_INDEX;
    size_t index = 0U;

    if (plan->index.is_unique) {
        kind = MYLITE_ALTER_TABLE_ACTION_ADD_UNIQUE_INDEX;
    }

    const struct mylite_alter_table_action action = {
        .kind = kind,
        .index = plan->index,
    };
    int status = mylite_table_ddl_apply_alter_table_index_action(database, &action, model, NULL);

    *out_index = NULL;
    if (status != MYLITE_OK) {
        return status;
    }

    index = alter_table_model_index(model, plan->index.name);
    if (index == model->index_count) {
        return MYLITE_MISUSE;
    }
    *out_index = &model->indexes[index];
    return MYLITE_OK;
}

static int apply_drop_index_to_model(mylite_db *database, struct mylite_index_ddl_plan *plan,
                                     struct mylite_alter_table_model *model)
{
    const struct mylite_alter_table_action action = {
        .kind = MYLITE_ALTER_TABLE_ACTION_DROP_INDEX,
        .old_name = plan->index_name,
    };

    return mylite_table_ddl_apply_alter_table_index_action(database, &action, model, NULL);
}

static int create_index_transaction(mylite_db *database,
                                    const struct mylite_alter_table_model *model,
                                    const struct mylite_alter_table_index *index)
{
    int status = mylite_transaction_begin_storage(database);

    if (status != MYLITE_OK) {
        return status;
    }

    status = insert_standalone_index_catalog_rows(database, model, index);
    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_storage(database);
        if (status == MYLITE_OK) {
            return MYLITE_OK;
        }
    }

    mylite_transaction_rollback_storage(database);
    return status;
}

static int insert_standalone_index_catalog_rows(mylite_db *database,
                                                const struct mylite_alter_table_model *model,
                                                const struct mylite_alter_table_index *index)
{
    sqlite3_stmt *insert = NULL;
    static const char sql[] =
        "INSERT INTO __mylite_index_catalog("
        "table_catalog, table_schema, table_name, non_unique, index_schema, index_name, "
        "seq_in_index, column_name, collation, cardinality, sub_part, packed, nullable, "
        "index_type, comment, index_comment, is_visible, expression)"
        " VALUES('def', ?, ?, ?, ?, ?, ?, ?, ?, NULL, ?, NULL, ?, ?, '', ?, ?, NULL)";
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &insert, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    for (size_t part = 0U; part < index->part_count; ++part) {
        int status = insert_standalone_index_catalog_part(database, insert, model, index,
                                                          &index->parts[part], part);

        if (status != MYLITE_OK) {
            sqlite3_finalize(insert);
            return status;
        }
    }

    sqlite3_finalize(insert);
    return MYLITE_OK;
}

static int insert_standalone_index_catalog_part(mylite_db *database, sqlite3_stmt *insert,
                                                const struct mylite_alter_table_model *model,
                                                const struct mylite_alter_table_index *index,
                                                const struct mylite_alter_table_index_part *part,
                                                size_t part_index)
{
    enum {
        bind_table_schema = 1,
        bind_table_name = 2,
        bind_non_unique = 3,
        bind_index_schema = 4,
        bind_index_name = 5,
        bind_seq_in_index = 6,
        bind_column_name = 7,
        bind_collation = 8,
        bind_sub_part = 9,
        bind_nullable = 10,
        bind_index_type = 11,
        bind_index_comment = 12,
        bind_is_visible = 13,
    };
    int rc = SQLITE_OK;

    sqlite3_reset(insert);
    sqlite3_clear_bindings(insert);
    sqlite3_bind_text(insert, bind_table_schema, model->schema_name, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_table_name, model->table_name, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_int(insert, bind_non_unique, index->non_unique);
    sqlite3_bind_text(insert, bind_index_schema, index->index_schema, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_index_name, index->name, -1, sqlite_transient_destructor());
    sqlite3_bind_int64(insert, bind_seq_in_index, (sqlite3_int64)part_index + 1);
    sqlite3_bind_text(insert, bind_column_name, part->column_name, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_collation, part->collation, -1, sqlite_transient_destructor());
    if (part->has_sub_part) {
        sqlite3_bind_int64(insert, bind_sub_part, (sqlite3_int64)part->sub_part);
    } else {
        sqlite3_bind_null(insert, bind_sub_part);
    }
    sqlite3_bind_text(insert, bind_nullable, part->nullable, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_index_type, index->index_type, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_index_comment, index->index_comment, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_is_visible, index->is_visible, -1,
                      sqlite_transient_destructor());

    rc = sqlite3_step(insert);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int drop_index_transaction(mylite_db *database, const struct mylite_index_ddl_plan *plan)
{
    int status = mylite_transaction_begin_storage(database);

    if (status != MYLITE_OK) {
        return status;
    }

    status = delete_index_catalog_rows(database, plan);
    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_storage(database);
        if (status == MYLITE_OK) {
            return MYLITE_OK;
        }
    }

    mylite_transaction_rollback_storage(database);
    return status;
}

static int delete_index_catalog_rows(mylite_db *database, const struct mylite_index_ddl_plan *plan)
{
    sqlite3_stmt *delete_stmt = NULL;
    static const char sql[] = "DELETE FROM __mylite_index_catalog "
                              "WHERE table_schema = ? AND table_name = ? AND index_name = ?";
    int rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &delete_stmt,
                                NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(delete_stmt, 1, plan->schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(delete_stmt, 2, plan->table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(delete_stmt, 3, plan->index_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(delete_stmt);
    sqlite3_finalize(delete_stmt);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int append_create_index_warnings(mylite_db *database,
                                        const struct mylite_alter_table_model *model,
                                        const struct mylite_create_table_index *index)
{
    int status = MYLITE_OK;

    if (index->algorithm == MYLITE_SQL_AST_INDEX_ALGORITHM_HASH) {
        status = append_index_hash_warning(database);
    }
    if (status == MYLITE_OK) {
        status = maybe_append_duplicate_index_warning(database, model, index);
    }
    return status;
}

static int append_index_hash_warning(mylite_db *database)
{
    return mylite_diagnostics_append_warning(
        database, MYLITE_MYSQL_ER_WARN_USING_OTHER_HANDLER,
        "This storage engine does not support HASH indexes; using BTREE instead");
}

static int append_index_duplicate_warning(mylite_db *database, const char *index_name)
{
    char *message = sqlite3_mprintf(
        "Duplicate index '%q' defined on the table. This is deprecated and will be disallowed in "
        "a future release.",
        index_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_warning(database, MYLITE_MYSQL_ER_DUP_INDEX, message);
    sqlite3_free(message);
    return status;
}

static int maybe_append_duplicate_index_warning(mylite_db *database,
                                                const struct mylite_alter_table_model *model,
                                                const struct mylite_create_table_index *index)
{
    for (size_t table_index = 0U; table_index < model->index_count; ++table_index) {
        if (alter_table_index_matches_create_index(&model->indexes[table_index], index)) {
            return append_index_duplicate_warning(database, index->name);
        }
    }
    return MYLITE_OK;
}

static bool
alter_table_index_matches_create_index(const struct mylite_alter_table_index *table_index,
                                       const struct mylite_create_table_index *create_index)
{
    int expected_non_unique = 1;

    if (create_index->is_unique) {
        expected_non_unique = 0;
    }
    if (table_index->non_unique != expected_non_unique ||
        table_index->part_count != create_index->part_count ||
        !mylite_ascii_case_equal(table_index->index_type, "BTREE")) {
        return false;
    }

    for (size_t part = 0U; part < create_index->part_count; ++part) {
        const struct mylite_alter_table_index_part *table_part = &table_index->parts[part];
        const struct mylite_create_table_key_part *create_part = &create_index->parts[part];

        if (!mylite_ascii_case_equal(table_part->column_name, create_part->column_name) ||
            strcmp(table_part->collation == NULL ? "" : table_part->collation,
                   mylite_table_ddl_index_collation_for_order(create_part->order)) != 0 ||
            table_part->has_sub_part != create_part->has_prefix_length ||
            (table_part->has_sub_part &&
             (uint64_t)table_part->sub_part != create_part->prefix_length)) {
            return false;
        }
    }
    return true;
}

static sqlite3_destructor_type sqlite_transient_destructor(void)
{
    return (sqlite3_destructor_type)SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
