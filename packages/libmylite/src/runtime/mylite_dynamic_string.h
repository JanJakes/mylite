#ifndef MYLITE_DYNAMIC_STRING_H
#define MYLITE_DYNAMIC_STRING_H

#include <stddef.h>

struct mylite_dynamic_string {
    char *text;
    size_t length;
    size_t capacity;
};

/* init requires a valid string pointer; mutating helpers reject invalid input. */
void mylite_dynamic_string_init(struct mylite_dynamic_string *string);
void mylite_dynamic_string_deinit(struct mylite_dynamic_string *string);
int mylite_dynamic_string_append(struct mylite_dynamic_string *string, const char *text);
int mylite_dynamic_string_append_bytes(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_size
);
int mylite_dynamic_string_append_char(struct mylite_dynamic_string *string, char byte);
int mylite_dynamic_string_append_quoted_identifier(
    struct mylite_dynamic_string *string,
    const char *text
);
int mylite_dynamic_string_append_mysql_quoted_identifier(
    struct mylite_dynamic_string *string,
    const char *text
);
int mylite_dynamic_string_reserve(struct mylite_dynamic_string *string, size_t required_capacity);
char *mylite_dynamic_string_take(struct mylite_dynamic_string *string);

#endif
