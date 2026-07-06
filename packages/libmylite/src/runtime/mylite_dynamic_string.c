#include "mylite_dynamic_string.h"

#include <mylite/mylite.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int append_quoted_identifier(
    struct mylite_dynamic_string *string,
    const char *text,
    char quote
);

void mylite_dynamic_string_init(struct mylite_dynamic_string *string) {
    *string = (struct mylite_dynamic_string){0};
}

void mylite_dynamic_string_deinit(struct mylite_dynamic_string *string) {
    if (string == NULL) {
        return;
    }

    free(string->text);
    *string = (struct mylite_dynamic_string){0};
}

int mylite_dynamic_string_append(struct mylite_dynamic_string *string, const char *text) {
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (string == NULL || text == NULL) {
        return MYLITE_MISUSE;
    }

    text_length = strlen(text);
    if (text_length > SIZE_MAX - string->length - 1U) {
        return MYLITE_NOMEM;
    }

    rc = mylite_dynamic_string_reserve(string, string->length + text_length + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }

    memcpy(&string->text[string->length], text, text_length);
    string->length += text_length;
    string->text[string->length] = '\0';

    return MYLITE_OK;
}

int mylite_dynamic_string_append_bytes(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_size
) {
    int rc = MYLITE_OK;

    if (string == NULL || (text == NULL && text_size != 0U)) {
        return MYLITE_MISUSE;
    }
    if (text_size > SIZE_MAX - string->length - 1U) {
        return MYLITE_NOMEM;
    }

    rc = mylite_dynamic_string_reserve(string, string->length + text_size + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (text_size != 0U) {
        memcpy(&string->text[string->length], text, text_size);
    }
    string->length += text_size;
    string->text[string->length] = '\0';
    return MYLITE_OK;
}

int mylite_dynamic_string_append_char(struct mylite_dynamic_string *string, char byte) {
    int rc = MYLITE_OK;

    if (string == NULL) {
        return MYLITE_MISUSE;
    }
    if (string->length > SIZE_MAX - 2U) {
        return MYLITE_NOMEM;
    }

    rc = mylite_dynamic_string_reserve(string, string->length + 2U);
    if (rc != MYLITE_OK) {
        return rc;
    }

    string->text[string->length] = byte;
    ++string->length;
    string->text[string->length] = '\0';

    return MYLITE_OK;
}

int mylite_dynamic_string_append_quoted_identifier(
    struct mylite_dynamic_string *string,
    const char *text
) {
    return append_quoted_identifier(string, text, '"');
}

int mylite_dynamic_string_append_mysql_quoted_identifier(
    struct mylite_dynamic_string *string,
    const char *text
) {
    return append_quoted_identifier(string, text, '`');
}

int mylite_dynamic_string_reserve(struct mylite_dynamic_string *string, size_t required_capacity) {
    enum { initial_capacity = 128 };

    char *text = NULL;
    size_t capacity = 0U;

    if (string == NULL) {
        return MYLITE_MISUSE;
    }

    capacity = string->capacity;

    if (required_capacity <= capacity) {
        return MYLITE_OK;
    }
    if (capacity == 0U) {
        capacity = initial_capacity;
    }
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }

    text = realloc(string->text, capacity);
    if (text == NULL) {
        return MYLITE_NOMEM;
    }
    if (string->capacity == 0U) {
        text[0] = '\0';
    }
    string->text = text;
    string->capacity = capacity;

    return MYLITE_OK;
}

char *mylite_dynamic_string_take(struct mylite_dynamic_string *string) {
    char *text = NULL;

    if (string == NULL) {
        return NULL;
    }

    text = string->text;
    string->text = NULL;
    string->length = 0U;
    string->capacity = 0U;

    return text;
}

static int append_quoted_identifier(
    struct mylite_dynamic_string *string,
    const char *text,
    char quote
) {
    size_t append_size = 2U;
    size_t output = 0U;
    int rc = MYLITE_OK;

    if (string == NULL || text == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; text[index] != '\0'; ++index) {
        size_t increment = text[index] == quote ? 2U : 1U;

        if (append_size > SIZE_MAX - increment) {
            return MYLITE_NOMEM;
        }
        append_size += increment;
    }
    if (append_size > SIZE_MAX - string->length - 1U) {
        return MYLITE_NOMEM;
    }
    rc = mylite_dynamic_string_reserve(string, string->length + append_size + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }

    output = string->length;
    string->text[output] = quote;
    ++output;
    for (size_t index = 0U; text[index] != '\0'; ++index) {
        string->text[output] = text[index];
        ++output;
        if (text[index] == quote) {
            string->text[output] = quote;
            ++output;
        }
    }
    string->text[output] = quote;
    ++output;
    string->text[output] = '\0';
    string->length = output;
    return MYLITE_OK;
}
