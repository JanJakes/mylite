#include <mylite/mylite.h>

#include "mylite_json.h"
#include "mylite_json_internal.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct json_schema_document_pair {
    const struct json_value *schema;
    const struct json_value *document;
};

struct json_schema_type_pair {
    const struct json_value *type;
    const struct json_value *document;
};

static int validate_schema_document(
    struct json_schema_document_pair pair,
    const char *schema_location,
    const char *document_location,
    struct mylite_json_schema_validation_result *result
);
static int validate_schema_members_are_supported(
    const struct json_value *schema,
    struct mylite_json_schema_validation_result *result
);
static int validate_schema_type(
    const struct json_value *type,
    const struct json_value *document,
    const char *schema_location,
    const char *document_location,
    struct mylite_json_schema_validation_result *result
);
static int validate_schema_type_array(
    struct json_schema_type_pair pair,
    const char *schema_location,
    const char *document_location,
    struct mylite_json_schema_validation_result *result
);
static int validate_schema_required(
    const struct json_value *required,
    const struct json_value *document,
    const char *schema_location,
    const char *document_location,
    struct mylite_json_schema_validation_result *result
);
static int validate_schema_properties(
    const struct json_value *properties,
    const struct json_value *document,
    const char *schema_location,
    const char *document_location,
    struct mylite_json_schema_validation_result *result
);
static int validate_schema_numeric_limit(
    const struct json_value *limit,
    const struct json_value *document,
    const char *schema_location,
    const char *document_location,
    const char *keyword,
    struct mylite_json_schema_validation_result *result
);
static bool json_schema_type_matches(
    const char *type,
    size_t type_length,
    const struct json_value *document
);
static int json_schema_number_value(const struct json_value *value, long double *out_number);
static bool json_schema_member_name_equals(const struct json_member *member, const char *name);
static bool json_schema_text_equals(const char *left, size_t left_length, const char *right);
static bool json_schema_keyword_is_supported(const struct json_member *member);
static bool json_schema_keyword_is_annotation(const struct json_member *member);
static int set_schema_validation_failure(
    struct mylite_json_schema_validation_result *result,
    const char *keyword,
    const char *schema_location,
    const char *document_location
);
static int set_schema_validation_location(const char *source, char **out_location);
static int json_pointer_property_location(
    const char *base,
    const char *property,
    size_t property_length,
    char **out_location
);
static int json_pointer_append_escaped(
    struct json_writer *writer,
    const char *property,
    size_t property_length
);
static int copy_json_schema_status(
    struct mylite_json_schema_validation_result *out_result,
    enum mylite_json_schema_validation_status status,
    const struct mylite_json_normalize_result *source_result
);
static int make_schema_validation_report_text(
    const struct mylite_json_schema_validation_result *result,
    char **out_text,
    size_t *out_text_length
);
static int append_schema_validation_report_failure(
    const struct mylite_json_schema_validation_result *result,
    struct json_writer *writer
);
static int append_schema_validation_report_reason(
    const struct mylite_json_schema_validation_result *result,
    struct json_writer *writer
);
static int append_schema_validation_failure_reason_string(
    const struct mylite_json_schema_validation_result *result,
    struct json_writer *writer
);
static int append_schema_validation_report_string_member(
    struct json_writer *writer,
    const char *name,
    const char *value
);
static int parse_json_schema_document(
    const char *text,
    size_t text_length,
    struct json_value *out_value,
    struct mylite_json_normalize_result *out_result
);

void mylite_json_schema_validation_result_deinit(struct mylite_json_schema_validation_result *result
) {
    if (result == NULL) {
        return;
    }
    free(result->schema_location);
    free(result->document_location);
    *result = (struct mylite_json_schema_validation_result){0};
}

