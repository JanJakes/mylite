#include "mylite_parser_helpers.h"
#include "mylite_parser_internal.h"

#include <stdbool.h>
#include <stddef.h>

struct column_attribute_positions {
    size_t charset;
    size_t collation;
    size_t binary_collation;
    size_t comment;
    size_t nullability;
    size_t default_value;
    size_t primary_key;
    size_t unique_key;
    size_t auto_increment;
    size_t generated;
    size_t visibility;
    size_t srid;
};

static int scan_column_attribute_positions(
    struct mylite_sql_parser_state *state,
    const struct mylite_sql_ast_node *attributes,
    struct column_attribute_positions *out_positions
);
static int record_column_attribute_position(
    struct mylite_sql_parser_state *state,
    size_t *slot,
    size_t position
);
static int validate_legacy_column_attribute_order(
    struct mylite_sql_parser_state *state,
    const struct column_attribute_positions *positions
);
static bool column_attribute_charset_order_is_valid(
    const struct column_attribute_positions *positions
);
static bool legacy_column_attribute_precedes_charset(
    const struct column_attribute_positions *positions
);
static bool column_attribute_position_precedes(size_t position, size_t limit);
static bool legacy_column_attribute_precedes_position(
    const struct column_attribute_positions *positions,
    size_t limit
);
static bool column_attribute_position_is_set(size_t position);

