#include "mylite_system_variables.h"

#include "mylite_catalog.h"
#include "mylite_charset.h"
#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_expression_descriptor.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum mylite_system_variable_id {
    MYLITE_SYSTEM_VARIABLE_AUTOCOMMIT = 0,
    MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT = 1,
    MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_CONNECTION = 2,
    MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_DATABASE = 3,
    MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_FILESYSTEM = 4,
    MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS = 5,
    MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_SERVER = 6,
    MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_SYSTEM = 7,
    MYLITE_SYSTEM_VARIABLE_CHARACTER_SETS_DIR = 8,
    MYLITE_SYSTEM_VARIABLE_COLLATION_CONNECTION = 9,
    MYLITE_SYSTEM_VARIABLE_COLLATION_DATABASE = 10,
    MYLITE_SYSTEM_VARIABLE_COLLATION_SERVER = 11,
    MYLITE_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE = 12,
    MYLITE_SYSTEM_VARIABLE_ERROR_COUNT = 13,
    MYLITE_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS = 14,
    MYLITE_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN = 15,
    MYLITE_SYSTEM_VARIABLE_LAST_INSERT_ID = 16,
    MYLITE_SYSTEM_VARIABLE_LOWER_CASE_TABLE_NAMES = 17,
    MYLITE_SYSTEM_VARIABLE_MAX_ALLOWED_PACKET = 18,
    MYLITE_SYSTEM_VARIABLE_MAX_CONNECTIONS = 19,
    MYLITE_SYSTEM_VARIABLE_MAX_ERROR_COUNT = 20,
    MYLITE_SYSTEM_VARIABLE_SQL_MODE = 21,
    MYLITE_SYSTEM_VARIABLE_SQL_NOTES = 22,
    MYLITE_SYSTEM_VARIABLE_TIME_ZONE = 23,
    MYLITE_SYSTEM_VARIABLE_TRANSACTION_ISOLATION = 24,
    MYLITE_SYSTEM_VARIABLE_TRANSACTION_READ_ONLY = 25,
    MYLITE_SYSTEM_VARIABLE_UNIQUE_CHECKS = 26,
    MYLITE_SYSTEM_VARIABLE_VERSION = 27,
    MYLITE_SYSTEM_VARIABLE_VERSION_COMMENT = 28,
    MYLITE_SYSTEM_VARIABLE_VERSION_COMPILE_MACHINE = 29,
    MYLITE_SYSTEM_VARIABLE_VERSION_COMPILE_OS = 30,
    MYLITE_SYSTEM_VARIABLE_VERSION_COMPILE_ZLIB = 31,
    MYLITE_SYSTEM_VARIABLE_WAIT_TIMEOUT = 32,
    MYLITE_SYSTEM_VARIABLE_WARNING_COUNT = 33,
    MYLITE_SYSTEM_VARIABLE_GTID_PURGED = 34,
    MYLITE_SYSTEM_VARIABLE_LOG_BIN = 35,
    MYLITE_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS = 36,
    MYLITE_SYSTEM_VARIABLE_SQL_LOG_BIN = 37,
};

enum mylite_system_variable_requested_scope {
    MYLITE_SYSTEM_VARIABLE_SCOPE_DEFAULT = 0,
    MYLITE_SYSTEM_VARIABLE_SCOPE_SESSION = 1,
    MYLITE_SYSTEM_VARIABLE_SCOPE_GLOBAL = 2,
};

enum mylite_system_variable_supported_scope {
    MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH = 0,
    MYLITE_SYSTEM_VARIABLE_SUPPORT_SESSION = 1,
    MYLITE_SYSTEM_VARIABLE_SUPPORT_GLOBAL = 2,
};

enum mylite_system_variable_value_kind {
    MYLITE_SYSTEM_VARIABLE_VALUE_STRING = 0,
    MYLITE_SYSTEM_VARIABLE_VALUE_UNSIGNED = 1,
    MYLITE_SYSTEM_VARIABLE_VALUE_BOOLEAN = 2,
};

static const uint64_t mylite_system_variable_default_max_error_count = 1024U;

struct mylite_system_variable_reference {
    const char *name;
    size_t name_length;
    enum mylite_system_variable_requested_scope requested_scope;
};

struct mylite_system_variable_entry {
    const char *name;
    enum mylite_system_variable_id id;
    enum mylite_system_variable_supported_scope supported_scope;
    enum mylite_system_variable_value_kind value_kind;
};

