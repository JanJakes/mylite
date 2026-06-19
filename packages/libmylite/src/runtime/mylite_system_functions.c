#include "mylite_system_functions.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_dynamic_string.h"
#include "mylite_mysql_error_codes.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    xml_initial_node_capacity = 16,
    xml_initial_stack_capacity = 8,
    xpath_max_segments = 32,
    xpath_error_preview_capacity = 96,
    sleep_milliseconds_per_second = 1000,
    sleep_minimum_milliseconds = 1,
};

struct system_xml_node {
    int parent;
    size_t start;
    size_t name_start;
    size_t name_size;
    size_t content_start;
    size_t content_end;
    size_t end;
};

struct system_xml_document {
    const char *text;
    size_t size;
    struct system_xml_node *nodes;
    size_t node_count;
    size_t node_capacity;
    int *stack;
    size_t stack_count;
    size_t stack_capacity;
};

struct system_xpath_segment {
    const char *text;
    size_t size;
};

struct system_xpath {
    bool descendant;
    struct system_xpath_segment segments[xpath_max_segments];
    size_t segment_count;
};

struct system_sqlite_text_argument {
    const char *text;
    size_t text_size;
    bool is_null;
};

static void sleep_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void load_file_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void extract_value_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void update_xml_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static bool system_sql_mode_is_strict(const struct mylite_db *database);
static int sleep_platform_seconds(double seconds);
static int parse_sqlite_sleep_argument(
    struct mylite_db *database,
    sqlite3_value *value,
    double *out_seconds,
    bool *out_invalid,
    bool *out_skip_sleep
);
static int parse_double_text(
    struct mylite_db *database,
    const char *text,
    size_t text_size,
    double *out_value,
    bool *out_valid
);
static int sqlite_text_argument(
    sqlite3_value *value,
    struct system_sqlite_text_argument *out_argument
);
static void sqlite_system_result_error(
    sqlite3_context *context,
    int rc,
    struct mylite_db *database
);
static int append_sleep_incorrect_argument_warning(struct mylite_db *database);
static int append_xml_warning(struct mylite_db *database);
static void set_sleep_incorrect_argument_error(struct mylite_db *database);
static void set_xpath_syntax_error(
    struct mylite_db *database,
    const char *xpath,
    size_t xpath_size
);
static void set_system_nomem_error(struct mylite_db *database, const char *message);
static int duplicate_bytes(
    struct mylite_db *database,
    const char *text,
    size_t text_size,
    char **out_text
);
static int parse_xml_document(
    struct mylite_db *database,
    const char *xml,
    size_t xml_size,
    struct system_xml_document *out_document,
    bool *out_valid
);
static void xml_document_deinit(struct system_xml_document *document);
static int xml_document_append_node(
    struct mylite_db *database,
    struct system_xml_document *document,
    struct system_xml_node node,
    int *out_index
);
static int xml_document_push_node(
    struct mylite_db *database,
    struct system_xml_document *document,
    int node_index
);
static bool parse_xml_start_tag(
    struct mylite_db *database,
    struct system_xml_document *document,
    size_t *inout_index,
    bool *out_valid
);
static bool parse_xml_end_tag(
    struct system_xml_document *document,
    size_t *inout_index,
    bool *out_valid
);
static bool skip_xml_processing_instruction(
    const struct system_xml_document *document,
    size_t *inout_index,
    bool *out_valid
);
static bool skip_xml_comment(
    const struct system_xml_document *document,
    size_t *inout_index,
    bool *out_valid
);
static bool skip_xml_declaration(
    const struct system_xml_document *document,
    size_t *inout_index,
    bool *out_valid
);
static bool skip_xml_tag_tail(
    const struct system_xml_document *document,
    size_t *inout_index,
    bool *out_self_closing,
    bool *out_valid
); // NOLINT(bugprone-easily-swappable-parameters)
static size_t scan_xml_name(
    const char *text,
    size_t size,
    size_t index
); // NOLINT(bugprone-easily-swappable-parameters)
static bool xml_name_byte_is_supported(char byte);
static bool parse_xpath(
    struct mylite_db *database,
    const char *xpath,
    size_t xpath_size,
    struct system_xpath *out_path
);
static bool xpath_node_matches(
    const struct system_xml_document *document,
    const struct system_xml_node *node,
    const struct system_xpath *path
);
static bool xpath_node_matches_absolute(
    const struct system_xml_document *document,
    int node_index,
    const struct system_xpath *path
);
static bool xpath_node_matches_suffix(
    const struct system_xml_document *document,
    int node_index,
    const struct system_xpath *path
);
static bool xpath_segment_equals_node(
    const struct system_xml_document *document,
    const struct system_xpath_segment *segment,
    const struct system_xml_node *node
);
static int extract_matching_xml_text(
    struct mylite_db *database,
    const struct system_xml_document *document,
    const struct system_xpath *path,
    char **out_text,
    size_t *out_text_size
);
static int append_xml_node_text(
    const struct system_xml_document *document,
    const struct system_xml_node *node,
    struct mylite_dynamic_string *out_string
);
static int update_matching_xml_text(
    struct mylite_db *database,
    const struct system_xml_document *document,
    const struct system_xpath *path,
    const char *replacement,
    size_t replacement_size,
    char **out_text,
    size_t *out_text_size
);

