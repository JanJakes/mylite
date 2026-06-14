#include "mylite_catalog.h"

#include "mylite_catalog_internal.h"

#include <stddef.h>

enum {
    catalog_table_key_block_size_eight = 8,
    catalog_table_key_block_size_sixteen = 16,
    catalog_table_stats_sample_pages_max = 65535,
};

static int text_equals_ascii_case_insensitive(const char *left, const char *right);
static int text_has_ascii_case_insensitive_prefix(const char *text, const char *prefix);
static char ascii_lower(unsigned char byte);

int mylite_catalog_validate_table_descriptor_input(
    const struct mylite_catalog_table_descriptor_input *input
) {
    int rc = input == NULL ? MYLITE_MISUSE : mylite_catalog_validate_positive_id(input->schema_id);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_logical_object_name(
            input->name,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            input->physical_name,
            MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_table_kind(input->kind);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            input->default_charset,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            input->default_collation,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_optional_name(
            input->comment,
            MYLITE_CATALOG_TABLE_COMMENT_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_optional_name(
            input->row_format_option,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc == MYLITE_OK && input->row_format_option != NULL &&
        input->row_format_option[0] != '\0' &&
        !text_equals_ascii_case_insensitive(input->row_format_option, "DYNAMIC") &&
        !text_equals_ascii_case_insensitive(input->row_format_option, "COMPACT") &&
        !text_equals_ascii_case_insensitive(input->row_format_option, "REDUNDANT") &&
        !text_equals_ascii_case_insensitive(input->row_format_option, "COMPRESSED")) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && input->key_block_size != 0 && input->key_block_size != 1 &&
        input->key_block_size != 2 && input->key_block_size != 4 &&
        input->key_block_size != catalog_table_key_block_size_eight &&
        input->key_block_size != catalog_table_key_block_size_sixteen) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && input->auto_increment_status < 0) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK &&
        (input->pack_keys < -1 || input->pack_keys > 1 || input->checksum < 0 ||
         input->checksum > 1 || input->stats_persistent < -1 || input->stats_persistent > 1 ||
         input->stats_auto_recalc < -1 || input->stats_auto_recalc > 1 ||
         input->stats_sample_pages < 0 ||
         input->stats_sample_pages > catalog_table_stats_sample_pages_max ||
         input->delay_key_write < -1 || input->delay_key_write > 1 || input->min_rows < 0 ||
         input->max_rows < 0 || input->avg_row_length < 0)) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK &&
        (input->created_time_utc_epoch < 0 || input->updated_time_utc_epoch < 0)) {
        rc = MYLITE_ERROR;
    }

    return rc;
}

bool mylite_catalog_name_is_reserved(const char *name) {
    static const char prefix[] = "_mylite_";

    if (name == NULL) {
        return false;
    }

    return text_has_ascii_case_insensitive_prefix(name, prefix) != 0;
}

static int text_has_ascii_case_insensitive_prefix(const char *text, const char *prefix) {
    size_t index = 0U;

    while (prefix[index] != '\0') {
        if (text[index] == '\0' ||
            ascii_lower((unsigned char)text[index]) != ascii_lower((unsigned char)prefix[index])) {
            return 0;
        }
        ++index;
    }

    return 1;
}

static int text_equals_ascii_case_insensitive(const char *left, const char *right) {
    size_t index = 0U;

    if (left == NULL || right == NULL) {
        return 0;
    }
    while (left[index] != '\0' && right[index] != '\0') {
        if (ascii_lower((unsigned char)left[index]) != ascii_lower((unsigned char)right[index])) {
            return 0;
        }
        ++index;
    }
    return left[index] == '\0' && right[index] == '\0';
}

static char ascii_lower(unsigned char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (char)(byte + ('a' - 'A'));
    }
    return (char)byte;
}
