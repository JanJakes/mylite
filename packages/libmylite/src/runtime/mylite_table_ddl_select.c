#include "mylite_table_ddl.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_dml_insert_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_metadata.h"
#include "mylite_runtime.h"
#include "mylite_select_context.h"
#include "mylite_select_prepare.h"
#include "mylite_span.h"
#include "mylite_statement_prepare.h"
#include "mylite_table_ddl_catalog.h"
#include "mylite_table_ddl_create_sql.h"
#include "mylite_table_ddl_create_validate.h"
#include "mylite_transactions.h"
#include "sqlite3.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    create_select_default_decimal_precision = 10,
    create_select_literal_integer_int_max_digits = 10,
};

struct mylite_create_select_origin_type {
    const char *data_type;
    const char *column_type;
    sqlite3_stmt *select;
};

struct mylite_create_select_type_mapping {
    const char *data_type;
    enum mylite_sql_ast_column_type ast_type;
};

static int prepare_create_select_source(
    mylite_db *database,
    struct mylite_create_table_plan *plan,
    mylite_stmt **out_stmt
);

static int infer_create_select_columns(
    mylite_db *database,
    const mylite_stmt *select_stmt,
    size_t predefined_column_count,
    struct mylite_create_table_plan *plan
);

static int append_create_select_column(
    mylite_db *database,
    const mylite_stmt *select_stmt,
    int column_index,
    struct mylite_create_table_plan *plan
);

static int copy_create_select_column(
    mylite_db *database,
    const mylite_stmt *select_stmt,
    int column_index,
    const struct mylite_create_table_plan *plan,
    struct mylite_create_table_column *column
);

static int merge_predefined_create_select_columns(
    mylite_db *database,
    const mylite_stmt *select_stmt,
    int select_column_count,
    size_t predefined_column_count,
    struct mylite_create_table_plan *plan
);

static int infer_unmatched_create_select_columns(
    mylite_db *database,
    const mylite_stmt *select_stmt,
    int select_column_count,
    size_t predefined_column_count,
    bool *matched_predefined_columns,
    int *selected_predefined_columns,
    struct mylite_create_table_column *selected_columns,
    struct mylite_create_table_plan *plan
);

static int find_predefined_create_select_column(
    const struct mylite_create_table_plan *plan,
    size_t predefined_column_count,
    const bool *matched_predefined_columns,
    const char *column_name
);

static int replace_create_select_columns(
    struct mylite_create_table_plan *plan,
    size_t predefined_column_count,
    const bool *matched_predefined_columns,
    const int *selected_predefined_columns,
    struct mylite_create_table_column *selected_columns,
    int select_column_count
);

static void deinit_create_select_column_slice(
    struct mylite_create_table_column *columns,
    size_t column_count
);

static int load_origin_create_select_column(
    mylite_db *database,
    const mylite_stmt *select_stmt,
    int column_index,
    struct mylite_create_table_column *column,
    bool *out_loaded
);

static int load_origin_create_select_column_from_catalog(
    mylite_db *database,
    const char *catalog_name,
    const mylite_stmt *select_stmt,
    int column_index,
    struct mylite_create_table_column *column,
    bool *out_loaded
);

static int copy_origin_create_select_column_row(
    mylite_db *database,
    sqlite3_stmt *select,
    struct mylite_create_table_column *column
);

static int infer_expression_create_select_column(
    mylite_db *database,
    const mylite_stmt *select_stmt,
    int column_index,
    const struct mylite_create_table_plan *plan,
    struct mylite_create_table_column *column
);

static const struct mylite_sql_ast_node *create_select_output_expression(
    const struct mylite_create_table_plan *plan,
    int column_index
);

static int assign_create_select_type_from_data_type(
    const struct mylite_create_select_origin_type *origin,
    struct mylite_create_table_column *column
);

static bool create_select_lookup_ast_type(
    const char *data_type,
    enum mylite_sql_ast_column_type *out_ast_type
);

static void set_create_select_character_length_attribute(
    const struct mylite_create_select_origin_type *origin,
    struct mylite_create_table_column *column
);

static void set_create_select_decimal_attributes(
    const struct mylite_create_select_origin_type *origin,
    struct mylite_create_table_column *column
);

static void set_create_select_bit_precision_attribute(
    const struct mylite_create_select_origin_type *origin,
    struct mylite_create_table_column *column
);

static void set_create_select_datetime_precision_attribute(
    const struct mylite_create_select_origin_type *origin,
    struct mylite_create_table_column *column
);

static bool create_select_ast_type_uses_character_length(enum mylite_sql_ast_column_type ast_type);

static bool create_select_ast_type_uses_datetime_precision(
    enum mylite_sql_ast_column_type ast_type
);

static int assign_create_select_type_from_descriptor(
    const struct mylite_field_descriptor *descriptor,
    struct mylite_create_table_column *column
);

static int refine_create_select_literal_type(
    const struct mylite_sql_ast_node *expression,
    struct mylite_create_table_column *column
);

static int set_create_select_character_set(
    struct mylite_create_table_column *column,
    const char *character_set
);

static int set_create_select_collation(
    struct mylite_create_table_column *column,
    const char *collation
);

static int set_create_select_implicit_default(struct mylite_create_table_column *column);

static bool create_select_column_uses_empty_string_default(
    const struct mylite_create_table_column *column
);

static char *create_select_decimal_zero_default(const struct mylite_create_table_column *column);

static int validate_create_select_column_names(
    mylite_db *database,
    const struct mylite_create_table_plan *plan
);

static int create_select_transaction(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_schema_default *schema_default,
    struct mylite_create_table_plan *plan,
    mylite_stmt *select_stmt
);

static int insert_create_select_rows(
    mylite_db *database,
    const char *schema_name,
    struct mylite_create_table_plan *plan,
    mylite_stmt *select_stmt
);