int mylite_system_sleep_seconds(struct mylite_db *database, double seconds, int64_t *out_value) {
    if (database == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0;
    if (!isfinite(seconds) || seconds < 0.0) {
        return mylite_system_sleep_invalid_argument(database, out_value);
    }
    if (seconds > 0.0 && sleep_platform_seconds(seconds) != 0) {
        mylite_diagnostics_set_error(
            &database->diagnostics,
            MYLITE_ERROR,
            "HY000",
            "SLEEP() failed"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

int mylite_system_sleep_invalid_argument(struct mylite_db *database, int64_t *out_value) {
    if (database == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0;
    if (system_sql_mode_is_strict(database)) {
        set_sleep_incorrect_argument_error(database);
        return MYLITE_ERROR;
    }
    return append_sleep_incorrect_argument_warning(database);
}

int mylite_system_append_truncated_double_warning(
    struct mylite_db *database,
    const char *text,
    size_t text_size
) {
    struct mylite_dynamic_string message;
    int rc = MYLITE_OK;

    if (database == NULL || text == NULL) {
        return MYLITE_MISUSE;
    }
    mylite_dynamic_string_init(&message);
    rc = mylite_dynamic_string_append(&message, "Truncated incorrect DOUBLE value: '");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_bytes(&message, text, text_size);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&message, '\'');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_diagnostics_append_warning(
            &database->diagnostics,
            mysql_warning_truncated_incorrect_double,
            "22007",
            message.text == NULL ? "" : message.text
        );
    }
    if (rc == MYLITE_NOMEM) {
        set_system_nomem_error(database, "failed to format SLEEP() truncation warning");
    }
    mylite_dynamic_string_deinit(&message);
    return rc;
}

int mylite_system_extract_value(
    struct mylite_db *database,
    const char *xml,
    size_t xml_size,
    const char *xpath,
    size_t xpath_size,
    char **out_text,
    size_t *out_text_size,
    bool *out_is_null
) {
    struct system_xml_document document = {0};
    struct system_xpath path = {0};
    bool xml_valid = false;
    int rc = MYLITE_OK;

    if (database == NULL || out_text == NULL || out_text_size == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_size = 0U;
    *out_is_null = false;
    if (xml == NULL || xpath == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (!parse_xpath(database, xpath, xpath_size, &path)) {
        return MYLITE_ERROR;
    }
    rc = parse_xml_document(database, xml, xml_size, &document, &xml_valid);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!xml_valid) {
        *out_is_null = true;
        rc = append_xml_warning(database);
        xml_document_deinit(&document);
        return rc;
    }

    rc = extract_matching_xml_text(database, &document, &path, out_text, out_text_size);
    xml_document_deinit(&document);
    return rc;
}

int mylite_system_update_xml(
    struct mylite_db *database,
    const char *xml,
    size_t xml_size,
    const char *xpath,
    size_t xpath_size,
    const char *replacement,
    size_t replacement_size,
    char **out_text,
    size_t *out_text_size,
    bool *out_is_null
) {
    struct system_xml_document document = {0};
    struct system_xpath path = {0};
    bool xml_valid = false;
    int rc = MYLITE_OK;

    if (database == NULL || out_text == NULL || out_text_size == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_size = 0U;
    *out_is_null = false;
    if (xml == NULL || xpath == NULL || replacement == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (!parse_xpath(database, xpath, xpath_size, &path)) {
        return MYLITE_ERROR;
    }
    rc = parse_xml_document(database, xml, xml_size, &document, &xml_valid);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!xml_valid) {
        *out_is_null = true;
        rc = append_xml_warning(database);
        xml_document_deinit(&document);
        return rc;
    }

    rc = update_matching_xml_text(
        database,
        &document,
        &path,
        replacement,
        replacement_size,
        out_text,
        out_text_size
    );
    xml_document_deinit(&document);
    return rc;
}

int mylite_sqlite_register_system_functions(sqlite3 *sqlite) {
    static const struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_sleep",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY,
            .application_data = NULL,
            .scalar_callback = sleep_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_load_file",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY,
            .application_data = NULL,
            .scalar_callback = load_file_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_extract_value",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY,
            .application_data = NULL,
            .scalar_callback = extract_value_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_update_xml",
            .argument_count = 3,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY,
            .application_data = NULL,
            .scalar_callback = update_xml_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
    };

    return mylite_sqlite_register_functions(
        sqlite,
        registrations,
        sizeof(registrations) / sizeof(registrations[0])
    );
}

static void sleep_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_db *database = NULL;
    double seconds = 0.0;
    int64_t value = 0;
    bool invalid = false;
    bool skip_sleep = false;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 1 || argv == NULL) {
        sqlite3_result_error(context, "invalid MyLite SLEEP callback", -1);
        return;
    }
    database = mylite_sqlite_bootstrap_owner_from_context(context);
    rc = parse_sqlite_sleep_argument(database, argv[0], &seconds, &invalid, &skip_sleep);
    if (rc == MYLITE_OK && invalid) {
        rc = mylite_system_sleep_invalid_argument(database, &value);
    } else if (rc == MYLITE_OK && !skip_sleep) {
        rc = mylite_system_sleep_seconds(database, seconds, &value);
    }
    if (rc != MYLITE_OK) {
        sqlite_system_result_error(context, rc, database);
        return;
    }
    sqlite3_result_int64(context, value);
}

static void load_file_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    (void)argv;
    if (context == NULL || argc != 1) {
        sqlite3_result_error(context, "invalid MyLite LOAD_FILE callback", -1);
        return;
    }
    sqlite3_result_null(context);
}

static void extract_value_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    struct system_sqlite_text_argument xml = {0};
    struct system_sqlite_text_argument xpath = {0};
    struct mylite_db *database = NULL;
    char *result = NULL;
    size_t result_size = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 2 || argv == NULL) {
        sqlite3_result_error(context, "invalid MyLite ExtractValue callback", -1);
        return;
    }
    database = mylite_sqlite_bootstrap_owner_from_context(context);
    rc = sqlite_text_argument(argv[0], &xml);
    if (rc == MYLITE_OK) {
        rc = sqlite_text_argument(argv[1], &xpath);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_system_extract_value(
            database,
            xml.is_null ? NULL : xml.text,
            xml.text_size,
            xpath.is_null ? NULL : xpath.text,
            xpath.text_size,
            &result,
            &result_size,
            &is_null
        );
    }
    if (rc != MYLITE_OK) {
        sqlite_system_result_error(context, rc, database);
        return;
    }
    if (is_null) {
        sqlite3_result_null(context);
    } else {
        sqlite3_result_text(
            context,
            result == NULL ? "" : result,
            (int)result_size,
            SQLITE_TRANSIENT
        );
    }
    free(result);
}

