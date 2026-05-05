#include "mylite_connection.h"

#include "mylite_catalog.h"
#include "mylite_charset.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_expression.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_transactions.h"
#include "mylite_vfs.h"
#include "sqlite3.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static const uint64_t mylite_embedded_connection_id = 1U;
static const char mylite_default_sql_mode[] =
    "ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,"
    "ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION";

enum mylite_sql_mode_mask {
    MYLITE_SQL_MODE_REAL_AS_FLOAT = 1U << 0U,
    MYLITE_SQL_MODE_PIPES_AS_CONCAT = 1U << 1U,
    MYLITE_SQL_MODE_ANSI_QUOTES = 1U << 2U,
    MYLITE_SQL_MODE_IGNORE_SPACE = 1U << 3U,
    MYLITE_SQL_MODE_ONLY_FULL_GROUP_BY = 1U << 4U,
    MYLITE_SQL_MODE_NO_UNSIGNED_SUBTRACTION = 1U << 5U,
    MYLITE_SQL_MODE_ANSI = 1U << 6U,
    MYLITE_SQL_MODE_NO_BACKSLASH_ESCAPES = 1U << 7U,
    MYLITE_SQL_MODE_STRICT_TRANS_TABLES = 1U << 8U,
    MYLITE_SQL_MODE_STRICT_ALL_TABLES = 1U << 9U,
    MYLITE_SQL_MODE_NO_ZERO_IN_DATE = 1U << 10U,
    MYLITE_SQL_MODE_NO_ZERO_DATE = 1U << 11U,
    MYLITE_SQL_MODE_INVALID_DATES = 1U << 12U,
    MYLITE_SQL_MODE_ERROR_FOR_DIVISION_BY_ZERO = 1U << 13U,
    MYLITE_SQL_MODE_TRADITIONAL = 1U << 14U,
    MYLITE_SQL_MODE_HIGH_NOT_PRECEDENCE = 1U << 15U,
    MYLITE_SQL_MODE_NO_ENGINE_SUBSTITUTION = 1U << 16U,
    MYLITE_SQL_MODE_PAD_CHAR_TO_FULL_LENGTH = 1U << 17U,
    MYLITE_SQL_MODE_TIME_TRUNCATE_FRACTIONAL = 1U << 18U,
};

struct mylite_sql_mode_entry {
    const char *name;
    uint64_t mask;
};

struct mylite_sql_mode_token_search {
    const char *sql_mode;
    const char *expected;
};

static int open_sqlite_database(const char *filename, int flags, const char *vfs_name,
                                mylite_db **out_db);
static int copy_canonical_sql_mode(mylite_db *database, const char *sql_mode, char **out_value);
static int append_sql_mode_token(mylite_db *database, char **out_value, size_t *out_length,
                                 const char *name);
static int set_invalid_sql_mode_error(mylite_db *database, const char *value, size_t value_length);
static const struct mylite_sql_mode_entry *find_sql_mode_entry(const char *text, size_t length);
static bool sql_mode_token_matches(const char *text, size_t length, const char *expected);
static bool sql_mode_contains_token(struct mylite_sql_mode_token_search search);
static bool ascii_is_space(char byte);

int mylite_open(const char *filename, mylite_db **out_db)
{
    int rc = SQLITE_OK;

    if (filename == NULL || out_db == NULL) {
        return MYLITE_MISUSE;
    }

    *out_db = NULL;
    rc = mylite_vfs_register();
    if (rc != SQLITE_OK) {
        return MYLITE_SQLITE_ERROR;
    }

    return open_sqlite_database(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                                mylite_vfs_name(), out_db);
}

int mylite_open_memory(mylite_db **out_db)
{
    if (out_db == NULL) {
        return MYLITE_MISUSE;
    }

    return open_sqlite_database(
        ":memory:", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY, NULL, out_db);
}

