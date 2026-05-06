#include "mylite_user_variables.h"

#include "mylite_diagnostics.h"
#include "mylite_expression_descriptor.h"
#include "mylite_metadata_constants.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "mylite_statement_ast.h"
#include "mylite_statement_types.h"

#include <stdlib.h>
#include <string.h>

struct set_user_variable_eval_context {
    mylite_db *database;
};

static int copy_user_variable_identifier_tail(const struct mylite_sql_ast_node *identifier,
                                              char **out_name);
static void lowercase_ascii_text(char *text);
static struct mylite_user_variable *
find_user_variable_entry(struct mylite_user_variable_store *store, const char *name);
static const struct mylite_user_variable *
find_user_variable_entry_const(const struct mylite_user_variable_store *store, const char *name);
static struct mylite_field_descriptor unset_user_variable_descriptor(void);
static struct mylite_field_descriptor
user_variable_descriptor_from_value(mylite_db *database,
                                    const struct mylite_expression_value *value);
static struct mylite_field_descriptor assigned_null_user_variable_descriptor(void);
static struct mylite_field_descriptor assigned_text_user_variable_descriptor(mylite_db *database);
static int copy_set_user_variable_assignments(const struct mylite_sql_ast_node *statement,
                                              struct mylite_set_user_variable_plan *plan);
static int
copy_set_user_variable_assignment(const struct mylite_sql_ast_node *assignment,
                                  struct mylite_user_variable_assignment_plan *assignment_plan);
static void
user_variable_assignment_plan_deinit(struct mylite_user_variable_assignment_plan *assignment);
static int eval_set_user_variable_identifier(void *user_data,
                                             const struct mylite_sql_ast_node *identifier,
                                             struct mylite_expression_value *out_value);
static int set_user_variable_store_value(mylite_db *database, const char *name,
                                         const struct mylite_expression_value *value,
                                         const struct mylite_field_descriptor *descriptor);
static void user_variable_entry_deinit(struct mylite_user_variable *entry);
static int map_set_user_variable_expression_status(mylite_db *database, int status);

bool mylite_user_variable_identifier_is_user_variable(const struct mylite_sql_ast_node *identifier)
{
    if (identifier == NULL || identifier->kind != MYLITE_SQL_AST_IDENTIFIER ||
        identifier->span.text == NULL || identifier->span.length < 2U) {
        return false;
    }
    if (identifier->span.text[0] != '@') {
        return false;
    }
    return identifier->span.text[1] != '@';
}

int mylite_user_variable_copy_identifier_name(const struct mylite_sql_ast_node *identifier,
                                              char **out_name)
{
    char *name = NULL;

    if (out_name == NULL) {
        return MYLITE_MISUSE;
    }
    *out_name = NULL;
    if (!mylite_user_variable_identifier_is_user_variable(identifier)) {
        return MYLITE_UNSUPPORTED;
    }

    if (copy_user_variable_identifier_tail(identifier, &name) != MYLITE_OK) {
        return MYLITE_NOMEM;
    }
    lowercase_ascii_text(name);
    *out_name = name;
    return MYLITE_OK;
}

int mylite_user_variable_eval_identifier(mylite_db *database,
                                         const struct mylite_sql_ast_node *identifier,
                                         struct mylite_expression_value *out_value)
{
    char *name = NULL;
    const struct mylite_user_variable *entry = NULL;
    int status = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct mylite_expression_value){0};
    status = mylite_user_variable_copy_identifier_name(identifier, &name);
    if (status != MYLITE_OK) {
        return status == MYLITE_NOMEM ? MYLITE_NOMEM : -1;
    }

    entry =
        find_user_variable_entry_const(database == NULL ? NULL : &database->user_variables, name);
    free(name);
    if (entry == NULL) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }
    if (mylite_expression_value_copy(&entry->value, out_value) != 0) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return 0;
}

