#include "mylite_select.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int compare_select_text_values(const char *left, size_t left_length, const char *right,
                                      size_t right_length);
static bool
select_using_column_range_is_in_range(const struct mylite_select_join_using_column *column,
                                      struct mylite_select_table_range range);
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

void mylite_select_column_sequence_deinit(struct mylite_select_column_sequence *sequence)
{
    if (sequence == NULL) {
        return;
    }
    free(sequence->column_indexes);
    *sequence = (struct mylite_select_column_sequence){0};
}

int mylite_select_plan_add_output_column(struct mylite_select_plan *plan,
                                         const struct mylite_select_output_column *output)
{
    struct mylite_select_output_column *outputs =
        realloc(plan->outputs, (plan->output_count + 1U) * sizeof(*plan->outputs));

    if (outputs == NULL) {
        return MYLITE_NOMEM;
    }

    plan->outputs = outputs;
    plan->outputs[plan->output_count++] = *output;
    return MYLITE_OK;
}

int mylite_select_plan_add_order_key(struct mylite_select_plan *plan,
                                     const struct mylite_select_order_key *order_key)
{
    struct mylite_select_order_key *order_keys =
        realloc(plan->order_keys, (plan->order_key_count + 1U) * sizeof(*plan->order_keys));

    if (order_keys == NULL) {
        return MYLITE_NOMEM;
    }

    plan->order_keys = order_keys;
    plan->order_keys[plan->order_key_count++] = *order_key;
    return MYLITE_OK;
}

int mylite_select_plan_add_group_key(struct mylite_select_plan *plan,
                                     const struct mylite_select_group_key *group_key)
{
    struct mylite_select_group_key *group_keys =
        realloc(plan->group_keys, (plan->group_key_count + 1U) * sizeof(*plan->group_keys));

    if (group_keys == NULL) {
        return MYLITE_NOMEM;
    }

    plan->group_keys = group_keys;
    plan->group_keys[plan->group_key_count++] = *group_key;
    return MYLITE_OK;
}

int mylite_select_plan_add_aggregate_binding(struct mylite_select_plan *plan,
                                             const struct mylite_select_aggregate_binding *binding)
{
    struct mylite_select_aggregate_binding *bindings =
        realloc(plan->aggregate_bindings,
                (plan->aggregate_binding_count + 1U) * sizeof(*plan->aggregate_bindings));

    if (bindings == NULL) {
        return MYLITE_NOMEM;
    }

    plan->aggregate_bindings = bindings;
    plan->aggregate_bindings[plan->aggregate_binding_count++] = *binding;
    return MYLITE_OK;
}

void mylite_select_plan_clear_aggregate_bindings(struct mylite_select_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    for (size_t index = 0U; index < plan->aggregate_binding_count; ++index) {
        mylite_select_aggregate_binding_deinit(&plan->aggregate_bindings[index]);
    }
    free(plan->aggregate_bindings);
    plan->aggregate_bindings = NULL;
    plan->aggregate_binding_count = 0U;
    plan->has_aggregate = false;
}

void mylite_select_plan_mark_output_order_reference(struct mylite_select_plan *plan,
                                                    size_t output_index)
{
    if (plan != NULL && output_index < plan->output_count) {
        plan->outputs[output_index].referenced_by_order = true;
    }
}

size_t mylite_select_output_label_count(const struct mylite_select_plan *plan, const char *label,
                                        size_t *out_index)
{
    size_t count = 0U;

    *out_index = plan->output_count;
    for (size_t index = 0U; index < plan->output_count; ++index) {
        if (plan->outputs[index].label != NULL &&
            mylite_ascii_case_equal(plan->outputs[index].label, label)) {
            if (count == 0U) {
                *out_index = index;
            }
            ++count;
        }
    }
    return count;
}

size_t mylite_select_output_label_span_count(const struct mylite_select_plan *plan,
                                             struct mylite_sql_source_span label, size_t *out_index)
{
    size_t count = 0U;

    *out_index = plan->output_count;
    for (size_t index = 0U; index < plan->output_count; ++index) {
        if (plan->outputs[index].label != NULL &&
            mylite_span_equal_ci(label, plan->outputs[index].label)) {
            if (count == 0U) {
                *out_index = index;
            }
            ++count;
        }
    }
    return count;
}

