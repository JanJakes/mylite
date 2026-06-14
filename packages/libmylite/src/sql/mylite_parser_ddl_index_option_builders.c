#include "mylite_parse.h"
#include "mylite_parser_helpers.h"
#include "mylite_parser_internal.h"

#include <stdbool.h>

struct mylite_sql_ast_node *mylite_sql_parser_make_create_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    bool is_unique,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *index_type,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *part_list,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;
    enum mylite_sql_ast_node_kind statement_kind = MYLITE_SQL_AST_CREATE_INDEX_STATEMENT;

    if (index_options != NULL) {
        span = mylite_sql_parser_span_join(span, index_options->span);
    } else if (part_list != NULL) {
        span = mylite_sql_parser_span_join(span, part_list->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    } else if (index_type != NULL) {
        span = mylite_sql_parser_span_join(span, index_type->span);
    } else if (index_name != NULL) {
        span = mylite_sql_parser_span_join(span, index_name->span);
    }

    if (is_unique) {
        statement_kind = MYLITE_SQL_AST_CREATE_UNIQUE_INDEX_STATEMENT;
    }
    statement = mylite_sql_parser_make_node(state, statement_kind, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, index_name);
    if (index_type != NULL) {
        mylite_sql_ast_node_append_child(statement, index_type);
    }
    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, part_list);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(statement, index_options);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_fulltext_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *part_list,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (index_options != NULL) {
        span = mylite_sql_parser_span_join(span, index_options->span);
    } else if (part_list != NULL) {
        span = mylite_sql_parser_span_join(span, part_list->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    } else if (index_name != NULL) {
        span = mylite_sql_parser_span_join(span, index_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CREATE_FULLTEXT_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, index_name);
    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, part_list);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(statement, index_options);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_spatial_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *part_list,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (index_options != NULL) {
        span = mylite_sql_parser_span_join(span, index_options->span);
    } else if (part_list != NULL) {
        span = mylite_sql_parser_span_join(span, part_list->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    } else if (index_name != NULL) {
        span = mylite_sql_parser_span_join(span, index_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CREATE_SPATIAL_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, index_name);
    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, part_list);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(statement, index_options);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *table_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&drop_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    } else if (index_name != NULL) {
        span = mylite_sql_parser_span_join(span, index_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_DROP_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, index_name);
    mylite_sql_ast_node_append_child(statement, table_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_if_not_exists_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token if_token,
    struct mylite_sql_token exists_token
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&if_token),
        mylite_sql_parser_span_from_token(&exists_token)
    );

    return mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_option_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *option
) {
    struct mylite_sql_source_span span =
        option == NULL ? (struct mylite_sql_source_span){0} : option->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_OPTION_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, option);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_table_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *option
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, option);
    if (option != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, option->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_engine_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token engine_token,
    struct mylite_sql_ast_node *engine_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&engine_token);
    struct mylite_sql_ast_node *option = NULL;

    if (engine_name != NULL) {
        span = mylite_sql_parser_span_join(span, engine_name->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_ENGINE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, engine_name);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_charset_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token charset_token,
    struct mylite_sql_ast_node *charset_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&charset_token);
    struct mylite_sql_ast_node *option = NULL;

    if (charset_name != NULL) {
        span = mylite_sql_parser_span_join(span, charset_name->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_CHARSET_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, charset_name);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_collation_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token collate_token,
    struct mylite_sql_ast_node *collation_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&collate_token);
    struct mylite_sql_ast_node *option = NULL;

    if (collation_name != NULL) {
        span = mylite_sql_parser_span_join(span, collation_name->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_COLLATION_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, collation_name);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_auto_increment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token auto_increment_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&auto_increment_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_AUTO_INCREMENT_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_comment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token comment_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&comment_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_COMMENT_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_row_format_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token row_format_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&row_format_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_ROW_FORMAT_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_key_block_size_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token key_block_size_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&key_block_size_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_KEY_BLOCK_SIZE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_pack_keys_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token pack_keys_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&pack_keys_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_PACK_KEYS_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_checksum_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token checksum_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&checksum_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_CHECKSUM_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_stats_persistent_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token stats_persistent_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&stats_persistent_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_STATS_PERSISTENT_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_stats_auto_recalc_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token stats_auto_recalc_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        mylite_sql_parser_span_from_token(&stats_auto_recalc_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_STATS_AUTO_RECALC_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_stats_sample_pages_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token stats_sample_pages_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        mylite_sql_parser_span_from_token(&stats_sample_pages_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_STATS_SAMPLE_PAGES_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_min_rows_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token min_rows_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&min_rows_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_MIN_ROWS_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_max_rows_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token max_rows_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&max_rows_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_MAX_ROWS_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_avg_row_length_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token avg_row_length_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&avg_row_length_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_AVG_ROW_LENGTH_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_delay_key_write_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token delay_key_write_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&delay_key_write_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_DELAY_KEY_WRITE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_tablespace_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token tablespace_token,
    struct mylite_sql_ast_node *tablespace_name,
    struct mylite_sql_ast_node *storage
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&tablespace_token);
    struct mylite_sql_ast_node *option = NULL;

    if (storage != NULL) {
        span = mylite_sql_parser_span_join(span, storage->span);
    } else if (tablespace_name != NULL) {
        span = mylite_sql_parser_span_join(span, tablespace_name->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_TABLESPACE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, tablespace_name);
    if (storage != NULL) {
        mylite_sql_ast_node_append_child(option, storage);
    }
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_union_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token union_token,
    struct mylite_sql_ast_node *table_names,
    struct mylite_sql_token end_token
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&union_token),
        mylite_sql_parser_span_from_token(&end_token)
    );
    struct mylite_sql_ast_node *option =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_UNION_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, table_names);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_insert_method_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token insert_method_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&insert_method_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_INSERT_METHOD_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_storage_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token storage_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&storage_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_STORAGE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_option_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *option
) {
    struct mylite_sql_source_span span =
        option == NULL ? (struct mylite_sql_source_span){0} : option->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_INDEX_OPTION_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, option);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_index_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *option
) {
    (void)state;
    mylite_sql_ast_node_append_child(list, option);
    if (option != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, option->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_type_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token using_token,
    struct mylite_sql_ast_node *type_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&using_token);
    struct mylite_sql_ast_node *option = NULL;

    if (type_name != NULL) {
        span = mylite_sql_parser_span_join(span, type_name->span);
    }
    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_INDEX_TYPE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, type_name);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_comment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token comment_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&comment_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }
    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_INDEX_COMMENT_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_visibility_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token visibility_token,
    enum mylite_sql_ast_column_visibility visibility
) {
    struct mylite_sql_ast_node *option = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_INDEX_VISIBILITY_OPTION,
        mylite_sql_parser_span_from_token(&visibility_token)
    );
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_visibility(option, visibility);
    return option;
}
