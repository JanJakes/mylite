struct stored_procedure_target {
    struct table_name_resolution resolution;
    bool was_schema_qualified;
};

enum {
    stored_procedure_placeholder_initial_token_capacity = 32,
    stored_procedure_placeholder_min_create_tokens = 7,
};

struct stored_procedure_local_runtime_value {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    enum mylite_session_stored_procedure_local_value_kind value_kind;
    char *value;
    size_t value_size;
    bool is_null;
};

struct stored_procedure_placeholder_token_list {
    const char *sql;
    size_t sql_size;
    struct mylite_sql_token *tokens;
    size_t token_count;
    size_t token_capacity;
};

struct stored_procedure_placeholder_body {
    struct mylite_session_stored_procedure_local_declaration *local_declarations;
    size_t local_declaration_count;
    struct mylite_session_stored_procedure_local_assignment *local_assignments;
    size_t local_assignment_count;
    char *select_sql;
    size_t select_sql_size;
    char *body_sql;
    size_t body_sql_size;
};

struct stored_procedure_placeholder_target_parts {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count;
};

struct stored_procedure_placeholder_select_span {
    size_t cursor;
    size_t begin_index;
    size_t body_end_index;
};

enum sys_stored_procedure_behavior {
    SYS_STORED_PROCEDURE_EMPTY_RESULT = 0,
    SYS_STORED_PROCEDURE_SUMMARY_RESULT = 1,
    SYS_STORED_PROCEDURE_TABLE_EXISTS = 2,
    SYS_STORED_PROCEDURE_EXECUTE_PREPARED_STMT = 3,
    SYS_STORED_PROCEDURE_UNSUPPORTED_DIAGNOSTIC = 4,
};

struct sys_stored_procedure_definition {
    const char *name;
    size_t argument_count;
    enum sys_stored_procedure_behavior behavior;
    const char *const *columns;
    size_t column_count;
    const char *summary;
};

static const char *const sys_summary_result_columns[] = {"summary"};
static const char *const sys_disabled_consumers_columns[] = {"disabled_consumers"};
static const char *const sys_enabled_consumers_columns[] = {"enabled_consumers"};
static const char *const sys_disabled_instruments_columns[] = {"disabled_instruments", "timed"};
static const char *const sys_enabled_instruments_columns[] = {"enabled_instruments", "timed"};