bool mylite_select_parse_uint64_span(struct mylite_sql_source_span span, uint64_t *out_value)
{
    enum { decimal_radix = 10U };
    uint64_t value = 0U;

    *out_value = 0U;
    if (span.text == NULL || span.length == 0U) {
        return false;
    }
    for (size_t index = 0U; index < span.length; ++index) {
        unsigned char byte = (unsigned char)span.text[index];
        uint64_t digit = 0U;

        if (byte < '0' || byte > '9') {
            return false;
        }
        digit = (uint64_t)(byte - '0');
        if (value > (UINT64_MAX - digit) / decimal_radix) {
            return false;
        }
        value = (value * decimal_radix) + digit;
    }
    *out_value = value;
    return true;
}

const struct mylite_select_column *
mylite_select_plan_column_const(const struct mylite_select_plan *plan, size_t column_index,
                                const struct mylite_select_table **out_table)
{
    size_t table_count = mylite_select_plan_table_count(plan);

    if (out_table != NULL) {
        *out_table = NULL;
    }
    for (size_t table_index = 0U; table_index < table_count; ++table_index) {
        const struct mylite_select_table *table = mylite_select_plan_table_const(plan, table_index);

        if (table != NULL && column_index >= table->first_column_index &&
            column_index < table->first_column_index + table->column_count) {
            if (out_table != NULL) {
                *out_table = table;
            }
            return &table->columns[column_index - table->first_column_index];
        }
    }
    return NULL;
}

size_t mylite_select_count_column_parts_using_matches(const struct mylite_select_plan *plan,
                                                      const char *column_name,
                                                      struct mylite_select_table_range range,
                                                      size_t *match_index)
{
    size_t match_count = 0U;

    if (plan == NULL || column_name == NULL || match_index == NULL) {
        return 0U;
    }

    for (size_t index = 0U; index < plan->using_column_count; ++index) {
        if (!mylite_ascii_case_equal(plan->using_columns[index].name, column_name) ||
            !select_using_column_range_is_in_range(&plan->using_columns[index], range)) {
            continue;
        }
        *match_index = plan->using_columns[index].coalesced_column_index;
        ++match_count;
    }
    return match_count;
}

int mylite_select_resolve_column_in_table(const struct mylite_select_plan *plan,
                                          const struct mylite_select_table *table,
                                          const char *column_name, size_t *out_index)
{
    (void)plan;
    if (table == NULL || column_name == NULL || out_index == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (mylite_ascii_case_equal(table->columns[index].name, column_name)) {
            *out_index = table->first_column_index + index;
            return MYLITE_OK;
        }
    }
    return MYLITE_UNSUPPORTED;
}

