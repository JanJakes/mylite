enum information_schema_join_compat_kind {
    INFORMATION_SCHEMA_JOIN_COMPAT_NONE = 0,
    INFORMATION_SCHEMA_JOIN_COMPAT_COLUMNS_STATISTICS,
    INFORMATION_SCHEMA_JOIN_COMPAT_COLUMNS_STATISTICS_UNION,
    INFORMATION_SCHEMA_JOIN_COMPAT_COLUMNS_TABLES,
    INFORMATION_SCHEMA_JOIN_COMPAT_TABLES_GROUPED_SIZE,
    INFORMATION_SCHEMA_JOIN_COMPAT_DYNAMIC_COLUMNS,
    INFORMATION_SCHEMA_JOIN_COMPAT_SCHEMATA_TABLES,
};

struct information_schema_join_compat_plan {
    enum information_schema_join_compat_kind kind;
    char *schema_name;
    char *table_name;
};

struct information_schema_join_compat_column_reference {
    const char *qualifier;
    const char *name;
};

struct information_schema_join_compat_projection {
    const char *qualifier;
    const char *name;
    const char *alias;
};

struct information_schema_join_compat_column_equality {
    struct information_schema_join_compat_column_reference left;
    struct information_schema_join_compat_column_reference right;
};

struct information_schema_join_compat_literal_filter {
    const char *qualifier;
    const char *name;
    char **out_value;
};

static int information_schema_join_compat_plan_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct information_schema_join_compat_plan *out_plan
);
static void information_schema_join_compat_plan_deinit(
    struct information_schema_join_compat_plan *plan
);
static int information_schema_join_compat_plan_columns_statistics_union(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct information_schema_join_compat_plan *out_plan,
    bool *out_matches
);
static int information_schema_join_compat_plan_columns_statistics(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct information_schema_join_compat_plan *out_plan,
    bool *out_matches
);
static int information_schema_join_compat_plan_columns_tables(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct information_schema_join_compat_plan *out_plan,
    bool *out_matches
);
static int information_schema_join_compat_plan_tables_grouped_size(
    const struct mylite_sql_ast_node *statement,
    bool *out_matches
);
static int information_schema_join_compat_plan_dynamic_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool *out_matches
);
static int information_schema_join_compat_plan_schemata_tables(
    const struct mylite_sql_ast_node *statement,
    bool *out_matches
);
static bool information_schema_join_compat_select_projections_match(
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_join_compat_projection *expected,
    size_t expected_count
);
static bool information_schema_join_compat_source_table_matches(
    const struct mylite_sql_ast_node *source,
    const char *schema_name,
    const char *table_name,
    const char *alias
);
static bool information_schema_join_compat_join_matches(
    const struct mylite_sql_ast_node *source,
    enum mylite_sql_ast_join_kind join_kind,
    const char *left_schema,
    const char *left_table,
    const char *left_alias,
    const char *right_schema,
    const char *right_table,
    const char *right_alias,
    const struct information_schema_join_compat_column_equality *equalities,
    size_t equality_count
);
static bool information_schema_join_compat_column_equalities_match(
    const struct mylite_sql_ast_node *predicate,
    const struct information_schema_join_compat_column_equality *expected,
    size_t expected_count
);
static int information_schema_join_compat_copy_literal_filters(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    struct information_schema_join_compat_literal_filter *filters,
    size_t filter_count,
    bool *out_matches
);
static bool information_schema_join_compat_order_matches(
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_join_compat_column_reference *expected,
    size_t expected_count
);
static bool information_schema_join_compat_group_matches(
    const struct mylite_sql_ast_node *statement,
    const char *qualifier,
    const char *name
);
static const struct mylite_sql_ast_node *information_schema_join_compat_direct_child_kind(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
);
static bool information_schema_join_compat_has_direct_child_kind(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
);
static bool information_schema_join_compat_column_reference_matches(
    const struct mylite_sql_ast_node *node,
    const char *qualifier,
    const char *name
);
static bool information_schema_join_compat_identifier_parts_match(
    const struct mylite_sql_ast_node *node,
    const char *const *expected,
    size_t expected_count
);
static bool information_schema_join_compat_identifier_span_matches(
    const struct mylite_sql_source_span *span,
    const char *expected
);
static bool information_schema_join_compat_alias_matches(
    const struct mylite_sql_ast_node *node,
    const char *expected
);
static bool information_schema_join_compat_ascii_equal(char left, char right);
static bool information_schema_join_compat_literal_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    const char *expected
);
static bool information_schema_join_compat_concat_projection_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const char *argument,
    const char *suffix,
    const char *alias,
    const char *source
);

static int information_schema_join_compat_plan_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct information_schema_join_compat_plan *out_plan
) {
    bool matches = false;
    int rc = MYLITE_OK;

    if (database == NULL || statement == NULL || out_plan == NULL) {
        return MYLITE_MISUSE;
    }
    *out_plan = (struct information_schema_join_compat_plan){0};

    rc = information_schema_join_compat_plan_columns_statistics_union(
        database,
        statement,
        out_plan,
        &matches
    );
    if (rc != MYLITE_OK || matches) {
        return rc;
    }
    rc = information_schema_join_compat_plan_columns_statistics(
        database,
        statement,
        out_plan,
        &matches
    );
    if (rc != MYLITE_OK || matches) {
        return rc;
    }
    rc =
        information_schema_join_compat_plan_columns_tables(database, statement, out_plan, &matches);
    if (rc != MYLITE_OK || matches) {
        return rc;
    }
    rc = information_schema_join_compat_plan_tables_grouped_size(statement, &matches);
    if (rc != MYLITE_OK || matches) {
        out_plan->kind = matches ? INFORMATION_SCHEMA_JOIN_COMPAT_TABLES_GROUPED_SIZE
                                 : INFORMATION_SCHEMA_JOIN_COMPAT_NONE;
        return rc;
    }
    rc = information_schema_join_compat_plan_dynamic_columns(database, statement, &matches);
    if (rc != MYLITE_OK || matches) {
        out_plan->kind = matches ? INFORMATION_SCHEMA_JOIN_COMPAT_DYNAMIC_COLUMNS
                                 : INFORMATION_SCHEMA_JOIN_COMPAT_NONE;
        return rc;
    }
    rc = information_schema_join_compat_plan_schemata_tables(statement, &matches);
    if (rc != MYLITE_OK || matches) {
        out_plan->kind = matches ? INFORMATION_SCHEMA_JOIN_COMPAT_SCHEMATA_TABLES
                                 : INFORMATION_SCHEMA_JOIN_COMPAT_NONE;
    }
    return rc;
}

