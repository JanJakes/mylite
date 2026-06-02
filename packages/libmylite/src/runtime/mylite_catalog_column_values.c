#include "mylite_catalog.h"

#include "mylite_catalog_internal.h"
#include "sqlite3.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct catalog_column_default_bind_indexes {
    int default_kind;
    int default_integer;
    int default_text;
    int on_update_current_timestamp;
};

struct catalog_column_text_attribute_bind_indexes {
    int character_set_name;
    int collation_name;
    int comment;
};

struct catalog_column_generated_bind_indexes {
    int is_generated;
    int generated_kind;
    int generation_expression;
    int sqlite_generation_expression;
};

static int bind_catalog_column_insert_core_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    int64_t ordinal_position,
    const struct mylite_catalog_column_values *values
);
static int bind_catalog_column_replace_core_values(
    sqlite3_stmt *statement,
    const struct mylite_catalog_column_values *values
);
static int bind_catalog_column_default_values(
    sqlite3_stmt *statement,
    struct catalog_column_default_bind_indexes indexes,
    const struct mylite_catalog_column_values *values
);
static int bind_catalog_column_text_attributes(
    sqlite3_stmt *statement,
    struct catalog_column_text_attribute_bind_indexes indexes,
    const struct mylite_catalog_column_values *values
);
static int bind_catalog_column_generated_values(
    sqlite3_stmt *statement,
    struct catalog_column_generated_bind_indexes indexes,
    const struct mylite_catalog_column_values *values
);
static const char *catalog_text_or_empty(const char *value);
static int validate_catalog_column_default_value(const struct mylite_catalog_column_values *values);
static int validate_catalog_generated_column_value(
    const struct mylite_catalog_column_values *values
);
static int validate_catalog_current_timestamp_default_value(
    const struct mylite_catalog_column_values *values
);
static int validate_catalog_current_date_default_value(
    const struct mylite_catalog_column_values *values
);
static int validate_catalog_current_time_default_value(
    const struct mylite_catalog_column_values *values
);
static int catalog_default_text_length(const char *default_text, size_t *out_text_length);
static int validate_catalog_text_default_value(
    const struct mylite_catalog_column_values *values,
    size_t text_length
);
static bool catalog_logical_type_accepts_integer_expression_default(const char *logical_type);
static bool catalog_logical_type_accepts_text_expression_default(const char *logical_type);
static bool catalog_logical_type_accepts_current_timestamp(const char *logical_type);
static bool catalog_logical_type_accepts_current_date(const char *logical_type);
static bool catalog_logical_type_accepts_current_time(const char *logical_type);
static bool catalog_logical_type_accepts_text_default(const char *logical_type);
static bool catalog_logical_type_accepts_binary_default(const char *logical_type);
static bool catalog_logical_type_is_binary_blob_family(const char *logical_type);
static bool catalog_default_text_is_hex(const char *text, size_t text_length);
static bool catalog_logical_type_is_bit(const char *logical_type);
static bool catalog_logical_type_is_text_family(const char *logical_type);
static bool catalog_logical_type_accepts_empty_text_default(const char *logical_type);
static bool catalog_logical_type_equals(const char *logical_type, const char *expected);
static int text_has_ascii_case_insensitive_prefix(const char *text, const char *prefix);
static char ascii_lower(unsigned char byte);

enum catalog_column_insert_bind_index {
    catalog_column_insert_table_id_bind = 1,
    catalog_column_insert_ordinal_position_bind = 2,
    catalog_column_insert_name_bind = 3,
    catalog_column_insert_logical_type_bind = 4,
    catalog_column_insert_physical_type_bind = 5,
    catalog_column_insert_is_nullable_bind = 6,
    catalog_column_insert_is_visible_bind = 7,
    catalog_column_insert_is_auto_increment_bind = 8,
    catalog_column_insert_default_kind_bind = 9,
    catalog_column_insert_default_integer_bind = 10,
    catalog_column_insert_default_text_bind = 11,
    catalog_column_insert_on_update_current_timestamp_bind = 12,
    catalog_column_insert_character_set_name_bind = 13,
    catalog_column_insert_collation_name_bind = 14,
    catalog_column_insert_comment_bind = 15,
    catalog_column_insert_is_generated_bind = 16,
    catalog_column_insert_generated_kind_bind = 17,
    catalog_column_insert_generation_expression_bind = 18,
    catalog_column_insert_sqlite_generation_expression_bind = 19,
    catalog_column_insert_generation_bind = 20,
};

