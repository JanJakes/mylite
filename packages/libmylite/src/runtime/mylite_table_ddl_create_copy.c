#include "mylite_table_ddl.h"

#include "mylite_span.h"
#include "mylite_statement_ast.h"
#include "mylite_table_ddl_create_column_copy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int copy_create_table_elements(
    const struct mylite_sql_ast_node *elements,
    struct mylite_create_table_plan *plan
);

static int copy_create_table_options(
    const struct mylite_sql_ast_node *statement,
    struct mylite_create_table_options *options
);

static int copy_create_table_select(
    const struct mylite_sql_ast_node *select_statement,
    struct mylite_create_table_plan *plan
);

static int add_create_table_index(
    struct mylite_create_table_plan *plan,
    struct mylite_create_table_index index
);

static int copy_create_table_foreign_key(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_create_table_plan *plan
);

static bool create_table_foreign_key_has_constraint_prefix(
    const struct mylite_sql_ast_node *action_node
);

static int add_create_table_foreign_key(
    struct mylite_create_table_plan *plan,
    struct mylite_create_table_foreign_key foreign_key
);

static int copy_foreign_key_identifier_list(
    const struct mylite_sql_ast_node *list,
    char ***out_names,
    size_t *out_count
);

static int copy_foreign_key_reference_options(
    const struct mylite_sql_ast_node *options,
    struct mylite_create_table_foreign_key *foreign_key
);

static char *copy_generated_foreign_key_constraint_name(
    const struct mylite_create_table_plan *plan
);

static const char *create_table_supporting_foreign_key_index_name(
    const struct mylite_create_table_plan *plan,
    const struct mylite_create_table_foreign_key *foreign_key
);

static bool create_table_index_supports_foreign_key(
    const struct mylite_create_table_index *index,
    const struct mylite_create_table_foreign_key *foreign_key
);

static bool create_table_has_supporting_foreign_key_index(
    const struct mylite_create_table_plan *plan,
    const struct mylite_create_table_foreign_key *foreign_key
);

static int add_create_table_foreign_key_index(
    struct mylite_create_table_plan *plan,
    const struct mylite_create_table_foreign_key *foreign_key
);

static int add_inline_create_table_column_indexes(
    struct mylite_create_table_plan *plan,
    const struct mylite_create_table_column *column
);

static int add_single_column_index(
    struct mylite_create_table_plan *plan,
    const char *column_name,
    bool is_primary,
    bool is_unique
);

int mylite_table_ddl_copy_create_table_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_create_table_plan *plan
) {
    const struct mylite_sql_ast_node *table_name = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *elements = mylite_ast_child_at(statement, 1U);
    int status = mylite_table_ddl_copy_create_table_name(table_name, plan);

    plan->temporary = statement->create_table_temporary;
    if (status != MYLITE_OK) {
        return status;
    }
    if (statement->create_table_like) {
        plan->like = true;
        return mylite_table_ddl_copy_table_name_parts(
            elements,
            &plan->source_schema_name,
            &plan->source_table_name
        );
    }
    if (statement->create_table_select) {
        const struct mylite_sql_ast_node *select_statement = NULL;
        const struct mylite_sql_ast_node *options = NULL;

        plan->select = true;
        if (elements != NULL && elements->kind == MYLITE_SQL_AST_COLUMN_DEFINITION_LIST) {
            select_statement = elements->next_sibling;
        } else {
            select_statement = elements;
            elements = NULL;
        }
        if (select_statement != NULL && select_statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
            return MYLITE_UNSUPPORTED;
        }
        options = select_statement == NULL ? NULL : select_statement->next_sibling;
        status = copy_create_table_select(select_statement, plan);
        if (status != MYLITE_OK) {
            return status;
        }
        if (elements != NULL) {
            status = copy_create_table_elements(elements, plan);
            if (status != MYLITE_OK) {
                return status;
            }
        }
        return copy_create_table_options(options, &plan->options);
    }
    status = copy_create_table_elements(elements, plan);
    if (status != MYLITE_OK) {
        return status;
    }
    return copy_create_table_options(statement, &plan->options);
}