static void information_schema_join_compat_plan_deinit(
    struct information_schema_join_compat_plan *plan
) {
    if (plan == NULL) {
        return;
    }
    free(plan->table_name);
    free(plan->schema_name);
    *plan = (struct information_schema_join_compat_plan){0};
}

static int information_schema_join_compat_plan_columns_statistics_union(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct information_schema_join_compat_plan *out_plan,
    bool *out_matches
) {
    const struct mylite_sql_ast_node *with_clause = child_at(statement, 0U);
    const struct mylite_sql_ast_node *compound = child_at(statement, 1U);
    const struct mylite_sql_ast_node *order_clause = child_at(statement, 2U);
    const struct mylite_sql_ast_node *columns_cte = NULL;
    const struct mylite_sql_ast_node *indexes_cte = NULL;
    const struct mylite_sql_ast_node *columns_select = NULL;
    const struct mylite_sql_ast_node *indexes_select = NULL;
    const struct mylite_sql_ast_node *union_terms = NULL;
    const struct mylite_sql_ast_node *union_term = NULL;
    const struct mylite_sql_ast_node *right_select = NULL;
    char *columns_schema = NULL;
    char *columns_table = NULL;
    char *indexes_schema = NULL;
    char *indexes_table = NULL;
    bool filters_match = false;
    int rc = MYLITE_OK;

    if (out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matches = false;
    if (with_clause == NULL || with_clause->kind != MYLITE_SQL_AST_WITH_CLAUSE ||
        compound == NULL || compound->kind != MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT ||
        order_clause == NULL || order_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE ||
        mylite_sql_ast_node_child_count(statement) != 3U ||
        mylite_sql_ast_node_child_count(with_clause) != 2U) {
        return MYLITE_OK;
    }
    columns_cte = child_at(with_clause, 0U);
    indexes_cte = child_at(with_clause, 1U);
    if (columns_cte == NULL || indexes_cte == NULL ||
        columns_cte->kind != MYLITE_SQL_AST_COMMON_TABLE_EXPRESSION ||
        indexes_cte->kind != MYLITE_SQL_AST_COMMON_TABLE_EXPRESSION ||
        !information_schema_join_compat_identifier_parts_match(
            child_at(columns_cte, 0U),
            (const char *const[]){"cols"},
            1U
        ) ||
        !information_schema_join_compat_identifier_parts_match(
            child_at(indexes_cte, 0U),
            (const char *const[]){"indexes"},
            1U
        )) {
        return MYLITE_OK;
    }
    columns_select = child_at(columns_cte, 1U);
    indexes_select = child_at(indexes_cte, 1U);
    if (!information_schema_join_compat_select_projections_match(
            columns_select,
            (const struct information_schema_join_compat_projection[]){
                {.name = "COLUMN_NAME", .alias = "column_name"},
            },
            1U
        ) ||
        !information_schema_join_compat_source_table_matches(
            child_at(columns_select, 1U),
            "information_schema",
            "columns",
            NULL
        ) ||
        !information_schema_join_compat_select_projections_match(
            indexes_select,
            (const struct information_schema_join_compat_projection[]){
                {.name = "INDEX_NAME", .alias = "index_name"},
            },
            1U
        ) ||
        mylite_sql_ast_node_select_modifier(indexes_select) !=
            MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT ||
        !information_schema_join_compat_source_table_matches(
            child_at(indexes_select, 1U),
            "information_schema",
            "statistics",
            NULL
        )) {
        return MYLITE_OK;
    }
    rc = information_schema_join_compat_copy_literal_filters(
        database,
        child_at(
            information_schema_join_compat_direct_child_kind(
                columns_select,
                MYLITE_SQL_AST_WHERE_CLAUSE
            ),
            0U
        ),
        (struct information_schema_join_compat_literal_filter[]){
            {.name = "TABLE_SCHEMA", .out_value = &columns_schema},
            {.name = "TABLE_NAME", .out_value = &columns_table},
        },
        2U,
        &filters_match
    );
    if (rc != MYLITE_OK || !filters_match) {
        goto cleanup;
    }
    rc = information_schema_join_compat_copy_literal_filters(
        database,
        child_at(
            information_schema_join_compat_direct_child_kind(
                indexes_select,
                MYLITE_SQL_AST_WHERE_CLAUSE
            ),
            0U
        ),
        (struct information_schema_join_compat_literal_filter[]){
            {.name = "TABLE_SCHEMA", .out_value = &indexes_schema},
            {.name = "TABLE_NAME", .out_value = &indexes_table},
        },
        2U,
        &filters_match
    );
    if (rc != MYLITE_OK || !filters_match ||
        !text_equals_ascii_case_insensitive(columns_schema, indexes_schema) ||
        !text_equals_ascii_case_insensitive(columns_table, indexes_table)) {
        goto cleanup;
    }
    union_terms = child_at(compound, 1U);
    union_term = child_at(union_terms, 0U);
    right_select = child_at(union_term, 0U);
    if (mylite_sql_ast_node_child_count(union_terms) != 1U ||
        mylite_sql_ast_node_set_operator_kind(union_term) != MYLITE_SQL_AST_SET_OPERATOR_UNION ||
        mylite_sql_ast_node_union_modifier(union_term) != MYLITE_SQL_AST_UNION_MODIFIER_ALL ||
        !information_schema_join_compat_concat_projection_matches(
            database,
            child_at(compound, 0U),
            "column_name",
            " (column)",
            "name",
            "cols"
        ) ||
        !information_schema_join_compat_concat_projection_matches(
            database,
            right_select,
            "index_name",
            " (index)",
            "name",
            "indexes"
        ) ||
        !information_schema_join_compat_column_reference_matches(
            child_at(order_clause, 0U),
            NULL,
            "name"
        )) {
        goto cleanup;
    }

    out_plan->kind = INFORMATION_SCHEMA_JOIN_COMPAT_COLUMNS_STATISTICS_UNION;
    out_plan->schema_name = columns_schema;
    out_plan->table_name = columns_table;
    columns_schema = NULL;
    columns_table = NULL;
    *out_matches = true;

cleanup:
    free(indexes_table);
    free(indexes_schema);
    free(columns_table);
    free(columns_schema);
    return rc;
}

static int information_schema_join_compat_plan_columns_statistics(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct information_schema_join_compat_plan *out_plan,
    bool *out_matches
) {
    static const struct information_schema_join_compat_projection projections[] = {
        {.qualifier = "cols", .name = "DATA_TYPE"},
        {.qualifier = "stats", .name = "INDEX_NAME"},
        {.qualifier = "stats", .name = "COLUMN_NAME"},
    };
    static const struct information_schema_join_compat_column_equality equalities[] = {
        {{"cols", "TABLE_SCHEMA"}, {"stats", "TABLE_SCHEMA"}},
        {{"cols", "TABLE_NAME"}, {"stats", "TABLE_NAME"}},
        {{"cols", "COLUMN_NAME"}, {"stats", "COLUMN_NAME"}},
    };
    static const struct information_schema_join_compat_column_reference order[] = {
        {NULL, "INDEX_NAME"},
    };
    const struct mylite_sql_ast_node *source = child_at(statement, 1U);
    bool filters_match = false;
    int rc = MYLITE_OK;

    *out_matches = false;
    if (!information_schema_join_compat_select_projections_match(
            statement,
            projections,
            sizeof(projections) / sizeof(projections[0])
        ) ||
        !information_schema_join_compat_join_matches(
            source,
            MYLITE_SQL_AST_JOIN_KIND_INNER,
            "information_schema",
            "columns",
            "cols",
            "information_schema",
            "statistics",
            "stats",
            equalities,
            sizeof(equalities) / sizeof(equalities[0])
        ) ||
        !information_schema_join_compat_order_matches(statement, order, 1U) ||
        information_schema_join_compat_has_direct_child_kind(
            statement,
            MYLITE_SQL_AST_GROUP_BY_CLAUSE
        ) ||
        information_schema_join_compat_has_direct_child_kind(
            statement,
            MYLITE_SQL_AST_HAVING_CLAUSE
        ) ||
        information_schema_join_compat_has_direct_child_kind(
            statement,
            MYLITE_SQL_AST_LIMIT_CLAUSE
        )) {
        return MYLITE_OK;
    }
    rc = information_schema_join_compat_copy_literal_filters(
        database,
        child_at(
            information_schema_join_compat_direct_child_kind(
                statement,
                MYLITE_SQL_AST_WHERE_CLAUSE
            ),
            0U
        ),
        (struct information_schema_join_compat_literal_filter[]){
            {.qualifier = "cols", .name = "TABLE_SCHEMA", .out_value = &out_plan->schema_name},
            {.qualifier = "cols", .name = "TABLE_NAME", .out_value = &out_plan->table_name},
        },
        2U,
        &filters_match
    );
    if (rc == MYLITE_OK && filters_match) {
        out_plan->kind = INFORMATION_SCHEMA_JOIN_COMPAT_COLUMNS_STATISTICS;
        *out_matches = true;
    } else if (rc == MYLITE_OK) {
        information_schema_join_compat_plan_deinit(out_plan);
    }
    return rc;
}

static int information_schema_join_compat_plan_columns_tables(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct information_schema_join_compat_plan *out_plan,
    bool *out_matches
) {
    static const struct information_schema_join_compat_projection projections[] = {
        {"c", "TABLE_NAME", NULL},
        {"c", "COLUMN_NAME", "field"},
        {"c", "DATA_TYPE", "type"},
        {"c", "COLUMN_TYPE", NULL},
        {"c", "CHARACTER_MAXIMUM_LENGTH", NULL},
        {"c", "CHARACTER_OCTET_LENGTH", NULL},
        {"c", "NUMERIC_PRECISION", NULL},
        {"c", "NUMERIC_SCALE", NULL},
        {"c", "IS_NULLABLE", "null"},
        {"c", "COLUMN_KEY", "key"},
        {"c", "COLUMN_DEFAULT", "default"},
        {"c", "EXTRA", NULL},
        {"c", "COLUMN_COMMENT", "comment"},
        {"c", "CHARACTER_SET_NAME", "characterset"},
        {"c", "COLLATION_NAME", "collation"},
    };
    static const struct information_schema_join_compat_column_equality equalities[] = {
        {{"t", "TABLE_NAME"}, {"c", "TABLE_NAME"}},
    };
    static const struct information_schema_join_compat_column_reference order[] = {
        {"c", "TABLE_NAME"},
        {"c", "ORDINAL_POSITION"},
    };
    char *table_schema = NULL;
    char *table_type = NULL;
    bool filters_match = false;
    int rc = MYLITE_OK;

    *out_matches = false;
    if (!information_schema_join_compat_select_projections_match(
            statement,
            projections,
            sizeof(projections) / sizeof(projections[0])
        ) ||
        !information_schema_join_compat_join_matches(
            child_at(statement, 1U),
            MYLITE_SQL_AST_JOIN_KIND_INNER,
            "information_schema",
            "columns",
            "c",
            "information_schema",
            "tables",
            "t",
            equalities,
            1U
        ) ||
        !information_schema_join_compat_order_matches(statement, order, 2U) ||
        information_schema_join_compat_has_direct_child_kind(
            statement,
            MYLITE_SQL_AST_GROUP_BY_CLAUSE
        ) ||
        information_schema_join_compat_has_direct_child_kind(
            statement,
            MYLITE_SQL_AST_HAVING_CLAUSE
        ) ||
        information_schema_join_compat_has_direct_child_kind(
            statement,
            MYLITE_SQL_AST_LIMIT_CLAUSE
        )) {
        return MYLITE_OK;
    }
    rc = information_schema_join_compat_copy_literal_filters(
        database,
        child_at(
            information_schema_join_compat_direct_child_kind(
                statement,
                MYLITE_SQL_AST_WHERE_CLAUSE
            ),
            0U
        ),
        (struct information_schema_join_compat_literal_filter[]){
            {.qualifier = "c", .name = "TABLE_SCHEMA", .out_value = &out_plan->schema_name},
            {.qualifier = "t", .name = "TABLE_SCHEMA", .out_value = &table_schema},
            {.qualifier = "t", .name = "TABLE_NAME", .out_value = &out_plan->table_name},
            {.qualifier = "t", .name = "TABLE_TYPE", .out_value = &table_type},
        },
        4U,
        &filters_match
    );
    if (rc == MYLITE_OK && filters_match &&
        text_equals_ascii_case_insensitive(out_plan->schema_name, table_schema) &&
        text_equals_ascii_case_insensitive(table_type, "BASE TABLE")) {
        out_plan->kind = INFORMATION_SCHEMA_JOIN_COMPAT_COLUMNS_TABLES;
        *out_matches = true;
    } else if (rc == MYLITE_OK) {
        information_schema_join_compat_plan_deinit(out_plan);
    }
    free(table_type);
    free(table_schema);
    return rc;
}

static int information_schema_join_compat_plan_tables_grouped_size(
    const struct mylite_sql_ast_node *statement,
    bool *out_matches
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *first_item = child_at(select_list, 0U);
    const struct mylite_sql_ast_node *second_item = child_at(select_list, 1U);
    const struct mylite_sql_ast_node *third_item = child_at(select_list, 2U);
    const struct mylite_sql_ast_node *sum = child_at(third_item, 0U);
    const struct mylite_sql_ast_node *addition = child_at(sum, 0U);
    static const struct information_schema_join_compat_column_reference order[] = {
        {NULL, "TABLE_NAME"},
    };

    if (out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matches =
        mylite_sql_ast_node_child_count(select_list) == 3U &&
        information_schema_join_compat_column_reference_matches(
            child_at(first_item, 0U),
            NULL,
            "TABLE_NAME"
        ) &&
        information_schema_join_compat_alias_matches(child_at(first_item, 1U), "table") &&
        information_schema_join_compat_column_reference_matches(
            child_at(second_item, 0U),
            NULL,
            "TABLE_ROWS"
        ) &&
        information_schema_join_compat_alias_matches(child_at(second_item, 1U), "rows") &&
        sum != NULL && sum->kind == MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION && addition != NULL &&
        addition->kind == MYLITE_SQL_AST_BINARY_EXPRESSION &&
        mylite_sql_ast_node_operator(addition) == MYLITE_SQL_AST_OPERATOR_ADD &&
        information_schema_join_compat_column_reference_matches(
            child_at(addition, 0U),
            NULL,
            "DATA_LENGTH"
        ) &&
        information_schema_join_compat_column_reference_matches(
            child_at(addition, 1U),
            NULL,
            "INDEX_LENGTH"
        ) &&
        information_schema_join_compat_alias_matches(child_at(third_item, 1U), "bytes") &&
        information_schema_join_compat_source_table_matches(
            child_at(statement, 1U),
            "information_schema",
            "tables",
            NULL
        ) &&
        information_schema_join_compat_group_matches(statement, NULL, "TABLE_NAME") &&
        information_schema_join_compat_order_matches(statement, order, 1U) &&
        !information_schema_join_compat_has_direct_child_kind(
            statement,
            MYLITE_SQL_AST_HAVING_CLAUSE
        ) &&
        !information_schema_join_compat_has_direct_child_kind(
            statement,
            MYLITE_SQL_AST_LIMIT_CLAUSE
        );
    return MYLITE_OK;
}

static bool information_schema_join_compat_select_projections_match(
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_join_compat_projection *expected,
    size_t expected_count
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *item = child_at(select_list, 0U);

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT ||
        select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        mylite_sql_ast_node_child_count(select_list) != expected_count) {
        return false;
    }
    for (size_t index = 0U; index < expected_count; ++index) {
        const struct mylite_sql_ast_node *alias = child_at(item, 1U);

        if (item == NULL || item->kind != MYLITE_SQL_AST_SELECT_ITEM ||
            !information_schema_join_compat_column_reference_matches(
                child_at(item, 0U),
                expected[index].qualifier,
                expected[index].name
            ) ||
            (expected[index].alias == NULL && alias != NULL) ||
            (expected[index].alias != NULL &&
             !information_schema_join_compat_identifier_parts_match(
                 alias,
                 &expected[index].alias,
                 1U
             ))) {
            return false;
        }
        item = item->next_sibling;
    }
    return item == NULL;
}

static bool information_schema_join_compat_source_table_matches(
    const struct mylite_sql_ast_node *source,
    const char *schema_name,
    const char *table_name,
    const char *alias
) {
    const struct mylite_sql_ast_node *name = child_at(source, 0U);
    const struct mylite_sql_ast_node *source_alias = NULL;
    const char *const qualified_parts[] = {schema_name, table_name};
    const char *const unqualified_parts[] = {table_name};

    if (source == NULL || source->kind != MYLITE_SQL_AST_FROM_TABLE || table_name == NULL) {
        return false;
    }
    source_alias = from_table_alias_node(source);
    if (from_table_index_hint_list_node(source) != NULL) {
        return false;
    }
    if ((alias == NULL && source_alias != NULL) ||
        (alias != NULL &&
         !information_schema_join_compat_identifier_parts_match(source_alias, &alias, 1U))) {
        return false;
    }
    return schema_name == NULL
               ? information_schema_join_compat_identifier_parts_match(name, unqualified_parts, 1U)
               : information_schema_join_compat_identifier_parts_match(name, qualified_parts, 2U);
}

static bool information_schema_join_compat_join_matches(
    const struct mylite_sql_ast_node *source,
    enum mylite_sql_ast_join_kind join_kind,
    const char *left_schema,
    const char *left_table,
    const char *left_alias,
    const char *right_schema,
    const char *right_table,
    const char *right_alias,
    const struct information_schema_join_compat_column_equality *equalities,
    size_t equality_count
) {
    return source != NULL && source->kind == MYLITE_SQL_AST_FROM_JOIN &&
           mylite_sql_ast_node_join_kind(source) == join_kind &&
           information_schema_join_compat_source_table_matches(
               child_at(source, 0U),
               left_schema,
               left_table,
               left_alias
           ) &&
           information_schema_join_compat_source_table_matches(
               child_at(source, 1U),
               right_schema,
               right_table,
               right_alias
           ) &&
           information_schema_join_compat_column_equalities_match(
               child_at(source, 2U),
               equalities,
               equality_count
           );
}

static bool information_schema_join_compat_column_equalities_match(
    const struct mylite_sql_ast_node *predicate,
    const struct information_schema_join_compat_column_equality *expected,
    size_t expected_count
) {
    const struct mylite_sql_ast_node *stack[8];
    bool matched[8] = {false};
    size_t stack_count = 0U;
    size_t leaf_count = 0U;

    if (predicate == NULL || expected == NULL || expected_count == 0U ||
        expected_count > sizeof(matched) / sizeof(matched[0])) {
        return false;
    }
    stack[stack_count++] = predicate;
    while (stack_count != 0U) {
        const struct mylite_sql_ast_node *current = stack[--stack_count];

        if (current != NULL && current->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
            current = child_at(current, 0U);
        }
        if (current != NULL && current->kind == MYLITE_SQL_AST_AND_PREDICATE) {
            if (stack_count + 2U > sizeof(stack) / sizeof(stack[0])) {
                return false;
            }
            stack[stack_count++] = child_at(current, 1U);
            stack[stack_count++] = child_at(current, 0U);
            continue;
        }
        if (current == NULL || current->kind != MYLITE_SQL_AST_COMPARISON_PREDICATE ||
            mylite_sql_ast_node_operator(current) != MYLITE_SQL_AST_OPERATOR_EQUAL) {
            return false;
        }
        ++leaf_count;
        for (size_t index = 0U; index < expected_count; ++index) {
            bool forward = information_schema_join_compat_column_reference_matches(
                               child_at(current, 0U),
                               expected[index].left.qualifier,
                               expected[index].left.name
                           ) &&
                           information_schema_join_compat_column_reference_matches(
                               child_at(current, 1U),
                               expected[index].right.qualifier,
                               expected[index].right.name
                           );
            bool reverse = information_schema_join_compat_column_reference_matches(
                               child_at(current, 1U),
                               expected[index].left.qualifier,
                               expected[index].left.name
                           ) &&
                           information_schema_join_compat_column_reference_matches(
                               child_at(current, 0U),
                               expected[index].right.qualifier,
                               expected[index].right.name
                           );

            if ((forward || reverse) && !matched[index]) {
                matched[index] = true;
                break;
            }
        }
    }
    if (leaf_count != expected_count) {
        return false;
    }
    for (size_t index = 0U; index < expected_count; ++index) {
        if (!matched[index]) {
            return false;
        }
    }
    return true;
}

static int information_schema_join_compat_copy_literal_filters(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    struct information_schema_join_compat_literal_filter *filters,
    size_t filter_count,
    bool *out_matches
) {
    const struct mylite_sql_ast_node *stack[8];
    bool matched[8] = {false};
    size_t stack_count = 0U;
    size_t leaf_count = 0U;
    int rc = MYLITE_OK;

    if (out_matches == NULL || filters == NULL || filter_count == 0U ||
        filter_count > sizeof(matched) / sizeof(matched[0])) {
        return MYLITE_MISUSE;
    }
    *out_matches = false;
    if (predicate == NULL) {
        return MYLITE_OK;
    }
    stack[stack_count++] = predicate;
    while (rc == MYLITE_OK && stack_count != 0U) {
        const struct mylite_sql_ast_node *current = stack[--stack_count];
        const struct mylite_sql_ast_node *column = NULL;
        const struct mylite_sql_ast_node *literal = NULL;
        size_t match_index = filter_count;

        if (current != NULL && current->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
            current = child_at(current, 0U);
        }
        if (current != NULL && current->kind == MYLITE_SQL_AST_AND_PREDICATE) {
            if (stack_count + 2U > sizeof(stack) / sizeof(stack[0])) {
                break;
            }
            stack[stack_count++] = child_at(current, 1U);
            stack[stack_count++] = child_at(current, 0U);
            continue;
        }
        if (current == NULL || current->kind != MYLITE_SQL_AST_COMPARISON_PREDICATE ||
            mylite_sql_ast_node_operator(current) != MYLITE_SQL_AST_OPERATOR_EQUAL) {
            break;
        }
        column = child_at(current, 0U);
        literal = child_at(current, 1U);
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
            column = child_at(current, 1U);
            literal = child_at(current, 0U);
        }
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_STRING) {
            break;
        }
        for (size_t index = 0U; index < filter_count; ++index) {
            if (!matched[index] && information_schema_join_compat_column_reference_matches(
                                       column,
                                       filters[index].qualifier,
                                       filters[index].name
                                   )) {
                match_index = index;
                break;
            }
        }
        if (match_index == filter_count) {
            break;
        }
        size_t value_length = 0U;

        rc = decode_sql_string_literal(
            database,
            literal,
            "INFORMATION_SCHEMA compatibility filters require string literals",
            "INFORMATION_SCHEMA compatibility filters do not support NUL bytes",
            filters[match_index].out_value,
            &value_length
        );
        if (rc == MYLITE_OK) {
            matched[match_index] = true;
            ++leaf_count;
        }
    }
    if (rc == MYLITE_OK && stack_count == 0U && leaf_count == filter_count) {
        *out_matches = true;
    }
    if (!*out_matches) {
        for (size_t index = 0U; index < filter_count; ++index) {
            if (filters[index].out_value != NULL) {
                free(*filters[index].out_value);
                *filters[index].out_value = NULL;
            }
        }
    }
    return rc;
}

