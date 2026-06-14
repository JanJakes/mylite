#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_numeric.h"

#include <mylite/mylite.h>

#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <windows.h>
#endif

enum {
    double_format_error_capacity = 80,
    double_text_max_significant_digits = 17,
    double_text_capacity = mylite_execution_scalar_double_text_capacity,
};

static const double double_scientific_integer_threshold = 1.0e15;

static bool should_format_scientific_integer_double(double value);
static int format_scientific_double_text(
    struct mylite_db *database,
    double value,
    const char *function_name,
    char *buffer,
    size_t buffer_size
);

int mylite_execution_format_double_text(
    struct mylite_db *database,
    double value,
    const char *function_name,
    char *buffer,
    size_t buffer_size
) {
    if (function_name == NULL || buffer == NULL || buffer_size == 0U) {
        return MYLITE_MISUSE;
    }

    if (should_format_scientific_integer_double(value)) {
        return format_scientific_double_text(database, value, function_name, buffer, buffer_size);
    }

    for (int precision = 1; precision <= double_text_max_significant_digits; ++precision) {
        char candidate[double_text_capacity];
        char *end = NULL;
        double parsed = 0.0;
        int written = mylite_execution_format_c_locale_text(
            candidate,
            sizeof(candidate),
            "%.*g",
            precision,
            value
        );

        if (written < 0 || (size_t)written >= sizeof(candidate)) {
            mylite_execution_set_double_format_error(database, function_name);
            return MYLITE_ERROR;
        }
        if (fabs(value) >= 1.0 && fabs(value) < double_scientific_integer_threshold &&
            (strchr(candidate, 'e') != NULL || strchr(candidate, 'E') != NULL)) {
            continue;
        }
        if (mylite_execution_parse_c_locale_double(candidate, &end, &parsed) == MYLITE_OK &&
            end != candidate && end != NULL && *end == '\0' && parsed == value) {
            return mylite_execution_copy_normalized_double_text(
                database,
                candidate,
                function_name,
                buffer,
                buffer_size
            );
        }
    }

    mylite_execution_set_double_format_error(database, function_name);
    return MYLITE_ERROR;
}

static bool should_format_scientific_integer_double(double value) {
    double magnitude = fabs(value);

    if (magnitude < double_scientific_integer_threshold) {
        return false;
    }
    return trunc(value) == value;
}

static int format_scientific_double_text(
    struct mylite_db *database,
    double value,
    const char *function_name,
    char *buffer,
    size_t buffer_size
) {
    if (function_name == NULL || buffer == NULL || buffer_size == 0U) {
        return MYLITE_MISUSE;
    }

    for (int precision = 0; precision < double_text_max_significant_digits; ++precision) {
        char candidate[double_text_capacity];
        char *end = NULL;
        double parsed = 0.0;
        int written = mylite_execution_format_c_locale_text(
            candidate,
            sizeof(candidate),
            "%.*e",
            precision,
            value
        );

        if (written < 0 || (size_t)written >= sizeof(candidate)) {
            mylite_execution_set_double_format_error(database, function_name);
            return MYLITE_ERROR;
        }
        if (mylite_execution_parse_c_locale_double(candidate, &end, &parsed) == MYLITE_OK &&
            end != candidate && end != NULL && *end == '\0' && parsed == value) {
            return mylite_execution_copy_normalized_scientific_double_text(
                database,
                candidate,
                function_name,
                buffer,
                buffer_size
            );
        }
    }

    mylite_execution_set_double_format_error(database, function_name);
    return MYLITE_ERROR;
}

int mylite_execution_format_c_locale_text(
    char *buffer,
    size_t buffer_size,
    const char *format,
    ...
) {
    va_list arguments;
    int written = 0;

    if (buffer == NULL || buffer_size == 0U || format == NULL) {
        return -1;
    }

    va_start(arguments, format);
#ifdef _WIN32
    _locale_t c_locale = _create_locale(LC_NUMERIC, "C");

    if (c_locale == NULL) {
        va_end(arguments);
        return -1;
    }
    written = _vsnprintf_l(buffer, buffer_size, format, c_locale, arguments);
    _free_locale(c_locale);
#else
    locale_t c_locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    locale_t previous_locale = (locale_t)0;

    if (c_locale == (locale_t)0) {
        va_end(arguments);
        return -1;
    }
    previous_locale = uselocale(c_locale);
    if (previous_locale == (locale_t)0) {
        freelocale(c_locale);
        va_end(arguments);
        return -1;
    }
    written = vsnprintf(buffer, buffer_size, format, arguments);
    if (uselocale(previous_locale) == (locale_t)0) {
        written = -1;
    }
    freelocale(c_locale);
#endif
    va_end(arguments);
    return written;
}

