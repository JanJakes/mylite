#include "mylite_table_ddl.h"

#include "mylite_span.h"

#include <stdlib.h>

static int copy_index_ddl_table_name(
    const struct mylite_sql_ast_node *table_name,
    struct mylite_index_ddl_plan *plan
);

static int copy_drop_table_target(
    const struct mylite_sql_ast_node *table_name,
    struct mylite_drop_table_target *target
);

static int add_drop_table_target(
    struct mylite_drop_table_plan *plan,
    struct mylite_drop_table_target target
);

static int copy_rename_table_pair(
    const struct mylite_sql_ast_node *pair,
    struct mylite_rename_table_target *target
);

int mylite_table_ddl_copy_create_index_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_index_ddl_plan *plan
) {
    const struct mylite_sql_ast_node *child = statement == NULL ? NULL : statement->first_child;
    const struct mylite_sql_ast_node *index_name = child;
    const struct mylite_sql_ast_node *pre_index_type = NULL;
    const struct mylite_sql_ast_node *table_name = NULL;
    const struct mylite_sql_ast_node *key_parts = NULL;
    const struct mylite_sql_ast_node *options = NULL;
    int status = MYLITE_OK;

    if (statement == NULL || plan == NULL) {
        return MYLITE_MISUSE;
    }

    child = child == NULL ? NULL : child->next_sibling;
    if (child != NULL && child->kind == MYLITE_SQL_AST_INDEX_TYPE) {
        pre_index_type = child;
        child = child->next_sibling;
    }
    table_name = child;
    child = child == NULL ? NULL : child->next_sibling;
    key_parts = child;
    child = child == NULL ? NULL : child->next_sibling;
    options = child;

    plan->index_class = statement->index_class;
    plan->index = (struct mylite_create_table_index){
        .algorithm = MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE,
        .is_unique = statement->index_class == MYLITE_SQL_AST_INDEX_CLASS_UNIQUE,
        .is_visible = true,
        .explicit_name = true,
    };

    status = copy_index_ddl_table_name(table_name, plan);
    if (status != MYLITE_OK) {
        return status;
    }

    plan->index.name = mylite_copy_identifier_span(index_name);
    if (plan->index.name == NULL) {
        return MYLITE_NOMEM;
    }
    if (pre_index_type != NULL) {
        plan->index.algorithm = pre_index_type->index_algorithm;
        plan->index.display_index_type =
            pre_index_type->index_algorithm == MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE;
    }
    status = mylite_table_ddl_copy_create_table_key_parts(key_parts, &plan->index);
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_copy_create_table_index_options(options, &plan->index);
    }
    return status;
}

int mylite_table_ddl_copy_drop_index_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_index_ddl_plan *plan
) {
    const struct mylite_sql_ast_node *index_name = NULL;
    const struct mylite_sql_ast_node *table_name = NULL;
    int status = MYLITE_OK;

    if (statement == NULL || plan == NULL) {
        return MYLITE_MISUSE;
    }

    index_name = mylite_ast_child_at(statement, 0U);
    table_name = mylite_ast_child_at(statement, 1U);
    status = copy_index_ddl_table_name(table_name, plan);
    if (status != MYLITE_OK) {
        return status;
    }

    plan->index_name = mylite_copy_identifier_span(index_name);
    return plan->index_name == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static int copy_index_ddl_table_name(
    const struct mylite_sql_ast_node *table_name,
    struct mylite_index_ddl_plan *plan
) {
    struct mylite_create_table_plan table_plan = {0};
    int status = mylite_table_ddl_copy_create_table_name(table_name, &table_plan);

    if (status != MYLITE_OK) {
        return status;
    }

    plan->schema_name = table_plan.schema_name;
    plan->table_name = table_plan.table_name;
    table_plan.schema_name = NULL;
    table_plan.table_name = NULL;
    mylite_table_ddl_create_table_plan_deinit(&table_plan);
    return MYLITE_OK;
}

int mylite_table_ddl_copy_drop_table_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_drop_table_plan *plan
) {
    const struct mylite_sql_ast_node *table_names = mylite_ast_child_at(statement, 0U);

    plan->temporary = statement->drop_table_temporary;
    plan->restrict_mode = statement->drop_table_restrict;
    plan->cascade_mode = statement->drop_table_cascade;

    for (const struct mylite_sql_ast_node *table_name =
             table_names == NULL ? NULL : table_names->first_child;
         table_name != NULL;
         table_name = table_name->next_sibling) {
        struct mylite_drop_table_target target = {0};
        int status = copy_drop_table_target(table_name, &target);

        if (status == MYLITE_OK) {
            status = add_drop_table_target(plan, target);
        }
        if (status != MYLITE_OK) {
            mylite_table_ddl_drop_table_target_deinit(&target);
            return status;
        }
    }
    return plan->target_count == 0U ? MYLITE_UNSUPPORTED : MYLITE_OK;
}