void mylite_close(mylite_db *database)
{
    if (database == NULL) {
        return;
    }

    if (database->transaction_active) {
        (void)mylite_transaction_rollback_explicit(database);
    }
    sqlite3_close(database->sqlite);
    free(database->error_message);
    mylite_expression_warnings_deinit(&database->warnings);
    free(database->selected_schema);
    free(database->sql_mode);
    mylite_transaction_savepoint_state_deinit(&database->savepoints);
    mylite_transaction_clear_pending_auto_increments(database);
    free(database);
}

uint64_t mylite_last_insert_id(const mylite_db *database)
{
    if (database == NULL) {
        return 0U;
    }

    return database->last_insert_id;
}

const char *mylite_connection_character_set_client(const mylite_db *database)
{
    return database == NULL ? NULL : database->character_set_client;
}

const char *mylite_connection_character_set_connection(const mylite_db *database)
{
    return database == NULL ? NULL : database->character_set_connection;
}

const char *mylite_connection_character_set_results(const mylite_db *database)
{
    return database == NULL ? NULL : database->character_set_results;
}

const char *mylite_connection_collation_connection(const mylite_db *database)
{
    return database == NULL ? NULL : database->collation_connection;
}

int mylite_connection_set_default_state(mylite_db *database)
{
    database->character_set_client = mylite_charset_default_name();
    database->character_set_connection = mylite_charset_default_name();
    database->character_set_results = mylite_charset_default_name();
    database->collation_connection = mylite_charset_default_collation_name();
    return MYLITE_OK;
}

