#include "mylite_select.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_span.h"

#include <stdlib.h>
#include <string.h>

static int compare_select_text_values(const char *left, size_t left_length, const char *right,
                                      size_t right_length);
static size_t expression_value_text_length(const struct mylite_expression_value *value);
static size_t nullable_text_length(const char *text);

void mylite_select_plan_deinit(struct mylite_select_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    mylite_select_table_deinit(&plan->table);
    for (size_t index = 0U; index < plan->table_count; ++index) {
        mylite_select_table_deinit(&plan->tables[index]);
    }
    free(plan->tables);
    free(plan->from_ranges);
    free(plan->join_steps);
    for (size_t index = 0U; index < plan->output_count; ++index) {
        mylite_select_output_column_deinit(&plan->outputs[index]);
    }
    free(plan->outputs);
    free(plan->order_keys);
    free(plan->group_keys);
    for (size_t index = 0U; index < plan->aggregate_binding_count; ++index) {
        mylite_select_aggregate_binding_deinit(&plan->aggregate_bindings[index]);
    }
    free(plan->aggregate_bindings);
    free(plan->join_predicates);
    for (size_t index = 0U; index < plan->using_column_count; ++index) {
        free(plan->using_columns[index].name);
    }
    free(plan->using_columns);
    for (size_t index = 0U; index < plan->using_request_count; ++index) {
        for (size_t name_index = 0U; name_index < plan->using_requests[index].name_count;
             ++name_index) {
            free(plan->using_requests[index].names[name_index]);
        }
        free((void *)plan->using_requests[index].names);
    }
    free(plan->using_requests);
    *plan = (struct mylite_select_plan){0};
}

void mylite_select_table_deinit(struct mylite_select_table *table)
{
    if (table == NULL) {
        return;
    }

    free(table->schema_name);
    free(table->table_name);
    free(table->alias);
    free(table->physical_name);
    for (size_t index = 0U; index < table->column_count; ++index) {
        mylite_select_column_deinit(&table->columns[index]);
    }
    free(table->columns);
    *table = (struct mylite_select_table){0};
}

void mylite_select_column_deinit(struct mylite_select_column *column)
{
    if (column == NULL) {
        return;
    }

    free(column->name);
    *column = (struct mylite_select_column){0};
}

void mylite_select_output_column_deinit(struct mylite_select_output_column *column)
{
    if (column == NULL) {
        return;
    }

    free(column->label);
    *column = (struct mylite_select_output_column){0};
}

void mylite_select_aggregate_binding_deinit(struct mylite_select_aggregate_binding *binding)
{
    if (binding == NULL) {
        return;
    }

    free(binding->argument_descriptors);
    *binding = (struct mylite_select_aggregate_binding){0};
}

