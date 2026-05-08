#include "mylite_table_ddl.h"

#include "mylite_diagnostics.h"
#include "mylite_schema_types.h"
#include "mylite_table_ddl_catalog.h"
#include "mylite_table_ddl_create_sql.h"
#include "mylite_table_ddl_create_validate.h"
#include "mylite_transactions.h"

#include <mylite/mylite.h>

#include <string.h>

static int create_table_transaction(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_plan *plan
);

static int commit_create_table_implicit_transaction(
    mylite_db *database,
    const struct mylite_create_table_plan *plan
);

static const char *create_table_column_type_name(enum mylite_sql_ast_column_type column_type);

static bool create_table_column_uses_integer_descriptor(
    enum mylite_sql_ast_column_type column_type
);

static bool create_table_column_uses_string_binary_descriptor(
    enum mylite_sql_ast_column_type column_type
);

static bool create_table_column_uses_character_set_defaults(
    enum mylite_sql_ast_column_type column_type
);

static bool create_table_column_uses_numeric_descriptor(
    enum mylite_sql_ast_column_type column_type
);

static bool create_table_column_uses_temporal_descriptor(
    enum mylite_sql_ast_column_type column_type
);

int mylite_table_ddl_execute_create_table_statement(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_create_table_plan *plan,
    bool if_not_exists
) {
    const char *schema_name = plan->schema_name == NULL ? selected_schema : plan->schema_name;
    struct mylite_schema_default schema_default;
    bool skip_create = false;
    int status = MYLITE_OK;

    status = commit_create_table_implicit_transaction(database, plan);
    if (status != MYLITE_OK) {
        return status;
    }
    if (plan->like) {
        return mylite_table_ddl_execute_create_table_like_statement(
            database,
            selected_schema,
            plan,
            if_not_exists
        );
    }
    if (plan->select) {
        return mylite_table_ddl_execute_create_table_select_statement(
            database,
            selected_schema,
            plan,
            if_not_exists
        );
    }

    if (schema_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "No database selected");
        return MYLITE_EXEC_ERROR;
    }

    status = mylite_table_ddl_validate_create_table_plan(
        database,
        schema_name,
        plan,
        if_not_exists,
        &schema_default,
        &skip_create
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (skip_create) {
        return MYLITE_OK;
    }

    return create_table_transaction(database, schema_name, &schema_default, plan);
}

static int commit_create_table_implicit_transaction(
    mylite_db *database,
    const struct mylite_create_table_plan *plan
) {
    if (plan->temporary || !database->transaction_active) {
        return MYLITE_OK;
    }

    return mylite_transaction_commit_explicit(database);
}

static int create_table_transaction(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_plan *plan
) {
    int status = mylite_transaction_begin_storage(database);

    if (status != MYLITE_OK) {
        return status;
    }

    status = mylite_table_ddl_create_physical_table(database, schema_name, schema_default, plan);
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_insert_create_table_catalog_rows(
            database,
            schema_name,
            schema_default,
            plan
        );
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
    struct mylite_column_type_descriptor *out_descriptor
) {
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
        status = mylite_column_type_describe_integer(
            type_name,
            strlen(type_name),
            attributes,
            out_descriptor
        );
    } else if (create_table_column_uses_string_binary_descriptor(column->type.ast_type)) {
        status = mylite_column_type_describe_string_binary(
            type_name,
            strlen(type_name),
            attributes,
            out_descriptor
        );
    } else if (create_table_column_uses_numeric_descriptor(column->type.ast_type)) {
        status = mylite_column_type_describe_numeric(
            type_name,
            strlen(type_name),
            attributes,
            out_descriptor
        );
    } else if (create_table_column_uses_temporal_descriptor(column->type.ast_type)) {
        status = mylite_column_type_describe_temporal(
            type_name,
            strlen(type_name),
            attributes,
            out_descriptor
        );
    } else {
        return MYLITE_UNSUPPORTED;
    }

    return status == MYLITE_COLUMN_TYPE_OK ? MYLITE_OK : MYLITE_EXEC_ERROR;
}

const char *mylite_table_ddl_create_table_column_extra(
    const struct mylite_create_table_column *column
) {
    if (column->auto_increment) {
        return "auto_increment";
    }
    if (column->generated_column_storage == MYLITE_SQL_AST_GENERATED_COLUMN_STORAGE_STORED &&
        !column->visible) {
        return "STORED GENERATED INVISIBLE";
    }
    if (column->generated_column_storage == MYLITE_SQL_AST_GENERATED_COLUMN_STORAGE_STORED) {
        return "STORED GENERATED";
    }
    if (column->generated_column_storage == MYLITE_SQL_AST_GENERATED_COLUMN_STORAGE_VIRTUAL &&
        !column->visible) {
        return "VIRTUAL GENERATED INVISIBLE";
    }
    if (column->generated_column_storage == MYLITE_SQL_AST_GENERATED_COLUMN_STORAGE_VIRTUAL) {
        return "VIRTUAL GENERATED";
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

const char *mylite_table_ddl_index_collation_for_order(enum mylite_sql_ast_key_part_order order) {
    return order == MYLITE_SQL_AST_KEY_PART_ORDER_DESC ? "D" : "A";
}

static const char *create_table_column_type_name(enum mylite_sql_ast_column_type column_type) {
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
    case MYLITE_SQL_AST_COLUMN_TYPE_SERIAL:
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

static bool create_table_column_uses_integer_descriptor(
    enum mylite_sql_ast_column_type column_type
) {
    if (column_type == MYLITE_SQL_AST_COLUMN_TYPE_SERIAL) {
        return true;
    }
    if (column_type < MYLITE_SQL_AST_COLUMN_TYPE_TINYINT) {
        return false;
    }
    if (column_type > MYLITE_SQL_AST_COLUMN_TYPE_BOOLEAN) {
        return false;
    }
    return true;
}

static bool create_table_column_uses_string_binary_descriptor(
    enum mylite_sql_ast_column_type column_type
) {
    if (column_type < MYLITE_SQL_AST_COLUMN_TYPE_CHAR) {
        return false;
    }
    if (column_type > MYLITE_SQL_AST_COLUMN_TYPE_LONGBLOB) {
        return false;
    }
    return true;
}

static bool create_table_column_uses_character_set_defaults(
    enum mylite_sql_ast_column_type column_type
) {
    if (column_type < MYLITE_SQL_AST_COLUMN_TYPE_CHAR) {
        return false;
    }
    if (column_type > MYLITE_SQL_AST_COLUMN_TYPE_LONGTEXT) {
        return false;
    }
    return true;
}

static bool create_table_column_uses_numeric_descriptor(
    enum mylite_sql_ast_column_type column_type
) {
    if (column_type < MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL) {
        return false;
    }
    if (column_type > MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE) {
        return false;
    }
    return true;
}

static bool create_table_column_uses_temporal_descriptor(
    enum mylite_sql_ast_column_type column_type
) {
    if (column_type < MYLITE_SQL_AST_COLUMN_TYPE_DATE) {
        return false;
    }
    if (column_type > MYLITE_SQL_AST_COLUMN_TYPE_YEAR) {
        return false;
    }
    return true;
}