static int copy_create_table_select(
    const struct mylite_sql_ast_node *select_statement,
    struct mylite_create_table_plan *plan
) {
    struct mylite_sql_ast_node *select_copy = NULL;

    if (select_statement == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    plan->select_sql_text =
        mylite_copy_span_text(select_statement->span.text, select_statement->span.length);
    if (plan->select_sql_text == NULL) {
        return MYLITE_NOMEM;
    }

    if (mylite_statement_ast_clone_subtree(
            &plan->select_ast,
            select_statement,
            select_statement->span.text,
            plan->select_sql_text,
            select_statement->span.length,
            &select_copy
        ) != MYLITE_OK) {
        return MYLITE_NOMEM;
    }
    plan->select_statement = select_copy;
    return MYLITE_OK;
}

int mylite_table_ddl_copy_create_table_name(
    const struct mylite_sql_ast_node *table_name,
    struct mylite_create_table_plan *plan
) {
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

int mylite_table_ddl_copy_create_table_index(
    const struct mylite_sql_ast_node *index_node,
    struct mylite_create_table_plan *plan
) {
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
    index.is_fulltext = index_node->index_class == MYLITE_SQL_AST_INDEX_CLASS_FULLTEXT;
    index.is_spatial = index_node->index_class == MYLITE_SQL_AST_INDEX_CLASS_SPATIAL;
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
            index.display_index_type =
                child->index_algorithm == MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE;
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

int mylite_table_ddl_copy_create_table_key_parts(
    const struct mylite_sql_ast_node *key_parts,
    struct mylite_create_table_index *index
) {
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

int mylite_table_ddl_copy_create_table_index_options(
    const struct mylite_sql_ast_node *options,
    struct mylite_create_table_index *index
) {
    const struct mylite_sql_ast_node *option = NULL;

    for (option = options == NULL ? NULL : options->first_child; option != NULL;
         option = option->next_sibling) {
        char *copy = NULL;

        switch (option->index_option) {
        case MYLITE_SQL_AST_INDEX_OPTION_USING:
            index->algorithm = option->index_algorithm;
            index->display_index_type =
                option->index_algorithm == MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE;
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
        case MYLITE_SQL_AST_INDEX_OPTION_SECONDARY_ENGINE_ATTRIBUTE:
        case MYLITE_SQL_AST_INDEX_OPTION_NONE:
            break;
        case MYLITE_SQL_AST_INDEX_OPTION_ENGINE_ATTRIBUTE:
            index->has_engine_attribute = true;
            break;
        case MYLITE_SQL_AST_INDEX_OPTION_WITH_PARSER:
            copy = mylite_copy_identifier_span(mylite_ast_child_at(option, 0U));
            if (copy == NULL) {
                return MYLITE_NOMEM;
            }
            free(index->parser_name);
            index->parser_name = copy;
            index->has_with_parser = true;
            break;
        }
    }
    return MYLITE_OK;
}

static int copy_create_table_elements(
    const struct mylite_sql_ast_node *elements,
    struct mylite_create_table_plan *plan
) {
    const struct mylite_sql_ast_node *element = NULL;
    int status = MYLITE_OK;

    if (elements == NULL || elements->first_child == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    for (element = elements->first_child; element != NULL; element = element->next_sibling) {
        if (element->kind == MYLITE_SQL_AST_COLUMN_DEFINITION) {
            status = mylite_table_ddl_copy_create_table_column(element, plan);
            if (status == MYLITE_OK) {
                status = add_inline_create_table_column_indexes(
                    plan,
                    &plan->columns[plan->column_count - 1U]
                );
            }
        } else if (
            element->kind == MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT ||
            element->kind == MYLITE_SQL_AST_UNIQUE_INDEX ||
            element->kind == MYLITE_SQL_AST_SECONDARY_INDEX
        ) {
            status = mylite_table_ddl_copy_create_table_index(element, plan);
        } else if (
            element->kind == MYLITE_SQL_AST_ALTER_TABLE_ACTION &&
            element->alter_table_action == MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_CHECK
        ) {
            const struct mylite_sql_ast_node *first_child = mylite_ast_child_at(element, 0U);
            const struct mylite_sql_ast_node *constraint_name =
                first_child != NULL && first_child->kind == MYLITE_SQL_AST_IDENTIFIER ? first_child
                                                                                      : NULL;
            const struct mylite_sql_ast_node *expression =
                constraint_name == NULL ? first_child : mylite_ast_child_at(element, 1U);
            const struct mylite_create_table_check_ast input = {
                .constraint_name = constraint_name,
                .expression = expression,
                .enforcement = element->constraint_enforcement,
            };

            status = mylite_table_ddl_add_create_table_check(plan, &input);
        } else if (
            element->kind == MYLITE_SQL_AST_ALTER_TABLE_ACTION &&
            element->alter_table_action == MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_FOREIGN_KEY
        ) {
            status = copy_create_table_foreign_key(element, plan);
        } else {
            status = MYLITE_UNSUPPORTED;
        }
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int copy_create_table_options(
    const struct mylite_sql_ast_node *statement,
    struct mylite_create_table_options *options
) {
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

static int add_create_table_index(
    struct mylite_create_table_plan *plan,
    struct mylite_create_table_index index
) {
    struct mylite_create_table_index *indexes =
        realloc(plan->indexes, (plan->index_count + 1U) * sizeof(*plan->indexes));

    if (indexes == NULL) {
        return MYLITE_NOMEM;
    }

    plan->indexes = indexes;
    plan->indexes[plan->index_count++] = index;
    return MYLITE_OK;
}

static int copy_create_table_foreign_key(
    const struct mylite_sql_ast_node *action_node,
    struct mylite_create_table_plan *plan
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

    if (create_table_foreign_key_has_constraint_prefix(action_node) && child != NULL &&
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
    } else {
        foreign_key.constraint_name = copy_generated_foreign_key_constraint_name(plan);
        foreign_key.generated_constraint_name = true;
    }
    if (foreign_key.constraint_name == NULL) {
        status = MYLITE_NOMEM;
    }
    if (status == MYLITE_OK) {
        status = copy_foreign_key_identifier_list(
            columns,
            &foreign_key.column_names,
            &foreign_key.column_count
        );
    }
    if (status == MYLITE_OK) {
        const char *existing_index =
            create_table_supporting_foreign_key_index_name(plan, &foreign_key);

        if (existing_index != NULL) {
            foreign_key.supporting_index_name =
                mylite_copy_span_text(existing_index, strlen(existing_index));
        } else if (constraint_name != NULL) {
            foreign_key.supporting_index_name = mylite_copy_span_text(
                foreign_key.constraint_name,
                strlen(foreign_key.constraint_name)
            );
        } else if (index_name != NULL) {
            foreign_key.supporting_index_name = mylite_copy_identifier_span(index_name);
        } else {
            foreign_key.supporting_index_name = mylite_copy_span_text(
                foreign_key.column_names[0],
                strlen(foreign_key.column_names[0])
            );
        }
        if (foreign_key.supporting_index_name == NULL) {
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
        status = copy_foreign_key_identifier_list(
            referenced_columns,
            &foreign_key.referenced_column_names,
            &foreign_key.referenced_column_count
        );
    }
    if (status == MYLITE_OK) {
        status = copy_foreign_key_reference_options(reference_options, &foreign_key);
    }
    if (status == MYLITE_OK && foreign_key.column_count != foreign_key.referenced_column_count) {
        status = MYLITE_UNSUPPORTED;
    }
    if (status == MYLITE_OK && !create_table_has_supporting_foreign_key_index(plan, &foreign_key)) {
        status = add_create_table_foreign_key_index(plan, &foreign_key);
    }
    if (status == MYLITE_OK) {
        status = add_create_table_foreign_key(plan, foreign_key);
    }
    if (status != MYLITE_OK) {
        mylite_table_ddl_create_table_foreign_key_deinit(&foreign_key);
    }
    return status;
}

static bool create_table_foreign_key_has_constraint_prefix(
    const struct mylite_sql_ast_node *action_node
) {
    const char constraint_keyword[] = "CONSTRAINT";
    size_t keyword_length = strlen(constraint_keyword);

    if (action_node == NULL || action_node->span.text == NULL ||
        action_node->span.length < keyword_length) {
        return false;
    }
    for (size_t index = 0U; index < keyword_length; ++index) {
        char actual = action_node->span.text[index];
        char expected = constraint_keyword[index];

        if (actual >= 'a' && actual <= 'z') {
            actual = (char)(actual - 'a' + 'A');
        }
        if (actual != expected) {
            return false;
        }
    }
    return action_node->span.length == keyword_length ||
           action_node->span.text[keyword_length] == ' ' ||
           action_node->span.text[keyword_length] == '\t' ||
           action_node->span.text[keyword_length] == '\n' ||
           action_node->span.text[keyword_length] == '\r';
}

static int add_create_table_foreign_key(
    struct mylite_create_table_plan *plan,
    struct mylite_create_table_foreign_key foreign_key
) {
    struct mylite_create_table_foreign_key *foreign_keys =
        realloc(plan->foreign_keys, (plan->foreign_key_count + 1U) * sizeof(*plan->foreign_keys));

    if (foreign_keys == NULL) {
        return MYLITE_NOMEM;
    }

    plan->foreign_keys = foreign_keys;
    plan->foreign_keys[plan->foreign_key_count++] = foreign_key;
    return MYLITE_OK;
}

static int copy_foreign_key_identifier_list(
    const struct mylite_sql_ast_node *list,
    char ***out_names,
    size_t *out_count
) {
    *out_names = NULL;
    *out_count = 0U;
    for (const struct mylite_sql_ast_node *node = list == NULL ? NULL : list->first_child;
         node != NULL;
         node = node->next_sibling) {
        char **names = realloc(*out_names, (*out_count + 1U) * sizeof(**out_names));
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

static int copy_foreign_key_reference_options(
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

static char *copy_generated_foreign_key_constraint_name(
    const struct mylite_create_table_plan *plan
) {
    size_t prefix_length = strlen(plan->table_name) + strlen("_ibfk_");
    size_t generated_count = 0U;
    char *name = NULL;
    int written = 0;

    for (size_t index = 0U; index < plan->foreign_key_count; ++index) {
        char *prefix = NULL;
        bool generated = false;

        prefix = malloc(prefix_length + 1U);
        if (prefix == NULL) {
            return NULL;
        }
        (void)snprintf(prefix, prefix_length + 1U, "%s_ibfk_", plan->table_name);
        generated = strncmp(plan->foreign_keys[index].constraint_name, prefix, prefix_length) == 0;
        free(prefix);
        if (generated) {
            ++generated_count;
        }
    }

    name = malloc(prefix_length + 20U + 1U);
    if (name == NULL) {
        return NULL;
    }
    written = snprintf(
        name,
        prefix_length + 20U + 1U,
        "%s_ibfk_%zu",
        plan->table_name,
        generated_count + 1U
    );
    if (written < 0) {
        free(name);
        return NULL;
    }
    return name;
}

static const char *create_table_supporting_foreign_key_index_name(
    const struct mylite_create_table_plan *plan,
    const struct mylite_create_table_foreign_key *foreign_key
) {
    for (size_t index = 0U; index < plan->index_count; ++index) {
        if (create_table_index_supports_foreign_key(&plan->indexes[index], foreign_key)) {
            return plan->indexes[index].name;
        }
    }
    return NULL;
}

static bool create_table_index_supports_foreign_key(
    const struct mylite_create_table_index *index,
    const struct mylite_create_table_foreign_key *foreign_key
) {
    if (index->part_count < foreign_key->column_count) {
        return false;
    }
    for (size_t part = 0U; part < foreign_key->column_count; ++part) {
        if (!mylite_ascii_case_equal(
                index->parts[part].column_name,
                foreign_key->column_names[part]
            )) {
            return false;
        }
    }
    return true;
}

static bool create_table_has_supporting_foreign_key_index(
    const struct mylite_create_table_plan *plan,
    const struct mylite_create_table_foreign_key *foreign_key
) {
    for (size_t index = 0U; index < plan->index_count; ++index) {
        if (create_table_index_supports_foreign_key(&plan->indexes[index], foreign_key)) {
            return true;
        }
    }
    return false;
}

static int add_create_table_foreign_key_index(
    struct mylite_create_table_plan *plan,
    const struct mylite_create_table_foreign_key *foreign_key
) {
    struct mylite_create_table_index index = {
        .name = mylite_copy_span_text(
            foreign_key->supporting_index_name,
            strlen(foreign_key->supporting_index_name)
        ),
        .algorithm = MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE,
        .is_visible = true,
        .explicit_name = true,
    };
    int status = MYLITE_OK;

    if (index.name == NULL) {
        return MYLITE_NOMEM;
    }
    for (size_t part_index = 0U; part_index < foreign_key->column_count; ++part_index) {
        struct mylite_create_table_key_part *parts =
            realloc(index.parts, (index.part_count + 1U) * sizeof(*index.parts));
        struct mylite_create_table_key_part part = {
            .column_name = mylite_copy_span_text(
                foreign_key->column_names[part_index],
                strlen(foreign_key->column_names[part_index])
            ),
            .order = MYLITE_SQL_AST_KEY_PART_ORDER_NONE,
        };

        if (parts == NULL || part.column_name == NULL) {
            free(part.column_name);
            mylite_table_ddl_create_table_index_deinit(&index);
            return MYLITE_NOMEM;
        }
        index.parts = parts;
        index.parts[index.part_count++] = part;
    }

    status = add_create_table_index(plan, index);
    if (status != MYLITE_OK) {
        mylite_table_ddl_create_table_index_deinit(&index);
    }
    return status;
}

static int add_inline_create_table_column_indexes(
    struct mylite_create_table_plan *plan,
    const struct mylite_create_table_column *column
) {
    int status = MYLITE_OK;

    if (column->primary_key) {
        status = add_single_column_index(plan, column->name, true, true);
    }
    if (status == MYLITE_OK && column->unique_key) {
        status = add_single_column_index(plan, column->name, false, true);
    }
    return status;
}

static int add_single_column_index(
    struct mylite_create_table_plan *plan,
    const char *column_name,
    bool is_primary,
    bool is_unique
) {
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