int mylite_select_set_ambiguous_column_error(mylite_db *database, const char *column_name,
                                             const char *clause_context)
{
    char *message = sqlite3_mprintf("Column '%q' in %s is ambiguous", column_name,
                                    clause_context == NULL ? "field list" : clause_context);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_NON_UNIQ_ERROR, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

bool mylite_select_column_index_is_using_column_in_range(const struct mylite_select_plan *plan,
                                                         size_t column_index,
                                                         struct mylite_select_table_range range)
{
    if (plan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < plan->using_column_count; ++index) {
        const struct mylite_select_join_using_column *column = &plan->using_columns[index];

        if (select_using_column_range_is_in_range(column, range) &&
            (column->left_column_index == column_index ||
             column->right_column_index == column_index)) {
            return true;
        }
    }
    return false;
}

bool mylite_select_plan_has_column_span(const struct mylite_select_plan *plan,
                                        struct mylite_sql_source_span name)
{
    for (size_t index = 0U; index < mylite_select_plan_column_count(plan); ++index) {
        const struct mylite_select_column *column =
            mylite_select_plan_column_const(plan, index, NULL);

        if (column != NULL && column->name != NULL && mylite_span_equal_ci(name, column->name)) {
            return true;
        }
    }
    return false;
}

bool mylite_select_plan_has_visible_table_span(const struct mylite_select_plan *plan,
                                               struct mylite_sql_source_span name)
{
    for (size_t index = 0U; index < mylite_select_plan_table_count(plan); ++index) {
        const struct mylite_select_table *table = mylite_select_plan_table_const(plan, index);
        const char *visible_name = table == NULL || table->alias == NULL ? NULL : table->alias;

        if (table != NULL && visible_name == NULL) {
            visible_name = table->table_name;
        }
        if (visible_name != NULL && mylite_span_equal_ci(name, visible_name)) {
            return true;
        }
    }
    return false;
}

bool mylite_select_plan_has_outer_join(const struct mylite_select_plan *plan)
{
    for (size_t index = 0U; plan != NULL && index < plan->join_step_count; ++index) {
        enum mylite_sql_ast_join_type join_type = plan->join_steps[index].join_type;

        if (join_type == MYLITE_SQL_AST_JOIN_LEFT || join_type == MYLITE_SQL_AST_JOIN_RIGHT) {
            return true;
        }
    }
    return false;
}

bool mylite_select_duplicate_mode_is_distinct(enum mylite_sql_ast_select_duplicate_mode mode)
{
    return mode == MYLITE_SQL_AST_SELECT_DUPLICATES_DISTINCT;
}

bool mylite_select_plan_requires_custom_runtime(const struct mylite_select_plan *plan,
                                                const struct mylite_select_clause_nodes *clauses)
{
    if (clauses != NULL &&
        (clauses->where != NULL || clauses->group_by != NULL || clauses->having != NULL ||
         clauses->order_by != NULL || clauses->limit != NULL)) {
        return true;
    }
    if (plan == NULL) {
        return false;
    }
    if (mylite_select_duplicate_mode_is_distinct(plan->duplicate_mode)) {
        return true;
    }
    if (mylite_select_plan_table_count(plan) > 1U) {
        return true;
    }
    if (plan->has_aggregate) {
        return true;
    }
    for (size_t index = 0U; index < plan->output_count; ++index) {
        if (plan->outputs[index].kind == MYLITE_SELECT_OUTPUT_EXPRESSION) {
            return true;
        }
    }
    return false;
}

size_t mylite_select_plan_table_count(const struct mylite_select_plan *plan)
{
    if (plan == NULL) {
        return 0U;
    }
    if (plan->table_count != 0U) {
        return plan->table_count;
    }
    return plan->table.table_name == NULL ? 0U : 1U;
}

struct mylite_select_table *mylite_select_plan_table(struct mylite_select_plan *plan,
                                                     size_t table_index)
{
    if (plan == NULL) {
        return NULL;
    }
    if (plan->table_count != 0U) {
        return table_index < plan->table_count ? &plan->tables[table_index] : NULL;
    }
    return table_index == 0U && plan->table.table_name != NULL ? &plan->table : NULL;
}

const struct mylite_select_table *
mylite_select_plan_table_const(const struct mylite_select_plan *plan, size_t table_index)
{
    if (plan == NULL) {
        return NULL;
    }
    if (plan->table_count != 0U) {
        return table_index < plan->table_count ? &plan->tables[table_index] : NULL;
    }
    return table_index == 0U && plan->table.table_name != NULL ? &plan->table : NULL;
}

size_t mylite_select_plan_column_count(const struct mylite_select_plan *plan)
{
    if (plan == NULL) {
        return 0U;
    }
    if (plan->column_count != 0U || plan->table_count != 0U) {
        return plan->column_count;
    }
    return plan->table.column_count;
}

int mylite_select_bind_limit_clause(const struct mylite_sql_ast_node *limit_clause,
                                    struct mylite_select_plan *plan)
{
    const struct mylite_sql_ast_node *offset = mylite_ast_child_at(limit_clause, 0U);
    const struct mylite_sql_ast_node *row_count = mylite_ast_child_at(limit_clause, 1U);

    if (limit_clause == NULL || limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE ||
        offset == NULL || offset->kind != MYLITE_SQL_AST_LIMIT_BOUND ||
        !offset->has_limit_bound_value || row_count == NULL ||
        row_count->kind != MYLITE_SQL_AST_LIMIT_BOUND || !row_count->has_limit_bound_value) {
        return MYLITE_UNSUPPORTED;
    }

    plan->limit = (struct mylite_select_limit){
        .offset = offset->limit_bound_value,
        .row_count = row_count->limit_bound_value,
        .has_limit = true,
    };
    return MYLITE_OK;
}

bool mylite_select_limit_row_is_kept(const struct mylite_select_limit *limit,
                                     struct mylite_select_limit_position position)
{
    if (!limit->has_limit) {
        return true;
    }
    if (position.matched_row < limit->offset) {
        return false;
    }
    if (mylite_select_limit_is_full(limit, position.kept_count)) {
        return false;
    }
    return true;
}

bool mylite_select_limit_is_full(const struct mylite_select_limit *limit, size_t kept_count)
{
    if (!limit->has_limit) {
        return false;
    }
    if (limit->row_count > (uint64_t)SIZE_MAX) {
        return false;
    }
    return kept_count >= (size_t)limit->row_count;
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

char *mylite_select_copy_alias(const struct mylite_sql_ast_node *alias)
{
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

static bool
select_using_column_range_is_in_range(const struct mylite_select_join_using_column *column,
                                      struct mylite_select_table_range range)
{
    size_t range_end = range.first_table + range.table_count;
    size_t column_end = column->first_table + column->table_count;

    if (column->first_table < range.first_table) {
        return false;
    }
    if (column_end > range_end) {
        return false;
    }
    return true;
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