static bool information_schema_join_compat_order_matches(
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_join_compat_column_reference *expected,
    size_t expected_count
) {
    const struct mylite_sql_ast_node *clause =
        information_schema_join_compat_direct_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    const struct mylite_sql_ast_node *first = child_at(clause, 0U);

    if (clause == NULL || expected == NULL || expected_count == 0U) {
        return false;
    }
    if (expected_count == 1U && first != NULL && first->kind != MYLITE_SQL_AST_ORDER_BY_ITEM_LIST) {
        const struct mylite_sql_ast_node *direction = child_at(clause, 1U);

        return child_at(clause, 2U) == NULL &&
               information_schema_join_compat_column_reference_matches(
                   first,
                   expected[0].qualifier,
                   expected[0].name
               ) &&
               (direction == NULL || mylite_sql_ast_node_order_direction(direction) !=
                                         MYLITE_SQL_AST_ORDER_DIRECTION_DESC);
    }
    if (first == NULL || first->kind != MYLITE_SQL_AST_ORDER_BY_ITEM_LIST ||
        mylite_sql_ast_node_child_count(first) != expected_count) {
        return false;
    }
    const struct mylite_sql_ast_node *item = child_at(first, 0U);

    for (size_t index = 0U; index < expected_count; ++index) {
        const struct mylite_sql_ast_node *direction = child_at(item, 1U);

        if (item == NULL || item->kind != MYLITE_SQL_AST_ORDER_BY_ITEM ||
            !information_schema_join_compat_column_reference_matches(
                child_at(item, 0U),
                expected[index].qualifier,
                expected[index].name
            ) ||
            (direction != NULL && mylite_sql_ast_node_order_direction(direction) ==
                                      MYLITE_SQL_AST_ORDER_DIRECTION_DESC)) {
            return false;
        }
        item = item->next_sibling;
    }
    return item == NULL;
}

