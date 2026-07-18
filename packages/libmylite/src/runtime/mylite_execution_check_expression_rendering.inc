
static int render_check_constraint_expression(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct planned_column *inline_column,
    size_t inline_column_index,
    const struct mylite_sql_ast_node *expression,
    char *check_clause,
    size_t check_clause_size,
    char *sqlite_expression,
    size_t sqlite_expression_size
) {
    struct mylite_dynamic_string check_string;
    struct mylite_dynamic_string sqlite_string;
    struct check_expression_render_context context = {
        .database = database,
        .plan = plan,
        .inline_column = inline_column,
        .inline_column_index = inline_column_index,
        .check_clause = &check_string,
        .sqlite_expression = &sqlite_string,
    };
    bool is_boolean = false;
    int rc = MYLITE_OK;

    mylite_dynamic_string_init(&check_string);
    mylite_dynamic_string_init(&sqlite_string);
    rc = mylite_dynamic_string_reserve(&check_string, 1U);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_reserve(&sqlite_string, 1U);
    }
    if (rc == MYLITE_OK) {
        rc = render_check_expression_node(&context, expression, true, &is_boolean);
    }
    if (rc == MYLITE_OK && !is_boolean) {
        set_check_constraint_non_boolean_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && (check_string.length >= check_clause_size ||
                            sqlite_string.length >= sqlite_expression_size)) {
        set_unsupported_error(database, "CHECK constraint expression is too long");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        memcpy(check_clause, check_string.text, check_string.length + 1U);
        memcpy(sqlite_expression, sqlite_string.text, sqlite_string.length + 1U);
    }

    mylite_dynamic_string_deinit(&check_string);
    mylite_dynamic_string_deinit(&sqlite_string);
    return rc;
}

static int render_check_expression_node(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    bool require_boolean,
    bool *out_is_boolean
) {
    struct check_expression_render_work_item *items = NULL;
    size_t item_count = 0U;
    bool root_seen = false;
    int rc = MYLITE_OK;

    *out_is_boolean = false;

    rc = append_check_expression_render_node(
        context->database,
        &items,
        &item_count,
        node,
        require_boolean
    );
    while (rc == MYLITE_OK && item_count != 0U) {
        bool item_is_boolean = false;
        struct check_expression_render_work_item item = items[--item_count];

        rc =
            render_check_expression_work_item(context, item, &items, &item_count, &item_is_boolean);
        if (rc == MYLITE_OK && !root_seen && item.kind == CHECK_EXPRESSION_RENDER_NODE) {
            *out_is_boolean = item_is_boolean;
            root_seen = true;
        }
    }
    free(items);
    return rc;
}

static int render_check_expression_work_item(
    struct check_expression_render_context *context,
    struct check_expression_render_work_item item,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    bool *out_is_boolean
) {
    const struct mylite_sql_ast_node *node = unwrap_parenthesized_expression(item.node);
    int rc = MYLITE_OK;

    *out_is_boolean = false;
    switch (item.kind) {
    case CHECK_EXPRESSION_RENDER_TEXT:
        rc = mylite_dynamic_string_append(context->check_clause, item.text);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(context->sqlite_expression, item.text);
        }
        return rc;
    case CHECK_EXPRESSION_RENDER_CHAR:
        rc = mylite_dynamic_string_append_char(context->check_clause, item.character);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(context->sqlite_expression, item.character);
        }
        return rc;
    case CHECK_EXPRESSION_RENDER_OPERATOR:
        rc = mylite_dynamic_string_append(
            context->check_clause,
            check_expression_check_clause_operator_sql(item.operator_kind)
        );
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(
                context->sqlite_expression,
                check_expression_sqlite_operator_sql(item.operator_kind)
            );
        }
        return rc;
    case CHECK_EXPRESSION_RENDER_NODE:
        break;
    }

    if (node == NULL) {
        set_parse_error(context->database, NULL);
        return MYLITE_ERROR;
    }

    switch (node->kind) {
    case MYLITE_SQL_AST_IDENTIFIER:
        return render_check_expression_column(context, node);
    case MYLITE_SQL_AST_LITERAL:
        return render_check_expression_literal(context, node, item.require_boolean, out_is_boolean);
    case MYLITE_SQL_AST_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_JSON_VALID_FUNCTION:
        return render_check_expression_function(context, node, out_is_boolean);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return render_check_expression_unary(context, node, items, item_count, out_is_boolean);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return render_check_expression_binary(context, node, items, item_count, out_is_boolean);
    case MYLITE_SQL_AST_SCALAR_SUBQUERY:
        set_check_constraint_subquery_error(context->database);
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
        set_check_constraint_variable_error(context->database);
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_RAND_FUNCTION:
    case MYLITE_SQL_AST_RAND_SEED_FUNCTION:
    case MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR:
        set_check_constraint_function_error(context->database);
        return MYLITE_ERROR;
    default:
        break;
    }

    if (is_scalar_function_projection_expression(node)) {
        set_check_constraint_function_error(context->database);
        return MYLITE_ERROR;
    }

    set_unsupported_error(
        context->database,
        "CHECK constraints support only descriptor columns, literals, deterministic baseline "
        "functions, boolean comparisons, IS NULL, IS NOT NULL, AND, OR, and NOT"
    );
    return MYLITE_ERROR;
}