static int eval_system_variable_entry(
    mylite_db *database,
    const struct mylite_system_variable_reference *reference,
    const struct mylite_system_variable_entry *entry,
    struct mylite_expression_value *out_value
);

static int infer_system_variable_entry(
    const struct mylite_system_variable_entry *entry,
    struct mylite_field_descriptor *out_descriptor
);

static bool parse_system_variable_reference(
    const struct mylite_sql_ast_node *identifier,
    struct mylite_system_variable_reference *out_reference
);

static const struct mylite_system_variable_entry *find_system_variable_entry(
    const struct mylite_system_variable_reference *reference
);

static bool system_variable_scope_is_allowed(
    const struct mylite_system_variable_reference *reference,
    const struct mylite_system_variable_entry *entry
);

static int set_system_variable_scope_error(
    mylite_db *database,
    const struct mylite_system_variable_reference *reference,
    const struct mylite_system_variable_entry *entry
);

static int set_unknown_system_variable_error(
    mylite_db *database,
    const struct mylite_system_variable_reference *reference
);

static enum mylite_system_variable_requested_scope effective_system_variable_scope(
    const struct mylite_system_variable_reference *reference,
    const struct mylite_system_variable_entry *entry
);

static int copy_system_variable_string_value(
    mylite_db *database,
    enum mylite_system_variable_id id,
    enum mylite_system_variable_requested_scope scope,
    struct mylite_expression_value *out_value
);

static uint64_t system_variable_unsigned_value(
    const mylite_db *database,
    enum mylite_system_variable_id id,
    enum mylite_system_variable_requested_scope scope
);

static int64_t system_variable_boolean_value(
    const mylite_db *database,
    enum mylite_system_variable_id id,
    enum mylite_system_variable_requested_scope scope
);

static const char *system_variable_string_value(
    mylite_db *database,
    enum mylite_system_variable_id id,
    enum mylite_system_variable_requested_scope scope,
    struct mylite_schema_default *schema_default
);

static struct mylite_field_descriptor system_variable_string_descriptor(void);

static struct mylite_field_descriptor system_variable_unsigned_descriptor(void);

static struct mylite_field_descriptor system_variable_boolean_descriptor(void);

static int set_system_variable_text_value(
    const char *value,
    struct mylite_expression_value *out_value
);

static int set_system_variable_error(mylite_db *database, unsigned int code, char *message);

static bool span_prefix_match_ci(struct mylite_sql_source_span span, const char *prefix);

static bool span_equal_ci(struct mylite_sql_source_span span, const char *text);

bool mylite_system_variable_identifier_is_system_variable(
    const struct mylite_sql_ast_node *identifier
) {
    if (identifier == NULL || identifier->kind != MYLITE_SQL_AST_IDENTIFIER ||
        identifier->span.length < 2U || identifier->span.text == NULL) {
        return false;
    }
    if (identifier->span.text[0] != '@') {
        return false;
    }
    if (identifier->span.text[1] != '@') {
        return false;
    }
    return true;
}

int mylite_system_variable_eval_identifier(
    mylite_db *database,
    const struct mylite_sql_ast_node *identifier,
    struct mylite_expression_value *out_value
) {
    struct mylite_system_variable_reference reference = {0};
    const struct mylite_system_variable_entry *entry = NULL;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (!parse_system_variable_reference(identifier, &reference)) {
        return -1;
    }

    entry = find_system_variable_entry(&reference);
    if (entry == NULL) {
        return set_unknown_system_variable_error(database, &reference);
    }
    if (!system_variable_scope_is_allowed(&reference, entry)) {
        return set_system_variable_scope_error(database, &reference, entry);
    }
    return eval_system_variable_entry(database, &reference, entry, out_value);
}

int mylite_system_variable_infer_identifier(
    mylite_db *database,
    const struct mylite_sql_ast_node *identifier,
    struct mylite_field_descriptor *out_descriptor
) {
    struct mylite_system_variable_reference reference = {0};
    const struct mylite_system_variable_entry *entry = NULL;

    if (out_descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    if (!parse_system_variable_reference(identifier, &reference)) {
        return MYLITE_UNSUPPORTED;
    }

    entry = find_system_variable_entry(&reference);
    if (entry == NULL) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return set_unknown_system_variable_error(database, &reference);
    }
    if (!system_variable_scope_is_allowed(&reference, entry)) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return set_system_variable_scope_error(database, &reference, entry);
    }
    return infer_system_variable_entry(entry, out_descriptor);
}

