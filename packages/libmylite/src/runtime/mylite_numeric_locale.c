#if defined(__linux__) && !defined(_XOPEN_SOURCE)
#  define _XOPEN_SOURCE 700 /* NOLINT(bugprone-reserved-identifier): POSIX feature macro. */
#endif

#include "mylite_numeric_locale.h"

#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

enum numeric_locale_state {
    NUMERIC_LOCALE_UNINITIALIZED = 0,
    NUMERIC_LOCALE_INITIALIZING,
    NUMERIC_LOCALE_READY,
    NUMERIC_LOCALE_FAILED,
};

#ifdef _WIN32
typedef _locale_t mylite_numeric_locale;
#else
typedef locale_t mylite_numeric_locale;
#endif

static bool acquire_numeric_locale(mylite_numeric_locale *out_locale);
#ifndef _WIN32
static double parse_double_with_numeric_locale(locale_t locale, const char *text, char **out_end);
static long double parse_long_double_with_numeric_locale(
    locale_t locale,
    const char *text,
    char **out_end
);
#endif
static atomic_int shared_numeric_locale_state = NUMERIC_LOCALE_UNINITIALIZED;
static mylite_numeric_locale shared_numeric_locale;

double mylite_numeric_parse_double(const char *text, char **out_end) {
    mylite_numeric_locale locale;

    if (text == NULL || !acquire_numeric_locale(&locale)) {
        if (out_end != NULL) {
            *out_end = text == NULL ? NULL : (char *)text;
        }
        errno = EINVAL;
        return 0.0;
    }
#ifdef _WIN32
    return _strtod_l(text, out_end, locale);
#else
    return parse_double_with_numeric_locale(locale, text, out_end);
#endif
}

long double mylite_numeric_parse_long_double(const char *text, char **out_end) {
    mylite_numeric_locale locale;

    if (text == NULL || !acquire_numeric_locale(&locale)) {
        if (out_end != NULL) {
            *out_end = text == NULL ? NULL : (char *)text;
        }
        errno = EINVAL;
        return 0.0L;
    }
#ifdef _WIN32
    return _strtold_l(text, out_end, locale);
#else
    return parse_long_double_with_numeric_locale(locale, text, out_end);
#endif
}

int mylite_numeric_format(char *buffer, size_t buffer_size, const char *format, ...) {
    va_list arguments;
    int written = -1;

    va_start(arguments, format);
    written = mylite_numeric_vformat(buffer, buffer_size, format, arguments);
    va_end(arguments);
    return written;
}

int mylite_numeric_vformat(
    char *buffer,
    size_t buffer_size,
    const char *format,
    va_list arguments
) {
    mylite_numeric_locale locale;

    if (buffer == NULL || buffer_size == 0U || format == NULL || !acquire_numeric_locale(&locale)) {
        errno = EINVAL;
        return -1;
    }

#ifdef _WIN32
    return _vsnprintf_l(buffer, buffer_size, format, locale, arguments);
#else
    locale_t previous_locale = uselocale(locale);
    int written = -1;

    if (previous_locale == (locale_t)0) {
        return -1;
    }
    written = vsnprintf(buffer, buffer_size, format, arguments);
    if (uselocale(previous_locale) == (locale_t)0) {
        return -1;
    }
    return written;
#endif
}

#ifndef _WIN32
static double parse_double_with_numeric_locale(locale_t locale, const char *text, char **out_end) {
    locale_t previous_locale = uselocale(locale);
    double value = 0.0;
    int parse_errno = 0;

    if (previous_locale == (locale_t)0) {
        if (out_end != NULL) {
            *out_end = (char *)text;
        }
        errno = EINVAL;
        return 0.0;
    }
    value = strtod(text, out_end);
    parse_errno = errno;
    if (uselocale(previous_locale) == (locale_t)0) {
        if (out_end != NULL) {
            *out_end = (char *)text;
        }
        errno = EINVAL;
        return 0.0;
    }
    errno = parse_errno;
    return value;
}

static long double parse_long_double_with_numeric_locale(
    locale_t locale,
    const char *text,
    char **out_end
) {
    locale_t previous_locale = uselocale(locale);
    long double value = 0.0L;
    int parse_errno = 0;

    if (previous_locale == (locale_t)0) {
        if (out_end != NULL) {
            *out_end = (char *)text;
        }
        errno = EINVAL;
        return 0.0L;
    }
    value = strtold(text, out_end);
    parse_errno = errno;
    if (uselocale(previous_locale) == (locale_t)0) {
        if (out_end != NULL) {
            *out_end = (char *)text;
        }
        errno = EINVAL;
        return 0.0L;
    }
    errno = parse_errno;
    return value;
}
#endif

static bool acquire_numeric_locale(mylite_numeric_locale *out_locale) {
    int state = atomic_load_explicit(&shared_numeric_locale_state, memory_order_acquire);

    if (out_locale == NULL) {
        return false;
    }
    if (state == NUMERIC_LOCALE_UNINITIALIZED) {
        int expected = NUMERIC_LOCALE_UNINITIALIZED;

        if (atomic_compare_exchange_strong_explicit(
                &shared_numeric_locale_state,
                &expected,
                NUMERIC_LOCALE_INITIALIZING,
                memory_order_acq_rel,
                memory_order_acquire
            )) {
#ifdef _WIN32
            shared_numeric_locale = _create_locale(LC_NUMERIC, "C");
#else
            shared_numeric_locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
#endif
            atomic_store_explicit(
                &shared_numeric_locale_state,
                shared_numeric_locale == (mylite_numeric_locale)0 ? NUMERIC_LOCALE_FAILED
                                                                  : NUMERIC_LOCALE_READY,
                memory_order_release
            );
        }
    }

    do {
        state = atomic_load_explicit(&shared_numeric_locale_state, memory_order_acquire);
    } while (state == NUMERIC_LOCALE_INITIALIZING);
    if (state != NUMERIC_LOCALE_READY) {
        return false;
    }
    *out_locale = shared_numeric_locale;
    return true;
}