static void update_xml_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct system_sqlite_text_argument xml = {0};
    struct system_sqlite_text_argument xpath = {0};
    struct system_sqlite_text_argument replacement = {0};
    struct mylite_db *database = NULL;
    char *result = NULL;
    size_t result_size = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 3 || argv == NULL) {
        sqlite3_result_error(context, "invalid MyLite UpdateXML callback", -1);
        return;
    }
    database = mylite_sqlite_bootstrap_owner_from_context(context);
    rc = sqlite_text_argument(argv[0], &xml);
    if (rc == MYLITE_OK) {
        rc = sqlite_text_argument(argv[1], &xpath);
    }
    if (rc == MYLITE_OK) {
        rc = sqlite_text_argument(argv[2], &replacement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_system_update_xml(
            database,
            xml.is_null ? NULL : xml.text,
            xml.text_size,
            xpath.is_null ? NULL : xpath.text,
            xpath.text_size,
            replacement.is_null ? NULL : replacement.text,
            replacement.text_size,
            &result,
            &result_size,
            &is_null
        );
    }
    if (rc != MYLITE_OK) {
        sqlite_system_result_error(context, rc, database);
        return;
    }
    if (is_null) {
        sqlite3_result_null(context);
    } else {
        sqlite3_result_text(
            context,
            result == NULL ? "" : result,
            (int)result_size,
            SQLITE_TRANSIENT
        );
    }
    free(result);
}

