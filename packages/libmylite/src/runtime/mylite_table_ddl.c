#include "mylite_table_ddl.h"

#include "mylite_catalog.h"
#include "mylite_charset.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_transactions.h"
#include "sqlite3.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int validate_create_table_plan(mylite_db *database, const char *schema_name,
                                      struct mylite_create_table_plan *plan, bool if_not_exists,
                                      struct mylite_schema_default *schema_default,
                                      bool *out_skip_create);
static int create_table_transaction(mylite_db *database, const char *schema_name,
                                    const struct mylite_schema_default *schema_default,
                                    const struct mylite_create_table_plan *plan);
static int create_physical_table(mylite_db *database, const char *schema_name,
                                 const struct mylite_schema_default *schema_default,
                                 const struct mylite_create_table_plan *plan);
static int insert_create_table_catalog_rows(mylite_db *database, const char *schema_name,
                                            const struct mylite_schema_default *schema_default,
                                            const struct mylite_create_table_plan *plan);
static int validate_drop_table_plan(mylite_db *database, const char *selected_schema,
                                    struct mylite_drop_table_plan *plan, bool if_exists);
static int validate_drop_table_temporary_target(mylite_db *database,
                                                const struct mylite_drop_table_target *target);
static int validate_drop_table_target(mylite_db *database, struct mylite_drop_table_target *target,
                                      bool if_exists);
static bool drop_table_target_is_duplicate(const struct mylite_drop_table_plan *plan,
                                           size_t target_index);
static int drop_table_transaction(mylite_db *database, const struct mylite_drop_table_plan *plan);
static int drop_physical_table(mylite_db *database, const struct mylite_drop_table_target *target);
static int set_unknown_table_error(mylite_db *database, const char *schema_name,
                                   const char *table_name);
static int append_unknown_table_note(mylite_db *database, const char *schema_name,
                                     const char *table_name);
static int validate_rename_table_plan(mylite_db *database, const char *selected_schema,
                                      struct mylite_rename_table_plan *plan);
static int resolve_rename_table_names(mylite_db *database, const char *selected_schema,
                                      struct mylite_rename_table_plan *plan);
static int validate_rename_table_target_schemas(mylite_db *database,
                                                const struct mylite_rename_table_target *target);
static int validate_rename_table_target(mylite_db *database,
                                        const struct mylite_rename_table_plan *plan,
                                        size_t target_index);
static int simulated_rename_table_exists_before_target(mylite_db *database,
                                                       const struct mylite_rename_table_plan *plan,
                                                       const char *schema_name,
                                                       const char *table_name, size_t target_index,
                                                       bool *out_exists);
static bool rename_table_target_source_matches(const struct mylite_rename_table_target *target,
                                               const char *schema_name, const char *table_name);
static bool rename_table_target_destination_matches(const struct mylite_rename_table_target *target,
                                                    const char *schema_name,
                                                    const char *table_name);
static int rename_table_transaction(mylite_db *database,
                                    const struct mylite_rename_table_plan *plan);
static int rename_table_target(mylite_db *database,
                               const struct mylite_rename_table_target *target);
static int rename_physical_table(mylite_db *database,
                                 const struct mylite_rename_table_target *target);
static int rewrite_rename_table_catalog(mylite_db *database,
                                        const struct mylite_rename_table_target *target);
static int rewrite_rename_table_catalog_row(mylite_db *database, const char *sql,
                                            const struct mylite_rename_table_target *target);
static int rewrite_rename_table_index_catalog(mylite_db *database,
                                              const struct mylite_rename_table_target *target);
static int set_rename_table_exists_error(mylite_db *database, const char *schema_name,
                                         const char *table_name);
static int validate_truncate_table_plan(mylite_db *database, const char *selected_schema,
                                        struct mylite_truncate_table_plan *plan);
static int resolve_truncate_table_name(mylite_db *database, const char *selected_schema,
                                       struct mylite_truncate_table_plan *plan);
static int validate_truncate_table_target(mylite_db *database,
                                          const struct mylite_truncate_table_plan *plan);
static int truncate_table_transaction(mylite_db *database,
                                      const struct mylite_truncate_table_plan *plan);
static int delete_truncate_table_rows(mylite_db *database,
                                      const struct mylite_truncate_table_plan *plan);
static int reset_truncate_table_auto_increment(mylite_db *database,
                                               const struct mylite_truncate_table_plan *plan);
static int load_alter_table_columns(mylite_db *database, struct mylite_alter_table_model *model);
static int load_alter_table_column_from_catalog_row(mylite_db *database, sqlite3_stmt *select,
                                                    struct mylite_alter_table_model *model);
static int load_alter_table_column_text_catalog_fields(sqlite3_stmt *select,
                                                       struct mylite_alter_table_column *column);
static void
load_alter_table_column_numeric_catalog_fields(sqlite3_stmt *select,
                                               struct mylite_alter_table_column *column);
static void load_alter_table_column_flags(struct mylite_alter_table_column *column);
static int load_alter_table_indexes(mylite_db *database, struct mylite_alter_table_model *model);
static int add_alter_table_index_part(mylite_db *database, struct mylite_alter_table_model *model,
                                      sqlite3_stmt *select);
static size_t loaded_alter_table_index_index(const struct mylite_alter_table_model *model,
                                             const char *name);
static int copy_sqlite_text_column(sqlite3_stmt *stmt, int column, char **out_text);
static int copy_sqlite_nullable_text_column(sqlite3_stmt *stmt, int column, char **out_text);
static int normalize_create_table_options(mylite_db *database, const char *schema_name,
                                          const struct mylite_schema_default *schema_default,
                                          struct mylite_create_table_options *options);
static int normalize_create_table_option_text(mylite_db *database, char **target,
                                              const char *value);
static bool validate_create_table_column_names(mylite_db *database,
                                               const struct mylite_create_table_plan *plan);
static bool validate_create_table_indexes(mylite_db *database,
                                          const struct mylite_create_table_plan *plan);
static void apply_create_table_primary_key_nullability(struct mylite_create_table_plan *plan);
static bool create_table_index_name_exists(const struct mylite_create_table_plan *plan,
                                           const char *name, size_t before_index);
static bool is_supported_engine_name(const char *name);
static int insert_table_catalog_row(mylite_db *database, const char *schema_name,
                                    const struct mylite_schema_default *schema_default,
                                    const struct mylite_create_table_plan *plan);
static int insert_column_catalog_rows(mylite_db *database, const char *schema_name,
                                      const struct mylite_schema_default *schema_default,
                                      const struct mylite_create_table_plan *plan);
static int insert_column_catalog_row(mylite_db *database, sqlite3_stmt *insert,
                                     const char *schema_name,
                                     const struct mylite_schema_default *schema_default,
                                     const struct mylite_create_table_plan *plan,
                                     const struct mylite_create_table_column *column,
                                     size_t column_index);
static int insert_index_catalog_rows(mylite_db *database, const char *schema_name,
                                     const struct mylite_create_table_plan *plan);
static int insert_index_catalog_part(mylite_db *database, sqlite3_stmt *insert,
                                     const char *schema_name,
                                     const struct mylite_create_table_plan *plan,
                                     const struct mylite_create_table_index *index,
                                     const struct mylite_create_table_key_part *part,
                                     size_t part_index);
static char *build_create_physical_table_sql(mylite_db *database, const char *physical_name,
                                             const struct mylite_schema_default *schema_default,
                                             const struct mylite_create_table_plan *plan);
static const char *
sqlite_affinity_for_descriptor(const struct mylite_column_type_descriptor *descriptor);
static const char *create_table_column_key(const struct mylite_create_table_plan *plan,
                                           const char *column_name);
static struct mylite_create_table_column_index_status
create_table_column_index_status(const struct mylite_create_table_plan *plan,
                                 const char *column_name);
static const char *create_table_column_type_name(enum mylite_sql_ast_column_type column_type);
static bool
create_table_column_uses_integer_descriptor(enum mylite_sql_ast_column_type column_type);
static bool
create_table_column_uses_string_binary_descriptor(enum mylite_sql_ast_column_type column_type);
static bool
create_table_column_uses_character_set_defaults(enum mylite_sql_ast_column_type column_type);
static bool
create_table_column_uses_numeric_descriptor(enum mylite_sql_ast_column_type column_type);
static bool
create_table_column_uses_temporal_descriptor(enum mylite_sql_ast_column_type column_type);
static sqlite3_destructor_type sqlite_transient_destructor(void);
static const struct mylite_create_table_column *
find_create_table_column(const struct mylite_create_table_plan *plan, const char *name);

