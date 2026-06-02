#include <mylite/mylite.h>

#include "mylite_json.h"
#include "mylite_json_internal.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int mutate_json_document(
    const char *text,
    size_t text_length,
    const char *const *paths,
    const size_t *path_lengths,
    enum json_mutation_mode mode,
    const struct mylite_json_sql_value *values,
    size_t pair_count,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
);
static int json_value_from_sql_value(
    const struct mylite_json_sql_value *sql_value,
    struct json_value *out_value,
    struct mylite_json_normalize_result *out_result
);
static int json_object_key_from_sql_value(
    const struct mylite_json_sql_value *sql_value,
    char **out_key,
    size_t *out_key_length
);
static int copy_integer_text(int64_t value, char **out_text, size_t *out_text_length);
static int emit_constructed_json(
    struct json_value *value,
    char **out_text,
    size_t *out_text_length
);

int mylite_json_normalize(
    const char *text,
    size_t text_length,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser parser = {
        .text = text,
        .length = text_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_value value = {0};
    struct json_writer writer = {0};
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    if (text == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_json_internal_parse_document(&parser, &value);
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_emit_value(&writer, &value);
    }
    if (rc == MYLITE_OK) {
        *out_text_length = writer.length;
        *out_text = mylite_json_internal_writer_take(&writer);
        if (*out_text == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    *out_result = parser.result;
    mylite_json_internal_value_deinit(&value);
    mylite_json_internal_writer_deinit(&writer);
    return rc;
}

int mylite_json_validate(const char *text, size_t text_length, bool *out_is_valid) {
    struct json_parser parser = {
        .text = text,
        .length = text_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };

    if (out_is_valid == NULL) {
        return MYLITE_MISUSE;
    }
    *out_is_valid = false;
    if (text == NULL) {
        return MYLITE_ERROR;
    }

    *out_is_valid = mylite_json_internal_validate_document(&parser);
    return MYLITE_OK;
}

int mylite_json_type(
    const char *text,
    size_t text_length,
    const char **out_type,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser parser = {
        .text = text,
        .length = text_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_value value = {0};
    int rc = MYLITE_OK;

    if (out_type == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_type = NULL;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    if (text == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_json_internal_parse_document(&parser, &value);
    *out_result = parser.result;
    if (rc == MYLITE_OK) {
        *out_type = mylite_json_internal_value_type_name(&value);
    }

    mylite_json_internal_value_deinit(&value);
    return rc;
}

int mylite_json_length(
    const char *text,
    size_t text_length,
    const char *path,
    size_t path_length,
    bool has_path,
    int64_t *out_length,
    bool *out_is_null,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser document_parser = {
        .text = text,
        .length = text_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_parser path_parser = {
        .text = path,
        .length = path_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_value document = {0};
    const struct json_value *measured_value = NULL;
    bool matched = false;
    int rc = MYLITE_OK;

    if (out_length == NULL || out_is_null == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_length = 0;
    *out_is_null = false;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    if (text == NULL || (has_path && path == NULL)) {
        return MYLITE_ERROR;
    }

    rc = mylite_json_internal_parse_document(&document_parser, &document);
    *out_result = document_parser.result;
    if (rc == MYLITE_OK && has_path) {
        rc = mylite_json_internal_extract_path_value(
            &path_parser,
            &document,
            &measured_value,
            &matched
        );
        *out_result = path_parser.result;
        if (rc == MYLITE_OK && !matched) {
            *out_is_null = true;
        }
    } else if (rc == MYLITE_OK) {
        measured_value = &document;
        matched = true;
    }
    if (rc == MYLITE_OK && matched) {
        rc = mylite_json_internal_value_shallow_length(measured_value, out_length);
    }

    mylite_json_internal_value_deinit(&document);
    return rc;
}

int mylite_json_keys(
    const char *text,
    size_t text_length,
    const char *path,
    size_t path_length,
    bool has_path,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser document_parser = {
        .text = text,
        .length = text_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_parser path_parser = {
        .text = path,
        .length = path_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_value document = {0};
    struct json_writer writer = {0};
    const struct json_value *selected_value = NULL;
    bool matched = false;
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_is_null == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    if (text == NULL || (has_path && path == NULL)) {
        return MYLITE_ERROR;
    }

    rc = mylite_json_internal_parse_document(&document_parser, &document);
    *out_result = document_parser.result;
    if (rc == MYLITE_OK && has_path) {
        rc = mylite_json_internal_extract_path_value(
            &path_parser,
            &document,
            &selected_value,
            &matched
        );
        *out_result = path_parser.result;
        if (rc == MYLITE_OK && !matched) {
            *out_is_null = true;
        }
    } else if (rc == MYLITE_OK) {
        selected_value = &document;
        matched = true;
    }
    if (rc == MYLITE_OK && matched && selected_value->kind != JSON_VALUE_OBJECT) {
        *out_is_null = true;
    } else if (rc == MYLITE_OK && matched) {
        rc = mylite_json_internal_emit_object_keys(&writer, &selected_value->payload.object);
        if (rc == MYLITE_OK) {
            *out_text_length = writer.length;
            *out_text = mylite_json_internal_writer_take(&writer);
            if (*out_text == NULL) {
                rc = MYLITE_NOMEM;
            }
        }
    }

    mylite_json_internal_value_deinit(&document);
    mylite_json_internal_writer_deinit(&writer);
    return rc;
}

int mylite_json_extract(
    const char *text,
    size_t text_length,
    const char *path,
    size_t path_length,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser document_parser = {
        .text = text,
        .length = text_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_parser path_parser = {
        .text = path,
        .length = path_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_value document = {0};
    struct json_writer writer = {0};
    const struct json_value *matched_value = NULL;
    bool matched = false;
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_is_null == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    if (text == NULL || path == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_json_internal_parse_document(&document_parser, &document);
    *out_result = document_parser.result;
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_extract_path_value(
            &path_parser,
            &document,
            &matched_value,
            &matched
        );
        *out_result = path_parser.result;
    }
    if (rc == MYLITE_OK && !matched) {
        *out_is_null = true;
    } else if (rc == MYLITE_OK) {
        rc = mylite_json_internal_emit_value(&writer, matched_value);
        if (rc == MYLITE_OK) {
            *out_text_length = writer.length;
            *out_text = mylite_json_internal_writer_take(&writer);
            if (*out_text == NULL) {
                rc = MYLITE_NOMEM;
            }
        }
    }

    mylite_json_internal_value_deinit(&document);
    mylite_json_internal_writer_deinit(&writer);
    return rc;
}

int mylite_json_value(
    const char *text,
    size_t text_length,
    const char *path,
    size_t path_length,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser document_parser = {
        .text = text,
        .length = text_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_parser path_parser = {
        .text = path,
        .length = path_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_value document = {0};
    struct json_writer writer = {0};
    const struct json_value *matched_value = NULL;
    bool matched = false;
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_is_null == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    if (text == NULL || path == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_json_internal_parse_document(&document_parser, &document);
    *out_result = document_parser.result;
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_extract_path_value(
            &path_parser,
            &document,
            &matched_value,
            &matched
        );
        *out_result = path_parser.result;
    }
    if (rc == MYLITE_OK && (!matched || matched_value->kind == JSON_VALUE_NULL)) {
        *out_is_null = true;
    } else if (
        rc == MYLITE_OK &&
        (matched_value->kind == JSON_VALUE_STRING || matched_value->kind == JSON_VALUE_NUMBER)
    ) {
        rc = mylite_json_internal_copy_result_text(
            matched_value->payload.text.text,
            matched_value->payload.text.length,
            out_text,
            out_text_length
        );
    } else if (rc == MYLITE_OK && matched_value->kind == JSON_VALUE_BOOL) {
        const char *literal = "false";

        if ((int)matched_value->payload.boolean != 0) {
            literal = "true";
        }

        rc = mylite_json_internal_copy_result_text(
            literal,
            strlen(literal),
            out_text,
            out_text_length
        );
    } else if (rc == MYLITE_OK) {
        rc = mylite_json_internal_emit_value(&writer, matched_value);
        if (rc == MYLITE_OK) {
            *out_text_length = writer.length;
            *out_text = mylite_json_internal_writer_take(&writer);
            if (*out_text == NULL) {
                rc = MYLITE_NOMEM;
            }
        }
    }

    mylite_json_internal_value_deinit(&document);
    mylite_json_internal_writer_deinit(&writer);
    return rc;
}

int mylite_json_contains(
    const char *target,
    size_t target_length,
    const char *candidate,
    size_t candidate_length,
    const char *path,
    size_t path_length,
    bool has_path,
    int64_t *out_contains,
    bool *out_is_null,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser target_parser = {
        .text = target,
        .length = target_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_parser candidate_parser = {
        .text = candidate,
        .length = candidate_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_parser path_parser = {
        .text = path,
        .length = path_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_value target_value = {0};
    struct json_value candidate_value = {0};
    const struct json_value *matched_target = NULL;
    bool matched = false;
    int rc = MYLITE_OK;

    if (out_contains == NULL || out_is_null == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_contains = 0;
    *out_is_null = false;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    if (target == NULL || candidate == NULL || (has_path && path == NULL)) {
        return MYLITE_ERROR;
    }

    rc = mylite_json_internal_parse_document(&target_parser, &target_value);
    *out_result = target_parser.result;
    if (rc == MYLITE_OK) {
        rc = mylite_json_internal_parse_document(&candidate_parser, &candidate_value);
        *out_result = candidate_parser.result;
    }
    if (rc == MYLITE_OK && has_path) {
        rc = mylite_json_internal_extract_path_value(
            &path_parser,
            &target_value,
            &matched_target,
            &matched
        );
        *out_result = path_parser.result;
        if (rc == MYLITE_OK && !matched) {
            *out_is_null = true;
        }
    } else if (rc == MYLITE_OK) {
        matched_target = &target_value;
        matched = true;
    }
    if (rc == MYLITE_OK && matched) {
        if (mylite_json_internal_value_contains(matched_target, &candidate_value)) {
            *out_contains = 1;
        }
    }

    mylite_json_internal_value_deinit(&candidate_value);
    mylite_json_internal_value_deinit(&target_value);
    return rc;
}

int mylite_json_contains_path(
    const char *text,
    size_t text_length,
    const char *const *paths,
    const size_t *path_lengths,
    size_t path_count,
    bool require_all,
    int64_t *out_contains,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser document_parser = {
        .text = text,
        .length = text_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_value document = {0};
    bool any_match = false;
    int rc = MYLITE_OK;

    if (out_contains == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_contains = 0;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    if (text == NULL || (path_count != 0U && (paths == NULL || path_lengths == NULL))) {
        return MYLITE_ERROR;
    }

    rc = mylite_json_internal_parse_document(&document_parser, &document);
    *out_result = document_parser.result;
    for (size_t path_index = 0U; rc == MYLITE_OK && path_index < path_count; ++path_index) {
        struct json_parser path_parser = {
            .text = paths[path_index],
            .length = path_lengths[path_index],
            .position = 0U,
            .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
        };
        const struct json_value *matched_value = NULL;
        bool matched = false;

        rc = mylite_json_internal_extract_path_value(
            &path_parser,
            &document,
            &matched_value,
            &matched
        );
        (void)matched_value;
        *out_result = path_parser.result;
        if (rc != MYLITE_OK) {
            break;
        }
        if (matched) {
            any_match = true;
        } else if (require_all) {
            *out_contains = 0;
            mylite_json_internal_value_deinit(&document);
            return MYLITE_OK;
        }
        if (matched && !require_all) {
            *out_contains = 1;
            mylite_json_internal_value_deinit(&document);
            return MYLITE_OK;
        }
    }
    if (rc == MYLITE_OK && (require_all || any_match)) {
        *out_contains = 1;
    }

    mylite_json_internal_value_deinit(&document);
    return rc;
}

int mylite_json_set(
    const char *text,
    size_t text_length,
    const char *const *paths,
    const size_t *path_lengths,
    const struct mylite_json_sql_value *values,
    size_t pair_count,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
) {
    return mutate_json_document(
        text,
        text_length,
        paths,
        path_lengths,
        JSON_MUTATION_SET,
        values,
        pair_count,
        out_text,
        out_text_length,
        out_result
    );
}

int mylite_json_replace(
    const char *text,
    size_t text_length,
    const char *const *paths,
    const size_t *path_lengths,
    const struct mylite_json_sql_value *values,
    size_t pair_count,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
) {
    return mutate_json_document(
        text,
        text_length,
        paths,
        path_lengths,
        JSON_MUTATION_REPLACE,
        values,
        pair_count,
        out_text,
        out_text_length,
        out_result
    );
}

int mylite_json_insert(
    const char *text,
    size_t text_length,
    const char *const *paths,
    const size_t *path_lengths,
    const struct mylite_json_sql_value *values,
    size_t pair_count,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
) {
    return mutate_json_document(
        text,
        text_length,
        paths,
        path_lengths,
        JSON_MUTATION_INSERT,
        values,
        pair_count,
        out_text,
        out_text_length,
        out_result
    );
}

int mylite_json_remove(
    const char *text,
    size_t text_length,
    const char *const *paths,
    const size_t *path_lengths,
    size_t path_count,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
) {
    return mutate_json_document(
        text,
        text_length,
        paths,
        path_lengths,
        JSON_MUTATION_REMOVE,
        NULL,
        path_count,
        out_text,
        out_text_length,
        out_result
    );
}

int mylite_json_mutation_validate_before_null(
    const char *text,
    size_t text_length,
    const char *const *paths,
    const size_t *path_lengths,
    size_t path_count,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser document_parser = {
        .text = text,
        .length = text_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_value document = {0};
    int rc = MYLITE_OK;

    if (out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    if (text == NULL || (path_count != 0U && (paths == NULL || path_lengths == NULL))) {
        return MYLITE_ERROR;
    }

    rc = mylite_json_internal_parse_document(&document_parser, &document);
    *out_result = document_parser.result;
    for (size_t path_index = 0U; rc == MYLITE_OK && path_index < path_count; ++path_index) {
        struct json_parser path_parser = {
            .text = paths[path_index],
            .length = path_lengths[path_index],
            .position = 0U,
            .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
        };
        struct json_set_path path = {0};

        rc = mylite_json_internal_parse_set_path(&path_parser, &path);
        *out_result = path_parser.result;
        if (rc != MYLITE_OK && out_result->status == MYLITE_JSON_NORMALIZE_INVALID) {
            out_result->status = MYLITE_JSON_NORMALIZE_INVALID_PATH;
        }
        mylite_json_internal_set_path_deinit(&path);
    }

    mylite_json_internal_value_deinit(&document);
    return rc;
}

int mylite_json_remove_validate_before_null(
    const char *text,
    size_t text_length,
    const char *const *paths,
    const size_t *path_lengths,
    size_t path_count,
    struct mylite_json_normalize_result *out_result
) {
    return mylite_json_mutation_validate_before_null(
        text,
        text_length,
        paths,
        path_lengths,
        path_count,
        out_result
    );
}

static int mutate_json_document(
    const char *text,
    size_t text_length,
    const char *const *paths,
    const size_t *path_lengths,
    enum json_mutation_mode mode,
    const struct mylite_json_sql_value *values,
    size_t pair_count,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser document_parser = {
        .text = text,
        .length = text_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    struct json_value document = {0};
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    if (text == NULL || (pair_count != 0U && (paths == NULL || path_lengths == NULL)) ||
        (mode != JSON_MUTATION_REMOVE && pair_count != 0U && values == NULL)) {
        return MYLITE_ERROR;
    }

    rc = mylite_json_internal_parse_document(&document_parser, &document);
    *out_result = document_parser.result;
    for (size_t pair_index = 0U; rc == MYLITE_OK && pair_index < pair_count; ++pair_index) {
        struct json_parser path_parser = {
            .text = paths[pair_index],
            .length = path_lengths[pair_index],
            .position = 0U,
            .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
        };
        struct json_set_path path = {0};
        struct json_value value = {0};

        rc = mylite_json_internal_parse_set_path(&path_parser, &path);
        *out_result = path_parser.result;
        if (rc != MYLITE_OK && out_result->status == MYLITE_JSON_NORMALIZE_INVALID) {
            out_result->status = MYLITE_JSON_NORMALIZE_INVALID_PATH;
        }
        if (rc == MYLITE_OK && mode == JSON_MUTATION_REMOVE && path.count == 0U) {
            out_result->status = MYLITE_JSON_NORMALIZE_PATH_NOT_ALLOWED;
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK && mode != JSON_MUTATION_REMOVE) {
            rc = json_value_from_sql_value(&values[pair_index], &value, out_result);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_json_internal_apply_mutation_path(&document, &path, &value, mode);
        }
        mylite_json_internal_value_deinit(&value);
        mylite_json_internal_set_path_deinit(&path);
    }
    if (rc == MYLITE_OK) {
        rc = emit_constructed_json(&document, out_text, out_text_length);
    }

    mylite_json_internal_value_deinit(&document);
    return rc;
}

int mylite_json_path_validate(
    const char *path,
    size_t path_length,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser path_parser = {
        .text = path,
        .length = path_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    const struct json_value *matched_value = NULL;
    bool matched = false;

    if (out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    if (path == NULL) {
        return MYLITE_ERROR;
    }

    int rc = mylite_json_internal_extract_path_value(&path_parser, NULL, &matched_value, &matched);

    (void)matched_value;
    (void)matched;
    *out_result = path_parser.result;
    return rc;
}

int mylite_json_unquote(
    const char *text,
    size_t text_length,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
) {
    struct json_parser parser = {
        .text = text,
        .length = text_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    char *decoded = NULL;
    size_t decoded_length = 0U;
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = 0U,
    };
    if (text == NULL) {
        return MYLITE_ERROR;
    }

    if (text_length < 2U || text[0] != '"' || text[text_length - 1U] != '"') {
        *out_result = (struct mylite_json_normalize_result){
            .status = MYLITE_JSON_NORMALIZE_OK,
            .position = 0U,
        };
        return mylite_json_internal_copy_result_text(text, text_length, out_text, out_text_length);
    }

    rc = mylite_json_internal_parse_string(&parser, &decoded, &decoded_length);
    if (rc == MYLITE_OK && !mylite_json_internal_parser_at_end(&parser)) {
        rc = mylite_json_internal_parser_invalid(&parser, parser.position);
    }
    *out_result = parser.result;
    if (rc == MYLITE_OK) {
        *out_text = decoded;
        *out_text_length = decoded_length;
        decoded = NULL;
    }

    free(decoded);
    return rc;
}

int mylite_json_quote_string(
    const char *text,
    size_t text_length,
    char **out_text,
    size_t *out_text_length
) {
    struct json_writer writer = {0};
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    if (text == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_json_internal_emit_string(&writer, text, text_length);
    if (rc == MYLITE_OK) {
        size_t result_length = writer.length;

        *out_text = mylite_json_internal_writer_take(&writer);
        if (*out_text == NULL) {
            rc = MYLITE_NOMEM;
        } else {
            *out_text_length = result_length;
        }
    }
    mylite_json_internal_writer_deinit(&writer);
    return rc;
}

const char *mylite_json_invalid_text_error_message(
    const struct mylite_json_normalize_result *result
) {
    if (result != NULL && result->error_detail == MYLITE_JSON_ERROR_MISSING_OBJECT_MEMBER_NAME) {
        return "Missing a name for object member.";
    }
    return "Invalid value.";
}

int mylite_json_array_from_sql_values(
    const struct mylite_json_sql_value *values,
    size_t value_count,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
) {
    struct json_value array = {.kind = JSON_VALUE_ARRAY};
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_OK,
        .position = 0U,
    };
    if (value_count != 0U && values == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t value_index = 0U; rc == MYLITE_OK && value_index < value_count; ++value_index) {
        struct json_value value = {0};
        struct json_value *stored_value = NULL;

        rc = json_value_from_sql_value(&values[value_index], &value, out_result);
        if (rc == MYLITE_OK) {
            rc = mylite_json_internal_array_append_value(
                &array.payload.array,
                &value,
                &stored_value
            );
            (void)stored_value;
        }
        if (rc != MYLITE_OK) {
            mylite_json_internal_value_deinit(&value);
        }
    }
    if (rc == MYLITE_OK) {
        rc = emit_constructed_json(&array, out_text, out_text_length);
    }

    mylite_json_internal_value_deinit(&array);
    return rc;
}

int mylite_json_object_from_sql_values(
    const struct mylite_json_sql_value *keys,
    const struct mylite_json_sql_value *values,
    size_t pair_count,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
) {
    struct json_value object = {.kind = JSON_VALUE_OBJECT};
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_result = (struct mylite_json_normalize_result){
        .status = MYLITE_JSON_NORMALIZE_OK,
        .position = 0U,
    };
    if (pair_count != 0U && (keys == NULL || values == NULL)) {
        return MYLITE_MISUSE;
    }

    for (size_t pair_index = 0U; rc == MYLITE_OK && pair_index < pair_count; ++pair_index) {
        char *key = NULL;
        size_t key_length = 0U;
        struct json_value value = {0};
        struct json_value *stored_value = NULL;

        rc = json_object_key_from_sql_value(&keys[pair_index], &key, &key_length);
        if (rc == MYLITE_OK) {
            rc = json_value_from_sql_value(&values[pair_index], &value, out_result);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_json_internal_object_append_member(
                &object.payload.object,
                key,
                key_length,
                &value,
                &stored_value
            );
            if (rc == MYLITE_OK) {
                key = NULL;
            }
            (void)stored_value;
        }
        free(key);
        if (rc != MYLITE_OK) {
            mylite_json_internal_value_deinit(&value);
        }
    }
    if (rc == MYLITE_OK) {
        mylite_json_internal_sort_object_members_by_mysql_display_order(&object.payload.object);
        rc = emit_constructed_json(&object, out_text, out_text_length);
    }

    mylite_json_internal_value_deinit(&object);
    return rc;
}

static int json_value_from_sql_value(
    const struct mylite_json_sql_value *sql_value,
    struct json_value *out_value,
    struct mylite_json_normalize_result *out_result
) {
    if (sql_value == NULL || out_value == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct json_value){0};

    switch (sql_value->kind) {
    case MYLITE_JSON_SQL_VALUE_NULL:
        out_value->kind = JSON_VALUE_NULL;
        return MYLITE_OK;
    case MYLITE_JSON_SQL_VALUE_INTEGER:
        out_value->kind = JSON_VALUE_NUMBER;
        return copy_integer_text(
            sql_value->integer,
            &out_value->payload.text.text,
            &out_value->payload.text.length
        );
    case MYLITE_JSON_SQL_VALUE_BOOLEAN:
        out_value->kind = JSON_VALUE_BOOL;
        out_value->payload.boolean = sql_value->boolean;
        return MYLITE_OK;
    case MYLITE_JSON_SQL_VALUE_STRING:
        out_value->kind = JSON_VALUE_STRING;
        return mylite_json_internal_copy_result_text(
            sql_value->text,
            sql_value->text_length,
            &out_value->payload.text.text,
            &out_value->payload.text.length
        );
    case MYLITE_JSON_SQL_VALUE_JSON: {
        struct json_parser parser = {
            .text = sql_value->text,
            .length = sql_value->text_length,
            .position = 0U,
            .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
        };
        int rc = MYLITE_OK;

        if (sql_value->text == NULL) {
            return MYLITE_ERROR;
        }
        rc = mylite_json_internal_parse_document(&parser, out_value);
        *out_result = parser.result;
        return rc;
    }
    }

    return MYLITE_ERROR;
}

static int json_object_key_from_sql_value(
    const struct mylite_json_sql_value *sql_value,
    char **out_key,
    size_t *out_key_length
) {
    if (sql_value == NULL || out_key == NULL || out_key_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_key = NULL;
    *out_key_length = 0U;

    switch (sql_value->kind) {
    case MYLITE_JSON_SQL_VALUE_STRING:
        return mylite_json_internal_copy_result_text(
            sql_value->text,
            sql_value->text_length,
            out_key,
            out_key_length
        );
    case MYLITE_JSON_SQL_VALUE_INTEGER:
        return copy_integer_text(sql_value->integer, out_key, out_key_length);
    case MYLITE_JSON_SQL_VALUE_BOOLEAN:
        if (sql_value->boolean) {
            return mylite_json_internal_copy_result_text("1", 1U, out_key, out_key_length);
        }
        return mylite_json_internal_copy_result_text("0", 1U, out_key, out_key_length);
    case MYLITE_JSON_SQL_VALUE_NULL:
    case MYLITE_JSON_SQL_VALUE_JSON:
        break;
    }

    return MYLITE_ERROR;
}

static int copy_integer_text(int64_t value, char **out_text, size_t *out_text_length) {
    char buffer[json_sql_integer_buffer_length];
    int written = snprintf(buffer, sizeof(buffer), "%" PRId64, value);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return MYLITE_ERROR;
    }
    return mylite_json_internal_copy_result_text(
        buffer,
        (size_t)written,
        out_text,
        out_text_length
    );
}

static int emit_constructed_json(
    struct json_value *value,
    char **out_text,
    size_t *out_text_length
) {
    struct json_writer writer = {0};
    int rc = mylite_json_internal_emit_value(&writer, value);

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
