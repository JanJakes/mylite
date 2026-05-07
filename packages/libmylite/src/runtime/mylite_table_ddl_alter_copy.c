#include "mylite_table_ddl.h"
#include "mylite_table_ddl_create_column_copy.h"

#include "mylite_span.h"

#include <stdlib.h>
#include <string.h>

static int copy_alter_table_item(
    const struct mylite_sql_ast_node *item,
    struct mylite_alter_table_plan *plan
);

static int copy_alter_table_action(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_alter_table_plan *plan
);

static int copy_alter_table_add_column_action(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_alter_table_action *action
);

static int copy_alter_table_named_action(
    const struct mylite_sql_ast_node *action_node,
    enum mylite_alter_table_action_kind kind,
    struct mylite_alter_table_action *action
);

static int copy_alter_table_rename_action(
    const struct mylite_sql_ast_node *action_node,
    enum mylite_alter_table_action_kind kind,
    struct mylite_alter_table_action *action
);

static int copy_alter_table_rename_table_action(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_alter_table_action *action
);

static int copy_alter_table_change_column_action(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_alter_table_action *action
);

static int copy_alter_table_modify_column_action(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_alter_table_action *action
);

static int copy_alter_table_alter_index_visibility_action(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_alter_table_action *action
);

static int copy_alter_table_index_action(
    const struct mylite_sql_ast_node *action_node,
    enum mylite_alter_table_action_kind kind,
    struct mylite_alter_table_action *action
);

static int copy_alter_table_add_check_action(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_alter_table_action *action
);

static int copy_alter_table_alter_check_action(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_alter_table_action *action
);

static int copy_alter_table_add_foreign_key_action(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_alter_table_action *action
);

static int copy_alter_table_drop_foreign_key_action(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_alter_table_action *action
);

static int copy_alter_table_column_definition(
    const struct mylite_sql_ast_node *column_node,
    struct mylite_create_table_column *out_column
);

static bool alter_table_foreign_key_has_constraint_prefix(
    const struct mylite_sql_ast_node *action_node
);

static int copy_alter_table_foreign_key_identifier_list(
    const struct mylite_sql_ast_node *list,
    char ***out_names,
    size_t *out_count
);

static int copy_alter_table_foreign_key_reference_options(
    const struct mylite_sql_ast_node *options,
    struct mylite_create_table_foreign_key *foreign_key
);

static int copy_alter_table_column_position(
    const struct mylite_sql_ast_node *position_node,
    struct mylite_alter_table_action *action
);

static int add_alter_table_action(
    struct mylite_alter_table_plan *plan,
    struct mylite_alter_table_action action
);

static bool alter_table_option_is_default(const struct mylite_sql_ast_node *option);

int mylite_table_ddl_copy_alter_table_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_alter_table_plan *plan
) {
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
                   plan->unsupported_lock == NULL && !plan->has_auto_increment
               ? MYLITE_UNSUPPORTED
               : MYLITE_OK;
}

