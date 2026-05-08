#include "mylite_expression_descriptor_cast.h"

#include "mylite_charset.h"
#include "mylite_connection.h"
#include "mylite_expression.h"
#include "mylite_expression_descriptor.h"
#include "mylite_expression_validation.h"
#include "mylite_metadata_constants.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <stdint.h>
#include <stdlib.h>

static struct mylite_field_descriptor cast_signed_descriptor(
    const struct mylite_field_descriptor *source
);

static struct mylite_field_descriptor cast_unsigned_descriptor(
    const struct mylite_field_descriptor *source
);

static struct mylite_field_descriptor cast_decimal_descriptor(
    const struct mylite_sql_ast_node *target,
    const struct mylite_field_descriptor *source
);

static struct mylite_field_descriptor cast_float_descriptor(
    mylite_db *database,
    const struct mylite_sql_ast_node *target,
    const struct mylite_field_descriptor *source
);

static struct mylite_field_descriptor cast_character_descriptor(
    mylite_db *database,
    const struct mylite_sql_ast_node *target,
    const struct mylite_expression_value *value,
    const struct mylite_field_descriptor *source
);

static struct mylite_field_descriptor cast_date_descriptor(void);

static struct mylite_field_descriptor cast_time_descriptor(
    const struct mylite_sql_ast_node *target
);

static struct mylite_field_descriptor cast_datetime_descriptor(
    const struct mylite_sql_ast_node *target
);

static unsigned int cast_decimal_precision(const struct mylite_sql_ast_node *target);

static unsigned int cast_decimal_scale(const struct mylite_sql_ast_node *target);

static bool cast_float_target_is_double(const struct mylite_sql_ast_node *target);

static bool cast_float_target_is_real_as_float(
    mylite_db *database,
    const struct mylite_sql_ast_node *target
);

static unsigned int cast_temporal_fsp(const struct mylite_sql_ast_node *target);

static uint64_t cast_character_length(
    mylite_db *database,
    const struct mylite_sql_ast_node *target,
    const struct mylite_expression_value *value,
    const struct mylite_field_descriptor *source
);

static bool cast_target_uses_binary_charset(const struct mylite_sql_ast_node *target);

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_expression_descriptor_infer_cast_expression(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_cast_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *source = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *target = mylite_ast_child_at(expression, 1U);
    struct mylite_field_descriptor source_descriptor = mylite_expression_descriptor_defaults();
    int status = MYLITE_OK;

    if (callbacks == NULL || callbacks->infer_expression_descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    status =
        callbacks->infer_expression_descriptor(database, plan, source, NULL, &source_descriptor);
    if (status != MYLITE_OK) {
        return status;
    }
    if (target == NULL || target->kind != MYLITE_SQL_AST_COLUMN_TYPE) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_OK;
    }
    status = mylite_expression_validate_cast_target_charset(database, expression);
    if (status != MYLITE_OK) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status;
    }

    switch (target->column_type) {
    case MYLITE_SQL_AST_COLUMN_TYPE_BIGINT:
        if (target->column_type_unsigned) {
            *out_descriptor = cast_unsigned_descriptor(&source_descriptor);
        } else {
            *out_descriptor = cast_signed_descriptor(&source_descriptor);
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL:
        *out_descriptor = cast_decimal_descriptor(target, &source_descriptor);
        return MYLITE_OK;
    case MYLITE_SQL_AST_COLUMN_TYPE_FLOAT:
    case MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE:
        *out_descriptor = cast_float_descriptor(database, target, &source_descriptor);
        return MYLITE_OK;
    case MYLITE_SQL_AST_COLUMN_TYPE_CHAR:
    case MYLITE_SQL_AST_COLUMN_TYPE_BINARY:
        *out_descriptor = cast_character_descriptor(database, target, value, &source_descriptor);
        return MYLITE_OK;
    case MYLITE_SQL_AST_COLUMN_TYPE_DATE:
        *out_descriptor = cast_date_descriptor();
        return MYLITE_OK;
    case MYLITE_SQL_AST_COLUMN_TYPE_TIME:
        *out_descriptor = cast_time_descriptor(target);
        return MYLITE_OK;
    case MYLITE_SQL_AST_COLUMN_TYPE_DATETIME:
        *out_descriptor = cast_datetime_descriptor(target);
        return MYLITE_OK;
    case MYLITE_SQL_AST_COLUMN_TYPE_NONE:
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYINT:
    case MYLITE_SQL_AST_COLUMN_TYPE_SMALLINT:
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMINT:
    case MYLITE_SQL_AST_COLUMN_TYPE_INT:
    case MYLITE_SQL_AST_COLUMN_TYPE_SERIAL:
    case MYLITE_SQL_AST_COLUMN_TYPE_BIT:
    case MYLITE_SQL_AST_COLUMN_TYPE_BOOL:
    case MYLITE_SQL_AST_COLUMN_TYPE_BOOLEAN:
    case MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR:
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYTEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_TEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMTEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGTEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_VARBINARY:
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYBLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_BLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMBLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGBLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_TIMESTAMP:
    case MYLITE_SQL_AST_COLUMN_TYPE_YEAR:
        break;
    }

    *out_descriptor = mylite_expression_descriptor_defaults();
    return MYLITE_OK;
}