int mylite_user_variable_infer_identifier(mylite_db *database,
                                          const struct mylite_sql_ast_node *identifier,
                                          struct mylite_field_descriptor *out_descriptor)
{
    char *name = NULL;
    const struct mylite_user_variable *entry = NULL;
    int status = MYLITE_OK;

    if (out_descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    *out_descriptor = mylite_expression_descriptor_defaults();
    status = mylite_user_variable_copy_identifier_name(identifier, &name);
    if (status != MYLITE_OK) {
        return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_UNSUPPORTED;
    }

    entry =
        find_user_variable_entry_const(database == NULL ? NULL : &database->user_variables, name);
    free(name);
    *out_descriptor = entry == NULL ? unset_user_variable_descriptor() : entry->descriptor;
    return MYLITE_OK;
}

int mylite_user_variable_prepare_set_statement(mylite_db *database,
                                               const struct mylite_sql_ast_node *statement,
                                               mylite_stmt **out_stmt)
{
    mylite_stmt *stmt = calloc(1U, sizeof(*stmt));
    int status = MYLITE_OK;

    if (stmt == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    *stmt = (mylite_stmt){
        .database = database,
        .kind = MYLITE_STMT_SET_USER_VARIABLE,
        .affected_rows = 0,
    };

    status = copy_set_user_variable_assignments(statement, &stmt->set_user_variable);
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        mylite_finalize(stmt);
        return status;
    }

    *out_stmt = stmt;
    return MYLITE_OK;
}

int mylite_user_variable_execute_set_statement(mylite_stmt *stmt)
{
    const struct mylite_set_user_variable_plan *plan =
        stmt == NULL ? NULL : &stmt->set_user_variable;
    struct mylite_expression_value *values = NULL;
    struct mylite_field_descriptor *descriptors = NULL;
    struct set_user_variable_eval_context user_context = {
        .database = stmt == NULL ? NULL : stmt->database,
    };
    struct mylite_expression_eval_context expression_context = {
        .user_data = &user_context,
        .resolve_identifier = eval_set_user_variable_identifier,
    };
    int status = MYLITE_OK;

    if (stmt == NULL || plan == NULL) {
        return MYLITE_MISUSE;
    }
    if (plan->assignment_count == 0U) {
        return MYLITE_OK;
    }

    values = calloc(plan->assignment_count, sizeof(*values));
    descriptors = calloc(plan->assignment_count, sizeof(*descriptors));
    if (values == NULL || descriptors == NULL) {
        free(values);
        free(descriptors);
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    for (size_t index = 0U; index < plan->assignment_count; ++index) {
        status = mylite_expression_eval_with_context(plan->assignments[index].expression,
                                                     &expression_context, &stmt->database->warnings,
                                                     &values[index]);
        if (status != 0) {
            status = map_set_user_variable_expression_status(stmt->database, status);
            goto done;
        }
        descriptors[index] = user_variable_descriptor_from_value(stmt->database, &values[index]);
    }

    for (size_t index = 0U; index < plan->assignment_count; ++index) {
        status = set_user_variable_store_value(stmt->database, plan->assignments[index].name,
                                               &values[index], &descriptors[index]);
        if (status != MYLITE_OK) {
            goto done;
        }
    }

done:
    for (size_t index = 0U; index < plan->assignment_count; ++index) {
        mylite_expression_value_deinit(&values[index]);
    }
    free(values);
    free(descriptors);
    return status;
}

void mylite_user_variable_store_deinit(struct mylite_user_variable_store *store)
{
    if (store == NULL) {
        return;
    }
    for (size_t index = 0U; index < store->count; ++index) {
        user_variable_entry_deinit(&store->items[index]);
    }
    free(store->items);
    *store = (struct mylite_user_variable_store){0};
}

void mylite_user_variable_set_plan_deinit(struct mylite_set_user_variable_plan *plan)
{
    if (plan == NULL) {
        return;
    }
    for (size_t index = 0U; index < plan->assignment_count; ++index) {
        user_variable_assignment_plan_deinit(&plan->assignments[index]);
    }
    free(plan->assignments);
    *plan = (struct mylite_set_user_variable_plan){0};
}

static int copy_user_variable_identifier_tail(const struct mylite_sql_ast_node *identifier,
                                              char **out_name)
{
    struct mylite_sql_ast_node tail_node = {0};
    const char *tail = identifier->span.text + 1U;
    size_t tail_length = identifier->span.length - 1U;

    tail_node = (struct mylite_sql_ast_node){
        .kind = MYLITE_SQL_AST_IDENTIFIER,
        .span =
            (struct mylite_sql_source_span){
                .text = tail,
                .length = tail_length,
            },
    };

    if (tail_length >= 2U && tail[0] == '`' && tail[tail_length - 1U] == '`') {
        *out_name = mylite_copy_identifier_span(&tail_node);
    } else if (tail_length >= 2U && (tail[0] == '\'' || tail[0] == '"') &&
               tail[tail_length - 1U] == tail[0]) {
        *out_name = mylite_copy_string_literal_span(&tail_node);
    } else {
        *out_name = mylite_copy_span_text(tail, tail_length);
    }
    return *out_name == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static void lowercase_ascii_text(char *text)
{
    for (size_t index = 0U; text != NULL && text[index] != '\0'; ++index) {
        if (text[index] >= 'A' && text[index] <= 'Z') {
            text[index] = (char)(text[index] - 'A' + 'a');
        }
    }
}

static struct mylite_user_variable *
find_user_variable_entry(struct mylite_user_variable_store *store, const char *name)
{
    if (store == NULL || name == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < store->count; ++index) {
        if (strcmp(store->items[index].name, name) == 0) {
            return &store->items[index];
        }
    }
    return NULL;
}

static const struct mylite_user_variable *
find_user_variable_entry_const(const struct mylite_user_variable_store *store, const char *name)
{
    if (store == NULL || name == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < store->count; ++index) {
        if (strcmp(store->items[index].name, name) == 0) {
            return &store->items[index];
        }
    }
    return NULL;
}

static struct mylite_field_descriptor unset_user_variable_descriptor(void)
{
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = mylite_mysql_text_length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };
}

static struct mylite_field_descriptor
user_variable_descriptor_from_value(mylite_db *database,
                                    const struct mylite_expression_value *value)
{
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_from_value(value);

    if (value == NULL || value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return assigned_null_user_variable_descriptor();
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return assigned_text_user_variable_descriptor(database);
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        descriptor.length = mylite_mysql_signed_longlong_display_length;
    } else if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        descriptor.length = mylite_mysql_unsigned_longlong_display_length;
    }
    return descriptor;
}

static struct mylite_field_descriptor assigned_null_user_variable_descriptor(void)
{
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_MEDIUM_BLOB,
        .flags = MYLITE_FIELD_FLAG_BLOB | MYLITE_FIELD_FLAG_BINARY,
        .length = mylite_mysql_medium_text_length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };
}