struct mylite_sql_ast_node *mylite_sql_parser_make_column_definition_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *column
) {
    struct mylite_sql_source_span span =
        column == NULL ? (struct mylite_sql_source_span){0} : column->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_COLUMN_DEFINITION_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, column);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_column_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *column
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, column);
    if (column != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, column->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_primary_key_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token primary_token,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): grammar order.
    struct mylite_sql_ast_node *index_name,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): grammar order.
    struct mylite_sql_ast_node *index_type,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): grammar order.
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&primary_token),
        mylite_sql_parser_span_from_token(&right_paren)
    );
    struct mylite_sql_ast_node *primary_key =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_PRIMARY_KEY_DEFINITION, span);
    if (primary_key == NULL) {
        return NULL;
    }

    (void)index_name;
    mylite_sql_ast_node_append_child(primary_key, key_parts);
    if (index_type != NULL) {
        mylite_sql_ast_node_append_child(primary_key, index_type);
    }
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(primary_key, index_options);
        mylite_sql_ast_node_set_span(
            primary_key,
            mylite_sql_parser_span_join(primary_key->span, index_options->span)
        );
    }
    return primary_key;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_primary_key_part_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *key_part
) {
    struct mylite_sql_source_span span =
        key_part == NULL ? (struct mylite_sql_source_span){0} : key_part->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_PRIMARY_KEY_PART_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, key_part);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_primary_key_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *key_part
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, key_part);
    if (key_part != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, key_part->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_secondary_index_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token index_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *index_type,
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&index_token),
        mylite_sql_parser_span_from_token(&right_paren)
    );
    struct mylite_sql_ast_node *secondary_index =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION, span);
    if (secondary_index == NULL) {
        return NULL;
    }

    if (index_name != NULL) {
        mylite_sql_ast_node_append_child(secondary_index, index_name);
    }
    if (index_type != NULL) {
        mylite_sql_ast_node_append_child(secondary_index, index_type);
    }
    mylite_sql_ast_node_append_child(secondary_index, key_parts);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(secondary_index, index_options);
        mylite_sql_ast_node_set_span(
            secondary_index,
            mylite_sql_parser_span_join(secondary_index->span, index_options->span)
        );
    }
    return secondary_index;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_unique_index_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token unique_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *index_type,
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&unique_token),
        mylite_sql_parser_span_from_token(&right_paren)
    );
    struct mylite_sql_ast_node *unique_index =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION, span);
    if (unique_index == NULL) {
        return NULL;
    }

    if (index_name != NULL) {
        mylite_sql_ast_node_append_child(unique_index, index_name);
    }
    if (index_type != NULL) {
        mylite_sql_ast_node_append_child(unique_index, index_type);
    }
    mylite_sql_ast_node_append_child(unique_index, key_parts);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(unique_index, index_options);
        mylite_sql_ast_node_set_span(
            unique_index,
            mylite_sql_parser_span_join(unique_index->span, index_options->span)
        );
    }
    return unique_index;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_fulltext_index_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token fulltext_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&fulltext_token),
        mylite_sql_parser_span_from_token(&right_paren)
    );
    struct mylite_sql_ast_node *fulltext_index =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION, span);
    if (fulltext_index == NULL) {
        return NULL;
    }

    if (index_name != NULL) {
        mylite_sql_ast_node_append_child(fulltext_index, index_name);
    }
    mylite_sql_ast_node_append_child(fulltext_index, key_parts);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(fulltext_index, index_options);
        mylite_sql_ast_node_set_span(
            fulltext_index,
            mylite_sql_parser_span_join(fulltext_index->span, index_options->span)
        );
    }
    return fulltext_index;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_spatial_index_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token spatial_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&spatial_token),
        mylite_sql_parser_span_from_token(&right_paren)
    );
    struct mylite_sql_ast_node *spatial_index =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SPATIAL_INDEX_DEFINITION, span);
    if (spatial_index == NULL) {
        return NULL;
    }

    if (index_name != NULL) {
        mylite_sql_ast_node_append_child(spatial_index, index_name);
    }
    mylite_sql_ast_node_append_child(spatial_index, key_parts);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(spatial_index, index_options);
        mylite_sql_ast_node_set_span(
            spatial_index,
            mylite_sql_parser_span_join(spatial_index->span, index_options->span)
        );
    }
    return spatial_index;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_foreign_key_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *constraint_name,
    struct mylite_sql_token foreign_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *child_parts,
    struct mylite_sql_ast_node *referenced_table,
    struct mylite_sql_ast_node *referenced_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *actions
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&foreign_token),
        mylite_sql_parser_span_from_token(&right_paren)
    );
    struct mylite_sql_ast_node *definition = NULL;

    if (constraint_name != NULL) {
        span = mylite_sql_parser_span_join(constraint_name->span, span);
    }
    if (actions != NULL) {
        span = mylite_sql_parser_span_join(span, actions->span);
    }

    definition = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_FOREIGN_KEY_DEFINITION, span);
    if (definition == NULL) {
        return NULL;
    }

    if (constraint_name != NULL) {
        mylite_sql_ast_node_append_child(definition, constraint_name);
    }
    if (index_name != NULL) {
        mylite_sql_ast_node_append_child(definition, index_name);
    }
    mylite_sql_ast_node_append_child(definition, child_parts);
    mylite_sql_ast_node_append_child(definition, referenced_table);
    mylite_sql_ast_node_append_child(definition, referenced_parts);
    if (actions != NULL) {
        mylite_sql_ast_node_append_child(definition, actions);
    }
    return definition;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_check_constraint_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *constraint_name,
    struct mylite_sql_token check_token,
    struct mylite_sql_ast_node *expression,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *enforcement
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&check_token),
        mylite_sql_parser_span_from_token(&right_paren)
    );
    struct mylite_sql_ast_node *definition = NULL;

    if (constraint_name != NULL) {
        span = mylite_sql_parser_span_join(constraint_name->span, span);
    }
    if (enforcement != NULL) {
        span = mylite_sql_parser_span_join(span, enforcement->span);
    }

    definition =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CHECK_CONSTRAINT_DEFINITION, span);
    if (definition == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(definition, expression);
    if (constraint_name != NULL) {
        mylite_sql_ast_node_append_child(definition, constraint_name);
    }
    if (enforcement != NULL) {
        mylite_sql_ast_node_append_child(definition, enforcement);
    }
    return definition;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_check_enforcement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token,
    enum mylite_sql_ast_node_kind kind
) {
    return mylite_sql_parser_make_node(state, kind, mylite_sql_parser_span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_foreign_key_part_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *key_part
) {
    struct mylite_sql_source_span span =
        key_part == NULL ? (struct mylite_sql_source_span){0} : key_part->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_FOREIGN_KEY_PART_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, key_part);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_foreign_key_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *key_part
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, key_part);
    if (key_part != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, key_part->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_foreign_key_index_name(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *identifier
) {
    struct mylite_sql_source_span span =
        identifier == NULL ? (struct mylite_sql_source_span){0} : identifier->span;
    struct mylite_sql_ast_node *index_name =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_FOREIGN_KEY_INDEX_NAME, span);
    if (index_name == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(index_name, identifier);
    return index_name;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_foreign_key_action_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *action
) {
    struct mylite_sql_source_span span =
        action == NULL ? (struct mylite_sql_source_span){0} : action->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_FOREIGN_KEY_ACTION_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, action);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_foreign_key_action(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *action
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, action);
    if (action != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, action->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_foreign_key_action(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token,
    enum mylite_sql_ast_node_kind kind
) {
    return mylite_sql_parser_make_node(
        state,
        kind,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&first_token),
            mylite_sql_parser_span_from_token(&last_token)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_secondary_index_part_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *key_part
) {
    struct mylite_sql_source_span span =
        key_part == NULL ? (struct mylite_sql_source_span){0} : key_part->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SECONDARY_INDEX_PART_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, key_part);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_secondary_index_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *key_part
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, key_part);
    if (key_part != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, key_part->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_secondary_index_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *column,
    struct mylite_sql_ast_node *prefix_length,
    struct mylite_sql_ast_node *direction
) {
    struct mylite_sql_source_span span =
        column == NULL ? (struct mylite_sql_source_span){0} : column->span;
    struct mylite_sql_ast_node *part = NULL;

    if (prefix_length != NULL) {
        span = mylite_sql_parser_span_join(span, prefix_length->span);
    }
    if (direction != NULL) {
        span = mylite_sql_parser_span_join(span, direction->span);
    }

    part = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SECONDARY_INDEX_PART, span);
    if (part == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(part, column);
    if (prefix_length != NULL) {
        mylite_sql_ast_node_append_child(part, prefix_length);
    }
    if (direction != NULL) {
        mylite_sql_ast_node_append_child(part, direction);
    }
    return part;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_functional_index_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *expression,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *direction
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&left_paren),
        mylite_sql_parser_span_from_token(&right_paren)
    );
    struct mylite_sql_ast_node *part = NULL;

    if (direction != NULL) {
        span = mylite_sql_parser_span_join(span, direction->span);
    }

    part = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SECONDARY_INDEX_PART, span);
    if (part == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(part, expression);
    if (direction != NULL) {
        mylite_sql_ast_node_append_child(part, direction);
    }
    return part;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_multi_valued_index_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_multi_valued_index_part_tokens tokens,
    struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_node_kind cast_target,
    struct mylite_sql_ast_node *direction
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&tokens.left_paren),
        mylite_sql_parser_span_from_token(&tokens.right_part_paren)
    );
    struct mylite_sql_ast_node *multi_valued = NULL;
    struct mylite_sql_ast_node *cast_type = NULL;
    struct mylite_sql_ast_node *part = NULL;

    if (direction != NULL) {
        span = mylite_sql_parser_span_join(span, direction->span);
    }

    multi_valued = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_MULTI_VALUED_INDEX_PART, span);
    if (multi_valued == NULL) {
        return NULL;
    }

    cast_type = mylite_sql_parser_make_node(
        state,
        cast_target,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&tokens.cast_token),
            mylite_sql_parser_span_from_token(&tokens.right_cast_paren)
        )
    );
    if (cast_type == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(multi_valued, expression);
    mylite_sql_ast_node_append_child(multi_valued, cast_type);

    part = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SECONDARY_INDEX_PART, span);
    if (part == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(part, multi_valued);
    if (direction != NULL) {
        mylite_sql_ast_node_append_child(part, direction);
    }
    return part;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_inline_primary_key(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token primary_token,
    struct mylite_sql_token key_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_INLINE_PRIMARY_KEY,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&primary_token),
            mylite_sql_parser_span_from_token(&key_token)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_inline_unique_key(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token unique_token,
    struct mylite_sql_token end_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_INLINE_UNIQUE_KEY,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&unique_token),
            mylite_sql_parser_span_from_token(&end_token)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_attribute_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *attribute
) {
    struct mylite_sql_source_span span =
        attribute == NULL ? (struct mylite_sql_source_span){0} : attribute->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, attribute);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_column_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *attribute
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, attribute);
    if (attribute != NULL) {
        mylite_sql_ast_node_set_span(
            list,
            mylite_sql_parser_span_join(list->span, attribute->span)
        );
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_auto_increment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token auto_increment_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT,
        mylite_sql_parser_span_from_token(&auto_increment_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_on_update_current_timestamp(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token on_token,
    struct mylite_sql_ast_node *current_timestamp_value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&on_token);
    struct mylite_sql_ast_node *on_update = NULL;

    if (current_timestamp_value != NULL) {
        span = mylite_sql_parser_span_join(span, current_timestamp_value->span);
    }
    on_update =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_COLUMN_ON_UPDATE_CURRENT_TIMESTAMP, span);
    if (on_update == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(on_update, current_timestamp_value);
    return on_update;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_charset_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token charset_token,
    struct mylite_sql_ast_node *charset_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&charset_token);
    struct mylite_sql_ast_node *attribute = NULL;

    if (charset_name != NULL) {
        span = mylite_sql_parser_span_join(span, charset_name->span);
    }

    attribute = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_COLUMN_CHARSET_ATTRIBUTE, span);
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(attribute, charset_name);
    return attribute;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_collation_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token collate_token,
    struct mylite_sql_ast_node *collation_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&collate_token);
    struct mylite_sql_ast_node *attribute = NULL;

    if (collation_name != NULL) {
        span = mylite_sql_parser_span_join(span, collation_name->span);
    }

    attribute = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_COLUMN_COLLATION_ATTRIBUTE, span);
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(attribute, collation_name);
    return attribute;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_binary_collation_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token binary_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_COLUMN_BINARY_COLLATION_ATTRIBUTE,
        mylite_sql_parser_span_from_token(&binary_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_comment_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token comment_token,
    struct mylite_sql_ast_node *comment
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&comment_token);
    struct mylite_sql_ast_node *attribute = NULL;

    if (comment != NULL) {
        span = mylite_sql_parser_span_join(span, comment->span);
    }

    attribute = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_COLUMN_COMMENT_ATTRIBUTE, span);
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(attribute, comment);
    return attribute;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_visibility_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token visibility_token,
    enum mylite_sql_ast_column_visibility visibility
) {
    struct mylite_sql_ast_node *attribute = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_COLUMN_VISIBILITY_ATTRIBUTE,
        mylite_sql_parser_span_from_token(&visibility_token)
    );
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_visibility(attribute, visibility);
    return attribute;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_srid_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token srid_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&srid_token);
    struct mylite_sql_ast_node *attribute = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    attribute = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_COLUMN_SRID_ATTRIBUTE, span);
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(attribute, value);
    return attribute;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_generated_column_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token as_token,
    struct mylite_sql_ast_node *expression,
    struct mylite_sql_token right_paren_token,
    struct mylite_sql_ast_node *storage
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&as_token);
    struct mylite_sql_ast_node *clause = NULL;

    span = mylite_sql_parser_span_join(span, mylite_sql_parser_span_from_token(&right_paren_token));
    if (storage != NULL) {
        span = mylite_sql_parser_span_join(span, storage->span);
    }

    clause = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_GENERATED_COLUMN_CLAUSE, span);
    if (clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(clause, expression);
    mylite_sql_ast_node_append_child(clause, storage);
    return clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_generated_column_storage(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token,
    enum mylite_sql_ast_node_kind kind
) {
    return mylite_sql_parser_make_node(state, kind, mylite_sql_parser_span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *column_type,
    struct mylite_sql_ast_node *nullability,
    struct mylite_sql_ast_node *default_null,
    struct mylite_sql_ast_node *primary_key
) {
    struct mylite_sql_source_span span =
        name == NULL ? (struct mylite_sql_source_span){0} : name->span;
    struct mylite_sql_ast_node *column = NULL;

    if (column_type != NULL) {
        span = mylite_sql_parser_span_join(span, column_type->span);
    }
    if (nullability != NULL) {
        span = mylite_sql_parser_span_join(span, nullability->span);
    }
    if (default_null != NULL) {
        span = mylite_sql_parser_span_join(span, default_null->span);
    }
    if (primary_key != NULL) {
        span = mylite_sql_parser_span_join(span, primary_key->span);
    }

    column = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_COLUMN_DEFINITION, span);
    if (column == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(column, name);
    mylite_sql_ast_node_append_child(column, column_type);
    mylite_sql_ast_node_append_child(column, nullability);
    mylite_sql_ast_node_append_child(column, default_null);
    mylite_sql_ast_node_append_child(column, primary_key);
    return column;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_definition_with_attributes(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *column_type,
    struct mylite_sql_ast_node *attributes
) {
    struct mylite_sql_source_span span =
        name == NULL ? (struct mylite_sql_source_span){0} : name->span;
    struct column_attribute_positions positions = {0};
    struct mylite_sql_ast_node *column = NULL;
    struct mylite_sql_ast_node *attribute = NULL;
    int rc = MYLITE_SQL_PARSE_OK;

    if (column_type != NULL) {
        span = mylite_sql_parser_span_join(span, column_type->span);
    }
    if (attributes != NULL) {
        span = mylite_sql_parser_span_join(span, attributes->span);
    }

    rc = scan_column_attribute_positions(state, attributes, &positions);
    if (rc == MYLITE_SQL_PARSE_OK) {
        rc = validate_legacy_column_attribute_order(state, &positions);
    }
    if (rc != MYLITE_SQL_PARSE_OK) {
        return NULL;
    }

    column = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_COLUMN_DEFINITION, span);
    if (column == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(column, name);
    mylite_sql_ast_node_append_child(column, column_type);
    attribute = attributes == NULL ? NULL : attributes->first_child;
    while (attribute != NULL) {
        struct mylite_sql_ast_node *next = attribute->next_sibling;

        mylite_sql_ast_node_append_child(column, attribute);
        attribute = next;
    }

    return column;
}

static int scan_column_attribute_positions(
    struct mylite_sql_parser_state *state,
    const struct mylite_sql_ast_node *attributes,
    struct column_attribute_positions *out_positions
) {
    const struct mylite_sql_ast_node *attribute = NULL;
    size_t position = 0U;
    int rc = MYLITE_SQL_PARSE_OK;

    *out_positions = (struct column_attribute_positions){
        .charset = (size_t)-1,
        .collation = (size_t)-1,
        .binary_collation = (size_t)-1,
        .comment = (size_t)-1,
        .nullability = (size_t)-1,
        .default_value = (size_t)-1,
        .primary_key = (size_t)-1,
        .unique_key = (size_t)-1,
        .auto_increment = (size_t)-1,
        .generated = (size_t)-1,
        .visibility = (size_t)-1,
        .srid = (size_t)-1,
    };

    attribute = attributes == NULL ? NULL : attributes->first_child;
    while (rc == MYLITE_SQL_PARSE_OK && attribute != NULL) {
        switch (attribute->kind) {
        case MYLITE_SQL_AST_COLUMN_CHARSET_ATTRIBUTE:
            rc = record_column_attribute_position(state, &out_positions->charset, position);
            break;
        case MYLITE_SQL_AST_COLUMN_BINARY_COLLATION_ATTRIBUTE:
            rc =
                record_column_attribute_position(state, &out_positions->binary_collation, position);
            if (rc != MYLITE_SQL_PARSE_OK) {
                break;
            }
            rc = record_column_attribute_position(state, &out_positions->collation, position);
            break;
        case MYLITE_SQL_AST_COLUMN_COLLATION_ATTRIBUTE:
            rc = record_column_attribute_position(state, &out_positions->collation, position);
            break;
        case MYLITE_SQL_AST_COLUMN_COMMENT_ATTRIBUTE:
            if (!column_attribute_position_is_set(out_positions->comment)) {
                out_positions->comment = position;
            }
            break;
        case MYLITE_SQL_AST_NULLABILITY:
            out_positions->nullability = position;
            break;
        case MYLITE_SQL_AST_COLUMN_DEFAULT_NULL:
        case MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE:
            rc = record_column_attribute_position(state, &out_positions->default_value, position);
            break;
        case MYLITE_SQL_AST_INLINE_PRIMARY_KEY:
            rc = record_column_attribute_position(state, &out_positions->primary_key, position);
            break;
        case MYLITE_SQL_AST_INLINE_UNIQUE_KEY:
            rc = record_column_attribute_position(state, &out_positions->unique_key, position);
            break;
        case MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT:
            rc = record_column_attribute_position(state, &out_positions->auto_increment, position);
            break;
        case MYLITE_SQL_AST_GENERATED_COLUMN_CLAUSE:
            rc = record_column_attribute_position(state, &out_positions->generated, position);
            break;
        case MYLITE_SQL_AST_COLUMN_VISIBILITY_ATTRIBUTE:
            rc = record_column_attribute_position(state, &out_positions->visibility, position);
            break;
        case MYLITE_SQL_AST_COLUMN_SRID_ATTRIBUTE:
            rc = record_column_attribute_position(state, &out_positions->srid, position);
            break;
        default:
            break;
        }

        ++position;
        attribute = attribute->next_sibling;
    }

    return rc;
}

static int record_column_attribute_position(
    struct mylite_sql_parser_state *state,
    size_t *slot,
    size_t position
) {
    if (column_attribute_position_is_set(*slot)) {
        mylite_sql_parser_set_state_status(state, MYLITE_SQL_PARSE_SYNTAX_ERROR);
        return MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }

    *slot = position;
    return MYLITE_SQL_PARSE_OK;
}

static int validate_legacy_column_attribute_order(
    struct mylite_sql_parser_state *state,
    const struct column_attribute_positions *positions
) {
    bool invalid_order = false;

    if (column_attribute_position_is_set(positions->auto_increment)) {
        if (!column_attribute_position_is_set(positions->charset) &&
            !column_attribute_position_is_set(positions->collation)) {
            return MYLITE_SQL_PARSE_OK;
        }
    }
    if (!column_attribute_charset_order_is_valid(positions)) {
        invalid_order = true;
    }
    if (column_attribute_position_is_set(positions->generated) &&
        ((column_attribute_position_is_set(positions->nullability) &&
          positions->nullability < positions->generated) ||
         (column_attribute_position_is_set(positions->default_value) &&
          positions->default_value < positions->generated) ||
         (column_attribute_position_is_set(positions->primary_key) &&
          positions->primary_key < positions->generated) ||
         (column_attribute_position_is_set(positions->unique_key) &&
          positions->unique_key < positions->generated) ||
         (column_attribute_position_is_set(positions->auto_increment) &&
          positions->auto_increment < positions->generated) ||
         (column_attribute_position_is_set(positions->comment) &&
          positions->comment < positions->generated))) {
        invalid_order = true;
    }
    if (legacy_column_attribute_precedes_charset(positions)) {
        invalid_order = true;
    }
    if (invalid_order) {
        mylite_sql_parser_set_state_status(state, MYLITE_SQL_PARSE_SYNTAX_ERROR);
        return MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }
    if (column_attribute_position_is_set(positions->auto_increment)) {
        return MYLITE_SQL_PARSE_OK;
    }

    return MYLITE_SQL_PARSE_OK;
}

static bool column_attribute_charset_order_is_valid(
    const struct column_attribute_positions *positions
) {
    if (!column_attribute_position_is_set(positions->charset) ||
        !column_attribute_position_is_set(positions->collation) ||
        positions->charset <= positions->collation) {
        return true;
    }

    return column_attribute_position_is_set(positions->binary_collation) &&
           positions->binary_collation == positions->collation &&
           positions->charset == positions->collation + 1U;
}

static bool legacy_column_attribute_precedes_charset(
    const struct column_attribute_positions *positions
) {
    if (positions == NULL || !column_attribute_position_is_set(positions->charset)) {
        return false;
    }

    return legacy_column_attribute_precedes_position(positions, positions->charset);
}

static bool column_attribute_position_precedes(size_t position, size_t limit) {
    return column_attribute_position_is_set(position) && position < limit;
}

static bool legacy_column_attribute_precedes_position(
    const struct column_attribute_positions *positions,
    size_t limit
) {
    return column_attribute_position_precedes(positions->nullability, limit) ||
           column_attribute_position_precedes(positions->default_value, limit) ||
           column_attribute_position_precedes(positions->primary_key, limit) ||
           column_attribute_position_precedes(positions->unique_key, limit) ||
           column_attribute_position_precedes(positions->comment, limit) ||
           column_attribute_position_precedes(positions->auto_increment, limit);
}

static bool column_attribute_position_is_set(size_t position) {
    return position != (size_t)-1;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_default_null(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token default_token,
    struct mylite_sql_token null_token
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&default_token),
        mylite_sql_parser_span_from_token(&null_token)
    );

    return mylite_sql_parser_make_node(state, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_default_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token default_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&default_token);
    struct mylite_sql_ast_node *default_value = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    default_value = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE, span);
    if (default_value == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(default_value, value);
    return default_value;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_integer_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token type_token,
    enum mylite_sql_ast_integer_type integer_type,
    struct mylite_sql_token display_width_token,
    struct mylite_sql_token display_width_end_token,
    struct mylite_sql_token attribute_token,
    int is_unsigned,
    int is_bool_alias,
    int is_serial_alias
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&type_token);
    struct mylite_sql_ast_node *type = NULL;

    if (display_width_end_token.text != NULL) {
        span = mylite_sql_parser_span_join(
            span,
            mylite_sql_parser_span_from_token(&display_width_end_token)
        );
    }
    if (attribute_token.text != NULL) {
        span =
            mylite_sql_parser_span_join(span, mylite_sql_parser_span_from_token(&attribute_token));
    }

    type = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_INTEGER_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_integer_type(
        type,
        (struct mylite_sql_ast_integer_type_payload){
            .kind = integer_type,
            .is_unsigned = is_unsigned,
            .has_display_width = display_width_token.text != NULL,
            .is_bool_alias = is_bool_alias,
            .is_serial_alias = is_serial_alias,
            .display_width_span = mylite_sql_parser_span_from_token(&display_width_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_varchar_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_varchar_type_tokens tokens
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&tokens.type_token),
        mylite_sql_parser_span_from_token(&tokens.end_token)
    );
    struct mylite_sql_ast_node *type =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_VARCHAR_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_varchar_type(
        type,
        (struct mylite_sql_ast_varchar_type_payload){
            .is_national = tokens.is_national,
            .length_span = mylite_sql_parser_span_from_token(&tokens.length_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_char_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_char_type_tokens tokens
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&tokens.type_token);
    struct mylite_sql_ast_node *type = NULL;

    if (tokens.has_explicit_length ||
        (tokens.end_token.text != NULL && tokens.end_token.text != tokens.type_token.text)) {
        span =
            mylite_sql_parser_span_join(span, mylite_sql_parser_span_from_token(&tokens.end_token));
    }

    type = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CHAR_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_char_type(
        type,
        (struct mylite_sql_ast_char_type_payload){
            .has_explicit_length = tokens.has_explicit_length,
            .is_national = tokens.is_national,
            .length_span = mylite_sql_parser_span_from_token(&tokens.length_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_text_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_text_type_tokens tokens
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&tokens.type_token);
    struct mylite_sql_ast_node *type = NULL;

    if (tokens.end_token.text != NULL) {
        span =
            mylite_sql_parser_span_join(span, mylite_sql_parser_span_from_token(&tokens.end_token));
    }

    type = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TEXT_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_text_type(
        type,
        (struct mylite_sql_ast_text_type_payload){
            .kind = tokens.text_type,
            .has_length = tokens.has_length,
            .length_span = mylite_sql_parser_span_from_token(&tokens.length_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_json_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token type_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_JSON_TYPE,
        mylite_sql_parser_span_from_token(&type_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_spatial_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_spatial_type_tokens tokens
) {
    struct mylite_sql_ast_node *type = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_SPATIAL_TYPE,
        mylite_sql_parser_span_from_token(&tokens.type_token)
    );
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_spatial_type(type, tokens.spatial_type);
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_enum_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token type_token,
    struct mylite_sql_ast_node *label_list,
    struct mylite_sql_token end_token
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&type_token),
        mylite_sql_parser_span_from_token(&end_token)
    );
    struct mylite_sql_ast_node *type =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ENUM_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(type, label_list);
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_enum_label_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *label
) {
    struct mylite_sql_ast_node *list = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_ENUM_LABEL_LIST,
        label == NULL ? (struct mylite_sql_source_span){0} : label->span
    );
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, label);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_enum_label(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *label_list,
    struct mylite_sql_ast_node *label
) {
    if (!mylite_sql_parser_is_parse_ok(state) || label_list == NULL) {
        return label_list;
    }

    mylite_sql_ast_node_append_child(label_list, label);
    if (label != NULL) {
        mylite_sql_ast_node_set_span(
            label_list,
            mylite_sql_parser_span_join(label_list->span, label->span)
        );
    }
    return label_list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token type_token,
    struct mylite_sql_ast_node *member_list,
    struct mylite_sql_token end_token
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&type_token),
        mylite_sql_parser_span_from_token(&end_token)
    );
    struct mylite_sql_ast_node *type =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SET_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(type, member_list);
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_member_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *member
) {
    struct mylite_sql_ast_node *list = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_SET_MEMBER_LIST,
        member == NULL ? (struct mylite_sql_source_span){0} : member->span
    );
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, member);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_set_member(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *member_list,
    struct mylite_sql_ast_node *member
) {
    if (!mylite_sql_parser_is_parse_ok(state) || member_list == NULL) {
        return member_list;
    }

    mylite_sql_ast_node_append_child(member_list, member);
    if (member != NULL) {
        mylite_sql_ast_node_set_span(
            member_list,
            mylite_sql_parser_span_join(member_list->span, member->span)
        );
    }
    return member_list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_binary_string_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_binary_string_type_tokens tokens
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&tokens.type_token);
    struct mylite_sql_ast_node *type = NULL;

    if (tokens.end_token.text != NULL) {
        span =
            mylite_sql_parser_span_join(span, mylite_sql_parser_span_from_token(&tokens.end_token));
    }

    type = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_BINARY_STRING_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_binary_string_type(
        type,
        (struct mylite_sql_ast_binary_string_type_payload){
            .kind = tokens.binary_string_type,
            .has_length = tokens.has_length,
            .length_span = mylite_sql_parser_span_from_token(&tokens.length_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_bit_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_bit_type_tokens tokens
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&tokens.type_token);
    struct mylite_sql_ast_node *type = NULL;

    if (tokens.end_token.text != NULL) {
        span =
            mylite_sql_parser_span_join(span, mylite_sql_parser_span_from_token(&tokens.end_token));
    }

    type = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_BIT_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_bit_type(
        type,
        (struct mylite_sql_ast_bit_type_payload){
            .has_length = tokens.has_length,
            .length_span = mylite_sql_parser_span_from_token(&tokens.length_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_year_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_year_type_tokens tokens
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&tokens.type_token);
    struct mylite_sql_ast_node *type = NULL;

    if (tokens.end_token.text != NULL) {
        span =
            mylite_sql_parser_span_join(span, mylite_sql_parser_span_from_token(&tokens.end_token));
    }

    type = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_YEAR_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_year_type(
        type,
        (struct mylite_sql_ast_year_type_payload){
            .has_width = tokens.has_width,
            .width_span = mylite_sql_parser_span_from_token(&tokens.width_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_decimal_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_decimal_type_tokens tokens
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&tokens.type_token),
        mylite_sql_parser_span_from_token(&tokens.end_token)
    );
    struct mylite_sql_ast_node *type =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_DECIMAL_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_decimal_type(
        type,
        (struct mylite_sql_ast_decimal_type_payload){
            .kind = tokens.decimal_type,
            .has_precision = tokens.has_precision,
            .has_scale = tokens.has_scale,
            .is_unsigned = tokens.is_unsigned,
            .precision_span = mylite_sql_parser_span_from_token(&tokens.precision_token),
            .scale_span = mylite_sql_parser_span_from_token(&tokens.scale_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_approximate_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_approximate_type_tokens tokens
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&tokens.type_token),
        mylite_sql_parser_span_from_token(&tokens.end_token)
    );
    struct mylite_sql_ast_node *type =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_APPROXIMATE_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_approximate_type(
        type,
        (struct mylite_sql_ast_approximate_type_payload){
            .kind = tokens.approximate_type,
            .has_precision = tokens.has_precision,
            .has_scale = tokens.has_scale,
            .is_unsigned = tokens.is_unsigned,
            .precision_span = mylite_sql_parser_span_from_token(&tokens.precision_token),
            .scale_span = mylite_sql_parser_span_from_token(&tokens.scale_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_date_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token date_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_DATE_TYPE,
        mylite_sql_parser_span_from_token(&date_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_datetime_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_temporal_type_tokens tokens
) {
    struct mylite_sql_ast_node *type = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_DATETIME_TYPE,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&tokens.type_token),
            mylite_sql_parser_span_from_token(&tokens.end_token)
        )
    );
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_temporal_fractional_precision(
        type,
        (struct mylite_sql_ast_temporal_fractional_precision_payload){
            .has_precision = tokens.has_precision,
            .precision_span = mylite_sql_parser_span_from_token(&tokens.precision_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_timestamp_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_temporal_type_tokens tokens
) {
    struct mylite_sql_ast_node *type = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_TIMESTAMP_TYPE,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&tokens.type_token),
            mylite_sql_parser_span_from_token(&tokens.end_token)
        )
    );
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_temporal_fractional_precision(
        type,
        (struct mylite_sql_ast_temporal_fractional_precision_payload){
            .has_precision = tokens.has_precision,
            .precision_span = mylite_sql_parser_span_from_token(&tokens.precision_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_time_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_temporal_type_tokens tokens
) {
    struct mylite_sql_ast_node *type = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_TIME_TYPE,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&tokens.type_token),
            mylite_sql_parser_span_from_token(&tokens.end_token)
        )
    );
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_temporal_fractional_precision(
        type,
        (struct mylite_sql_ast_temporal_fractional_precision_payload){
            .has_precision = tokens.has_precision,
            .precision_span = mylite_sql_parser_span_from_token(&tokens.precision_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_nullability(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_nullability nullability,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token
) {
    struct mylite_sql_ast_node *node = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_NULLABILITY,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&first_token),
            mylite_sql_parser_span_from_token(&last_token)
        )
    );
    if (node == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_nullability(node, nullability);
    return node;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_identifier_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *identifier
) {
    struct mylite_sql_source_span span =
        identifier == NULL ? (struct mylite_sql_source_span){0} : identifier->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_IDENTIFIER_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, identifier);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_empty_identifier_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_paren,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *list = mylite_sql_parser_make_identifier_list(state, NULL);

    if (list == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_set_span(
        list,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&left_paren),
            mylite_sql_parser_span_from_token(&right_paren)
        )
    );
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_identifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *identifier
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, identifier);
    if (identifier != NULL) {
        mylite_sql_ast_node_set_span(
            list,
            mylite_sql_parser_span_join(list->span, identifier->span)
        );
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_row_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *row
) {
    struct mylite_sql_source_span span =
        row == NULL ? (struct mylite_sql_source_span){0} : row->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_INSERT_ROW_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, row);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_values_row_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *row
) {
    struct mylite_sql_source_span span =
        row == NULL ? (struct mylite_sql_source_span){0} : row->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_VALUES_ROW_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, row);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_insert_row(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *row
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, row);
    if (row != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, row->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_values_row(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *row
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, row);
    if (row != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, row->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_row_values(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        value == NULL ? (struct mylite_sql_source_span){0} : value->span;
    struct mylite_sql_ast_node *row =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_INSERT_ROW, span);
    if (row == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(row, value);
    return row;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_values_row_values(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        value == NULL ? (struct mylite_sql_source_span){0} : value->span;
    struct mylite_sql_ast_node *row =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_VALUES_ROW, span);
    if (row == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(row, value);
    return row;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_insert_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *row,
    struct mylite_sql_ast_node *value
) {
    if (!mylite_sql_parser_is_parse_ok(state) || row == NULL) {
        return row;
    }

    mylite_sql_ast_node_append_child(row, value);
    if (value != NULL) {
        mylite_sql_ast_node_set_span(row, mylite_sql_parser_span_join(row->span, value->span));
    }
    return row;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_values_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *row,
    struct mylite_sql_ast_node *value
) {
    if (!mylite_sql_parser_is_parse_ok(state) || row == NULL) {
        return row;
    }

    mylite_sql_ast_node_append_child(row, value);
    if (value != NULL) {
        mylite_sql_ast_node_set_span(row, mylite_sql_parser_span_join(row->span, value->span));
    }
    return row;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_row(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *values,
    struct mylite_sql_token right_paren
) {
    if (!mylite_sql_parser_is_parse_ok(state) || values == NULL) {
        return values;
    }

    mylite_sql_ast_node_set_span(
        values,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&left_paren),
            mylite_sql_parser_span_from_token(&right_paren)
        )
    );
    return values;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_values_row(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token row_token,
    struct mylite_sql_ast_node *values,
    struct mylite_sql_token right_paren
) {
    if (!mylite_sql_parser_is_parse_ok(state) || values == NULL) {
        return values;
    }

    mylite_sql_ast_node_set_span(
        values,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&row_token),
            mylite_sql_parser_span_from_token(&right_paren)
        )
    );
    return values;
}
