#include "mylite_table_ddl.h"

#include "mylite_catalog.h"
#include "mylite_charset.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_table_ddl_catalog.h"
#include "mylite_table_ddl_create_sql.h"
#include "mylite_table_ddl_plan_lookup.h"
#include "mylite_transactions.h"

#include <mylite/mylite.h>

#include <stdlib.h>
#include <string.h>

static int validate_create_table_plan(mylite_db *database, const char *schema_name,
                                      struct mylite_create_table_plan *plan, bool if_not_exists,
                                      struct mylite_schema_default *schema_default,
                                      bool *out_skip_create);
static int create_table_transaction(mylite_db *database, const char *schema_name,
                                    const struct mylite_schema_default *schema_default,
                                    const struct mylite_create_table_plan *plan);
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

    status = mylite_table_ddl_create_physical_table(database, schema_name, schema_default, plan);
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_insert_create_table_catalog_rows(database, schema_name,
                                                                   schema_default, plan);
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
