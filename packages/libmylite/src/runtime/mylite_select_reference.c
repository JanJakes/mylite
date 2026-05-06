#include "mylite_select.h"

#include "mylite_span.h"

#include <stdlib.h>
#include <string.h>

int mylite_select_resolve_column_reference(
    const struct mylite_select_table *table,
    const struct mylite_sql_ast_node *expression,
    size_t *out_index
) {
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = mylite_copy_identifier_parts(expression, parts, &part_count);

    *out_index = table->column_count;
    if (status != MYLITE_OK) {
        return status;
    }

    if (part_count >= 1U && part_count <= 3U &&
        mylite_select_reference_qualifiers_match(table, parts, part_count)) {
        *out_index = mylite_select_column_index(table, parts[part_count - 1U]);
    }

    for (size_t index = 0U; index < part_count && index < 3U; ++index) {
        free(parts[index]);
    }
    return MYLITE_OK;
}

bool mylite_select_reference_qualifiers_match(
    const struct mylite_select_table *table,
    char **parts,
    size_t part_count
) {
    if (part_count == 1U) {
        return true;
    }
    if (part_count == 2U) {
        const char *visible_table = table->alias == NULL ? table->table_name : table->alias;

        if (strcmp(parts[0], visible_table) == 0) {
            return true;
        }
        return false;
    }
    if (part_count == 3U && table->alias == NULL) {
        if (strcmp(parts[0], table->schema_name) == 0 && strcmp(parts[1], table->table_name) == 0) {
            return true;
        }
        return false;
    }
    return false;
}

size_t mylite_select_column_index(
    const struct mylite_select_table *table,
    const char *column_name
) {
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (mylite_ascii_case_equal(table->columns[index].name, column_name)) {
            return index;
        }
    }
    return table->column_count;
}

char *mylite_select_copy_reference_name(const struct mylite_sql_ast_node *identifier) {
    char *parts[3] = {0};
    size_t part_count = 0U;
    size_t length = 0U;
    char *name = NULL;
    int status = mylite_copy_identifier_parts(identifier, parts, &part_count);

    if (status != MYLITE_OK) {
        for (size_t index = 0U; index < part_count; ++index) {
            free(parts[index]);
        }
        if (status == MYLITE_NOMEM) {
            return NULL;
        }
        return mylite_copy_span_text(identifier->span.text, identifier->span.length);
    }
    if (part_count == 0U) {
        return mylite_copy_span_text(identifier->span.text, identifier->span.length);
    }

    for (size_t index = 0U; index < part_count; ++index) {
        length += strlen(parts[index]);
        if (index != 0U) {
            length += 1U;
        }
    }

    name = malloc(length + 1U);
    if (name != NULL) {
        size_t offset = 0U;

        for (size_t index = 0U; index < part_count; ++index) {
            size_t part_length = strlen(parts[index]);

            if (index != 0U) {
                name[offset++] = '.';
            }
            memcpy(name + offset, parts[index], part_length);
            offset += part_length;
        }
        name[offset] = '\0';
    }

    for (size_t index = 0U; index < part_count; ++index) {
        free(parts[index]);
    }
    return name;
}

char *mylite_select_copy_expression_label(const struct mylite_sql_ast_node *expression) {
    if (expression == NULL) {
        return NULL;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL &&
        expression->literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        return mylite_copy_string_literal_span(expression);
    }
    return mylite_copy_span_text(expression->span.text, expression->span.length);
}

char *mylite_select_copy_alias(const struct mylite_sql_ast_node *alias) {
    if (alias == NULL) {
        return NULL;
    }
    if (alias->kind == MYLITE_SQL_AST_LITERAL &&
        alias->literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        return mylite_copy_string_literal_span(alias);
    }
    if (alias->kind == MYLITE_SQL_AST_IDENTIFIER) {
        return mylite_copy_identifier_span(alias);
    }
    return NULL;
}