static bool information_schema_join_compat_group_matches(
    const struct mylite_sql_ast_node *statement,
    const char *qualifier,
    const char *name
) {
    const struct mylite_sql_ast_node *clause =
        information_schema_join_compat_direct_child_kind(statement, MYLITE_SQL_AST_GROUP_BY_CLAUSE);
    const struct mylite_sql_ast_node *key = child_at(clause, 0U);

    if (key != NULL && key->kind == MYLITE_SQL_AST_IDENTIFIER_LIST) {
        if (mylite_sql_ast_node_child_count(key) != 1U) {
            return false;
        }
        key = child_at(key, 0U);
    }
    return information_schema_join_compat_column_reference_matches(key, qualifier, name);
}

static const struct mylite_sql_ast_node *information_schema_join_compat_direct_child_kind(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
) {
    const struct mylite_sql_ast_node *child = node == NULL ? NULL : node->first_child;

    while (child != NULL) {
        if (child->kind == kind) {
            return child;
        }
        child = child->next_sibling;
    }
    return NULL;
}

static bool information_schema_join_compat_has_direct_child_kind(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
) {
    return information_schema_join_compat_direct_child_kind(node, kind) != NULL;
}

static bool information_schema_join_compat_column_reference_matches(
    const struct mylite_sql_ast_node *node,
    const char *qualifier,
    const char *name
) {
    if (name == NULL) {
        return false;
    }
    if (qualifier == NULL) {
        return information_schema_join_compat_identifier_parts_match(node, &name, 1U);
    }
    return information_schema_join_compat_identifier_parts_match(
        node,
        (const char *const[]){qualifier, name},
        2U
    );
}