static char *build_create_select_insert_sql(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_create_table_plan *plan
);

static int bind_create_select_row(
    sqlite3_stmt *insert,
    const struct mylite_create_table_plan *plan,
    const mylite_stmt *select_stmt
);

static int bind_create_select_column(
    sqlite3_stmt *insert,
    const struct mylite_create_table_plan *plan,
    const mylite_stmt *select_stmt,
    size_t column_index
);

static int append_create_select_insert_source(
    struct mylite_create_table_plan *plan,
    int source_column_index
);

static int validate_create_select_unmatched_defaults(
    mylite_db *database,
    const struct mylite_create_table_plan *plan
);

static int append_create_select_column_to_plan(
    struct mylite_create_table_plan *plan,
    struct mylite_create_table_column column
);

static bool create_select_text_contains(const char *text, const char *needle);

static char *copy_nullable_text(const char *text);

static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_table_ddl_execute_create_table_select_statement(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_create_table_plan *plan,
    bool if_not_exists
) {
    const char *schema_name = plan->schema_name == NULL ? selected_schema : plan->schema_name;
    struct mylite_schema_default schema_default;
    mylite_stmt *select_stmt = NULL;
    size_t predefined_column_count = plan->column_count;
    bool skip_create = false;
    int status = MYLITE_OK;

    plan->selected_row_count = 0;
    status = prepare_create_select_source(database, plan, &select_stmt);
    if (status != MYLITE_OK) {
        return status;
    }
    if (schema_name == NULL) {
        mylite_finalize(select_stmt);
        (void)mylite_diagnostics_set_error_message(database, "No database selected");
        return MYLITE_EXEC_ERROR;
    }

    status = infer_create_select_columns(database, select_stmt, predefined_column_count, plan);
    if (status == MYLITE_OK) {
        status = validate_create_select_column_names(database, plan);
    }
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_validate_create_table_plan(
            database,
            schema_name,
            plan,
            if_not_exists,
            &schema_default,
            &skip_create
        );
    }
    if (status == MYLITE_OK && !skip_create) {
        status = validate_create_select_unmatched_defaults(database, plan);
    }
    if (status == MYLITE_OK && !skip_create) {
        status =
            create_select_transaction(database, schema_name, &schema_default, plan, select_stmt);
    }

    mylite_finalize(select_stmt);
    return status;
}

static int prepare_create_select_source(
    mylite_db *database,
    struct mylite_create_table_plan *plan,
    mylite_stmt **out_stmt
) {
    const struct mylite_statement_prepare_callbacks *callbacks =
        mylite_select_context_statement_prepare_callbacks();
    int status = MYLITE_OK;

    *out_stmt = NULL;
    if (callbacks == NULL || callbacks->select_callbacks == NULL) {
        return MYLITE_MISUSE;
    }
    status = mylite_select_prepare_statement(
        database,
        plan->select_statement,
        plan->select_sql_text,
        plan->select_sql_text == NULL ? 0U : strlen(plan->select_sql_text),
        out_stmt,
        callbacks->select_callbacks
    );
    if (status == MYLITE_UNSUPPORTED && database->error_message == NULL) {
        (void)mylite_diagnostics_set_error_message(
            database,
            "unsupported CREATE TABLE ... SELECT query"
        );
        return MYLITE_EXEC_ERROR;
    }
    return status;
}

static int infer_create_select_columns(
    mylite_db *database,
    const mylite_stmt *select_stmt,
    size_t predefined_column_count,
    struct mylite_create_table_plan *plan
) {
    int column_count = mylite_column_count(select_stmt);

    if (column_count <= 0) {
        (void)mylite_diagnostics_set_error_message(
            database,
            "CREATE TABLE ... SELECT requires at least one column"
        );
        return MYLITE_EXEC_ERROR;
    }
    if (predefined_column_count != 0U) {
        return merge_predefined_create_select_columns(
            database,
            select_stmt,
            column_count,
            predefined_column_count,
            plan
        );
    }
    for (int index = 0; index < column_count; ++index) {
        size_t target_index = plan->column_count;
        int status = append_create_select_column(database, select_stmt, index, plan);

        if (status == MYLITE_OK) {
            status = append_create_select_insert_source(plan, index);
        }
        if (status != MYLITE_OK) {
            return status;
        }
        if (target_index + 1U != plan->column_count) {
            return MYLITE_MISUSE;
        }
    }
    return MYLITE_OK;
}

static int append_create_select_column(
    mylite_db *database,
    const mylite_stmt *select_stmt,
    int column_index,
    struct mylite_create_table_plan *plan
) {
    struct mylite_create_table_column column = {0};
    int status = MYLITE_OK;

    status = copy_create_select_column(database, select_stmt, column_index, plan, &column);
    if (status == MYLITE_OK) {
        status = append_create_select_column_to_plan(plan, column);
    }
    if (status != MYLITE_OK) {
        mylite_table_ddl_create_table_column_deinit(&column);
    }
    return status;
}

static int copy_create_select_column(
    mylite_db *database,
    const mylite_stmt *select_stmt,
    int column_index,
    const struct mylite_create_table_plan *plan,
    struct mylite_create_table_column *column
) {
    const char *column_name = mylite_column_name(select_stmt, column_index);
    bool loaded_origin = false;
    int status = MYLITE_OK;

    *column = (struct mylite_create_table_column){
        .nullable = mylite_column_is_nullable(select_stmt, column_index) != 0,
        .visible = true,
    };
    column->name = copy_nullable_text(column_name == NULL ? "" : column_name);
    if (column->name == NULL) {
        return MYLITE_NOMEM;
    }

    status = load_origin_create_select_column(
        database,
        select_stmt,
        column_index,
        column,
        &loaded_origin
    );
    if (status == MYLITE_OK && !loaded_origin) {
        status = infer_expression_create_select_column(
            database,
            select_stmt,
            column_index,
            plan,
            column
        );
    }
    if (status == MYLITE_OK && !column->has_default && column->nullable) {
        column->has_default = true;
    }
    return status;
}