static bool system_sql_mode_is_strict(const struct mylite_db *database) {
    return database != NULL &&
           (database->session.sql_mode & (MYLITE_SESSION_SQL_MODE_STRICT_TRANS_TABLES |
                                          MYLITE_SESSION_SQL_MODE_STRICT_ALL_TABLES)) != 0U;
}

static int sleep_platform_seconds(double seconds) {
    while (seconds > 0.0) {
        double requested_milliseconds = seconds * (double)sleep_milliseconds_per_second;
        int milliseconds =
            requested_milliseconds > (double)INT_MAX ? INT_MAX : (int)ceil(requested_milliseconds);

        if (milliseconds < sleep_minimum_milliseconds) {
            milliseconds = sleep_minimum_milliseconds;
        }
        sqlite3_sleep(milliseconds);
        seconds -= (double)milliseconds / (double)sleep_milliseconds_per_second;
    }
    return 0;
}

static int parse_sqlite_sleep_argument(
    struct mylite_db *database,
    sqlite3_value *value,
    double *out_seconds,
    bool *out_invalid,
    bool *out_skip_sleep
) {
    int value_type = SQLITE_NULL;

    if (out_seconds == NULL || out_invalid == NULL || out_skip_sleep == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_seconds = 0.0;
    *out_invalid = false;
    *out_skip_sleep = false;

    value_type = sqlite3_value_type(value);
    if (value_type == SQLITE_NULL) {
        *out_invalid = true;
        return MYLITE_OK;
    }
    if (value_type == SQLITE_TEXT || value_type == SQLITE_BLOB) {
        const unsigned char *text = sqlite3_value_text(value);
        int byte_count = sqlite3_value_bytes(value);
        bool valid = false;
        int rc = MYLITE_OK;

        if (text == NULL || byte_count < 0) {
            return MYLITE_NOMEM;
        }
        rc = parse_double_text(
            database,
            (const char *)text,
            (size_t)byte_count,
            out_seconds,
            &valid
        );
        if (rc != MYLITE_OK || valid) {
            return rc;
        }
        rc = mylite_system_append_truncated_double_warning(
            database,
            (const char *)text,
            (size_t)byte_count
        );
        *out_seconds = 0.0;
        *out_skip_sleep = true;
        return rc;
    }

    *out_seconds = sqlite3_value_double(value);
    if (!isfinite(*out_seconds) || *out_seconds < 0.0) {
        *out_invalid = true;
    }
    return MYLITE_OK;
}

static int parse_double_text(
    struct mylite_db *database,
    const char *text,
    size_t text_size,
    double *out_value,
    bool *out_valid
) {
    char *copy = NULL;
    char *start = NULL;
    char *end = NULL;

    if (text == NULL || out_value == NULL || out_valid == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0.0;
    *out_valid = false;
    copy = (char *)calloc(text_size + 1U, sizeof(*copy));
    if (copy == NULL) {
        set_system_nomem_error(database, "out of memory while parsing DOUBLE value");
        return MYLITE_NOMEM;
    }
    memcpy(copy, text, text_size);
    start = copy;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        ++start;
    }
    errno = 0;
    *out_value = strtod(start, &end);
    while (end != NULL && *end != '\0' && isspace((unsigned char)*end)) {
        ++end;
    }
    *out_valid = errno != ERANGE && end != start && end != NULL && *end == '\0';
    free(copy);
    return MYLITE_OK;
}

static int sqlite_text_argument(
    sqlite3_value *value,
    struct system_sqlite_text_argument *out_argument
) {
    const unsigned char *text = NULL;
    int byte_count = 0;

    if (value == NULL || out_argument == NULL) {
        return MYLITE_MISUSE;
    }
    *out_argument = (struct system_sqlite_text_argument){0};
    if (sqlite3_value_type(value) == SQLITE_NULL) {
        out_argument->is_null = true;
        return MYLITE_OK;
    }
    text = sqlite3_value_text(value);
    byte_count = sqlite3_value_bytes(value);
    if (text == NULL || byte_count < 0) {
        return MYLITE_NOMEM;
    }
    out_argument->text = (const char *)text;
    out_argument->text_size = (size_t)byte_count;
    return MYLITE_OK;
}

static void sqlite_system_result_error(
    sqlite3_context *context,
    int rc,
    struct mylite_db *database
) {
    const char *message = "MyLite system function failed";

    if (database != NULL) {
        message = mylite_diagnostics_errmsg(&database->diagnostics);
    }
    sqlite3_result_error(context, message, -1);
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
    }
}