static bool information_schema_join_compat_identifier_parts_match(
    const struct mylite_sql_ast_node *node,
    const char *const *expected,
    size_t expected_count
) {
    const struct mylite_sql_ast_node *tails[4];
    const struct mylite_sql_ast_node *current = node;
    size_t tail_count = 0U;

    if (node == NULL || expected == NULL || expected_count == 0U || expected_count > 4U) {
        return false;
    }
    while (current != NULL && current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        if (tail_count == sizeof(tails) / sizeof(tails[0])) {
            return false;
        }
        tails[tail_count++] = child_at(current, 1U);
        current = child_at(current, 0U);
    }
    if (current == NULL || current->kind != MYLITE_SQL_AST_IDENTIFIER ||
        tail_count + 1U != expected_count ||
        !information_schema_join_compat_identifier_span_matches(&current->span, expected[0])) {
        return false;
    }
    for (size_t index = 0U; index < tail_count; ++index) {
        const struct mylite_sql_ast_node *part = tails[tail_count - index - 1U];

        if (part == NULL || part->kind != MYLITE_SQL_AST_IDENTIFIER ||
            !information_schema_join_compat_identifier_span_matches(
                &part->span,
                expected[index + 1U]
            )) {
            return false;
        }
    }
    return true;
}

static bool information_schema_join_compat_identifier_span_matches(
    const struct mylite_sql_source_span *span,
    const char *expected
) {
    size_t source_index = 0U;
    size_t expected_index = 0U;
    char quote = '\0';

    if (span == NULL || span->text == NULL || expected == NULL || span->length == 0U) {
        return false;
    }
    if ((span->text[0] == '`' || span->text[0] == '"' || span->text[0] == '\'') &&
        span->length >= 2U && span->text[span->length - 1U] == span->text[0]) {
        quote = span->text[0];
        source_index = 1U;
    }
    while (source_index < span->length - (quote == '\0' ? 0U : 1U)) {
        char byte = span->text[source_index];

        if (quote != '\0' && byte == quote && source_index + 1U < span->length - 1U &&
            span->text[source_index + 1U] == quote) {
            ++source_index;
        }
        if (expected[expected_index] == '\0' ||
            !information_schema_join_compat_ascii_equal(byte, expected[expected_index])) {
            return false;
        }
        ++source_index;
        ++expected_index;
    }
    return expected[expected_index] == '\0';
}

