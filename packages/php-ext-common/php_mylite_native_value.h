#ifndef PHP_MYLITE_NATIVE_VALUE_H
#define PHP_MYLITE_NATIVE_VALUE_H

#include <php.h>

#include <mylite/mylite.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static inline void mylite_php_native_value_to_zval(
    enum mylite_result_column_type type,
    const void *bytes,
    size_t byte_count,
    bool stringify,
    zval *out_value
) {
    const unsigned char *unsigned_bytes = (const unsigned char *)bytes;
    const char *text = bytes == NULL ? "" : (const char *)bytes;
    bool integral =
        type == MYLITE_RESULT_COLUMN_TYPE_TINY || type == MYLITE_RESULT_COLUMN_TYPE_SHORT ||
        type == MYLITE_RESULT_COLUMN_TYPE_LONG || type == MYLITE_RESULT_COLUMN_TYPE_LONGLONG ||
        type == MYLITE_RESULT_COLUMN_TYPE_INT24 || type == MYLITE_RESULT_COLUMN_TYPE_YEAR;

    if (type == MYLITE_RESULT_COLUMN_TYPE_BIT && bytes != NULL && byte_count <= sizeof(uint64_t)) {
        uint64_t bit_value = 0U;

        for (size_t index = 0U; index < byte_count; ++index) {
            bit_value = (bit_value << 8U) | unsigned_bytes[index];
        }
        if (!stringify && bit_value <= (uint64_t)ZEND_LONG_MAX) {
            ZVAL_LONG(out_value, (zend_long)bit_value);
            return;
        }

        char bit_text[sizeof("18446744073709551615")];
        int bit_length = snprintf(bit_text, sizeof(bit_text), "%" PRIu64, bit_value);

        if (bit_length > 0 && (size_t)bit_length < sizeof(bit_text)) {
            ZVAL_STRINGL(out_value, bit_text, (size_t)bit_length);
            return;
        }
    }

    if (integral && bytes != NULL && byte_count > 0U) {
        size_t index = 0U;
        bool negative = false;
        bool valid = true;
        zend_ulong magnitude = 0U;
        zend_ulong limit = (zend_ulong)ZEND_LONG_MAX;

        if (text[index] == '-' || text[index] == '+') {
            negative = text[index] == '-';
            ++index;
        }
        if (negative) {
            limit += 1U;
        }
        if (index == byte_count) {
            valid = false;
        }
        for (; valid && index < byte_count; ++index) {
            unsigned char digit = (unsigned char)text[index];

            if (digit < (unsigned char)'0' || digit > (unsigned char)'9') {
                valid = false;
                break;
            }
            digit = (unsigned char)(digit - (unsigned char)'0');
            if (magnitude > (limit - (zend_ulong)digit) / 10U) {
                valid = false;
                break;
            }
            magnitude = (magnitude * 10U) + (zend_ulong)digit;
        }
        if (valid && !stringify) {
            if (negative && magnitude == (zend_ulong)ZEND_LONG_MAX + 1U) {
                ZVAL_LONG(out_value, ZEND_LONG_MIN);
            } else {
                zend_long value = (zend_long)magnitude;

                ZVAL_LONG(out_value, negative ? -value : value);
            }
            return;
        }
    }

    if (!stringify &&
        (type == MYLITE_RESULT_COLUMN_TYPE_FLOAT || type == MYLITE_RESULT_COLUMN_TYPE_DOUBLE) &&
        bytes != NULL) {
        ZVAL_DOUBLE(out_value, zend_strtod(text, NULL));
        return;
    }

    ZVAL_STRINGL(out_value, text, byte_count);
}

#endif