static struct mylite_field_descriptor assigned_text_user_variable_descriptor(mylite_db *database)
{
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_MEDIUM_BLOB,
        .flags = MYLITE_FIELD_FLAG_BLOB | MYLITE_FIELD_FLAG_NOT_NULL,
        .length = mylite_mysql_medium_text_length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_collation_id(database),
        .nullable = false,
    };
}

static int copy_set_user_variable_assignments(const struct mylite_sql_ast_node *statement,
                                              struct mylite_set_user_variable_plan *plan)
{
    const struct mylite_sql_ast_node *assignment_list = mylite_ast_child_at(statement, 0U);

    if (assignment_list == NULL ||
        assignment_list->kind != MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT_LIST) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *assignment = assignment_list->first_child;
         assignment != NULL; assignment = assignment->next_sibling) {
        struct mylite_user_variable_assignment_plan *assignments =
            realloc(plan->assignments, (plan->assignment_count + 1U) * sizeof(*plan->assignments));
        int status = MYLITE_OK;

        if (assignments == NULL) {
            return MYLITE_NOMEM;
        }
        plan->assignments = assignments;
        plan->assignments[plan->assignment_count] =
            (struct mylite_user_variable_assignment_plan){0};
        status = copy_set_user_variable_assignment(assignment,
                                                   &plan->assignments[plan->assignment_count]);
        if (status != MYLITE_OK) {
            return status;
        }
        ++plan->assignment_count;
    }
    return MYLITE_OK;
}

