#include "mylite_dynamic_string.h"

#include <mylite/mylite.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
    int rc = MYLITE_OK;

    if (string == NULL || text == NULL) {
        return MYLITE_MISUSE;
    }

    rc = mylite_dynamic_string_append_char(string, '"');

    for (size_t index = 0U; rc == MYLITE_OK && text[index] != '\0'; ++index) {
        if (text[index] == '"') {
            rc = mylite_dynamic_string_append(string, "\"\"");
        } else {
            rc = mylite_dynamic_string_append_char(string, text[index]);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '"');
    }

    return rc;
}

int mylite_dynamic_string_append_mysql_quoted_identifier(
    struct mylite_dynamic_string *string,
    const char *text
) {
    int rc = MYLITE_OK;

    if (string == NULL || text == NULL) {
        return MYLITE_MISUSE;
    }

    rc = mylite_dynamic_string_append_char(string, '`');

    for (size_t index = 0U; rc == MYLITE_OK && text[index] != '\0'; ++index) {
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