int mylite_execution_parse_c_locale_double(const char *text, char **out_end, double *out_value) {
    if (text == NULL || out_end == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

#ifdef _WIN32
    _locale_t c_locale = _create_locale(LC_NUMERIC, "C");

    if (c_locale == NULL) {
        return MYLITE_ERROR;
    }
    *out_value = _strtod_l(text, out_end, c_locale);
    _free_locale(c_locale);
#else
    locale_t c_locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    locale_t previous_locale = (locale_t)0;
    int rc = MYLITE_OK;

    if (c_locale == (locale_t)0) {
        return MYLITE_ERROR;
    }
    previous_locale = uselocale(c_locale);
    if (previous_locale == (locale_t)0) {
        freelocale(c_locale);
        return MYLITE_ERROR;
    }
    *out_value = strtod(text, out_end);
    if (uselocale(previous_locale) == (locale_t)0) {
        rc = MYLITE_ERROR;
    }
    freelocale(c_locale);
    if (rc != MYLITE_OK) {
        return rc;
    }
#endif

    return MYLITE_OK;
}

int mylite_execution_copy_normalized_double_text(
    struct mylite_db *database,
    const char *candidate,
    const char *function_name,
    char *buffer,
    size_t buffer_size
) {
    size_t write_index = 0U;

    if (candidate == NULL || function_name == NULL || buffer == NULL || buffer_size == 0U) {
        return MYLITE_MISUSE;
    }
    for (size_t read_index = 0U; candidate[read_index] != '\0'; ++read_index) {
        if (write_index + 1U >= buffer_size) {
            mylite_execution_set_double_format_error(database, function_name);
            return MYLITE_ERROR;
        }
        buffer[write_index] = candidate[read_index];
        ++write_index;
        if ((candidate[read_index] == 'e' || candidate[read_index] == 'E') &&
            candidate[read_index + 1U] == '+') {
            ++read_index;
        }
    }
    buffer[write_index] = '\0';
    return MYLITE_OK;
}

int mylite_execution_copy_normalized_scientific_double_text(
    struct mylite_db *database,
    const char *candidate,
    const char *function_name,
    char *buffer,
    size_t buffer_size
) {
    const char *exponent = NULL;
    size_t mantissa_end = 0U;
    size_t write_index = 0U;
    size_t exponent_index = 0U;

    if (candidate == NULL || function_name == NULL || buffer == NULL || buffer_size == 0U) {
        return MYLITE_MISUSE;
    }
    exponent = strchr(candidate, 'e');
    if (exponent == NULL) {
        exponent = strchr(candidate, 'E');
    }
    if (exponent == NULL) {
        mylite_execution_set_double_format_error(database, function_name);
        return MYLITE_ERROR;
    }

    mantissa_end = (size_t)(exponent - candidate);
    while (mantissa_end > 0U && candidate[mantissa_end - 1U] == '0') {
        --mantissa_end;
    }
    if (mantissa_end > 0U && candidate[mantissa_end - 1U] == '.') {
        --mantissa_end;
    }
    for (size_t read_index = 0U; read_index < mantissa_end; ++read_index) {
        if (write_index + 1U >= buffer_size) {
            mylite_execution_set_double_format_error(database, function_name);
            return MYLITE_ERROR;
        }
        buffer[write_index] = candidate[read_index];
        ++write_index;
    }

    if (write_index + 1U >= buffer_size) {
        mylite_execution_set_double_format_error(database, function_name);
        return MYLITE_ERROR;
    }
    buffer[write_index] = 'e';
    ++write_index;

    exponent_index = (size_t)(exponent - candidate) + 1U;
    if (candidate[exponent_index] == '+') {
        ++exponent_index;
    } else if (candidate[exponent_index] == '-') {
        if (write_index + 1U >= buffer_size) {
            mylite_execution_set_double_format_error(database, function_name);
            return MYLITE_ERROR;
        }
        buffer[write_index] = '-';
        ++write_index;
        ++exponent_index;
    }
    while (candidate[exponent_index] == '0' && candidate[exponent_index + 1U] != '\0') {
        ++exponent_index;
    }
    while (candidate[exponent_index] != '\0') {
        if (write_index + 1U >= buffer_size) {
            mylite_execution_set_double_format_error(database, function_name);
            return MYLITE_ERROR;
        }
        buffer[write_index] = candidate[exponent_index];
        ++write_index;
        ++exponent_index;
    }
    buffer[write_index] = '\0';
    return MYLITE_OK;
}

void mylite_execution_set_double_format_error(
    struct mylite_db *database,
    const char *function_name
) {
    char message[double_format_error_capacity];
    int written = snprintf(
        message,
        sizeof(message),
        "failed to format %s() value",
        function_name == NULL ? "scalar double" : function_name
    );

    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_execution_set_runtime_error(database, "failed to format scalar double value");
        return;
    }
    mylite_execution_set_runtime_error(database, message);
}
