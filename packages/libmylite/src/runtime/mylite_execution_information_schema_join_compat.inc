struct information_schema_join_compat_name_rows {
    char **items;
    size_t count;
};

struct information_schema_join_compat_statistics_request {
    const struct information_schema_row_set *columns;
    const struct information_schema_row_set *statistics;
};

struct information_schema_join_compat_columns_tables_request {
    const struct information_schema_row_set *columns;
    const struct information_schema_row_set *tables;
};

struct information_schema_join_compat_statistics_order {
    size_t left_index;
    size_t right_index;
};

struct information_schema_join_compat_source_column {
    const struct mylite_execution_catalog_table_definition *definition;
    const char *source_alias;
    const char *source_column_name;
    const char *label;
};

static int information_schema_join_compat_execute_columns_statistics_join(
    struct mylite_db *database,
    const struct information_schema_join_compat_plan *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int information_schema_join_compat_execute_columns_statistics_union(
    struct mylite_db *database,
    const struct information_schema_join_compat_plan *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int information_schema_join_compat_execute_columns_tables_join(
    struct mylite_db *database,
    const struct information_schema_join_compat_plan *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int information_schema_join_compat_execute_tables_grouped_size_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int information_schema_join_compat_execute_dynamic_columns_select(
    struct mylite_db *database,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int information_schema_join_compat_execute_schemata_tables_left_join(
    struct mylite_db *database,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int information_schema_join_compat_finish_result(
    struct mylite_db *database,
    bool apply_sql_select_limit,
    mylite_result *result,
    mylite_result **out_result
);
static int information_schema_join_compat_append_definition_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_execution_catalog_table_definition *definition,
    const char *source_alias
);
static int information_schema_join_compat_append_source_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_join_compat_source_column *columns,
    size_t column_count
);
static int information_schema_join_compat_append_source_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_join_compat_source_column *column
);
static int information_schema_join_compat_find_definition_column(
    const struct mylite_execution_catalog_table_definition *definition,
    const char *name,
    size_t *out_index
);
static int information_schema_join_compat_append_concat_name_column(
    struct mylite_db *database,
    mylite_result *result
);
static int information_schema_join_compat_append_grouped_bytes_column(
    struct mylite_db *database,
    mylite_result *result
);
static int information_schema_join_compat_append_dynamic_id_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_catalog_table_descriptor *table
);
static int information_schema_join_compat_append_columns_statistics_join_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_join_compat_statistics_request *request,
    const char *schema_name,
    const char *table_name
);
static int information_schema_join_compat_collect_matching_statistics_rows(
    struct mylite_db *database,
    const struct information_schema_row_set *statistics,
    const char *schema_name,
    const char *table_name,
    size_t **out_indexes,
    size_t *out_index_count
);
static int information_schema_join_compat_sort_statistics_row_indexes(
    const struct information_schema_row_set *statistics,
    size_t *indexes,
    size_t index_count
);
static int information_schema_join_compat_compare_statistics_row_indexes(
    const struct information_schema_row_set *statistics,
    struct information_schema_join_compat_statistics_order order
);
static char **information_schema_join_compat_find_column_row(
    const struct information_schema_row_set *columns,
    const char *schema_name,
    const char *table_name,
    const char *column_name
);
static bool information_schema_join_compat_row_matches_schema_table(
    char *const *row,
    size_t schema_column,
    size_t table_column,
    const char *schema_name,
    const char *table_name
);
static int information_schema_join_compat_append_columns_statistics_union_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_join_compat_statistics_request *request,
    const char *schema_name,
    const char *table_name
);
static int information_schema_join_compat_append_columns_tables_join_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_join_compat_columns_tables_request *request,
    const char *schema_name,
    const char *table_name
);
static bool information_schema_join_compat_has_base_table_row(
    const struct information_schema_row_set *tables,
    const char *schema_name,
    const char *table_name
);
static int information_schema_join_compat_append_columns_tables_join_row(
    struct mylite_db *database,
    mylite_result *result,
    char *const *row
);
static int information_schema_join_compat_append_tables_grouped_size_rows(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result
);
static int information_schema_join_compat_append_tables_grouped_size_row(
    struct mylite_db *database,
    char *const *row,
    mylite_result *result
);
static int information_schema_join_compat_format_table_size_bytes(
    struct mylite_db *database,
    char *const *row,
    char *buffer,
    size_t buffer_size
);
static bool information_schema_join_compat_parse_unsigned_text(
    const char *text,
    uint64_t *out_value
);
static int information_schema_join_compat_append_column_names(
    struct mylite_db *database,
    struct information_schema_join_compat_name_rows *names,
    const struct information_schema_row_set *columns,
    const char *schema_name,
    const char *table_name
);
static int information_schema_join_compat_append_distinct_index_names(
    struct mylite_db *database,
    struct information_schema_join_compat_name_rows *names,
    const struct information_schema_row_set *statistics,
    const char *schema_name,
    const char *table_name
);
static int information_schema_join_compat_append_name(
    struct mylite_db *database,
    struct information_schema_join_compat_name_rows *names,
    const char *name,
    const char *suffix
);
static bool information_schema_join_compat_name_rows_contain(
    const struct information_schema_join_compat_name_rows *names,
    const char *name
);
static int information_schema_join_compat_sort_name_rows(
    struct information_schema_join_compat_name_rows *names
);
static void information_schema_join_compat_name_rows_deinit(
    struct information_schema_join_compat_name_rows *names
);
static int information_schema_join_compat_append_schemata_left_join_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_row_set *schemata,
    const struct information_schema_row_set *tables
);
static size_t information_schema_join_compat_count_table_matches(
    const struct information_schema_row_set *tables,
    const char *schema_name
);
static size_t information_schema_join_compat_next_schema_row(
    const struct information_schema_row_set *schemata,
    const bool *emitted
);
static int information_schema_join_compat_read_dynamic_schema_id(
    struct mylite_db *database,
    struct mylite_catalog_table_descriptor *out_table,
    char **out_id
);
static int information_schema_join_compat_build_dynamic_schema_id_sql(
    const struct mylite_catalog_table_descriptor *table,
    char **out_sql
);
static int information_schema_join_compat_append_dynamic_columns_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_row_set *rows,
    const char *id_text
);
static int execute_information_schema_join_compat_select_if_needed(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result,
    bool *out_handled
) {
    struct information_schema_join_compat_plan plan = {0};
    int rc = information_schema_join_compat_plan_select(database, statement, &plan);

    (void)context;
    if (out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (rc != MYLITE_OK) {
        information_schema_join_compat_plan_deinit(&plan);
        return rc;
    }
    *out_handled = plan.kind != INFORMATION_SCHEMA_JOIN_COMPAT_NONE;
    switch (plan.kind) {
    case INFORMATION_SCHEMA_JOIN_COMPAT_COLUMNS_STATISTICS:
        rc = information_schema_join_compat_execute_columns_statistics_join(
            database,
            &plan,
            apply_sql_select_limit,
            out_result
        );
        break;
    case INFORMATION_SCHEMA_JOIN_COMPAT_COLUMNS_STATISTICS_UNION:
        rc = information_schema_join_compat_execute_columns_statistics_union(
            database,
            &plan,
            apply_sql_select_limit,
            out_result
        );
        break;
    case INFORMATION_SCHEMA_JOIN_COMPAT_COLUMNS_TABLES:
        rc = information_schema_join_compat_execute_columns_tables_join(
            database,
            &plan,
            apply_sql_select_limit,
            out_result
        );
        break;
    case INFORMATION_SCHEMA_JOIN_COMPAT_TABLES_GROUPED_SIZE:
        rc = information_schema_join_compat_execute_tables_grouped_size_select(
            database,
            statement,
            apply_sql_select_limit,
            out_result
        );
        break;
    case INFORMATION_SCHEMA_JOIN_COMPAT_DYNAMIC_COLUMNS:
        rc = information_schema_join_compat_execute_dynamic_columns_select(
            database,
            apply_sql_select_limit,
            out_result
        );
        break;
    case INFORMATION_SCHEMA_JOIN_COMPAT_SCHEMATA_TABLES:
        rc = information_schema_join_compat_execute_schemata_tables_left_join(
            database,
            apply_sql_select_limit,
            out_result
        );
        break;
    case INFORMATION_SCHEMA_JOIN_COMPAT_NONE:
        break;
    }
    information_schema_join_compat_plan_deinit(&plan);
    return rc;
}

static int information_schema_join_compat_select_statement_is_supported(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool *out_matches
) {
    struct information_schema_join_compat_plan plan = {0};
    int rc = MYLITE_OK;

    if (out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    rc = information_schema_join_compat_plan_select(database, statement, &plan);
    *out_matches =
        rc == MYLITE_OK && plan.kind == INFORMATION_SCHEMA_JOIN_COMPAT_COLUMNS_STATISTICS_UNION;
    information_schema_join_compat_plan_deinit(&plan);
    return rc;
}

static int information_schema_join_compat_execute_columns_statistics_join(
    struct mylite_db *database,
    const struct information_schema_join_compat_plan *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    const struct mylite_execution_catalog_table_definition *columns_definition =
        find_information_schema_table_definition("COLUMNS");
    const struct mylite_execution_catalog_table_definition *statistics_definition =
        find_information_schema_table_definition("STATISTICS");
    const struct information_schema_join_compat_source_column result_columns[] = {
        {columns_definition, "cols", "DATA_TYPE", "DATA_TYPE"},
        {statistics_definition, "stats", "INDEX_NAME", "INDEX_NAME"},
        {statistics_definition, "stats", "COLUMN_NAME", "COLUMN_NAME"},
    };
    struct information_schema_row_set columns = {0};
    struct information_schema_row_set statistics = {0};
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (columns_definition == NULL || statistics_definition == NULL) {
        set_runtime_error(database, "missing INFORMATION_SCHEMA join definition");
        return MYLITE_ERROR;
    }
    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = information_schema_join_compat_append_source_columns(
        database,
        result,
        result_columns,
        sizeof(result_columns) / sizeof(result_columns[0])
    );
    if (rc == MYLITE_OK) {
        rc = build_information_schema_rows(database, NULL, columns_definition, &columns);
    }
    if (rc == MYLITE_OK) {
        rc = build_information_schema_rows(database, NULL, statistics_definition, &statistics);
    }
    if (rc == MYLITE_OK && plan->schema_name != NULL && plan->table_name != NULL) {
        rc = information_schema_join_compat_append_columns_statistics_join_rows(
            database,
            result,
            &(const struct information_schema_join_compat_statistics_request){
                .columns = &columns,
                .statistics = &statistics,
            },
            plan->schema_name,
            plan->table_name
        );
    }
    information_schema_row_set_deinit(&statistics);
    information_schema_row_set_deinit(&columns);
    if (rc == MYLITE_OK) {
        return information_schema_join_compat_finish_result(
            database,
            apply_sql_select_limit,
            result,
            out_result
        );
    }
    mylite_result_free(result);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int information_schema_join_compat_execute_columns_statistics_union(
    struct mylite_db *database,
    const struct information_schema_join_compat_plan *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    const struct mylite_execution_catalog_table_definition *columns_definition =
        find_information_schema_table_definition("COLUMNS");
    const struct mylite_execution_catalog_table_definition *statistics_definition =
        find_information_schema_table_definition("STATISTICS");
    struct information_schema_row_set columns = {0};
    struct information_schema_row_set statistics = {0};
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (columns_definition == NULL || statistics_definition == NULL) {
        set_runtime_error(database, "missing INFORMATION_SCHEMA union definition");
        return MYLITE_ERROR;
    }
    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = information_schema_join_compat_append_concat_name_column(database, result);
    if (rc == MYLITE_OK) {
        rc = build_information_schema_rows(database, NULL, columns_definition, &columns);
    }
    if (rc == MYLITE_OK) {
        rc = build_information_schema_rows(database, NULL, statistics_definition, &statistics);
    }
    if (rc == MYLITE_OK && plan->schema_name != NULL && plan->table_name != NULL) {
        rc = information_schema_join_compat_append_columns_statistics_union_rows(
            database,
            result,
            &(const struct information_schema_join_compat_statistics_request){
                .columns = &columns,
                .statistics = &statistics,
            },
            plan->schema_name,
            plan->table_name
        );
    }
    information_schema_row_set_deinit(&statistics);
    information_schema_row_set_deinit(&columns);
    if (rc == MYLITE_OK) {
        return information_schema_join_compat_finish_result(
            database,
            apply_sql_select_limit,
            result,
            out_result
        );
    }
    mylite_result_free(result);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int information_schema_join_compat_execute_columns_tables_join(
    struct mylite_db *database,
    const struct information_schema_join_compat_plan *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    const struct mylite_execution_catalog_table_definition *columns_definition =
        find_information_schema_table_definition("COLUMNS");
    const struct mylite_execution_catalog_table_definition *tables_definition =
        find_information_schema_table_definition("TABLES");
    const struct information_schema_join_compat_source_column result_columns[] = {
        {columns_definition, "c", "TABLE_NAME", "TABLE_NAME"},
        {columns_definition, "c", "COLUMN_NAME", "field"},
        {columns_definition, "c", "DATA_TYPE", "type"},
        {columns_definition, "c", "COLUMN_TYPE", "COLUMN_TYPE"},
        {columns_definition, "c", "CHARACTER_MAXIMUM_LENGTH", "CHARACTER_MAXIMUM_LENGTH"},
        {columns_definition, "c", "CHARACTER_OCTET_LENGTH", "CHARACTER_OCTET_LENGTH"},
        {columns_definition, "c", "NUMERIC_PRECISION", "NUMERIC_PRECISION"},
        {columns_definition, "c", "NUMERIC_SCALE", "NUMERIC_SCALE"},
        {columns_definition, "c", "IS_NULLABLE", "null"},
        {columns_definition, "c", "COLUMN_KEY", "key"},
        {columns_definition, "c", "COLUMN_DEFAULT", "default"},
        {columns_definition, "c", "EXTRA", "EXTRA"},
        {columns_definition, "c", "COLUMN_COMMENT", "comment"},
        {columns_definition, "c", "CHARACTER_SET_NAME", "characterset"},
        {columns_definition, "c", "COLLATION_NAME", "collation"},
    };
    struct information_schema_row_set columns = {0};
    struct information_schema_row_set tables = {0};
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (columns_definition == NULL || tables_definition == NULL) {
        set_runtime_error(database, "missing INFORMATION_SCHEMA join definition");
        return MYLITE_ERROR;
    }
    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = information_schema_join_compat_append_source_columns(
        database,
        result,
        result_columns,
        sizeof(result_columns) / sizeof(result_columns[0])
    );
    if (rc == MYLITE_OK) {
        rc = build_information_schema_rows(database, NULL, columns_definition, &columns);
    }
    if (rc == MYLITE_OK) {
        rc = build_information_schema_rows(database, NULL, tables_definition, &tables);
    }
    if (rc == MYLITE_OK && plan->schema_name != NULL && plan->table_name != NULL) {
        const struct information_schema_join_compat_columns_tables_request request = {
            .columns = &columns,
            .tables = &tables,
        };

        rc = information_schema_join_compat_append_columns_tables_join_rows(
            database,
            result,
            &request,
            plan->schema_name,
            plan->table_name
        );
    }
    information_schema_row_set_deinit(&tables);
    information_schema_row_set_deinit(&columns);
    if (rc == MYLITE_OK) {
        return information_schema_join_compat_finish_result(
            database,
            apply_sql_select_limit,
            result,
            out_result
        );
    }
    mylite_result_free(result);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int information_schema_join_compat_execute_tables_grouped_size_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    const struct mylite_execution_catalog_table_definition *definition =
        find_information_schema_table_definition("TABLES");
    const struct information_schema_join_compat_source_column result_columns[] = {
        {definition, NULL, "TABLE_NAME", "table"},
        {definition, NULL, "TABLE_ROWS", "rows"},
    };
    struct information_schema_row_set rows = {0};
    struct information_schema_query query = {.definition = definition};
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (definition == NULL) {
        set_runtime_error(database, "missing INFORMATION_SCHEMA.TABLES definition");
        return MYLITE_ERROR;
    }
    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = information_schema_join_compat_append_source_columns(
        database,
        result,
        result_columns,
        sizeof(result_columns) / sizeof(result_columns[0])
    );
    if (rc == MYLITE_OK) {
        rc = information_schema_join_compat_append_grouped_bytes_column(database, result);
    }
    if (rc == MYLITE_OK) {
        rc = build_information_schema_rows(database, NULL, definition, &rows);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_join_compat_append_tables_grouped_size_rows(
            database,
            statement,
            &query,
            &rows,
            result
        );
    }
    information_schema_row_set_deinit(&rows);
    if (rc == MYLITE_OK) {
        return information_schema_join_compat_finish_result(
            database,
            apply_sql_select_limit,
            result,
            out_result
        );
    }
    mylite_result_free(result);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int information_schema_join_compat_execute_dynamic_columns_select(
    struct mylite_db *database,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    const struct mylite_execution_catalog_table_definition *definition =
        find_information_schema_table_definition("COLUMNS");
    struct information_schema_row_set rows = {0};
    struct mylite_catalog_table_descriptor table = {0};
    mylite_result *result = NULL;
    char *id_text = NULL;
    int rc = MYLITE_OK;

    if (definition == NULL) {
        set_runtime_error(database, "missing INFORMATION_SCHEMA.COLUMNS definition");
        return MYLITE_ERROR;
    }
    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_join_compat_read_dynamic_schema_id(database, &table, &id_text);
    }
    if (rc == MYLITE_OK) {
        const struct information_schema_join_compat_source_column result_columns[] = {
            {definition, "sub", "TABLE_SCHEMA", "TABLE_SCHEMA"},
            {definition, "sub", "TABLE_NAME", "TABLE_NAME"},
            {definition, "sub", "COLUMN_NAME", "COLUMN_NAME"},
        };

        rc = information_schema_join_compat_append_dynamic_id_column(database, result, &table);
        if (rc == MYLITE_OK) {
            rc = information_schema_join_compat_append_source_columns(
                database,
                result,
                result_columns,
                sizeof(result_columns) / sizeof(result_columns[0])
            );
        }
    }
    if (rc == MYLITE_OK && id_text != NULL) {
        rc = build_information_schema_rows(database, NULL, definition, &rows);
    }
    if (rc == MYLITE_OK && id_text != NULL) {
        rc = information_schema_join_compat_append_dynamic_columns_rows(
            database,
            result,
            &rows,
            id_text
        );
    }
    information_schema_row_set_deinit(&rows);
    free(id_text);
    if (rc == MYLITE_OK) {
        return information_schema_join_compat_finish_result(
            database,
            apply_sql_select_limit,
            result,
            out_result
        );
    }
    mylite_result_free(result);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int information_schema_join_compat_execute_schemata_tables_left_join(
    struct mylite_db *database,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    const struct mylite_execution_catalog_table_definition *schemata_definition =
        find_information_schema_table_definition("SCHEMATA");
    const struct mylite_execution_catalog_table_definition *tables_definition =
        find_information_schema_table_definition("TABLES");
    struct information_schema_row_set schemata = {0};
    struct information_schema_row_set tables = {0};
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (schemata_definition == NULL || tables_definition == NULL) {
        set_runtime_error(database, "missing INFORMATION_SCHEMA join definition");
        return MYLITE_ERROR;
    }
    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = information_schema_join_compat_append_definition_columns(
        database,
        result,
        schemata_definition,
        "s"
    );
    if (rc == MYLITE_OK) {
        rc = build_information_schema_rows(database, NULL, schemata_definition, &schemata);
    }
    if (rc == MYLITE_OK) {
        rc = build_information_schema_rows(database, NULL, tables_definition, &tables);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_join_compat_append_schemata_left_join_rows(
            database,
            result,
            &schemata,
            &tables
        );
    }
    information_schema_row_set_deinit(&tables);
    information_schema_row_set_deinit(&schemata);
    if (rc == MYLITE_OK) {
        return information_schema_join_compat_finish_result(
            database,
            apply_sql_select_limit,
            result,
            out_result
        );
    }
    mylite_result_free(result);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int information_schema_join_compat_finish_result(
    struct mylite_db *database,
    bool apply_sql_select_limit,
    mylite_result *result,
    mylite_result **out_result
) {
    mylite_result_set_affected_rows(result, 0);
    if (apply_sql_select_limit) {
        apply_sql_select_limit_to_result(database, result);
    }
    return finish_successful_result(database, result, out_result);
}

static int information_schema_join_compat_append_definition_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_execution_catalog_table_definition *definition,
    const char *source_alias
) {
    int rc = MYLITE_OK;

    if (definition == NULL) {
        set_runtime_error(database, "missing INFORMATION_SCHEMA definition");
        return MYLITE_ERROR;
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < definition->column_count;
         ++column_index) {
        const struct information_schema_join_compat_source_column column = {
            .definition = definition,
            .source_alias = source_alias,
            .source_column_name = definition->columns[column_index].name,
            .label = definition->columns[column_index].name,
        };

        rc = information_schema_join_compat_append_source_column(database, result, &column);
    }
    return rc;
}

static int information_schema_join_compat_append_source_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_join_compat_source_column *columns,
    size_t column_count
) {
    int rc = MYLITE_OK;

    if (columns == NULL && column_count != 0U) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < column_count; ++index) {
        rc = information_schema_join_compat_append_source_column(database, result, &columns[index]);
    }
    return rc;
}

static int information_schema_join_compat_append_source_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_join_compat_source_column *column
) {
    size_t projection_index = 0U;
    enum information_schema_projection_kind projection_kind = INFORMATION_SCHEMA_PROJECTION_COLUMN;
    const struct mylite_sql_ast_node *projection_expression = NULL;
    char *projection_name = NULL;
    struct information_schema_query query = {0};
    struct mylite_result_column_descriptor descriptor = {0};
    size_t alias_length = 0U;
    int rc = MYLITE_OK;

    if (column == NULL || column->definition == NULL || column->source_column_name == NULL ||
        column->label == NULL) {
        return MYLITE_MISUSE;
    }
    rc = information_schema_join_compat_find_definition_column(
        column->definition,
        column->source_column_name,
        &projection_index
    );
    if (rc != MYLITE_OK) {
        set_runtime_error(database, "missing INFORMATION_SCHEMA result column definition");
        return MYLITE_ERROR;
    }

    projection_name = (char *)column->label;
    query = (struct information_schema_query){
        .definition = column->definition,
        .source_schema_name = "information_schema",
        .projection_indexes = &projection_index,
        .projection_kinds = &projection_kind,
        .projection_expressions = &projection_expression,
        .projection_names = &projection_name,
        .projection_count = 1U,
    };
    if (column->source_alias != NULL) {
        alias_length = strlen(column->source_alias);
        if (alias_length >= sizeof(query.alias)) {
            set_runtime_error(database, "INFORMATION_SCHEMA source alias is too long");
            return MYLITE_ERROR;
        }
        memcpy(query.alias, column->source_alias, alias_length + 1U);
        query.has_alias = true;
    }
    rc = information_schema_make_source_column_descriptor(database, &query, 0U, &descriptor);
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_column_descriptor(result, &descriptor);
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int information_schema_join_compat_find_definition_column(
    const struct mylite_execution_catalog_table_definition *definition,
    const char *name,
    size_t *out_index
) {
    if (definition == NULL || name == NULL || out_index == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; index < definition->column_count; ++index) {
        if (text_equals_ascii_case_insensitive(definition->columns[index].name, name)) {
            *out_index = index;
            return MYLITE_OK;
        }
    }
    return MYLITE_ERROR;
}

static int information_schema_join_compat_append_concat_name_column(
    struct mylite_db *database,
    mylite_result *result
) {
    const uint64_t character_length = 73U;
    const uint64_t max_bytes =
        result_metadata_collation_max_bytes_per_character(database->session.collation_connection);
    struct mylite_result_column_descriptor descriptor = unknown_result_column_descriptor("name");
    int rc = MYLITE_OK;

    descriptor.logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
    descriptor.charset_id = result_metadata_collation_id(database->session.collation_connection);
    descriptor.collation_id = descriptor.charset_id;
    descriptor.display_length = result_metadata_display_length_cap(character_length * max_bytes);
    rc = mylite_result_append_column_descriptor(result, &descriptor);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int information_schema_join_compat_append_grouped_bytes_column(
    struct mylite_db *database,
    mylite_result *result
) {
    struct mylite_result_column_descriptor descriptor = unknown_result_column_descriptor("bytes");
    int rc = MYLITE_OK;

    descriptor.logical_type = MYLITE_RESULT_LOGICAL_TYPE_NEWDECIMAL;
    descriptor.flags = MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM;
    descriptor.charset_id = mysql_collation_binary_id;
    descriptor.collation_id = mysql_collation_binary_id;
    descriptor.display_length = 45U;
    descriptor.decimals = 0U;
    descriptor.nullable = true;
    rc = mylite_result_append_column_descriptor(result, &descriptor);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int information_schema_join_compat_append_dynamic_id_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    size_t column_index = 0U;
    struct result_column_metadata_context metadata_context = result_column_metadata_context_init();
    struct mylite_result_column_descriptor descriptor = unknown_result_column_descriptor("id");
    int rc = MYLITE_OK;

    if (table == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->table_id == 0) {
        rc = mylite_result_append_column_descriptor(result, &descriptor);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
        return rc;
    }
    rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = find_column_index(columns, column_count, "id", &column_index);
        if (rc != MYLITE_OK) {
            set_runtime_error(database, "dynamic INFORMATION_SCHEMA source is missing id column");
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = load_result_column_metadata_context_for_table_columns(
            database,
            table->table_id,
            columns,
            column_count,
            &metadata_context
        );
    }
    if (rc == MYLITE_OK) {
        descriptor.schema_name = database->session.selected_schema;
        descriptor.table_name = "sub";
        descriptor.origin_schema_name = database->session.selected_schema;
        descriptor.origin_table_name = table->name;
        descriptor.origin_column_name = columns[column_index].name;
        descriptor.nullable = columns[column_index].is_nullable;
        rc = populate_select_result_column_descriptor(
            database,
            table,
            &columns[column_index],
            &metadata_context,
            &descriptor
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_column_descriptor(result, &descriptor);
    }
    result_column_metadata_context_deinit(&metadata_context);
    free(columns);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int information_schema_join_compat_append_columns_statistics_join_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_join_compat_statistics_request *request,
    const char *schema_name,
    const char *table_name
) {
    size_t *indexes = NULL;
    size_t index_count = 0U;
    int rc = information_schema_join_compat_collect_matching_statistics_rows(
        database,
        request->statistics,
        schema_name,
        table_name,
        &indexes,
        &index_count
    );

    if (rc == MYLITE_OK) {
        rc = information_schema_join_compat_sort_statistics_row_indexes(
            request->statistics,
            indexes,
            index_count
        );
    }
    for (size_t output_index = 0U; rc == MYLITE_OK && output_index < index_count; ++output_index) {
        char **statistics_row = request->statistics->rows[indexes[output_index]];
        char **column_row = NULL;
        const char *values[] = {
            NULL,
            statistics_row[information_schema_statistics_index_name_column],
            statistics_row[information_schema_statistics_column_name_column],
        };

        column_row = information_schema_join_compat_find_column_row(
            request->columns,
            schema_name,
            table_name,
            statistics_row[information_schema_statistics_column_name_column]
        );
        if (column_row == NULL) {
            continue;
        }
        values[0] = column_row[information_schema_columns_data_type_column];
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    free(indexes);
    return rc;
}

static int information_schema_join_compat_collect_matching_statistics_rows(
    struct mylite_db *database,
    const struct information_schema_row_set *statistics,
    const char *schema_name,
    const char *table_name,
    size_t **out_indexes,
    size_t *out_index_count
) {
    size_t *indexes = NULL;
    size_t index_count = 0U;

    if (out_indexes == NULL || out_index_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_indexes = NULL;
    *out_index_count = 0U;
    for (size_t row_index = 0U; row_index < statistics->row_count; ++row_index) {
        char **row = statistics->rows[row_index];
        size_t *grown_indexes = NULL;

        if (!information_schema_join_compat_row_matches_schema_table(
                row,
                information_schema_statistics_table_schema_column,
                information_schema_statistics_table_name_column,
                schema_name,
                table_name
            )) {
            continue;
        }
        if (index_count + 1U > SIZE_MAX / sizeof(*indexes)) {
            free(indexes);
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        grown_indexes = realloc(indexes, (index_count + 1U) * sizeof(*indexes));
        if (grown_indexes == NULL) {
            free(indexes);
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        indexes = grown_indexes;
        indexes[index_count] = row_index;
        ++index_count;
    }

    *out_indexes = indexes;
    *out_index_count = index_count;
    return MYLITE_OK;
}

static int information_schema_join_compat_sort_statistics_row_indexes(
    const struct information_schema_row_set *statistics,
    size_t *indexes,
    size_t index_count
) {
    for (size_t left = 0U; left < index_count; ++left) {
        size_t best = left;

        for (size_t right = left + 1U; right < index_count; ++right) {
            if (information_schema_join_compat_compare_statistics_row_indexes(
                    statistics,
                    (struct information_schema_join_compat_statistics_order){
                        .left_index = indexes[right],
                        .right_index = indexes[best],
                    }
                ) < 0) {
                best = right;
            }
        }
        if (best != left) {
            size_t temporary = indexes[left];

            indexes[left] = indexes[best];
            indexes[best] = temporary;
        }
    }
    return MYLITE_OK;
}

static int information_schema_join_compat_compare_statistics_row_indexes(
    const struct information_schema_row_set *statistics,
    struct information_schema_join_compat_statistics_order order
) {
    char **left = statistics->rows[order.left_index];
    char **right = statistics->rows[order.right_index];
    int comparison = compare_ascii_case_insensitive_text(
        left[information_schema_statistics_index_name_column],
        right[information_schema_statistics_index_name_column]
    );

    if (comparison != 0) {
        return comparison;
    }
    return compare_ascii_case_insensitive_text(
        left[information_schema_statistics_seq_in_index_column],
        right[information_schema_statistics_seq_in_index_column]
    );
}

static char **information_schema_join_compat_find_column_row(
    const struct information_schema_row_set *columns,
    const char *schema_name,
    const char *table_name,
    const char *column_name
) {
    for (size_t row_index = 0U; row_index < columns->row_count; ++row_index) {
        char **row = columns->rows[row_index];

        if (information_schema_join_compat_row_matches_schema_table(
                row,
                information_schema_columns_table_schema_column,
                information_schema_columns_table_name_column,
                schema_name,
                table_name
            ) &&
            text_equals_ascii_case_insensitive(
                row[information_schema_columns_column_name_column],
                column_name
            )) {
            return row;
        }
    }
    return NULL;
}

static bool information_schema_join_compat_row_matches_schema_table(
    char *const *row,
    size_t schema_column,
    size_t table_column,
    const char *schema_name,
    const char *table_name
) {
    return (row != NULL &&
            text_equals_ascii_case_insensitive(row[schema_column], schema_name) &&
            text_equals_ascii_case_insensitive(row[table_column], table_name)) != 0;
}

static int information_schema_join_compat_append_columns_statistics_union_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_join_compat_statistics_request *request,
    const char *schema_name,
    const char *table_name
) {
    struct information_schema_join_compat_name_rows names = {0};
    int rc = information_schema_join_compat_append_column_names(
        database,
        &names,
        request->columns,
        schema_name,
        table_name
    );

    if (rc == MYLITE_OK) {
        rc = information_schema_join_compat_append_distinct_index_names(
            database,
            &names,
            request->statistics,
            schema_name,
            table_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_join_compat_sort_name_rows(&names);
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < names.count; ++index) {
        const char *const values[] = {names.items[index]};

        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    information_schema_join_compat_name_rows_deinit(&names);
    return rc;
}

static int information_schema_join_compat_append_columns_tables_join_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_join_compat_columns_tables_request *request,
    const char *schema_name,
    const char *table_name
) {
    int rc = MYLITE_OK;

    if (!information_schema_join_compat_has_base_table_row(request->tables, schema_name, table_name)) {
        return MYLITE_OK;
    }
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < request->columns->row_count;
         ++row_index) {
        char **row = request->columns->rows[row_index];

        if (!information_schema_join_compat_row_matches_schema_table(
                row,
                information_schema_columns_table_schema_column,
                information_schema_columns_table_name_column,
                schema_name,
                table_name
            )) {
            continue;
        }
        rc = information_schema_join_compat_append_columns_tables_join_row(
            database,
            result,
            row
        );
    }
    return rc;
}

static bool information_schema_join_compat_has_base_table_row(
    const struct information_schema_row_set *tables,
    const char *schema_name,
    const char *table_name
) {
    if (tables == NULL || schema_name == NULL || table_name == NULL) {
        return false;
    }
    for (size_t row_index = 0U; row_index < tables->row_count; ++row_index) {
        char **row = tables->rows[row_index];

        if (information_schema_join_compat_row_matches_schema_table(
                row,
                information_schema_tables_table_schema_column,
                information_schema_tables_table_name_column,
                schema_name,
                table_name
            ) &&
            text_equals_ascii_case_insensitive(
                row[information_schema_tables_table_type_column],
                "BASE TABLE"
            )) {
            return true;
        }
    }
    return false;
}

static int information_schema_join_compat_append_columns_tables_join_row(
    struct mylite_db *database,
    mylite_result *result,
    char *const *row
) {
    const char *const values[] = {
        row[information_schema_columns_table_name_column],
        row[information_schema_columns_column_name_column],
        row[information_schema_columns_data_type_column],
        row[information_schema_columns_column_type_column],
        row[information_schema_columns_character_maximum_length_column],
        row[information_schema_columns_character_octet_length_column],
        row[information_schema_columns_numeric_precision_column],
        row[information_schema_columns_numeric_scale_column],
        row[information_schema_columns_is_nullable_column],
        row[information_schema_columns_column_key_column],
        row[information_schema_columns_default_column],
        row[information_schema_columns_extra_column],
        row[information_schema_columns_column_comment_column],
        row[information_schema_columns_character_set_name_column],
        row[information_schema_columns_collation_name_column],
    };
    int rc = mylite_result_append_text_row(result, values);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}

static int information_schema_join_compat_append_tables_grouped_size_rows(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result
) {
    struct information_schema_row_index_set indexes = {0};
    int rc = information_schema_matching_row_indexes(
        database,
        statement,
        query,
        rows,
        &indexes
    );

    if (rc == MYLITE_OK) {
        rc = information_schema_sort_tables_default_row_indexes(
            database,
            rows,
            indexes.items,
            indexes.count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < indexes.count; ++index) {
        rc = information_schema_join_compat_append_tables_grouped_size_row(
            database,
            rows->rows[indexes.items[index]],
            result
        );
    }
    information_schema_row_index_set_deinit(&indexes);
    return rc;
}

static int information_schema_join_compat_append_tables_grouped_size_row(
    struct mylite_db *database,
    char *const *row,
    mylite_result *result
) {
    char bytes_text[integer_text_capacity];
    const char *values[] = {
        row[information_schema_tables_table_name_column],
        row[information_schema_tables_table_rows_column],
        bytes_text,
    };
    int rc = information_schema_join_compat_format_table_size_bytes(
        database,
        row,
        bytes_text,
        sizeof(bytes_text)
    );

    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static int information_schema_join_compat_format_table_size_bytes(
    struct mylite_db *database,
    char *const *row,
    char *buffer,
    size_t buffer_size
) {
    uint64_t data_length = 0;
    uint64_t index_length = 0;

    if (!information_schema_join_compat_parse_unsigned_text(
            row[information_schema_tables_data_length_column],
            &data_length
        ) ||
        !information_schema_join_compat_parse_unsigned_text(
            row[information_schema_tables_index_length_column],
            &index_length
        ) ||
        data_length > UINT64_MAX - index_length) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA TABLES size metadata exceeds unsigned 64-bit range"
        );
        return MYLITE_ERROR;
    }
    return format_uint64(database, data_length + index_length, buffer, buffer_size);
}

static bool information_schema_join_compat_parse_unsigned_text(
    const char *text,
    uint64_t *out_value
) {
    uint64_t value = 0;

    if (text == NULL || *text == '\0' || out_value == NULL) {
        return false;
    }
    while (*text != '\0') {
        unsigned char byte = (unsigned char)*text;
        uint64_t digit = 0;

        if (byte < (unsigned char)'0' || byte > (unsigned char)'9') {
            return false;
        }
        digit = (uint64_t)(byte - (unsigned char)'0');
        if (value > (UINT64_MAX - digit) / (uint64_t)decimal_base) {
            return false;
        }
        value = (value * (uint64_t)decimal_base) + digit;
        ++text;
    }
    *out_value = value;
    return true;
}

static int information_schema_join_compat_append_column_names(
    struct mylite_db *database,
    struct information_schema_join_compat_name_rows *names,
    const struct information_schema_row_set *columns,
    const char *schema_name,
    const char *table_name
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < columns->row_count; ++row_index) {
        char **row = columns->rows[row_index];

        if (!information_schema_join_compat_row_matches_schema_table(
                row,
                information_schema_columns_table_schema_column,
                information_schema_columns_table_name_column,
                schema_name,
                table_name
            )) {
            continue;
        }
        rc = information_schema_join_compat_append_name(
            database,
            names,
            row[information_schema_columns_column_name_column],
            " (column)"
        );
    }
    return rc;
}

static int information_schema_join_compat_append_distinct_index_names(
    struct mylite_db *database,
    struct information_schema_join_compat_name_rows *names,
    const struct information_schema_row_set *statistics,
    const char *schema_name,
    const char *table_name
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < statistics->row_count; ++row_index) {
        char **row = statistics->rows[row_index];
        struct mylite_dynamic_string candidate;

        if (!information_schema_join_compat_row_matches_schema_table(
                row,
                information_schema_statistics_table_schema_column,
                information_schema_statistics_table_name_column,
                schema_name,
                table_name
            )) {
            continue;
        }

        mylite_dynamic_string_init(&candidate);
        rc = mylite_dynamic_string_append(
            &candidate,
            row[information_schema_statistics_index_name_column]
        );
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&candidate, " (index)");
        }
        if (rc == MYLITE_OK && !information_schema_join_compat_name_rows_contain(
                                names,
                                candidate.text
                            )) {
            rc = information_schema_join_compat_append_name(
                database,
                names,
                row[information_schema_statistics_index_name_column],
                " (index)"
            );
        }
        mylite_dynamic_string_deinit(&candidate);
    }
    return rc;
}

static int information_schema_join_compat_append_name(
    struct mylite_db *database,
    struct information_schema_join_compat_name_rows *names,
    const char *name,
    const char *suffix
) {
    struct mylite_dynamic_string string;
    char **grown_items = NULL;
    int rc = MYLITE_OK;

    if (names->count + 1U > SIZE_MAX / sizeof(*names->items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    mylite_dynamic_string_init(&string);
    rc = mylite_dynamic_string_append(&string, name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, suffix);
    }
    if (rc == MYLITE_OK) {
        grown_items = (char **)realloc(
            (void *)names->items,
            (names->count + 1U) * sizeof(*names->items)
        );
        if (grown_items == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_OK) {
        names->items = grown_items;
        names->items[names->count] = mylite_dynamic_string_take(&string);
        if (names->items[names->count] == NULL) {
            rc = MYLITE_NOMEM;
        } else {
            ++names->count;
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}

static bool information_schema_join_compat_name_rows_contain(
    const struct information_schema_join_compat_name_rows *names,
    const char *name
) {
    for (size_t index = 0U; index < names->count; ++index) {
        if (text_equals_ascii_case_insensitive(names->items[index], name)) {
            return true;
        }
    }
    return false;
}

static int information_schema_join_compat_sort_name_rows(
    struct information_schema_join_compat_name_rows *names
) {
    for (size_t left = 0U; left < names->count; ++left) {
        size_t best = left;

        for (size_t right = left + 1U; right < names->count; ++right) {
            if (compare_ascii_case_insensitive_text(names->items[right], names->items[best]) < 0) {
                best = right;
            }
        }
        if (best != left) {
            char *temporary = names->items[left];

            names->items[left] = names->items[best];
            names->items[best] = temporary;
        }
    }
    return MYLITE_OK;
}

static void information_schema_join_compat_name_rows_deinit(
    struct information_schema_join_compat_name_rows *names
) {
    if (names == NULL) {
        return;
    }
    for (size_t index = 0U; index < names->count; ++index) {
        free(names->items[index]);
    }
    free((void *)names->items);
    *names = (struct information_schema_join_compat_name_rows){0};
}

static int information_schema_join_compat_append_schemata_left_join_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_row_set *schemata,
    const struct information_schema_row_set *tables
) {
    bool *emitted = NULL;
    int rc = MYLITE_OK;

    if (schemata == NULL || tables == NULL || schemata->row_count == 0U) {
        return MYLITE_OK;
    }
    emitted = calloc(schemata->row_count, sizeof(*emitted));
    if (emitted == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t output_index = 0U; rc == MYLITE_OK && output_index < schemata->row_count;
         ++output_index) {
        size_t row_index = information_schema_join_compat_next_schema_row(schemata, emitted);
        char **row = schemata->rows[row_index];
        size_t match_count = 0U;

        emitted[row_index] = true;
        if (row == NULL) {
            continue;
        }
        match_count = information_schema_join_compat_count_table_matches(tables, row[1]);
        if (match_count == 0U) {
            match_count = 1U;
        }
        for (size_t match_index = 0U; rc == MYLITE_OK && match_index < match_count; ++match_index) {
            rc = mylite_result_append_text_row(result, (const char *const *)row);
            if (rc != MYLITE_OK) {
                set_nomem_error(database);
            }
        }
    }
    free(emitted);
    return rc;
}

static size_t information_schema_join_compat_count_table_matches(
    const struct information_schema_row_set *tables,
    const char *schema_name
) {
    size_t match_count = 0U;

    if (tables == NULL || schema_name == NULL) {
        return 0U;
    }
    for (size_t row_index = 0U; row_index < tables->row_count; ++row_index) {
        char **row = tables->rows[row_index];

        if (row != NULL &&
            text_equals_ascii_case_insensitive(
                row[information_schema_tables_table_schema_column],
                schema_name
            )) {
            ++match_count;
        }
    }
    return match_count;
}

static size_t information_schema_join_compat_next_schema_row(
    const struct information_schema_row_set *schemata,
    const bool *emitted
) {
    size_t best_index = SIZE_MAX;

    for (size_t row_index = 0U; row_index < schemata->row_count; ++row_index) {
        char **row = schemata->rows[row_index];
        char **best = best_index == SIZE_MAX ? NULL : schemata->rows[best_index];

        if (emitted[row_index] || row == NULL) {
            continue;
        }
        if (best == NULL || compare_ascii_case_insensitive_text(row[1], best[1]) < 0) {
            best_index = row_index;
        }
    }
    return best_index == SIZE_MAX ? 0U : best_index;
}

static int information_schema_join_compat_read_dynamic_schema_id(
    struct mylite_db *database,
    struct mylite_catalog_table_descriptor *out_table,
    char **out_id
) {
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    if (out_table == NULL || out_id == NULL) {
        return MYLITE_MISUSE;
    }
    *out_table = (struct mylite_catalog_table_descriptor){0};
    *out_id = NULL;
    if (!database->session.has_selected_schema) {
        return MYLITE_OK;
    }

    rc = resolve_selected_schema(database, &schema);
    if (rc == MYLITE_OK) {
        bool found = false;

        rc = mylite_catalog_try_read_table_by_name(database, schema.schema_id, "t", &table, &found);
        if (rc == MYLITE_OK && !found) {
            return MYLITE_OK;
        }
    }
    if (rc == MYLITE_OK) {
        *out_table = table;
        rc = information_schema_join_compat_build_dynamic_schema_id_sql(&table, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_bind_text(
            statement,
            1,
            database->session.selected_schema,
            sqlite_use_nul_terminated_string,
            SQLITE_TRANSIENT
        );
        rc = mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            const unsigned char *text = sqlite3_column_text(statement, 0);

            if (text != NULL) {
                rc = duplicate_text(database, (const char *)text, out_id);
            }
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    if (rc != MYLITE_OK && rc != MYLITE_NOMEM) {
        set_physical_sqlite_error(database);
    }
    return rc;
}

static int information_schema_join_compat_build_dynamic_schema_id_sql(
    const struct mylite_catalog_table_descriptor *table,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (table == NULL || out_sql == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    mylite_dynamic_string_init(&string);
    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, "id");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, table->physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, "db_name");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " = ? ORDER BY ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, "id");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " LIMIT 1");
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int information_schema_join_compat_append_dynamic_columns_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_row_set *rows,
    const char *id_text
) {
    int rc = MYLITE_OK;

    if (rows == NULL || id_text == NULL) {
        return MYLITE_OK;
    }
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < rows->row_count; ++row_index) {
        char **row = rows->rows[row_index];

        if (row == NULL ||
            !text_equals_ascii_case_insensitive(
                row[information_schema_columns_table_schema_column],
                database->session.selected_schema
            ) ||
            !text_equals_ascii_case_insensitive(
                row[information_schema_columns_table_name_column],
                "t"
            )) {
            continue;
        }
        const char *const values[] = {
            id_text,
            row[information_schema_columns_table_schema_column],
            row[information_schema_columns_table_name_column],
            row[information_schema_columns_column_name_column],
        };

        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}