int mylite_json_schema_validate(
    const char *schema,
    size_t schema_length,
    const char *document,
    size_t document_length,
    struct mylite_json_schema_validation_result *out_result
) {
    struct json_value schema_value = {0};
    struct json_value document_value = {0};
    struct mylite_json_normalize_result schema_result = {0};
    struct mylite_json_normalize_result document_result = {0};
    int rc = MYLITE_OK;

    if (out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = (struct mylite_json_schema_validation_result){
        .status = MYLITE_JSON_SCHEMA_VALIDATION_OK,
        .is_valid = false,
    };
    if (schema == NULL || document == NULL) {
        out_result->status = MYLITE_JSON_SCHEMA_VALIDATION_INVALID_SCHEMA_JSON;
        return MYLITE_ERROR;
    }

    rc = parse_json_schema_document(schema, schema_length, &schema_value, &schema_result);
    if (rc != MYLITE_OK) {
        rc = copy_json_schema_status(
            out_result,
            MYLITE_JSON_SCHEMA_VALIDATION_INVALID_SCHEMA_JSON,
            &schema_result
        );
        goto cleanup;
    }
    if (schema_value.kind != JSON_VALUE_OBJECT) {
        out_result->status = MYLITE_JSON_SCHEMA_VALIDATION_INVALID_SCHEMA_TYPE;
        rc = MYLITE_ERROR;
        goto cleanup;
    }

    rc = parse_json_schema_document(document, document_length, &document_value, &document_result);
    if (rc != MYLITE_OK) {
        rc = copy_json_schema_status(
            out_result,
            MYLITE_JSON_SCHEMA_VALIDATION_INVALID_DOCUMENT_JSON,
            &document_result
        );
        goto cleanup;
    }

    rc = validate_schema_document(
        (struct json_schema_document_pair){.schema = &schema_value, .document = &document_value},
        "#",
        "#",
        out_result
    );
    if (rc == MYLITE_OK && out_result->status == MYLITE_JSON_SCHEMA_VALIDATION_OK &&
        !out_result->is_valid) {
        rc = MYLITE_OK;
    }

cleanup:
    mylite_json_internal_value_deinit(&document_value);
    mylite_json_internal_value_deinit(&schema_value);
    if (rc == MYLITE_NOMEM) {
        mylite_json_schema_validation_result_deinit(out_result);
    }
    return rc;
}

int mylite_json_schema_validation_report(
    const char *schema,
    size_t schema_length,
    const char *document,
    size_t document_length,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_schema_validation_result *out_result
) {
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;

    rc = mylite_json_schema_validate(schema, schema_length, document, document_length, out_result);
    if (rc != MYLITE_OK || out_result->status != MYLITE_JSON_SCHEMA_VALIDATION_OK) {
        return rc;
    }
    return make_schema_validation_report_text(out_result, out_text, out_text_length);
}

static int validate_schema_document(
    struct json_schema_document_pair pair,
    const char *schema_location,
    const char *document_location,
    struct mylite_json_schema_validation_result *result
) {
    const struct json_value *type = NULL;
    const struct json_value *required = NULL;
    const struct json_value *properties = NULL;
    const struct json_value *minimum = NULL;
    const struct json_value *maximum = NULL;
    int rc = MYLITE_OK;

    result->is_valid = true;
    rc = validate_schema_members_are_supported(pair.schema, result);
    if (rc != MYLITE_OK) {
        return rc;
    }

    type = mylite_json_internal_object_member_value(pair.schema, "type", strlen("type"));
    if (type != NULL) {
        rc = validate_schema_type(type, pair.document, schema_location, document_location, result);
        if (rc != MYLITE_OK || !result->is_valid) {
            return rc;
        }
    }

    required =
        mylite_json_internal_object_member_value(pair.schema, "required", strlen("required"));
    if (required != NULL) {
        rc = validate_schema_required(
            required,
            pair.document,
            schema_location,
            document_location,
            result
        );
        if (rc != MYLITE_OK || !result->is_valid) {
            return rc;
        }
    }

    properties =
        mylite_json_internal_object_member_value(pair.schema, "properties", strlen("properties"));
    if (properties != NULL) {
        rc = validate_schema_properties(
            properties,
            pair.document,
            schema_location,
            document_location,
            result
        );
        if (rc != MYLITE_OK || !result->is_valid) {
            return rc;
        }
    }

    minimum = mylite_json_internal_object_member_value(pair.schema, "minimum", strlen("minimum"));
    if (minimum != NULL) {
        rc = validate_schema_numeric_limit(
            minimum,
            pair.document,
            schema_location,
            document_location,
            "minimum",
            result
        );
        if (rc != MYLITE_OK || !result->is_valid) {
            return rc;
        }
    }

    maximum = mylite_json_internal_object_member_value(pair.schema, "maximum", strlen("maximum"));
    if (maximum != NULL) {
        rc = validate_schema_numeric_limit(
            maximum,
            pair.document,
            schema_location,
            document_location,
            "maximum",
            result
        );
    }
    return rc;
}

static int validate_schema_members_are_supported(
    const struct json_value *schema,
    struct mylite_json_schema_validation_result *result
) {
    if (schema == NULL || schema->kind != JSON_VALUE_OBJECT || result == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; index < schema->payload.object.count; ++index) {
        const struct json_member *member = &schema->payload.object.members[index];

        if (json_schema_member_name_equals(member, "$ref")) {
            result->status = MYLITE_JSON_SCHEMA_VALIDATION_UNSUPPORTED_REFERENCE;
            return MYLITE_ERROR;
        }
        if (json_schema_keyword_is_annotation(member) || json_schema_keyword_is_supported(member)) {
            continue;
        }
        result->status = MYLITE_JSON_SCHEMA_VALIDATION_UNSUPPORTED_SCHEMA;
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int validate_schema_type(
    const struct json_value *type,
    const struct json_value *document,
    const char *schema_location,
    const char *document_location,
    struct mylite_json_schema_validation_result *result
) {
    if (type == NULL || document == NULL || result == NULL) {
        return MYLITE_MISUSE;
    }
    if (type->kind == JSON_VALUE_STRING) {
        if (json_schema_type_matches(
                type->payload.text.text,
                type->payload.text.length,
                document
            )) {
            return MYLITE_OK;
        }
        return set_schema_validation_failure(result, "type", schema_location, document_location);
    }
    if (type->kind == JSON_VALUE_ARRAY) {
        return validate_schema_type_array(
            (struct json_schema_type_pair){.type = type, .document = document},
            schema_location,
            document_location,
            result
        );
    }
    result->status = MYLITE_JSON_SCHEMA_VALIDATION_UNSUPPORTED_SCHEMA;
    return MYLITE_ERROR;
}

static int validate_schema_type_array(
    struct json_schema_type_pair pair,
    const char *schema_location,
    const char *document_location,
    struct mylite_json_schema_validation_result *result
) {
    if (pair.type->payload.array.count == 0U) {
        result->status = MYLITE_JSON_SCHEMA_VALIDATION_UNSUPPORTED_SCHEMA;
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; index < pair.type->payload.array.count; ++index) {
        const struct json_value *entry = &pair.type->payload.array.values[index];

        if (entry->kind != JSON_VALUE_STRING) {
            result->status = MYLITE_JSON_SCHEMA_VALIDATION_UNSUPPORTED_SCHEMA;
            return MYLITE_ERROR;
        }
        if (json_schema_type_matches(
                entry->payload.text.text,
                entry->payload.text.length,
                pair.document
            )) {
            return MYLITE_OK;
        }
    }
    return set_schema_validation_failure(result, "type", schema_location, document_location);
}

static int validate_schema_required(
    const struct json_value *required,
    const struct json_value *document,
    const char *schema_location,
    const char *document_location,
    struct mylite_json_schema_validation_result *result
) {
    if (required == NULL || document == NULL || result == NULL) {
        return MYLITE_MISUSE;
    }
    if (required->kind != JSON_VALUE_ARRAY) {
        result->status = MYLITE_JSON_SCHEMA_VALIDATION_UNSUPPORTED_SCHEMA;
        return MYLITE_ERROR;
    }
    if (document->kind != JSON_VALUE_OBJECT) {
        return MYLITE_OK;
    }

    for (size_t index = 0U; index < required->payload.array.count; ++index) {
        const struct json_value *entry = &required->payload.array.values[index];

        if (entry->kind != JSON_VALUE_STRING) {
            result->status = MYLITE_JSON_SCHEMA_VALIDATION_UNSUPPORTED_SCHEMA;
            return MYLITE_ERROR;
        }
        if (mylite_json_internal_object_member_value(
                document,
                entry->payload.text.text,
                entry->payload.text.length
            ) == NULL) {
            return set_schema_validation_failure(
                result,
                "required",
                schema_location,
                document_location
            );
        }
    }
    return MYLITE_OK;
}

static int validate_schema_properties(
    const struct json_value *properties,
    const struct json_value *document,
    const char *schema_location,
    const char *document_location,
    struct mylite_json_schema_validation_result *result
) {
    int rc = MYLITE_OK;

    if (properties == NULL || document == NULL || result == NULL) {
        return MYLITE_MISUSE;
    }
    if (properties->kind != JSON_VALUE_OBJECT) {
        result->status = MYLITE_JSON_SCHEMA_VALIDATION_UNSUPPORTED_SCHEMA;
        return MYLITE_ERROR;
    }
    if (document->kind != JSON_VALUE_OBJECT) {
        return MYLITE_OK;
    }

    for (size_t index = 0U; index < properties->payload.object.count; ++index) {
        const struct json_member *property = &properties->payload.object.members[index];
        const struct json_value *property_document =
            mylite_json_internal_object_member_value(document, property->key, property->key_length);
        char *property_schema_location = NULL;
        char *property_document_location = NULL;

        if (property->value == NULL || property->value->kind != JSON_VALUE_OBJECT) {
            result->status = MYLITE_JSON_SCHEMA_VALIDATION_UNSUPPORTED_SCHEMA;
            return MYLITE_ERROR;
        }
        if (property_document == NULL) {
            continue;
        }

        rc = json_pointer_property_location(
            schema_location,
            "properties",
            strlen("properties"),
            &property_schema_location
        );
        if (rc == MYLITE_OK) {
            char *nested_schema_location = NULL;

            rc = json_pointer_property_location(
                property_schema_location,
                property->key,
                property->key_length,
                &nested_schema_location
            );
            free(property_schema_location);
            property_schema_location = nested_schema_location;
        }
        if (rc == MYLITE_OK) {
            rc = json_pointer_property_location(
                document_location,
                property->key,
                property->key_length,
                &property_document_location
            );
        }
        if (rc == MYLITE_OK) {
            rc = validate_schema_document(
                (struct json_schema_document_pair){
                    .schema = property->value,
                    .document = property_document,
                },
                property_schema_location,
                property_document_location,
                result
            );
        }
        free(property_schema_location);
        free(property_document_location);
        if (rc != MYLITE_OK || !result->is_valid) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static int validate_schema_numeric_limit(
    const struct json_value *limit,
    const struct json_value *document,
    const char *schema_location,
    const char *document_location,
    const char *keyword,
    struct mylite_json_schema_validation_result *result
) {
    long double limit_number = 0.0L;
    long double document_number = 0.0L;
    int rc = MYLITE_OK;

    if (limit == NULL || document == NULL || keyword == NULL || result == NULL) {
        return MYLITE_MISUSE;
    }
    if (limit->kind != JSON_VALUE_NUMBER) {
        result->status = MYLITE_JSON_SCHEMA_VALIDATION_UNSUPPORTED_SCHEMA;
        return MYLITE_ERROR;
    }
    if (document->kind != JSON_VALUE_NUMBER) {
        return MYLITE_OK;
    }

    rc = json_schema_number_value(limit, &limit_number);
    if (rc == MYLITE_OK) {
        rc = json_schema_number_value(document, &document_number);
    }
    if (rc != MYLITE_OK) {
        result->status = MYLITE_JSON_SCHEMA_VALIDATION_UNSUPPORTED_SCHEMA;
        return MYLITE_ERROR;
    }
    if (json_schema_text_equals(keyword, strlen(keyword), "minimum") &&
        document_number < limit_number) {
        return set_schema_validation_failure(result, keyword, schema_location, document_location);
    }
    if (json_schema_text_equals(keyword, strlen(keyword), "maximum") &&
        document_number > limit_number) {
        return set_schema_validation_failure(result, keyword, schema_location, document_location);
    }
    return MYLITE_OK;
}

static bool json_schema_type_matches(
    const char *type,
    size_t type_length,
    const struct json_value *document
) {
    if (document == NULL) {
        return false;
    }
    if (json_schema_text_equals(type, type_length, "object")) {
        return document->kind == JSON_VALUE_OBJECT;
    }
    if (json_schema_text_equals(type, type_length, "array")) {
        return document->kind == JSON_VALUE_ARRAY;
    }
    if (json_schema_text_equals(type, type_length, "string")) {
        return document->kind == JSON_VALUE_STRING;
    }
    if (json_schema_text_equals(type, type_length, "number")) {
        return document->kind == JSON_VALUE_NUMBER;
    }
    if (json_schema_text_equals(type, type_length, "integer")) {
        return document->kind == JSON_VALUE_NUMBER && document->number_kind == JSON_NUMBER_INTEGER;
    }
    if (json_schema_text_equals(type, type_length, "boolean")) {
        return document->kind == JSON_VALUE_BOOL;
    }
    if (json_schema_text_equals(type, type_length, "null")) {
        return document->kind == JSON_VALUE_NULL;
    }
    return false;
}

static int json_schema_number_value(const struct json_value *value, long double *out_number) {
    char *end = NULL;

    if (value == NULL || value->kind != JSON_VALUE_NUMBER || out_number == NULL) {
        return MYLITE_MISUSE;
    }
    errno = 0;
    *out_number = strtold(value->payload.text.text, &end);
    if (errno == ERANGE || end == value->payload.text.text ||
        end != value->payload.text.text + value->payload.text.length) {
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static bool json_schema_member_name_equals(const struct json_member *member, const char *name) {
    if (member == NULL || name == NULL) {
        return false;
    }
    return json_schema_text_equals(member->key, member->key_length, name);
}

static bool json_schema_text_equals(const char *left, size_t left_length, const char *right) {
    size_t right_length = right == NULL ? 0U : strlen(right);

    if (left == NULL || right == NULL || left_length != right_length) {
        return false;
    }
    return memcmp(left, right, left_length) == 0;
}

static bool json_schema_keyword_is_supported(const struct json_member *member) {
    return json_schema_member_name_equals(member, "type") ||
           json_schema_member_name_equals(member, "required") ||
           json_schema_member_name_equals(member, "properties") ||
           json_schema_member_name_equals(member, "minimum") ||
           json_schema_member_name_equals(member, "maximum");
}

static bool json_schema_keyword_is_annotation(const struct json_member *member) {
    return json_schema_member_name_equals(member, "id") ||
           json_schema_member_name_equals(member, "$schema") ||
           json_schema_member_name_equals(member, "description");
}

static int set_schema_validation_failure(
    struct mylite_json_schema_validation_result *result,
    const char *keyword,
    const char *schema_location,
    const char *document_location
) {
    int rc = MYLITE_OK;

    if (result == NULL || keyword == NULL || schema_location == NULL || document_location == NULL) {
        return MYLITE_MISUSE;
    }
    result->is_valid = false;
    result->failed_keyword = keyword;
    rc = set_schema_validation_location(schema_location, &result->schema_location);
    if (rc == MYLITE_OK) {
        rc = set_schema_validation_location(document_location, &result->document_location);
    }
    return rc;
}

static int set_schema_validation_location(const char *source, char **out_location) {
    size_t length = 0U;
    char *location = NULL;

    if (source == NULL || out_location == NULL) {
        return MYLITE_MISUSE;
    }
    length = strlen(source);
    location = (char *)malloc(length + 1U);
    if (location == NULL) {
        return MYLITE_NOMEM;
    }
    memcpy(location, source, length + 1U);
    free(*out_location);
    *out_location = location;
    return MYLITE_OK;
}

static int json_pointer_property_location(
    const char *base,
    const char *property,
    size_t property_length,
    char **out_location
) {
    struct json_writer writer = {0};
    int rc = MYLITE_OK;

    if (base == NULL || property == NULL || out_location == NULL) {
        return MYLITE_MISUSE;
    }
    *out_location = NULL;
    rc = mylite_json_internal_writer_append_text(&writer, base, strlen(base));
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_writer_append_char(&writer, '/');
    }
    if (rc == MYLITE_OK) {
        rc = json_pointer_append_escaped(&writer, property, property_length);
    }
    if (rc == MYLITE_OK) {
        *out_location = mylite_json_internal_writer_take(&writer);
        if (*out_location == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    mylite_json_internal_writer_deinit(&writer);
    return rc;
}

static int json_pointer_append_escaped(
    struct json_writer *writer,
    const char *property,
    size_t property_length
) {
    if (writer == NULL || property == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; index < property_length; ++index) {
        if (property[index] == '~') {
            if (mylite_json_internal_writer_append_text(writer, "~0", 2U) != MYLITE_OK) {
                return MYLITE_NOMEM;
            }
        } else if (property[index] == '/') {
            if (mylite_json_internal_writer_append_text(writer, "~1", 2U) != MYLITE_OK) {
                return MYLITE_NOMEM;
            }
        } else if (mylite_json_internal_writer_append_char(writer, property[index]) != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

static int copy_json_schema_status(
    struct mylite_json_schema_validation_result *out_result,
    enum mylite_json_schema_validation_status status,
    const struct mylite_json_normalize_result *source_result
) {
    if (out_result == NULL || source_result == NULL) {
        return MYLITE_MISUSE;
    }
    out_result->status = status;
    if (status == MYLITE_JSON_SCHEMA_VALIDATION_INVALID_SCHEMA_JSON) {
        out_result->schema_result = *source_result;
    } else {
        out_result->document_result = *source_result;
    }
    return MYLITE_ERROR;
}

static int make_schema_validation_report_text(
    const struct mylite_json_schema_validation_result *result,
    char **out_text,
    size_t *out_text_length
) {
    struct json_writer writer = {0};
    int rc = MYLITE_OK;

    if (result == NULL || out_text == NULL || out_text_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    if (result->is_valid) {
        const char *valid_report = "{\"valid\": true}";

        rc = mylite_json_internal_writer_append_text(&writer, valid_report, strlen(valid_report));
    } else {
        rc = append_schema_validation_report_failure(result, &writer);
    }
    if (rc == MYLITE_OK) {
        *out_text_length = writer.length;
        *out_text = mylite_json_internal_writer_take(&writer);
        if (*out_text == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    mylite_json_internal_writer_deinit(&writer);
    return rc;
}

static int append_schema_validation_report_failure(
    const struct mylite_json_schema_validation_result *result,
    struct json_writer *writer
) {
    const char *schema_location = result->schema_location == NULL ? "#" : result->schema_location;
    const char *document_location =
        result->document_location == NULL ? "#" : result->document_location;
    const char *keyword = result->failed_keyword == NULL ? "type" : result->failed_keyword;
    const char *prefix = "{\"valid\": false, ";
    int rc = mylite_json_internal_writer_append_text(writer, prefix, strlen(prefix));

    if (rc == MYLITE_OK) {
        rc = append_schema_validation_report_reason(result, writer);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_writer_append_text(writer, ", ", 2U);
    }
    if (rc == MYLITE_OK) {
        rc = append_schema_validation_report_string_member(
            writer,
            "schema-location",
            schema_location
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_writer_append_text(writer, ", ", 2U);
    }
    if (rc == MYLITE_OK) {
        rc = append_schema_validation_report_string_member(
            writer,
            "document-location",
            document_location
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_writer_append_text(writer, ", ", 2U);
    }
    if (rc == MYLITE_OK) {
        rc =
            append_schema_validation_report_string_member(writer, "schema-failed-keyword", keyword);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_writer_append_char(writer, '}');
    }
    return rc;
}

static int append_schema_validation_report_reason(
    const struct mylite_json_schema_validation_result *result,
    struct json_writer *writer
) {
    int rc = mylite_json_internal_emit_string(writer, "reason", strlen("reason"));

    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_writer_append_text(writer, ": ", 2U);
    }
    if (rc == MYLITE_OK) {
        rc = append_schema_validation_failure_reason_string(result, writer);
    }
    return rc;
}

static int append_schema_validation_failure_reason_string(
    const struct mylite_json_schema_validation_result *result,
    struct json_writer *writer
) {
    struct json_writer reason = {0};
    const char *schema_location = result->schema_location == NULL ? "#" : result->schema_location;
    const char *document_location =
        result->document_location == NULL ? "#" : result->document_location;
    const char *keyword = result->failed_keyword == NULL ? "type" : result->failed_keyword;
    const char *prefix = "The JSON document location '";
    const char *middle = "' failed requirement '";
    const char *suffix = "' at JSON Schema location '";
    int rc = mylite_json_internal_writer_append_text(&reason, prefix, strlen(prefix));

    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_writer_append_text(
            &reason,
            document_location,
            strlen(document_location)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_writer_append_text(&reason, middle, strlen(middle));
    }
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_writer_append_text(&reason, keyword, strlen(keyword));
    }
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_writer_append_text(&reason, suffix, strlen(suffix));
    }
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_writer_append_text(
            &reason,
            schema_location,
            strlen(schema_location)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_writer_append_char(&reason, '\'');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_emit_string(writer, reason.text, reason.length);
    }
    mylite_json_internal_writer_deinit(&reason);
    return rc;
}

static int append_schema_validation_report_string_member(
    struct json_writer *writer,
    const char *name,
    const char *value
) {
    int rc = MYLITE_OK;

    if (writer == NULL || name == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_json_internal_emit_string(writer, name, strlen(name));
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_writer_append_text(writer, ": ", 2U);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_emit_string(writer, value, strlen(value));
    }
    return rc;
}

static int parse_json_schema_document(
    const char *text,
    size_t text_length,
    struct json_value *out_value,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser parser = {
        .text = text,
        .length = text_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    int rc = MYLITE_OK;

    if (out_value == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    rc = mylite_json_internal_parse_document(&parser, out_value);
    *out_result = parser.result;
    return rc;
}