static int append_sleep_incorrect_argument_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        &database->diagnostics,
        mysql_error_incorrect_arguments,
        "HY000",
        "Incorrect arguments to sleep."
    );
}

static int append_xml_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        &database->diagnostics,
        mysql_warning_incorrect_xml_value,
        "HY000",
        "Incorrect XML value: 'parse error'"
    );
}

static void set_sleep_incorrect_argument_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        &database->diagnostics,
        mysql_error_incorrect_arguments,
        "HY000",
        "Incorrect arguments to sleep."
    );
}

static void set_xpath_syntax_error(
    struct mylite_db *database,
    const char *xpath,
    size_t xpath_size
) {
    char preview[xpath_error_preview_capacity];
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t preview_size = xpath_size < sizeof(preview) - 1U ? xpath_size : sizeof(preview) - 1U;
    int written = 0;

    memcpy(preview, xpath == NULL ? "" : xpath, preview_size);
    preview[preview_size] = '\0';
    written = snprintf(message, sizeof(message), "XPATH syntax error: '%s'", preview);
    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_diagnostics_set_error(
            &database->diagnostics,
            MYLITE_NOMEM,
            "HY001",
            "failed to format XPATH syntax error"
        );
        return;
    }
    mylite_diagnostics_set_error(&database->diagnostics, mysql_error_unknown, "HY000", message);
}

static void set_system_nomem_error(struct mylite_db *database, const char *message) {
    if (database == NULL) {
        return;
    }
    mylite_diagnostics_set_error(
        &database->diagnostics,
        MYLITE_NOMEM,
        "HY001",
        message == NULL ? "out of memory" : message
    );
}

static int duplicate_bytes(
    struct mylite_db *database,
    const char *text,
    size_t text_size,
    char **out_text
) {
    char *copy = NULL;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    copy = (char *)calloc(text_size + 1U, sizeof(*copy));
    if (copy == NULL) {
        set_system_nomem_error(database, "out of memory while copying system function result");
        return MYLITE_NOMEM;
    }
    if (text_size != 0U) {
        memcpy(copy, text, text_size);
    }
    *out_text = copy;
    return MYLITE_OK;
}

static int parse_xml_document(
    struct mylite_db *database,
    const char *xml,
    size_t xml_size,
    struct system_xml_document *out_document,
    bool *out_valid
) {
    size_t index = 0U;

    if (out_document == NULL || out_valid == NULL || xml == NULL) {
        return MYLITE_MISUSE;
    }
    *out_document = (struct system_xml_document){.text = xml, .size = xml_size};
    *out_valid = true;

    while (index < xml_size) {
        if (xml[index] != '<') {
            ++index;
            continue;
        }
        if (index + 1U >= xml_size) {
            *out_valid = false;
            return MYLITE_OK;
        }
        if (xml[index + 1U] == '/') {
            if (!parse_xml_end_tag(out_document, &index, out_valid)) {
                return MYLITE_OK;
            }
        } else if (index + 1U < xml_size && xml[index + 1U] == '?') {
            if (!skip_xml_processing_instruction(out_document, &index, out_valid)) {
                return MYLITE_OK;
            }
        } else if (index + 3U < xml_size && memcmp(&xml[index], "<!--", 4U) == 0) {
            if (!skip_xml_comment(out_document, &index, out_valid)) {
                return MYLITE_OK;
            }
        } else if (index + 1U < xml_size && xml[index + 1U] == '!') {
            if (!skip_xml_declaration(out_document, &index, out_valid)) {
                return MYLITE_OK;
            }
        } else if (!parse_xml_start_tag(database, out_document, &index, out_valid)) {
            if (!*out_valid) {
                return MYLITE_OK;
            }
            xml_document_deinit(out_document);
            return MYLITE_NOMEM;
        }
    }
    if (out_document->stack_count != 0U) {
        *out_valid = false;
    }
    return MYLITE_OK;
}

static void xml_document_deinit(struct system_xml_document *document) {
    if (document == NULL) {
        return;
    }
    free(document->nodes);
    free(document->stack);
    *document = (struct system_xml_document){0};
}