int mylite_connection_set_released_error(mylite_db *database)
{
    int status = mylite_diagnostics_set_error_message(
        database, "Connection was released by transaction completion");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_connection_set_selected_schema(mylite_db *database, const char *schema_name)
{
    char *copy = mylite_copy_span_text(schema_name, strlen(schema_name));

    if (copy == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    free(database->selected_schema);
    database->selected_schema = copy;
    return MYLITE_OK;
}

void mylite_connection_clear_selected_schema_if_matches(mylite_db *database,
                                                        const char *schema_name)
{
    if (database->selected_schema != NULL && strcmp(database->selected_schema, schema_name) == 0) {
        free(database->selected_schema);
        database->selected_schema = NULL;
    }
}

int mylite_connection_set_names_state(mylite_db *database,
                                      struct mylite_connection_names_state state)
{
    const struct mylite_charset *character_set = mylite_charset_lookup(state.character_set_name);
    const struct mylite_collation *collation = NULL;

    if (character_set == NULL) {
        return mylite_diagnostics_set_unknown_charset_error(database, state.character_set_name);
    }

    if (state.collation_name == NULL) {
        collation = mylite_collation_lookup(character_set->default_collation);
    } else {
        collation = mylite_collation_lookup(state.collation_name);
        if (collation == NULL) {
            return mylite_diagnostics_set_unknown_collation_error(database, state.collation_name);
        }
        if (!mylite_charset_collation_match(character_set, collation)) {
            return mylite_diagnostics_set_collation_charset_error(database, collation->name,
                                                                  character_set->name);
        }
    }

    database->character_set_client = character_set->name;
    database->character_set_connection = character_set->name;
    database->character_set_results = character_set->name;
    database->collation_connection = collation->name;
    return MYLITE_OK;
}

int mylite_connection_set_character_set_state(mylite_db *database, const char *character_set_name)
{
    struct mylite_schema_default schema_default;
    const struct mylite_charset *character_set = mylite_charset_lookup(character_set_name);
    const struct mylite_collation *connection_collation = NULL;
    int status = MYLITE_OK;

    if (character_set == NULL) {
        return mylite_diagnostics_set_unknown_charset_error(database, character_set_name);
    }

    status = mylite_catalog_selected_schema_default(database, &schema_default);
    if (status != MYLITE_OK) {
        return status;
    }

    connection_collation = mylite_collation_lookup(schema_default.collation);
    if (connection_collation == NULL) {
        return mylite_diagnostics_set_unknown_collation_error(database, schema_default.collation);
    }

    database->character_set_client = character_set->name;
    database->character_set_connection = connection_collation->character_set;
    database->character_set_results = character_set->name;
    database->collation_connection = connection_collation->name;
    return MYLITE_OK;
}

int mylite_connection_set_default_sql_mode(mylite_db *database)
{
    return mylite_connection_set_sql_mode(database, mylite_default_sql_mode);
}

int mylite_connection_set_sql_mode(mylite_db *database, const char *sql_mode)
{
    char *canonical = NULL;
    int status = copy_canonical_sql_mode(database, sql_mode, &canonical);

    if (status != MYLITE_OK) {
        return status;
    }

    free(database->sql_mode);
    database->sql_mode = canonical;
    return MYLITE_OK;
}

const char *mylite_connection_default_sql_mode(void)
{
    return mylite_default_sql_mode;
}

const char *mylite_connection_sql_mode(const mylite_db *database)
{
    return database == NULL || database->sql_mode == NULL ? mylite_default_sql_mode
                                                          : database->sql_mode;
}

bool mylite_connection_sql_mode_has_only_full_group_by(const mylite_db *database)
{
    return sql_mode_contains_token((struct mylite_sql_mode_token_search){
        .sql_mode = mylite_connection_sql_mode(database),
        .expected = "ONLY_FULL_GROUP_BY",
    });
}

static int open_sqlite_database(const char *filename, int flags, const char *vfs_name,
                                mylite_db **out_db)
{
    mylite_db *database = calloc(1U, sizeof(*database));
    int rc = SQLITE_OK;

    *out_db = NULL;
    if (database == NULL) {
        return MYLITE_NOMEM;
    }
    database->status_started_at = time(NULL);
    database->connection_id = mylite_embedded_connection_id;

    rc = sqlite3_open_v2(filename, &database->sqlite, flags, vfs_name);
    if (rc != SQLITE_OK) {
        sqlite3_close(database->sqlite);
        free(database);
        return MYLITE_SQLITE_ERROR;
    }

    rc = mylite_catalog_initialize(database);
    if (rc != MYLITE_OK) {
        sqlite3_close(database->sqlite);
        free(database->error_message);
        free(database);
        return rc;
    }

    (void)mylite_connection_set_default_state(database);
    rc = mylite_connection_set_default_sql_mode(database);
    if (rc != MYLITE_OK) {
        sqlite3_close(database->sqlite);
        free(database->error_message);
        mylite_expression_warnings_deinit(&database->warnings);
        free(database->selected_schema);
        free(database->sql_mode);
        free(database);
        return rc;
    }
    *out_db = database;
    return MYLITE_OK;
}

static int copy_canonical_sql_mode(mylite_db *database, const char *sql_mode, char **out_value)
{
    static const struct mylite_sql_mode_entry canonical_order[] = {
        {"REAL_AS_FLOAT", MYLITE_SQL_MODE_REAL_AS_FLOAT},
        {"PIPES_AS_CONCAT", MYLITE_SQL_MODE_PIPES_AS_CONCAT},
        {"ANSI_QUOTES", MYLITE_SQL_MODE_ANSI_QUOTES},
        {"IGNORE_SPACE", MYLITE_SQL_MODE_IGNORE_SPACE},
        {"ONLY_FULL_GROUP_BY", MYLITE_SQL_MODE_ONLY_FULL_GROUP_BY},
        {"NO_UNSIGNED_SUBTRACTION", MYLITE_SQL_MODE_NO_UNSIGNED_SUBTRACTION},
        {"ANSI", MYLITE_SQL_MODE_ANSI},
        {"NO_BACKSLASH_ESCAPES", MYLITE_SQL_MODE_NO_BACKSLASH_ESCAPES},
        {"STRICT_TRANS_TABLES", MYLITE_SQL_MODE_STRICT_TRANS_TABLES},
        {"STRICT_ALL_TABLES", MYLITE_SQL_MODE_STRICT_ALL_TABLES},
        {"NO_ZERO_IN_DATE", MYLITE_SQL_MODE_NO_ZERO_IN_DATE},
        {"NO_ZERO_DATE", MYLITE_SQL_MODE_NO_ZERO_DATE},
        {"ALLOW_INVALID_DATES", MYLITE_SQL_MODE_INVALID_DATES},
        {"ERROR_FOR_DIVISION_BY_ZERO", MYLITE_SQL_MODE_ERROR_FOR_DIVISION_BY_ZERO},
        {"TRADITIONAL", MYLITE_SQL_MODE_TRADITIONAL},
        {"HIGH_NOT_PRECEDENCE", MYLITE_SQL_MODE_HIGH_NOT_PRECEDENCE},
        {"NO_ENGINE_SUBSTITUTION", MYLITE_SQL_MODE_NO_ENGINE_SUBSTITUTION},
        {"PAD_CHAR_TO_FULL_LENGTH", MYLITE_SQL_MODE_PAD_CHAR_TO_FULL_LENGTH},
        {"TIME_TRUNCATE_FRACTIONAL", MYLITE_SQL_MODE_TIME_TRUNCATE_FRACTIONAL},
    };
    uint64_t mask = 0U;
    char *value = NULL;
    size_t value_length = 0U;
    const char *token_start = sql_mode == NULL ? "" : sql_mode;

    while (token_start != NULL) {
        const char *token_end = strchr(token_start, ',');
        const struct mylite_sql_mode_entry *entry = NULL;
        size_t token_length =
            token_end == NULL ? strlen(token_start) : (size_t)(token_end - token_start);

        while (token_length > 0U && ascii_is_space(token_start[0])) {
            ++token_start;
            --token_length;
        }
        while (token_length > 0U && ascii_is_space(token_start[token_length - 1U])) {
            --token_length;
        }
        if (token_length > 0U) {
            entry = find_sql_mode_entry(token_start, token_length);
            if (entry == NULL) {
                return set_invalid_sql_mode_error(database, token_start, token_length);
            }
            mask |= entry->mask;
        }

        token_start = token_end == NULL ? NULL : token_end + 1;
    }

    for (size_t index = 0U; index < sizeof(canonical_order) / sizeof(canonical_order[0]); ++index) {
        if ((mask & canonical_order[index].mask) == 0U) {
            continue;
        }
        if (append_sql_mode_token(database, &value, &value_length, canonical_order[index].name) !=
            MYLITE_OK) {
            free(value);
            return MYLITE_NOMEM;
        }
    }

    if (value == NULL) {
        value = mylite_copy_span_text("", 0U);
        if (value == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
    }

    *out_value = value;
    return MYLITE_OK;
}

static int append_sql_mode_token(mylite_db *database, char **out_value, size_t *out_length,
                                 const char *name)
{
    size_t name_length = strlen(name);
    size_t separator_length = *out_length == 0U ? 0U : 1U;
    size_t new_length = *out_length + separator_length + name_length;
    char *value = realloc(*out_value, new_length + 1U);

    if (value == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (separator_length > 0U) {
        value[*out_length] = ',';
    }
    memcpy(value + *out_length + separator_length, name, name_length);
    value[new_length] = '\0';

    *out_value = value;
    *out_length = new_length;
    return MYLITE_OK;
}

static int set_invalid_sql_mode_error(mylite_db *database, const char *value, size_t value_length)
{
    char *token = mylite_copy_span_text(value, value_length);
    char *message = NULL;
    int status = MYLITE_OK;

    if (token == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    message = sqlite3_mprintf("Variable 'sql_mode' can't be set to the value of '%q'", token);
    free(token);
    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status =
            mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_WRONG_VALUE_FOR_VAR, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static const struct mylite_sql_mode_entry *find_sql_mode_entry(const char *text, size_t length)
{
    static const struct mylite_sql_mode_entry modes[] = {
        {"ALLOW_INVALID_DATES", MYLITE_SQL_MODE_INVALID_DATES},
        {"ANSI", MYLITE_SQL_MODE_REAL_AS_FLOAT | MYLITE_SQL_MODE_PIPES_AS_CONCAT |
                     MYLITE_SQL_MODE_ANSI_QUOTES | MYLITE_SQL_MODE_IGNORE_SPACE |
                     MYLITE_SQL_MODE_ONLY_FULL_GROUP_BY | MYLITE_SQL_MODE_ANSI},
        {"ANSI_QUOTES", MYLITE_SQL_MODE_ANSI_QUOTES},
        {"ERROR_FOR_DIVISION_BY_ZERO", MYLITE_SQL_MODE_ERROR_FOR_DIVISION_BY_ZERO},
        {"HIGH_NOT_PRECEDENCE", MYLITE_SQL_MODE_HIGH_NOT_PRECEDENCE},
        {"IGNORE_SPACE", MYLITE_SQL_MODE_IGNORE_SPACE},
        {"NO_BACKSLASH_ESCAPES", MYLITE_SQL_MODE_NO_BACKSLASH_ESCAPES},
        {"NO_ENGINE_SUBSTITUTION", MYLITE_SQL_MODE_NO_ENGINE_SUBSTITUTION},
        {"NO_UNSIGNED_SUBTRACTION", MYLITE_SQL_MODE_NO_UNSIGNED_SUBTRACTION},
        {"NO_ZERO_DATE", MYLITE_SQL_MODE_NO_ZERO_DATE},
        {"NO_ZERO_IN_DATE", MYLITE_SQL_MODE_NO_ZERO_IN_DATE},
        {"ONLY_FULL_GROUP_BY", MYLITE_SQL_MODE_ONLY_FULL_GROUP_BY},
        {"PAD_CHAR_TO_FULL_LENGTH", MYLITE_SQL_MODE_PAD_CHAR_TO_FULL_LENGTH},
        {"PIPES_AS_CONCAT", MYLITE_SQL_MODE_PIPES_AS_CONCAT},
        {"REAL_AS_FLOAT", MYLITE_SQL_MODE_REAL_AS_FLOAT},
        {"STRICT_ALL_TABLES", MYLITE_SQL_MODE_STRICT_ALL_TABLES},
        {"STRICT_TRANS_TABLES", MYLITE_SQL_MODE_STRICT_TRANS_TABLES},
        {"TIME_TRUNCATE_FRACTIONAL", MYLITE_SQL_MODE_TIME_TRUNCATE_FRACTIONAL},
        {"TRADITIONAL", MYLITE_SQL_MODE_STRICT_TRANS_TABLES | MYLITE_SQL_MODE_STRICT_ALL_TABLES |
                            MYLITE_SQL_MODE_NO_ZERO_IN_DATE | MYLITE_SQL_MODE_NO_ZERO_DATE |
                            MYLITE_SQL_MODE_ERROR_FOR_DIVISION_BY_ZERO |
                            MYLITE_SQL_MODE_TRADITIONAL | MYLITE_SQL_MODE_NO_ENGINE_SUBSTITUTION},
    };

    for (size_t index = 0U; index < sizeof(modes) / sizeof(modes[0]); ++index) {
        if (sql_mode_token_matches(text, length, modes[index].name)) {
            return &modes[index];
        }
    }
    return NULL;
}

static bool sql_mode_token_matches(const char *text, size_t length, const char *expected)
{
    for (size_t index = 0U;; ++index) {
        char left = index < length ? text[index] : '\0';
        char right = expected[index];

        if (left >= 'a' && left <= 'z') {
            left = (char)(left - 'a' + 'A');
        }
        if (right >= 'a' && right <= 'z') {
            right = (char)(right - 'a' + 'A');
        }
        if (left != right) {
            return false;
        }
        if (left == '\0') {
            return true;
        }
    }
}

static bool sql_mode_contains_token(struct mylite_sql_mode_token_search search)
{
    const char *token_start = search.sql_mode == NULL ? "" : search.sql_mode;

    while (token_start != NULL) {
        const char *token_end = strchr(token_start, ',');
        size_t token_length =
            token_end == NULL ? strlen(token_start) : (size_t)(token_end - token_start);

        if (sql_mode_token_matches(token_start, token_length, search.expected)) {
            return true;
        }
        token_start = token_end == NULL ? NULL : token_end + 1;
    }
    return false;
}

static bool ascii_is_space(char byte)
{
    if (byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r' || byte == '\f') {
        return true;
    }
    if (byte == '\v') {
        return true;
    }
    return false;
}
