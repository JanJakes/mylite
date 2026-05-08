#include "mylite_diagnostics.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *mylite_diagnostics_ok_sqlstate(void);
static const char *mylite_diagnostics_ok_message(void);
static int reserve_warnings(struct mylite_diagnostics *diagnostics, size_t required_capacity);

struct diagnostic_values {
    int code;
    const char *sqlstate;
    const char *message;
};

static void set_record(
    struct mylite_diagnostic_record *record,
    const struct diagnostic_values *values
);
static void set_sqlstate(struct mylite_diagnostic_record *record, const char *sqlstate);
static int sqlstate_is_valid(const char *sqlstate);
static void copy_text(char *destination, size_t destination_size, const char *source);

void mylite_diagnostics_init(struct mylite_diagnostics *diagnostics) {
    if (diagnostics == NULL) {
        return;
    }

    diagnostics->warnings = NULL;
    diagnostics->warning_count = 0U;
    diagnostics->warning_capacity = 0U;
    mylite_diagnostics_reset(diagnostics);
}

void mylite_diagnostics_deinit(struct mylite_diagnostics *diagnostics) {
    if (diagnostics == NULL) {
        return;
    }

    free(diagnostics->warnings);
    diagnostics->warnings = NULL;
    diagnostics->warning_count = 0U;
    diagnostics->warning_capacity = 0U;
    mylite_diagnostics_reset(diagnostics);
}

void mylite_diagnostics_reset(struct mylite_diagnostics *diagnostics) {
    if (diagnostics == NULL) {
        return;
    }

    set_record(
        &diagnostics->condition,
        &(struct diagnostic_values){
            .code = MYLITE_OK,
            .sqlstate = mylite_diagnostics_ok_sqlstate(),
            .message = mylite_diagnostics_ok_message(),
        }
    );
    diagnostics->warning_count = 0U;
}

int mylite_diagnostics_replace(
    struct mylite_diagnostics *destination,
    const struct mylite_diagnostics *source
) {
    struct mylite_diagnostics copy;
    int rc = MYLITE_OK;

    if (destination == NULL || source == NULL) {
        return MYLITE_MISUSE;
    }
    if (destination == source) {
        return MYLITE_OK;
    }

    mylite_diagnostics_init(&copy);
    copy.condition = source->condition;
    for (size_t index = 0U; rc == MYLITE_OK && index < source->warning_count; ++index) {
        const struct mylite_diagnostic_record *warning = &source->warnings[index];
        rc = mylite_diagnostics_append_warning(
            &copy,
            warning->code,
            warning->sqlstate,
            warning->message
        );
    }
    if (rc != MYLITE_OK) {
        mylite_diagnostics_deinit(&copy);
        return rc;
    }

    mylite_diagnostics_deinit(destination);
    *destination = copy;
    return MYLITE_OK;
}

void mylite_diagnostics_set_error(
    struct mylite_diagnostics *diagnostics,
    int code,
    const char *sqlstate,
    const char *message
) {
    if (diagnostics == NULL) {
        return;
    }

    set_record(
        &diagnostics->condition,
        &(struct diagnostic_values){
            .code = code,
            .sqlstate = sqlstate,
            .message = message,
        }
    );
}

int mylite_diagnostics_append_warning(
    struct mylite_diagnostics *diagnostics,
    int code,
    const char *sqlstate,
    const char *message
) {
    struct mylite_diagnostic_record *warning = NULL;
    int rc = MYLITE_OK;

    if (diagnostics == NULL) {
        return MYLITE_MISUSE;
    }
    if (diagnostics->warning_count == SIZE_MAX) {
        mylite_diagnostics_set_error(diagnostics, MYLITE_NOMEM, "HY001", "too many warnings");
        return MYLITE_NOMEM;
    }

    rc = reserve_warnings(diagnostics, diagnostics->warning_count + 1U);
    if (rc != MYLITE_OK) {
        mylite_diagnostics_set_error(
            diagnostics,
            MYLITE_NOMEM,
            "HY001",
            "out of memory while recording warning"
        );
        return rc;
    }

    warning = &diagnostics->warnings[diagnostics->warning_count];
    set_record(
        warning,
        &(struct diagnostic_values){
            .code = code,
            .sqlstate = sqlstate,
            .message = message,
        }
    );
    ++diagnostics->warning_count;

    return MYLITE_OK;
}