static bool information_schema_join_compat_alias_matches(
    const struct mylite_sql_ast_node *node,
    const char *expected
) {
    return node != NULL &&
           (node->kind == MYLITE_SQL_AST_IDENTIFIER ||
            (node->kind == MYLITE_SQL_AST_LITERAL &&
             mylite_sql_ast_node_literal_kind(node) == MYLITE_SQL_AST_LITERAL_STRING)) &&
           information_schema_join_compat_identifier_span_matches(&node->span, expected);
}

static bool information_schema_join_compat_ascii_equal(char left, char right) {
    if (left >= 'a' && left <= 'z') {
        left = (char)(left - ('a' - 'A'));
    }
    if (right >= 'a' && right <= 'z') {
        right = (char)(right - ('a' - 'A'));
    }
    return left == right;
}

static bool information_schema_join_compat_literal_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    const char *expected
) {
    (void)database;
    if (node == NULL || node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(node) != MYLITE_SQL_AST_LITERAL_STRING) {
        return false;
    }
    return information_schema_join_compat_identifier_span_matches(&node->span, expected);
}

static bool information_schema_join_compat_concat_projection_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const char *argument,
    const char *suffix,
    const char *alias,
    const char *source
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *item = child_at(select_list, 0U);
    const struct mylite_sql_ast_node *function = child_at(item, 0U);
    const struct mylite_sql_ast_node *arguments = child_at(function, 0U);

    return statement != NULL && statement->kind == MYLITE_SQL_AST_SELECT_STATEMENT &&
           mylite_sql_ast_node_child_count(select_list) == 1U && function != NULL &&
           function->kind == MYLITE_SQL_AST_CONCAT_FUNCTION && arguments != NULL &&
           arguments->kind == MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST &&
           mylite_sql_ast_node_child_count(arguments) == 2U &&
           information_schema_join_compat_column_reference_matches(
               child_at(arguments, 0U),
               NULL,
               argument
           ) &&
           information_schema_join_compat_literal_matches(
               database,
               child_at(arguments, 1U),
               suffix
           ) &&
           information_schema_join_compat_identifier_parts_match(child_at(item, 1U), &alias, 1U) &&
           information_schema_join_compat_source_table_matches(
               child_at(statement, 1U),
               NULL,
               source,
               NULL
           );
}