static int render_check_expression_column(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node
) {
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_index = 0U;
    int rc = copy_identifier_text(node, column_name, sizeof(column_name), context->database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = find_planned_column_index(
        context->plan->columns,
        context->plan->column_count,
        column_name,
        &column_index
    );
    if (rc != MYLITE_OK) {
        if (context->alter_check_constraint_name != NULL) {
            set_alter_check_constraint_unknown_column_error(
                context->database,
                column_name,
                context->alter_check_constraint_name
            );
        } else {
            set_check_constraint_unknown_column_error(context->database, column_name);
        }
        return MYLITE_ERROR;
    }
    if (context->inline_column != NULL && column_index != context->inline_column_index) {
        set_check_constraint_column_ref_error(context->database, "", column_name);
        return MYLITE_ERROR;
    }
    if (context->plan->columns[column_index].is_auto_increment) {
        set_check_constraint_auto_increment_error(context->database);
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append_mysql_quoted_identifier(
        context->check_clause,
        context->plan->columns[column_index].name
    );
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            context->sqlite_expression,
            context->plan->columns[column_index].name
        );
    }
    return rc;
}

static int render_check_expression_literal(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    bool require_boolean,
    bool *out_is_boolean
) {
    enum mylite_sql_ast_literal_kind literal_kind = mylite_sql_ast_node_literal_kind(node);

    *out_is_boolean = false;
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_is_boolean = true;
        if (mylite_dynamic_string_append(context->check_clause, "TRUE") != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        return mylite_dynamic_string_append(context->sqlite_expression, "1");
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_is_boolean = true;
        if (mylite_dynamic_string_append(context->check_clause, "FALSE") != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        return mylite_dynamic_string_append(context->sqlite_expression, "0");
    }
    if (require_boolean) {
        set_check_constraint_non_boolean_error(context->database);
        return MYLITE_ERROR;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
        literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FLOAT) {
        if (append_check_expression_span_text(context->database, context->check_clause, node) !=
            MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        return append_check_expression_span_text(
            context->database,
            context->sqlite_expression,
            node
        );
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        if (mylite_dynamic_string_append(context->check_clause, "NULL") != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        return mylite_dynamic_string_append(context->sqlite_expression, "NULL");
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        char *text = NULL;
        size_t text_length = 0U;
        int rc = decode_sql_string_literal(
            context->database,
            node,
            "CHECK constraints support only SQL string literals",
            "CHECK constraint string literals cannot contain NUL bytes",
            &text,
            &text_length
        );

        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(context->check_clause, "_utf8mb4");
        }
        if (rc == MYLITE_OK) {
            rc = append_check_expression_mysql_string_literal(
                context->check_clause,
                text,
                text_length
            );
        }
        if (rc == MYLITE_OK) {
            rc = append_check_expression_sqlite_string_literal(
                context->sqlite_expression,
                text,
                text_length
            );
        }
        free(text);
        return rc;
    }

    set_unsupported_error(
        context->database,
        "CHECK constraints support only numeric, string, NULL, and boolean literals"
    );
    return MYLITE_ERROR;
}

static int render_check_expression_function(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    bool *out_is_boolean
) {
    if (mylite_sql_ast_node_child_count(node) != 1U) {
        set_check_constraint_function_error(context->database);
        return MYLITE_ERROR;
    }
    if (node->kind == MYLITE_SQL_AST_LENGTH_FUNCTION) {
        return render_check_expression_identifier_argument_function(
            context,
            child_at(node, 0U),
            "length",
            "length",
            false,
            out_is_boolean
        );
    }
    if (node->kind == MYLITE_SQL_AST_JSON_VALID_FUNCTION) {
        return render_check_expression_identifier_argument_function(
            context,
            child_at(node, 0U),
            "json_valid",
            "_mylite_json_valid",
            true,
            out_is_boolean
        );
    }

    set_check_constraint_function_error(context->database);
    return MYLITE_ERROR;
}

static int render_check_expression_identifier_argument_function(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *argument,
    const char *check_clause_function,
    const char *sqlite_function,
    bool is_boolean,
    bool *out_is_boolean
) {
    const struct mylite_sql_ast_node *unwrapped = unwrap_parenthesized_expression(argument);
    int rc = MYLITE_OK;

    *out_is_boolean = false;
    if (unwrapped == NULL || unwrapped->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_check_constraint_function_error(context->database);
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(context->check_clause, check_clause_function);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(context->check_clause, '(');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(context->sqlite_expression, sqlite_function);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(context->sqlite_expression, '(');
    }
    if (rc == MYLITE_OK) {
        rc = render_check_expression_column(context, unwrapped);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(context->check_clause, ')');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(context->sqlite_expression, ')');
    }
    if (rc == MYLITE_OK) {
        *out_is_boolean = is_boolean;
    }
    return rc;
}

static int append_check_expression_mysql_string_literal(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_length
) {
    int rc = mylite_dynamic_string_append_char(string, '\'');

    for (size_t index = 0U; rc == MYLITE_OK && index < text_length; ++index) {
        switch (text[index]) {
        case '\'':
            rc = mylite_dynamic_string_append(string, "''");
            break;
        case '\\':
            rc = mylite_dynamic_string_append(string, "\\\\");
            break;
        case '\n':
            rc = mylite_dynamic_string_append(string, "\\n");
            break;
        case '\r':
            rc = mylite_dynamic_string_append(string, "\\r");
            break;
        case '\t':
            rc = mylite_dynamic_string_append(string, "\\t");
            break;
        default:
            rc = mylite_dynamic_string_append_char(string, text[index]);
            break;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\'');
    }
    return rc;
}

static int append_check_expression_sqlite_string_literal(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_length
) {
    int rc = mylite_dynamic_string_append_char(string, '\'');

    for (size_t index = 0U; rc == MYLITE_OK && index < text_length; ++index) {
        if (text[index] == '\'') {
            rc = mylite_dynamic_string_append(string, "''");
        } else {
            rc = mylite_dynamic_string_append_char(string, text[index]);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\'');
    }
    return rc;
}

static int render_check_expression_unary(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    bool *out_is_boolean
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(node);
    const char *operator_sql = NULL;
    int rc = MYLITE_OK;

    *out_is_boolean = false;
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        operator_sql = "+";
        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            operator_sql = "-";
        }
        rc = append_check_expression_render_node(
            context->database,
            items,
            item_count,
            child_at(node, 0U),
            false
        );
        if (rc == MYLITE_OK) {
            rc = append_check_expression_render_text(
                context->database,
                items,
                item_count,
                operator_sql
            );
        }
        return rc;
    }
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT) {
        set_unsupported_error(
            context->database,
            "CHECK constraints support only +, -, and NOT unary operators"
        );
        return MYLITE_ERROR;
    }

    rc = append_check_expression_render_char(context->database, items, item_count, ')');
    if (rc == MYLITE_OK) {
        rc = append_check_expression_render_node(
            context->database,
            items,
            item_count,
            child_at(node, 0U),
            true
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_check_expression_render_text(context->database, items, item_count, "(NOT ");
    }
    if (rc == MYLITE_OK) {
        *out_is_boolean = true;
    }
    return rc;
}