static int xml_document_append_node(
    struct mylite_db *database,
    struct system_xml_document *document,
    struct system_xml_node node,
    int *out_index
) {
    struct system_xml_node *nodes = NULL;
    size_t capacity =
        document->node_capacity == 0U ? xml_initial_node_capacity : document->node_capacity;

    if (out_index == NULL) {
        return MYLITE_MISUSE;
    }
    *out_index = -1;
    if (document->node_count == document->node_capacity) {
        while (capacity <= document->node_count) {
            if (capacity > SIZE_MAX / 2U) {
                set_system_nomem_error(database, "too many XML elements");
                return MYLITE_NOMEM;
            }
            capacity *= 2U;
        }
        nodes = (struct system_xml_node *)realloc(document->nodes, capacity * sizeof(*nodes));
        if (nodes == NULL) {
            set_system_nomem_error(database, "out of memory while parsing XML");
            return MYLITE_NOMEM;
        }
        document->nodes = nodes;
        document->node_capacity = capacity;
    }
    if (document->node_count > (size_t)INT_MAX) {
        set_system_nomem_error(database, "too many XML elements");
        return MYLITE_NOMEM;
    }
    *out_index = (int)document->node_count;
    document->nodes[document->node_count++] = node;
    return MYLITE_OK;
}

static int xml_document_push_node(
    struct mylite_db *database,
    struct system_xml_document *document,
    int node_index
) {
    int *stack = NULL;
    size_t capacity =
        document->stack_capacity == 0U ? xml_initial_stack_capacity : document->stack_capacity;

    if (document->stack_count == document->stack_capacity) {
        while (capacity <= document->stack_count) {
            if (capacity > SIZE_MAX / 2U) {
                set_system_nomem_error(database, "XML nesting is too deep");
                return MYLITE_NOMEM;
            }
            capacity *= 2U;
        }
        stack = (int *)realloc(document->stack, capacity * sizeof(*stack));
        if (stack == NULL) {
            set_system_nomem_error(database, "out of memory while parsing XML");
            return MYLITE_NOMEM;
        }
        document->stack = stack;
        document->stack_capacity = capacity;
    }
    document->stack[document->stack_count++] = node_index;
    return MYLITE_OK;
}

static bool parse_xml_start_tag(
    struct mylite_db *database,
    struct system_xml_document *document,
    size_t *inout_index,
    bool *out_valid
) {
    size_t tag_start = *inout_index;
    size_t name_start = tag_start + 1U;
    size_t name_end = scan_xml_name(document->text, document->size, name_start);
    bool self_closing = false;
    int parent = document->stack_count == 0U ? -1 : document->stack[document->stack_count - 1U];
    int node_index = -1;
    struct system_xml_node node = {0};

    if (name_end == name_start) {
        *out_valid = false;
        return false;
    }
    *inout_index = name_end;
    if (!skip_xml_tag_tail(document, inout_index, &self_closing, out_valid)) {
        return false;
    }

    node.parent = parent;
    node.start = tag_start;
    node.name_start = name_start;
    node.name_size = name_end - name_start;
    node.content_start = *inout_index;
    node.content_end = self_closing ? *inout_index : document->size;
    node.end = self_closing ? *inout_index : document->size;
    if (xml_document_append_node(database, document, node, &node_index) != MYLITE_OK) {
        return false;
    }
    if (!self_closing && xml_document_push_node(database, document, node_index) != MYLITE_OK) {
        return false;
    }
    return true;
}

static bool parse_xml_end_tag(
    struct system_xml_document *document,
    size_t *inout_index,
    bool *out_valid
) {
    size_t tag_start = *inout_index;
    size_t name_start = tag_start + 2U;
    size_t name_end = scan_xml_name(document->text, document->size, name_start);
    size_t index = name_end;
    int node_index = -1;
    struct system_xml_node *node = NULL;

    if (name_end == name_start || document->stack_count == 0U) {
        *out_valid = false;
        return false;
    }
    while (index < document->size && isspace((unsigned char)document->text[index])) {
        ++index;
    }
    if (index >= document->size || document->text[index] != '>') {
        *out_valid = false;
        return false;
    }
    ++index;

    node_index = document->stack[--document->stack_count];
    node = &document->nodes[node_index];
    if (node->name_size != name_end - name_start ||
        memcmp(&document->text[node->name_start], &document->text[name_start], node->name_size) !=
            0) {
        *out_valid = false;
        return false;
    }
    node->content_end = tag_start;
    node->end = index;
    *inout_index = index;
    return true;
}