enum catalog_column_replace_bind_index {
    catalog_column_replace_name_bind = 1,
    catalog_column_replace_logical_type_bind = 2,
    catalog_column_replace_physical_type_bind = 3,
    catalog_column_replace_is_nullable_bind = 4,
    catalog_column_replace_is_visible_bind = 5,
    catalog_column_replace_is_auto_increment_bind = 6,
    catalog_column_replace_default_kind_bind = 7,
    catalog_column_replace_default_integer_bind = 8,
    catalog_column_replace_default_text_bind = 9,
    catalog_column_replace_on_update_current_timestamp_bind = 10,
    catalog_column_replace_character_set_name_bind = 11,
    catalog_column_replace_collation_name_bind = 12,
    catalog_column_replace_comment_bind = 13,
    catalog_column_replace_is_generated_bind = 14,
    catalog_column_replace_generated_kind_bind = 15,
    catalog_column_replace_generation_expression_bind = 16,
    catalog_column_replace_sqlite_generation_expression_bind = 17,
    catalog_column_replace_generation_bind = 18,
    catalog_column_replace_table_id_bind = 19,
    catalog_column_replace_column_id_bind = 20,
};

int mylite_catalog_bind_column_insert_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    int64_t ordinal_position,
    const struct mylite_catalog_column_values *values,
    uint64_t generation
) {
    int rc = bind_catalog_column_insert_core_values(statement, table_id, ordinal_position, values);

    if (rc == MYLITE_OK) {
        rc = bind_catalog_column_default_values(
            statement,
            (struct catalog_column_default_bind_indexes){
                .default_kind = catalog_column_insert_default_kind_bind,
                .default_integer = catalog_column_insert_default_integer_bind,
                .default_text = catalog_column_insert_default_text_bind,
                .on_update_current_timestamp =
                    catalog_column_insert_on_update_current_timestamp_bind,
            },
            values
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_catalog_column_text_attributes(
            statement,
            (struct catalog_column_text_attribute_bind_indexes){
                .character_set_name = catalog_column_insert_character_set_name_bind,
                .collation_name = catalog_column_insert_collation_name_bind,
                .comment = catalog_column_insert_comment_bind,
            },
            values
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_catalog_column_generated_values(
            statement,
            (struct catalog_column_generated_bind_indexes){
                .is_generated = catalog_column_insert_is_generated_bind,
                .generated_kind = catalog_column_insert_generated_kind_bind,
                .generation_expression = catalog_column_insert_generation_expression_bind,
                .sqlite_generation_expression =
                    catalog_column_insert_sqlite_generation_expression_bind,
            },
            values
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, catalog_column_insert_generation_bind, generation);
    }

    return rc;
}

int mylite_catalog_bind_column_replace_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    int64_t column_id,
    const struct mylite_catalog_column_values *values,
    uint64_t generation
) {
    int rc = bind_catalog_column_replace_core_values(statement, values);

    if (rc == MYLITE_OK) {
        rc = bind_catalog_column_default_values(
            statement,
            (struct catalog_column_default_bind_indexes){
                .default_kind = catalog_column_replace_default_kind_bind,
                .default_integer = catalog_column_replace_default_integer_bind,
                .default_text = catalog_column_replace_default_text_bind,
                .on_update_current_timestamp =
                    catalog_column_replace_on_update_current_timestamp_bind,
            },
            values
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_catalog_column_text_attributes(
            statement,
            (struct catalog_column_text_attribute_bind_indexes){
                .character_set_name = catalog_column_replace_character_set_name_bind,
                .collation_name = catalog_column_replace_collation_name_bind,
                .comment = catalog_column_replace_comment_bind,
            },
            values
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_catalog_column_generated_values(
            statement,
            (struct catalog_column_generated_bind_indexes){
                .is_generated = catalog_column_replace_is_generated_bind,
                .generated_kind = catalog_column_replace_generated_kind_bind,
                .generation_expression = catalog_column_replace_generation_expression_bind,
                .sqlite_generation_expression =
                    catalog_column_replace_sqlite_generation_expression_bind,
            },
            values
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, catalog_column_replace_generation_bind, generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_column_replace_table_id_bind, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_column_replace_column_id_bind, column_id);
    }

    return rc;
}

int mylite_catalog_validate_column_values(
    const struct mylite_catalog_column_values *values,
    bool use_logical_object_name
) {
    int rc = MYLITE_OK;

    if (use_logical_object_name) {
        rc = mylite_catalog_validate_logical_object_name(
            values->name,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    } else {
        rc =
            mylite_catalog_validate_required_name(values->name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(
        values->logical_type,
        MYLITE_CATALOG_TYPE_NAME_CAPACITY
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(
        values->physical_type,
        MYLITE_CATALOG_TYPE_NAME_CAPACITY
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_optional_name(
        values->character_set_name,
        MYLITE_CATALOG_IDENTIFIER_CAPACITY
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_optional_name(
        values->collation_name,
        MYLITE_CATALOG_IDENTIFIER_CAPACITY
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_optional_name(
        values->comment,
        MYLITE_CATALOG_COLUMN_COMMENT_CAPACITY
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = validate_catalog_column_default_value(values);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return validate_catalog_generated_column_value(values);
}

bool mylite_catalog_column_default_kind_stores_integer(
    enum mylite_catalog_column_default_kind default_kind
) {
    return (default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION) != 0;
}

bool mylite_catalog_column_default_kind_stores_text(
    enum mylite_catalog_column_default_kind default_kind
) {
    return (default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION) != 0;
}

static int bind_catalog_column_insert_core_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    int64_t ordinal_position,
    const struct mylite_catalog_column_values *values
) {
    int rc = mylite_catalog_bind_i64(statement, catalog_column_insert_table_id_bind, table_id);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_insert_ordinal_position_bind,
            ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, catalog_column_insert_name_bind, values->name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_column_insert_logical_type_bind,
            values->logical_type
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_column_insert_physical_type_bind,
            values->physical_type
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_insert_is_nullable_bind,
            mylite_catalog_bool_value(values->is_nullable)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_insert_is_visible_bind,
            mylite_catalog_bool_value(values->is_visible)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_insert_is_auto_increment_bind,
            mylite_catalog_bool_value(values->is_auto_increment)
        );
    }

    return rc;
}

static int bind_catalog_column_replace_core_values(
    sqlite3_stmt *statement,
    const struct mylite_catalog_column_values *values
) {
    int rc = mylite_catalog_bind_text(statement, catalog_column_replace_name_bind, values->name);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_column_replace_logical_type_bind,
            values->logical_type
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_column_replace_physical_type_bind,
            values->physical_type
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_replace_is_nullable_bind,
            mylite_catalog_bool_value(values->is_nullable)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_replace_is_visible_bind,
            mylite_catalog_bool_value(values->is_visible)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_replace_is_auto_increment_bind,
            mylite_catalog_bool_value(values->is_auto_increment)
        );
    }

    return rc;
}

static int bind_catalog_column_default_values(
    sqlite3_stmt *statement,
    struct catalog_column_default_bind_indexes indexes,
    const struct mylite_catalog_column_values *values
) {
    int rc =
        mylite_catalog_bind_i64(statement, indexes.default_kind, (int64_t)values->default_kind);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_nullable_i64(
            statement,
            indexes.default_integer,
            mylite_catalog_column_default_kind_stores_integer(values->default_kind),
            values->default_integer
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_nullable_text(
            statement,
            indexes.default_text,
            mylite_catalog_column_default_kind_stores_text(values->default_kind),
            values->default_text
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            indexes.on_update_current_timestamp,
            mylite_catalog_bool_value(values->on_update_current_timestamp)
        );
    }

    return rc;
}

static int bind_catalog_column_text_attributes(
    sqlite3_stmt *statement,
    struct catalog_column_text_attribute_bind_indexes indexes,
    const struct mylite_catalog_column_values *values
) {
    int rc = mylite_catalog_bind_text(
        statement,
        indexes.character_set_name,
        catalog_text_or_empty(values->character_set_name)
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            indexes.collation_name,
            catalog_text_or_empty(values->collation_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            indexes.comment,
            catalog_text_or_empty(values->comment)
        );
    }

    return rc;
}

static int bind_catalog_column_generated_values(
    sqlite3_stmt *statement,
    struct catalog_column_generated_bind_indexes indexes,
    const struct mylite_catalog_column_values *values
) {
    int rc = mylite_catalog_bind_i64(
        statement,
        indexes.is_generated,
        mylite_catalog_bool_value(values->is_generated)
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            indexes.generated_kind,
            (int64_t)values->generated_kind
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            indexes.generation_expression,
            catalog_text_or_empty(values->generation_expression)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            indexes.sqlite_generation_expression,
            catalog_text_or_empty(values->sqlite_generation_expression)
        );
    }

    return rc;
}

static const char *catalog_text_or_empty(const char *value) {
    if (value == NULL) {
        return "";
    }
    return value;
}

static int validate_catalog_column_default_value(
    const struct mylite_catalog_column_values *values
) {
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (values == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_catalog_validate_column_default_kind(values->default_kind);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (values->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP) {
        return validate_catalog_current_timestamp_default_value(values);
    }
    if (values->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE) {
        return validate_catalog_current_date_default_value(values);
    }
    if (values->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME) {
        return validate_catalog_current_time_default_value(values);
    }
    if (values->on_update_current_timestamp &&
        !catalog_logical_type_accepts_current_timestamp(values->logical_type)) {
        return MYLITE_MISUSE;
    }
    if (!mylite_catalog_column_default_kind_stores_text(values->default_kind)) {
        return MYLITE_OK;
    }
    rc = catalog_default_text_length(values->default_text, &text_length);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return validate_catalog_text_default_value(values, text_length);
}

static int validate_catalog_generated_column_value(
    const struct mylite_catalog_column_values *values
) {
    size_t expression_length = 0U;
    size_t sqlite_expression_length = 0U;

    if (values == NULL) {
        return MYLITE_MISUSE;
    }
    if (!values->is_generated) {
        if (values->generated_kind != MYLITE_CATALOG_GENERATED_COLUMN_INVALID ||
            values->generation_expression == NULL || values->generation_expression[0] != '\0' ||
            values->sqlite_generation_expression == NULL ||
            values->sqlite_generation_expression[0] != '\0') {
            return MYLITE_MISUSE;
        }
        return MYLITE_OK;
    }
    if (values->generated_kind != MYLITE_CATALOG_GENERATED_COLUMN_VIRTUAL &&
        values->generated_kind != MYLITE_CATALOG_GENERATED_COLUMN_STORED) {
        return MYLITE_MISUSE;
    }
    if (values->generation_expression == NULL || values->sqlite_generation_expression == NULL) {
        return MYLITE_MISUSE;
    }

    expression_length = strlen(values->generation_expression);
    sqlite_expression_length = strlen(values->sqlite_generation_expression);
    if (expression_length == 0U ||
        expression_length >= MYLITE_CATALOG_GENERATION_EXPRESSION_CAPACITY ||
        sqlite_expression_length == 0U ||
        sqlite_expression_length >= MYLITE_CATALOG_GENERATION_EXPRESSION_CAPACITY) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_catalog_current_timestamp_default_value(
    const struct mylite_catalog_column_values *values
) {
    if (!catalog_logical_type_accepts_current_timestamp(values->logical_type)) {
        return MYLITE_MISUSE;
    }
    if (values->default_text != NULL && values->default_text[0] != '\0') {
        return MYLITE_MISUSE;
    }
    return MYLITE_OK;
}

static int validate_catalog_current_date_default_value(
    const struct mylite_catalog_column_values *values
) {
    if (!catalog_logical_type_accepts_current_date(values->logical_type)) {
        return MYLITE_MISUSE;
    }
    if (values->default_text != NULL && values->default_text[0] != '\0') {
        return MYLITE_MISUSE;
    }
    return MYLITE_OK;
}

static int validate_catalog_current_time_default_value(
    const struct mylite_catalog_column_values *values
) {
    if (!catalog_logical_type_accepts_current_time(values->logical_type)) {
        return MYLITE_MISUSE;
    }
    if (values->default_text != NULL && values->default_text[0] != '\0') {
        return MYLITE_MISUSE;
    }
    return MYLITE_OK;
}

static int catalog_default_text_length(const char *default_text, size_t *out_text_length) {
    size_t text_length = 0U;

    if (default_text == NULL) {
        return MYLITE_MISUSE;
    }
    for (; text_length < MYLITE_CATALOG_DEFAULT_TEXT_CAPACITY; ++text_length) {
        if (default_text[text_length] == '\0') {
            break;
        }
    }
    if (text_length == MYLITE_CATALOG_DEFAULT_TEXT_CAPACITY) {
        return MYLITE_MISUSE;
    }
    *out_text_length = text_length;
    return MYLITE_OK;
}

static int validate_catalog_text_default_value(
    const struct mylite_catalog_column_values *values,
    size_t text_length
) {
    if (values->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL) {
        if (text_length == 0U ||
            !text_has_ascii_case_insensitive_prefix(values->logical_type, "DECIMAL(")) {
            return MYLITE_MISUSE;
        }
        return MYLITE_OK;
    }
    if (values->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION) {
        if (text_length == 0U ||
            !catalog_logical_type_accepts_integer_expression_default(values->logical_type)) {
            return MYLITE_MISUSE;
        }
        return MYLITE_OK;
    }
    if (values->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION) {
        if ((!catalog_logical_type_accepts_integer_expression_default(values->logical_type) &&
             !catalog_logical_type_accepts_text_expression_default(values->logical_type) &&
             !catalog_logical_type_is_text_family(values->logical_type) &&
             !catalog_logical_type_is_binary_blob_family(values->logical_type)) ||
            strcmp(values->default_text, "NULL") != 0) {
            return MYLITE_MISUSE;
        }
        return MYLITE_OK;
    }
    if (values->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY) {
        if (!catalog_logical_type_accepts_binary_default(values->logical_type) ||
            !catalog_default_text_is_hex(values->default_text, text_length)) {
            return MYLITE_MISUSE;
        }
        return MYLITE_OK;
    }
    if (values->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION) {
        if (!catalog_logical_type_accepts_text_expression_default(values->logical_type)) {
            return MYLITE_MISUSE;
        }
        return MYLITE_OK;
    }
    if (!catalog_logical_type_accepts_text_default(values->logical_type)) {
        return MYLITE_MISUSE;
    }
    if (text_length == 0U &&
        !catalog_logical_type_accepts_empty_text_default(values->logical_type)) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static bool catalog_logical_type_accepts_integer_expression_default(const char *logical_type) {
    return (catalog_logical_type_equals(logical_type, "TINYINT") ||
            catalog_logical_type_equals(logical_type, "TINYINT(1)") ||
            catalog_logical_type_equals(logical_type, "TINYINT UNSIGNED") ||
            catalog_logical_type_equals(logical_type, "SMALLINT") ||
            catalog_logical_type_equals(logical_type, "SMALLINT UNSIGNED") ||
            catalog_logical_type_equals(logical_type, "MEDIUMINT") ||
            catalog_logical_type_equals(logical_type, "MEDIUMINT UNSIGNED") ||
            catalog_logical_type_equals(logical_type, "INT") ||
            catalog_logical_type_equals(logical_type, "INT UNSIGNED") ||
            catalog_logical_type_equals(logical_type, "BIGINT") ||
            catalog_logical_type_equals(logical_type, "BIGINT UNSIGNED") ||
            catalog_logical_type_equals(logical_type, "YEAR") ||
            text_has_ascii_case_insensitive_prefix(logical_type, "BIT(")) != 0;
}

static bool catalog_logical_type_accepts_text_expression_default(const char *logical_type) {
    return (text_has_ascii_case_insensitive_prefix(logical_type, "CHAR(") ||
            text_has_ascii_case_insensitive_prefix(logical_type, "VARCHAR(") ||
            text_has_ascii_case_insensitive_prefix(logical_type, "NCHAR(") ||
            text_has_ascii_case_insensitive_prefix(logical_type, "NVARCHAR(")) != 0;
}

static bool catalog_logical_type_accepts_current_timestamp(const char *logical_type) {
    return (catalog_logical_type_equals(logical_type, "DATETIME") ||
            catalog_logical_type_equals(logical_type, "TIMESTAMP")) != 0;
}

static bool catalog_logical_type_accepts_current_date(const char *logical_type) {
    return catalog_logical_type_equals(logical_type, "DATE");
}

static bool catalog_logical_type_accepts_current_time(const char *logical_type) {
    return catalog_logical_type_equals(logical_type, "TIME");
}

static bool catalog_logical_type_accepts_text_default(const char *logical_type) {
    if (catalog_logical_type_accepts_empty_text_default(logical_type)) {
        return true;
    }
    if (catalog_logical_type_is_text_family(logical_type)) {
        return true;
    }
    if (text_has_ascii_case_insensitive_prefix(logical_type, "ENUM(") != 0) {
        return true;
    }
    if (text_has_ascii_case_insensitive_prefix(logical_type, "SET(") != 0) {
        return true;
    }
    if (catalog_logical_type_is_bit(logical_type)) {
        return true;
    }
    if (catalog_logical_type_equals(logical_type, "DATE") ||
        catalog_logical_type_equals(logical_type, "TIME") ||
        catalog_logical_type_equals(logical_type, "DATETIME") ||
        catalog_logical_type_equals(logical_type, "TIMESTAMP") ||
        catalog_logical_type_equals(logical_type, "YEAR") ||
        catalog_logical_type_equals(logical_type, "FLOAT") ||
        catalog_logical_type_equals(logical_type, "FLOAT UNSIGNED") ||
        catalog_logical_type_equals(logical_type, "DOUBLE") ||
        catalog_logical_type_equals(logical_type, "DOUBLE UNSIGNED")) {
        return true;
    }

    return false;
}

static bool catalog_logical_type_accepts_binary_default(const char *logical_type) {
    return (text_has_ascii_case_insensitive_prefix(logical_type, "BINARY(") ||
            text_has_ascii_case_insensitive_prefix(logical_type, "VARBINARY(") ||
            catalog_logical_type_is_binary_blob_family(logical_type)) != 0;
}

static bool catalog_logical_type_is_binary_blob_family(const char *logical_type) {
    return (catalog_logical_type_equals(logical_type, "TINYBLOB") ||
            catalog_logical_type_equals(logical_type, "BLOB") ||
            catalog_logical_type_equals(logical_type, "MEDIUMBLOB") ||
            catalog_logical_type_equals(logical_type, "LONGBLOB")) != 0;
}

static bool catalog_default_text_is_hex(const char *text, size_t text_length) {
    if (text == NULL || (text_length % 2U) != 0U) {
        return false;
    }

    for (size_t index = 0U; index < text_length; ++index) {
        char byte = text[index];

        if ((byte >= '0' && byte <= '9') || (byte >= 'A' && byte <= 'F')) {
            continue;
        }
        return false;
    }
    return true;
}

static bool catalog_logical_type_is_bit(const char *logical_type) {
    size_t index = 0U;

    if (logical_type == NULL || text_has_ascii_case_insensitive_prefix(logical_type, "BIT(") == 0) {
        return false;
    }
    index = sizeof("BIT(") - 1U;
    if (logical_type[index] < '1' || logical_type[index] > '9') {
        return false;
    }
    while (logical_type[index] >= '0' && logical_type[index] <= '9') {
        ++index;
    }

    if (logical_type[index] != ')') {
        return false;
    }
    if (logical_type[index + 1U] != '\0') {
        return false;
    }
    return true;
}

static bool catalog_logical_type_is_text_family(const char *logical_type) {
    return (catalog_logical_type_equals(logical_type, "TINYTEXT") ||
            catalog_logical_type_equals(logical_type, "TEXT") ||
            catalog_logical_type_equals(logical_type, "MEDIUMTEXT") ||
            catalog_logical_type_equals(logical_type, "LONGTEXT")) != 0;
}

static bool catalog_logical_type_accepts_empty_text_default(const char *logical_type) {
    if (catalog_logical_type_is_text_family(logical_type)) {
        return true;
    }
    if (text_has_ascii_case_insensitive_prefix(logical_type, "CHAR(") != 0) {
        return true;
    }
    if (text_has_ascii_case_insensitive_prefix(logical_type, "NCHAR(") != 0) {
        return true;
    }
    if (text_has_ascii_case_insensitive_prefix(logical_type, "VARCHAR(") != 0) {
        return true;
    }
    if (text_has_ascii_case_insensitive_prefix(logical_type, "NVARCHAR(") != 0) {
        return true;
    }
    if (text_has_ascii_case_insensitive_prefix(logical_type, "ENUM(") != 0) {
        return true;
    }
    if (text_has_ascii_case_insensitive_prefix(logical_type, "SET(") != 0) {
        return true;
    }

    return false;
}

static bool catalog_logical_type_equals(const char *logical_type, const char *expected) {
    size_t index = 0U;

    if (logical_type == NULL || expected == NULL) {
        return false;
    }
    for (; logical_type[index] != '\0' && expected[index] != '\0'; ++index) {
        if (ascii_lower((unsigned char)logical_type[index]) !=
            ascii_lower((unsigned char)expected[index])) {
            return false;
        }
    }

    if (logical_type[index] == '\0' && expected[index] == '\0') {
        return true;
    }

    return false;
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

static char ascii_lower(unsigned char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (char)(byte + ('a' - 'A'));
    }
    return (char)byte;
}
