#include "mylite_table_ddl.h"

#include "mylite_catalog.h"
#include "mylite_charset.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static int assign_generated_index_names(mylite_db *database, struct mylite_create_table_plan *plan);
static int assign_generated_index_name(mylite_db *database, struct mylite_create_table_plan *plan,
                                       size_t index);
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
static int copy_create_table_elements(const struct mylite_sql_ast_node *elements,
                                      struct mylite_create_table_plan *plan);
static int copy_create_table_column_type(const struct mylite_sql_ast_node *type_node,
                                         struct mylite_create_table_column_type *type);
static int copy_create_table_column_attributes(const struct mylite_sql_ast_node *attributes,
                                               struct mylite_create_table_column *column);
static int copy_create_table_options(const struct mylite_sql_ast_node *statement,
                                     struct mylite_create_table_options *options);
static int add_create_table_index(struct mylite_create_table_plan *plan,
                                  struct mylite_create_table_index index);
static int add_inline_create_table_column_indexes(struct mylite_create_table_plan *plan,
                                                  const struct mylite_create_table_column *column);
static int add_single_column_index(struct mylite_create_table_plan *plan, const char *column_name,
                                   bool is_primary, bool is_unique);
static char *copy_expression_text(const struct mylite_sql_ast_node *node);
static void create_table_options_deinit(struct mylite_create_table_options *options);

int mylite_table_ddl_validate_create_table_plan(mylite_db *database, const char *schema_name,
                                                struct mylite_create_table_plan *plan,
                                                bool if_not_exists,
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
    status = assign_generated_index_names(database, plan);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!validate_create_table_indexes(database, plan)) {
        return MYLITE_EXEC_ERROR;
    }
    apply_create_table_primary_key_nullability(plan);
    return MYLITE_OK;
}

int mylite_table_ddl_insert_create_table_catalog_rows(
    mylite_db *database, const char *schema_name,
    const struct mylite_schema_default *schema_default, const struct mylite_create_table_plan *plan)
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