static int eval_system_variable_entry(
    mylite_db *database,
    const struct mylite_system_variable_reference *reference,
    const struct mylite_system_variable_entry *entry,
    struct mylite_expression_value *out_value
) {
    enum mylite_system_variable_requested_scope scope =
        effective_system_variable_scope(reference, entry);

    switch (entry->value_kind) {
    case MYLITE_SYSTEM_VARIABLE_VALUE_STRING:
        return copy_system_variable_string_value(database, entry->id, scope, out_value);
    case MYLITE_SYSTEM_VARIABLE_VALUE_UNSIGNED:
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_UINT64,
            .uint64_value = system_variable_unsigned_value(database, entry->id, scope),
        };
        return MYLITE_OK;
    case MYLITE_SYSTEM_VARIABLE_VALUE_BOOLEAN:
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = system_variable_boolean_value(database, entry->id, scope),
        };
        return MYLITE_OK;
    }
    return MYLITE_UNSUPPORTED;
}

static int infer_system_variable_entry(
    const struct mylite_system_variable_entry *entry,
    struct mylite_field_descriptor *out_descriptor
) {
    switch (entry->value_kind) {
    case MYLITE_SYSTEM_VARIABLE_VALUE_STRING:
        *out_descriptor = system_variable_string_descriptor();
        return MYLITE_OK;
    case MYLITE_SYSTEM_VARIABLE_VALUE_UNSIGNED:
        *out_descriptor = system_variable_unsigned_descriptor();
        return MYLITE_OK;
    case MYLITE_SYSTEM_VARIABLE_VALUE_BOOLEAN:
        *out_descriptor = system_variable_boolean_descriptor();
        return MYLITE_OK;
    }
    *out_descriptor = mylite_expression_descriptor_defaults();
    return MYLITE_UNSUPPORTED;
}

static bool parse_system_variable_reference(
    const struct mylite_sql_ast_node *identifier,
    struct mylite_system_variable_reference *out_reference
) {
    struct mylite_sql_source_span tail = {0};

    if (!mylite_system_variable_identifier_is_system_variable(identifier) ||
        out_reference == NULL) {
        return false;
    }

    tail = (struct mylite_sql_source_span){
        .text = identifier->span.text + 2U,
        .length = identifier->span.length - 2U,
    };
    *out_reference = (struct mylite_system_variable_reference){
        .name = tail.text,
        .name_length = tail.length,
        .requested_scope = MYLITE_SYSTEM_VARIABLE_SCOPE_DEFAULT,
    };

    if (span_prefix_match_ci(tail, "global.")) {
        out_reference->name = tail.text + strlen("global.");
        out_reference->name_length = tail.length - strlen("global.");
        out_reference->requested_scope = MYLITE_SYSTEM_VARIABLE_SCOPE_GLOBAL;
    } else if (span_prefix_match_ci(tail, "session.")) {
        out_reference->name = tail.text + strlen("session.");
        out_reference->name_length = tail.length - strlen("session.");
        out_reference->requested_scope = MYLITE_SYSTEM_VARIABLE_SCOPE_SESSION;
    } else if (span_prefix_match_ci(tail, "local.")) {
        out_reference->name = tail.text + strlen("local.");
        out_reference->name_length = tail.length - strlen("local.");
        out_reference->requested_scope = MYLITE_SYSTEM_VARIABLE_SCOPE_SESSION;
    }
    return true;
}