static int copy_alter_table_item(
    const struct mylite_sql_ast_node *item,
    struct mylite_alter_table_plan *plan
) {
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
        } else if (item->ddl_table_option == MYLITE_SQL_AST_DDL_TABLE_OPTION_AUTO_INCREMENT) {
            plan->has_auto_increment = true;
            plan->auto_increment = mylite_ast_child_at(item, 0U)->column_length;
            return MYLITE_OK;
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

static int copy_alter_table_action(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_alter_table_plan *plan
) {
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
        status = copy_alter_table_named_action(
            action_node,
            MYLITE_ALTER_TABLE_ACTION_DROP_COLUMN,
            &action
        );
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_RENAME_COLUMN:
        status = copy_alter_table_rename_action(
            action_node,
            MYLITE_ALTER_TABLE_ACTION_RENAME_COLUMN,
            &action
        );
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
        status = copy_alter_table_index_action(
            action_node,
            MYLITE_ALTER_TABLE_ACTION_ADD_PRIMARY_KEY,
            &action
        );
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_PRIMARY_KEY:
        action.kind = MYLITE_ALTER_TABLE_ACTION_DROP_PRIMARY_KEY;
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_UNIQUE_INDEX:
        status = copy_alter_table_index_action(
            action_node,
            MYLITE_ALTER_TABLE_ACTION_ADD_UNIQUE_INDEX,
            &action
        );
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_SECONDARY_INDEX:
        status = copy_alter_table_index_action(
            action_node,
            MYLITE_ALTER_TABLE_ACTION_ADD_SECONDARY_INDEX,
            &action
        );
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_FULLTEXT_INDEX:
        status = copy_alter_table_index_action(
            action_node,
            MYLITE_ALTER_TABLE_ACTION_ADD_FULLTEXT_INDEX,
            &action
        );
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_SPATIAL_INDEX:
        status = copy_alter_table_index_action(
            action_node,
            MYLITE_ALTER_TABLE_ACTION_ADD_SPATIAL_INDEX,
            &action
        );
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_INDEX:
        status = copy_alter_table_named_action(
            action_node,
            MYLITE_ALTER_TABLE_ACTION_DROP_INDEX,
            &action
        );
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_RENAME_INDEX:
        status = copy_alter_table_rename_action(
            action_node,
            MYLITE_ALTER_TABLE_ACTION_RENAME_INDEX,
            &action
        );
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ALTER_INDEX_VISIBILITY:
        status = copy_alter_table_alter_index_visibility_action(action_node, &action);
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_CHECK:
        status = copy_alter_table_add_check_action(action_node, &action);
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_CHECK_OR_CONSTRAINT:
        status = copy_alter_table_named_action(
            action_node,
            MYLITE_ALTER_TABLE_ACTION_DROP_CHECK,
            &action
        );
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ALTER_CHECK_OR_CONSTRAINT:
        status = copy_alter_table_alter_check_action(action_node, &action);
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_FOREIGN_KEY:
        status = copy_alter_table_add_foreign_key_action(action_node, &action);
        break;
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_FOREIGN_KEY:
        status = copy_alter_table_drop_foreign_key_action(action_node, &action);
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

static int copy_alter_table_add_column_action(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_alter_table_action *action
) {
    int status = MYLITE_OK;

    action->kind = MYLITE_ALTER_TABLE_ACTION_ADD_COLUMN;
    status =
        copy_alter_table_column_definition(mylite_ast_child_at(action_node, 0U), &action->column);
    if (status != MYLITE_OK) {
        return status;
    }
    return copy_alter_table_column_position(mylite_ast_child_at(action_node, 1U), action);
}

static int copy_alter_table_named_action(
    const struct mylite_sql_ast_node *action_node,
    enum mylite_alter_table_action_kind kind,
    struct mylite_alter_table_action *action
) {
    action->kind = kind;
    action->old_name = mylite_copy_identifier_span(mylite_ast_child_at(action_node, 0U));
    if (action->old_name == NULL) {
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int copy_alter_table_rename_action(
    const struct mylite_sql_ast_node *action_node,
    enum mylite_alter_table_action_kind kind,
    struct mylite_alter_table_action *action
) {
    action->kind = kind;
    action->old_name = mylite_copy_identifier_span(mylite_ast_child_at(action_node, 0U));
    action->new_name = mylite_copy_identifier_span(mylite_ast_child_at(action_node, 1U));
    if (action->old_name == NULL || action->new_name == NULL) {
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int copy_alter_table_rename_table_action(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_alter_table_action *action
) {
    action->kind = MYLITE_ALTER_TABLE_ACTION_RENAME_TABLE;
    return mylite_table_ddl_copy_table_name_parts(
        mylite_ast_child_at(action_node, 0U),
        &action->new_schema_name,
        &action->new_name
    );
}

static int copy_alter_table_change_column_action(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_alter_table_action *action
) {
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

static int copy_alter_table_modify_column_action(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_alter_table_action *action
) {
    int status = MYLITE_OK;

    action->kind = MYLITE_ALTER_TABLE_ACTION_MODIFY_COLUMN;
    status =
        copy_alter_table_column_definition(mylite_ast_child_at(action_node, 0U), &action->column);
    if (status != MYLITE_OK) {
        return status;
    }
    return copy_alter_table_column_position(mylite_ast_child_at(action_node, 1U), action);
}

static int copy_alter_table_alter_index_visibility_action(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_alter_table_action *action
) {
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

static int copy_alter_table_index_action(
    const struct mylite_sql_ast_node *action_node,
    enum mylite_alter_table_action_kind kind,
    struct mylite_alter_table_action *action
) {
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

static int copy_alter_table_add_check_action(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_alter_table_action *action
) {
    const struct mylite_sql_ast_node *first_child = mylite_ast_child_at(action_node, 0U);
    const struct mylite_sql_ast_node *constraint_name =
        first_child != NULL && first_child->kind == MYLITE_SQL_AST_IDENTIFIER ? first_child : NULL;
    const struct mylite_sql_ast_node *expression =
        constraint_name == NULL ? first_child : mylite_ast_child_at(action_node, 1U);

    action->kind = MYLITE_ALTER_TABLE_ACTION_ADD_CHECK;
    action->check.generated_name = constraint_name == NULL;
    action->check.enforced =
        action_node->constraint_enforcement != MYLITE_SQL_AST_CONSTRAINT_ENFORCEMENT_NOT_ENFORCED;
    if (constraint_name != NULL) {
        action->check.name = mylite_copy_identifier_span(constraint_name);
        if (action->check.name == NULL) {
            return MYLITE_NOMEM;
        }
    }
    action->check.clause = mylite_table_ddl_copy_check_clause_text(expression);
    if (action->check.clause == NULL) {
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int copy_alter_table_alter_check_action(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_alter_table_action *action
) {
    int status =
        copy_alter_table_named_action(action_node, MYLITE_ALTER_TABLE_ACTION_ALTER_CHECK, action);

    if (status != MYLITE_OK) {
        return status;
    }
    action->check.enforced =
        action_node->constraint_enforcement != MYLITE_SQL_AST_CONSTRAINT_ENFORCEMENT_NOT_ENFORCED;
    return MYLITE_OK;
}

static int copy_alter_table_add_foreign_key_action(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_alter_table_action *action
) {
    struct mylite_create_table_foreign_key foreign_key = {
        .match = MYLITE_SQL_AST_REFERENCE_MATCH_NONE,
        .on_update = MYLITE_SQL_AST_REFERENCE_ACTION_NO_ACTION,
        .on_delete = MYLITE_SQL_AST_REFERENCE_ACTION_NO_ACTION,
    };
    const struct mylite_sql_ast_node *child = action_node->first_child;
    const struct mylite_sql_ast_node *constraint_name = NULL;
    const struct mylite_sql_ast_node *index_name = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *reference = NULL;
    const struct mylite_sql_ast_node *referenced_table = NULL;
    const struct mylite_sql_ast_node *referenced_columns = NULL;
    const struct mylite_sql_ast_node *reference_options = NULL;
    int status = MYLITE_OK;

    if (alter_table_foreign_key_has_constraint_prefix(action_node) && child != NULL &&
        child->kind == MYLITE_SQL_AST_IDENTIFIER) {
        constraint_name = child;
        child = child->next_sibling;
    }
    if (child != NULL && child->kind == MYLITE_SQL_AST_IDENTIFIER) {
        index_name = child;
        child = child->next_sibling;
    }
    columns = child;
    reference = columns == NULL ? NULL : columns->next_sibling;
    referenced_table = mylite_ast_child_at(reference, 0U);
    referenced_columns = mylite_ast_child_at(reference, 1U);
    reference_options = mylite_ast_child_at(reference, 2U);

    if (constraint_name != NULL) {
        foreign_key.constraint_name = mylite_copy_identifier_span(constraint_name);
        if (foreign_key.constraint_name == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK) {
        status = copy_alter_table_foreign_key_identifier_list(
            columns,
            &foreign_key.column_names,
            &foreign_key.column_count
        );
    }
    if (status == MYLITE_OK) {
        if (index_name != NULL) {
            foreign_key.supporting_index_name = mylite_copy_identifier_span(index_name);
        } else if (foreign_key.constraint_name != NULL) {
            foreign_key.supporting_index_name =
                mylite_copy_nonempty_cstring(foreign_key.constraint_name);
        }
        if ((index_name != NULL || foreign_key.constraint_name != NULL) &&
            foreign_key.supporting_index_name == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_copy_table_name_parts(
            referenced_table,
            &foreign_key.referenced_schema_name,
            &foreign_key.referenced_table_name
        );
    }
    if (status == MYLITE_OK) {
        status = copy_alter_table_foreign_key_identifier_list(
            referenced_columns,
            &foreign_key.referenced_column_names,
            &foreign_key.referenced_column_count
        );
    }
    if (status == MYLITE_OK) {
        status = copy_alter_table_foreign_key_reference_options(reference_options, &foreign_key);
    }
    if (status == MYLITE_OK && foreign_key.column_count != foreign_key.referenced_column_count) {
        status = MYLITE_UNSUPPORTED;
    }
    if (status == MYLITE_OK) {
        action->kind = MYLITE_ALTER_TABLE_ACTION_ADD_FOREIGN_KEY;
        action->foreign_key = foreign_key;
    }
    if (status != MYLITE_OK) {
        mylite_table_ddl_create_table_foreign_key_deinit(&foreign_key);
    }
    return status;
}

static int copy_alter_table_drop_foreign_key_action(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_alter_table_action *action
) {
    action->kind = MYLITE_ALTER_TABLE_ACTION_DROP_FOREIGN_KEY;
    action->old_name = mylite_copy_identifier_span(mylite_ast_child_at(action_node, 0U));
    if (action->old_name == NULL) {
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int copy_alter_table_column_definition(
    const struct mylite_sql_ast_node *column_node,
    struct mylite_create_table_column *out_column
) {
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

static bool alter_table_foreign_key_has_constraint_prefix(
    const struct mylite_sql_ast_node *action_node
) {
    const char constraint_keyword[] = "CONSTRAINT";
    const char foreign_keyword[] = "FOREIGN";
    size_t keyword_length = strlen(constraint_keyword);
    size_t foreign_length = strlen(foreign_keyword);

    if (action_node == NULL || action_node->span.text == NULL) {
        return false;
    }
    for (size_t index = 0U; index < action_node->span.length; ++index) {
        if (index + foreign_length <= action_node->span.length) {
            struct mylite_sql_source_span candidate = {
                .text = action_node->span.text + index,
                .length = foreign_length,
            };

            if (mylite_span_equal_ci(candidate, foreign_keyword)) {
                return false;
            }
        }
        if (index + keyword_length <= action_node->span.length) {
            struct mylite_sql_source_span candidate = {
                .text = action_node->span.text + index,
                .length = keyword_length,
            };

            if (mylite_span_equal_ci(candidate, constraint_keyword)) {
                return true;
            }
        }
    }
    return false;
}

static int copy_alter_table_foreign_key_identifier_list(
    const struct mylite_sql_ast_node *list,
    char ***out_names,
    size_t *out_count
) {
    *out_names = NULL;
    *out_count = 0U;
    for (const struct mylite_sql_ast_node *node = list == NULL ? NULL : list->first_child;
         node != NULL;
         node = node->next_sibling) {
        char **names =
            (char **)realloc((void *)*out_names, (*out_count + 1U) * sizeof(**out_names));
        char *name = NULL;

        if (names == NULL) {
            return MYLITE_NOMEM;
        }
        *out_names = names;
        name = mylite_copy_identifier_span(node);
        if (name == NULL) {
            return MYLITE_NOMEM;
        }
        (*out_names)[(*out_count)++] = name;
    }
    return *out_count == 0U ? MYLITE_UNSUPPORTED : MYLITE_OK;
}

static int copy_alter_table_foreign_key_reference_options(
    const struct mylite_sql_ast_node *options,
    struct mylite_create_table_foreign_key *foreign_key
) {
    for (const struct mylite_sql_ast_node *option = options == NULL ? NULL : options->first_child;
         option != NULL;
         option = option->next_sibling) {
        switch (option->reference_option) {
        case MYLITE_SQL_AST_REFERENCE_OPTION_ON_DELETE:
            foreign_key->on_delete = option->reference_action;
            break;
        case MYLITE_SQL_AST_REFERENCE_OPTION_ON_UPDATE:
            foreign_key->on_update = option->reference_action;
            break;
        case MYLITE_SQL_AST_REFERENCE_OPTION_MATCH:
            foreign_key->match = option->reference_match;
            break;
        case MYLITE_SQL_AST_REFERENCE_OPTION_NONE:
            break;
        }
    }
    return MYLITE_OK;
}

static int copy_alter_table_column_position(
    const struct mylite_sql_ast_node *position_node,
    struct mylite_alter_table_action *action
) {
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

static int add_alter_table_action(
    struct mylite_alter_table_plan *plan,
    struct mylite_alter_table_action action
) {
    struct mylite_alter_table_action *actions =
        realloc(plan->actions, (plan->action_count + 1U) * sizeof(*plan->actions));

    if (actions == NULL) {
        return MYLITE_NOMEM;
    }

    plan->actions = actions;
    plan->actions[plan->action_count++] = action;
    return MYLITE_OK;
}

static bool alter_table_option_is_default(const struct mylite_sql_ast_node *option) {
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