int mylite_table_ddl_execute_create_table_statement(mylite_db *database,
                                                    const char *selected_schema,
                                                    struct mylite_create_table_plan *plan,
                                                    bool if_not_exists)
{
    const char *schema_name = plan->schema_name == NULL ? selected_schema : plan->schema_name;
    struct mylite_schema_default schema_default;
    bool skip_create = false;
    int status = MYLITE_OK;

    if (schema_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "No database selected");
        return MYLITE_EXEC_ERROR;
    }

    status = validate_create_table_plan(database, schema_name, plan, if_not_exists, &schema_default,
                                        &skip_create);
    if (status != MYLITE_OK) {
        return status;
    }
    if (skip_create) {
        return MYLITE_OK;
    }

    return create_table_transaction(database, schema_name, &schema_default, plan);
}

static int validate_create_table_plan(mylite_db *database, const char *schema_name,
                                      struct mylite_create_table_plan *plan, bool if_not_exists,
                                      struct mylite_schema_default *schema_default,
                                      bool *out_skip_create)
{
    struct mylite_schema_presence presence;
    bool exists = false;
    int status = mylite_catalog_schema_exists(database, schema_name, &presence);

    *out_skip_create = false;
    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(database, "Unknown database '",
                                                         schema_name, "'");
        return MYLITE_EXEC_ERROR;
    }
    if (presence.is_system) {
        (void)mylite_diagnostics_set_error_message_parts(database, "Access to system schema '",
                                                         schema_name, "' is rejected.");
        return MYLITE_EXEC_ERROR;
    }

    status = mylite_catalog_table_exists(database, schema_name, plan->table_name, &exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (exists) {
        if (if_not_exists) {
            int note_status = mylite_diagnostics_set_error_message_parts(
                database, "Table '", plan->table_name, "' already exists");

            if (note_status == MYLITE_NOMEM) {
                return MYLITE_NOMEM;
            }
            note_status = mylite_diagnostics_append_note(
                database, MYLITE_MYSQL_ER_TABLE_EXISTS_ERROR, mylite_error_message(database));
            if (note_status == MYLITE_OK) {
                *out_skip_create = true;
            }
            return note_status;
        }
        (void)mylite_diagnostics_set_error_message_parts(database, "Table '", plan->table_name,
                                                         "' already exists");
        return MYLITE_EXEC_ERROR;
    }

    status = mylite_catalog_schema_default_by_name(database, schema_name, schema_default);
    if (status != MYLITE_OK) {
        return status;
    }
    status = normalize_create_table_options(database, schema_name, schema_default, &plan->options);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!validate_create_table_column_names(database, plan)) {
        return MYLITE_EXEC_ERROR;
    }
    status = mylite_table_ddl_assign_generated_index_names(database, plan);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!validate_create_table_indexes(database, plan)) {
        return MYLITE_EXEC_ERROR;
    }
    apply_create_table_primary_key_nullability(plan);
    return MYLITE_OK;
}

static int create_table_transaction(mylite_db *database, const char *schema_name,
                                    const struct mylite_schema_default *schema_default,
                                    const struct mylite_create_table_plan *plan)
{
    int status = mylite_transaction_begin_storage(database);

    if (status != MYLITE_OK) {
        return status;
    }

    status = create_physical_table(database, schema_name, schema_default, plan);
    if (status == MYLITE_OK) {
        status = insert_create_table_catalog_rows(database, schema_name, schema_default, plan);
    }
    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_storage(database);
        if (status == MYLITE_OK) {
            return MYLITE_OK;
        }
    }

    mylite_transaction_rollback_storage(database);
    return status;
}

static int create_physical_table(mylite_db *database, const char *schema_name,
                                 const struct mylite_schema_default *schema_default,
                                 const struct mylite_create_table_plan *plan)
{
    char *physical_name = mylite_catalog_physical_table_name(schema_name, plan->table_name);
    char *sql = NULL;
    int rc = SQLITE_OK;

    if (physical_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    sql = build_create_physical_table_sql(database, physical_name, schema_default, plan);
    free(physical_name);
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    return MYLITE_OK;
}

static int insert_create_table_catalog_rows(mylite_db *database, const char *schema_name,
                                            const struct mylite_schema_default *schema_default,
                                            const struct mylite_create_table_plan *plan)
{
    int status = insert_table_catalog_row(database, schema_name, schema_default, plan);

    if (status == MYLITE_OK) {
        status = insert_column_catalog_rows(database, schema_name, schema_default, plan);
    }
    if (status == MYLITE_OK) {
        status = insert_index_catalog_rows(database, schema_name, plan);
    }
    return status;
}

static char *build_create_physical_table_sql(mylite_db *database, const char *physical_name,
                                             const struct mylite_schema_default *schema_default,
                                             const struct mylite_create_table_plan *plan)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendf(sql, "CREATE TABLE \"%w\"(", physical_name);
    for (size_t index = 0U; index < plan->column_count; ++index) {
        struct mylite_column_type_descriptor descriptor;
        int status = mylite_table_ddl_describe_create_table_column(
            &plan->columns[index], schema_default, &plan->options, &descriptor);

        if (status != MYLITE_OK) {
            sqlite3_free(sqlite3_str_finish(sql));
            return NULL;
        }
        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "\"%w\" %s", plan->columns[index].name,
                            sqlite_affinity_for_descriptor(&descriptor));
    }
    sqlite3_str_append(sql, ")", 1);
    return sqlite3_str_finish(sql);
}

int mylite_table_ddl_execute_drop_table_statement(mylite_db *database, const char *selected_schema,
                                                  struct mylite_drop_table_plan *plan,
                                                  bool if_exists)
{
    int status = validate_drop_table_plan(database, selected_schema, plan, if_exists);

    if (status != MYLITE_OK) {
        return status;
    }
    if (plan->temporary) {
        if (if_exists) {
            return append_unknown_table_note(database, plan->targets[0].schema_name,
                                             plan->targets[0].table_name);
        }
        return set_unknown_table_error(database, plan->targets[0].schema_name,
                                       plan->targets[0].table_name);
    }

    return drop_table_transaction(database, plan);
}

static int validate_drop_table_plan(mylite_db *database, const char *selected_schema,
                                    struct mylite_drop_table_plan *plan, bool if_exists)
{
    for (size_t index = 0U; index < plan->target_count; ++index) {
        struct mylite_drop_table_target *target = &plan->targets[index];

        if (target->schema_name == NULL) {
            if (selected_schema == NULL) {
                (void)mylite_diagnostics_set_error_message(database, "No database selected");
                return MYLITE_EXEC_ERROR;
            }
            target->schema_name = mylite_copy_span_text(selected_schema, strlen(selected_schema));
            if (target->schema_name == NULL) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
        }

        if (drop_table_target_is_duplicate(plan, index)) {
            (void)mylite_diagnostics_set_error_message_parts(database, "Not unique table/alias: '",
                                                             target->table_name, "'");
            return MYLITE_EXEC_ERROR;
        }
    }