static const struct mylite_system_variable_entry *find_system_variable_entry(
    const struct mylite_system_variable_reference *reference
) {
    static const struct mylite_system_variable_entry entries[] = {
        {"autocommit",
         MYLITE_SYSTEM_VARIABLE_AUTOCOMMIT,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_BOOLEAN},
        {"character_set_client",
         MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"character_set_connection",
         MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_CONNECTION,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"character_set_database",
         MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_DATABASE,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"character_set_filesystem",
         MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_FILESYSTEM,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"character_set_results",
         MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"character_set_server",
         MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_SERVER,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"character_set_system",
         MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_SYSTEM,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_GLOBAL,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"character_sets_dir",
         MYLITE_SYSTEM_VARIABLE_CHARACTER_SETS_DIR,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_GLOBAL,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"collation_connection",
         MYLITE_SYSTEM_VARIABLE_COLLATION_CONNECTION,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"collation_database",
         MYLITE_SYSTEM_VARIABLE_COLLATION_DATABASE,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"collation_server",
         MYLITE_SYSTEM_VARIABLE_COLLATION_SERVER,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"default_storage_engine",
         MYLITE_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"error_count",
         MYLITE_SYSTEM_VARIABLE_ERROR_COUNT,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_SESSION,
         MYLITE_SYSTEM_VARIABLE_VALUE_UNSIGNED},
        {"foreign_key_checks",
         MYLITE_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_BOOLEAN},
        {"gtid_purged",
         MYLITE_SYSTEM_VARIABLE_GTID_PURGED,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_GLOBAL,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"group_concat_max_len",
         MYLITE_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_UNSIGNED},
        {"last_insert_id",
         MYLITE_SYSTEM_VARIABLE_LAST_INSERT_ID,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_SESSION,
         MYLITE_SYSTEM_VARIABLE_VALUE_UNSIGNED},
        {"lower_case_table_names",
         MYLITE_SYSTEM_VARIABLE_LOWER_CASE_TABLE_NAMES,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_GLOBAL,
         MYLITE_SYSTEM_VARIABLE_VALUE_UNSIGNED},
        {"log_bin",
         MYLITE_SYSTEM_VARIABLE_LOG_BIN,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_GLOBAL,
         MYLITE_SYSTEM_VARIABLE_VALUE_BOOLEAN},
        {"log_bin_trust_function_creators",
         MYLITE_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_GLOBAL,
         MYLITE_SYSTEM_VARIABLE_VALUE_BOOLEAN},
        {"max_allowed_packet",
         MYLITE_SYSTEM_VARIABLE_MAX_ALLOWED_PACKET,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_UNSIGNED},
        {"max_connections",
         MYLITE_SYSTEM_VARIABLE_MAX_CONNECTIONS,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_GLOBAL,
         MYLITE_SYSTEM_VARIABLE_VALUE_UNSIGNED},
        {"max_error_count",
         MYLITE_SYSTEM_VARIABLE_MAX_ERROR_COUNT,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_UNSIGNED},
        {"sql_mode",
         MYLITE_SYSTEM_VARIABLE_SQL_MODE,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"sql_log_bin",
         MYLITE_SYSTEM_VARIABLE_SQL_LOG_BIN,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_SESSION,
         MYLITE_SYSTEM_VARIABLE_VALUE_BOOLEAN},
        {"sql_notes",
         MYLITE_SYSTEM_VARIABLE_SQL_NOTES,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_BOOLEAN},
        {"time_zone",
         MYLITE_SYSTEM_VARIABLE_TIME_ZONE,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"transaction_isolation",
         MYLITE_SYSTEM_VARIABLE_TRANSACTION_ISOLATION,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"transaction_read_only",
         MYLITE_SYSTEM_VARIABLE_TRANSACTION_READ_ONLY,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_BOOLEAN},
        {"unique_checks",
         MYLITE_SYSTEM_VARIABLE_UNIQUE_CHECKS,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_BOOLEAN},
        {"version",
         MYLITE_SYSTEM_VARIABLE_VERSION,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_GLOBAL,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"version_comment",
         MYLITE_SYSTEM_VARIABLE_VERSION_COMMENT,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_GLOBAL,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"version_compile_machine",
         MYLITE_SYSTEM_VARIABLE_VERSION_COMPILE_MACHINE,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_GLOBAL,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"version_compile_os",
         MYLITE_SYSTEM_VARIABLE_VERSION_COMPILE_OS,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_GLOBAL,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"version_compile_zlib",
         MYLITE_SYSTEM_VARIABLE_VERSION_COMPILE_ZLIB,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_GLOBAL,
         MYLITE_SYSTEM_VARIABLE_VALUE_STRING},
        {"wait_timeout",
         MYLITE_SYSTEM_VARIABLE_WAIT_TIMEOUT,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_BOTH,
         MYLITE_SYSTEM_VARIABLE_VALUE_UNSIGNED},
        {"warning_count",
         MYLITE_SYSTEM_VARIABLE_WARNING_COUNT,
         MYLITE_SYSTEM_VARIABLE_SUPPORT_SESSION,
         MYLITE_SYSTEM_VARIABLE_VALUE_UNSIGNED},
    };

    for (size_t index = 0U; index < sizeof(entries) / sizeof(entries[0]); ++index) {
        if (span_equal_ci(
                (struct mylite_sql_source_span){
                    .text = reference->name,
                    .length = reference->name_length,
                },
                entries[index].name
            )) {
            return &entries[index];
        }
    }
    return NULL;
}

