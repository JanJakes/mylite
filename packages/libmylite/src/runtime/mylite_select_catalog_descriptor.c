#include "mylite_select_catalog_descriptor.h"

#include "mylite_metadata_constants.h"
#include "mylite_select_catalog_descriptor_source.h"
#include "mylite_select_catalog_descriptor_type.h"
#include "mylite_span.h"

#include <stdbool.h>

static void apply_catalog_column_flags(
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor
);

static bool catalog_column_has_current_timestamp_default(
    const struct mylite_catalog_column_descriptor_source *source
);

static struct mylite_field_descriptor catalog_field_descriptor_defaults(void);

int mylite_select_catalog_load_column_descriptor(
    mylite_db *database,
    sqlite3_stmt *select,
    struct mylite_field_descriptor *out_descriptor
) {
    struct mylite_catalog_column_descriptor_source source =
        mylite_select_catalog_column_descriptor_source(select);
    struct mylite_field_descriptor descriptor = catalog_field_descriptor_defaults();
    int status = MYLITE_OK;

    descriptor.nullable = source.nullable;
    status = mylite_select_catalog_apply_column_type_descriptor(database, &source, &descriptor);
    if (status != MYLITE_OK) {
        return status;
    }
    apply_catalog_column_flags(&source, &descriptor);
    mylite_field_descriptor_set_nullable(&descriptor, source.nullable);

    *out_descriptor = descriptor;
    return MYLITE_OK;
}

static void apply_catalog_column_flags(
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor
) {
    bool is_timestamp = mylite_ascii_case_equal(source->data_type, "timestamp");
    bool has_on_update_current_timestamp =
        mylite_select_catalog_text_contains_word((struct mylite_catalog_text_match){
            .text = source->extra,
            .word = "on update CURRENT_TIMESTAMP",
        });
    bool no_default = false;

    if (sqlite3_column_type(source->select, source->column_default_index) == SQLITE_NULL) {
        if (!source->nullable && !source->auto_increment) {
            no_default = true;
        }
    }
    if (source->is_unsigned) {
        descriptor->flags |= MYLITE_FIELD_FLAG_UNSIGNED;
    }
    if (source->is_zerofill) {
        descriptor->flags |= MYLITE_FIELD_FLAG_ZEROFILL | MYLITE_FIELD_FLAG_UNSIGNED;
    }
    if (mylite_ascii_case_equal(source->column_key, "PRI")) {
        descriptor->flags |= MYLITE_FIELD_FLAG_PRI_KEY | MYLITE_FIELD_FLAG_PART_KEY;
    } else if (mylite_ascii_case_equal(source->column_key, "UNI")) {
        descriptor->flags |= MYLITE_FIELD_FLAG_UNIQUE_KEY | MYLITE_FIELD_FLAG_PART_KEY;
    } else if (mylite_ascii_case_equal(source->column_key, "MUL")) {
        descriptor->flags |= MYLITE_FIELD_FLAG_MULTIPLE_KEY | MYLITE_FIELD_FLAG_PART_KEY;
    }
    if (source->auto_increment) {
        descriptor->flags |= MYLITE_FIELD_FLAG_AUTO_INCREMENT;
    }
    if (is_timestamp &&
        (has_on_update_current_timestamp || catalog_column_has_current_timestamp_default(source))) {
        descriptor->flags |= MYLITE_FIELD_FLAG_TIMESTAMP;
    }
    if (is_timestamp && has_on_update_current_timestamp) {
        descriptor->flags |= MYLITE_FIELD_FLAG_ON_UPDATE_NOW;
    }
    if (no_default) {
        descriptor->flags |= MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE;
    }
}

static bool catalog_column_has_current_timestamp_default(
    const struct mylite_catalog_column_descriptor_source *source
) {
    const char *default_text =
        (const char *)sqlite3_column_text(source->select, source->column_default_index);

    return mylite_column_default_is_current_timestamp(default_text);
}

static struct mylite_field_descriptor catalog_field_descriptor_defaults(void) {
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_NULL,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };
}
