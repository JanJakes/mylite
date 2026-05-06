#include "mylite_schema.h"

#include "mylite_catalog.h"
#include "mylite_charset.h"
#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_runtime.h"
#include "mylite_span.h"

#include <stdlib.h>
#include <string.h>

static int normalize_schema_charset_and_collation(
    mylite_db *database,
    struct mylite_schema_options *options
);

static int normalize_schema_option_text(mylite_db *database, char **target, const char *value);

static int apply_schema_option(
    const struct mylite_sql_ast_node *option,
    struct mylite_schema_options *options
);

static bool is_valid_encryption_value(const char *value);

void mylite_schema_options_deinit(struct mylite_schema_options *options) {
    if (options == NULL) {
        return;
    }

    free(options->character_set);
    free(options->collation);
    free(options->encryption);
    *options = (struct mylite_schema_options){0};
}

int mylite_schema_normalize_options(mylite_db *database, struct mylite_schema_options *options) {
    int status = MYLITE_OK;

    if (options->invalid_encryption) {
        (void)mylite_diagnostics_set_error_message(
            database,
            "Incorrect argument (should be Y or N) value"
        );
        return MYLITE_EXEC_ERROR;
    }
    if (options->invalid_read_only) {
        (void)mylite_diagnostics_set_error_message(database, "Incorrect READ ONLY value");
        return MYLITE_EXEC_ERROR;
    }

    status = normalize_schema_charset_and_collation(database, options);
    return status;
}

int mylite_schema_copy_statement_name(
    const struct mylite_sql_ast_node *statement,
    char **out_schema_name
) {
    const struct mylite_sql_ast_node *schema_name = NULL;

    *out_schema_name = NULL;
    schema_name = mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_IDENTIFIER);

    if (schema_name == NULL) {
        return MYLITE_OK;
    }

    *out_schema_name = mylite_copy_identifier_span(schema_name);
    return *out_schema_name == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

int mylite_schema_copy_options(
    const struct mylite_sql_ast_node *statement,
    struct mylite_schema_options *options
) {
    const struct mylite_sql_ast_node *option_list =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_SCHEMA_OPTION_LIST);
    int status = MYLITE_OK;

    for (const struct mylite_sql_ast_node *option = option_list == NULL ? NULL
                                                                        : option_list->first_child;
         option != NULL;
         option = option->next_sibling) {
        status = apply_schema_option(option, options);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

int mylite_schema_execute_create_statement(mylite_stmt *stmt) {
    struct mylite_schema_presence presence;
    int status = mylite_schema_normalize_options(stmt->database, &stmt->options);

    if (status != MYLITE_OK) {
        return status;
    }
    status = mylite_catalog_schema_exists(stmt->database, stmt->schema_name, &presence);
    if (status != MYLITE_OK) {
        return status;
    }
    if (presence.exists) {
        if (stmt->if_not_exists) {
            int note_status = mylite_diagnostics_set_error_message_parts(
                stmt->database,
                "Can't create database '",
                stmt->schema_name,
                "'; database exists"
            );

            if (note_status == MYLITE_NOMEM) {
                return MYLITE_NOMEM;
            }
            return mylite_diagnostics_append_note(
                stmt->database,
                MYLITE_MYSQL_ER_DB_CREATE_EXISTS,
                mylite_error_message(stmt->database)
            );
        }
        (void)mylite_diagnostics_set_error_message_parts(
            stmt->database,
            "Can't create database '",
            stmt->schema_name,
            "'; database exists"
        );
        return MYLITE_EXEC_ERROR;
    }

    return mylite_catalog_insert_schema(stmt->database, stmt->schema_name, &stmt->options);
}

int mylite_schema_execute_alter_statement(mylite_stmt *stmt) {
    const char *schema_name =
        stmt->schema_name == NULL ? stmt->database->selected_schema : stmt->schema_name;
    struct mylite_schema_presence presence;
    int status = mylite_schema_normalize_options(stmt->database, &stmt->options);

    if (status != MYLITE_OK) {
        return status;
    }
    if (schema_name == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "No database selected");
        return MYLITE_EXEC_ERROR;
    }

    status = mylite_catalog_schema_exists(stmt->database, schema_name, &presence);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(
            stmt->database,
            "Database '",
            schema_name,
            "' doesn't exist"
        );
        return MYLITE_EXEC_ERROR;
    }
    if (presence.is_system) {
        return mylite_diagnostics_set_schema_access_denied_error(stmt->database, schema_name);
    }

    return mylite_catalog_update_schema(stmt->database, schema_name, &stmt->options);
}

int mylite_schema_execute_drop_statement(mylite_stmt *stmt) {
    struct mylite_schema_presence presence;
    int status = mylite_catalog_schema_exists(stmt->database, stmt->schema_name, &presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        if (stmt->if_exists) {
            return MYLITE_OK;
        }
        (void)mylite_diagnostics_set_error_message_parts(
            stmt->database,
            "Can't drop database '",
            stmt->schema_name,
            "'; database doesn't exist"
        );
        return MYLITE_EXEC_ERROR;
    }
    if (presence.is_system) {
        return mylite_diagnostics_set_schema_access_denied_error(stmt->database, stmt->schema_name);
    }

    status = mylite_catalog_delete_schema(stmt->database, stmt->schema_name);
    if (status == MYLITE_OK) {
        mylite_connection_clear_selected_schema_if_matches(stmt->database, stmt->schema_name);
    }
    return status;
}