static int merge_predefined_create_select_columns(
    mylite_db *database,
    const mylite_stmt *select_stmt,
    int select_column_count,
    size_t predefined_column_count,
    struct mylite_create_table_plan *plan
) {
    bool *matched_predefined_columns =
        calloc(predefined_column_count, sizeof(*matched_predefined_columns));
    int *selected_predefined_columns =
        malloc((size_t)select_column_count * sizeof(*selected_predefined_columns));
    struct mylite_create_table_column *selected_columns =
        calloc((size_t)select_column_count, sizeof(*selected_columns));
    int status = MYLITE_OK;

    if (matched_predefined_columns == NULL || selected_predefined_columns == NULL ||
        selected_columns == NULL) {
        free(matched_predefined_columns);
        free(selected_predefined_columns);
        free(selected_columns);
        return MYLITE_NOMEM;
    }
    for (int index = 0; index < select_column_count; ++index) {
        selected_predefined_columns[index] = -1;
    }

    status = infer_unmatched_create_select_columns(
        database,
        select_stmt,
        select_column_count,
        predefined_column_count,
        matched_predefined_columns,
        selected_predefined_columns,
        selected_columns,
        plan
    );
    if (status == MYLITE_OK) {
        status = replace_create_select_columns(
            plan,
            predefined_column_count,
            matched_predefined_columns,
            selected_predefined_columns,
            selected_columns,
            select_column_count
        );
    }
    if (status != MYLITE_OK) {
        deinit_create_select_column_slice(selected_columns, (size_t)select_column_count);
    }
    free(selected_columns);
    free(selected_predefined_columns);
    free(matched_predefined_columns);
    return status;
}