static struct mylite_field_descriptor cast_signed_descriptor(
    const struct mylite_field_descriptor *source
) {
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_LONGLONG,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = mylite_mysql_signed_longlong_display_length,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = mylite_expression_descriptor_is_nullable(source),
    };

    mylite_field_descriptor_set_nullable(&descriptor, descriptor.nullable);
    return descriptor;
}

static struct mylite_field_descriptor cast_unsigned_descriptor(
    const struct mylite_field_descriptor *source
) {
    struct mylite_field_descriptor descriptor = cast_signed_descriptor(source);

    descriptor.flags |= MYLITE_FIELD_FLAG_UNSIGNED;
    return descriptor;
}

static struct mylite_field_descriptor cast_decimal_descriptor(
    const struct mylite_sql_ast_node *target,
    const struct mylite_field_descriptor *source
) {
    unsigned int precision = cast_decimal_precision(target);
    unsigned int scale = cast_decimal_scale(target);
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_NEWDECIMAL,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = (uint64_t)precision + (scale == 0U ? 1U : 2U),
        .decimals = scale,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = mylite_expression_descriptor_is_nullable(source),
    };

    mylite_field_descriptor_set_nullable(&descriptor, descriptor.nullable);
    return descriptor;
}

static struct mylite_field_descriptor cast_float_descriptor(
    mylite_db *database,
    const struct mylite_sql_ast_node *target,
    const struct mylite_field_descriptor *source
) {
    struct mylite_field_descriptor descriptor = {
        .type = cast_float_target_is_double(target) &&
                        !cast_float_target_is_real_as_float(database, target)
                    ? MYLITE_FIELD_TYPE_DOUBLE
                    : MYLITE_FIELD_TYPE_FLOAT,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = mylite_mysql_double_display_length + 1U,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = mylite_expression_descriptor_is_nullable(source),
    };

    mylite_field_descriptor_set_nullable(&descriptor, descriptor.nullable);
    return descriptor;
}

static struct mylite_field_descriptor cast_character_descriptor(
    mylite_db *database,
    const struct mylite_sql_ast_node *target,
    const struct mylite_expression_value *value,
    const struct mylite_field_descriptor *source
) {
    bool binary_charset = cast_target_uses_binary_charset(target);
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = 0U,
        .length = cast_character_length(database, target, value, source),
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = binary_charset ? mylite_mysql_binary_charset_id
                                     : mylite_expression_descriptor_connection_charset_id(database),
        .nullable = true,
    };

    if (binary_charset) {
        descriptor.flags |= MYLITE_FIELD_FLAG_BINARY;
    }
    mylite_field_descriptor_set_nullable(&descriptor, true);
    return descriptor;
}