static int
copy_set_user_variable_assignment(const struct mylite_sql_ast_node *assignment,
                                  struct mylite_user_variable_assignment_plan *assignment_plan)
{
    const struct mylite_sql_ast_node *variable = mylite_ast_child_at(assignment, 0U);
    const struct mylite_sql_ast_node *expression = mylite_ast_child_at(assignment, 1U);
    struct mylite_sql_ast_node *expression_clone = NULL;
    int status = MYLITE_OK;

    status = mylite_user_variable_copy_identifier_name(variable, &assignment_plan->name);
    if (status != MYLITE_OK) {
        return status;
    }
    if (expression == NULL || expression->span.text == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    assignment_plan->sql_text =
        mylite_copy_span_text(expression->span.text, expression->span.length);
    if (assignment_plan->sql_text == NULL) {
        return MYLITE_NOMEM;
    }

    status = mylite_statement_ast_clone_subtree(&assignment_plan->expression_ast, expression,
                                                expression->span.text, assignment_plan->sql_text,
                                                expression->span.length, &expression_clone);
    if (status != MYLITE_OK) {
        return status;
    }
    assignment_plan->expression = expression_clone;
    return MYLITE_OK;
}

static void
user_variable_assignment_plan_deinit(struct mylite_user_variable_assignment_plan *assignment)
{
    if (assignment == NULL) {
        return;
    }
    free(assignment->name);
    free(assignment->sql_text);
    mylite_sql_ast_deinit(&assignment->expression_ast);
    *assignment = (struct mylite_user_variable_assignment_plan){0};
}

static int eval_set_user_variable_identifier(void *user_data,
                                             const struct mylite_sql_ast_node *identifier,
                                             struct mylite_expression_value *out_value)
{
    struct set_user_variable_eval_context *context = user_data;

    if (!mylite_user_variable_identifier_is_user_variable(identifier)) {
        return -1;
    }
    return mylite_user_variable_eval_identifier(context == NULL ? NULL : context->database,
                                                identifier, out_value);
}

static int set_user_variable_store_value(mylite_db *database, const char *name,
                                         const struct mylite_expression_value *value,
                                         const struct mylite_field_descriptor *descriptor)
{
    struct mylite_user_variable *entry = NULL;

    if (database == NULL || name == NULL || value == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    entry = find_user_variable_entry(&database->user_variables, name);
    if (entry == NULL) {
        struct mylite_user_variable *items =
            realloc(database->user_variables.items, (database->user_variables.count + 1U) *
                                                        sizeof(*database->user_variables.items));

        if (items == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        database->user_variables.items = items;
        entry = &database->user_variables.items[database->user_variables.count++];
        *entry = (struct mylite_user_variable){0};
        entry->name = mylite_copy_span_text(name, strlen(name));
        if (entry->name == NULL) {
            --database->user_variables.count;
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
    } else {
        mylite_expression_value_deinit(&entry->value);
    }

    if (mylite_expression_value_copy(value, &entry->value) != 0) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    entry->descriptor = *descriptor;
    return MYLITE_OK;
}

static void user_variable_entry_deinit(struct mylite_user_variable *entry)
{
    if (entry == NULL) {
        return;
    }
    free(entry->name);
    mylite_expression_value_deinit(&entry->value);
    *entry = (struct mylite_user_variable){0};
}

static int map_set_user_variable_expression_status(mylite_db *database, int status)
{
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (database != NULL && database->error_message != NULL) {
        return status > 0 ? status : MYLITE_EXEC_ERROR;
    }
    (void)mylite_diagnostics_set_error_message(database,
                                               "unsupported SET user variable expression");
    return MYLITE_UNSUPPORTED;
}