static bool system_variable_scope_is_allowed(
    const struct mylite_system_variable_reference *reference,
    const struct mylite_system_variable_entry *entry
) {
    if (reference->requested_scope == MYLITE_SYSTEM_VARIABLE_SCOPE_DEFAULT) {
        return true;
    }
    if (reference->requested_scope == MYLITE_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        return entry->supported_scope != MYLITE_SYSTEM_VARIABLE_SUPPORT_SESSION;
    }
    return entry->supported_scope != MYLITE_SYSTEM_VARIABLE_SUPPORT_GLOBAL;
}

static int set_system_variable_scope_error(
    mylite_db *database,
    const struct mylite_system_variable_reference *reference,
    const struct mylite_system_variable_entry *entry
) {
    char *name = mylite_copy_span_text(reference->name, reference->name_length);
    char *message = NULL;

    if (name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    message = sqlite3_mprintf(
        "Variable '%q' is a %s variable",
        name,
        entry->supported_scope == MYLITE_SYSTEM_VARIABLE_SUPPORT_GLOBAL ? "GLOBAL" : "SESSION"
    );
    free(name);
    return set_system_variable_error(database, MYLITE_MYSQL_ER_INCORRECT_GLOBAL_LOCAL_VAR, message);
}

static int set_unknown_system_variable_error(
    mylite_db *database,
    const struct mylite_system_variable_reference *reference
) {
    char *name = mylite_copy_span_text(reference->name, reference->name_length);
    char *message = NULL;

    if (name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    message = sqlite3_mprintf("Unknown system variable '%q'", name);
    free(name);
    return set_system_variable_error(database, MYLITE_MYSQL_ER_UNKNOWN_SYSTEM_VARIABLE, message);
}

static enum mylite_system_variable_requested_scope effective_system_variable_scope(
    const struct mylite_system_variable_reference *reference,
    const struct mylite_system_variable_entry *entry
) {
    if (reference->requested_scope != MYLITE_SYSTEM_VARIABLE_SCOPE_DEFAULT) {
        return reference->requested_scope;
    }
    if (entry->supported_scope == MYLITE_SYSTEM_VARIABLE_SUPPORT_GLOBAL) {
        return MYLITE_SYSTEM_VARIABLE_SCOPE_GLOBAL;
    }
    return MYLITE_SYSTEM_VARIABLE_SCOPE_SESSION;
}

static int copy_system_variable_string_value(
    mylite_db *database,
    enum mylite_system_variable_id id,
    enum mylite_system_variable_requested_scope scope,
    struct mylite_expression_value *out_value
) {
    struct mylite_schema_default schema_default = {
        .character_set = mylite_charset_default_name(),
        .collation = mylite_charset_default_collation_name(),
    };
    int status = MYLITE_OK;
    const char *value = NULL;

    if (database != NULL && scope == MYLITE_SYSTEM_VARIABLE_SCOPE_SESSION &&
        (id == MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_DATABASE ||
         id == MYLITE_SYSTEM_VARIABLE_COLLATION_DATABASE)) {
        status = mylite_catalog_selected_schema_default(database, &schema_default);
        if (status != MYLITE_OK) {
            return status;
        }
    }

    if (id == MYLITE_SYSTEM_VARIABLE_SQL_MODE && scope == MYLITE_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        char *global_sql_mode = NULL;

        status = mylite_connection_copy_global_sql_mode(database, &global_sql_mode);
        if (status != MYLITE_OK) {
            return status;
        }
        status = set_system_variable_text_value(global_sql_mode, out_value);
        free(global_sql_mode);
        return status;
    }

    value = system_variable_string_value(database, id, scope, &schema_default);
    return set_system_variable_text_value(value, out_value);
}

static uint64_t system_variable_unsigned_value(
    const mylite_db *database,
    enum mylite_system_variable_id id,
    enum mylite_system_variable_requested_scope scope
) {
    switch (id) {
    case MYLITE_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN:
        return scope == MYLITE_SYSTEM_VARIABLE_SCOPE_GLOBAL
                   ? mylite_connection_default_group_concat_max_len()
                   : mylite_connection_group_concat_max_len(database);
    case MYLITE_SYSTEM_VARIABLE_LAST_INSERT_ID:
        return database == NULL ? 0U : database->last_insert_id;
    case MYLITE_SYSTEM_VARIABLE_LOWER_CASE_TABLE_NAMES:
        return 0U;
    case MYLITE_SYSTEM_VARIABLE_MAX_ALLOWED_PACKET:
        return mylite_connection_default_max_allowed_packet();
    case MYLITE_SYSTEM_VARIABLE_MAX_CONNECTIONS:
        return mylite_connection_default_max_connections();
    case MYLITE_SYSTEM_VARIABLE_MAX_ERROR_COUNT:
        return mylite_system_variable_default_max_error_count;
    case MYLITE_SYSTEM_VARIABLE_WAIT_TIMEOUT:
        return scope == MYLITE_SYSTEM_VARIABLE_SCOPE_GLOBAL
                   ? mylite_connection_default_wait_timeout()
                   : mylite_connection_wait_timeout(database);
    case MYLITE_SYSTEM_VARIABLE_WARNING_COUNT:
    case MYLITE_SYSTEM_VARIABLE_ERROR_COUNT:
        return 0U;
    case MYLITE_SYSTEM_VARIABLE_AUTOCOMMIT:
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT:
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_CONNECTION:
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_DATABASE:
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_FILESYSTEM:
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS:
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_SERVER:
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_SYSTEM:
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SETS_DIR:
    case MYLITE_SYSTEM_VARIABLE_COLLATION_CONNECTION:
    case MYLITE_SYSTEM_VARIABLE_COLLATION_DATABASE:
    case MYLITE_SYSTEM_VARIABLE_COLLATION_SERVER:
    case MYLITE_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE:
    case MYLITE_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS:
    case MYLITE_SYSTEM_VARIABLE_GTID_PURGED:
    case MYLITE_SYSTEM_VARIABLE_LOG_BIN:
    case MYLITE_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS:
    case MYLITE_SYSTEM_VARIABLE_SQL_MODE:
    case MYLITE_SYSTEM_VARIABLE_SQL_NOTES:
    case MYLITE_SYSTEM_VARIABLE_SQL_LOG_BIN:
    case MYLITE_SYSTEM_VARIABLE_TIME_ZONE:
    case MYLITE_SYSTEM_VARIABLE_TRANSACTION_ISOLATION:
    case MYLITE_SYSTEM_VARIABLE_TRANSACTION_READ_ONLY:
    case MYLITE_SYSTEM_VARIABLE_UNIQUE_CHECKS:
    case MYLITE_SYSTEM_VARIABLE_VERSION:
    case MYLITE_SYSTEM_VARIABLE_VERSION_COMMENT:
    case MYLITE_SYSTEM_VARIABLE_VERSION_COMPILE_MACHINE:
    case MYLITE_SYSTEM_VARIABLE_VERSION_COMPILE_OS:
    case MYLITE_SYSTEM_VARIABLE_VERSION_COMPILE_ZLIB:
        break;
    }
    return 0U;
}

static int64_t system_variable_boolean_value(
    const mylite_db *database,
    enum mylite_system_variable_id id,
    enum mylite_system_variable_requested_scope scope
) {
    switch (id) {
    case MYLITE_SYSTEM_VARIABLE_AUTOCOMMIT:
        return 1;
    case MYLITE_SYSTEM_VARIABLE_SQL_NOTES:
        if (scope == MYLITE_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            if (mylite_connection_default_sql_notes()) {
                return 1;
            }
            return 0;
        }
        if (mylite_connection_sql_notes(database)) {
            return 1;
        }
        return 0;
    case MYLITE_SYSTEM_VARIABLE_SQL_LOG_BIN:
        if (mylite_connection_sql_log_bin(database)) {
            return 1;
        }
        return 0;
    case MYLITE_SYSTEM_VARIABLE_LOG_BIN:
    case MYLITE_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS:
        return 0;
    case MYLITE_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS:
        if (scope == MYLITE_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            if (mylite_connection_default_foreign_key_checks()) {
                return 1;
            }
            return 0;
        }
        if (mylite_connection_foreign_key_checks(database)) {
            return 1;
        }
        return 0;
    case MYLITE_SYSTEM_VARIABLE_TRANSACTION_READ_ONLY:
        return 0;
    case MYLITE_SYSTEM_VARIABLE_UNIQUE_CHECKS:
        if (scope == MYLITE_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            if (mylite_connection_default_unique_checks()) {
                return 1;
            }
            return 0;
        }
        if (mylite_connection_unique_checks(database)) {
            return 1;
        }
        return 0;
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT:
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_CONNECTION:
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_DATABASE:
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_FILESYSTEM:
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS:
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_SERVER:
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_SYSTEM:
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SETS_DIR:
    case MYLITE_SYSTEM_VARIABLE_COLLATION_CONNECTION:
    case MYLITE_SYSTEM_VARIABLE_COLLATION_DATABASE:
    case MYLITE_SYSTEM_VARIABLE_COLLATION_SERVER:
    case MYLITE_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE:
    case MYLITE_SYSTEM_VARIABLE_ERROR_COUNT:
    case MYLITE_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN:
    case MYLITE_SYSTEM_VARIABLE_GTID_PURGED:
    case MYLITE_SYSTEM_VARIABLE_LAST_INSERT_ID:
    case MYLITE_SYSTEM_VARIABLE_LOWER_CASE_TABLE_NAMES:
    case MYLITE_SYSTEM_VARIABLE_MAX_ALLOWED_PACKET:
    case MYLITE_SYSTEM_VARIABLE_MAX_CONNECTIONS:
    case MYLITE_SYSTEM_VARIABLE_MAX_ERROR_COUNT:
    case MYLITE_SYSTEM_VARIABLE_SQL_MODE:
    case MYLITE_SYSTEM_VARIABLE_TIME_ZONE:
    case MYLITE_SYSTEM_VARIABLE_TRANSACTION_ISOLATION:
    case MYLITE_SYSTEM_VARIABLE_VERSION:
    case MYLITE_SYSTEM_VARIABLE_VERSION_COMMENT:
    case MYLITE_SYSTEM_VARIABLE_VERSION_COMPILE_MACHINE:
    case MYLITE_SYSTEM_VARIABLE_VERSION_COMPILE_OS:
    case MYLITE_SYSTEM_VARIABLE_VERSION_COMPILE_ZLIB:
    case MYLITE_SYSTEM_VARIABLE_WAIT_TIMEOUT:
    case MYLITE_SYSTEM_VARIABLE_WARNING_COUNT:
        break;
    }
    return 0;
}

static const char *system_variable_string_value(
    mylite_db *database,
    enum mylite_system_variable_id id,
    enum mylite_system_variable_requested_scope scope,
    struct mylite_schema_default *schema_default
) {
    const bool global = scope == MYLITE_SYSTEM_VARIABLE_SCOPE_GLOBAL;

    switch (id) {
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT:
        return global || database == NULL ? mylite_charset_default_name()
                                          : database->character_set_client;
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_CONNECTION:
        return global || database == NULL ? mylite_charset_default_name()
                                          : database->character_set_connection;
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_DATABASE:
        return global || schema_default == NULL ? mylite_charset_default_name()
                                                : schema_default->character_set;
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_FILESYSTEM:
        return mylite_mysql_binary_charset_name;
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS:
        return global || database == NULL ? mylite_charset_default_name()
                                          : database->character_set_results;
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_SERVER:
        return mylite_charset_default_name();
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SET_SYSTEM:
        return mylite_mysql_utf8mb3_charset_name;
    case MYLITE_SYSTEM_VARIABLE_CHARACTER_SETS_DIR:
        return "";
    case MYLITE_SYSTEM_VARIABLE_COLLATION_CONNECTION:
        return global || database == NULL ? mylite_charset_default_collation_name()
                                          : database->collation_connection;
    case MYLITE_SYSTEM_VARIABLE_COLLATION_DATABASE:
        return global || schema_default == NULL ? mylite_charset_default_collation_name()
                                                : schema_default->collation;
    case MYLITE_SYSTEM_VARIABLE_COLLATION_SERVER:
        return mylite_charset_default_collation_name();
    case MYLITE_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE:
        if (global) {
            return mylite_connection_default_storage_engine();
        }
        return mylite_connection_storage_engine(database);
    case MYLITE_SYSTEM_VARIABLE_GTID_PURGED:
        return "";
    case MYLITE_SYSTEM_VARIABLE_SQL_MODE:
        if (global) {
            return mylite_connection_default_sql_mode();
        }
        return mylite_connection_sql_mode(database);
    case MYLITE_SYSTEM_VARIABLE_TIME_ZONE:
        if (global) {
            return mylite_connection_default_time_zone();
        }
        return mylite_connection_time_zone(database);
    case MYLITE_SYSTEM_VARIABLE_TRANSACTION_ISOLATION:
        return "REPEATABLE-READ";
    case MYLITE_SYSTEM_VARIABLE_VERSION:
        return mylite_mysql_compatibility_version;
    case MYLITE_SYSTEM_VARIABLE_VERSION_COMMENT:
        return "MyLite";
    case MYLITE_SYSTEM_VARIABLE_VERSION_COMPILE_MACHINE:
    case MYLITE_SYSTEM_VARIABLE_VERSION_COMPILE_OS:
    case MYLITE_SYSTEM_VARIABLE_VERSION_COMPILE_ZLIB:
        return "";
    case MYLITE_SYSTEM_VARIABLE_AUTOCOMMIT:
    case MYLITE_SYSTEM_VARIABLE_ERROR_COUNT:
    case MYLITE_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS:
    case MYLITE_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN:
    case MYLITE_SYSTEM_VARIABLE_LAST_INSERT_ID:
    case MYLITE_SYSTEM_VARIABLE_LOWER_CASE_TABLE_NAMES:
    case MYLITE_SYSTEM_VARIABLE_LOG_BIN:
    case MYLITE_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS:
    case MYLITE_SYSTEM_VARIABLE_MAX_ALLOWED_PACKET:
    case MYLITE_SYSTEM_VARIABLE_MAX_CONNECTIONS:
    case MYLITE_SYSTEM_VARIABLE_MAX_ERROR_COUNT:
    case MYLITE_SYSTEM_VARIABLE_SQL_LOG_BIN:
    case MYLITE_SYSTEM_VARIABLE_SQL_NOTES:
    case MYLITE_SYSTEM_VARIABLE_TRANSACTION_READ_ONLY:
    case MYLITE_SYSTEM_VARIABLE_UNIQUE_CHECKS:
    case MYLITE_SYSTEM_VARIABLE_WAIT_TIMEOUT:
    case MYLITE_SYSTEM_VARIABLE_WARNING_COUNT:
        break;
    }
    return "";
}

static struct mylite_field_descriptor system_variable_string_descriptor(void) {
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .length = mylite_mysql_system_variable_string_display_length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_latin1_swedish_ci_charset_id,
        .nullable = true,
    };
}

static struct mylite_field_descriptor system_variable_unsigned_descriptor(void) {
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_LONGLONG,
        .flags = MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = mylite_mysql_system_variable_integer_display_length,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };
}

