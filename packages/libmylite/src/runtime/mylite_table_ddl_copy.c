#include "mylite_table_ddl.h"

#include "mylite_span.h"

#include <stdlib.h>
#include <string.h>

static int copy_index_ddl_table_name(const struct mylite_sql_ast_node *table_name,
                                     struct mylite_index_ddl_plan *plan);
static int copy_drop_table_target(const struct mylite_sql_ast_node *table_name,
                                  struct mylite_drop_table_target *target);
static int add_drop_table_target(struct mylite_drop_table_plan *plan,
                                 struct mylite_drop_table_target target);
static int copy_rename_table_pair(const struct mylite_sql_ast_node *pair,
                                  struct mylite_rename_table_target *target);
static int copy_alter_table_item(const struct mylite_sql_ast_node *item,
                                 struct mylite_alter_table_plan *plan);
static int copy_alter_table_action(const struct mylite_sql_ast_node *action_node,
                                   struct mylite_alter_table_plan *plan);
static int copy_alter_table_add_column_action(const struct mylite_sql_ast_node *action_node,
                                              struct mylite_alter_table_action *action);
static int copy_alter_table_named_action(const struct mylite_sql_ast_node *action_node,
                                         enum mylite_alter_table_action_kind kind,
                                         struct mylite_alter_table_action *action);
static int copy_alter_table_rename_action(const struct mylite_sql_ast_node *action_node,
                                          enum mylite_alter_table_action_kind kind,
                                          struct mylite_alter_table_action *action);
static int copy_alter_table_rename_table_action(const struct mylite_sql_ast_node *action_node,
                                                struct mylite_alter_table_action *action);
static int copy_alter_table_change_column_action(const struct mylite_sql_ast_node *action_node,
                                                 struct mylite_alter_table_action *action);
static int copy_alter_table_modify_column_action(const struct mylite_sql_ast_node *action_node,
                                                 struct mylite_alter_table_action *action);
static int
copy_alter_table_alter_index_visibility_action(const struct mylite_sql_ast_node *action_node,
                                               struct mylite_alter_table_action *action);
static int copy_alter_table_index_action(const struct mylite_sql_ast_node *action_node,
                                         enum mylite_alter_table_action_kind kind,
                                         struct mylite_alter_table_action *action);
static int copy_alter_table_column_definition(const struct mylite_sql_ast_node *column_node,
                                              struct mylite_create_table_column *out_column);
static int copy_alter_table_column_position(const struct mylite_sql_ast_node *position_node,
                                            struct mylite_alter_table_action *action);
static int add_alter_table_action(struct mylite_alter_table_plan *plan,
                                  struct mylite_alter_table_action action);
static bool alter_table_option_is_default(const struct mylite_sql_ast_node *option);

int mylite_table_ddl_copy_create_index_statement(const struct mylite_sql_ast_node *statement,
                                                 struct mylite_index_ddl_plan *plan)
{
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
    }
    status = mylite_table_ddl_copy_create_table_key_parts(key_parts, &plan->index);
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_copy_create_table_index_options(options, &plan->index);
    }
    return status;
}

int mylite_table_ddl_copy_drop_index_statement(const struct mylite_sql_ast_node *statement,
                                               struct mylite_index_ddl_plan *plan)
{
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

static int copy_index_ddl_table_name(const struct mylite_sql_ast_node *table_name,
                                     struct mylite_index_ddl_plan *plan)
{
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

int mylite_table_ddl_copy_drop_table_statement(const struct mylite_sql_ast_node *statement,
                                               struct mylite_drop_table_plan *plan)
{
    const struct mylite_sql_ast_node *table_names = mylite_ast_child_at(statement, 0U);

    plan->temporary = statement->drop_table_temporary;
    plan->restrict_mode = statement->drop_table_restrict;
    plan->cascade_mode = statement->drop_table_cascade;