int mylite_schema_execute_use_statement(mylite_stmt *stmt) {
    struct mylite_schema_presence presence;
    int status = MYLITE_OK;

    if (mylite_span_contains_newline(stmt->schema_name, strlen(stmt->schema_name))) {
        (void)mylite_diagnostics_set_error_message(
            stmt->database,
            "USE database names must be single-line"
        );
        return MYLITE_EXEC_ERROR;
    }

    status = mylite_catalog_schema_exists(stmt->database, stmt->schema_name, &presence);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(
            stmt->database,
            "Unknown database '",
            stmt->schema_name,
            "'"
        );
        return MYLITE_EXEC_ERROR;
    }

    return mylite_connection_set_selected_schema(stmt->database, stmt->schema_name);
}

static int normalize_schema_charset_and_collation(
    mylite_db *database,
    struct mylite_schema_options *options
) {
    const struct mylite_charset *character_set = mylite_charset_lookup(options->character_set);
    const struct mylite_collation *collation = mylite_collation_lookup(options->collation);
    int status = MYLITE_OK;

    if (options->character_set != NULL && character_set == NULL) {
        return mylite_diagnostics_set_unknown_charset_error(database, options->character_set);
    }
    if (options->collation != NULL && collation == NULL) {
        return mylite_diagnostics_set_unknown_collation_error(database, options->collation);
    }
    if (character_set != NULL && collation != NULL &&
        !mylite_charset_collation_match(character_set, collation)) {
        return mylite_diagnostics_set_collation_charset_error(
            database,
            collation->name,
            character_set->name
        );
    }
    if (character_set == NULL && collation == NULL) {
        return MYLITE_OK;
    }

    if (character_set == NULL) {
        character_set = mylite_charset_lookup(collation->character_set);
    }
    if (collation == NULL) {
        collation = mylite_collation_lookup(character_set->default_collation);
    }
    if (character_set == NULL || collation == NULL) {
        (void)mylite_diagnostics_set_error_message(
            database,
            "Unsupported charset/collation registry entry"
        );
        return MYLITE_EXEC_ERROR;
    }

    status = normalize_schema_option_text(database, &options->character_set, character_set->name);
    if (status != MYLITE_OK) {
        return status;
    }
    return normalize_schema_option_text(database, &options->collation, collation->name);
}

static int normalize_schema_option_text(mylite_db *database, char **target, const char *value) {
    char *copy = mylite_copy_span_text(value, strlen(value));

    if (copy == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    free(*target);
    *target = copy;
    return MYLITE_OK;
}

static int apply_schema_option(
    const struct mylite_sql_ast_node *option,
    struct mylite_schema_options *options
) {
    const struct mylite_sql_ast_node *value = mylite_ast_child_at(option, 0U);
    char **target = NULL;
    char *copy = NULL;

    switch (option->schema_option) {
    case MYLITE_SQL_AST_SCHEMA_OPTION_CHARACTER_SET:
        target = &options->character_set;
        copy = mylite_copy_schema_text_span(value);
        break;
    case MYLITE_SQL_AST_SCHEMA_OPTION_COLLATE:
        target = &options->collation;
        copy = mylite_copy_schema_text_span(value);
        break;
    case MYLITE_SQL_AST_SCHEMA_OPTION_ENCRYPTION:
        target = &options->encryption;
        copy = mylite_copy_string_literal_span(value);
        break;
    case MYLITE_SQL_AST_SCHEMA_OPTION_READ_ONLY:
        options->has_read_only = true;
        options->read_only = 0;
        if (value != NULL && value->kind != MYLITE_SQL_AST_IDENTIFIER) {
            if (value->span.length == 1U && value->span.text != NULL &&
                value->span.text[0] == '1') {
                options->read_only = 1;
            } else if (
                value->span.length != 1U || value->span.text == NULL || value->span.text[0] != '0'
            ) {
                options->invalid_read_only = true;
            }
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_SCHEMA_OPTION_NONE:
        return MYLITE_OK;
    }

    if (copy == NULL) {
        return MYLITE_NOMEM;
    }
    if (option->schema_option == MYLITE_SQL_AST_SCHEMA_OPTION_ENCRYPTION &&
        !is_valid_encryption_value(copy)) {
        options->invalid_encryption = true;
    }

    free(*target);
    *target = copy;
    return MYLITE_OK;
}

static bool is_valid_encryption_value(const char *value) {
    if (value == NULL || value[0] == '\0' || value[1] != '\0') {
        return false;
    }
    if (value[0] == 'Y' || value[0] == 'y' || value[0] == 'N' || value[0] == 'n') {
        return true;
    }
    return false;
}