static int copy_drop_table_target(
    const struct mylite_sql_ast_node *table_name,
    struct mylite_drop_table_target *target
) {
    if (table_name == NULL) {
        return MYLITE_NOMEM;
    }
    if (table_name->kind == MYLITE_SQL_AST_IDENTIFIER) {
        target->table_name = mylite_copy_identifier_span(table_name);
        return target->table_name == NULL ? MYLITE_NOMEM : MYLITE_OK;
    }
    if (table_name->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER &&
        mylite_ast_child_at(table_name, 0U) != NULL &&
        mylite_ast_child_at(table_name, 1U) != NULL &&
        mylite_ast_child_at(table_name, 0U)->kind == MYLITE_SQL_AST_IDENTIFIER &&
        mylite_ast_child_at(table_name, 1U)->kind == MYLITE_SQL_AST_IDENTIFIER) {
        target->schema_name = mylite_copy_identifier_span(mylite_ast_child_at(table_name, 0U));
        if (target->schema_name == NULL) {
            return MYLITE_NOMEM;
        }
        target->table_name = mylite_copy_identifier_span(mylite_ast_child_at(table_name, 1U));
        if (target->table_name == NULL) {
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }
    return MYLITE_UNSUPPORTED;
}

static int add_drop_table_target(
    struct mylite_drop_table_plan *plan,
    struct mylite_drop_table_target target
) {
    struct mylite_drop_table_target *targets =
        realloc(plan->targets, sizeof(*plan->targets) * (plan->target_count + 1U));

    if (targets == NULL) {
        return MYLITE_NOMEM;
    }

    plan->targets = targets;
    plan->targets[plan->target_count] = target;
    ++plan->target_count;
    return MYLITE_OK;
}

int mylite_table_ddl_copy_rename_table_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_rename_table_plan *plan
) {
    const struct mylite_sql_ast_node *pairs = mylite_ast_child_at(statement, 0U);

    for (const struct mylite_sql_ast_node *pair = pairs == NULL ? NULL : pairs->first_child;
         pair != NULL;
         pair = pair->next_sibling) {
        struct mylite_rename_table_target target = {0};
        int status = copy_rename_table_pair(pair, &target);

        if (status == MYLITE_OK) {
            status = mylite_table_ddl_add_rename_table_target(plan, target);
        }
        if (status != MYLITE_OK) {
            mylite_table_ddl_rename_table_target_deinit(&target);
            return status;
        }
    }
    return plan->target_count == 0U ? MYLITE_UNSUPPORTED : MYLITE_OK;
}

static int copy_rename_table_pair(
    const struct mylite_sql_ast_node *pair,
    struct mylite_rename_table_target *target
) {
    int status = MYLITE_OK;

    if (pair == NULL || pair->kind != MYLITE_SQL_AST_RENAME_TABLE_PAIR) {
        return MYLITE_UNSUPPORTED;
    }

    status = mylite_table_ddl_copy_table_name_parts(
        mylite_ast_child_at(pair, 0U),
        &target->source_schema_name,
        &target->source_table_name
    );

    if (status != MYLITE_OK) {
        return status;
    }
    return mylite_table_ddl_copy_table_name_parts(
        mylite_ast_child_at(pair, 1U),
        &target->target_schema_name,
        &target->target_table_name
    );
}

int mylite_table_ddl_copy_truncate_table_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_truncate_table_plan *plan
) {
    return mylite_table_ddl_copy_table_name_parts(
        mylite_ast_child_at(statement, 0U),
        &plan->schema_name,
        &plan->table_name
    );
}

int mylite_table_ddl_copy_table_name_parts(
    const struct mylite_sql_ast_node *table_name,
    char **out_schema_name,
    char **out_table_name
) {
    *out_schema_name = NULL;
    *out_table_name = NULL;
    if (table_name == NULL) {
        return MYLITE_NOMEM;
    }
    if (table_name->kind == MYLITE_SQL_AST_IDENTIFIER) {
        *out_table_name = mylite_copy_identifier_span(table_name);
        return *out_table_name == NULL ? MYLITE_NOMEM : MYLITE_OK;
    }
    if (table_name->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER &&
        mylite_ast_child_at(table_name, 0U) != NULL &&
        mylite_ast_child_at(table_name, 1U) != NULL &&
        mylite_ast_child_at(table_name, 0U)->kind == MYLITE_SQL_AST_IDENTIFIER &&
        mylite_ast_child_at(table_name, 1U)->kind == MYLITE_SQL_AST_IDENTIFIER) {
        *out_schema_name = mylite_copy_identifier_span(mylite_ast_child_at(table_name, 0U));
        *out_table_name = mylite_copy_identifier_span(mylite_ast_child_at(table_name, 1U));
        if (*out_schema_name == NULL || *out_table_name == NULL) {
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }
    return MYLITE_UNSUPPORTED;
}

int mylite_table_ddl_add_rename_table_target(
    struct mylite_rename_table_plan *plan,
    struct mylite_rename_table_target target
) {
    struct mylite_rename_table_target *targets =
        realloc(plan->targets, (plan->target_count + 1U) * sizeof(*plan->targets));

    if (targets == NULL) {
        return MYLITE_NOMEM;
    }

    plan->targets = targets;
    plan->targets[plan->target_count++] = target;
    return MYLITE_OK;
}