int mylite_table_ddl_create_physical_table(mylite_db *database, const char *schema_name,
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

const char *mylite_table_ddl_create_table_column_key(const struct mylite_create_table_plan *plan,
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
    const char *column_key = mylite_table_ddl_create_table_column_key(plan, column->name);
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
        mylite_table_ddl_find_create_table_column(plan, part->column_name);
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

int mylite_table_ddl_copy_create_table_statement(const struct mylite_sql_ast_node *statement,
                                                 struct mylite_create_table_plan *plan)
{
    const struct mylite_sql_ast_node *table_name = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *elements = mylite_ast_child_at(statement, 1U);
    int status = mylite_table_ddl_copy_create_table_name(table_name, plan);

    if (status != MYLITE_OK) {
        return status;
    }
    status = copy_create_table_elements(elements, plan);
    if (status != MYLITE_OK) {
        return status;
    }
    return copy_create_table_options(statement, &plan->options);
}

int mylite_table_ddl_copy_create_table_name(const struct mylite_sql_ast_node *table_name,
                                            struct mylite_create_table_plan *plan)
{
    if (table_name == NULL) {
        return MYLITE_NOMEM;
    }
    if (table_name->kind == MYLITE_SQL_AST_IDENTIFIER) {
        plan->table_name = mylite_copy_identifier_span(table_name);
        return plan->table_name == NULL ? MYLITE_NOMEM : MYLITE_OK;
    }
    if (table_name->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER &&
        mylite_ast_child_at(table_name, 0U) != NULL &&
        mylite_ast_child_at(table_name, 1U) != NULL &&
        mylite_ast_child_at(table_name, 0U)->kind == MYLITE_SQL_AST_IDENTIFIER &&
        mylite_ast_child_at(table_name, 1U)->kind == MYLITE_SQL_AST_IDENTIFIER) {
        plan->schema_name = mylite_copy_identifier_span(mylite_ast_child_at(table_name, 0U));
        plan->table_name = mylite_copy_identifier_span(mylite_ast_child_at(table_name, 1U));
        if (plan->schema_name == NULL || plan->table_name == NULL) {
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }
    return MYLITE_UNSUPPORTED;
}

int mylite_table_ddl_copy_create_table_column(const struct mylite_sql_ast_node *column_node,
                                              struct mylite_create_table_plan *plan)
{
    struct mylite_create_table_column *columns = NULL;
    struct mylite_create_table_column column = {
        .nullable = true,
        .visible = true,
    };
    int status = MYLITE_OK;

    column.name = mylite_copy_identifier_span(mylite_ast_child_at(column_node, 0U));
    if (column.name == NULL) {
        return MYLITE_NOMEM;
    }
    status = copy_create_table_column_type(mylite_ast_child_at(column_node, 1U), &column.type);
    if (status == MYLITE_OK) {
        status = copy_create_table_column_attributes(mylite_ast_child_at(column_node, 2U), &column);
    }
    if (status != MYLITE_OK) {
        mylite_table_ddl_create_table_column_deinit(&column);
        return status;
    }

    columns = realloc(plan->columns, (plan->column_count + 1U) * sizeof(*plan->columns));
    if (columns == NULL) {
        mylite_table_ddl_create_table_column_deinit(&column);
        return MYLITE_NOMEM;
    }

    plan->columns = columns;
    plan->columns[plan->column_count++] = column;
    return MYLITE_OK;
}

int mylite_table_ddl_copy_create_table_index(const struct mylite_sql_ast_node *index_node,
                                             struct mylite_create_table_plan *plan)
{
    struct mylite_create_table_index index = {
        .algorithm = MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE,
        .is_visible = true,
    };
    const struct mylite_sql_ast_node *child = NULL;
    const struct mylite_sql_ast_node *key_parts =
        mylite_ast_find_child_kind(index_node, MYLITE_SQL_AST_KEY_PART_LIST);
    const struct mylite_sql_ast_node *options =
        mylite_ast_find_child_kind(index_node, MYLITE_SQL_AST_INDEX_OPTION_LIST);
    int status = MYLITE_OK;

    index.is_primary = index_node->kind == MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT;
    if (index.is_primary) {
        index.is_unique = true;
    } else {
        index.is_unique = index_node->kind == MYLITE_SQL_AST_UNIQUE_INDEX;
    }

    for (child = index_node->first_child; child != NULL && child != key_parts;
         child = child->next_sibling) {
        if (child->kind == MYLITE_SQL_AST_IDENTIFIER) {
            free(index.name);
            index.name = mylite_copy_identifier_span(child);
            index.explicit_name = true;
            if (index.name == NULL) {
                mylite_table_ddl_create_table_index_deinit(&index);
                return MYLITE_NOMEM;
            }
        } else if (child->kind == MYLITE_SQL_AST_INDEX_TYPE) {
            index.algorithm = child->index_algorithm;
        }
    }
    if (index.is_primary) {
        free(index.name);
        index.name = mylite_copy_span_text("PRIMARY", strlen("PRIMARY"));
        index.explicit_name = true;
        if (index.name == NULL) {
            mylite_table_ddl_create_table_index_deinit(&index);
            return MYLITE_NOMEM;
        }
    }

    status = mylite_table_ddl_copy_create_table_key_parts(key_parts, &index);
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_copy_create_table_index_options(options, &index);
    }
    if (status == MYLITE_OK) {
        status = add_create_table_index(plan, index);
    }
    if (status != MYLITE_OK) {
        mylite_table_ddl_create_table_index_deinit(&index);
    }
    return status;
}

int mylite_table_ddl_copy_create_table_key_parts(const struct mylite_sql_ast_node *key_parts,
                                                 struct mylite_create_table_index *index)
{
    const struct mylite_sql_ast_node *part_node = NULL;

    if (key_parts == NULL || key_parts->first_child == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    for (part_node = key_parts->first_child; part_node != NULL;
         part_node = part_node->next_sibling) {
        struct mylite_create_table_key_part *parts = NULL;
        struct mylite_create_table_key_part part = {
            .order = part_node->key_part_order,
        };
        const struct mylite_sql_ast_node *prefix = mylite_ast_child_at(part_node, 1U);

        part.column_name = mylite_copy_identifier_span(mylite_ast_child_at(part_node, 0U));
        if (part.column_name == NULL) {
            return MYLITE_NOMEM;
        }
        if (prefix != NULL) {
            part.has_prefix_length = true;
            part.prefix_length = prefix->column_length;
        }

        parts = realloc(index->parts, (index->part_count + 1U) * sizeof(*index->parts));
        if (parts == NULL) {
            mylite_table_ddl_create_table_key_part_deinit(&part);
            return MYLITE_NOMEM;
        }
        index->parts = parts;
        index->parts[index->part_count++] = part;
    }
    return MYLITE_OK;
}

int mylite_table_ddl_copy_create_table_index_options(const struct mylite_sql_ast_node *options,
                                                     struct mylite_create_table_index *index)
{
    const struct mylite_sql_ast_node *option = NULL;

    for (option = options == NULL ? NULL : options->first_child; option != NULL;
         option = option->next_sibling) {
        char *copy = NULL;

        switch (option->index_option) {
        case MYLITE_SQL_AST_INDEX_OPTION_USING:
            index->algorithm = option->index_algorithm;
            break;
        case MYLITE_SQL_AST_INDEX_OPTION_COMMENT:
            copy = mylite_copy_string_literal_span(mylite_ast_child_at(option, 0U));
            if (copy == NULL) {
                return MYLITE_NOMEM;
            }
            free(index->comment);
            index->comment = copy;
            break;
        case MYLITE_SQL_AST_INDEX_OPTION_VISIBLE:
            index->is_visible = true;
            break;
        case MYLITE_SQL_AST_INDEX_OPTION_INVISIBLE:
            index->is_visible = false;
            break;
        case MYLITE_SQL_AST_INDEX_OPTION_KEY_BLOCK_SIZE:
        case MYLITE_SQL_AST_INDEX_OPTION_ENGINE_ATTRIBUTE:
        case MYLITE_SQL_AST_INDEX_OPTION_SECONDARY_ENGINE_ATTRIBUTE:
        case MYLITE_SQL_AST_INDEX_OPTION_NONE:
            break;
        case MYLITE_SQL_AST_INDEX_OPTION_WITH_PARSER:
            index->has_with_parser = true;
            break;
        }
    }
    return MYLITE_OK;
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

const struct mylite_create_table_column *
mylite_table_ddl_find_create_table_column(const struct mylite_create_table_plan *plan,
                                          const char *name)
{
    for (size_t index = 0U; index < plan->column_count; ++index) {
        if (mylite_ascii_case_equal(plan->columns[index].name, name)) {
            return &plan->columns[index];
        }
    }
    return NULL;
}

void mylite_table_ddl_create_table_plan_deinit(struct mylite_create_table_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    free(plan->schema_name);
    free(plan->table_name);
    create_table_options_deinit(&plan->options);
    for (size_t index = 0U; index < plan->column_count; ++index) {
        mylite_table_ddl_create_table_column_deinit(&plan->columns[index]);
    }
    free(plan->columns);
    for (size_t index = 0U; index < plan->index_count; ++index) {
        mylite_table_ddl_create_table_index_deinit(&plan->indexes[index]);
    }
    free(plan->indexes);
    *plan = (struct mylite_create_table_plan){0};
}

void mylite_table_ddl_drop_table_plan_deinit(struct mylite_drop_table_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    for (size_t index = 0U; index < plan->target_count; ++index) {
        mylite_table_ddl_drop_table_target_deinit(&plan->targets[index]);
    }
    free(plan->targets);
    *plan = (struct mylite_drop_table_plan){0};
}

void mylite_table_ddl_rename_table_plan_deinit(struct mylite_rename_table_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    for (size_t index = 0U; index < plan->target_count; ++index) {
        mylite_table_ddl_rename_table_target_deinit(&plan->targets[index]);
    }
    free(plan->targets);
    *plan = (struct mylite_rename_table_plan){0};
}

void mylite_table_ddl_truncate_table_plan_deinit(struct mylite_truncate_table_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    free(plan->schema_name);
    free(plan->table_name);
    *plan = (struct mylite_truncate_table_plan){0};
}

void mylite_table_ddl_alter_table_plan_deinit(struct mylite_alter_table_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    free(plan->schema_name);
    free(plan->table_name);
    for (size_t index = 0U; index < plan->action_count; ++index) {
        mylite_table_ddl_alter_table_action_deinit(&plan->actions[index]);
    }
    free(plan->actions);
    free(plan->unsupported_algorithm);
    free(plan->unsupported_lock);
    *plan = (struct mylite_alter_table_plan){0};
}

void mylite_table_ddl_index_ddl_plan_deinit(struct mylite_index_ddl_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    free(plan->schema_name);
    free(plan->table_name);
    free(plan->index_name);
    mylite_table_ddl_create_table_index_deinit(&plan->index);
    *plan = (struct mylite_index_ddl_plan){0};
}

void mylite_table_ddl_drop_table_target_deinit(struct mylite_drop_table_target *target)
{
    if (target == NULL) {
        return;
    }

    free(target->schema_name);
    free(target->table_name);
    *target = (struct mylite_drop_table_target){0};
}

void mylite_table_ddl_rename_table_target_deinit(struct mylite_rename_table_target *target)
{
    if (target == NULL) {
        return;
    }

    free(target->source_schema_name);
    free(target->source_table_name);
    free(target->target_schema_name);
    free(target->target_table_name);
    *target = (struct mylite_rename_table_target){0};
}

void mylite_table_ddl_alter_table_model_deinit(struct mylite_alter_table_model *model)
{
    if (model == NULL) {
        return;
    }

    free(model->schema_name);
    free(model->table_name);
    free(model->physical_name);
    free(model->table_collation);
    for (size_t index = 0U; index < model->column_count; ++index) {
        mylite_table_ddl_alter_table_column_deinit(&model->columns[index]);
    }
    free(model->columns);
    for (size_t index = 0U; index < model->index_count; ++index) {
        mylite_table_ddl_alter_table_index_deinit(&model->indexes[index]);
    }
    free(model->indexes);
    *model = (struct mylite_alter_table_model){0};
}

void mylite_table_ddl_alter_table_column_deinit(struct mylite_alter_table_column *column)
{
    if (column == NULL) {
        return;
    }

    free(column->name);
    free(column->source_name);
    free(column->column_default);
    free(column->is_nullable);
    free(column->data_type);
    free(column->character_set_name);
    free(column->collation_name);
    free(column->column_type);
    free(column->column_key);
    free(column->extra);
    free(column->column_comment);
    free(column->generation_expression);
    *column = (struct mylite_alter_table_column){0};
}

void mylite_table_ddl_alter_table_index_deinit(struct mylite_alter_table_index *index)
{
    if (index == NULL) {
        return;
    }

    free(index->index_schema);
    free(index->name);
    free(index->index_type);
    free(index->comment);
    free(index->index_comment);
    free(index->is_visible);
    for (size_t part = 0U; part < index->part_count; ++part) {
        mylite_table_ddl_alter_table_index_part_deinit(&index->parts[part]);
    }
    free(index->parts);
    *index = (struct mylite_alter_table_index){0};
}

void mylite_table_ddl_alter_table_index_part_deinit(struct mylite_alter_table_index_part *part)
{
    if (part == NULL) {
        return;
    }

    free(part->column_name);
    free(part->collation);
    free(part->nullable);
    *part = (struct mylite_alter_table_index_part){0};
}

void mylite_table_ddl_create_table_column_deinit(struct mylite_create_table_column *column)
{
    if (column == NULL) {
        return;
    }

    free(column->name);
    free(column->type.character_set);
    free(column->type.collation);
    free(column->default_text);
    free(column->comment);
    *column = (struct mylite_create_table_column){0};
}

void mylite_table_ddl_create_table_index_deinit(struct mylite_create_table_index *index)
{
    if (index == NULL) {
        return;
    }

    free(index->name);
    free(index->comment);
    for (size_t part = 0U; part < index->part_count; ++part) {
        mylite_table_ddl_create_table_key_part_deinit(&index->parts[part]);
    }
    free(index->parts);
    *index = (struct mylite_create_table_index){0};
}

void mylite_table_ddl_create_table_key_part_deinit(struct mylite_create_table_key_part *part)
{
    if (part == NULL) {
        return;
    }

    free(part->column_name);
    *part = (struct mylite_create_table_key_part){0};
}

void mylite_table_ddl_alter_table_action_deinit(struct mylite_alter_table_action *action)
{
    if (action == NULL) {
        return;
    }

    free(action->old_name);
    free(action->new_name);
    free(action->new_schema_name);
    free(action->after_column);
    mylite_table_ddl_create_table_column_deinit(&action->column);
    mylite_table_ddl_create_table_index_deinit(&action->index);
    *action = (struct mylite_alter_table_action){0};
}

static int copy_create_table_elements(const struct mylite_sql_ast_node *elements,
                                      struct mylite_create_table_plan *plan)
{
    const struct mylite_sql_ast_node *element = NULL;
    int status = MYLITE_OK;

    if (elements == NULL || elements->first_child == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    for (element = elements->first_child; element != NULL; element = element->next_sibling) {
        if (element->kind == MYLITE_SQL_AST_COLUMN_DEFINITION) {
            size_t column_index = plan->column_count;

            status = mylite_table_ddl_copy_create_table_column(element, plan);
            if (status == MYLITE_OK) {
                status = add_inline_create_table_column_indexes(plan, &plan->columns[column_index]);
            }
        } else if (element->kind == MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT ||
                   element->kind == MYLITE_SQL_AST_UNIQUE_INDEX ||
                   element->kind == MYLITE_SQL_AST_SECONDARY_INDEX) {
            status = mylite_table_ddl_copy_create_table_index(element, plan);
        } else {
            status = MYLITE_UNSUPPORTED;
        }
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int copy_create_table_column_type(const struct mylite_sql_ast_node *type_node,
                                         struct mylite_create_table_column_type *type)
{
    if (type_node == NULL || type_node->kind != MYLITE_SQL_AST_COLUMN_TYPE) {
        return MYLITE_UNSUPPORTED;
    }

    *type = (struct mylite_create_table_column_type){
        .ast_type = type_node->column_type,
        .attributes =
            {
                .display_width = type_node->column_display_width,
                .length = type_node->column_length,
                .precision = type_node->column_precision,
                .scale = type_node->column_scale,
                .has_display_width = type_node->has_column_display_width,
                .has_signed = type_node->column_type_signed,
                .has_unsigned = type_node->column_type_unsigned,
                .has_length = type_node->has_column_length,
                .has_precision = type_node->has_column_precision,
                .has_scale = type_node->has_column_scale,
                .has_binary_attribute = type_node->column_binary_attribute,
                .has_byte_attribute = type_node->column_byte_attribute,
                .has_zerofill_attribute = type_node->column_zerofill_attribute,
                .is_national = type_node->column_national_attribute,
            },
    };
    if (type_node->has_column_character_set) {
        type->character_set = mylite_copy_span_text(type_node->column_character_set.text,
                                                    type_node->column_character_set.length);
        if (type->character_set == NULL) {
            return MYLITE_NOMEM;
        }
        type->attributes.has_character_set = true;
        type->attributes.character_set = type->character_set;
        type->attributes.character_set_length = strlen(type->character_set);
    }
    if (type_node->has_column_collation) {
        type->collation = mylite_copy_span_text(type_node->column_collation.text,
                                                type_node->column_collation.length);
        if (type->collation == NULL) {
            return MYLITE_NOMEM;
        }
        type->attributes.has_collation = true;
        type->attributes.collation = type->collation;
        type->attributes.collation_length = strlen(type->collation);
    }
    return MYLITE_OK;
}

static int copy_create_table_column_attributes(const struct mylite_sql_ast_node *attributes,
                                               struct mylite_create_table_column *column)
{
    const struct mylite_sql_ast_node *attribute = NULL;

    for (attribute = attributes == NULL ? NULL : attributes->first_child; attribute != NULL;
         attribute = attribute->next_sibling) {
        char *copy = NULL;

        switch (attribute->column_attribute) {
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NULL:
            column->nullable = true;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NOT_NULL:
            column->nullable = false;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_DEFAULT:
            copy = copy_expression_text(mylite_ast_child_at(attribute, 0U));
            if (copy == NULL && mylite_ast_child_at(attribute, 0U) != NULL &&
                mylite_ast_child_at(attribute, 0U)->literal_kind != MYLITE_SQL_AST_LITERAL_NULL) {
                return MYLITE_NOMEM;
            }
            free(column->default_text);
            column->default_text = copy;
            if (mylite_ast_child_at(attribute, 0U) != NULL &&
                (mylite_ast_child_at(attribute, 0U)->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP ||
                 mylite_ast_child_at(attribute, 0U)->kind ==
                     MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION)) {
                column->has_generated_default = true;
            }
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_ON_UPDATE:
            column->has_on_update_current_timestamp = true;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_COMMENT:
            copy = mylite_copy_string_literal_span(mylite_ast_child_at(attribute, 0U));
            if (copy == NULL) {
                return MYLITE_NOMEM;
            }
            free(column->comment);
            column->comment = copy;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_VISIBLE:
            column->visible = true;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_INVISIBLE:
            column->visible = false;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_AUTO_INCREMENT:
            column->auto_increment = true;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_PRIMARY_KEY:
            column->primary_key = true;
            column->nullable = false;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_UNIQUE_KEY:
            column->unique_key = true;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_COLUMN_FORMAT:
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_STORAGE:
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NONE:
            break;
        }
    }
    return MYLITE_OK;
}

static int copy_create_table_options(const struct mylite_sql_ast_node *statement,
                                     struct mylite_create_table_options *options)
{
    const struct mylite_sql_ast_node *option_list =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_TABLE_OPTION_LIST);
    const struct mylite_sql_ast_node *option = NULL;

    for (option = option_list == NULL ? NULL : option_list->first_child; option != NULL;
         option = option->next_sibling) {
        char *copy = NULL;

        switch (option->table_option) {
        case MYLITE_SQL_AST_TABLE_OPTION_ENGINE:
            copy = mylite_copy_identifier_span(mylite_ast_child_at(option, 0U));
            if (copy == NULL) {
                return MYLITE_NOMEM;
            }
            free(options->engine);
            options->engine = copy;
            break;
        case MYLITE_SQL_AST_TABLE_OPTION_CHARACTER_SET:
            if (mylite_ast_child_at(option, 0U) != NULL &&
                mylite_ast_child_at(option, 0U)->kind == MYLITE_SQL_AST_DEFAULT) {
                free(options->character_set);
                options->character_set = NULL;
                break;
            }
            copy = mylite_copy_schema_text_span(mylite_ast_child_at(option, 0U));
            if (copy == NULL) {
                return MYLITE_NOMEM;
            }
            free(options->character_set);
            options->character_set = copy;
            break;
        case MYLITE_SQL_AST_TABLE_OPTION_COLLATE:
            if (mylite_ast_child_at(option, 0U) != NULL &&
                mylite_ast_child_at(option, 0U)->kind == MYLITE_SQL_AST_DEFAULT) {
                free(options->collation);
                options->collation = NULL;
                break;
            }
            copy = mylite_copy_schema_text_span(mylite_ast_child_at(option, 0U));
            if (copy == NULL) {
                return MYLITE_NOMEM;
            }
            free(options->collation);
            options->collation = copy;
            break;
        case MYLITE_SQL_AST_TABLE_OPTION_COMMENT:
            copy = mylite_copy_string_literal_span(mylite_ast_child_at(option, 0U));
            if (copy == NULL) {
                return MYLITE_NOMEM;
            }
            free(options->comment);
            options->comment = copy;
            break;
        case MYLITE_SQL_AST_TABLE_OPTION_AUTO_INCREMENT:
            options->has_auto_increment = true;
            options->auto_increment = mylite_ast_child_at(option, 0U)->column_length;
            break;
        case MYLITE_SQL_AST_TABLE_OPTION_NONE:
            break;
        }
    }
    return MYLITE_OK;
}

static int add_create_table_index(struct mylite_create_table_plan *plan,
                                  struct mylite_create_table_index index)
{
    struct mylite_create_table_index *indexes =
        realloc(plan->indexes, (plan->index_count + 1U) * sizeof(*plan->indexes));

    if (indexes == NULL) {
        return MYLITE_NOMEM;
    }

    plan->indexes = indexes;
    plan->indexes[plan->index_count++] = index;
    return MYLITE_OK;
}

static int add_inline_create_table_column_indexes(struct mylite_create_table_plan *plan,
                                                  const struct mylite_create_table_column *column)
{
    int status = MYLITE_OK;

    if (column->primary_key) {
        status = add_single_column_index(plan, column->name, true, true);
    }
    if (status == MYLITE_OK && column->unique_key) {
        status = add_single_column_index(plan, column->name, false, true);
    }
    return status;
}

static int add_single_column_index(struct mylite_create_table_plan *plan, const char *column_name,
                                   bool is_primary, bool is_unique)
{
    struct mylite_create_table_index index = {
        .algorithm = MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE,
        .is_primary = is_primary,
        .is_unique = is_unique,
        .is_visible = true,
        .explicit_name = is_primary,
        .part_count = 1U,
    };

    if (is_primary) {
        index.name = mylite_copy_span_text("PRIMARY", strlen("PRIMARY"));
    }
    index.parts = calloc(1U, sizeof(*index.parts));
    if ((is_primary && index.name == NULL) || index.parts == NULL) {
        mylite_table_ddl_create_table_index_deinit(&index);
        return MYLITE_NOMEM;
    }
    index.parts[0].column_name = mylite_copy_span_text(column_name, strlen(column_name));
    if (index.parts[0].column_name == NULL) {
        mylite_table_ddl_create_table_index_deinit(&index);
        return MYLITE_NOMEM;
    }

    int status = add_create_table_index(plan, index);
    if (status != MYLITE_OK) {
        mylite_table_ddl_create_table_index_deinit(&index);
    }
    return status;
}

static char *copy_expression_text(const struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return NULL;
    }
    if (node->kind == MYLITE_SQL_AST_LITERAL &&
        node->literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        return mylite_copy_string_literal_span(node);
    }
    if (node->kind == MYLITE_SQL_AST_LITERAL && node->literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        return NULL;
    }
    if (node->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP) {
        return mylite_copy_span_text("CURRENT_TIMESTAMP", strlen("CURRENT_TIMESTAMP"));
    }
    return mylite_copy_span_text(node->span.text, node->span.length);
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
            if (mylite_table_ddl_find_create_table_column(
                    plan, table_index->parts[part].column_name) == NULL) {
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

static int assign_generated_index_names(mylite_db *database, struct mylite_create_table_plan *plan)
{
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

static void create_table_options_deinit(struct mylite_create_table_options *options)
{
    if (options == NULL) {
        return;
    }

    free(options->engine);
    free(options->character_set);
    free(options->collation);
    free(options->comment);
    *options = (struct mylite_create_table_options){0};
}
