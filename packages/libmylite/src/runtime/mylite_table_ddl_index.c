#include "mylite_table_ddl.h"

#include "mylite_runtime.h"
#include "mylite_table_ddl_alter_index.h"
#include "mylite_table_ddl_alter_index_model.h"
#include "mylite_table_ddl_index_catalog.h"
#include "mylite_table_ddl_index_validate.h"
#include "mylite_table_ddl_index_warnings.h"
#include "mylite_transactions.h"

#include <mylite/mylite.h>

#include <stddef.h>

static int apply_create_index_to_model(
    mylite_db *database,
    struct mylite_index_ddl_plan *plan,
    struct mylite_alter_table_model *model,
    const struct mylite_alter_table_index **out_index
);

static int apply_drop_index_to_model(
    mylite_db *database,
    struct mylite_index_ddl_plan *plan,
    struct mylite_alter_table_model *model
);

static int commit_index_ddl_implicit_transaction(mylite_db *database);

int mylite_table_ddl_execute_create_index_statement(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_index_ddl_plan *plan
) {
    const struct mylite_alter_table_index *created_index = NULL;
    struct mylite_alter_table_model model = {0};
    int status = MYLITE_OK;

    if (database == NULL || plan == NULL) {
        return MYLITE_MISUSE;
    }

    status = commit_index_ddl_implicit_transaction(database);
    if (status == MYLITE_OK) {
        status =
            mylite_table_ddl_validate_create_index_plan(database, selected_schema, plan, &model);
    }
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_validate_create_index_columns(database, &model, &plan->index);
    }
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_validate_create_index_supported_features(database, plan, &model);
    }
    if (status == MYLITE_OK && plan->index_class == MYLITE_SQL_AST_INDEX_CLASS_UNIQUE) {
        status =
            mylite_table_ddl_validate_create_unique_index_values(database, &model, &plan->index);
    }
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_append_create_index_warnings(database, &model, &plan->index);
    }
    if (status == MYLITE_OK) {
        status = apply_create_index_to_model(database, plan, &model, &created_index);
    }
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_create_index_catalog_transaction(database, &model, created_index);
    }

    mylite_table_ddl_alter_table_model_deinit(&model);
    return status;
}

int mylite_table_ddl_execute_drop_index_statement(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_index_ddl_plan *plan
) {
    struct mylite_alter_table_model model = {0};
    int status = MYLITE_OK;

    if (database == NULL || plan == NULL) {
        return MYLITE_MISUSE;
    }

    status = commit_index_ddl_implicit_transaction(database);
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_validate_drop_index_plan(database, selected_schema, plan, &model);
    }
    if (status == MYLITE_OK) {
        status = apply_drop_index_to_model(database, plan, &model);
    }
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_drop_index_catalog_transaction(database, plan, &model);
    }

    mylite_table_ddl_alter_table_model_deinit(&model);
    return status;
}

static int commit_index_ddl_implicit_transaction(mylite_db *database) {
    if (!database->transaction_active) {
        return MYLITE_OK;
    }

    return mylite_transaction_commit_explicit(database);
}

static int apply_create_index_to_model(
    mylite_db *database,
    struct mylite_index_ddl_plan *plan,
    struct mylite_alter_table_model *model,
    const struct mylite_alter_table_index **out_index
) {
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

    index = mylite_table_ddl_alter_table_index_index(model, plan->index.name);
    if (index == model->index_count) {
        return MYLITE_MISUSE;
    }
    *out_index = &model->indexes[index];
    return MYLITE_OK;
}

static int apply_drop_index_to_model(
    mylite_db *database,
    struct mylite_index_ddl_plan *plan,
    struct mylite_alter_table_model *model
) {
    const struct mylite_alter_table_action action = {
        .kind = MYLITE_ALTER_TABLE_ACTION_DROP_INDEX,
        .old_name = plan->index_name,
    };

    return mylite_table_ddl_apply_alter_table_index_action(database, &action, model, NULL);
}