int mylite_diagnostics_errcode(const struct mylite_diagnostics *diagnostics) {
    if (diagnostics == NULL) {
        return MYLITE_MISUSE;
    }

    return diagnostics->condition.code;
}

const char *mylite_diagnostics_sqlstate(const struct mylite_diagnostics *diagnostics) {
    if (diagnostics == NULL) {
        return mylite_diagnostics_misuse_sqlstate();
    }
    if (diagnostics->condition.sqlstate[0] == '\0') {
        return mylite_diagnostics_ok_sqlstate();
    }

    return diagnostics->condition.sqlstate;
}

const char *mylite_diagnostics_errmsg(const struct mylite_diagnostics *diagnostics) {
    if (diagnostics == NULL) {
        return mylite_diagnostics_misuse_message();
    }
    if (diagnostics->condition.message[0] == '\0') {
        return mylite_diagnostics_ok_message();
    }

    return diagnostics->condition.message;
}

size_t mylite_diagnostics_warning_count(const struct mylite_diagnostics *diagnostics) {
    if (diagnostics == NULL) {
        return 0U;
    }

    return diagnostics->warning_count;
}

const struct mylite_diagnostic_record *mylite_diagnostics_warning_at(
    const struct mylite_diagnostics *diagnostics,
    size_t index
) {
    if (diagnostics == NULL || index >= diagnostics->warning_count) {
        return NULL;
    }

    return &diagnostics->warnings[index];
}

const char *mylite_diagnostics_misuse_sqlstate(void) {
    return "HY000";
}

const char *mylite_diagnostics_misuse_message(void) {
    return "bad parameter or other API misuse";
}

static const char *mylite_diagnostics_ok_sqlstate(void) {
    return "00000";
}

static const char *mylite_diagnostics_ok_message(void) {
    return "not an error";
}

static int reserve_warnings(struct mylite_diagnostics *diagnostics, size_t required_capacity) {
    enum { initial_warning_capacity = 4 };

    struct mylite_diagnostic_record *warnings = NULL;
    size_t capacity = diagnostics->warning_capacity;

    if (required_capacity <= capacity) {
        return MYLITE_OK;
    }

    if (capacity == 0U) {
        capacity = initial_warning_capacity;
    }
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*warnings)) {
        return MYLITE_NOMEM;
    }

    warnings = realloc(diagnostics->warnings, capacity * sizeof(*warnings));
    if (warnings == NULL) {
        return MYLITE_NOMEM;
    }

    diagnostics->warnings = warnings;
    diagnostics->warning_capacity = capacity;

    return MYLITE_OK;
}

static void set_record(
    struct mylite_diagnostic_record *record,
    const struct diagnostic_values *values
) {
    record->code = values->code;
    set_sqlstate(record, values->sqlstate);
    copy_text(record->message, sizeof(record->message), values->message);
}

static void set_sqlstate(struct mylite_diagnostic_record *record, const char *sqlstate) {
    const char *source =
        sqlstate_is_valid(sqlstate) ? sqlstate : mylite_diagnostics_misuse_sqlstate();

    memcpy(record->sqlstate, source, MYLITE_SQLSTATE_LENGTH);
    record->sqlstate[MYLITE_SQLSTATE_LENGTH] = '\0';
}

static int sqlstate_is_valid(const char *sqlstate) {
    return sqlstate != NULL && strlen(sqlstate) == MYLITE_SQLSTATE_LENGTH;
}

static void copy_text(char *destination, size_t destination_size, const char *source) {
    int written = snprintf(destination, destination_size, "%s", source == NULL ? "" : source);

    if (written < 0) {
        destination[0] = '\0';
    }
}