int mylite_select_resolve_table_target(mylite_db *database, struct mylite_select_table *table)
{
    struct mylite_schema_presence presence;
    bool exists = false;
    int status = MYLITE_OK;

    if (table->schema_name == NULL) {
        if (database->selected_schema == NULL || database->selected_schema[0] == '\0') {
            (void)mylite_diagnostics_set_error_message(database, "No database selected");
            return MYLITE_EXEC_ERROR;
        }
        table->schema_name = mylite_copy_nonempty_cstring(database->selected_schema);
        if (table->schema_name == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    if (mylite_select_schema_name_is_system(table->schema_name)) {
        return MYLITE_UNSUPPORTED;
    }

    status = mylite_catalog_schema_exists(database, table->schema_name, &presence);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(database, "Unknown database '",
                                                         table->schema_name, "'");
        return MYLITE_EXEC_ERROR;
    }
    if (presence.is_system) {
        return MYLITE_UNSUPPORTED;
    }

    status = mylite_catalog_table_exists(database, table->schema_name, table->table_name, &exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        return mylite_diagnostics_set_table_doesnt_exist_error(database, table->schema_name,
                                                               table->table_name);
    }

    table->physical_name =
        mylite_catalog_physical_table_name(table->schema_name, table->table_name);
    if (table->physical_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

bool mylite_select_schema_name_is_system(const char *schema_name)
{
    if (mylite_ascii_case_equal(schema_name, "information_schema")) {
        return true;
    }
    if (mylite_ascii_case_equal(schema_name, "mysql")) {
        return true;
    }
    if (mylite_ascii_case_equal(schema_name, "performance_schema")) {
        return true;
    }
    if (mylite_ascii_case_equal(schema_name, "sys")) {
        return true;
    }
    return false;
}

int mylite_select_resolve_column_reference(const struct mylite_select_table *table,
                                           const struct mylite_sql_ast_node *expression,
                                           size_t *out_index)
{
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

bool mylite_select_reference_qualifiers_match(const struct mylite_select_table *table, char **parts,
                                              size_t part_count)
{
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

size_t mylite_select_column_index(const struct mylite_select_table *table, const char *column_name)
{
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (mylite_ascii_case_equal(table->columns[index].name, column_name)) {
            return index;
        }
    }
    return table->column_count;
}

char *mylite_select_copy_reference_name(const struct mylite_sql_ast_node *identifier)
{
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

int mylite_select_compare_values(const struct mylite_expression_value *left,
                                 const struct mylite_expression_value *right)
{
    bool left_null = left->kind == MYLITE_EXPRESSION_VALUE_NULL;
    bool right_null = right->kind == MYLITE_EXPRESSION_VALUE_NULL;

    if (left_null || right_null) {
        if (left_null == right_null) {
            return 0;
        }
        if (left_null) {
            return -1;
        }
        return 1;
    }
    if (left->kind == MYLITE_EXPRESSION_VALUE_TEXT && right->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return compare_select_text_values(left->text_value, expression_value_text_length(left),
                                          right->text_value, expression_value_text_length(right));
    }
    if (left->kind == MYLITE_EXPRESSION_VALUE_REAL || right->kind == MYLITE_EXPRESSION_VALUE_REAL) {
        double left_value = left->kind == MYLITE_EXPRESSION_VALUE_REAL
                                ? left->real_value
                                : (double)mylite_expression_value_to_int64(left);
        double right_value = right->kind == MYLITE_EXPRESSION_VALUE_REAL
                                 ? right->real_value
                                 : (double)mylite_expression_value_to_int64(right);

        return (left_value > right_value) - (left_value < right_value);
    }
    if (left->kind == MYLITE_EXPRESSION_VALUE_UINT64 ||
        right->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        uint64_t left_value = left->kind == MYLITE_EXPRESSION_VALUE_UINT64
                                  ? left->uint64_value
                                  : (uint64_t)mylite_expression_value_to_int64(left);
        uint64_t right_value = right->kind == MYLITE_EXPRESSION_VALUE_UINT64
                                   ? right->uint64_value
                                   : (uint64_t)mylite_expression_value_to_int64(right);

        return (left_value > right_value) - (left_value < right_value);
    }
    if (left->kind == MYLITE_EXPRESSION_VALUE_TEXT || right->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        char *left_text = mylite_expression_value_to_text(left);
        char *right_text = mylite_expression_value_to_text(right);
        int comparison = compare_select_text_values(left_text, nullable_text_length(left_text),
                                                    right_text, nullable_text_length(right_text));

        free(left_text);
        free(right_text);
        return comparison;
    }
    return (left->int64_value > right->int64_value) - (left->int64_value < right->int64_value);
}

int mylite_select_compare_binary_text_values(const char *left, size_t left_length,
                                             const char *right, size_t right_length)
{
    size_t compare_length = 0U;
    int comparison = 0;

    if (left == NULL) {
        left = "";
        left_length = 0U;
    }
    if (right == NULL) {
        right = "";
        right_length = 0U;
    }
    compare_length = left_length < right_length ? left_length : right_length;
    comparison = compare_length == 0U ? 0 : memcmp(left, right, compare_length);
    if (comparison == 0) {
        return (left_length > right_length) - (left_length < right_length);
    }
    return (comparison > 0) - (comparison < 0);
}

static int compare_select_text_values(const char *left, size_t left_length, const char *right,
                                      size_t right_length)
{
    size_t index = 0U;
    size_t compare_length = 0U;

    if (left == NULL) {
        left = "";
        left_length = 0U;
    }
    if (right == NULL) {
        right = "";
        right_length = 0U;
    }
    compare_length = left_length < right_length ? left_length : right_length;
    while (index < compare_length) {
        unsigned char left_byte = (unsigned char)left[index];
        unsigned char right_byte = (unsigned char)right[index];

        if (left_byte >= 'A' && left_byte <= 'Z') {
            left_byte = (unsigned char)(left_byte - 'A' + 'a');
        }
        if (right_byte >= 'A' && right_byte <= 'Z') {
            right_byte = (unsigned char)(right_byte - 'A' + 'a');
        }
        if (left_byte != right_byte) {
            return (left_byte > right_byte) - (left_byte < right_byte);
        }
        ++index;
    }
    return (left_length > right_length) - (left_length < right_length);
}

static size_t expression_value_text_length(const struct mylite_expression_value *value)
{
    if (value == NULL || value->text_value == NULL) {
        return 0U;
    }
    return value->text_length;
}

static size_t nullable_text_length(const char *text)
{
    return text == NULL ? 0U : strlen(text);
}