static const struct sys_stored_procedure_definition sys_stored_procedures[] = {
    {"create_synonym_db", 2U, SYS_STORED_PROCEDURE_UNSUPPORTED_DIAGNOSTIC, NULL, 0U, NULL},
    {"diagnostics", 3U, SYS_STORED_PROCEDURE_UNSUPPORTED_DIAGNOSTIC, NULL, 0U, NULL},
    {"execute_prepared_stmt", 1U, SYS_STORED_PROCEDURE_EXECUTE_PREPARED_STMT, NULL, 0U, NULL},
    {"ps_setup_disable_background_threads",
     0U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Disabled 0 background threads"},
    {"ps_setup_disable_consumer",
     1U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Disabled 0 consumers"},
    {"ps_setup_disable_instrument",
     1U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Disabled 0 instruments"},
    {"ps_setup_disable_thread",
     1U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Disabled 0 threads"},
    {"ps_setup_enable_background_threads",
     0U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Enabled 0 background threads"},
    {"ps_setup_enable_consumer",
     1U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Enabled 0 consumers"},
    {"ps_setup_enable_instrument",
     1U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Enabled 0 instruments"},
    {"ps_setup_enable_thread",
     1U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Enabled 0 threads"},
    {"ps_setup_reload_saved",
     0U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Reloaded 0 saved Performance Schema setup rows"},
    {"ps_setup_reset_to_default",
     1U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Reset Performance Schema setup to MyLite defaults"},
    {"ps_setup_save",
     1U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Saved 0 Performance Schema setup rows"},
    {"ps_setup_show_disabled",
     2U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "No mutable Performance Schema setup rows are disabled"},
    {"ps_setup_show_disabled_consumers",
     0U,
     SYS_STORED_PROCEDURE_EMPTY_RESULT,
     sys_disabled_consumers_columns,
     sizeof(sys_disabled_consumers_columns) / sizeof(sys_disabled_consumers_columns[0]),
     NULL},
    {"ps_setup_show_disabled_instruments",
     0U,
     SYS_STORED_PROCEDURE_EMPTY_RESULT,
     sys_disabled_instruments_columns,
     sizeof(sys_disabled_instruments_columns) / sizeof(sys_disabled_instruments_columns[0]),
     NULL},
    {"ps_setup_show_enabled",
     2U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "No mutable Performance Schema setup rows are enabled"},
    {"ps_setup_show_enabled_consumers",
     0U,
     SYS_STORED_PROCEDURE_EMPTY_RESULT,
     sys_enabled_consumers_columns,
     sizeof(sys_enabled_consumers_columns) / sizeof(sys_enabled_consumers_columns[0]),
     NULL},
    {"ps_setup_show_enabled_instruments",
     0U,
     SYS_STORED_PROCEDURE_EMPTY_RESULT,
     sys_enabled_instruments_columns,
     sizeof(sys_enabled_instruments_columns) / sizeof(sys_enabled_instruments_columns[0]),
     NULL},
    {"ps_statement_avg_latency_histogram",
     0U,
     SYS_STORED_PROCEDURE_UNSUPPORTED_DIAGNOSTIC,
     NULL,
     0U,
     NULL},
    {"ps_trace_statement_digest", 5U, SYS_STORED_PROCEDURE_UNSUPPORTED_DIAGNOSTIC, NULL, 0U, NULL},
    {"ps_trace_thread", 7U, SYS_STORED_PROCEDURE_UNSUPPORTED_DIAGNOSTIC, NULL, 0U, NULL},
    {"ps_truncate_all_tables",
     1U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Truncated 0 Performance Schema tables"},
    {"statement_performance_analyzer",
     3U,
     SYS_STORED_PROCEDURE_UNSUPPORTED_DIAGNOSTIC,
     NULL,
     0U,
     NULL},
    {"table_exists", 3U, SYS_STORED_PROCEDURE_TABLE_EXISTS, NULL, 0U, NULL},
};

static int parse_stored_procedure_local_variables_placeholder(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct stored_procedure_target *out_target,
    struct stored_procedure_placeholder_body *out_body,
    bool *out_matches
);
static int lex_stored_procedure_placeholder_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct stored_procedure_placeholder_token_list *out_tokens
);
static bool append_stored_procedure_placeholder_token(
    const struct mylite_sql_token *token,
    struct stored_procedure_placeholder_token_list *tokens
);
static int parse_stored_procedure_placeholder_target(
    struct mylite_db *database,
    const struct stored_procedure_placeholder_token_list *tokens,
    struct stored_procedure_placeholder_target_parts *out_target_parts,
    size_t *out_begin_index,
    bool *out_matches
);
static int resolve_stored_procedure_placeholder_target(
    struct mylite_db *database,
    const struct stored_procedure_placeholder_target_parts *target_parts,
    struct stored_procedure_target *out_target
);
static int parse_stored_procedure_placeholder_body(
    struct mylite_db *database,
    const struct stored_procedure_placeholder_token_list *tokens,
    size_t begin_index,
    struct stored_procedure_placeholder_body *out_body
);
static int parse_stored_procedure_placeholder_declaration(
    struct mylite_db *database,
    const struct stored_procedure_placeholder_token_list *tokens,
    size_t *inout_index,
    size_t body_end_index,
    struct stored_procedure_placeholder_body *out_body
);
static int parse_stored_procedure_placeholder_assignment(
    struct mylite_db *database,
    const struct stored_procedure_placeholder_token_list *tokens,
    size_t *inout_index,
    size_t body_end_index,
    struct stored_procedure_placeholder_body *out_body
);
static int parse_stored_procedure_placeholder_select(
    struct mylite_db *database,
    const struct stored_procedure_placeholder_token_list *tokens,
    struct stored_procedure_placeholder_select_span span,
    struct stored_procedure_placeholder_body *out_body
);
static int append_stored_procedure_placeholder_declaration(
    struct mylite_db *database,
    const char *name,
    enum mylite_session_stored_procedure_local_value_kind value_kind,
    char *default_sql,
    size_t default_sql_size,
    struct stored_procedure_placeholder_body *out_body
);
static int append_stored_procedure_placeholder_assignment(
    struct mylite_db *database,
    const char *name,
    char *value_sql,
    size_t value_sql_size,
    struct stored_procedure_placeholder_body *out_body
);
static int store_stored_procedure_placeholder_body(
    struct mylite_db *database,
    const struct stored_procedure_target *target,
    struct stored_procedure_placeholder_body *body,
    mylite_result **out_result
);
static int resolve_stored_procedure_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_node,
    struct stored_procedure_target *out_target
);
static int try_resolve_sys_stored_procedure_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_node,
    struct stored_procedure_target *out_target,
    bool *out_found
);
static int execute_sys_stored_procedure_call(
    struct mylite_db *database,
    const struct stored_procedure_target *target,
    const struct mylite_sql_ast_node *arguments,
    mylite_result **out_result
);
static const struct sys_stored_procedure_definition *find_sys_stored_procedure_definition(
    const char *name
);
static size_t stored_procedure_argument_count(const struct mylite_sql_ast_node *arguments);
static const struct mylite_sql_ast_node *stored_procedure_argument_at(
    const struct mylite_sql_ast_node *arguments,
    size_t index
);
static int execute_sys_table_exists_procedure(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    mylite_result **out_result
);
static int execute_sys_execute_prepared_stmt_procedure(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    mylite_result **out_result
);
static int execute_sys_summary_procedure(
    struct mylite_db *database,
    const struct sys_stored_procedure_definition *definition,
    mylite_result **out_result
);
static int execute_sys_empty_result_procedure(
    struct mylite_db *database,
    const struct sys_stored_procedure_definition *definition,
    mylite_result **out_result
);
static int execute_sys_unsupported_procedure(
    struct mylite_db *database,
    const struct sys_stored_procedure_definition *definition
);
static int append_sys_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const char *const *columns,
    size_t column_count
);
static int copy_sys_procedure_string_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    char *buffer,
    size_t buffer_size
);
static int sys_table_exists_value(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char **out_value
);
static int finish_sys_empty_ok_result(struct mylite_db *database, mylite_result **out_result);
static int fill_builtin_stored_procedure_resolution(
    const struct mylite_execution_catalog_builtin_schema *schema,
    const char *table_name,
    struct table_name_resolution *out_resolution
);
static void set_sys_stored_procedure_argument_count_error(
    struct mylite_db *database,
    const struct stored_procedure_target *target,
    size_t expected,
    size_t actual
);
static int copy_stored_procedure_sql(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    char **out_sql,
    size_t *out_sql_size
);
static int copy_stored_procedure_sql_span(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    size_t start_offset,
    size_t end_offset,
    char **out_sql,
    size_t *out_sql_size
);
static int build_stored_procedure_body_sql_from_select(
    struct mylite_db *database,
    const char *select_sql,
    size_t select_sql_size,
    char **out_sql,
    size_t *out_sql_size
);
static bool stored_procedure_local_value_kind_for_type_token(
    const struct mylite_sql_token *token,
    enum mylite_session_stored_procedure_local_value_kind *out_kind
);
static bool stored_procedure_local_declaration_name_exists(
    const struct mylite_session_stored_procedure_local_declaration *declarations,
    size_t declaration_count,
    const char *name
);
static size_t stored_procedure_local_declaration_index_by_name(
    const struct mylite_session_stored_procedure_local_declaration *declarations,
    size_t declaration_count,
    const char *name
);
static int execute_stored_procedure_local_body(
    struct mylite_db *database,
    const struct mylite_session_stored_procedure *procedure,
    mylite_result **out_result
);
static int initialize_stored_procedure_local_runtime_values(
    struct mylite_db *database,
    const struct mylite_session_stored_procedure *procedure,
    struct stored_procedure_local_runtime_value **out_values,
    size_t *out_value_count
);
static int apply_stored_procedure_local_assignments(
    struct mylite_db *database,
    const struct mylite_session_stored_procedure *procedure,
    struct stored_procedure_local_runtime_value *values,
    size_t value_count
);
static int evaluate_stored_procedure_local_expression(
    struct mylite_db *database,
    const char *expression_sql,
    size_t expression_sql_size,
    struct stored_procedure_local_runtime_value *values,
    size_t value_count,
    struct stored_procedure_local_runtime_value *out_value
);
static int set_stored_procedure_local_runtime_value(
    struct mylite_db *database,
    struct stored_procedure_local_runtime_value *out_value,
    const char *text,
    size_t text_size,
    bool is_null
);
static int expand_stored_procedure_local_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    const struct stored_procedure_local_runtime_value *values,
    size_t value_count,
    bool preserve_names,
    char **out_sql,
    size_t *out_sql_size
);
static int append_stored_procedure_local_reference(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct stored_procedure_local_runtime_value *value
);
static int append_stored_procedure_local_literal(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct stored_procedure_local_runtime_value *value
);
static int append_stored_procedure_quoted_string_literal(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_size
);
static const struct stored_procedure_local_runtime_value *find_stored_procedure_local_runtime_value(
    const struct stored_procedure_local_runtime_value *values,
    size_t value_count,
    const char *name
);
static size_t stored_procedure_local_runtime_value_index_by_name(
    const struct stored_procedure_local_runtime_value *values,
    size_t value_count,
    const char *name
);
static int append_stored_procedure_sql_quoted_token(
    struct mylite_dynamic_string *string,
    const char *sql,
    size_t sql_size,
    size_t *inout_index,
    char quote
);
static int append_stored_procedure_sql_line_comment(
    struct mylite_dynamic_string *string,
    const char *sql,
    size_t sql_size,
    size_t *inout_index
);
static int append_stored_procedure_sql_block_comment(
    struct mylite_dynamic_string *string,
    const char *sql,
    size_t sql_size,
    size_t *inout_index
);
static int append_stored_procedure_sql_variable_token(
    struct mylite_dynamic_string *string,
    const char *sql,
    size_t sql_size,
    size_t *inout_index
);
static bool stored_procedure_local_reference_should_preserve_name(
    const char *sql,
    size_t sql_size,
    size_t start_index,
    size_t end_index
);
static bool stored_procedure_sql_previous_token_is_select(const char *sql, size_t token_end_index);
static bool stored_procedure_sql_word_at(
    const char *sql,
    size_t sql_size,
    size_t index,
    const char *word
);
static size_t stored_procedure_sql_next_nonspace(const char *sql, size_t sql_size, size_t index);
static bool stored_procedure_sql_identifier_start(unsigned char byte);
static bool stored_procedure_sql_identifier_continue(unsigned char byte);
static size_t stored_procedure_placeholder_top_level_semicolon(
    const struct stored_procedure_placeholder_token_list *tokens,
    size_t start_index,
    size_t end_index
);
static size_t stored_procedure_placeholder_top_level_token(
    const struct stored_procedure_placeholder_token_list *tokens,
    size_t start_index,
    size_t end_index,
    const char *text
);
static bool stored_procedure_placeholder_token_text_equals(
    const struct stored_procedure_placeholder_token_list *tokens,
    size_t index,
    const char *text
);
static bool stored_procedure_placeholder_raw_token_text_equals(
    const struct mylite_sql_token *token,
    const char *text
);
static bool stored_procedure_placeholder_token_is_identifier(
    const struct stored_procedure_placeholder_token_list *tokens,
    size_t index
);
static int copy_stored_procedure_placeholder_identifier(
    struct mylite_db *database,
    const struct mylite_sql_token *token,
    char *destination,
    size_t destination_size
);
static bool stored_procedure_placeholder_token_is_comment(enum mylite_sql_token_kind token_kind);
static void deinit_stored_procedure_placeholder_token_list(
    struct stored_procedure_placeholder_token_list *tokens
);
static void deinit_stored_procedure_placeholder_body(struct stored_procedure_placeholder_body *body
);
static int build_stored_procedure_show_create_sql(
    struct mylite_db *database,
    const struct stored_procedure_target *target,
    const char *body_sql,
    size_t body_sql_size,
    char **out_sql,
    size_t *out_sql_size
);
static int append_mysql_quoted_identifier_bytes(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_size
);
static int append_show_create_procedure_result(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_session_stored_procedure *procedure
);
static void copy_stored_procedure_field(char *destination, size_t size, const char *source);
static void deinit_session_stored_procedure_local_declaration(
    struct mylite_session_stored_procedure_local_declaration *declaration
);
static void deinit_session_stored_procedure_local_assignment(
    struct mylite_session_stored_procedure_local_assignment *assignment
);
static void deinit_stored_procedure_local_runtime_values(
    struct stored_procedure_local_runtime_value *values,
    size_t value_count
);
static bool drop_procedure_statement_has_if_exists(const struct mylite_sql_ast_node *statement);
static int append_missing_procedure_note(
    struct mylite_db *database,
    const struct stored_procedure_target *target
);
static void set_routine_exists_error(struct mylite_db *database, const char *routine_name);
static void set_duplicate_local_variable_error(
    struct mylite_db *database,
    const char *variable_name
);
static void set_routine_does_not_exist_error(
    struct mylite_db *database,
    const struct stored_procedure_target *target,
    bool include_schema
);
static void format_routine_does_not_exist_message(
    const struct stored_procedure_target *target,
    bool include_schema,
    char *message,
    size_t message_size
);

static int try_execute_stored_procedure_local_variables_placeholder_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result,
    bool *out_handled
) {
    struct stored_procedure_target target = {0};
    struct stored_procedure_placeholder_body body = {0};
    bool matches = false;
    int rc = MYLITE_OK;

    if (out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;

    rc = parse_stored_procedure_local_variables_placeholder(
        database,
        statement,
        &target,
        &body,
        &matches
    );
    if (rc == MYLITE_OK && matches) {
        *out_handled = true;
        rc = store_stored_procedure_placeholder_body(database, &target, &body, out_result);
    } else if (matches) {
        *out_handled = true;
    } else if (rc != MYLITE_OK) {
        rc = MYLITE_OK;
    }

    deinit_stored_procedure_placeholder_body(&body);
    return rc;
}

static int parse_stored_procedure_local_variables_placeholder(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct stored_procedure_target *out_target,
    struct stored_procedure_placeholder_body *out_body,
    bool *out_matches
) {
    struct stored_procedure_placeholder_token_list tokens = {0};
    struct stored_procedure_placeholder_target_parts target_parts = {0};
    size_t begin_index = 0U;
    int rc = MYLITE_OK;

    if (out_target == NULL || out_body == NULL || out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matches = false;
    if (statement == NULL ||
        statement->kind != MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT) {
        return MYLITE_OK;
    }

    rc = lex_stored_procedure_placeholder_statement(database, statement, &tokens);
    if (rc == MYLITE_OK) {
        rc = parse_stored_procedure_placeholder_target(
            database,
            &tokens,
            &target_parts,
            &begin_index,
            out_matches
        );
    }
    if (rc == MYLITE_OK && *out_matches) {
        rc = parse_stored_procedure_placeholder_body(database, &tokens, begin_index, out_body);
    }
    if (rc == MYLITE_OK && *out_matches) {
        rc = resolve_stored_procedure_placeholder_target(database, &target_parts, out_target);
    }

    deinit_stored_procedure_placeholder_token_list(&tokens);
    return rc;
}

static int lex_stored_procedure_placeholder_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct stored_procedure_placeholder_token_list *out_tokens
) {
    struct mylite_sql_lexer lexer;

    if (statement == NULL || statement->span.text == NULL || out_tokens == NULL) {
        return MYLITE_MISUSE;
    }
    *out_tokens = (struct stored_procedure_placeholder_token_list){0};
    out_tokens->sql = statement->span.text;
    out_tokens->sql_size = statement->span.length;
    mylite_sql_lexer_init(
        &lexer,
        (struct mylite_sql_lexer_config){
            .input = statement->span.text,
            .length = statement->span.length,
            .modes = mylite_execution_lexer_modes_for_session_sql_mode(&database->session),
        }
    );

    for (;;) {
        struct mylite_sql_token token = {0};

        if (mylite_sql_lexer_next(&lexer, &token) != 0) {
            return MYLITE_MISUSE;
        }
        if (token.text != NULL && token.text >= statement->span.text) {
            token.offset = (size_t)(token.text - statement->span.text);
        }
        if (token.kind == MYLITE_SQL_TOKEN_ERROR) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        if (token.kind == MYLITE_SQL_TOKEN_EOF) {
            return MYLITE_OK;
        }
        if (stored_procedure_placeholder_token_is_comment(token.kind)) {
            continue;
        }
        if (!append_stored_procedure_placeholder_token(&token, out_tokens)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
    }
}

static bool append_stored_procedure_placeholder_token(
    const struct mylite_sql_token *token,
    struct stored_procedure_placeholder_token_list *tokens
) {
    struct mylite_sql_token *grown = NULL;
    size_t new_capacity = 0U;

    if (token == NULL || tokens == NULL) {
        return false;
    }
    if (tokens->token_count == tokens->token_capacity) {
        new_capacity = tokens->token_capacity == 0U
                           ? stored_procedure_placeholder_initial_token_capacity
                           : tokens->token_capacity * 2U;
        if (new_capacity < tokens->token_capacity) {
            return false;
        }
        grown = (struct mylite_sql_token *)
            realloc(tokens->tokens, new_capacity * sizeof(*tokens->tokens));
        if (grown == NULL) {
            return false;
        }
        tokens->tokens = grown;
        tokens->token_capacity = new_capacity;
    }

    tokens->tokens[tokens->token_count] = *token;
    ++tokens->token_count;
    return true;
}

static int parse_stored_procedure_placeholder_target(
    struct mylite_db *database,
    const struct stored_procedure_placeholder_token_list *tokens,
    struct stored_procedure_placeholder_target_parts *out_target_parts,
    size_t *out_begin_index,
    bool *out_matches
) {
    size_t cursor = 2U;
    int rc = MYLITE_OK;

    if (tokens == NULL || out_target_parts == NULL || out_begin_index == NULL ||
        out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    *out_target_parts = (struct stored_procedure_placeholder_target_parts){0};
    *out_matches = false;
    *out_begin_index = 0U;
    if (tokens->token_count < stored_procedure_placeholder_min_create_tokens ||
        !stored_procedure_placeholder_token_text_equals(tokens, 0U, "CREATE") ||
        !stored_procedure_placeholder_token_text_equals(tokens, 1U, "PROCEDURE")) {
        return MYLITE_OK;
    }

    if (!stored_procedure_placeholder_token_is_identifier(tokens, cursor)) {
        set_parse_error(database, NULL);
        *out_matches = true;
        return MYLITE_ERROR;
    }
    rc = copy_stored_procedure_placeholder_identifier(
        database,
        &tokens->tokens[cursor],
        out_target_parts->parts[out_target_parts->part_count],
        sizeof(out_target_parts->parts[out_target_parts->part_count])
    );
    if (rc != MYLITE_OK) {
        *out_matches = true;
        return rc;
    }
    ++cursor;
    ++out_target_parts->part_count;

    if (stored_procedure_placeholder_token_text_equals(tokens, cursor, ".")) {
        ++cursor;
        if (!stored_procedure_placeholder_token_is_identifier(tokens, cursor)) {
            set_parse_error(database, NULL);
            *out_matches = true;
            return MYLITE_ERROR;
        }
        rc = copy_stored_procedure_placeholder_identifier(
            database,
            &tokens->tokens[cursor],
            out_target_parts->parts[out_target_parts->part_count],
            sizeof(out_target_parts->parts[out_target_parts->part_count])
        );
        if (rc != MYLITE_OK) {
            *out_matches = true;
            return rc;
        }
        ++cursor;
        ++out_target_parts->part_count;
    }

    if (!stored_procedure_placeholder_token_text_equals(tokens, cursor, "(") ||
        !stored_procedure_placeholder_token_text_equals(tokens, cursor + 1U, ")") ||
        !stored_procedure_placeholder_token_text_equals(tokens, cursor + 2U, "BEGIN")) {
        return MYLITE_OK;
    }

    *out_matches = true;
    *out_begin_index = cursor + 2U;
    return MYLITE_OK;
}

static int resolve_stored_procedure_placeholder_target(
    struct mylite_db *database,
    const struct stored_procedure_placeholder_target_parts *target_parts,
    struct stored_procedure_target *out_target
) {
    int rc = MYLITE_OK;

    if (target_parts == NULL || out_target == NULL) {
        return MYLITE_MISUSE;
    }
    *out_target = (struct stored_procedure_target){0};
    if (target_parts->part_count == 1U) {
        rc = resolve_selected_schema(database, &out_target->resolution.schema);
        if (rc == MYLITE_OK) {
            memcpy(
                out_target->resolution.table_name,
                target_parts->parts[0],
                strlen(target_parts->parts[0]) + 1U
            );
        }
        return rc;
    }
    if (target_parts->part_count == 2U) {
        if (mylite_catalog_name_is_reserved(target_parts->parts[0])) {
            set_reserved_name_error(database, "database", target_parts->parts[0]);
            return MYLITE_ERROR;
        }
        rc = resolve_schema_name(database, target_parts->parts[0], &out_target->resolution.schema);
        if (rc == MYLITE_OK) {
            memcpy(
                out_target->resolution.table_name,
                target_parts->parts[1],
                strlen(target_parts->parts[1]) + 1U
            );
            out_target->was_schema_qualified = true;
        }
        return rc;
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int parse_stored_procedure_placeholder_body(
    struct mylite_db *database,
    const struct stored_procedure_placeholder_token_list *tokens,
    size_t begin_index,
    struct stored_procedure_placeholder_body *out_body
) {
    size_t body_end_index = 0U;
    size_t cursor = begin_index + 1U;
    int rc = MYLITE_OK;

    if (tokens == NULL || out_body == NULL || begin_index >= tokens->token_count) {
        return MYLITE_MISUSE;
    }
    body_end_index = tokens->token_count - 1U;
    if (stored_procedure_placeholder_token_text_equals(tokens, body_end_index, ";")) {
        if (body_end_index == 0U) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        --body_end_index;
    }
    if (!stored_procedure_placeholder_token_text_equals(tokens, body_end_index, "END")) {
        set_unsupported_error(database, "stored procedure body shape is not supported");
        return MYLITE_ERROR;
    }

    while (rc == MYLITE_OK &&
           stored_procedure_placeholder_token_text_equals(tokens, cursor, "DECLARE")) {
        rc = parse_stored_procedure_placeholder_declaration(
            database,
            tokens,
            &cursor,
            body_end_index,
            out_body
        );
    }
    while (rc == MYLITE_OK && stored_procedure_placeholder_token_text_equals(tokens, cursor, "SET")
    ) {
        rc = parse_stored_procedure_placeholder_assignment(
            database,
            tokens,
            &cursor,
            body_end_index,
            out_body
        );
    }
    if (rc == MYLITE_OK) {
        rc = parse_stored_procedure_placeholder_select(
            database,
            tokens,
            (struct stored_procedure_placeholder_select_span){
                .cursor = cursor,
                .begin_index = begin_index,
                .body_end_index = body_end_index,
            },
            out_body
        );
    }

    return rc;
}

static int parse_stored_procedure_placeholder_declaration(
    struct mylite_db *database,
    const struct stored_procedure_placeholder_token_list *tokens,
    size_t *inout_index,
    size_t body_end_index,
    struct stored_procedure_placeholder_body *out_body
) {
    enum mylite_session_stored_procedure_local_value_kind value_kind =
        MYLITE_SESSION_STORED_PROCEDURE_LOCAL_VALUE_STRING;
    char *default_sql = NULL;
    size_t cursor = inout_index == NULL ? 0U : *inout_index + 1U;
    size_t names_start = cursor;
    size_t type_start = 0U;
    size_t default_index = SIZE_MAX;
    size_t semicolon_index = 0U;
    size_t default_sql_size = 0U;
    int rc = MYLITE_OK;

    if (tokens == NULL || inout_index == NULL || out_body == NULL) {
        return MYLITE_MISUSE;
    }
    if (!stored_procedure_placeholder_token_is_identifier(tokens, cursor)) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    for (;;) {
        ++cursor;
        if (!stored_procedure_placeholder_token_text_equals(tokens, cursor, ",")) {
            break;
        }
        ++cursor;
        if (!stored_procedure_placeholder_token_is_identifier(tokens, cursor)) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
    }

    type_start = cursor;
    semicolon_index =
        stored_procedure_placeholder_top_level_semicolon(tokens, type_start, body_end_index);
    if (semicolon_index == SIZE_MAX || type_start >= semicolon_index) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    default_index = stored_procedure_placeholder_top_level_token(
        tokens,
        type_start,
        semicolon_index,
        "DEFAULT"
    );
    if (!stored_procedure_local_value_kind_for_type_token(
            &tokens->tokens[type_start],
            &value_kind
        )) {
        set_unsupported_error(database, "stored procedure local variable type is not supported");
        return MYLITE_ERROR;
    }
    if (default_index != SIZE_MAX) {
        if (default_index + 1U >= semicolon_index) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        rc = copy_stored_procedure_sql_span(
            database,
            tokens->sql,
            tokens->sql_size,
            tokens->tokens[default_index + 1U].offset,
            tokens->tokens[semicolon_index].offset,
            &default_sql,
            &default_sql_size
        );
    }

    cursor = names_start;
    while (rc == MYLITE_OK && cursor < type_start) {
        char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        char *declaration_default_sql = NULL;
        size_t declaration_default_sql_size = 0U;

        rc = copy_stored_procedure_placeholder_identifier(
            database,
            &tokens->tokens[cursor],
            name,
            sizeof(name)
        );
        if (rc != MYLITE_OK) {
            break;
        }
        if (default_sql != NULL) {
            rc = copy_stored_procedure_sql_span(
                database,
                default_sql,
                default_sql_size,
                0U,
                default_sql_size,
                &declaration_default_sql,
                &declaration_default_sql_size
            );
        }
        if (rc == MYLITE_OK) {
            rc = append_stored_procedure_placeholder_declaration(
                database,
                name,
                value_kind,
                declaration_default_sql,
                declaration_default_sql_size,
                out_body
            );
        }
        if (rc == MYLITE_OK) {
            declaration_default_sql = NULL;
            cursor = stored_procedure_placeholder_token_text_equals(tokens, cursor + 1U, ",")
                         ? cursor + 2U
                         : type_start;
        }
        free(declaration_default_sql);
    }
    free(default_sql);
    if (rc == MYLITE_OK) {
        *inout_index = semicolon_index + 1U;
    }
    return rc;
}

static int parse_stored_procedure_placeholder_assignment(
    struct mylite_db *database,
    const struct stored_procedure_placeholder_token_list *tokens,
    size_t *inout_index,
    size_t body_end_index,
    struct stored_procedure_placeholder_body *out_body
) {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *value_sql = NULL;
    size_t cursor = inout_index == NULL ? 0U : *inout_index + 1U;
    size_t semicolon_index = 0U;
    size_t value_sql_size = 0U;
    int rc = MYLITE_OK;

    if (tokens == NULL || inout_index == NULL || out_body == NULL) {
        return MYLITE_MISUSE;
    }
    if (!stored_procedure_placeholder_token_is_identifier(tokens, cursor) ||
        !stored_procedure_placeholder_token_text_equals(tokens, cursor + 1U, "=")) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    rc = copy_stored_procedure_placeholder_identifier(
        database,
        &tokens->tokens[cursor],
        name,
        sizeof(name)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (stored_procedure_local_declaration_index_by_name(
            out_body->local_declarations,
            out_body->local_declaration_count,
            name
        ) == SIZE_MAX) {
        set_unknown_system_variable_name_error(database, name);
        return MYLITE_ERROR;
    }

    semicolon_index =
        stored_procedure_placeholder_top_level_semicolon(tokens, cursor + 2U, body_end_index);
    if (semicolon_index == SIZE_MAX || cursor + 2U >= semicolon_index) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    rc = copy_stored_procedure_sql_span(
        database,
        tokens->sql,
        tokens->sql_size,
        tokens->tokens[cursor + 2U].offset,
        tokens->tokens[semicolon_index].offset,
        &value_sql,
        &value_sql_size
    );
    if (rc == MYLITE_OK) {
        rc = append_stored_procedure_placeholder_assignment(
            database,
            name,
            value_sql,
            value_sql_size,
            out_body
        );
    }
    if (rc == MYLITE_OK) {
        value_sql = NULL;
        *inout_index = semicolon_index + 1U;
    }

    free(value_sql);
    return rc;
}

static int parse_stored_procedure_placeholder_select(
    struct mylite_db *database,
    const struct stored_procedure_placeholder_token_list *tokens,
    struct stored_procedure_placeholder_select_span span,
    struct stored_procedure_placeholder_body *out_body
) {
    size_t semicolon_index = 0U;
    size_t statement_size = 0U;
    int rc = MYLITE_OK;

    if (tokens == NULL || out_body == NULL || span.cursor >= tokens->token_count) {
        return MYLITE_MISUSE;
    }
    if (!stored_procedure_placeholder_token_text_equals(tokens, span.cursor, "SELECT")) {
        set_unsupported_error(database, "stored procedure body shape is not supported");
        return MYLITE_ERROR;
    }

    semicolon_index = stored_procedure_placeholder_top_level_semicolon(
        tokens,
        span.cursor + 1U,
        span.body_end_index
    );
    if (semicolon_index == SIZE_MAX || semicolon_index + 1U != span.body_end_index) {
        set_unsupported_error(database, "stored procedure body shape is not supported");
        return MYLITE_ERROR;
    }

    statement_size = tokens->sql_size;
    rc = copy_stored_procedure_sql_span(
        database,
        tokens->sql,
        statement_size,
        tokens->tokens[span.cursor].offset,
        tokens->tokens[semicolon_index].offset,
        &out_body->select_sql,
        &out_body->select_sql_size
    );
    if (rc == MYLITE_OK) {
        rc = copy_stored_procedure_sql_span(
            database,
            tokens->sql,
            statement_size,
            tokens->tokens[span.begin_index].offset,
            tokens->tokens[span.body_end_index].offset + tokens->tokens[span.body_end_index].length,
            &out_body->body_sql,
            &out_body->body_sql_size
        );
    }
    return rc;
}

static int append_stored_procedure_placeholder_declaration(
    struct mylite_db *database,
    const char *name,
    enum mylite_session_stored_procedure_local_value_kind value_kind,
    char *default_sql,
    size_t default_sql_size,
    struct stored_procedure_placeholder_body *out_body
) {
    struct mylite_session_stored_procedure_local_declaration *grown = NULL;
    struct mylite_session_stored_procedure_local_declaration *declaration = NULL;
    size_t old_count = 0U;
    size_t new_count = 0U;

    if (name == NULL || out_body == NULL) {
        return MYLITE_MISUSE;
    }
    if (stored_procedure_local_declaration_name_exists(
            out_body->local_declarations,
            out_body->local_declaration_count,
            name
        )) {
        set_duplicate_local_variable_error(database, name);
        return MYLITE_ERROR;
    }

    old_count = out_body->local_declaration_count;
    new_count = old_count + 1U;
    if (new_count < old_count) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    grown = (struct mylite_session_stored_procedure_local_declaration *)
        realloc(out_body->local_declarations, new_count * sizeof(*out_body->local_declarations));
    if (grown == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_body->local_declarations = grown;
    declaration = &out_body->local_declarations[old_count];
    *declaration = (struct mylite_session_stored_procedure_local_declaration){0};
    memcpy(declaration->name, name, strlen(name) + 1U);
    declaration->value_kind = value_kind;
    declaration->default_sql = default_sql;
    declaration->default_sql_size = default_sql_size;
    out_body->local_declaration_count = new_count;
    return MYLITE_OK;
}

static int append_stored_procedure_placeholder_assignment(
    struct mylite_db *database,
    const char *name,
    char *value_sql,
    size_t value_sql_size,
    struct stored_procedure_placeholder_body *out_body
) {
    struct mylite_session_stored_procedure_local_assignment *grown = NULL;
    struct mylite_session_stored_procedure_local_assignment *assignment = NULL;
    size_t old_count = 0U;
    size_t new_count = 0U;

    if (name == NULL || value_sql == NULL || out_body == NULL) {
        return MYLITE_MISUSE;
    }
    old_count = out_body->local_assignment_count;
    new_count = old_count + 1U;
    if (new_count < old_count) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    grown = (struct mylite_session_stored_procedure_local_assignment *)
        realloc(out_body->local_assignments, new_count * sizeof(*out_body->local_assignments));
    if (grown == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_body->local_assignments = grown;
    assignment = &out_body->local_assignments[old_count];
    *assignment = (struct mylite_session_stored_procedure_local_assignment){0};
    memcpy(assignment->name, name, strlen(name) + 1U);
    assignment->value_sql = value_sql;
    assignment->value_sql_size = value_sql_size;
    out_body->local_assignment_count = new_count;
    return MYLITE_OK;
}

static int store_stored_procedure_placeholder_body(
    struct mylite_db *database,
    const struct stored_procedure_target *target,
    struct stored_procedure_placeholder_body *body,
    mylite_result **out_result
) {
    struct mylite_session_stored_procedure *procedure = NULL;
    char *show_create_sql = NULL;
    size_t show_create_sql_size = 0U;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (target == NULL || body == NULL || body->select_sql == NULL || body->body_sql == NULL) {
        return MYLITE_MISUSE;
    }
    if (find_session_stored_procedure(
            &database->session,
            target->resolution.schema.name,
            target->resolution.table_name
        ) != NULL) {
        set_routine_exists_error(database, target->resolution.table_name);
        return MYLITE_ERROR;
    }

    rc = build_stored_procedure_show_create_sql(
        database,
        target,
        body->body_sql,
        body->body_sql_size,
        &show_create_sql,
        &show_create_sql_size
    );
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = prepare_session_stored_procedure_slot(database, &procedure);
    }
    if (rc == MYLITE_OK) {
        *procedure = (struct mylite_session_stored_procedure){0};
        memcpy(
            procedure->schema_name,
            target->resolution.schema.name,
            strlen(target->resolution.schema.name) + 1U
        );
        memcpy(
            procedure->name,
            target->resolution.table_name,
            strlen(target->resolution.table_name) + 1U
        );
        procedure->select_sql = body->select_sql;
        procedure->select_sql_size = body->select_sql_size;
        procedure->local_declarations = body->local_declarations;
        procedure->local_declaration_count = body->local_declaration_count;
        procedure->local_assignments = body->local_assignments;
        procedure->local_assignment_count = body->local_assignment_count;
        procedure->show_create_sql = show_create_sql;
        procedure->show_create_sql_size = show_create_sql_size;
        copy_stored_procedure_field(
            procedure->sql_mode_text,
            sizeof(procedure->sql_mode_text),
            database->session.sql_mode_text
        );
        copy_stored_procedure_field(
            procedure->character_set_client,
            sizeof(procedure->character_set_client),
            mylite_connection_statement_character_set_client(database)
        );
        copy_stored_procedure_field(
            procedure->collation_connection,
            sizeof(procedure->collation_connection),
            mylite_connection_statement_collation_connection(database)
        );
        copy_stored_procedure_field(
            procedure->database_collation,
            sizeof(procedure->database_collation),
            target->resolution.schema.default_collation
        );
        body->select_sql = NULL;
        body->select_sql_size = 0U;
        body->local_declarations = NULL;
        body->local_declaration_count = 0U;
        body->local_assignments = NULL;
        body->local_assignment_count = 0U;
        show_create_sql = NULL;
        ++database->session.stored_procedure_count;
    }

    free(show_create_sql);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_create_procedure_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct stored_procedure_target target = {0};
    struct mylite_session_stored_procedure *procedure = NULL;
    const struct mylite_sql_ast_node *select_statement = NULL;
    char *select_sql = NULL;
    char *body_sql = NULL;
    char *show_create_sql = NULL;
    mylite_result *result = NULL;
    size_t select_sql_size = 0U;
    size_t body_sql_size = 0U;
    size_t show_create_sql_size = 0U;
    int rc = resolve_stored_procedure_target(database, child_at(statement, 0U), &target);

    if (rc == MYLITE_OK && find_session_stored_procedure(
                               &database->session,
                               target.resolution.schema.name,
                               target.resolution.table_name
                           ) != NULL) {
        set_routine_exists_error(database, target.resolution.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        select_statement = child_at(statement, 1U);
    }
    if (rc == MYLITE_OK) {
        rc = copy_stored_procedure_sql(database, select_statement, &select_sql, &select_sql_size);
    }
    if (rc == MYLITE_OK) {
        rc = build_stored_procedure_body_sql_from_select(
            database,
            select_sql,
            select_sql_size,
            &body_sql,
            &body_sql_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = build_stored_procedure_show_create_sql(
            database,
            &target,
            body_sql,
            body_sql_size,
            &show_create_sql,
            &show_create_sql_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = prepare_session_stored_procedure_slot(database, &procedure);
    }
    if (rc == MYLITE_OK) {
        *procedure = (struct mylite_session_stored_procedure){0};
        memcpy(
            procedure->schema_name,
            target.resolution.schema.name,
            strlen(target.resolution.schema.name) + 1U
        );
        memcpy(
            procedure->name,
            target.resolution.table_name,
            strlen(target.resolution.table_name) + 1U
        );
        procedure->select_sql = select_sql;
        procedure->select_sql_size = select_sql_size;
        procedure->show_create_sql = show_create_sql;
        procedure->show_create_sql_size = show_create_sql_size;
        copy_stored_procedure_field(
            procedure->sql_mode_text,
            sizeof(procedure->sql_mode_text),
            database->session.sql_mode_text
        );
        copy_stored_procedure_field(
            procedure->character_set_client,
            sizeof(procedure->character_set_client),
            mylite_connection_statement_character_set_client(database)
        );
        copy_stored_procedure_field(
            procedure->collation_connection,
            sizeof(procedure->collation_connection),
            mylite_connection_statement_collation_connection(database)
        );
        copy_stored_procedure_field(
            procedure->database_collation,
            sizeof(procedure->database_collation),
            target.resolution.schema.default_collation
        );
        select_sql = NULL;
        show_create_sql = NULL;
        ++database->session.stored_procedure_count;
    }

    free(select_sql);
    free(body_sql);
    free(show_create_sql);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_drop_procedure_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct stored_procedure_target target = {0};
    mylite_result *result = NULL;
    size_t index = SIZE_MAX;
    bool if_exists = drop_procedure_statement_has_if_exists(statement);
    int rc = resolve_stored_procedure_target(database, child_at(statement, 0U), &target);

    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        index = find_session_stored_procedure_index(
            &database->session,
            target.resolution.schema.name,
            target.resolution.table_name
        );
        if (index != SIZE_MAX) {
            remove_session_stored_procedure_at(&database->session, index);
        } else if (if_exists) {
            rc = append_missing_procedure_note(database, &target);
        } else {
            set_routine_does_not_exist_error(database, &target, true);
            rc = MYLITE_ERROR;
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_create_procedure_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct stored_procedure_target target = {0};
    struct mylite_session_stored_procedure *procedure = NULL;
    mylite_result *result = NULL;
    int rc = resolve_stored_procedure_target(database, child_at(statement, 0U), &target);

    if (rc == MYLITE_OK) {
        procedure = find_session_stored_procedure(
            &database->session,
            target.resolution.schema.name,
            target.resolution.table_name
        );
        if (procedure == NULL) {
            set_routine_does_not_exist_error(database, &target, target.was_schema_qualified);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_procedure_result(database, result, procedure);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_call_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct stored_procedure_target target = {0};
    struct mylite_session_stored_procedure *procedure = NULL;
    bool is_sys_target = false;
    int rc = try_resolve_sys_stored_procedure_target(
        database,
        child_at(statement, 0U),
        &target,
        &is_sys_target
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_sys_target) {
        return execute_sys_stored_procedure_call(
            database,
            &target,
            child_at(statement, 1U),
            out_result
        );
    }

    if (child_at(statement, 1U) != NULL) {
        set_unsupported_error(database, "stored procedure arguments are not supported");
        return MYLITE_ERROR;
    }

    rc = resolve_stored_procedure_target(database, child_at(statement, 0U), &target);
    if (rc != MYLITE_OK) {
        return rc;
    }

    procedure = find_session_stored_procedure(
        &database->session,
        target.resolution.schema.name,
        target.resolution.table_name
    );
    if (procedure == NULL) {
        set_routine_does_not_exist_error(database, &target, true);
        return MYLITE_ERROR;
    }

    if (procedure->local_declaration_count != 0U || procedure->local_assignment_count != 0U) {
        return execute_stored_procedure_local_body(database, procedure, out_result);
    }
    return mylite_execute(database, procedure->select_sql, procedure->select_sql_size, out_result);
}

static int execute_sys_stored_procedure_call(
    struct mylite_db *database,
    const struct stored_procedure_target *target,
    const struct mylite_sql_ast_node *arguments,
    mylite_result **out_result
) {
    const struct sys_stored_procedure_definition *definition =
        find_sys_stored_procedure_definition(target->resolution.table_name);
    size_t actual_argument_count = stored_procedure_argument_count(arguments);

    if (definition == NULL) {
        set_routine_does_not_exist_error(database, target, true);
        return MYLITE_ERROR;
    }
    if (actual_argument_count != definition->argument_count) {
        set_sys_stored_procedure_argument_count_error(
            database,
            target,
            definition->argument_count,
            actual_argument_count
        );
        return MYLITE_ERROR;
    }

    switch (definition->behavior) {
    case SYS_STORED_PROCEDURE_TABLE_EXISTS:
        return execute_sys_table_exists_procedure(database, arguments, out_result);
    case SYS_STORED_PROCEDURE_EXECUTE_PREPARED_STMT:
        return execute_sys_execute_prepared_stmt_procedure(database, arguments, out_result);
    case SYS_STORED_PROCEDURE_SUMMARY_RESULT:
        return execute_sys_summary_procedure(database, definition, out_result);
    case SYS_STORED_PROCEDURE_EMPTY_RESULT:
        return execute_sys_empty_result_procedure(database, definition, out_result);
    case SYS_STORED_PROCEDURE_UNSUPPORTED_DIAGNOSTIC:
        return execute_sys_unsupported_procedure(database, definition);
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static const struct sys_stored_procedure_definition *find_sys_stored_procedure_definition(
    const char *name
) {
    size_t procedure_count = sizeof(sys_stored_procedures) / sizeof(sys_stored_procedures[0]);

    for (size_t index = 0U; index < procedure_count; ++index) {
        if (mylite_execution_text_equals_ascii_case_insensitive(
                name,
                sys_stored_procedures[index].name
            )) {
            return &sys_stored_procedures[index];
        }
    }

    return NULL;
}

static size_t stored_procedure_argument_count(const struct mylite_sql_ast_node *arguments) {
    size_t count = 0U;

    if (arguments == NULL) {
        return 0U;
    }
    for (const struct mylite_sql_ast_node *argument = child_at(arguments, 0U); argument != NULL;
         argument = argument->next_sibling) {
        ++count;
    }

    return count;
}

static const struct mylite_sql_ast_node *stored_procedure_argument_at(
    const struct mylite_sql_ast_node *arguments,
    size_t index
) {
    const struct mylite_sql_ast_node *argument = arguments == NULL ? NULL : child_at(arguments, 0U);

    while (argument != NULL && index > 0U) {
        argument = argument->next_sibling;
        --index;
    }

    return argument;
}

static int execute_sys_table_exists_procedure(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    mylite_result **out_result
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct session_scalar_cell output = {0};
    const struct mylite_sql_ast_node *out_argument = stored_procedure_argument_at(arguments, 2U);
    const char *exists_value = "";
    int rc = copy_sys_procedure_string_argument(
        database,
        stored_procedure_argument_at(arguments, 0U),
        schema_name,
        sizeof(schema_name)
    );

    if (rc == MYLITE_OK) {
        rc = copy_sys_procedure_string_argument(
            database,
            stored_procedure_argument_at(arguments, 1U),
            table_name,
            sizeof(table_name)
        );
    }
    if (rc == MYLITE_OK &&
        (out_argument == NULL || out_argument->kind != MYLITE_SQL_AST_USER_VARIABLE)) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = sys_table_exists_value(database, schema_name, table_name, &exists_value);
    }
    if (rc == MYLITE_OK) {
        output.value = exists_value;
        output.value_size = strlen(exists_value);
        output.has_value_size = true;
        rc = mylite_execution_set_session_user_variable(
            database,
            out_argument,
            &output,
            MYLITE_SESSION_USER_VARIABLE_VALUE_STRING
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    return finish_sys_empty_ok_result(database, out_result);
}

static int execute_sys_execute_prepared_stmt_procedure(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    mylite_result **out_result
) {
    struct session_scalar_cell query = {0};
    char saved_schema[MYLITE_SESSION_SCHEMA_CAPACITY];
    char *sql = NULL;
    size_t sql_size = 0U;
    bool saved_has_schema = database->session.has_selected_schema;
    int written =
        snprintf(saved_schema, sizeof(saved_schema), "%s", database->session.selected_schema);
    int rc = MYLITE_OK;

    if (written < 0 || (size_t)written >= sizeof(saved_schema)) {
        set_identifier_too_long_error(database, "database");
        return MYLITE_ERROR;
    }

    rc = session_scalar_value(database, stored_procedure_argument_at(arguments, 0U), &query);
    if (rc == MYLITE_OK) {
        rc = copy_session_scalar_sql_text(database, &query, "NULL", &sql, &sql_size);
    }
    if (rc == MYLITE_OK) {
        written = snprintf(
            database->session.selected_schema,
            sizeof(database->session.selected_schema),
            "sys"
        );
        if (written < 0 || (size_t)written >= sizeof(database->session.selected_schema)) {
            set_identifier_too_long_error(database, "database");
            rc = MYLITE_ERROR;
        } else {
            database->session.has_selected_schema = true;
        }
    }
    if (rc == MYLITE_OK) {
        rc = execute_expanded_prepared_statement_sql(database, sql, sql_size, NULL, out_result);
    }

    database->session.has_selected_schema = saved_has_schema;
    memcpy(
        database->session.selected_schema,
        saved_schema,
        sizeof(database->session.selected_schema)
    );
    session_scalar_cell_deinit(&query);
    free(sql);
    return rc;
}

static int execute_sys_summary_procedure(
    struct mylite_db *database,
    const struct sys_stored_procedure_definition *definition,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    const char *values[] = {definition == NULL ? "" : definition->summary};
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = append_sys_result_columns(database, result, definition->columns, definition->column_count);
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_sys_empty_result_procedure(
    struct mylite_db *database,
    const struct sys_stored_procedure_definition *definition,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = append_sys_result_columns(database, result, definition->columns, definition->column_count);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_sys_unsupported_procedure(
    struct mylite_db *database,
    const struct sys_stored_procedure_definition *definition
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "sys.%s() requires live Performance Schema instrumentation and is not supported by MyLite",
        definition == NULL ? "" : definition->name
    );

    if (written < 0 || (size_t)written >= sizeof(message)) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_not_supported_yet,
        "42000",
        message
    );
    return MYLITE_ERROR;
}

static int append_sys_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const char *const *columns,
    size_t column_count
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        rc = mylite_result_append_column(result, columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int copy_sys_procedure_string_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    char *buffer,
    size_t buffer_size
) {
    struct session_scalar_cell value = {0};
    const char *source = NULL;
    size_t source_size = 0U;
    int rc = MYLITE_OK;

    if (buffer == NULL || buffer_size == 0U) {
        return MYLITE_MISUSE;
    }
    buffer[0] = '\0';
    rc = session_scalar_value(database, argument, &value);
    if (rc == MYLITE_OK && value.value != NULL) {
        source = value.value;
        source_size = value.has_value_size ? value.value_size : strlen(value.value);
        if (source_size >= buffer_size) {
            set_identifier_too_long_error(database, "identifier");
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && source != NULL) {
        memcpy(buffer, source, source_size);
        buffer[source_size] = '\0';
    }

    session_scalar_cell_deinit(&value);
    return rc;
}

static int sys_table_exists_value(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char **out_value
) {
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    bool found = false;
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = "";
    if (schema_name == NULL || schema_name[0] == '\0' || table_name == NULL ||
        table_name[0] == '\0') {
        return MYLITE_OK;
    }

    rc = mylite_temporary_catalog_try_read_table_by_name(
        &database->session.temporary_catalog,
        schema_name,
        table_name,
        &table,
        &found
    );
    if (rc != MYLITE_OK || found) {
        *out_value = found ? "TEMPORARY" : "";
        return rc;
    }

    rc = mylite_catalog_try_read_schema_by_name(database, schema_name, &schema, &found);
    if (rc != MYLITE_OK || !found) {
        return rc;
    }
    rc = mylite_catalog_try_read_table_by_name(
        database,
        schema.schema_id,
        table_name,
        &table,
        &found
    );
    if (rc != MYLITE_OK || !found) {
        return rc;
    }

    *out_value = table.kind == MYLITE_CATALOG_TABLE_KIND_VIEW ? "VIEW" : "BASE TABLE";
    return MYLITE_OK;
}

static int finish_sys_empty_ok_result(struct mylite_db *database, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int resolve_stored_procedure_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_node,
    struct stored_procedure_target *out_target
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    bool is_sys_target = false;
    int rc = MYLITE_OK;

    if (out_target == NULL) {
        return MYLITE_MISUSE;
    }

    *out_target = (struct stored_procedure_target){0};
    rc = try_resolve_sys_stored_procedure_target(database, name_node, out_target, &is_sys_target);
    if (rc != MYLITE_OK || is_sys_target) {
        return rc;
    }

    rc =
        collect_identifier_parts(name_node, parts, table_name_part_capacity, &part_count, database);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = resolve_table_name(database, name_node, &out_target->resolution);
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_target->was_schema_qualified = part_count == 2U;
    return MYLITE_OK;
}

static int try_resolve_sys_stored_procedure_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_node,
    struct stored_procedure_target *out_target,
    bool *out_found
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const struct mylite_execution_catalog_builtin_schema *schema = NULL;
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (out_target == NULL || out_found == NULL) {
        return MYLITE_MISUSE;
    }
    *out_found = false;
    rc =
        collect_identifier_parts(name_node, parts, table_name_part_capacity, &part_count, database);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (part_count == 2U && mylite_execution_text_equals_ascii_case_insensitive(parts[0], "sys")) {
        schema = find_builtin_schema_descriptor(parts[0]);
        if (schema == NULL) {
            return MYLITE_OK;
        }
        rc = fill_builtin_stored_procedure_resolution(schema, parts[1], &out_target->resolution);
        out_target->was_schema_qualified = true;
        *out_found = rc == MYLITE_OK;
        return rc;
    }
    if (part_count == 1U && database != NULL && database->session.has_selected_schema &&
        mylite_execution_text_equals_ascii_case_insensitive(
            database->session.selected_schema,
            "sys"
        )) {
        schema = find_builtin_schema_descriptor(database->session.selected_schema);
        if (schema == NULL) {
            return MYLITE_OK;
        }
        rc = fill_builtin_stored_procedure_resolution(schema, parts[0], &out_target->resolution);
        out_target->was_schema_qualified = false;
        *out_found = rc == MYLITE_OK;
        return rc;
    }

    return MYLITE_OK;
}

static int fill_builtin_stored_procedure_resolution(
    const struct mylite_execution_catalog_builtin_schema *schema,
    const char *table_name,
    struct table_name_resolution *out_resolution
) {
    int written = 0;

    if (schema == NULL || table_name == NULL || out_resolution == NULL) {
        return MYLITE_MISUSE;
    }
    *out_resolution = (struct table_name_resolution){0};
    written = snprintf(
        out_resolution->schema.name,
        sizeof(out_resolution->schema.name),
        "%s",
        schema->name
    );
    if (written < 0 || (size_t)written >= sizeof(out_resolution->schema.name)) {
        return MYLITE_ERROR;
    }
    written = snprintf(
        out_resolution->schema.default_charset,
        sizeof(out_resolution->schema.default_charset),
        "%s",
        schema->default_charset
    );
    if (written < 0 || (size_t)written >= sizeof(out_resolution->schema.default_charset)) {
        return MYLITE_ERROR;
    }
    written = snprintf(
        out_resolution->schema.default_collation,
        sizeof(out_resolution->schema.default_collation),
        "%s",
        schema->default_collation
    );
    if (written < 0 || (size_t)written >= sizeof(out_resolution->schema.default_collation)) {
        return MYLITE_ERROR;
    }
    written =
        snprintf(out_resolution->table_name, sizeof(out_resolution->table_name), "%s", table_name);
    return written < 0 || (size_t)written >= sizeof(out_resolution->table_name) ? MYLITE_ERROR
                                                                                : MYLITE_OK;
}

static void set_sys_stored_procedure_argument_count_error(
    struct mylite_db *database,
    const struct stored_procedure_target *target,
    size_t expected,
    size_t actual
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect number of arguments for PROCEDURE %s.%s; expected %zu, got %zu",
        target == NULL ? "" : target->resolution.schema.name,
        target == NULL ? "" : target->resolution.table_name,
        expected,
        actual
    );

    if (written < 0 || (size_t)written >= sizeof(message)) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_routine_argument_count,
        "42000",
        message
    );
}

static int copy_stored_procedure_sql(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    char **out_sql,
    size_t *out_sql_size
) {
    char *copy = NULL;

    if (out_sql == NULL || out_sql_size == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    *out_sql_size = 0U;
    if (statement == NULL || statement->span.text == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (statement->span.length > SIZE_MAX - 1U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    copy = (char *)malloc(statement->span.length + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    memcpy(copy, statement->span.text, statement->span.length);
    copy[statement->span.length] = '\0';
    *out_sql = copy;
    *out_sql_size = statement->span.length;
    return MYLITE_OK;
}

static int copy_stored_procedure_sql_span(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    size_t start_offset,
    size_t end_offset,
    char **out_sql,
    size_t *out_sql_size
) {
    char *copy = NULL;
    size_t copy_size = 0U;

    if (sql == NULL || out_sql == NULL || out_sql_size == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    *out_sql_size = 0U;
    if (start_offset > end_offset || end_offset > sql_size) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    copy_size = end_offset - start_offset;
    if (copy_size > SIZE_MAX - 1U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    copy = (char *)malloc(copy_size + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(copy, &sql[start_offset], copy_size);
    copy[copy_size] = '\0';
    *out_sql = copy;
    *out_sql_size = copy_size;
    return MYLITE_OK;
}

static int build_stored_procedure_body_sql_from_select(
    struct mylite_db *database,
    const char *select_sql,
    size_t select_sql_size,
    char **out_sql,
    size_t *out_sql_size
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (select_sql == NULL || out_sql == NULL || out_sql_size == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    *out_sql_size = 0U;

    mylite_dynamic_string_init(&string);
    rc = mylite_dynamic_string_append(&string, "BEGIN ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_bytes(&string, select_sql, select_sql_size);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, "; END");
    }
    if (rc == MYLITE_OK) {
        *out_sql_size = string.length;
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static bool stored_procedure_local_value_kind_for_type_token(
    const struct mylite_sql_token *token,
    enum mylite_session_stored_procedure_local_value_kind *out_kind
) {
    static const char *const number_types[] = {
        "INT",
        "INTEGER",
        "BIGINT",
        "TINYINT",
        "SMALLINT",
        "MEDIUMINT",
        "BOOL",
        "BOOLEAN",
        "DECIMAL",
        "DEC",
        "NUMERIC",
        "FIXED",
        "FLOAT",
        "DOUBLE",
        "REAL",
        "BIT",
        "YEAR",
    };
    static const char *const string_types[] = {
        "CHAR",     "VARCHAR",    "NCHAR",    "NVARCHAR", "NATIONAL",  "TEXT",
        "TINYTEXT", "MEDIUMTEXT", "LONGTEXT", "BINARY",   "VARBINARY", "BLOB",
        "TINYBLOB", "MEDIUMBLOB", "LONGBLOB", "DATE",     "DATETIME",  "TIMESTAMP",
        "TIME",     "ENUM",       "SET",      "JSON",     "LONG",
    };

    if (token == NULL || out_kind == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(number_types) / sizeof(number_types[0]); ++index) {
        if (stored_procedure_placeholder_raw_token_text_equals(token, number_types[index])) {
            *out_kind = MYLITE_SESSION_STORED_PROCEDURE_LOCAL_VALUE_NUMBER;
            return true;
        }
    }
    for (size_t index = 0U; index < sizeof(string_types) / sizeof(string_types[0]); ++index) {
        if (stored_procedure_placeholder_raw_token_text_equals(token, string_types[index])) {
            *out_kind = MYLITE_SESSION_STORED_PROCEDURE_LOCAL_VALUE_STRING;
            return true;
        }
    }
    return false;
}

static bool stored_procedure_local_declaration_name_exists(
    const struct mylite_session_stored_procedure_local_declaration *declarations,
    size_t declaration_count,
    const char *name
) {
    return stored_procedure_local_declaration_index_by_name(
               declarations,
               declaration_count,
               name
           ) != SIZE_MAX;
}

static size_t stored_procedure_local_declaration_index_by_name(
    const struct mylite_session_stored_procedure_local_declaration *declarations,
    size_t declaration_count,
    const char *name
) {
    if (declarations == NULL || name == NULL) {
        return SIZE_MAX;
    }
    for (size_t index = 0U; index < declaration_count; ++index) {
        if (mylite_execution_text_equals_ascii_case_insensitive(declarations[index].name, name)) {
            return index;
        }
    }
    return SIZE_MAX;
}

static int execute_stored_procedure_local_body(
    struct mylite_db *database,
    const struct mylite_session_stored_procedure *procedure,
    mylite_result **out_result
) {
    struct stored_procedure_local_runtime_value *values = NULL;
    char *expanded_sql = NULL;
    size_t value_count = 0U;
    size_t expanded_sql_size = 0U;
    int rc = initialize_stored_procedure_local_runtime_values(
        database,
        procedure,
        &values,
        &value_count
    );

    if (rc == MYLITE_OK) {
        rc = apply_stored_procedure_local_assignments(database, procedure, values, value_count);
    }
    if (rc == MYLITE_OK) {
        rc = expand_stored_procedure_local_sql(
            database,
            procedure->select_sql,
            procedure->select_sql_size,
            values,
            value_count,
            true,
            &expanded_sql,
            &expanded_sql_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execute(database, expanded_sql, expanded_sql_size, out_result);
    }

    free(expanded_sql);
    deinit_stored_procedure_local_runtime_values(values, value_count);
    free(values);
    return rc;
}

static int initialize_stored_procedure_local_runtime_values(
    struct mylite_db *database,
    const struct mylite_session_stored_procedure *procedure,
    struct stored_procedure_local_runtime_value **out_values,
    size_t *out_value_count
) {
    struct stored_procedure_local_runtime_value *values = NULL;
    size_t declaration_count = procedure == NULL ? 0U : procedure->local_declaration_count;
    int rc = MYLITE_OK;

    if (out_values == NULL || out_value_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_values = NULL;
    *out_value_count = 0U;
    if (declaration_count == 0U) {
        return MYLITE_OK;
    }

    values =
        (struct stored_procedure_local_runtime_value *)calloc(declaration_count, sizeof(*values));
    if (values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t index = 0U; rc == MYLITE_OK && index < declaration_count; ++index) {
        const struct mylite_session_stored_procedure_local_declaration *declaration =
            &procedure->local_declarations[index];

        memcpy(values[index].name, declaration->name, strlen(declaration->name) + 1U);
        values[index].value_kind = declaration->value_kind;
        values[index].is_null = true;
        if (declaration->default_sql != NULL) {
            rc = evaluate_stored_procedure_local_expression(
                database,
                declaration->default_sql,
                declaration->default_sql_size,
                values,
                index,
                &values[index]
            );
        }
    }
    if (rc != MYLITE_OK) {
        deinit_stored_procedure_local_runtime_values(values, declaration_count);
        free(values);
        return rc;
    }

    *out_values = values;
    *out_value_count = declaration_count;
    return MYLITE_OK;
}

static int apply_stored_procedure_local_assignments(
    struct mylite_db *database,
    const struct mylite_session_stored_procedure *procedure,
    struct stored_procedure_local_runtime_value *values,
    size_t value_count
) {
    if (procedure == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; index < procedure->local_assignment_count; ++index) {
        const struct mylite_session_stored_procedure_local_assignment *assignment =
            &procedure->local_assignments[index];
        size_t value_index = stored_procedure_local_runtime_value_index_by_name(
            values,
            value_count,
            assignment->name
        );
        int rc = MYLITE_OK;

        if (value_index == SIZE_MAX) {
            set_unknown_system_variable_name_error(database, assignment->name);
            return MYLITE_ERROR;
        }
        rc = evaluate_stored_procedure_local_expression(
            database,
            assignment->value_sql,
            assignment->value_sql_size,
            values,
            value_count,
            &values[value_index]
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static int evaluate_stored_procedure_local_expression(
    struct mylite_db *database,
    const char *expression_sql,
    size_t expression_sql_size,
    struct stored_procedure_local_runtime_value *values,
    size_t value_count,
    struct stored_procedure_local_runtime_value *out_value
) {
    struct mylite_dynamic_string query;
    mylite_result *result = NULL;
    char *expanded_expression = NULL;
    char *query_sql = NULL;
    const char *value_text = NULL;
    size_t query_sql_size = 0U;
    size_t value_size = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (expression_sql == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

    mylite_dynamic_string_init(&query);
    rc = expand_stored_procedure_local_sql(
        database,
        expression_sql,
        expression_sql_size,
        values,
        value_count,
        false,
        &expanded_expression,
        &expression_sql_size
    );
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&query, "SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_bytes(&query, expanded_expression, expression_sql_size);
    }
    if (rc == MYLITE_OK) {
        query_sql_size = query.length;
        query_sql = mylite_dynamic_string_take(&query);
        if (query_sql == NULL) {
            set_nomem_error(database);
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execute(database, query_sql, query_sql_size, &result);
    }
    if (rc == MYLITE_OK &&
        (mylite_result_column_count(result) != 1U || mylite_result_row_count(result) != 1U)) {
        set_runtime_error(database, "stored procedure local expression did not return one value");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        value_text = mylite_result_value_text(result, 0U, 0U);
        is_null = value_text == NULL;
        value_size = is_null ? 0U : mylite_result_value_size(result, 0U, 0U);
        rc = set_stored_procedure_local_runtime_value(
            database,
            out_value,
            value_text,
            value_size,
            is_null
        );
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }

    mylite_result_free(result);
    free(query_sql);
    free(expanded_expression);
    mylite_dynamic_string_deinit(&query);
    return rc;
}

static int set_stored_procedure_local_runtime_value(
    struct mylite_db *database,
    struct stored_procedure_local_runtime_value *out_value,
    const char *text,
    size_t text_size,
    bool is_null
) {
    char *copy = NULL;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (is_null) {
        free(out_value->value);
        out_value->value = NULL;
        out_value->value_size = 0U;
        out_value->is_null = true;
        return MYLITE_OK;
    }
    if (text == NULL || text_size > SIZE_MAX - 1U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    copy = (char *)malloc(text_size + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(copy, text, text_size);
    copy[text_size] = '\0';
    free(out_value->value);
    out_value->value = copy;
    out_value->value_size = text_size;
    out_value->is_null = false;
    return MYLITE_OK;
}

static int expand_stored_procedure_local_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    const struct stored_procedure_local_runtime_value *values,
    size_t value_count,
    bool preserve_names,
    char **out_sql,
    size_t *out_sql_size
) {
    struct mylite_dynamic_string string;
    size_t index = 0U;
    int rc = MYLITE_OK;

    if (sql == NULL || out_sql == NULL || out_sql_size == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    *out_sql_size = 0U;

    mylite_dynamic_string_init(&string);
    while (rc == MYLITE_OK && index < sql_size) {
        unsigned char byte = (unsigned char)sql[index];

        if (byte == '\'' || byte == '"' || byte == '`') {
            rc = append_stored_procedure_sql_quoted_token(
                &string,
                sql,
                sql_size,
                &index,
                (char)byte
            );
        } else if (byte == '#' ||
                   (byte == '-' && index + 1U < sql_size && sql[index + 1U] == '-')) {
            rc = append_stored_procedure_sql_line_comment(&string, sql, sql_size, &index);
        } else if (byte == '/' && index + 1U < sql_size && sql[index + 1U] == '*') {
            rc = append_stored_procedure_sql_block_comment(&string, sql, sql_size, &index);
        } else if (byte == '@') {
            rc = append_stored_procedure_sql_variable_token(&string, sql, sql_size, &index);
        } else if (stored_procedure_sql_identifier_start(byte)) {
            char identifier[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
            const struct stored_procedure_local_runtime_value *value = NULL;
            size_t start = index;
            size_t identifier_size = 0U;
            bool is_qualified_component = false;

            while (index < sql_size &&
                   stored_procedure_sql_identifier_continue((unsigned char)sql[index])) {
                ++index;
            }
            identifier_size = index - start;
            is_qualified_component =
                (start > 0U && sql[start - 1U] == '.') || (index < sql_size && sql[index] == '.');
            if (!is_qualified_component && identifier_size < sizeof(identifier)) {
                memcpy(identifier, &sql[start], identifier_size);
                identifier[identifier_size] = '\0';
                value = find_stored_procedure_local_runtime_value(values, value_count, identifier);
            }
            if (value != NULL) {
                bool preserve_reference_name =
                    preserve_names && stored_procedure_local_reference_should_preserve_name(
                                          sql,
                                          sql_size,
                                          start,
                                          index
                                      );

                rc = preserve_reference_name
                         ? append_stored_procedure_local_reference(database, &string, value)
                         : append_stored_procedure_local_literal(database, &string, value);
            } else {
                rc = mylite_dynamic_string_append_bytes(&string, &sql[start], index - start);
            }
        } else {
            rc = mylite_dynamic_string_append_char(&string, sql[index]);
            ++index;
        }
    }
    if (rc == MYLITE_OK) {
        *out_sql_size = string.length;
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            set_nomem_error(database);
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_stored_procedure_local_reference(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct stored_procedure_local_runtime_value *value
) {
    int rc = mylite_dynamic_string_append(string, "NAME_CONST(");

    if (rc == MYLITE_OK) {
        rc = append_stored_procedure_quoted_string_literal(
            database,
            string,
            value == NULL ? "" : value->name,
            value == NULL ? 0U : strlen(value->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK) {
        rc = append_stored_procedure_local_literal(database, string, value);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_stored_procedure_local_literal(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct stored_procedure_local_runtime_value *value
) {
    if (value == NULL || value->is_null) {
        return mylite_dynamic_string_append(string, "NULL");
    }
    if (value->value_kind == MYLITE_SESSION_STORED_PROCEDURE_LOCAL_VALUE_STRING) {
        return append_stored_procedure_quoted_string_literal(
            database,
            string,
            value->value,
            value->value_size
        );
    }
    return mylite_dynamic_string_append_bytes(string, value->value, value->value_size);
}

static int append_stored_procedure_quoted_string_literal(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_size
) {
    int rc = MYLITE_OK;

    if (text == NULL && text_size != 0U) {
        return MYLITE_MISUSE;
    }
    if (text_size != 0U && memchr(text, '\0', text_size) != NULL) {
        set_unsupported_error(
            database,
            "stored procedure local variables do not support NUL bytes"
        );
        return MYLITE_ERROR;
    }

    rc = append_execute_string_literal(
        string,
        mylite_execution_lexer_modes_for_session_sql_mode(&database->session),
        text == NULL ? "" : text,
        text_size
    );
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static const struct stored_procedure_local_runtime_value *find_stored_procedure_local_runtime_value(
    const struct stored_procedure_local_runtime_value *values,
    size_t value_count,
    const char *name
) {
    size_t index = stored_procedure_local_runtime_value_index_by_name(values, value_count, name);

    return index == SIZE_MAX ? NULL : &values[index];
}

static size_t stored_procedure_local_runtime_value_index_by_name(
    const struct stored_procedure_local_runtime_value *values,
    size_t value_count,
    const char *name
) {
    if (values == NULL || name == NULL) {
        return SIZE_MAX;
    }
    for (size_t index = 0U; index < value_count; ++index) {
        if (mylite_execution_text_equals_ascii_case_insensitive(values[index].name, name)) {
            return index;
        }
    }
    return SIZE_MAX;
}

static int append_stored_procedure_sql_quoted_token(
    struct mylite_dynamic_string *string,
    const char *sql,
    size_t sql_size,
    size_t *inout_index,
    char quote
) {
    size_t index = inout_index == NULL ? 0U : *inout_index;
    int rc = mylite_dynamic_string_append_char(string, quote);

    if (inout_index == NULL || sql == NULL || index >= sql_size) {
        return MYLITE_MISUSE;
    }
    ++index;
    while (rc == MYLITE_OK && index < sql_size) {
        char byte = sql[index];

        rc = mylite_dynamic_string_append_char(string, byte);
        ++index;
        if (byte == '\\' && quote != '`' && index < sql_size) {
            rc = mylite_dynamic_string_append_char(string, sql[index]);
            ++index;
        } else if (byte == quote) {
            if (index < sql_size && sql[index] == quote) {
                rc = mylite_dynamic_string_append_char(string, sql[index]);
                ++index;
            } else {
                break;
            }
        }
    }
    *inout_index = index;
    return rc;
}

static int append_stored_procedure_sql_line_comment(
    struct mylite_dynamic_string *string,
    const char *sql,
    size_t sql_size,
    size_t *inout_index
) {
    size_t index = inout_index == NULL ? 0U : *inout_index;
    int rc = MYLITE_OK;

    if (inout_index == NULL || sql == NULL || index >= sql_size) {
        return MYLITE_MISUSE;
    }
    while (rc == MYLITE_OK && index < sql_size) {
        char byte = sql[index];

        rc = mylite_dynamic_string_append_char(string, byte);
        ++index;
        if (byte == '\n') {
            break;
        }
    }
    *inout_index = index;
    return rc;
}

static int append_stored_procedure_sql_block_comment(
    struct mylite_dynamic_string *string,
    const char *sql,
    size_t sql_size,
    size_t *inout_index
) {
    size_t index = inout_index == NULL ? 0U : *inout_index;
    int rc = MYLITE_OK;

    if (inout_index == NULL || sql == NULL || index >= sql_size) {
        return MYLITE_MISUSE;
    }
    while (rc == MYLITE_OK && index < sql_size) {
        char byte = sql[index];

        rc = mylite_dynamic_string_append_char(string, byte);
        ++index;
        if (byte == '*' && index < sql_size && sql[index] == '/') {
            rc = mylite_dynamic_string_append_char(string, sql[index]);
            ++index;
            break;
        }
    }
    *inout_index = index;
    return rc;
}

static int append_stored_procedure_sql_variable_token(
    struct mylite_dynamic_string *string,
    const char *sql,
    size_t sql_size,
    size_t *inout_index
) {
    size_t index = inout_index == NULL ? 0U : *inout_index;
    int rc = MYLITE_OK;

    if (inout_index == NULL || sql == NULL || index >= sql_size) {
        return MYLITE_MISUSE;
    }
    rc = mylite_dynamic_string_append_char(string, sql[index]);
    ++index;
    if (rc == MYLITE_OK && index < sql_size && sql[index] == '@') {
        rc = mylite_dynamic_string_append_char(string, sql[index]);
        ++index;
    }
    while (rc == MYLITE_OK && index < sql_size &&
           (stored_procedure_sql_identifier_continue((unsigned char)sql[index]) || sql[index] == '.'
           )) {
        rc = mylite_dynamic_string_append_char(string, sql[index]);
        ++index;
    }
    *inout_index = index;
    return rc;
}

static bool stored_procedure_local_reference_should_preserve_name(
    const char *sql,
    size_t sql_size,
    size_t start_index,
    size_t end_index
) {
    size_t previous = start_index;
    size_t next = stored_procedure_sql_next_nonspace(sql, sql_size, end_index);
    bool left_is_projection_start = false;

    if (sql == NULL || start_index > end_index || end_index > sql_size) {
        return false;
    }
    while (previous > 0U && isspace((unsigned char)sql[previous - 1U]) != 0) {
        --previous;
    }
    left_is_projection_start = previous == 0U || sql[previous - 1U] == ',' ||
                               stored_procedure_sql_previous_token_is_select(sql, previous);
    if (!left_is_projection_start) {
        return false;
    }
    if (next >= sql_size || sql[next] == ',' || sql[next] == ';') {
        return true;
    }
    return stored_procedure_sql_word_at(sql, sql_size, next, "FROM") ||
           stored_procedure_sql_word_at(sql, sql_size, next, "WHERE") ||
           stored_procedure_sql_word_at(sql, sql_size, next, "GROUP") ||
           stored_procedure_sql_word_at(sql, sql_size, next, "HAVING") ||
           stored_procedure_sql_word_at(sql, sql_size, next, "ORDER") ||
           stored_procedure_sql_word_at(sql, sql_size, next, "LIMIT");
}

static bool stored_procedure_sql_previous_token_is_select(const char *sql, size_t token_end_index) {
    size_t prefix_size = token_end_index;
    size_t token_start = token_end_index;

    if (sql == NULL) {
        return false;
    }
    while (token_start > 0U &&
           stored_procedure_sql_identifier_continue((unsigned char)sql[token_start - 1U])) {
        --token_start;
    }
    return token_start < token_end_index &&
           stored_procedure_sql_word_at(sql, prefix_size, token_start, "SELECT");
}

static bool stored_procedure_sql_word_at(
    const char *sql,
    size_t sql_size,
    size_t index,
    const char *word
) {
    size_t word_size = word == NULL ? 0U : strlen(word);

    if (sql == NULL || word == NULL || index > sql_size || sql_size - index < word_size) {
        return false;
    }
    for (size_t offset = 0U; offset < word_size; ++offset) {
        unsigned char actual = (unsigned char)sql[index + offset];
        unsigned char expected = (unsigned char)word[offset];

        if (actual >= 'a' && actual <= 'z') {
            actual = (unsigned char)(actual - 'a' + 'A');
        }
        if (expected >= 'a' && expected <= 'z') {
            expected = (unsigned char)(expected - 'a' + 'A');
        }
        if (actual != expected) {
            return false;
        }
    }
    return index + word_size >= sql_size ||
           !stored_procedure_sql_identifier_continue((unsigned char)sql[index + word_size]);
}

static size_t stored_procedure_sql_next_nonspace(const char *sql, size_t sql_size, size_t index) {
    while (sql != NULL && index < sql_size && isspace((unsigned char)sql[index]) != 0) {
        ++index;
    }
    return index;
}

static bool stored_procedure_sql_identifier_start(unsigned char byte) {
    return isalpha(byte) != 0 || byte == '_';
}

static bool stored_procedure_sql_identifier_continue(unsigned char byte) {
    return isalnum(byte) != 0 || byte == '_' || byte == '$';
}

static size_t stored_procedure_placeholder_top_level_semicolon(
    const struct stored_procedure_placeholder_token_list *tokens,
    size_t start_index,
    size_t end_index
) {
    size_t depth = 0U;

    if (tokens == NULL || start_index > end_index || end_index > tokens->token_count) {
        return SIZE_MAX;
    }
    for (size_t index = start_index; index < end_index; ++index) {
        if (stored_procedure_placeholder_token_text_equals(tokens, index, "(")) {
            ++depth;
        } else if (stored_procedure_placeholder_token_text_equals(tokens, index, ")")) {
            if (depth > 0U) {
                --depth;
            }
        } else if (depth == 0U &&
                   stored_procedure_placeholder_token_text_equals(tokens, index, ";")) {
            return index;
        }
    }
    return SIZE_MAX;
}

static size_t stored_procedure_placeholder_top_level_token(
    const struct stored_procedure_placeholder_token_list *tokens,
    size_t start_index,
    size_t end_index,
    const char *text
) {
    size_t depth = 0U;

    if (tokens == NULL || text == NULL || start_index > end_index ||
        end_index > tokens->token_count) {
        return SIZE_MAX;
    }
    for (size_t index = start_index; index < end_index; ++index) {
        if (stored_procedure_placeholder_token_text_equals(tokens, index, "(")) {
            ++depth;
        } else if (stored_procedure_placeholder_token_text_equals(tokens, index, ")")) {
            if (depth > 0U) {
                --depth;
            }
        } else if (depth == 0U &&
                   stored_procedure_placeholder_token_text_equals(tokens, index, text)) {
            return index;
        }
    }
    return SIZE_MAX;
}

static bool stored_procedure_placeholder_token_text_equals(
    const struct stored_procedure_placeholder_token_list *tokens,
    size_t index,
    const char *text
) {
    if (tokens == NULL || index >= tokens->token_count) {
        return false;
    }
    return stored_procedure_placeholder_raw_token_text_equals(&tokens->tokens[index], text);
}

static bool stored_procedure_placeholder_raw_token_text_equals(
    const struct mylite_sql_token *token,
    const char *text
) {
    size_t text_size = text == NULL ? 0U : strlen(text);

    if (token == NULL || token->text == NULL || text == NULL || token->length != text_size) {
        return false;
    }
    for (size_t index = 0U; index < text_size; ++index) {
        unsigned char actual = (unsigned char)token->text[index];
        unsigned char expected = (unsigned char)text[index];

        if (actual >= 'a' && actual <= 'z') {
            actual = (unsigned char)(actual - 'a' + 'A');
        }
        if (expected >= 'a' && expected <= 'z') {
            expected = (unsigned char)(expected - 'a' + 'A');
        }
        if (actual != expected) {
            return false;
        }
    }
    return true;
}

static bool stored_procedure_placeholder_token_is_identifier(
    const struct stored_procedure_placeholder_token_list *tokens,
    size_t index
) {
    enum mylite_sql_token_kind kind = MYLITE_SQL_TOKEN_EOF;

    if (tokens == NULL || index >= tokens->token_count) {
        return false;
    }
    kind = tokens->tokens[index].kind;
    return kind == MYLITE_SQL_TOKEN_IDENTIFIER || kind == MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER ||
           kind == MYLITE_SQL_TOKEN_KEYWORD;
}

static int copy_stored_procedure_placeholder_identifier(
    struct mylite_db *database,
    const struct mylite_sql_token *token,
    char *destination,
    size_t destination_size
) {
    int rc = MYLITE_OK;

    if (token == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_MISUSE;
    }
    destination[0] = '\0';
    if (token->kind == MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER) {
        rc = copy_quoted_identifier_text(token->text, token->length, destination, destination_size);
    } else if (token->kind == MYLITE_SQL_TOKEN_IDENTIFIER ||
               token->kind == MYLITE_SQL_TOKEN_KEYWORD) {
        rc = copy_unquoted_identifier_text(
            token->text,
            token->length,
            destination,
            destination_size
        );
    } else {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        set_identifier_too_long_error(database, "identifier");
    }
    return rc;
}

static bool stored_procedure_placeholder_token_is_comment(enum mylite_sql_token_kind token_kind) {
    return token_kind == MYLITE_SQL_TOKEN_COMMENT ||
           token_kind == MYLITE_SQL_TOKEN_VERSION_COMMENT ||
           token_kind == MYLITE_SQL_TOKEN_HINT_COMMENT;
}

static void deinit_stored_procedure_placeholder_token_list(
    struct stored_procedure_placeholder_token_list *tokens
) {
    if (tokens == NULL) {
        return;
    }
    free(tokens->tokens);
    *tokens = (struct stored_procedure_placeholder_token_list){0};
}

static void deinit_stored_procedure_placeholder_body(struct stored_procedure_placeholder_body *body
) {
    if (body == NULL) {
        return;
    }
    for (size_t index = 0U; index < body->local_declaration_count; ++index) {
        deinit_session_stored_procedure_local_declaration(&body->local_declarations[index]);
    }
    for (size_t index = 0U; index < body->local_assignment_count; ++index) {
        deinit_session_stored_procedure_local_assignment(&body->local_assignments[index]);
    }
    free(body->local_declarations);
    free(body->local_assignments);
    free(body->select_sql);
    free(body->body_sql);
    *body = (struct stored_procedure_placeholder_body){0};
}

static int build_stored_procedure_show_create_sql(
    struct mylite_db *database,
    const struct stored_procedure_target *target,
    const char *body_sql,
    size_t body_sql_size,
    char **out_sql,
    size_t *out_sql_size
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (target == NULL || body_sql == NULL || out_sql == NULL || out_sql_size == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    *out_sql_size = 0U;

    mylite_dynamic_string_init(&string);
    rc = mylite_dynamic_string_append(&string, "CREATE DEFINER=");
    if (rc == MYLITE_OK) {
        rc = append_show_create_definer(&string, database->session.current_user_identity);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " PROCEDURE ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(
            &string,
            target->resolution.table_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, "()\n");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_bytes(&string, body_sql, body_sql_size);
    }
    if (rc == MYLITE_OK) {
        *out_sql_size = string.length;
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_show_create_definer(struct mylite_dynamic_string *string, const char *identity) {
    const char *separator = NULL;
    const char *user = identity == NULL ? "" : identity;
    const char *host = "%";
    size_t user_size = strlen(user);
    size_t host_size = 1U;
    int rc = MYLITE_OK;

    separator = strchr(user, '@');
    if (separator != NULL) {
        user_size = (size_t)(separator - user);
        host = separator + 1;
        host_size = strlen(host);
    }

    rc = append_mysql_quoted_identifier_bytes(string, user, user_size);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '@');
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_quoted_identifier_bytes(string, host, host_size);
    }
    return rc;
}

static int append_mysql_quoted_identifier_bytes(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_size
) {
    int rc = mylite_dynamic_string_append_char(string, '`');

    for (size_t index = 0U; rc == MYLITE_OK && index < text_size; ++index) {
        if (text[index] == '`') {
            rc = mylite_dynamic_string_append(string, "``");
        } else {
            rc = mylite_dynamic_string_append_char(string, text[index]);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '`');
    }
    return rc;
}

static int append_show_create_procedure_result(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_session_stored_procedure *procedure
) {
    enum { column_count = 6 };

    static const char *const result_columns[column_count] = {
        "Procedure",
        "sql_mode",
        "Create Procedure",
        "character_set_client",
        "collation_connection",
        "Database Collation",
    };
    const char *values[column_count] = {
        procedure->name,
        procedure->sql_mode_text,
        procedure->show_create_sql,
        procedure->character_set_client,
        procedure->collation_connection,
        procedure->database_collation,
    };
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static void copy_stored_procedure_field(char *destination, size_t size, const char *source) {
    int written = snprintf(destination, size, "%s", source == NULL ? "" : source);

    if (written < 0 || (size_t)written >= size) {
        destination[0] = '\0';
    }
}

static void deinit_session_stored_procedure_local_declaration(
    struct mylite_session_stored_procedure_local_declaration *declaration
) {
    if (declaration == NULL) {
        return;
    }

    free(declaration->default_sql);
    *declaration = (struct mylite_session_stored_procedure_local_declaration){0};
}

static void deinit_session_stored_procedure_local_assignment(
    struct mylite_session_stored_procedure_local_assignment *assignment
) {
    if (assignment == NULL) {
        return;
    }

    free(assignment->value_sql);
    *assignment = (struct mylite_session_stored_procedure_local_assignment){0};
}

static void deinit_stored_procedure_local_runtime_values(
    struct stored_procedure_local_runtime_value *values,
    size_t value_count
) {
    if (values == NULL) {
        return;
    }
    for (size_t index = 0U; index < value_count; ++index) {
        free(values[index].value);
        values[index] = (struct stored_procedure_local_runtime_value){0};
    }
}

static bool drop_procedure_statement_has_if_exists(const struct mylite_sql_ast_node *statement) {
    return statement != NULL && child_at(statement, 1U) != NULL;
}

static int append_missing_procedure_note(
    struct mylite_db *database,
    const struct stored_procedure_target *target
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

    format_routine_does_not_exist_message(target, true, message, sizeof(message));
    return mylite_diagnostics_append_note(
        mylite_connection_diagnostics(database),
        mysql_error_routine_does_not_exist,
        "42000",
        message
    );
}

static void set_routine_exists_error(struct mylite_db *database, const char *routine_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "PROCEDURE %s already exists",
        routine_name == NULL ? "" : routine_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_routine_exists,
        "42000",
        message
    );
}

static void set_duplicate_local_variable_error(
    struct mylite_db *database,
    const char *variable_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Duplicate variable: %s",
        variable_name == NULL ? "" : variable_name
    );

    if (written < 0 || (size_t)written >= sizeof(message)) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_duplicate_variable,
        "42000",
        message
    );
}

static void set_routine_does_not_exist_error(
    struct mylite_db *database,
    const struct stored_procedure_target *target,
    bool include_schema
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

    format_routine_does_not_exist_message(target, include_schema, message, sizeof(message));
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_routine_does_not_exist,
        "42000",
        message
    );
}

static void format_routine_does_not_exist_message(
    const struct stored_procedure_target *target,
    bool include_schema,
    char *message,
    size_t message_size
) {
    int written = 0;

    if (target == NULL) {
        written = snprintf(message, message_size, "PROCEDURE does not exist");
    } else if (include_schema) {
        written = snprintf(
            message,
            message_size,
            "PROCEDURE %s.%s does not exist",
            target->resolution.schema.name,
            target->resolution.table_name
        );
    } else {
        written = snprintf(
            message,
            message_size,
            "PROCEDURE %s does not exist",
            target->resolution.table_name
        );
    }
    if (written < 0 || (size_t)written >= message_size) {
        message[0] = '\0';
    }
}

static struct mylite_session_stored_procedure *find_session_stored_procedure(
    struct mylite_session_state *session,
    const char *schema_name,
    const char *name
) {
    size_t index = find_session_stored_procedure_index(session, schema_name, name);

    return index == SIZE_MAX ? NULL : &session->stored_procedures[index];
}

static size_t find_session_stored_procedure_index(
    const struct mylite_session_state *session,
    const char *schema_name,
    const char *name
) {
    if (session == NULL || schema_name == NULL || name == NULL) {
        return SIZE_MAX;
    }
    for (size_t index = 0U; index < session->stored_procedure_count; ++index) {
        const struct mylite_session_stored_procedure *procedure =
            &session->stored_procedures[index];

        if (strcmp(procedure->schema_name, schema_name) == 0 &&
            strcmp(procedure->name, name) == 0) {
            return index;
        }
    }
    return SIZE_MAX;
}

static void remove_session_stored_procedure_at(struct mylite_session_state *session, size_t index) {
    if (session == NULL || index >= session->stored_procedure_count) {
        return;
    }

    deinit_session_stored_procedure(&session->stored_procedures[index]);
    for (size_t move_index = index + 1U; move_index < session->stored_procedure_count;
         ++move_index) {
        session->stored_procedures[move_index - 1U] = session->stored_procedures[move_index];
    }
    --session->stored_procedure_count;
    session->stored_procedures[session->stored_procedure_count] =
        (struct mylite_session_stored_procedure){0};
}

static void deinit_session_stored_procedure(struct mylite_session_stored_procedure *procedure) {
    if (procedure == NULL) {
        return;
    }

    for (size_t index = 0U; index < procedure->local_declaration_count; ++index) {
        deinit_session_stored_procedure_local_declaration(&procedure->local_declarations[index]);
    }
    for (size_t index = 0U; index < procedure->local_assignment_count; ++index) {
        deinit_session_stored_procedure_local_assignment(&procedure->local_assignments[index]);
    }
    free(procedure->select_sql);
    free(procedure->local_declarations);
    free(procedure->local_assignments);
    free(procedure->show_create_sql);
    *procedure = (struct mylite_session_stored_procedure){0};
}

static int prepare_session_stored_procedure_slot(
    struct mylite_db *database,
    struct mylite_session_stored_procedure **out_procedure
) {
    struct mylite_session_state *session = database == NULL ? NULL : &database->session;
    struct mylite_session_stored_procedure *procedures = NULL;
    size_t new_capacity = 0U;

    if (session == NULL || out_procedure == NULL) {
        return MYLITE_MISUSE;
    }
    *out_procedure = NULL;

    if (session->stored_procedure_count >= session->stored_procedure_capacity) {
        new_capacity =
            session->stored_procedure_capacity == 0U ? 4U : session->stored_procedure_capacity * 2U;
        if (new_capacity < session->stored_procedure_capacity ||
            new_capacity > SIZE_MAX / sizeof(*procedures)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }

        procedures = (struct mylite_session_stored_procedure *)
            realloc(session->stored_procedures, new_capacity * sizeof(*procedures));
        if (procedures == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        memset(
            procedures + session->stored_procedure_capacity,
            0,
            (new_capacity - session->stored_procedure_capacity) * sizeof(*procedures)
        );
        session->stored_procedures = procedures;
        session->stored_procedure_capacity = new_capacity;
    }

    if (session->stored_procedures == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    *out_procedure = &session->stored_procedures[session->stored_procedure_count];
    return MYLITE_OK;
}
