#include "mylite_execution_text_internal.h"

#include "mylite_execution_ast_internal.h"
#include "mylite_execution_diagnostics.h"

#include <mylite/mylite.h>

#include <stdlib.h>
#include <string.h>

int mylite_execution_copy_source_span_text(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_text
) {
    char *text = NULL;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (span == NULL || span->text == NULL || span->length == 0U) {
        mylite_execution_set_parse_result_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (span->length == SIZE_MAX) {
        mylite_execution_diagnostics_set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    text = (char *)malloc(span->length + 1U);
    if (text == NULL) {
        mylite_execution_diagnostics_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(text, span->text, span->length);
    text[span->length] = '\0';
    *out_text = text;

    return MYLITE_OK;
}

int mylite_execution_copy_identifier_text(
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size,
    struct mylite_db *database
) {
    const char *source = NULL;
    size_t source_size = 0U;
    int rc = MYLITE_OK;

    if (node == NULL || node->kind != MYLITE_SQL_AST_IDENTIFIER || destination == NULL) {
        mylite_execution_set_parse_result_error(database, NULL);
        return MYLITE_ERROR;
    }

    source = node->span.text;
    source_size = node->span.length;
    if (source == NULL || source_size == 0U) {
        mylite_execution_set_parse_result_error(database, NULL);
        return MYLITE_ERROR;
    }

    if (source[0] == '`' || source[0] == '"') {
        rc = mylite_execution_copy_quoted_identifier_text(
            source,
            source_size,
            destination,
            destination_size
        );
    } else {
        rc = mylite_execution_copy_unquoted_identifier_text(
            source,
            source_size,
            destination,
            destination_size
        );
    }
    if (rc != MYLITE_OK) {
        mylite_execution_diagnostics_set_identifier_too_long_error(database, "identifier");
        return rc;
    }

    return MYLITE_OK;
}

int mylite_execution_copy_quoted_identifier_text(
    const char *source,
    size_t source_size,
    char *destination,
    size_t destination_size
) {
    size_t destination_index = 0U;
    char quote = '\0';

    if (source_size > 0U) {
        quote = source[0];
    }

    if ((quote != '`' && quote != '"') || source_size < 2U || source[source_size - 1U] != quote) {
        return MYLITE_ERROR;
    }

    for (size_t source_index = 1U; source_index + 1U < source_size; ++source_index) {
        if (destination_index + 1U >= destination_size) {
            return MYLITE_ERROR;
        }
        if (source[source_index] == quote && source[source_index + 1U] == quote) {
            destination[destination_index] = quote;
            ++source_index;
        } else {
            destination[destination_index] = source[source_index];
        }
        ++destination_index;
    }
    destination[destination_index] = '\0';

    return destination_index == 0U ? MYLITE_ERROR : MYLITE_OK;
}

int mylite_execution_copy_unquoted_identifier_text(
    const char *source,
    size_t source_size,
    char *destination,
    size_t destination_size
) {
    if (source_size == 0U || source_size >= destination_size) {
        return MYLITE_ERROR;
    }

    memcpy(destination, source, source_size);
    destination[source_size] = '\0';

    return MYLITE_OK;
}

int mylite_execution_duplicate_text(
    struct mylite_db *database,
    const char *source,
    char **out_text
) {
    size_t length = 0U;
    char *text = NULL;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (source == NULL) {
        mylite_execution_set_parse_result_error(database, NULL);
        return MYLITE_ERROR;
    }

    length = strlen(source);
    if (length == SIZE_MAX) {
        mylite_execution_diagnostics_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    text = (char *)malloc(length + 1U);
    if (text == NULL) {
        mylite_execution_diagnostics_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(text, source, length + 1U);
    *out_text = text;

    return MYLITE_OK;
}