static struct mylite_field_descriptor cast_date_descriptor(void) {
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_DATE,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = mylite_mysql_date_display_length,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };

    mylite_field_descriptor_set_nullable(&descriptor, true);
    return descriptor;
}

static struct mylite_field_descriptor cast_time_descriptor(
    const struct mylite_sql_ast_node *target
) {
    unsigned int decimals = cast_temporal_fsp(target);
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_TIME,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = decimals == 0U ? mylite_mysql_time_display_length
                                 : mylite_mysql_time_fraction_display_base + decimals,
        .decimals = decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };

    mylite_field_descriptor_set_nullable(&descriptor, true);
    return descriptor;
}

static struct mylite_field_descriptor cast_datetime_descriptor(
    const struct mylite_sql_ast_node *target
) {
    unsigned int decimals = cast_temporal_fsp(target);
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_DATETIME,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = decimals == 0U ? mylite_mysql_datetime_display_length
                                 : mylite_mysql_datetime_fraction_display_base + decimals,
        .decimals = decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };

    mylite_field_descriptor_set_nullable(&descriptor, true);
    return descriptor;
}

static unsigned int cast_decimal_precision(const struct mylite_sql_ast_node *target) {
    if (target != NULL && target->has_column_precision) {
        return (unsigned int)target->column_precision;
    }
    return mylite_mysql_cast_default_decimal_precision;
}

static unsigned int cast_decimal_scale(const struct mylite_sql_ast_node *target) {
    if (target != NULL && target->has_column_scale) {
        return (unsigned int)target->column_scale;
    }
    return 0U;
}

static bool cast_float_target_is_double(const struct mylite_sql_ast_node *target) {
    enum { mysql_float_double_precision_cutover = 24U };

    if (target == NULL || target->column_type == MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE) {
        return true;
    }
    return target->column_type == MYLITE_SQL_AST_COLUMN_TYPE_FLOAT &&
           target->has_column_precision &&
           target->column_precision > mysql_float_double_precision_cutover;
}

static bool cast_float_target_is_real_as_float(
    mylite_db *database,
    const struct mylite_sql_ast_node *target
) {
    return mylite_connection_sql_mode_has_real_as_float(database) && target != NULL &&
           target->column_type == MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE &&
           mylite_span_equal_ci(target->span, "REAL");
}

static unsigned int cast_temporal_fsp(const struct mylite_sql_ast_node *target) {
    if (target != NULL && target->has_column_precision &&
        target->column_precision <= mylite_mysql_temporal_max_fsp) {
        return (unsigned int)target->column_precision;
    }
    return 0U;
}

static uint64_t cast_character_length(
    mylite_db *database,
    const struct mylite_sql_ast_node *target,
    const struct mylite_expression_value *value,
    const struct mylite_field_descriptor *source
) {
    uint64_t max_length = mylite_expression_descriptor_connection_character_max_length(database);

    if (target != NULL && target->has_column_length) {
        if (cast_target_uses_binary_charset(target)) {
            max_length = 1U;
        }
        return target->column_length * max_length;
    }
    if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return (uint64_t)(value->text_value == NULL ? 0U : value->text_length) * max_length;
    }
    if (source != NULL && source->length != 0U) {
        return source->length;
    }
    return 0U;
}

static bool cast_target_uses_binary_charset(const struct mylite_sql_ast_node *target) {
    char *name = NULL;
    const struct mylite_charset *charset = NULL;

    if (target == NULL) {
        return false;
    }
    if (target->column_type == MYLITE_SQL_AST_COLUMN_TYPE_BINARY) {
        return true;
    }
    if (!target->has_column_character_set) {
        return false;
    }

    name = mylite_copy_unquoted_span_text(target->column_character_set);
    if (name == NULL) {
        return false;
    }
    charset = mylite_charset_lookup(name);
    free(name);
    return charset != NULL &&
           mylite_ascii_case_equal(charset->name, mylite_mysql_binary_charset_name);
}