static int render_check_expression_binary(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    bool *out_is_boolean
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(node);
    bool require_child_boolean =
        check_expression_binary_operator_requires_boolean_children(operator_kind);
    int rc = MYLITE_OK;

    *out_is_boolean = false;
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR) {
        set_unsupported_error(context->database, "CHECK constraints do not yet support XOR");
        return MYLITE_ERROR;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_IS_TRUE ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_IS_FALSE ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN) {
        set_unsupported_error(
            context->database,
            "CHECK constraints support only IS NULL and IS NOT NULL"
        );
        return MYLITE_ERROR;
    }
    if (!check_expression_binary_operator_is_boolean(operator_kind) &&
        !check_expression_binary_operator_is_arithmetic(operator_kind)) {
        set_unsupported_error(
            context->database,
            "CHECK constraints do not support this binary operator"
        );
        return MYLITE_ERROR;
    }

    rc = append_check_expression_render_char(context->database, items, item_count, ')');
    if (rc == MYLITE_OK) {
        if (operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NULL ||
            operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL) {
            rc = append_check_expression_render_text(context->database, items, item_count, "NULL");
        } else {
            rc = append_check_expression_render_node(
                context->database,
                items,
                item_count,
                child_at(node, 1U),
                require_child_boolean
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_check_expression_render_text(context->database, items, item_count, " ");
    }
    if (rc == MYLITE_OK) {
        rc = append_check_expression_render_operator(
            context->database,
            items,
            item_count,
            operator_kind
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_check_expression_render_text(context->database, items, item_count, " ");
    }
    if (rc == MYLITE_OK) {
        rc = append_check_expression_render_node(
            context->database,
            items,
            item_count,
            child_at(node, 0U),
            require_child_boolean
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_check_expression_render_char(context->database, items, item_count, '(');
    }
    if (rc == MYLITE_OK && check_expression_binary_operator_is_boolean(operator_kind)) {
        *out_is_boolean = true;
    }
    return rc;
}

static bool check_expression_binary_operator_is_boolean(enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR:
        return true;
    default:
        return false;
    }
}

static bool check_expression_binary_operator_is_arithmetic(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        return true;
    default:
        return false;
    }
}

static bool check_expression_binary_operator_requires_boolean_children(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR:
        return true;
    default:
        return false;
    }
}