    if (plan->temporary) {
        for (size_t index = 0U; index < plan->target_count; ++index) {
            int status = validate_drop_table_temporary_target(database, &plan->targets[index]);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        return MYLITE_OK;
    }

    for (size_t index = 0U; index < plan->target_count; ++index) {
        struct mylite_drop_table_target *target = &plan->targets[index];
        int status = validate_drop_table_target(database, target, if_exists);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int validate_drop_table_temporary_target(mylite_db *database,
                                                const struct mylite_drop_table_target *target)
{
    struct mylite_schema_presence presence;
    int status = mylite_catalog_schema_exists(database, target->schema_name, &presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (presence.is_system) {
        (void)mylite_diagnostics_set_error_message_parts(database, "Access to system schema '",
                                                         target->schema_name, "' is rejected.");
        return MYLITE_EXEC_ERROR;
    }
    return MYLITE_OK;
}

static int validate_drop_table_target(mylite_db *database, struct mylite_drop_table_target *target,
                                      bool if_exists)
{
    struct mylite_schema_presence presence;
    bool exists = false;
    int status = mylite_catalog_schema_exists(database, target->schema_name, &presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (presence.is_system) {
        (void)mylite_diagnostics_set_error_message_parts(database, "Access to system schema '",
                                                         target->schema_name, "' is rejected.");
        return MYLITE_EXEC_ERROR;
    }
    if (!presence.exists) {
        if (if_exists) {
            return append_unknown_table_note(database, target->schema_name, target->table_name);
        }
        return set_unknown_table_error(database, target->schema_name, target->table_name);
    }

    status =
        mylite_catalog_table_exists(database, target->schema_name, target->table_name, &exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        if (if_exists) {
            return append_unknown_table_note(database, target->schema_name, target->table_name);
        }
        return set_unknown_table_error(database, target->schema_name, target->table_name);
    }
    target->exists = exists;
    return MYLITE_OK;
}

static bool drop_table_target_is_duplicate(const struct mylite_drop_table_plan *plan,
                                           size_t target_index)
{
    const struct mylite_drop_table_target *target = &plan->targets[target_index];

    for (size_t index = 0U; index < target_index; ++index) {
        if (strcmp(plan->targets[index].schema_name, target->schema_name) == 0 &&
            strcmp(plan->targets[index].table_name, target->table_name) == 0) {
            return true;
        }
    }
    return false;
}

static int drop_table_transaction(mylite_db *database, const struct mylite_drop_table_plan *plan)
{
    int status = mylite_transaction_begin_storage(database);

    if (status != MYLITE_OK) {
        return status;
    }

    for (size_t index = 0U; index < plan->target_count && status == MYLITE_OK; ++index) {
        if (!plan->targets[index].exists) {
            continue;
        }
        status = drop_physical_table(database, &plan->targets[index]);
        if (status == MYLITE_OK) {
            status = mylite_catalog_delete_table_rows(
                database, plan->targets[index].schema_name, plan->targets[index].table_name,
                MYLITE_CATALOG_DELETE_TABLE_INDEXES | MYLITE_CATALOG_DELETE_TABLE_COLUMNS |
                    MYLITE_CATALOG_DELETE_TABLE_ROW);
        }
    }

    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_storage(database);
        if (status == MYLITE_OK) {
            return MYLITE_OK;
        }
    }

    mylite_transaction_rollback_storage(database);
    return status;
}

static int drop_physical_table(mylite_db *database, const struct mylite_drop_table_target *target)
{
    char *physical_name =
        mylite_catalog_physical_table_name(target->schema_name, target->table_name);
    char *drop_sql = NULL;
    sqlite3_str *sql = NULL;
    int rc = SQLITE_OK;

    if (physical_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    sql = sqlite3_str_new(database->sqlite);
    if (sql == NULL) {
        free(physical_name);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    sqlite3_str_appendf(sql, "DROP TABLE \"%w\"", physical_name);
    free(physical_name);

    drop_sql = sqlite3_str_finish(sql);
    if (drop_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_exec(database->sqlite, drop_sql, NULL, NULL, NULL);
    sqlite3_free(drop_sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    return MYLITE_OK;
}

static int set_unknown_table_error(mylite_db *database, const char *schema_name,
                                   const char *table_name)
{
    char *message = sqlite3_mprintf("Unknown table '%q.%q'", schema_name, table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status =
            mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_BAD_TABLE_ERROR, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int append_unknown_table_note(mylite_db *database, const char *schema_name,
                                     const char *table_name)
{
    char *message = sqlite3_mprintf("Unknown table '%q.%q'", schema_name, table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_append_note(database, MYLITE_MYSQL_ER_BAD_TABLE_ERROR, message);
    sqlite3_free(message);
    return status;
}

int mylite_table_ddl_execute_rename_table_statement(mylite_db *database,
                                                    const char *selected_schema,
                                                    struct mylite_rename_table_plan *plan)
{
    int status = validate_rename_table_plan(database, selected_schema, plan);

    if (status == MYLITE_OK) {
        status = rename_table_transaction(database, plan);
    }
    return status;
}

static int validate_rename_table_plan(mylite_db *database, const char *selected_schema,
                                      struct mylite_rename_table_plan *plan)
{
    int status = MYLITE_OK;

    if (plan->target_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    status = resolve_rename_table_names(database, selected_schema, plan);
    if (status != MYLITE_OK) {
        return status;
    }

    for (size_t index = 0U; index < plan->target_count; ++index) {
        status = validate_rename_table_target_schemas(database, &plan->targets[index]);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    for (size_t index = 0U; index < plan->target_count; ++index) {
        status = validate_rename_table_target(database, plan, index);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int resolve_rename_table_names(mylite_db *database, const char *selected_schema,
                                      struct mylite_rename_table_plan *plan)
{
    for (size_t index = 0U; index < plan->target_count; ++index) {
        struct mylite_rename_table_target *target = &plan->targets[index];

        if ((target->source_schema_name == NULL || target->target_schema_name == NULL) &&
            (selected_schema == NULL || selected_schema[0] == '\0')) {
            (void)mylite_diagnostics_set_error_message(database, "No database selected");
            return MYLITE_EXEC_ERROR;
        }
        if (target->source_schema_name == NULL) {
            target->source_schema_name = mylite_copy_nonempty_cstring(selected_schema);
            if (target->source_schema_name == NULL) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
        }
        if (target->target_schema_name == NULL) {
            target->target_schema_name = mylite_copy_nonempty_cstring(selected_schema);
            if (target->target_schema_name == NULL) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
        }
    }
    return MYLITE_OK;
}

static int validate_rename_table_target_schemas(mylite_db *database,
                                                const struct mylite_rename_table_target *target)
{
    struct mylite_schema_presence source_presence;
    struct mylite_schema_presence target_presence;
    int status =
        mylite_catalog_schema_exists(database, target->source_schema_name, &source_presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (!source_presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(database, "Unknown database '",
                                                         target->source_schema_name, "'");
        return MYLITE_EXEC_ERROR;
    }
    if (source_presence.is_system) {
        (void)mylite_diagnostics_set_error_message_parts(
            database, "Access to system schema '", target->source_schema_name, "' is rejected.");
        return MYLITE_EXEC_ERROR;
    }

    status = mylite_catalog_schema_exists(database, target->target_schema_name, &target_presence);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!target_presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(database, "Unknown database '",
                                                         target->target_schema_name, "'");
        return MYLITE_EXEC_ERROR;
    }
    if (target_presence.is_system) {
        (void)mylite_diagnostics_set_error_message_parts(
            database, "Access to system schema '", target->target_schema_name, "' is rejected.");
        return MYLITE_EXEC_ERROR;
    }
    return MYLITE_OK;
}

static int validate_rename_table_target(mylite_db *database,
                                        const struct mylite_rename_table_plan *plan,
                                        size_t target_index)
{
    const struct mylite_rename_table_target *target = &plan->targets[target_index];
    bool source_exists = false;
    bool target_exists = false;
    int status = simulated_rename_table_exists_before_target(
        database, plan, target->source_schema_name, target->source_table_name, target_index,
        &source_exists);

    if (status != MYLITE_OK) {
        return status;
    }
    if (!source_exists) {
        return mylite_diagnostics_set_table_doesnt_exist_error(database, target->source_schema_name,
                                                               target->source_table_name);
    }

    status = simulated_rename_table_exists_before_target(database, plan, target->target_schema_name,
                                                         target->target_table_name, target_index,
                                                         &target_exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (target_exists) {
        return set_rename_table_exists_error(database, target->target_schema_name,
                                             target->target_table_name);
    }
    return MYLITE_OK;
}

static int simulated_rename_table_exists_before_target(mylite_db *database,
                                                       const struct mylite_rename_table_plan *plan,
                                                       const char *schema_name,
                                                       const char *table_name, size_t target_index,
                                                       bool *out_exists)
{
    int status = mylite_catalog_table_exists(database, schema_name, table_name, out_exists);

    if (status != MYLITE_OK) {
        return status;
    }
    for (size_t index = 0U; index < target_index; ++index) {
        const struct mylite_rename_table_target *target = &plan->targets[index];

        if (rename_table_target_source_matches(target, schema_name, table_name)) {
            *out_exists = false;
        }
        if (rename_table_target_destination_matches(target, schema_name, table_name)) {
            *out_exists = true;
        }
    }
    return MYLITE_OK;
}

static bool rename_table_target_source_matches(const struct mylite_rename_table_target *target,
                                               const char *schema_name, const char *table_name)
{
    const int schema_comparison = strcmp(target->source_schema_name, schema_name);
    const int table_comparison = strcmp(target->source_table_name, table_name);

    if (schema_comparison != 0) {
        return false;
    }
    if (table_comparison != 0) {
        return false;
    }
    return true;
}

static bool rename_table_target_destination_matches(const struct mylite_rename_table_target *target,
                                                    const char *schema_name, const char *table_name)
{
    const int schema_comparison = strcmp(target->target_schema_name, schema_name);
    const int table_comparison = strcmp(target->target_table_name, table_name);

    if (schema_comparison != 0) {
        return false;
    }
    if (table_comparison != 0) {
        return false;
    }
    return true;
}

static int rename_table_transaction(mylite_db *database,
                                    const struct mylite_rename_table_plan *plan)
{
    struct mylite_statement_atomicity atomicity = {0};
    int status = mylite_transaction_begin_statement_atomicity(database, &atomicity);

    for (size_t index = 0U; status == MYLITE_OK && index < plan->target_count; ++index) {
        status = rename_table_target(database, &plan->targets[index]);
    }
    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_statement_atomicity(database, &atomicity);
        if (status == MYLITE_OK) {
            return MYLITE_OK;
        }
    }

    mylite_transaction_rollback_statement_atomicity(database, &atomicity);
    return status;
}

static int rename_table_target(mylite_db *database, const struct mylite_rename_table_target *target)
{
    int status = rename_physical_table(database, target);

    if (status == MYLITE_OK) {
        status = rewrite_rename_table_catalog(database, target);
    }
    return status;
}

static int rename_physical_table(mylite_db *database,
                                 const struct mylite_rename_table_target *target)
{
    char *source_physical_name =
        mylite_catalog_physical_table_name(target->source_schema_name, target->source_table_name);
    char *target_physical_name =
        mylite_catalog_physical_table_name(target->target_schema_name, target->target_table_name);
    char *sql = NULL;
    int rc = SQLITE_OK;

    if (source_physical_name == NULL || target_physical_name == NULL) {
        free(source_physical_name);
        free(target_physical_name);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    sql = sqlite3_mprintf("ALTER TABLE \"%w\" RENAME TO \"%w\"", source_physical_name,
                          target_physical_name);
    free(source_physical_name);
    free(target_physical_name);
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    return rc == SQLITE_OK ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int rewrite_rename_table_catalog(mylite_db *database,
                                        const struct mylite_rename_table_target *target)
{
    static const char update_tables[] =
        "UPDATE __mylite_table_catalog SET table_schema = ?, table_name = ? "
        "WHERE table_schema = ? AND table_name = ?";
    static const char update_columns[] =
        "UPDATE __mylite_column_catalog SET table_schema = ?, table_name = ? "
        "WHERE table_schema = ? AND table_name = ?";
    int status = rewrite_rename_table_catalog_row(database, update_tables, target);

    if (status == MYLITE_OK) {
        status = rewrite_rename_table_catalog_row(database, update_columns, target);
    }
    if (status == MYLITE_OK) {
        status = rewrite_rename_table_index_catalog(database, target);
    }
    return status;
}

static int rewrite_rename_table_catalog_row(mylite_db *database, const char *sql,
                                            const struct mylite_rename_table_target *target)
{
    sqlite3_stmt *update = NULL;
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &update, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(update, 1, target->target_schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 2, target->target_table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 3, target->source_schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 4, target->source_table_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(update);
    sqlite3_finalize(update);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int rewrite_rename_table_index_catalog(mylite_db *database,
                                              const struct mylite_rename_table_target *target)
{
    enum {
        bind_target_schema = 1,
        bind_target_table = 2,
        bind_target_index_schema = 3,
        bind_source_schema = 4,
        bind_source_table = 5,
    };
    sqlite3_stmt *update = NULL;
    static const char sql[] = "UPDATE __mylite_index_catalog "
                              "SET table_schema = ?, table_name = ?, index_schema = ? "
                              "WHERE table_schema = ? AND table_name = ?";
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &update, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(update, bind_target_schema, target->target_schema_name, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(update, bind_target_table, target->target_table_name, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(update, bind_target_index_schema, target->target_schema_name, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(update, bind_source_schema, target->source_schema_name, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(update, bind_source_table, target->source_table_name, -1,
                      sqlite_transient_destructor());
    rc = sqlite3_step(update);
    sqlite3_finalize(update);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int set_rename_table_exists_error(mylite_db *database, const char *schema_name,
                                         const char *table_name)
{
    char *message = sqlite3_mprintf("Table '%q.%q' already exists", schema_name, table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_table_ddl_execute_truncate_table_statement(mylite_db *database,
                                                      const char *selected_schema,
                                                      struct mylite_truncate_table_plan *plan)
{
    int status = validate_truncate_table_plan(database, selected_schema, plan);

    if (status == MYLITE_OK) {
        status = truncate_table_transaction(database, plan);
    }
    return status;
}

static int validate_truncate_table_plan(mylite_db *database, const char *selected_schema,
                                        struct mylite_truncate_table_plan *plan)
{
    int status = resolve_truncate_table_name(database, selected_schema, plan);

    if (status != MYLITE_OK) {
        return status;
    }
    return validate_truncate_table_target(database, plan);
}

static int resolve_truncate_table_name(mylite_db *database, const char *selected_schema,
                                       struct mylite_truncate_table_plan *plan)
{
    if (plan->table_name == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (plan->schema_name != NULL) {
        return MYLITE_OK;
    }
    if (selected_schema == NULL || selected_schema[0] == '\0') {
        (void)mylite_diagnostics_set_error_message(database, "No database selected");
        return MYLITE_EXEC_ERROR;
    }

    plan->schema_name = mylite_copy_nonempty_cstring(selected_schema);
    if (plan->schema_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int validate_truncate_table_target(mylite_db *database,
                                          const struct mylite_truncate_table_plan *plan)
{
    struct mylite_schema_presence presence;
    bool exists = false;
    int status = mylite_catalog_schema_exists(database, plan->schema_name, &presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (presence.is_system) {
        (void)mylite_diagnostics_set_error_message_parts(database, "Access to system schema '",
                                                         plan->schema_name, "' is rejected.");
        return MYLITE_EXEC_ERROR;
    }
    if (!presence.exists) {
        return mylite_diagnostics_set_table_doesnt_exist_error(database, plan->schema_name,
                                                               plan->table_name);
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

static int truncate_table_transaction(mylite_db *database,
                                      const struct mylite_truncate_table_plan *plan)
{
    struct mylite_statement_atomicity atomicity = {0};
    int status = mylite_transaction_begin_statement_atomicity(database, &atomicity);

    if (status == MYLITE_OK) {
        status = delete_truncate_table_rows(database, plan);
    }
    if (status == MYLITE_OK) {
        status = reset_truncate_table_auto_increment(database, plan);
    }
    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_statement_atomicity(database, &atomicity);
        if (status == MYLITE_OK) {
            return MYLITE_OK;
        }
    }

    mylite_transaction_rollback_statement_atomicity(database, &atomicity);
    return status;
}

static int delete_truncate_table_rows(mylite_db *database,
                                      const struct mylite_truncate_table_plan *plan)
{
    char *physical_name = mylite_catalog_physical_table_name(plan->schema_name, plan->table_name);
    char *sql = NULL;
    int rc = SQLITE_OK;

    if (physical_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    sql = sqlite3_mprintf("DELETE FROM \"%w\"", physical_name);
    free(physical_name);
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    return rc == SQLITE_OK ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int reset_truncate_table_auto_increment(mylite_db *database,
                                               const struct mylite_truncate_table_plan *plan)
{
    sqlite3_stmt *update = NULL;
    static const char sql[] = "UPDATE __mylite_table_catalog SET auto_increment = NULL "
                              "WHERE table_schema = ? AND table_name = ?";
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &update, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(update, 1, plan->schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 2, plan->table_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(update);
    sqlite3_finalize(update);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

int mylite_table_ddl_load_alter_table_model(mylite_db *database, const char *schema_name,
                                            const char *table_name,
                                            struct mylite_alter_table_model *model)
{
    sqlite3_stmt *select = NULL;
    static const char sql[] =
        "SELECT table_collation FROM __mylite_table_catalog WHERE table_schema = ? "
        "AND table_name = ?";
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    *model = (struct mylite_alter_table_model){0};
    model->schema_name = mylite_copy_nonempty_cstring(schema_name);
    model->table_name = mylite_copy_nonempty_cstring(table_name);
    model->physical_name = mylite_catalog_physical_table_name(schema_name, table_name);
    if (model->schema_name == NULL || model->table_name == NULL || model->physical_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        mylite_table_ddl_alter_table_model_deinit(model);
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    if (rc != SQLITE_OK) {
        mylite_table_ddl_alter_table_model_deinit(model);
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(select, 1, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, table_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(select);
    if (rc == SQLITE_ROW) {
        status = copy_sqlite_nullable_text_column(select, 0, &model->table_collation);
    } else {
        status =
            rc == SQLITE_DONE
                ? mylite_diagnostics_set_table_doesnt_exist_error(database, schema_name, table_name)
                : mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_finalize(select);
    if (status == MYLITE_OK) {
        status = load_alter_table_columns(database, model);
    }
    if (status == MYLITE_OK) {
        status = load_alter_table_indexes(database, model);
    }
    if (status != MYLITE_OK) {
        mylite_table_ddl_alter_table_model_deinit(model);
    }
    return status;
}

static int load_alter_table_columns(mylite_db *database, struct mylite_alter_table_model *model)
{
    sqlite3_stmt *select = NULL;
    static const char sql[] =
        "SELECT column_name, column_default, is_nullable, data_type, "
        "character_maximum_length, character_octet_length, numeric_precision, numeric_scale, "
        "datetime_precision, character_set_name, collation_name, column_type, column_key, extra, "
        "column_comment, generation_expression, srs_id FROM __mylite_column_catalog "
        "WHERE table_schema = ? AND table_name = ? ORDER BY ordinal_position";
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(select, 1, model->schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, model->table_name, -1, sqlite_transient_destructor());
    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        int status = load_alter_table_column_from_catalog_row(database, select, model);

        if (status != MYLITE_OK) {
            sqlite3_finalize(select);
            return status;
        }
    }
    sqlite3_finalize(select);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    if (model->column_count == 0U) {
        return mylite_diagnostics_set_table_doesnt_exist_error(database, model->schema_name,
                                                               model->table_name);
    }
    return MYLITE_OK;
}

static int load_alter_table_column_from_catalog_row(mylite_db *database, sqlite3_stmt *select,
                                                    struct mylite_alter_table_model *model)
{
    struct mylite_alter_table_column column = {0};
    int status = load_alter_table_column_text_catalog_fields(select, &column);

    load_alter_table_column_numeric_catalog_fields(select, &column);
    if (status != MYLITE_OK) {
        mylite_table_ddl_alter_table_column_deinit(&column);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }

    load_alter_table_column_flags(&column);
    status = mylite_table_ddl_add_alter_table_column(model, column);
    if (status != MYLITE_OK) {
        mylite_table_ddl_alter_table_column_deinit(&column);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
    }
    return status;
}

static int load_alter_table_column_text_catalog_fields(sqlite3_stmt *select,
                                                       struct mylite_alter_table_column *column)
{
    int status =
        copy_sqlite_text_column(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_NAME, &column->name);

    if (status == MYLITE_OK) {
        column->source_name = mylite_copy_nonempty_cstring(column->name);
        if (column->source_name == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_nullable_text_column(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_DEFAULT,
                                                  &column->column_default);
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_text_column(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_IS_NULLABLE,
                                         &column->is_nullable);
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_nullable_text_column(
            select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_DATA_TYPE, &column->data_type);
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_nullable_text_column(
            select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_CHARACTER_SET_NAME,
            &column->character_set_name);
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_nullable_text_column(
            select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_COLLATION_NAME, &column->collation_name);
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_text_column(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_COLUMN_TYPE,
                                         &column->column_type);
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_text_column(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_COLUMN_KEY,
                                         &column->column_key);
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_nullable_text_column(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_EXTRA,
                                                  &column->extra);
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_text_column(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_COMMENT,
                                         &column->column_comment);
    }
    if (status == MYLITE_OK) {
        status =
            copy_sqlite_text_column(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_GENERATION_EXPRESSION,
                                    &column->generation_expression);
    }
    return status;
}

static void load_alter_table_column_numeric_catalog_fields(sqlite3_stmt *select,
                                                           struct mylite_alter_table_column *column)
{
    if (sqlite3_column_type(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_CHARACTER_MAXIMUM_LENGTH) !=
        SQLITE_NULL) {
        column->character_maximum_length = sqlite3_column_int64(
            select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_CHARACTER_MAXIMUM_LENGTH);
        column->has_character_maximum_length = true;
    }
    if (sqlite3_column_type(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_CHARACTER_OCTET_LENGTH) !=
        SQLITE_NULL) {
        column->character_octet_length =
            sqlite3_column_int64(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_CHARACTER_OCTET_LENGTH);
        column->has_character_octet_length = true;
    }
    if (sqlite3_column_type(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_NUMERIC_PRECISION) !=
        SQLITE_NULL) {
        column->numeric_precision =
            sqlite3_column_int64(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_NUMERIC_PRECISION);
        column->has_numeric_precision = true;
    }
    if (sqlite3_column_type(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_NUMERIC_SCALE) !=
        SQLITE_NULL) {
        column->numeric_scale =
            sqlite3_column_int64(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_NUMERIC_SCALE);
        column->has_numeric_scale = true;
    }
    if (sqlite3_column_type(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_DATETIME_PRECISION) !=
        SQLITE_NULL) {
        column->datetime_precision =
            sqlite3_column_int64(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_DATETIME_PRECISION);
        column->has_datetime_precision = true;
    }
    if (sqlite3_column_type(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_SRS_ID) != SQLITE_NULL) {
        column->srs_id = sqlite3_column_int64(select, MYLITE_ALTER_TABLE_COLUMN_CATALOG_SRS_ID);
        column->has_srs_id = true;
    }
}

static void load_alter_table_column_flags(struct mylite_alter_table_column *column)
{
    column->nullable = mylite_ascii_case_equal(column->is_nullable, "YES");
    column->auto_increment = mylite_text_contains_word(column->extra, "auto_increment");
    column->visible = true;
    if (mylite_text_contains_word(column->extra, "INVISIBLE")) {
        column->visible = false;
    }
}

int mylite_table_ddl_add_alter_table_column(struct mylite_alter_table_model *model,
                                            struct mylite_alter_table_column column)
{
    struct mylite_alter_table_column *columns =
        realloc(model->columns, (model->column_count + 1U) * sizeof(*model->columns));

    if (columns == NULL) {
        return MYLITE_NOMEM;
    }

    model->columns = columns;
    model->columns[model->column_count++] = column;
    return MYLITE_OK;
}

static int load_alter_table_indexes(mylite_db *database, struct mylite_alter_table_model *model)
{
    sqlite3_stmt *select = NULL;
    static const char sql[] =
        "SELECT non_unique, index_schema, index_name, seq_in_index, column_name, collation, "
        "sub_part, nullable, index_type, comment, index_comment, is_visible "
        "FROM __mylite_index_catalog WHERE table_schema = ? AND table_name = ? "
        "ORDER BY rowid";
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(select, 1, model->schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, model->table_name, -1, sqlite_transient_destructor());
    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        int status = add_alter_table_index_part(database, model, select);

        if (status != MYLITE_OK) {
            sqlite3_finalize(select);
            return status;
        }
    }
    sqlite3_finalize(select);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int add_alter_table_index_part(mylite_db *database, struct mylite_alter_table_model *model,
                                      sqlite3_stmt *select)
{
    const char *index_name =
        (const char *)sqlite3_column_text(select, MYLITE_ALTER_TABLE_INDEX_CATALOG_NAME);
    size_t index = loaded_alter_table_index_index(model, index_name);
    struct mylite_alter_table_index_part part = {0};
    int status = MYLITE_OK;

    if (index == model->index_count) {
        struct mylite_alter_table_index *indexes =
            realloc(model->indexes, (model->index_count + 1U) * sizeof(*model->indexes));

        if (indexes == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        model->indexes = indexes;
        model->indexes[model->index_count] = (struct mylite_alter_table_index){
            .non_unique = sqlite3_column_int(select, MYLITE_ALTER_TABLE_INDEX_CATALOG_NON_UNIQUE),
        };
        index = model->index_count++;

        status = copy_sqlite_text_column(select, MYLITE_ALTER_TABLE_INDEX_CATALOG_SCHEMA,
                                         &model->indexes[index].index_schema);
        if (status == MYLITE_OK) {
            status = copy_sqlite_text_column(select, MYLITE_ALTER_TABLE_INDEX_CATALOG_NAME,
                                             &model->indexes[index].name);
        }
        if (status == MYLITE_OK) {
            status = copy_sqlite_text_column(select, MYLITE_ALTER_TABLE_INDEX_CATALOG_TYPE,
                                             &model->indexes[index].index_type);
        }
        if (status == MYLITE_OK) {
            status = copy_sqlite_text_column(select, MYLITE_ALTER_TABLE_INDEX_CATALOG_COMMENT,
                                             &model->indexes[index].comment);
        }
        if (status == MYLITE_OK) {
            status = copy_sqlite_text_column(select, MYLITE_ALTER_TABLE_INDEX_CATALOG_INDEX_COMMENT,
                                             &model->indexes[index].index_comment);
        }
        if (status == MYLITE_OK) {
            status = copy_sqlite_text_column(select, MYLITE_ALTER_TABLE_INDEX_CATALOG_VISIBLE,
                                             &model->indexes[index].is_visible);
        }
        if (status != MYLITE_OK) {
            if (status == MYLITE_NOMEM) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
            }
            return status;
        }
    }

    status = copy_sqlite_nullable_text_column(select, MYLITE_ALTER_TABLE_INDEX_CATALOG_COLUMN_NAME,
                                              &part.column_name);
    if (status == MYLITE_OK) {
        status = copy_sqlite_nullable_text_column(
            select, MYLITE_ALTER_TABLE_INDEX_CATALOG_COLLATION, &part.collation);
    }
    if (sqlite3_column_type(select, MYLITE_ALTER_TABLE_INDEX_CATALOG_SUB_PART) != SQLITE_NULL) {
        part.sub_part = sqlite3_column_int64(select, MYLITE_ALTER_TABLE_INDEX_CATALOG_SUB_PART);
        part.has_sub_part = true;
    }
    if (status == MYLITE_OK) {
        status = copy_sqlite_text_column(select, MYLITE_ALTER_TABLE_INDEX_CATALOG_NULLABLE,
                                         &part.nullable);
    }
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_append_alter_table_index_part(&model->indexes[index], part);
    }
    if (status != MYLITE_OK) {
        mylite_table_ddl_alter_table_index_part_deinit(&part);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
    }
    return status;
}

static size_t loaded_alter_table_index_index(const struct mylite_alter_table_model *model,
                                             const char *name)
{
    for (size_t index = 0U; index < model->index_count; ++index) {
        if (mylite_ascii_case_equal(model->indexes[index].name, name)) {
            return index;
        }
    }
    return model->index_count;
}

int mylite_table_ddl_append_alter_table_index_part(struct mylite_alter_table_index *index,
                                                   struct mylite_alter_table_index_part part)
{
    struct mylite_alter_table_index_part *parts =
        realloc(index->parts, (index->part_count + 1U) * sizeof(*index->parts));

    if (parts == NULL) {
        return MYLITE_NOMEM;
    }
    index->parts = parts;
    index->parts[index->part_count++] = part;
    return MYLITE_OK;
}

static int copy_sqlite_text_column(sqlite3_stmt *stmt, int column, char **out_text)
{
    const unsigned char *text = sqlite3_column_text(stmt, column);
    int byte_count = sqlite3_column_bytes(stmt, column);

    *out_text = mylite_copy_span_text(text == NULL ? "" : (const char *)text,
                                      text == NULL || byte_count < 0 ? 0U : (size_t)byte_count);
    return *out_text == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static int copy_sqlite_nullable_text_column(sqlite3_stmt *stmt, int column, char **out_text)
{
    if (sqlite3_column_type(stmt, column) == SQLITE_NULL) {
        *out_text = NULL;
        return MYLITE_OK;
    }
    return copy_sqlite_text_column(stmt, column, out_text);
}

int mylite_table_ddl_describe_create_table_column(
    const struct mylite_create_table_column *column,
    const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_options *table_options,
    struct mylite_column_type_descriptor *out_descriptor)
{
    const char *type_name = create_table_column_type_name(column->type.ast_type);
    struct mylite_column_type_attributes attributes = column->type.attributes;
    enum mylite_column_type_status status = MYLITE_COLUMN_TYPE_OK;

    if (type_name == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    if (create_table_column_uses_character_set_defaults(column->type.ast_type) &&
        !attributes.has_character_set && !attributes.has_collation) {
        const char *character_set = table_options->character_set == NULL
                                        ? schema_default->character_set
                                        : table_options->character_set;
        const char *collation =
            table_options->collation == NULL ? schema_default->collation : table_options->collation;

        attributes.has_character_set = true;
        attributes.character_set = character_set;
        attributes.character_set_length = strlen(character_set);
        attributes.has_collation = true;
        attributes.collation = collation;
        attributes.collation_length = strlen(collation);
    }

    if (create_table_column_uses_integer_descriptor(column->type.ast_type)) {
        status = mylite_column_type_describe_integer(type_name, strlen(type_name), attributes,
                                                     out_descriptor);
    } else if (create_table_column_uses_string_binary_descriptor(column->type.ast_type)) {
        status = mylite_column_type_describe_string_binary(type_name, strlen(type_name), attributes,
                                                           out_descriptor);
    } else if (create_table_column_uses_numeric_descriptor(column->type.ast_type)) {
        status = mylite_column_type_describe_numeric(type_name, strlen(type_name), attributes,
                                                     out_descriptor);
    } else if (create_table_column_uses_temporal_descriptor(column->type.ast_type)) {
        status = mylite_column_type_describe_temporal(type_name, strlen(type_name), attributes,
                                                      out_descriptor);
    } else {
        return MYLITE_UNSUPPORTED;
    }

    return status == MYLITE_COLUMN_TYPE_OK ? MYLITE_OK : MYLITE_EXEC_ERROR;
}

static const char *create_table_column_key(const struct mylite_create_table_plan *plan,
                                           const char *column_name)
{
    struct mylite_create_table_column_index_status status =
        create_table_column_index_status(plan, column_name);

    if (status.primary) {
        return "PRI";
    }
    if (status.unique) {
        return "UNI";
    }
    if (status.indexed) {
        return "MUL";
    }
    return "";
}

const char *
mylite_table_ddl_create_table_column_extra(const struct mylite_create_table_column *column)
{
    if (column->auto_increment) {
        return "auto_increment";
    }
    if (column->has_generated_default && column->has_on_update_current_timestamp &&
        !column->visible) {
        return "DEFAULT_GENERATED on update CURRENT_TIMESTAMP INVISIBLE";
    }
    if (column->has_generated_default && column->has_on_update_current_timestamp) {
        return "DEFAULT_GENERATED on update CURRENT_TIMESTAMP";
    }
    if (column->has_generated_default && !column->visible) {
        return "DEFAULT_GENERATED INVISIBLE";
    }
    if (column->has_generated_default) {
        return "DEFAULT_GENERATED";
    }
    if (column->has_on_update_current_timestamp && !column->visible) {
        return "on update CURRENT_TIMESTAMP INVISIBLE";
    }
    if (column->has_on_update_current_timestamp) {
        return "on update CURRENT_TIMESTAMP";
    }
    if (!column->visible) {
        return "INVISIBLE";
    }
    return "";
}

const char *mylite_table_ddl_index_collation_for_order(enum mylite_sql_ast_key_part_order order)
{
    return order == MYLITE_SQL_AST_KEY_PART_ORDER_DESC ? "D" : "A";
}

static int insert_table_catalog_row(mylite_db *database, const char *schema_name,
                                    const struct mylite_schema_default *schema_default,
                                    const struct mylite_create_table_plan *plan)
{
    enum {
        bind_auto_increment = 4,
        bind_table_collation = 5,
        bind_table_comment = 6,
    };
    sqlite3_stmt *insert = NULL;
    static const char sql[] =
        "INSERT INTO __mylite_table_catalog("
        "table_catalog, table_schema, table_name, table_type, engine, version, row_format, "
        "table_rows, avg_row_length, data_length, max_data_length, index_length, data_free, "
        "auto_increment, create_time, update_time, check_time, table_collation, checksum, "
        "create_options, table_comment)"
        " VALUES('def', ?, ?, 'BASE TABLE', ?, 10, NULL, 0, NULL, NULL, NULL, NULL, NULL, "
        "?, '1970-01-01 00:00:00', NULL, NULL, ?, NULL, '', ?)";
    const char *collation =
        plan->options.collation == NULL ? schema_default->collation : plan->options.collation;
    const char *comment = plan->options.comment == NULL ? "" : plan->options.comment;
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &insert, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(insert, 1, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, 2, plan->table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, 3, "InnoDB", -1, SQLITE_STATIC);
    if (plan->options.has_auto_increment) {
        sqlite3_bind_int64(insert, bind_auto_increment,
                           (sqlite3_int64)plan->options.auto_increment);
    } else {
        sqlite3_bind_null(insert, bind_auto_increment);
    }
    sqlite3_bind_text(insert, bind_table_collation, collation, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_table_comment, comment, -1, sqlite_transient_destructor());

    rc = sqlite3_step(insert);
    sqlite3_finalize(insert);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    return MYLITE_OK;
}

static int insert_column_catalog_rows(mylite_db *database, const char *schema_name,
                                      const struct mylite_schema_default *schema_default,
                                      const struct mylite_create_table_plan *plan)
{
    sqlite3_stmt *insert = NULL;
    static const char sql[] =
        "INSERT INTO __mylite_column_catalog("
        "table_catalog, table_schema, table_name, column_name, ordinal_position, column_default, "
        "is_nullable, data_type, character_maximum_length, character_octet_length, "
        "numeric_precision, numeric_scale, datetime_precision, character_set_name, "
        "collation_name, column_type, column_key, extra, privileges, column_comment, "
        "generation_expression, srs_id)"
        " VALUES('def', ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "'select,insert,update,references', ?, '', NULL)";
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &insert, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    for (size_t index = 0U; index < plan->column_count; ++index) {
        int status = insert_column_catalog_row(database, insert, schema_name, schema_default, plan,
                                               &plan->columns[index], index);
        if (status != MYLITE_OK) {
            sqlite3_finalize(insert);
            return status;
        }
    }

    sqlite3_finalize(insert);
    return MYLITE_OK;
}

static int insert_column_catalog_row(mylite_db *database, sqlite3_stmt *insert,
                                     const char *schema_name,
                                     const struct mylite_schema_default *schema_default,
                                     const struct mylite_create_table_plan *plan,
                                     const struct mylite_create_table_column *column,
                                     size_t column_index)
{
    enum {
        bind_table_schema = 1,
        bind_table_name = 2,
        bind_column_name = 3,
        bind_ordinal_position = 4,
        bind_column_default = 5,
        bind_is_nullable = 6,
        bind_data_type = 7,
        bind_character_maximum_length = 8,
        bind_character_octet_length = 9,
        bind_numeric_precision = 10,
        bind_numeric_scale = 11,
        bind_datetime_precision = 12,
        bind_character_set_name = 13,
        bind_collation_name = 14,
        bind_column_type = 15,
        bind_column_key = 16,
        bind_extra = 17,
        bind_column_comment = 18,
    };
    struct mylite_column_type_descriptor descriptor;
    const char *column_key = create_table_column_key(plan, column->name);
    const char *extra = mylite_table_ddl_create_table_column_extra(column);
    const char *is_nullable = "NO";
    const char *comment = "";
    int status = mylite_table_ddl_describe_create_table_column(column, schema_default,
                                                               &plan->options, &descriptor);
    int rc = SQLITE_OK;

    if (status != MYLITE_OK) {
        return status;
    }
    if (column->nullable) {
        is_nullable = "YES";
    }
    if (column->comment != NULL) {
        comment = column->comment;
    }

    sqlite3_reset(insert);
    sqlite3_clear_bindings(insert);
    sqlite3_bind_text(insert, bind_table_schema, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_table_name, plan->table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_column_name, column->name, -1, sqlite_transient_destructor());
    sqlite3_bind_int64(insert, bind_ordinal_position, (sqlite3_int64)column_index + 1);
    if (column->default_text == NULL) {
        sqlite3_bind_null(insert, bind_column_default);
    } else {
        sqlite3_bind_text(insert, bind_column_default, column->default_text, -1,
                          sqlite_transient_destructor());
    }
    sqlite3_bind_text(insert, bind_is_nullable, is_nullable, -1, SQLITE_STATIC);
    sqlite3_bind_text(insert, bind_data_type, descriptor.data_type, -1, SQLITE_STATIC);
    if (descriptor.is_character_string || descriptor.is_binary_string) {
        sqlite3_bind_int64(insert, bind_character_maximum_length,
                           (sqlite3_int64)descriptor.character_maximum_length);
        sqlite3_bind_int64(insert, bind_character_octet_length,
                           (sqlite3_int64)descriptor.character_octet_length);
    } else {
        sqlite3_bind_null(insert, bind_character_maximum_length);
        sqlite3_bind_null(insert, bind_character_octet_length);
    }
    if (descriptor.numeric_precision != 0U) {
        sqlite3_bind_int(insert, bind_numeric_precision, (int)descriptor.numeric_precision);
    } else {
        sqlite3_bind_null(insert, bind_numeric_precision);
    }
    if (descriptor.has_numeric_scale) {
        sqlite3_bind_int(insert, bind_numeric_scale, (int)descriptor.numeric_scale);
    } else {
        sqlite3_bind_null(insert, bind_numeric_scale);
    }
    if (descriptor.has_datetime_precision) {
        sqlite3_bind_int(insert, bind_datetime_precision, (int)descriptor.datetime_precision);
    } else {
        sqlite3_bind_null(insert, bind_datetime_precision);
    }
    if (descriptor.character_set_name == NULL) {
        sqlite3_bind_null(insert, bind_character_set_name);
    } else {
        sqlite3_bind_text(insert, bind_character_set_name, descriptor.character_set_name, -1,
                          SQLITE_STATIC);
    }
    if (descriptor.collation_name == NULL) {
        sqlite3_bind_null(insert, bind_collation_name);
    } else {
        sqlite3_bind_text(insert, bind_collation_name, descriptor.collation_name, -1,
                          SQLITE_STATIC);
    }
    sqlite3_bind_text(insert, bind_column_type, descriptor.column_type, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_column_key, column_key, -1, SQLITE_STATIC);
    if (extra == NULL || extra[0] == '\0') {
        sqlite3_bind_text(insert, bind_extra, "", -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_text(insert, bind_extra, extra, -1, SQLITE_STATIC);
    }
    sqlite3_bind_text(insert, bind_column_comment, comment, -1, sqlite_transient_destructor());

    rc = sqlite3_step(insert);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    return MYLITE_OK;
}

static int insert_index_catalog_rows(mylite_db *database, const char *schema_name,
                                     const struct mylite_create_table_plan *plan)
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

    for (size_t index_index = 0U; index_index < plan->index_count; ++index_index) {
        const struct mylite_create_table_index *index = &plan->indexes[index_index];

        for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
            int status = insert_index_catalog_part(database, insert, schema_name, plan, index,
                                                   &index->parts[part_index], part_index);
            if (status != MYLITE_OK) {
                sqlite3_finalize(insert);
                return status;
            }
        }
    }

    sqlite3_finalize(insert);
    return MYLITE_OK;
}

static int insert_index_catalog_part(mylite_db *database, sqlite3_stmt *insert,
                                     const char *schema_name,
                                     const struct mylite_create_table_plan *plan,
                                     const struct mylite_create_table_index *index,
                                     const struct mylite_create_table_key_part *part,
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
    const struct mylite_create_table_column *column =
        find_create_table_column(plan, part->column_name);
    int non_unique = 1;
    const char *nullable = "";
    const char *index_type = "BTREE";
    const char *is_visible = "NO";
    int rc = SQLITE_OK;

    if (index->is_unique) {
        non_unique = 0;
    }
    if (column != NULL && column->nullable) {
        nullable = "YES";
    }
    if (index->algorithm == MYLITE_SQL_AST_INDEX_ALGORITHM_HASH) {
        index_type = "HASH";
    }
    if (index->is_visible) {
        is_visible = "YES";
    }

    sqlite3_reset(insert);
    sqlite3_clear_bindings(insert);
    sqlite3_bind_text(insert, bind_table_schema, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_table_name, plan->table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_int(insert, bind_non_unique, non_unique);
    sqlite3_bind_text(insert, bind_index_schema, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_index_name, index->name, -1, sqlite_transient_destructor());
    sqlite3_bind_int64(insert, bind_seq_in_index, (sqlite3_int64)part_index + 1);
    sqlite3_bind_text(insert, bind_column_name, part->column_name, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_collation,
                      mylite_table_ddl_index_collation_for_order(part->order), -1, SQLITE_STATIC);
    if (part->has_prefix_length) {
        sqlite3_bind_int64(insert, bind_sub_part, (sqlite3_int64)part->prefix_length);
    } else {
        sqlite3_bind_null(insert, bind_sub_part);
    }
    sqlite3_bind_text(insert, bind_nullable, nullable, -1, SQLITE_STATIC);
    sqlite3_bind_text(insert, bind_index_type, index_type, -1, SQLITE_STATIC);
    sqlite3_bind_text(insert, bind_index_comment, index->comment == NULL ? "" : index->comment, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_is_visible, is_visible, -1, SQLITE_STATIC);

    rc = sqlite3_step(insert);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    return MYLITE_OK;
}

static const char *
sqlite_affinity_for_descriptor(const struct mylite_column_type_descriptor *descriptor)
{
    if (descriptor->integer_type != MYLITE_COLUMN_INTEGER_NONE || descriptor->is_boolean_alias) {
        return "INTEGER";
    }
    if (descriptor->is_approximate_numeric) {
        return "REAL";
    }
    if (descriptor->is_exact_numeric) {
        return "NUMERIC";
    }
    if (descriptor->is_binary_string) {
        return "BLOB";
    }
    return "TEXT";
}

static const struct mylite_create_table_column *
find_create_table_column(const struct mylite_create_table_plan *plan, const char *name)
{
    for (size_t index = 0U; index < plan->column_count; ++index) {
        if (mylite_ascii_case_equal(plan->columns[index].name, name)) {
            return &plan->columns[index];
        }
    }
    return NULL;
}

static int normalize_create_table_options(mylite_db *database, const char *schema_name,
                                          const struct mylite_schema_default *schema_default,
                                          struct mylite_create_table_options *options)
{
    const struct mylite_charset *character_set = NULL;
    const struct mylite_collation *collation = NULL;
    const char *collation_name = NULL;
    int status = MYLITE_OK;

    (void)schema_name;
    if (options->engine != NULL && !is_supported_engine_name(options->engine)) {
        status = mylite_diagnostics_set_error_message_parts(
            database, "Unsupported storage engine: '", options->engine, "'");
        return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
    }
    if (options->character_set != NULL) {
        character_set = mylite_charset_lookup(options->character_set);
        if (character_set == NULL) {
            return mylite_diagnostics_set_unknown_charset_error(database, options->character_set);
        }
    }
    if (options->collation != NULL) {
        collation = mylite_collation_lookup(options->collation);
        if (collation == NULL) {
            return mylite_diagnostics_set_unknown_collation_error(database, options->collation);
        }
    }
    if (character_set == NULL && collation != NULL) {
        character_set = mylite_charset_lookup(collation->character_set);
    }
    if (character_set == NULL) {
        character_set = mylite_charset_lookup(schema_default->character_set);
    }
    if (collation == NULL) {
        collation_name = options->character_set == NULL ? schema_default->collation
                                                        : character_set->default_collation;
        collation = mylite_collation_lookup(collation_name);
    }
    if (character_set == NULL || collation == NULL) {
        (void)mylite_diagnostics_set_error_message(database,
                                                   "Unsupported charset/collation registry entry");
        return MYLITE_EXEC_ERROR;
    }
    if (!mylite_charset_collation_match(character_set, collation)) {
        return mylite_diagnostics_set_collation_charset_error(database, collation->name,
                                                              character_set->name);
    }

    status =
        normalize_create_table_option_text(database, &options->character_set, character_set->name);
    if (status != MYLITE_OK) {
        return status;
    }
    return normalize_create_table_option_text(database, &options->collation, collation->name);
}

static int normalize_create_table_option_text(mylite_db *database, char **target, const char *value)
{
    char *copy = mylite_copy_span_text(value, strlen(value));

    if (copy == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    free(*target);
    *target = copy;
    return MYLITE_OK;
}

static bool is_supported_engine_name(const char *name)
{
    if (name == NULL) {
        return true;
    }
    return mylite_ascii_case_equal(name, "InnoDB");
}

static bool validate_create_table_column_names(mylite_db *database,
                                               const struct mylite_create_table_plan *plan)
{
    if (plan->column_count == 0U) {
        (void)mylite_diagnostics_set_error_message(database,
                                                   "CREATE TABLE requires at least one column");
        return false;
    }

    for (size_t left = 0U; left < plan->column_count; ++left) {
        for (size_t right = left + 1U; right < plan->column_count; ++right) {
            if (mylite_ascii_case_equal(plan->columns[left].name, plan->columns[right].name)) {
                (void)mylite_diagnostics_set_error_message_parts(
                    database, "Duplicate column name '", plan->columns[right].name, "'");
                return false;
            }
        }
    }
    return true;
}

static bool validate_create_table_indexes(mylite_db *database,
                                          const struct mylite_create_table_plan *plan)
{
    bool has_primary = false;

    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct mylite_create_table_index *table_index = &plan->indexes[index];

        if (table_index->is_primary) {
            if (has_primary) {
                (void)mylite_diagnostics_set_error_message(database,
                                                           "Multiple primary key defined");
                return false;
            }
            has_primary = true;
        }
        if (table_index->explicit_name &&
            create_table_index_name_exists(plan, table_index->name, index)) {
            (void)mylite_diagnostics_set_error_message_parts(database, "Duplicate key name '",
                                                             table_index->name, "'");
            return false;
        }
        for (size_t part = 0U; part < table_index->part_count; ++part) {
            if (find_create_table_column(plan, table_index->parts[part].column_name) == NULL) {
                (void)mylite_diagnostics_set_error_message_parts(
                    database, "Key column '", table_index->parts[part].column_name,
                    "' doesn't exist in table");
                return false;
            }
        }
    }
    return true;
}

static void apply_create_table_primary_key_nullability(struct mylite_create_table_plan *plan)
{
    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct mylite_create_table_index *table_index = &plan->indexes[index];

        if (!table_index->is_primary) {
            continue;
        }
        for (size_t part = 0U; part < table_index->part_count; ++part) {
            for (size_t column = 0U; column < plan->column_count; ++column) {
                if (mylite_ascii_case_equal(plan->columns[column].name,
                                            table_index->parts[part].column_name)) {
                    plan->columns[column].nullable = false;
                }
            }
        }
    }
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

static struct mylite_create_table_column_index_status
create_table_column_index_status(const struct mylite_create_table_plan *plan,
                                 const char *column_name)
{
    struct mylite_create_table_column_index_status status = {
        .indexed = false,
        .unique = false,
        .primary = false,
    };

    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct mylite_create_table_index *table_index = &plan->indexes[index];

        for (size_t part = 0U; part < table_index->part_count; ++part) {
            if (!mylite_ascii_case_equal(table_index->parts[part].column_name, column_name)) {
                continue;
            }
            status.indexed = true;
            if (table_index->is_primary) {
                status.primary = true;
            }
            if (table_index->is_unique && part == 0U) {
                status.unique = true;
            }
        }
    }
    return status;
}

static const char *create_table_column_type_name(enum mylite_sql_ast_column_type column_type)
{
    switch (column_type) {
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYINT:
        return "TINYINT";
    case MYLITE_SQL_AST_COLUMN_TYPE_SMALLINT:
        return "SMALLINT";
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMINT:
        return "MEDIUMINT";
    case MYLITE_SQL_AST_COLUMN_TYPE_INT:
        return "INT";
    case MYLITE_SQL_AST_COLUMN_TYPE_BIGINT:
        return "BIGINT";
    case MYLITE_SQL_AST_COLUMN_TYPE_BOOL:
        return "BOOL";
    case MYLITE_SQL_AST_COLUMN_TYPE_BOOLEAN:
        return "BOOLEAN";
    case MYLITE_SQL_AST_COLUMN_TYPE_CHAR:
        return "CHAR";
    case MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR:
        return "VARCHAR";
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYTEXT:
        return "TINYTEXT";
    case MYLITE_SQL_AST_COLUMN_TYPE_TEXT:
        return "TEXT";
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMTEXT:
        return "MEDIUMTEXT";
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGTEXT:
        return "LONGTEXT";
    case MYLITE_SQL_AST_COLUMN_TYPE_BINARY:
        return "BINARY";
    case MYLITE_SQL_AST_COLUMN_TYPE_VARBINARY:
        return "VARBINARY";
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYBLOB:
        return "TINYBLOB";
    case MYLITE_SQL_AST_COLUMN_TYPE_BLOB:
        return "BLOB";
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMBLOB:
        return "MEDIUMBLOB";
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGBLOB:
        return "LONGBLOB";
    case MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL:
        return "DECIMAL";
    case MYLITE_SQL_AST_COLUMN_TYPE_FLOAT:
        return "FLOAT";
    case MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE:
        return "DOUBLE";
    case MYLITE_SQL_AST_COLUMN_TYPE_DATE:
        return "DATE";
    case MYLITE_SQL_AST_COLUMN_TYPE_TIME:
        return "TIME";
    case MYLITE_SQL_AST_COLUMN_TYPE_DATETIME:
        return "DATETIME";
    case MYLITE_SQL_AST_COLUMN_TYPE_TIMESTAMP:
        return "TIMESTAMP";
    case MYLITE_SQL_AST_COLUMN_TYPE_YEAR:
        return "YEAR";
    case MYLITE_SQL_AST_COLUMN_TYPE_NONE:
        return NULL;
    }

    return NULL;
}

static bool create_table_column_uses_integer_descriptor(enum mylite_sql_ast_column_type column_type)
{
    if (column_type < MYLITE_SQL_AST_COLUMN_TYPE_TINYINT) {
        return false;
    }
    if (column_type > MYLITE_SQL_AST_COLUMN_TYPE_BOOLEAN) {
        return false;
    }
    return true;
}

static bool
create_table_column_uses_string_binary_descriptor(enum mylite_sql_ast_column_type column_type)
{
    if (column_type < MYLITE_SQL_AST_COLUMN_TYPE_CHAR) {
        return false;
    }
    if (column_type > MYLITE_SQL_AST_COLUMN_TYPE_LONGBLOB) {
        return false;
    }
    return true;
}

static bool
create_table_column_uses_character_set_defaults(enum mylite_sql_ast_column_type column_type)
{
    if (column_type < MYLITE_SQL_AST_COLUMN_TYPE_CHAR) {
        return false;
    }
    if (column_type > MYLITE_SQL_AST_COLUMN_TYPE_LONGTEXT) {
        return false;
    }
    return true;
}

static bool create_table_column_uses_numeric_descriptor(enum mylite_sql_ast_column_type column_type)
{
    if (column_type < MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL) {
        return false;
    }
    if (column_type > MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE) {
        return false;
    }
    return true;
}

static bool
create_table_column_uses_temporal_descriptor(enum mylite_sql_ast_column_type column_type)
{
    if (column_type < MYLITE_SQL_AST_COLUMN_TYPE_DATE) {
        return false;
    }
    if (column_type > MYLITE_SQL_AST_COLUMN_TYPE_YEAR) {
        return false;
    }
    return true;
}

static sqlite3_destructor_type sqlite_transient_destructor(void)
{
    // SQLite's public macro intentionally uses this sentinel pointer value.
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