static int infer_unmatched_create_select_columns(
    mylite_db *database,
    const mylite_stmt *select_stmt,
    int select_column_count,
    size_t predefined_column_count,
    bool *matched_predefined_columns,
    int *selected_predefined_columns,
    struct mylite_create_table_column *selected_columns,
    struct mylite_create_table_plan *plan
) {
    for (int index = 0; index < select_column_count; ++index) {
        const char *column_name = mylite_column_name(select_stmt, index);
        int predefined_index = find_predefined_create_select_column(
            plan,
            predefined_column_count,
            matched_predefined_columns,
            column_name == NULL ? "" : column_name
        );

        selected_predefined_columns[index] = predefined_index;
        if (predefined_index >= 0) {
            matched_predefined_columns[predefined_index] = true;
            continue;
        }

        int status =
            copy_create_select_column(database, select_stmt, index, plan, &selected_columns[index]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int find_predefined_create_select_column(
    const struct mylite_create_table_plan *plan,
    size_t predefined_column_count,
    const bool *matched_predefined_columns,
    const char *column_name
) {
    for (size_t index = 0U; index < predefined_column_count; ++index) {
        if (!matched_predefined_columns[index] &&
            mylite_ascii_case_equal(plan->columns[index].name, column_name)) {
            return (int)index;
        }
    }
    return -1;
}

static int replace_create_select_columns(
    struct mylite_create_table_plan *plan,
    size_t predefined_column_count,
    const bool *matched_predefined_columns,
    const int *selected_predefined_columns,
    struct mylite_create_table_column *selected_columns,
    int select_column_count
) {
    size_t capacity = predefined_column_count + (size_t)select_column_count;
    struct mylite_create_table_column *merged_columns = calloc(capacity, sizeof(*merged_columns));
    int *insert_sources = malloc(capacity * sizeof(*insert_sources));
    size_t merged_count = 0U;

    if (merged_columns == NULL || insert_sources == NULL) {
        free(merged_columns);
        free(insert_sources);
        return MYLITE_NOMEM;
    }

    for (size_t index = 0U; index < predefined_column_count; ++index) {
        if (matched_predefined_columns[index]) {
            continue;
        }
        merged_columns[merged_count] = plan->columns[index];
        plan->columns[index] = (struct mylite_create_table_column){0};
        insert_sources[merged_count++] = -1;
    }
    for (int index = 0; index < select_column_count; ++index) {
        int predefined_index = selected_predefined_columns[index];

        if (predefined_index >= 0) {
            merged_columns[merged_count] = plan->columns[predefined_index];
            plan->columns[predefined_index] = (struct mylite_create_table_column){0};
        } else {
            merged_columns[merged_count] = selected_columns[index];
            selected_columns[index] = (struct mylite_create_table_column){0};
        }
        insert_sources[merged_count++] = index;
    }

    free(plan->columns);
    free(plan->select_insert_column_sources);
    plan->columns = merged_columns;
    plan->column_count = merged_count;
    plan->select_insert_column_sources = insert_sources;
    plan->select_insert_column_source_count = merged_count;
    return MYLITE_OK;
}

static void deinit_create_select_column_slice(
    struct mylite_create_table_column *columns,
    size_t column_count
) {
    for (size_t index = 0U; index < column_count; ++index) {
        mylite_table_ddl_create_table_column_deinit(&columns[index]);
    }
}

static int load_origin_create_select_column(
    mylite_db *database,
    const mylite_stmt *select_stmt,
    int column_index,
    struct mylite_create_table_column *column,
    bool *out_loaded
) {
    const char *schema_name = mylite_column_origin_schema_name(select_stmt, column_index);
    const char *table_name = mylite_column_origin_table_name(select_stmt, column_index);
    const char *column_name = mylite_column_origin_name(select_stmt, column_index);
    bool temporary = false;
    int status = MYLITE_OK;

    *out_loaded = false;
    if (schema_name == NULL || table_name == NULL || column_name == NULL) {
        return MYLITE_OK;
    }

    status = mylite_catalog_temporary_table_exists(database, schema_name, table_name, &temporary);
    if (status != MYLITE_OK) {
        return status;
    }
    if (temporary) {
        return load_origin_create_select_column_from_catalog(
            database,
            mylite_catalog_column_catalog_name(true),
            select_stmt,
            column_index,
            column,
            out_loaded
        );
    }
    return load_origin_create_select_column_from_catalog(
        database,
        mylite_catalog_column_catalog_name(false),
        select_stmt,
        column_index,
        column,
        out_loaded
    );
}

static int load_origin_create_select_column_from_catalog(
    mylite_db *database,
    const char *catalog_name,
    const mylite_stmt *select_stmt,
    int column_index,
    struct mylite_create_table_column *column,
    bool *out_loaded
) {
    enum {
        bind_schema_name = 1,
        bind_table_name = 2,
        bind_column_name = 3,
    };

    const char *schema_name = mylite_column_origin_schema_name(select_stmt, column_index);
    const char *table_name = mylite_column_origin_table_name(select_stmt, column_index);
    const char *column_name = mylite_column_origin_name(select_stmt, column_index);
    sqlite3_stmt *select = NULL;
    char *sql = sqlite3_mprintf(
        "SELECT column_default, is_nullable, data_type, character_maximum_length, "
        "numeric_precision, numeric_scale, datetime_precision, character_set_name, "
        "collation_name, column_type, column_comment, has_default FROM %s "
        "WHERE table_schema = ? AND table_name = ? AND column_name = ?",
        catalog_name
    );
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(select, bind_schema_name, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, bind_table_name, table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, bind_column_name, column_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(select);
    if (rc == SQLITE_ROW) {
        status = copy_origin_create_select_column_row(database, select, column);
        *out_loaded = status == MYLITE_OK;
    } else if (rc != SQLITE_DONE) {
        status = mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_finalize(select);
    return status;
}

static int copy_origin_create_select_column_row(
    mylite_db *database,
    sqlite3_stmt *select,
    struct mylite_create_table_column *column
) {
    enum {
        select_column_default = 0,
        select_is_nullable = 1,
        select_data_type = 2,
        select_character_maximum_length = 3,
        select_numeric_precision = 4,
        select_numeric_scale = 5,
        select_datetime_precision = 6,
        select_character_set_name = 7,
        select_collation_name = 8,
        select_column_type = 9,
        select_column_comment = 10,
        select_has_default = 11,
    };

    const char *is_nullable = (const char *)sqlite3_column_text(select, select_is_nullable);
    const char *data_type = (const char *)sqlite3_column_text(select, select_data_type);
    const char *column_type = (const char *)sqlite3_column_text(select, select_column_type);
    const char *character_set =
        (const char *)sqlite3_column_text(select, select_character_set_name);
    const char *collation = (const char *)sqlite3_column_text(select, select_collation_name);
    const char *comment = (const char *)sqlite3_column_text(select, select_column_comment);
    const struct mylite_create_select_origin_type origin = {
        .data_type = data_type,
        .column_type = column_type,
        .select = select,
    };
    int status = assign_create_select_type_from_data_type(&origin, column);

    column->nullable = mylite_ascii_case_equal(is_nullable, "YES");
    column->has_default = sqlite3_column_int(select, select_has_default) != 0;
    if (status == MYLITE_OK && sqlite3_column_type(select, select_column_default) != SQLITE_NULL) {
        column->default_text =
            copy_nullable_text((const char *)sqlite3_column_text(select, select_column_default));
        status = column->default_text == NULL ? MYLITE_NOMEM : MYLITE_OK;
    }
    if (status == MYLITE_OK && column->default_text == NULL && !column->nullable) {
        status = set_create_select_implicit_default(column);
    }
    if (status == MYLITE_OK && character_set != NULL) {
        status = set_create_select_character_set(column, character_set);
    }
    if (status == MYLITE_OK && collation != NULL) {
        status = set_create_select_collation(column, collation);
    }
    if (status == MYLITE_OK && comment != NULL && comment[0] != '\0') {
        column->comment = copy_nullable_text(comment);
        status = column->comment == NULL ? MYLITE_NOMEM : MYLITE_OK;
    }
    if (status != MYLITE_OK && status != MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(
            database,
            "unsupported CREATE TABLE ... SELECT column type"
        );
        return MYLITE_EXEC_ERROR;
    }
    return status;
}

static int infer_expression_create_select_column(
    mylite_db *database,
    const mylite_stmt *select_stmt,
    int column_index,
    const struct mylite_create_table_plan *plan,
    struct mylite_create_table_column *column
) {
    const struct mylite_result_column_metadata *metadata =
        mylite_result_metadata_column(select_stmt, column_index);
    const struct mylite_sql_ast_node *expression =
        create_select_output_expression(plan, column_index);
    int status = MYLITE_OK;

    if (metadata == NULL) {
        (void)mylite_diagnostics_set_error_message(
            database,
            "unsupported CREATE TABLE ... SELECT expression metadata"
        );
        return MYLITE_EXEC_ERROR;
    }
    column->nullable = metadata->descriptor.nullable;
    status = assign_create_select_type_from_descriptor(&metadata->descriptor, column);
    if (status == MYLITE_OK) {
        status = refine_create_select_literal_type(expression, column);
    }
    if (status == MYLITE_OK && !column->nullable) {
        status = set_create_select_implicit_default(column);
    }
    if (status != MYLITE_OK && status != MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(
            database,
            "unsupported CREATE TABLE ... SELECT expression type"
        );
        return MYLITE_EXEC_ERROR;
    }
    return status;
}

static const struct mylite_sql_ast_node *create_select_output_expression(
    const struct mylite_create_table_plan *plan,
    int column_index
) {
    const struct mylite_sql_ast_node *select_list =
        plan == NULL ? NULL : mylite_ast_child_at(plan->select_statement, 0U);
    int output_index = 0;

    for (const struct mylite_sql_ast_node *item = select_list == NULL ? NULL
                                                                      : select_list->first_child;
         item != NULL;
         item = item->next_sibling) {
        if (item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
            return NULL;
        }
        if (output_index == column_index) {
            return mylite_ast_child_at(item, 0U);
        }
        ++output_index;
    }
    return NULL;
}

static int assign_create_select_type_from_data_type(
    const struct mylite_create_select_origin_type *origin,
    struct mylite_create_table_column *column
) {
    struct mylite_column_type_attributes *attributes = &column->type.attributes;

    if (!create_select_lookup_ast_type(origin->data_type, &column->type.ast_type)) {
        return MYLITE_UNSUPPORTED;
    }

    set_create_select_character_length_attribute(origin, column);
    set_create_select_decimal_attributes(origin, column);
    set_create_select_bit_precision_attribute(origin, column);
    set_create_select_datetime_precision_attribute(origin, column);
    if (origin->column_type != NULL &&
        create_select_text_contains(origin->column_type, "unsigned")) {
        attributes->has_unsigned = true;
    }
    return MYLITE_OK;
}

static bool create_select_lookup_ast_type(
    const char *data_type,
    enum mylite_sql_ast_column_type *out_ast_type
) {
    static const struct mylite_create_select_type_mapping mappings[] = {
        {"tinyint", MYLITE_SQL_AST_COLUMN_TYPE_TINYINT},
        {"smallint", MYLITE_SQL_AST_COLUMN_TYPE_SMALLINT},
        {"mediumint", MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMINT},
        {"int", MYLITE_SQL_AST_COLUMN_TYPE_INT},
        {"bigint", MYLITE_SQL_AST_COLUMN_TYPE_BIGINT},
        {"bit", MYLITE_SQL_AST_COLUMN_TYPE_BIT},
        {"geometry", MYLITE_SQL_AST_COLUMN_TYPE_GEOMETRY},
        {"point", MYLITE_SQL_AST_COLUMN_TYPE_POINT},
        {"linestring", MYLITE_SQL_AST_COLUMN_TYPE_LINESTRING},
        {"polygon", MYLITE_SQL_AST_COLUMN_TYPE_POLYGON},
        {"multipoint", MYLITE_SQL_AST_COLUMN_TYPE_MULTIPOINT},
        {"multilinestring", MYLITE_SQL_AST_COLUMN_TYPE_MULTILINESTRING},
        {"multipolygon", MYLITE_SQL_AST_COLUMN_TYPE_MULTIPOLYGON},
        {"geomcollection", MYLITE_SQL_AST_COLUMN_TYPE_GEOMCOLLECTION},
        {"char", MYLITE_SQL_AST_COLUMN_TYPE_CHAR},
        {"varchar", MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR},
        {"binary", MYLITE_SQL_AST_COLUMN_TYPE_BINARY},
        {"varbinary", MYLITE_SQL_AST_COLUMN_TYPE_VARBINARY},
        {"decimal", MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL},
        {"float", MYLITE_SQL_AST_COLUMN_TYPE_FLOAT},
        {"double", MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE},
        {"date", MYLITE_SQL_AST_COLUMN_TYPE_DATE},
        {"time", MYLITE_SQL_AST_COLUMN_TYPE_TIME},
        {"datetime", MYLITE_SQL_AST_COLUMN_TYPE_DATETIME},
        {"timestamp", MYLITE_SQL_AST_COLUMN_TYPE_TIMESTAMP},
        {"year", MYLITE_SQL_AST_COLUMN_TYPE_YEAR},
        {"tinytext", MYLITE_SQL_AST_COLUMN_TYPE_TINYTEXT},
        {"text", MYLITE_SQL_AST_COLUMN_TYPE_TEXT},
        {"mediumtext", MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMTEXT},
        {"longtext", MYLITE_SQL_AST_COLUMN_TYPE_LONGTEXT},
        {"tinyblob", MYLITE_SQL_AST_COLUMN_TYPE_TINYBLOB},
        {"blob", MYLITE_SQL_AST_COLUMN_TYPE_BLOB},
        {"mediumblob", MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMBLOB},
        {"longblob", MYLITE_SQL_AST_COLUMN_TYPE_LONGBLOB},
    };

    if (out_ast_type == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(mappings) / sizeof(mappings[0]); ++index) {
        if (mylite_ascii_case_equal(data_type, mappings[index].data_type)) {
            *out_ast_type = mappings[index].ast_type;
            return true;
        }
    }
    return false;
}

static void set_create_select_character_length_attribute(
    const struct mylite_create_select_origin_type *origin,
    struct mylite_create_table_column *column
) {
    enum { select_character_maximum_length = 3 };
    struct mylite_column_type_attributes *attributes = &column->type.attributes;

    if (origin->select == NULL ||
        !create_select_ast_type_uses_character_length(column->type.ast_type) ||
        sqlite3_column_type(origin->select, select_character_maximum_length) == SQLITE_NULL) {
        return;
    }
    attributes->has_length = true;
    attributes->length =
        (uint64_t)sqlite3_column_int64(origin->select, select_character_maximum_length);
}

static void set_create_select_decimal_attributes(
    const struct mylite_create_select_origin_type *origin,
    struct mylite_create_table_column *column
) {
    enum {
        select_numeric_precision = 4,
        select_numeric_scale = 5,
    };
    struct mylite_column_type_attributes *attributes = &column->type.attributes;

    if (origin->select == NULL || column->type.ast_type != MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL) {
        return;
    }
    attributes->has_precision = true;
    attributes->precision =
        (uint64_t)sqlite3_column_int64(origin->select, select_numeric_precision);
    if (sqlite3_column_type(origin->select, select_numeric_scale) != SQLITE_NULL) {
        attributes->has_scale = true;
        attributes->scale = (uint64_t)sqlite3_column_int64(origin->select, select_numeric_scale);
    }
}

static void set_create_select_bit_precision_attribute(
    const struct mylite_create_select_origin_type *origin,
    struct mylite_create_table_column *column
) {
    enum { select_numeric_precision = 4 };
    struct mylite_column_type_attributes *attributes = &column->type.attributes;

    if (origin->select == NULL || column->type.ast_type != MYLITE_SQL_AST_COLUMN_TYPE_BIT ||
        sqlite3_column_type(origin->select, select_numeric_precision) == SQLITE_NULL) {
        return;
    }
    attributes->has_precision = true;
    attributes->precision =
        (uint64_t)sqlite3_column_int64(origin->select, select_numeric_precision);
}

static void set_create_select_datetime_precision_attribute(
    const struct mylite_create_select_origin_type *origin,
    struct mylite_create_table_column *column
) {
    enum { select_datetime_precision = 6 };
    struct mylite_column_type_attributes *attributes = &column->type.attributes;

    if (origin->select == NULL ||
        !create_select_ast_type_uses_datetime_precision(column->type.ast_type) ||
        sqlite3_column_type(origin->select, select_datetime_precision) == SQLITE_NULL ||
        sqlite3_column_int64(origin->select, select_datetime_precision) <= 0) {
        return;
    }
    attributes->has_precision = true;
    attributes->precision =
        (uint64_t)sqlite3_column_int64(origin->select, select_datetime_precision);
}

static bool create_select_ast_type_uses_character_length(enum mylite_sql_ast_column_type ast_type) {
    switch (ast_type) {
    case MYLITE_SQL_AST_COLUMN_TYPE_CHAR:
    case MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR:
    case MYLITE_SQL_AST_COLUMN_TYPE_BINARY:
    case MYLITE_SQL_AST_COLUMN_TYPE_VARBINARY:
        return true;
    default:
        return false;
    }
}

static bool create_select_ast_type_uses_datetime_precision(
    enum mylite_sql_ast_column_type ast_type
) {
    switch (ast_type) {
    case MYLITE_SQL_AST_COLUMN_TYPE_TIME:
    case MYLITE_SQL_AST_COLUMN_TYPE_DATETIME:
    case MYLITE_SQL_AST_COLUMN_TYPE_TIMESTAMP:
        return true;
    default:
        return false;
    }
}

static int assign_create_select_type_from_descriptor(
    const struct mylite_field_descriptor *descriptor,
    struct mylite_create_table_column *column
) {
    switch (descriptor->type) {
    case MYLITE_FIELD_TYPE_TINY:
        column->type.ast_type = MYLITE_SQL_AST_COLUMN_TYPE_TINYINT;
        break;
    case MYLITE_FIELD_TYPE_SHORT:
        column->type.ast_type = MYLITE_SQL_AST_COLUMN_TYPE_SMALLINT;
        break;
    case MYLITE_FIELD_TYPE_INT24:
        column->type.ast_type = MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMINT;
        break;
    case MYLITE_FIELD_TYPE_LONG:
        column->type.ast_type = MYLITE_SQL_AST_COLUMN_TYPE_INT;
        break;
    case MYLITE_FIELD_TYPE_LONGLONG:
        column->type.ast_type = MYLITE_SQL_AST_COLUMN_TYPE_BIGINT;
        break;
    case MYLITE_FIELD_TYPE_BIT:
        column->type.ast_type = MYLITE_SQL_AST_COLUMN_TYPE_BIT;
        column->type.attributes.has_precision = true;
        column->type.attributes.precision = descriptor->length == 0U ? 1U : descriptor->length;
        break;
    case MYLITE_FIELD_TYPE_GEOMETRY:
        column->type.ast_type = MYLITE_SQL_AST_COLUMN_TYPE_GEOMETRY;
        break;
    case MYLITE_FIELD_TYPE_FLOAT:
        column->type.ast_type = MYLITE_SQL_AST_COLUMN_TYPE_FLOAT;
        break;
    case MYLITE_FIELD_TYPE_DOUBLE:
        column->type.ast_type = MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE;
        break;
    case MYLITE_FIELD_TYPE_DECIMAL:
    case MYLITE_FIELD_TYPE_NEWDECIMAL:
        column->type.ast_type = MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL;
        column->type.attributes.has_precision = true;
        column->type.attributes.precision =
            descriptor->length == 0U ? create_select_default_decimal_precision : descriptor->length;
        column->type.attributes.has_scale = true;
        column->type.attributes.scale = descriptor->decimals;
        break;
    case MYLITE_FIELD_TYPE_DATE:
    case MYLITE_FIELD_TYPE_NEWDATE:
        column->type.ast_type = MYLITE_SQL_AST_COLUMN_TYPE_DATE;
        break;
    case MYLITE_FIELD_TYPE_TIME:
        column->type.ast_type = MYLITE_SQL_AST_COLUMN_TYPE_TIME;
        break;
    case MYLITE_FIELD_TYPE_DATETIME:
        column->type.ast_type = MYLITE_SQL_AST_COLUMN_TYPE_DATETIME;
        break;
    case MYLITE_FIELD_TYPE_TIMESTAMP:
        column->type.ast_type = MYLITE_SQL_AST_COLUMN_TYPE_TIMESTAMP;
        break;
    case MYLITE_FIELD_TYPE_YEAR:
        column->type.ast_type = MYLITE_SQL_AST_COLUMN_TYPE_YEAR;
        break;
    case MYLITE_FIELD_TYPE_STRING:
    case MYLITE_FIELD_TYPE_VAR_STRING:
    case MYLITE_FIELD_TYPE_VARCHAR:
        column->type.ast_type = MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR;
        column->type.attributes.has_length = true;
        column->type.attributes.length = descriptor->length == 0U ? 1U : descriptor->length;
        break;
    case MYLITE_FIELD_TYPE_NULL:
        column->type.ast_type = MYLITE_SQL_AST_COLUMN_TYPE_VARBINARY;
        column->type.attributes.has_length = true;
        column->type.attributes.length = 0U;
        column->nullable = true;
        break;
    default:
        return MYLITE_UNSUPPORTED;
    }
    if ((descriptor->flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U) {
        column->type.attributes.has_unsigned = true;
    }
    return MYLITE_OK;
}

static int refine_create_select_literal_type(
    const struct mylite_sql_ast_node *expression,
    struct mylite_create_table_column *column
) {
    size_t literal_length = 0U;
    char *literal_text = NULL;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_OK;
    }
    if (expression->literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        if (column->type.ast_type == MYLITE_SQL_AST_COLUMN_TYPE_BIGINT &&
            expression->span.length <= create_select_literal_integer_int_max_digits) {
            column->type.ast_type = MYLITE_SQL_AST_COLUMN_TYPE_INT;
        }
        return MYLITE_OK;
    }
    if (expression->literal_kind != MYLITE_SQL_AST_LITERAL_STRING &&
        expression->literal_kind != MYLITE_SQL_AST_LITERAL_NATIONAL_STRING) {
        return MYLITE_OK;
    }

    literal_text = mylite_copy_string_literal_span_with_length(expression, &literal_length);
    if (literal_text == NULL) {
        return MYLITE_NOMEM;
    }
    free(literal_text);
    if (column->type.ast_type == MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR) {
        column->type.attributes.has_length = true;
        column->type.attributes.length = (uint64_t)literal_length;
    }
    return MYLITE_OK;
}

static int set_create_select_character_set(
    struct mylite_create_table_column *column,
    const char *character_set
) {
    column->type.character_set = copy_nullable_text(character_set);
    if (column->type.character_set == NULL) {
        return MYLITE_NOMEM;
    }
    column->type.attributes.has_character_set = true;
    column->type.attributes.character_set = column->type.character_set;
    column->type.attributes.character_set_length = strlen(column->type.character_set);
    return MYLITE_OK;
}

static int set_create_select_collation(
    struct mylite_create_table_column *column,
    const char *collation
) {
    column->type.collation = copy_nullable_text(collation);
    if (column->type.collation == NULL) {
        return MYLITE_NOMEM;
    }
    column->type.attributes.has_collation = true;
    column->type.attributes.collation = column->type.collation;
    column->type.attributes.collation_length = strlen(column->type.collation);
    return MYLITE_OK;
}

static int set_create_select_implicit_default(struct mylite_create_table_column *column) {
    if (column->type.ast_type == MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL) {
        column->default_text = create_select_decimal_zero_default(column);
    } else if (create_select_column_uses_empty_string_default(column)) {
        column->default_text = copy_nullable_text("");
    } else {
        column->default_text = copy_nullable_text("0");
    }
    if (column->default_text == NULL) {
        return MYLITE_NOMEM;
    }
    column->has_default = true;
    return MYLITE_OK;
}

static bool create_select_column_uses_empty_string_default(
    const struct mylite_create_table_column *column
) {
    switch (column->type.ast_type) {
    case MYLITE_SQL_AST_COLUMN_TYPE_CHAR:
    case MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR:
    case MYLITE_SQL_AST_COLUMN_TYPE_BINARY:
    case MYLITE_SQL_AST_COLUMN_TYPE_VARBINARY:
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYTEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_TEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMTEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGTEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYBLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_BLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMBLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGBLOB:
        return true;
    default:
        return false;
    }
}

static char *create_select_decimal_zero_default(const struct mylite_create_table_column *column) {
    uint64_t scale = 0U;
    size_t length = 0U;
    char *text = NULL;

    if (column->type.attributes.has_scale) {
        scale = column->type.attributes.scale;
    }
    length = scale == 0U ? 1U : (size_t)scale + 2U;
    text = malloc(length + 1U);

    if (text == NULL) {
        return NULL;
    }
    text[0] = '0';
    if (scale > 0) {
        text[1] = '.';
        for (uint64_t index = 0; index < scale; ++index) {
            text[(size_t)index + 2U] = '0';
        }
    }
    text[length] = '\0';
    return text;
}

static int validate_create_select_column_names(
    mylite_db *database,
    const struct mylite_create_table_plan *plan
) {
    for (size_t left = 0U; left < plan->column_count; ++left) {
        for (size_t right = left + 1U; right < plan->column_count; ++right) {
            if (mylite_ascii_case_equal(plan->columns[left].name, plan->columns[right].name)) {
                int status = mylite_diagnostics_set_error_message_parts(
                    database,
                    "Duplicate column name '",
                    plan->columns[right].name,
                    "'"
                );

                if (status == MYLITE_OK) {
                    status = mylite_diagnostics_append_error(
                        database,
                        MYLITE_MYSQL_ER_DUP_FIELDNAME,
                        mylite_error_message(database)
                    );
                }
                return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
            }
        }
    }
    return MYLITE_OK;
}

static int create_select_transaction(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_schema_default *schema_default,
    struct mylite_create_table_plan *plan,
    mylite_stmt *select_stmt
) {
    int status = mylite_transaction_begin_storage(database);

    if (status == MYLITE_OK) {
        status =
            mylite_table_ddl_create_physical_table(database, schema_name, schema_default, plan);
    }
    if (status == MYLITE_OK) {
        status = insert_create_select_rows(database, schema_name, plan, select_stmt);
    }
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_insert_create_table_catalog_rows(
            database,
            schema_name,
            schema_default,
            plan
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_storage(database);
        if (status == MYLITE_OK) {
            return MYLITE_OK;
        }
    }

    mylite_transaction_rollback_storage(database);
    return status;
}

static int insert_create_select_rows(
    mylite_db *database,
    const char *schema_name,
    struct mylite_create_table_plan *plan,
    mylite_stmt *select_stmt
) {
    sqlite3_stmt *insert = NULL;
    char *sql = build_create_select_insert_sql(database, schema_name, plan);
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &insert, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    while ((status = mylite_step(select_stmt)) == MYLITE_ROW) {
        sqlite3_reset(insert);
        sqlite3_clear_bindings(insert);
        status = bind_create_select_row(insert, plan, select_stmt);
        if (status != MYLITE_OK) {
            sqlite3_finalize(insert);
            return status;
        }
        rc = sqlite3_step(insert);
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(insert);
            return mylite_diagnostics_set_sqlite_error(database);
        }
        ++plan->selected_row_count;
    }

    sqlite3_finalize(insert);
    return status == MYLITE_DONE ? MYLITE_OK : status;
}

static char *build_create_select_insert_sql(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_create_table_plan *plan
) {
    char *physical_name = mylite_catalog_physical_table_name(schema_name, plan->table_name);
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (physical_name == NULL || sql == NULL) {
        free(physical_name);
        if (sql != NULL) {
            sqlite3_free(sqlite3_str_finish(sql));
        }
        return NULL;
    }

    sqlite3_str_appendf(sql, "INSERT INTO \"%w\"(", physical_name);
    free(physical_name);
    for (size_t index = 0U; index < plan->column_count; ++index) {
        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "\"%w\"", plan->columns[index].name);
    }
    sqlite3_str_append(sql, ") VALUES(", (int)strlen(") VALUES("));
    for (size_t index = 0U; index < plan->column_count; ++index) {
        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_append(sql, "?", 1);
    }
    sqlite3_str_append(sql, ")", 1);
    return sqlite3_str_finish(sql);
}

static int bind_create_select_row(
    sqlite3_stmt *insert,
    const struct mylite_create_table_plan *plan,
    const mylite_stmt *select_stmt
) {
    for (size_t index = 0U; index < plan->column_count; ++index) {
        int status = bind_create_select_column(insert, plan, select_stmt, index);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int bind_create_select_column(
    sqlite3_stmt *insert,
    const struct mylite_create_table_plan *plan,
    const mylite_stmt *select_stmt,
    size_t column_index
) {
    int source_index = column_index < plan->select_insert_column_source_count
                           ? plan->select_insert_column_sources[column_index]
                           : (int)column_index;
    const struct mylite_create_table_column *column = &plan->columns[column_index];
    int bind_index = (int)column_index + 1;

    if (source_index >= 0) {
        const char *text = mylite_column_text(select_stmt, source_index);
        uint64_t bytes = mylite_column_bytes(select_stmt, source_index);

        if (text == NULL) {
            sqlite3_bind_null(insert, bind_index);
        } else {
            sqlite3_bind_text64(
                insert,
                bind_index,
                text,
                bytes,
                sqlite_transient_destructor(),
                SQLITE_UTF8
            );
        }
        return MYLITE_OK;
    }

    if (column->has_default && column->default_text != NULL) {
        sqlite3_bind_text(
            insert,
            bind_index,
            column->default_text,
            -1,
            sqlite_transient_destructor()
        );
    } else {
        sqlite3_bind_null(insert, bind_index);
    }
    return MYLITE_OK;
}

static int append_create_select_insert_source(
    struct mylite_create_table_plan *plan,
    int source_column_index
) {
    int *sources = realloc(
        plan->select_insert_column_sources,
        (plan->select_insert_column_source_count + 1U) * sizeof(*sources)
    );

    if (sources == NULL) {
        return MYLITE_NOMEM;
    }
    plan->select_insert_column_sources = sources;
    plan->select_insert_column_sources[plan->select_insert_column_source_count++] =
        source_column_index;
    return MYLITE_OK;
}

static int validate_create_select_unmatched_defaults(
    mylite_db *database,
    const struct mylite_create_table_plan *plan
) {
    for (size_t index = 0U; index < plan->select_insert_column_source_count; ++index) {
        const struct mylite_create_table_column *column = &plan->columns[index];

        if (plan->select_insert_column_sources[index] < 0 && !column->nullable &&
            !column->has_default && !column->auto_increment) {
            return mylite_dml_insert_set_no_default_error(database, column->name);
        }
    }
    return MYLITE_OK;
}

static int append_create_select_column_to_plan(
    struct mylite_create_table_plan *plan,
    struct mylite_create_table_column column
) {
    struct mylite_create_table_column *columns =
        realloc(plan->columns, (plan->column_count + 1U) * sizeof(*plan->columns));

    if (columns == NULL) {
        return MYLITE_NOMEM;
    }
    plan->columns = columns;
    plan->columns[plan->column_count++] = column;
    return MYLITE_OK;
}

static bool create_select_text_contains(const char *text, const char *needle) {
    if (text == NULL || needle == NULL) {
        return false;
    }
    return strstr(text, needle) != NULL;
}

static char *copy_nullable_text(const char *text) {
    return mylite_copy_span_text(text == NULL ? "" : text, text == NULL ? 0U : strlen(text));
}

static sqlite3_destructor_type sqlite_transient_destructor(void) {
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