static const char *check_expression_check_clause_operator_sql(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return "=";
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        return "<=>";
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return "<>";
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return "<";
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return "<=";
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return ">";
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return ">=";
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
        return "IS";
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
        return "IS NOT";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND:
        return "and";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR:
        return "or";
    case MYLITE_SQL_AST_OPERATOR_ADD:
        return "+";
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        return "-";
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        return "*";
    default:
        return "";
    }
}

static const char *check_expression_sqlite_operator_sql(enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return "=";
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        return "IS";
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return "<>";
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return "<";
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return "<=";
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return ">";
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return ">=";
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
        return "IS";
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
        return "IS NOT";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND:
        return "AND";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR:
        return "OR";
    case MYLITE_SQL_AST_OPERATOR_ADD:
        return "+";
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        return "-";
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        return "*";
    default:
        return "";
    }
}

static int append_check_expression_span_text(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->span.text == NULL || node->span.length == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; index < node->span.length; ++index) {
        int rc = mylite_dynamic_string_append_char(string, node->span.text[index]);

        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static int append_check_expression_render_node(
    struct mylite_db *database,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    const struct mylite_sql_ast_node *node,
    bool require_boolean
) {
    return append_check_expression_render_work_item(
        database,
        items,
        item_count,
        (struct check_expression_render_work_item){
            .kind = CHECK_EXPRESSION_RENDER_NODE,
            .node = node,
            .require_boolean = require_boolean,
        }
    );
}

static int append_check_expression_render_text(
    struct mylite_db *database,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    const char *text
) {
    return append_check_expression_render_work_item(
        database,
        items,
        item_count,
        (struct check_expression_render_work_item){
            .kind = CHECK_EXPRESSION_RENDER_TEXT,
            .text = text,
        }
    );
}

static int append_check_expression_render_char(
    struct mylite_db *database,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    char character
) {
    return append_check_expression_render_work_item(
        database,
        items,
        item_count,
        (struct check_expression_render_work_item){
            .kind = CHECK_EXPRESSION_RENDER_CHAR,
            .character = character,
        }
    );
}

static int append_check_expression_render_operator(
    struct mylite_db *database,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    enum mylite_sql_ast_operator operator_kind
) {
    return append_check_expression_render_work_item(
        database,
        items,
        item_count,
        (struct check_expression_render_work_item){
            .kind = CHECK_EXPRESSION_RENDER_OPERATOR,
            .operator_kind = operator_kind,
        }
    );
}

static int append_check_expression_render_work_item(
    struct mylite_db *database,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    struct check_expression_render_work_item item
) {
    struct check_expression_render_work_item *grown_items = NULL;
    size_t required_count = *item_count + 1U;

    if (required_count > SIZE_MAX / sizeof(*grown_items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    grown_items = realloc(*items, required_count * sizeof(*grown_items));
    if (grown_items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    *items = grown_items;
    (*items)[*item_count] = item;
    *item_count = required_count;

    return MYLITE_OK;
}
