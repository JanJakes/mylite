#include "mylite_table_ddl.h"

#include "mylite_span.h"
#include "mylite_table_ddl_create_column_copy.h"

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

static int add_create_table_index(
    struct mylite_create_table_plan *plan,
    struct mylite_create_table_index index
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
    status = copy_create_table_elements(elements, plan);
    if (status != MYLITE_OK) {
        return status;
    }
    return copy_create_table_options(statement, &plan->options);
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
            size_t column_index = plan->column_count;

            status = mylite_table_ddl_copy_create_table_column(element, plan);
            if (status == MYLITE_OK) {
                status = add_inline_create_table_column_indexes(plan, &plan->columns[column_index]);
            }
        } else if (
            element->kind == MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT ||
            element->kind == MYLITE_SQL_AST_UNIQUE_INDEX ||
            element->kind == MYLITE_SQL_AST_SECONDARY_INDEX
        ) {
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
