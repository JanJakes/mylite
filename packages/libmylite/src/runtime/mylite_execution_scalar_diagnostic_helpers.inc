
static void set_scalar_arithmetic_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "SELECT scalar arithmetic projection supports only signed 64-bit integer, boolean, "
        "NULL, and nested IF()/IFNULL()/COALESCE()/NULLIF()/ISNULL() operands with +, "
        "binary -, and * arithmetic plus unary +, unary -, %, infix MOD, MOD(), and infix DIV"
    );
}

static void set_scalar_division_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "SELECT scalar division projection supports only top-level / over signed 64-bit scalar "
        "arithmetic operands without nested / division"
    );
}

static void set_scalar_arithmetic_operand_out_of_range_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "SELECT scalar arithmetic projection supports only signed 64-bit integer operands"
    );
}

static void set_scalar_arithmetic_overflow_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_bigint_out_of_range,
        "22003",
        "BIGINT value is out of range in scalar arithmetic expression"
    );
}

static void set_scalar_bitwise_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "SELECT scalar bitwise projection supports only top-level numeric bitwise ~, &, |, ^, "
        "<<, and >> over signed 64-bit integer, boolean, NULL, and nested "
        "IF()/IFNULL()/COALESCE()/NULLIF()/ISNULL() scalar arithmetic operands"
    );
}

static void set_abs_signed_minimum_overflow_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_bigint_out_of_range,
        "22003",
        "BIGINT value is out of range in 'abs(-(9223372036854775808))'"
    );
}

static void set_abs_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "ABS() supports only top-level no-source SELECT, FROM DUAL SELECT, and DO numeric "
        "ABS() over integer, boolean, NULL, signed 64-bit scalar arithmetic, and limited "
        "numeric bitwise operands"
    );
}

static void set_sign_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "SIGN() supports only top-level no-source SELECT, FROM DUAL SELECT, and DO numeric "
        "SIGN() over integer, boolean, NULL, direct decimal integer literals, signed 64-bit "
        "scalar arithmetic, and limited numeric bitwise operands"
    );
}

static void set_rounding_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "CEIL()/CEILING()/FLOOR()/ROUND() support only top-level integer-domain scalar "
        "use; ROUND(value, places) supports only signed 64-bit negative-place rounding"
    );
}

static int set_rounding_signed_overflow_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_bigint_out_of_range,
        "22003",
        "BIGINT value is out of range in 'round()'"
    );
    return MYLITE_ERROR;
}

static void set_sqrt_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "SQRT() supports only top-level no-source SELECT, FROM DUAL SELECT, and DO numeric "
        "SQRT() over integer, boolean, NULL, signed 64-bit scalar arithmetic, and limited "
        "numeric bitwise operands"
    );
}

static void set_angle_conversion_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "DEGREES()/RADIANS() support only top-level no-source SELECT, FROM DUAL SELECT, "
        "and DO numeric conversion over integer, boolean, NULL, signed 64-bit scalar "
        "arithmetic, and limited numeric bitwise operands"
    );
}

static void set_inverse_trig_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "ACOS()/ASIN() support only top-level no-source SELECT, FROM DUAL SELECT, and DO "
        "inverse trigonometry over integer, boolean, NULL, signed 64-bit scalar arithmetic, "
        "and limited numeric bitwise operands"
    );
}

static void set_direct_trig_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "SIN()/COS()/TAN()/COT() support only top-level no-source SELECT, FROM DUAL SELECT, "
        "and DO trigonometry over integer, boolean, NULL, signed 64-bit scalar arithmetic, "
        "and limited numeric bitwise operands"
    );
}

static void set_atan_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "ATAN()/ATAN2() support only top-level no-source SELECT, FROM DUAL SELECT, and DO "
        "arc tangent over integer, boolean, NULL, signed 64-bit scalar arithmetic, and limited "
        "numeric bitwise operands"
    );
}

static void set_exp_log_power_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "EXP()/LN()/LOG()/LOG10()/LOG2()/POW()/POWER() support only top-level no-source "
        "SELECT, FROM DUAL SELECT, and DO use over integer, boolean, NULL, signed 64-bit "
        "scalar arithmetic, and limited numeric bitwise operands"
    );
}

static void set_base_conversion_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "BIN()/OCT() support only top-level no-source SELECT, FROM DUAL SELECT, and DO "
        "numeric conversion over integer, boolean, NULL, signed 64-bit scalar arithmetic, "
        "and limited numeric bitwise operands; CONV() support is limited to the same "
        "top-level scalar contexts, with signed 64-bit scalar arithmetic base arguments"
    );
}

static void set_bit_count_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "BIT_COUNT() supports only top-level no-source SELECT, FROM DUAL SELECT, and DO "
        "numeric BIT_COUNT() over integer, boolean, NULL, signed 64-bit scalar "
        "arithmetic, and limited numeric bitwise operands"
    );
}

static void set_crc32_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "CRC32() supports only top-level no-source SELECT, FROM DUAL SELECT, and DO "
        "CRC32() over string, hex, integer, boolean, and NULL literals"
    );
}

static void set_hex_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "HEX() supports only top-level no-source SELECT, FROM DUAL SELECT, DO, and "
        "single-table row-scalar SELECT over string, hex, integer, boolean, NULL, supported "
        "session scalar, binary cast/convert, and supported descriptor column arguments"
    );
}

static void set_format_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "FORMAT() supports only top-level no-source SELECT, FROM DUAL SELECT, and DO "
        "FORMAT(value, decimals) over exact integer, fixed decimal, boolean, and NULL "
        "literal operands"
    );
}

static void set_truncate_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "TRUNCATE() supports only top-level no-source SELECT, FROM DUAL SELECT, and DO "
        "TRUNCATE(value, decimals) over exact integer, fixed decimal, boolean, and NULL "
        "literal operands"
    );
}

static void set_scalar_logical_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "SELECT scalar logical projection supports only signed 64-bit integer, boolean, "
        "NULL, and nested IF()/IFNULL()/COALESCE()/NULLIF()/ISNULL() operands with signed "
        "64-bit +, binary -, *, %, infix MOD, MOD(), infix DIV, comparison operators "
        "=, <=>, <>, !=, <, <=, >, >=, LIKE, keyword logical NOT, AND, XOR, and OR, and "
        "scalar IS NULL, IS TRUE, IS FALSE, and IS UNKNOWN"
    );
}

static void set_scalar_comparison_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "SELECT scalar comparison projection supports only signed 64-bit integer, boolean, "
        "NULL, and nested IF()/IFNULL()/COALESCE()/NULLIF()/ISNULL() operands with signed "
        "64-bit +, binary -, *, %, infix MOD, MOD(), infix DIV, and comparison operators "
        "=, <=>, <>, !=, <, <=, >, >=, and LIKE"
    );
}