static struct mylite_field_descriptor system_variable_boolean_descriptor(void) {
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_LONGLONG,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = 1U,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };
}

static int set_system_variable_text_value(
    const char *value,
    struct mylite_expression_value *out_value
) {
    size_t value_length = 0U;
    char *copy = NULL;

    if (value == NULL) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return MYLITE_OK;
    }

    value_length = strlen(value);
    copy = mylite_copy_span_text(value, value_length);
    if (copy == NULL) {
        return MYLITE_NOMEM;
    }
    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_TEXT,
        .text_value = copy,
        .text_length = value_length,
    };
    return MYLITE_OK;
}

static int set_system_variable_error(mylite_db *database, unsigned int code, char *message) {
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(database, code, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static bool span_prefix_match_ci(struct mylite_sql_source_span span, const char *prefix) {
    size_t prefix_length = strlen(prefix);

    if (span.length < prefix_length) {
        return false;
    }
    return span_equal_ci(
        (struct mylite_sql_source_span){
            .text = span.text,
            .length = prefix_length,
        },
        prefix
    );
}

static bool span_equal_ci(struct mylite_sql_source_span span, const char *text) {
    size_t text_length = strlen(text);

    if (span.text == NULL || span.length != text_length) {
        return false;
    }
    for (size_t index = 0U; index < span.length; ++index) {
        unsigned char left = (unsigned char)span.text[index];
        unsigned char right = (unsigned char)text[index];

        if (left >= 'A' && left <= 'Z') {
            left = (unsigned char)(left - 'A' + 'a');
        }
        if (right >= 'A' && right <= 'Z') {
            right = (unsigned char)(right - 'A' + 'a');
        }
        if (left != right) {
            return false;
        }
    }
    return true;
}
