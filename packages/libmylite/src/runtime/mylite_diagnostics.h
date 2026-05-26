#ifndef MYLITE_RUNTIME_MYLITE_DIAGNOSTICS_H
#define MYLITE_RUNTIME_MYLITE_DIAGNOSTICS_H

#include <mylite/mylite.h>

#include <stddef.h>

enum {
    MYLITE_SQLSTATE_LENGTH = 5,
    MYLITE_DIAGNOSTIC_LEVEL_CAPACITY = 8,
    MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY = 256,
};

struct mylite_diagnostic_record {
    char level[MYLITE_DIAGNOSTIC_LEVEL_CAPACITY];
    int code;
    char sqlstate[MYLITE_SQLSTATE_LENGTH + 1U];
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
};

struct mylite_diagnostics {
    struct mylite_diagnostic_record condition;
    struct mylite_diagnostic_record *warnings;
    size_t warning_count;
    size_t warning_capacity;
};

void mylite_diagnostics_init(struct mylite_diagnostics *diagnostics);
void mylite_diagnostics_deinit(struct mylite_diagnostics *diagnostics);
void mylite_diagnostics_reset(struct mylite_diagnostics *diagnostics);
int mylite_diagnostics_replace(
    struct mylite_diagnostics *destination,
    const struct mylite_diagnostics *source
);

void mylite_diagnostics_set_error(
    struct mylite_diagnostics *diagnostics,
    int code,
    const char *sqlstate,
    const char *message
);
int mylite_diagnostics_append_warning(
    struct mylite_diagnostics *diagnostics,
    int code,
    const char *sqlstate,
    const char *message
);
int mylite_diagnostics_append_error(
    struct mylite_diagnostics *diagnostics,
    int code,
    const char *sqlstate,
    const char *message
);
int mylite_diagnostics_reserve_warning_capacity(
    struct mylite_diagnostics *diagnostics,
    size_t required_capacity
);
int mylite_diagnostics_append_note(
    struct mylite_diagnostics *diagnostics,
    int code,
    const char *sqlstate,
    const char *message
);

int mylite_diagnostics_errcode(const struct mylite_diagnostics *diagnostics);
const char *mylite_diagnostics_sqlstate(const struct mylite_diagnostics *diagnostics);
const char *mylite_diagnostics_errmsg(const struct mylite_diagnostics *diagnostics);

size_t mylite_diagnostics_warning_count(const struct mylite_diagnostics *diagnostics);
const struct mylite_diagnostic_record *mylite_diagnostics_warning_at(
    const struct mylite_diagnostics *diagnostics,
    size_t index
);

const char *mylite_diagnostics_misuse_sqlstate(void);
const char *mylite_diagnostics_misuse_message(void);

#endif