static bool skip_xml_processing_instruction(
    const struct system_xml_document *document,
    size_t *inout_index,
    bool *out_valid
) {
    size_t index = *inout_index + 2U;

    while (index + 1U < document->size) {
        if (document->text[index] == '?' && document->text[index + 1U] == '>') {
            *inout_index = index + 2U;
            return true;
        }
        ++index;
    }
    *out_valid = false;
    return false;
}

static bool skip_xml_comment(
    const struct system_xml_document *document,
    size_t *inout_index,
    bool *out_valid
) {
    size_t index = *inout_index + 4U;

    while (index + 2U < document->size) {
        if (memcmp(&document->text[index], "-->", 3U) == 0) {
            *inout_index = index + 3U;
            return true;
        }
        ++index;
    }
    *out_valid = false;
    return false;
}

static bool skip_xml_declaration(
    const struct system_xml_document *document,
    size_t *inout_index,
    bool *out_valid
) {
    size_t index = *inout_index + 2U;

    while (index < document->size) {
        if (document->text[index] == '>') {
            *inout_index = index + 1U;
            return true;
        }
        ++index;
    }
    *out_valid = false;
    return false;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static bool skip_xml_tag_tail(
    const struct system_xml_document *document,
    size_t *inout_index,
    bool *out_self_closing, // NOLINT(bugprone-easily-swappable-parameters)
    bool *out_valid
) {
    bool in_quote = false;
    char quote = '\0';
    size_t index = *inout_index;

    *out_self_closing = false;
    while (index < document->size) {
        char byte = document->text[index];

        if (in_quote) {
            if (byte == quote) {
                in_quote = false;
            }
            ++index;
            continue;
        }
        if (byte == '\'' || byte == '"') {
            in_quote = true;
            quote = byte;
            ++index;
            continue;
        }
        if (byte == '>') {
            *inout_index = index + 1U;
            return true;
        }
        if (byte == '/' && index + 1U < document->size && document->text[index + 1U] == '>') {
            *out_self_closing = true;
            *inout_index = index + 2U;
            return true;
        }
        ++index;
    }
    *out_valid = false;
    return false;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static size_t scan_xml_name(const char *text, size_t size, size_t index) {
    size_t current = index;

    while (current < size && xml_name_byte_is_supported(text[current])) {
        ++current;
    }
    return current;
}

static bool xml_name_byte_is_supported(char byte) {
    unsigned char value = (unsigned char)byte;

    return isalnum(value) || byte == '_' || byte == '-' || byte == ':' || byte == '.';
}

static bool parse_xpath(
    struct mylite_db *database,
    const char *xpath,
    size_t xpath_size,
    struct system_xpath *out_path
) {
    size_t index = 0U;

    if (xpath == NULL || out_path == NULL) {
        return false;
    }
    *out_path = (struct system_xpath){0};
    if (xpath_size == 0U || xpath[0] != '/') {
        set_xpath_syntax_error(database, xpath, xpath_size);
        return false;
    }
    index = 1U;
    if (index < xpath_size && xpath[index] == '/') {
        out_path->descendant = true;
        ++index;
    }
    while (index < xpath_size) {
        size_t segment_start = index;
        size_t segment_end = scan_xml_name(xpath, xpath_size, index);

        if (segment_end == segment_start || out_path->segment_count == xpath_max_segments) {
            set_xpath_syntax_error(database, xpath, xpath_size);
            return false;
        }
        out_path->segments[out_path->segment_count++] = (struct system_xpath_segment){
            .text = &xpath[segment_start],
            .size = segment_end - segment_start,
        };
        index = segment_end;
        if (index == xpath_size) {
            break;
        }
        if (xpath[index] != '/') {
            set_xpath_syntax_error(database, xpath, xpath_size);
            return false;
        }
        ++index;
        if (index == xpath_size || xpath[index] == '/') {
            set_xpath_syntax_error(database, xpath, xpath_size);
            return false;
        }
    }
    if (out_path->segment_count == 0U) {
        set_xpath_syntax_error(database, xpath, xpath_size);
        return false;
    }
    return true;
}

static bool xpath_node_matches(
    const struct system_xml_document *document,
    const struct system_xml_node *node,
    const struct system_xpath *path
) {
    int node_index = (int)(node - document->nodes);

    return path->descendant ? xpath_node_matches_suffix(document, node_index, path)
                            : xpath_node_matches_absolute(document, node_index, path);
}

static bool xpath_node_matches_absolute(
    const struct system_xml_document *document,
    int node_index,
    const struct system_xpath *path
) {
    size_t segment_index = path->segment_count;

    while (segment_index > 0U && node_index >= 0) {
        const struct system_xml_node *node = &document->nodes[node_index];

        --segment_index;
        if (!xpath_segment_equals_node(document, &path->segments[segment_index], node)) {
            return false;
        }
        node_index = node->parent;
    }
    return segment_index == 0U && node_index < 0;
}

static bool xpath_node_matches_suffix(
    const struct system_xml_document *document,
    int node_index,
    const struct system_xpath *path
) {
    size_t segment_index = path->segment_count;

    while (segment_index > 0U && node_index >= 0) {
        const struct system_xml_node *node = &document->nodes[node_index];

        --segment_index;
        if (!xpath_segment_equals_node(document, &path->segments[segment_index], node)) {
            return false;
        }
        node_index = node->parent;
    }
    return segment_index == 0U;
}

static bool xpath_segment_equals_node(
    const struct system_xml_document *document,
    const struct system_xpath_segment *segment,
    const struct system_xml_node *node
) {
    return segment->size == node->name_size &&
           memcmp(segment->text, &document->text[node->name_start], segment->size) == 0;
}

static int extract_matching_xml_text(
    struct mylite_db *database,
    const struct system_xml_document *document,
    const struct system_xpath *path,
    char **out_text,
    size_t *out_text_size
) {
    struct mylite_dynamic_string result;
    bool has_match = false;
    int rc = MYLITE_OK;

    mylite_dynamic_string_init(&result);
    for (size_t index = 0U; rc == MYLITE_OK && index < document->node_count; ++index) {
        if (!xpath_node_matches(document, &document->nodes[index], path)) {
            continue;
        }
        if (has_match) {
            rc = mylite_dynamic_string_append_char(&result, ' ');
        }
        if (rc == MYLITE_OK) {
            rc = append_xml_node_text(document, &document->nodes[index], &result);
        }
        has_match = true;
    }
    if (rc == MYLITE_OK) {
        *out_text_size = result.length;
        *out_text = mylite_dynamic_string_take(&result);
        if (*out_text == NULL) {
            rc = duplicate_bytes(database, "", 0U, out_text);
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_system_nomem_error(database, "out of memory while extracting XML value");
    }
    mylite_dynamic_string_deinit(&result);
    return rc;
}

static int append_xml_node_text(
    const struct system_xml_document *document,
    const struct system_xml_node *node,
    struct mylite_dynamic_string *out_string
) {
    size_t index = node->content_start;
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && index < node->content_end) {
        if (document->text[index] == '<') {
            while (index < node->content_end && document->text[index] != '>') {
                ++index;
            }
            if (index < node->content_end) {
                ++index;
            }
            continue;
        }
        rc = mylite_dynamic_string_append_char(out_string, document->text[index]);
        ++index;
    }
    return rc;
}

static int update_matching_xml_text(
    struct mylite_db *database,
    const struct system_xml_document *document,
    const struct system_xpath *path,
    const char *replacement,
    size_t replacement_size,
    char **out_text,
    size_t *out_text_size
) {
    const struct system_xml_node *match = NULL;
    size_t match_count = 0U;
    struct mylite_dynamic_string result;
    int rc = MYLITE_OK;

    for (size_t index = 0U; index < document->node_count; ++index) {
        if (!xpath_node_matches(document, &document->nodes[index], path)) {
            continue;
        }
        match = &document->nodes[index];
        ++match_count;
        if (match_count > 1U) {
            break;
        }
    }
    if (match_count != 1U || match == NULL) {
        *out_text_size = document->size;
        return duplicate_bytes(database, document->text, document->size, out_text);
    }

    mylite_dynamic_string_init(&result);
    rc = mylite_dynamic_string_append_bytes(&result, document->text, match->start);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_bytes(&result, replacement, replacement_size);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_bytes(
            &result,
            &document->text[match->end],
            document->size - match->end
        );
    }
    if (rc == MYLITE_OK) {
        *out_text_size = result.length;
        *out_text = mylite_dynamic_string_take(&result);
    }
    if (rc == MYLITE_NOMEM) {
        set_system_nomem_error(database, "out of memory while updating XML value");
    }
    mylite_dynamic_string_deinit(&result);
    return rc;
}