static int information_schema_join_compat_plan_dynamic_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool *out_matches
) {
    static const struct information_schema_join_compat_projection projections[] = {
        {"sub", "id", NULL},
        {"sub", "table_schema", NULL},
        {"sub", "table_name", NULL},
        {"sub", "column_name", NULL},
    };
    static const struct information_schema_join_compat_column_reference order[] = {
        {NULL, "ordinal_position"},
    };
    static const struct information_schema_join_compat_column_equality schema_join[] = {
        {{"s", "schema_name"}, {"c", "table_schema"}},
    };
    const struct mylite_sql_ast_node *derived = child_at(statement, 1U);
    const struct mylite_sql_ast_node *inner = child_at(derived, 0U);
    const struct mylite_sql_ast_node *outer_join = child_at(inner, 1U);
    const struct mylite_sql_ast_node *columns_table_join = child_at(outer_join, 0U);
    const struct mylite_sql_ast_node *dynamic_condition = child_at(columns_table_join, 2U);
    const struct mylite_sql_ast_node *dynamic_right = child_at(dynamic_condition, 1U);
    const struct mylite_sql_ast_node *concat_arguments = child_at(dynamic_right, 0U);
    const struct mylite_sql_ast_node *coalesce = child_at(concat_arguments, 0U);
    const struct mylite_sql_ast_node *coalesce_arguments = child_at(coalesce, 0U);
    char *table_name = NULL;
    bool filters_match = false;
    int rc = MYLITE_OK;

    if (out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matches = false;
    if (!information_schema_join_compat_select_projections_match(
            statement,
            projections,
            sizeof(projections) / sizeof(projections[0])
        ) ||
        derived == NULL || derived->kind != MYLITE_SQL_AST_FROM_DERIVED ||
        !information_schema_join_compat_identifier_parts_match(
            child_at(derived, 1U),
            (const char *const[]){"sub"},
            1U
        ) ||
        inner == NULL || inner->kind != MYLITE_SQL_AST_SELECT_STATEMENT ||
        mylite_sql_ast_node_child_count(child_at(inner, 0U)) != 1U ||
        child_at(child_at(child_at(inner, 0U), 0U), 0U) == NULL ||
        child_at(child_at(child_at(inner, 0U), 0U), 0U)->kind != MYLITE_SQL_AST_WILDCARD ||
        outer_join == NULL || outer_join->kind != MYLITE_SQL_AST_FROM_JOIN ||
        mylite_sql_ast_node_join_kind(outer_join) != MYLITE_SQL_AST_JOIN_KIND_INNER ||
        !information_schema_join_compat_source_table_matches(
            child_at(outer_join, 1U),
            "information_schema",
            "schemata",
            "s"
        ) ||
        !information_schema_join_compat_column_equalities_match(
            child_at(outer_join, 2U),
            schema_join,
            1U
        ) ||
        columns_table_join == NULL || columns_table_join->kind != MYLITE_SQL_AST_FROM_JOIN ||
        mylite_sql_ast_node_join_kind(columns_table_join) != MYLITE_SQL_AST_JOIN_KIND_INNER ||
        !information_schema_join_compat_source_table_matches(
            child_at(columns_table_join, 0U),
            "information_schema",
            "columns",
            "c"
        ) ||
        !information_schema_join_compat_source_table_matches(
            child_at(columns_table_join, 1U),
            NULL,
            "t",
            NULL
        ) ||
        dynamic_condition == NULL ||
        dynamic_condition->kind != MYLITE_SQL_AST_COMPARISON_PREDICATE ||
        mylite_sql_ast_node_operator(dynamic_condition) != MYLITE_SQL_AST_OPERATOR_EQUAL ||
        !information_schema_join_compat_column_reference_matches(
            child_at(dynamic_condition, 0U),
            "t",
            "db_name"
        ) ||
        dynamic_right == NULL || dynamic_right->kind != MYLITE_SQL_AST_CONCAT_FUNCTION ||
        mylite_sql_ast_node_child_count(concat_arguments) != 2U || coalesce == NULL ||
        coalesce->kind != MYLITE_SQL_AST_COALESCE_FUNCTION ||
        mylite_sql_ast_node_child_count(coalesce_arguments) != 2U ||
        !information_schema_join_compat_column_reference_matches(
            child_at(coalesce_arguments, 0U),
            "c",
            "table_schema"
        ) ||
        !information_schema_join_compat_literal_matches(
            database,
            child_at(coalesce_arguments, 1U),
            "default"
        ) ||
        !information_schema_join_compat_literal_matches(
            database,
            child_at(concat_arguments, 1U),
            ""
        ) ||
        !information_schema_join_compat_order_matches(statement, order, 1U) ||
        information_schema_join_compat_has_direct_child_kind(
            statement,
            MYLITE_SQL_AST_WHERE_CLAUSE
        ) ||
        information_schema_join_compat_has_direct_child_kind(
            statement,
            MYLITE_SQL_AST_GROUP_BY_CLAUSE
        ) ||
        information_schema_join_compat_has_direct_child_kind(
            statement,
            MYLITE_SQL_AST_HAVING_CLAUSE
        ) ||
        information_schema_join_compat_has_direct_child_kind(
            statement,
            MYLITE_SQL_AST_LIMIT_CLAUSE
        ) ||
        information_schema_join_compat_has_direct_child_kind(
            inner,
            MYLITE_SQL_AST_GROUP_BY_CLAUSE
        ) ||
        information_schema_join_compat_has_direct_child_kind(inner, MYLITE_SQL_AST_HAVING_CLAUSE) ||
        information_schema_join_compat_has_direct_child_kind(inner, MYLITE_SQL_AST_LIMIT_CLAUSE)) {
        return MYLITE_OK;
    }
    rc = information_schema_join_compat_copy_literal_filters(
        database,
        child_at(
            information_schema_join_compat_direct_child_kind(inner, MYLITE_SQL_AST_WHERE_CLAUSE),
            0U
        ),
        (struct information_schema_join_compat_literal_filter[]){
            {.qualifier = "c", .name = "table_name", .out_value = &table_name},
        },
        1U,
        &filters_match
    );
    if (rc == MYLITE_OK && filters_match && text_equals_ascii_case_insensitive(table_name, "t")) {
        *out_matches = true;
    }
    free(table_name);
    return rc;
}

static int information_schema_join_compat_plan_schemata_tables(
    const struct mylite_sql_ast_node *statement,
    bool *out_matches
) {
    static const struct information_schema_join_compat_column_equality equalities[] = {
        {{"t", "table_schema"}, {"s", "schema_name"}},
    };
    static const struct information_schema_join_compat_column_reference order[] = {
        {"s", "schema_name"},
    };
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *item = child_at(select_list, 0U);
    const struct mylite_sql_ast_node *wildcard = child_at(item, 0U);

    if (out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matches = mylite_sql_ast_node_child_count(select_list) == 1U && wildcard != NULL &&
                   wildcard->kind == MYLITE_SQL_AST_QUALIFIED_WILDCARD &&
                   information_schema_join_compat_identifier_parts_match(
                       child_at(wildcard, 0U),
                       (const char *const[]){"s"},
                       1U
                   ) &&
                   information_schema_join_compat_join_matches(
                       child_at(statement, 1U),
                       MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER,
                       "information_schema",
                       "schemata",
                       "s",
                       "information_schema",
                       "tables",
                       "t",
                       equalities,
                       1U
                   ) &&
                   information_schema_join_compat_order_matches(statement, order, 1U) &&
                   !information_schema_join_compat_has_direct_child_kind(
                       statement,
                       MYLITE_SQL_AST_WHERE_CLAUSE
                   ) &&
                   !information_schema_join_compat_has_direct_child_kind(
                       statement,
                       MYLITE_SQL_AST_GROUP_BY_CLAUSE
                   ) &&
                   !information_schema_join_compat_has_direct_child_kind(
                       statement,
                       MYLITE_SQL_AST_HAVING_CLAUSE
                   ) &&
                   !information_schema_join_compat_has_direct_child_kind(
                       statement,
                       MYLITE_SQL_AST_LIMIT_CLAUSE
                   );
    return MYLITE_OK;
}