    for (const struct mylite_sql_ast_node *table_name =
             table_names == NULL ? NULL : table_names->first_child;
         table_name != NULL; table_name = table_name->next_sibling) {
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

static int copy_drop_table_target(const struct mylite_sql_ast_node *table_name,
                                  struct mylite_drop_table_target *target)
{
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

static int add_drop_table_target(struct mylite_drop_table_plan *plan,
                                 struct mylite_drop_table_target target)
{
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

int mylite_table_ddl_copy_rename_table_statement(const struct mylite_sql_ast_node *statement,
                                                 struct mylite_rename_table_plan *plan)
{
    const struct mylite_sql_ast_node *pairs = mylite_ast_child_at(statement, 0U);

    for (const struct mylite_sql_ast_node *pair = pairs == NULL ? NULL : pairs->first_child;
         pair != NULL; pair = pair->next_sibling) {
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

static int copy_rename_table_pair(const struct mylite_sql_ast_node *pair,
                                  struct mylite_rename_table_target *target)
{
    int status = MYLITE_OK;

    if (pair == NULL || pair->kind != MYLITE_SQL_AST_RENAME_TABLE_PAIR) {
        return MYLITE_UNSUPPORTED;
    }

    status = mylite_table_ddl_copy_table_name_parts(
        mylite_ast_child_at(pair, 0U), &target->source_schema_name, &target->source_table_name);

    if (status != MYLITE_OK) {
        return status;
    }
    return mylite_table_ddl_copy_table_name_parts(
        mylite_ast_child_at(pair, 1U), &target->target_schema_name, &target->target_table_name);
}

int mylite_table_ddl_copy_truncate_table_statement(const struct mylite_sql_ast_node *statement,
                                                   struct mylite_truncate_table_plan *plan)
{
    return mylite_table_ddl_copy_table_name_parts(mylite_ast_child_at(statement, 0U),
                                                  &plan->schema_name, &plan->table_name);
}

int mylite_table_ddl_copy_alter_table_statement(const struct mylite_sql_ast_node *statement,
                                                struct mylite_alter_table_plan *plan)
{
    const struct mylite_sql_ast_node *table_name = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *items = mylite_ast_child_at(statement, 1U);
    int status =
        mylite_table_ddl_copy_table_name_parts(table_name, &plan->schema_name, &plan->table_name);

    if (status != MYLITE_OK) {
        return status;
    }
    if (items == NULL || items->kind != MYLITE_SQL_AST_ALTER_TABLE_ITEM_LIST ||
        items->first_child == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    for (const struct mylite_sql_ast_node *item = items->first_child; item != NULL;
         item = item->next_sibling) {
        status = copy_alter_table_item(item, plan);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return plan->action_count == 0U && plan->unsupported_algorithm == NULL &&
                   plan->unsupported_lock == NULL
               ? MYLITE_UNSUPPORTED
               : MYLITE_OK;
}

static int copy_alter_table_item(const struct mylite_sql_ast_node *item,
                                 struct mylite_alter_table_plan *plan)
{
    if (item == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (item->kind == MYLITE_SQL_AST_ALTER_TABLE_ACTION) {
        return copy_alter_table_action(item, plan);
    }
    if (item->kind == MYLITE_SQL_AST_DDL_TABLE_OPTION) {
        char **target = NULL;

        if (alter_table_option_is_default(item)) {
            return MYLITE_OK;
        }
        if (item->ddl_table_option == MYLITE_SQL_AST_DDL_TABLE_OPTION_ALGORITHM) {
            target = &plan->unsupported_algorithm;
        } else if (item->ddl_table_option == MYLITE_SQL_AST_DDL_TABLE_OPTION_LOCK) {
            target = &plan->unsupported_lock;
        } else {
            return MYLITE_UNSUPPORTED;
        }
        if (*target == NULL) {
            *target = mylite_copy_span_text(item->span.text, item->span.length);
            if (*target == NULL) {
                return MYLITE_NOMEM;
            }
        }
        return MYLITE_OK;
    }
    return MYLITE_UNSUPPORTED;
}

static int copy_alter_table_action(const struct mylite_sql_ast_node *action_node,
                                   struct mylite_alter_table_plan *plan)
{
    struct mylite_alter_table_action action = {0};
    int status = MYLITE_OK;

    if (action_node == NULL || action_node->kind != MYLITE_SQL_AST_ALTER_TABLE_ACTION) {
        return MYLITE_UNSUPPORTED;
    }

    switch (action_node->alter_table_action) {
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_COLUMN:
        status = copy_alter_table_add_column_action(action_node, &action);
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_COLUMN:
        status = copy_alter_table_named_action(action_node, MYLITE_ALTER_TABLE_ACTION_DROP_COLUMN,
                                               &action);
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_RENAME_COLUMN:
        status = copy_alter_table_rename_action(action_node,
                                                MYLITE_ALTER_TABLE_ACTION_RENAME_COLUMN, &action);
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_RENAME_TABLE:
        status = copy_alter_table_rename_table_action(action_node, &action);
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_CHANGE_COLUMN:
        status = copy_alter_table_change_column_action(action_node, &action);
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_MODIFY_COLUMN:
        status = copy_alter_table_modify_column_action(action_node, &action);
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_PRIMARY_KEY:
        status = copy_alter_table_index_action(action_node,
                                               MYLITE_ALTER_TABLE_ACTION_ADD_PRIMARY_KEY, &action);
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_PRIMARY_KEY:
        action.kind = MYLITE_ALTER_TABLE_ACTION_DROP_PRIMARY_KEY;
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_UNIQUE_INDEX:
        status = copy_alter_table_index_action(action_node,
                                               MYLITE_ALTER_TABLE_ACTION_ADD_UNIQUE_INDEX, &action);
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_SECONDARY_INDEX:
        status = copy_alter_table_index_action(
            action_node, MYLITE_ALTER_TABLE_ACTION_ADD_SECONDARY_INDEX, &action);
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_FULLTEXT_INDEX:
        status = copy_alter_table_index_action(
            action_node, MYLITE_ALTER_TABLE_ACTION_ADD_FULLTEXT_INDEX, &action);
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_SPATIAL_INDEX:
        status = copy_alter_table_index_action(
            action_node, MYLITE_ALTER_TABLE_ACTION_ADD_SPATIAL_INDEX, &action);
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_INDEX:
        status = copy_alter_table_named_action(action_node, MYLITE_ALTER_TABLE_ACTION_DROP_INDEX,
                                               &action);
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_RENAME_INDEX:
        status = copy_alter_table_rename_action(action_node, MYLITE_ALTER_TABLE_ACTION_RENAME_INDEX,
                                                &action);
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ALTER_INDEX_VISIBILITY:
        status = copy_alter_table_alter_index_visibility_action(action_node, &action);
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_CHECK:
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_CHECK_OR_CONSTRAINT:
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ALTER_CHECK_OR_CONSTRAINT:
        action.kind = MYLITE_ALTER_TABLE_ACTION_UNSUPPORTED_CHECK;
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_FOREIGN_KEY:
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_FOREIGN_KEY:
        action.kind = MYLITE_ALTER_TABLE_ACTION_UNSUPPORTED_FOREIGN_KEY;
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_NONE:
        status = MYLITE_UNSUPPORTED;
        break;
    }

    if (status == MYLITE_OK) {
        status = add_alter_table_action(plan, action);
    }
    if (status != MYLITE_OK) {
        mylite_table_ddl_alter_table_action_deinit(&action);
    }
    return status;
}

static int copy_alter_table_add_column_action(const struct mylite_sql_ast_node *action_node,
                                              struct mylite_alter_table_action *action)
{
    int status = MYLITE_OK;

    action->kind = MYLITE_ALTER_TABLE_ACTION_ADD_COLUMN;
    status =
        copy_alter_table_column_definition(mylite_ast_child_at(action_node, 0U), &action->column);
    if (status != MYLITE_OK) {
        return status;
    }
    return copy_alter_table_column_position(mylite_ast_child_at(action_node, 1U), action);
}

static int copy_alter_table_named_action(const struct mylite_sql_ast_node *action_node,
                                         enum mylite_alter_table_action_kind kind,
                                         struct mylite_alter_table_action *action)
{
    action->kind = kind;
    action->old_name = mylite_copy_identifier_span(mylite_ast_child_at(action_node, 0U));
    if (action->old_name == NULL) {
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int copy_alter_table_rename_action(const struct mylite_sql_ast_node *action_node,
                                          enum mylite_alter_table_action_kind kind,
                                          struct mylite_alter_table_action *action)
{
    action->kind = kind;
    action->old_name = mylite_copy_identifier_span(mylite_ast_child_at(action_node, 0U));
    action->new_name = mylite_copy_identifier_span(mylite_ast_child_at(action_node, 1U));
    if (action->old_name == NULL || action->new_name == NULL) {
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int copy_alter_table_rename_table_action(const struct mylite_sql_ast_node *action_node,
                                                struct mylite_alter_table_action *action)
{
    action->kind = MYLITE_ALTER_TABLE_ACTION_RENAME_TABLE;
    return mylite_table_ddl_copy_table_name_parts(mylite_ast_child_at(action_node, 0U),
                                                  &action->new_schema_name, &action->new_name);
}

static int copy_alter_table_change_column_action(const struct mylite_sql_ast_node *action_node,
                                                 struct mylite_alter_table_action *action)
{
    int status = MYLITE_OK;

    action->kind = MYLITE_ALTER_TABLE_ACTION_CHANGE_COLUMN;
    action->old_name = mylite_copy_identifier_span(mylite_ast_child_at(action_node, 0U));
    if (action->old_name == NULL) {
        return MYLITE_NOMEM;
    }
    status =
        copy_alter_table_column_definition(mylite_ast_child_at(action_node, 1U), &action->column);
    if (status != MYLITE_OK) {
        return status;
    }
    return copy_alter_table_column_position(mylite_ast_child_at(action_node, 2U), action);
}

static int copy_alter_table_modify_column_action(const struct mylite_sql_ast_node *action_node,
                                                 struct mylite_alter_table_action *action)
{
    int status = MYLITE_OK;

    action->kind = MYLITE_ALTER_TABLE_ACTION_MODIFY_COLUMN;
    status =
        copy_alter_table_column_definition(mylite_ast_child_at(action_node, 0U), &action->column);
    if (status != MYLITE_OK) {
        return status;
    }
    return copy_alter_table_column_position(mylite_ast_child_at(action_node, 1U), action);
}

static int
copy_alter_table_alter_index_visibility_action(const struct mylite_sql_ast_node *action_node,
                                               struct mylite_alter_table_action *action)
{
    action->kind = MYLITE_ALTER_TABLE_ACTION_ALTER_INDEX_VISIBILITY;
    action->old_name = mylite_copy_identifier_span(mylite_ast_child_at(action_node, 0U));
    if (action->old_name == NULL) {
        return MYLITE_NOMEM;
    }
    if (action_node->index_option == MYLITE_SQL_AST_INDEX_OPTION_VISIBLE) {
        action->index_visible = true;
    }
    return MYLITE_OK;
}

static int copy_alter_table_index_action(const struct mylite_sql_ast_node *action_node,
                                         enum mylite_alter_table_action_kind kind,
                                         struct mylite_alter_table_action *action)
{
    struct mylite_create_table_plan plan = {0};
    int status =
        mylite_table_ddl_copy_create_table_index(mylite_ast_child_at(action_node, 0U), &plan);

    if (status != MYLITE_OK) {
        mylite_table_ddl_create_table_plan_deinit(&plan);
        return status;
    }
    if (plan.index_count != 1U) {
        mylite_table_ddl_create_table_plan_deinit(&plan);
        return MYLITE_UNSUPPORTED;
    }

    action->kind = kind;
    action->index = plan.indexes[0];
    if (kind == MYLITE_ALTER_TABLE_ACTION_ADD_PRIMARY_KEY) {
        action->index.is_primary = true;
        action->index.is_unique = true;
    } else if (kind == MYLITE_ALTER_TABLE_ACTION_ADD_UNIQUE_INDEX) {
        action->index.is_unique = true;
    }
    plan.indexes[0] = (struct mylite_create_table_index){0};
    mylite_table_ddl_create_table_plan_deinit(&plan);
    return MYLITE_OK;
}

static int copy_alter_table_column_definition(const struct mylite_sql_ast_node *column_node,
                                              struct mylite_create_table_column *out_column)
{
    struct mylite_create_table_plan plan = {0};
    int status = mylite_table_ddl_copy_create_table_column(column_node, &plan);

    if (status != MYLITE_OK) {
        mylite_table_ddl_create_table_plan_deinit(&plan);
        return status;
    }
    if (plan.column_count != 1U) {
        mylite_table_ddl_create_table_plan_deinit(&plan);
        return MYLITE_UNSUPPORTED;
    }

    *out_column = plan.columns[0];
    plan.columns[0] = (struct mylite_create_table_column){0};
    mylite_table_ddl_create_table_plan_deinit(&plan);
    return MYLITE_OK;
}

static int copy_alter_table_column_position(const struct mylite_sql_ast_node *position_node,
                                            struct mylite_alter_table_action *action)
{
    if (position_node == NULL) {
        action->position = MYLITE_ALTER_TABLE_COLUMN_POSITION_NONE;
        return MYLITE_OK;
    }
    if (position_node->kind != MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION) {
        return MYLITE_UNSUPPORTED;
    }

    switch (position_node->alter_table_column_position) {
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION_NONE:
        action->position = MYLITE_ALTER_TABLE_COLUMN_POSITION_NONE;
        return MYLITE_OK;
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION_FIRST:
        action->position = MYLITE_ALTER_TABLE_COLUMN_POSITION_FIRST;
        return MYLITE_OK;
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION_AFTER:
        action->position = MYLITE_ALTER_TABLE_COLUMN_POSITION_AFTER;
        action->after_column = mylite_copy_identifier_span(mylite_ast_child_at(position_node, 0U));
        return action->after_column == NULL ? MYLITE_NOMEM : MYLITE_OK;
    }
    return MYLITE_UNSUPPORTED;
}

static int add_alter_table_action(struct mylite_alter_table_plan *plan,
                                  struct mylite_alter_table_action action)
{
    struct mylite_alter_table_action *actions =
        realloc(plan->actions, (plan->action_count + 1U) * sizeof(*plan->actions));

    if (actions == NULL) {
        return MYLITE_NOMEM;
    }

    plan->actions = actions;
    plan->actions[plan->action_count++] = action;
    return MYLITE_OK;
}

static bool alter_table_option_is_default(const struct mylite_sql_ast_node *option)
{
    struct mylite_sql_source_span span =
        option == NULL ? (struct mylite_sql_source_span){0} : option->span;

    for (size_t index = 0U; index + strlen("DEFAULT") <= span.length; ++index) {
        struct mylite_sql_source_span candidate = {
            .text = span.text + index,
            .length = strlen("DEFAULT"),
        };

        if (mylite_span_equal_ci(candidate, "DEFAULT")) {
            return true;
        }
    }
    return false;
}

int mylite_table_ddl_copy_table_name_parts(const struct mylite_sql_ast_node *table_name,
                                           char **out_schema_name, char **out_table_name)
{
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

int mylite_table_ddl_add_rename_table_target(struct mylite_rename_table_plan *plan,
                                             struct mylite_rename_table_target target)
{
    struct mylite_rename_table_target *targets =
        realloc(plan->targets, (plan->target_count + 1U) * sizeof(*plan->targets));

    if (targets == NULL) {
        return MYLITE_NOMEM;
    }

    plan->targets = targets;
    plan->targets[plan->target_count++] = target;
    return MYLITE_OK;
}
