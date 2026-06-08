enum explain_statement_format {
    EXPLAIN_STATEMENT_FORMAT_TRADITIONAL = 0,
    EXPLAIN_STATEMENT_FORMAT_JSON = 1,
    EXPLAIN_STATEMENT_FORMAT_TREE = 2,
};

static int plan_explain_statement_format(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *format_node,
    bool analyze,
    enum explain_statement_format *out_format
);
static const struct mylite_sql_ast_node *explain_statement_child_kind(
    const struct mylite_sql_ast_node *statement,
    enum mylite_sql_ast_node_kind kind
);
static int append_explain_traditional_result(
    struct mylite_db *database,
    bool analyze,
    mylite_result *result
);
static int append_explain_single_column_result(
    struct mylite_db *database,
    enum explain_statement_format format,
    bool analyze,
    mylite_result *result
);

static int execute_explain_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *format_node =
        explain_statement_child_kind(statement, MYLITE_SQL_AST_EXPLAIN_FORMAT);
    const struct mylite_sql_ast_node *analyze_node =
        explain_statement_child_kind(statement, MYLITE_SQL_AST_EXPLAIN_ANALYZE);
    enum explain_statement_format format = EXPLAIN_STATEMENT_FORMAT_TRADITIONAL;
    bool analyze = analyze_node != NULL;
    mylite_result *result = NULL;
    int rc = plan_explain_statement_format(database, format_node, analyze, &format);

    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK && format == EXPLAIN_STATEMENT_FORMAT_TRADITIONAL) {
        rc = append_explain_traditional_result(database, analyze, result);
    } else if (rc == MYLITE_OK) {
        rc = append_explain_single_column_result(database, format, analyze, result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static const struct mylite_sql_ast_node *explain_statement_child_kind(
    const struct mylite_sql_ast_node *statement,
    enum mylite_sql_ast_node_kind kind
) {
    const struct mylite_sql_ast_node *child = child_at(statement, 0U);

    while (child != NULL) {
        if (child->kind == kind) {
            return child;
        }
        child = child->next_sibling;
    }
    return NULL;
}

static int plan_explain_statement_format(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *format_node,
    bool analyze,
    enum explain_statement_format *out_format
) {
    const struct mylite_sql_ast_node *name = NULL;
    char format_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    *out_format = analyze ? EXPLAIN_STATEMENT_FORMAT_TREE : EXPLAIN_STATEMENT_FORMAT_TRADITIONAL;
    if (format_node == NULL) {
        return MYLITE_OK;
    }
    name = child_at(format_node, 0U);
    if (name == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    rc = copy_identifier_text(name, format_name, sizeof(format_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (text_equals_ascii_case_insensitive(format_name, "traditional")) {
        if (analyze) {
            set_unsupported_error(database, "EXPLAIN ANALYZE supports only TREE format");
            return MYLITE_ERROR;
        }
        *out_format = EXPLAIN_STATEMENT_FORMAT_TRADITIONAL;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(format_name, "json")) {
        if (analyze) {
            set_unsupported_error(database, "EXPLAIN ANALYZE supports only TREE format");
            return MYLITE_ERROR;
        }
        *out_format = EXPLAIN_STATEMENT_FORMAT_JSON;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(format_name, "tree")) {
        *out_format = EXPLAIN_STATEMENT_FORMAT_TREE;
        return MYLITE_OK;
    }

    set_unsupported_error(database, "EXPLAIN supports FORMAT=TRADITIONAL, JSON, or TREE");
    return MYLITE_ERROR;
}

static int append_explain_traditional_result(
    struct mylite_db *database,
    bool analyze,
    mylite_result *result
) {
    static const char *const columns[] = {
        "id",
        "select_type",
        "table",
        "partitions",
        "type",
        "possible_keys",
        "key",
        "key_len",
        "ref",
        "rows",
        "filtered",
        "Extra",
    };
    const char *const row[] = {
        "1",
        "SIMPLE",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        analyze ? "MyLite EXPLAIN ANALYZE placeholder" : "MyLite EXPLAIN placeholder",
    };
    int rc = MYLITE_OK;

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < sizeof(columns) / sizeof(columns[0]);
         ++column_index) {
        rc = mylite_result_append_column(result, columns[column_index]);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, row);
    }
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}

static int append_explain_single_column_result(
    struct mylite_db *database,
    enum explain_statement_format format,
    bool analyze,
    mylite_result *result
) {
    const char *tree_text =
        analyze ? "-> MyLite EXPLAIN ANALYZE placeholder\n" : "-> MyLite EXPLAIN placeholder\n";
    const char *json_text = "{\"query_block\":{\"message\":\"MyLite EXPLAIN placeholder\"}}";
    const char *const row[] = {
        format == EXPLAIN_STATEMENT_FORMAT_JSON ? json_text : tree_text,
    };
    int rc = mylite_result_append_column(result, "EXPLAIN");

    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, row);
    }
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}
