#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
PRIVILEGES="select,insert,update,references"

fail() {
    printf '%s\n' "mysql_baseline_mysql_password_history_table_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_show_table_status_row() {
    output=$(run_mysql "SHOW TABLE STATUS FROM mysql LIKE 'password_history';")
    field_count=$(printf '%s\n' "$output" | awk -F '\t' '{print NF}')
    prefix=$(printf '%s\n' "$output" | cut -f 1-11)
    create_time=$(printf '%s\n' "$output" | cut -f 12)
    stable_tail=$(printf '%s\n' "$output" | cut -f 13-18)

    if [ "$field_count" != "18" ]; then
        fail "SHOW TABLE STATUS password_history: expected 18 fields, got [$field_count]"
    fi
    expected_prefix=$(
        printf '%b' 'password_history\tInnoDB\t10\tDynamic\t0\t0\t16384\t0\t0\t4194304\tNULL'
    )
    if [ "$prefix" != "$expected_prefix" ]; then
        fail "SHOW TABLE STATUS password_history: expected prefix [$expected_prefix], got [$prefix]"
    fi
    case "$create_time" in
        ????-??-??\ ??:??:??) ;;
        *) fail "SHOW TABLE STATUS password_history: expected Create_time datetime, got [$create_time]" ;;
    esac
    expected_tail=$(
        printf '%b' 'NULL\tNULL\tutf8mb3_bin\tNULL\trow_format=DYNAMIC stats_persistent=0\tPassword history for user accounts'
    )
    if [ "$stable_tail" != "$expected_tail" ]; then
        fail "SHOW TABLE STATUS password_history: expected tail [$expected_tail], got [$stable_tail]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output \
    "mysql.password_history target row count" \
    "0" \
    "SELECT COUNT(*) FROM mysql.password_history;"

columns_expected=$(
    {
        printf '%b\n' 'Host\tchar(255)\tNO\tPRI\t\t'
        printf '%b\n' 'User\tchar(32)\tNO\tPRI\t\t'
        printf '%b\n' 'Password_timestamp\ttimestamp(6)\tNO\tPRI\tCURRENT_TIMESTAMP(6)\tDEFAULT_GENERATED'
        printf '%b\n' 'Password\ttext\tYES\t\tNULL\t'
    }
)
expect_output \
    "mysql.password_history SHOW COLUMNS rows" \
    "$columns_expected" \
    "SHOW COLUMNS FROM mysql.password_history;"

full_columns_expected=$(
    {
        printf '%b\n' "Host\tchar(255)\tascii_general_ci\tNO\tPRI\t\t\t$PRIVILEGES\t"
        printf '%b\n' "User\tchar(32)\tutf8mb3_bin\tNO\tPRI\t\t\t$PRIVILEGES\t"
        printf '%b\n' "Password_timestamp\ttimestamp(6)\tNULL\tNO\tPRI\tCURRENT_TIMESTAMP(6)\tDEFAULT_GENERATED\t$PRIVILEGES\t"
        printf '%b\n' "Password\ttext\tutf8mb3_bin\tYES\t\tNULL\t\t$PRIVILEGES\t"
    }
)
expect_output \
    "mysql.password_history SHOW FULL COLUMNS rows" \
    "$full_columns_expected" \
    "SHOW FULL COLUMNS FROM mysql.password_history;"

show_index_expected=$(
    printf '%b' \
        'password_history\t0\tPRIMARY\t1\tHost\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'password_history\t0\tPRIMARY\t2\tUser\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'password_history\t0\tPRIMARY\t3\tPassword_timestamp\tD\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL'
)
expect_output \
    "mysql.password_history SHOW INDEX rows" \
    "$show_index_expected" \
    "SHOW INDEX FROM mysql.password_history;"

information_schema_columns_expected=$(
    {
        printf '%b\n' "Host\t1\t\tNO\tchar\t255\t255\tNULL\tNULL\tNULL\tascii\tascii_general_ci\tchar(255)\tPRI\t\t$PRIVILEGES\t\t"
        printf '%b\n' "User\t2\t\tNO\tchar\t32\t96\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_bin\tchar(32)\tPRI\t\t$PRIVILEGES\t\t"
        printf '%b\n' "Password_timestamp\t3\tCURRENT_TIMESTAMP(6)\tNO\ttimestamp\tNULL\tNULL\tNULL\tNULL\t6\tNULL\tNULL\ttimestamp(6)\tPRI\tDEFAULT_GENERATED\t$PRIVILEGES\t\t"
        printf '%b\n' "Password\t4\tNULL\tYES\ttext\t65535\t65535\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_bin\ttext\t\t\t$PRIVILEGES\t\t"
    }
)
expect_output \
    "mysql.password_history INFORMATION_SCHEMA.COLUMNS rows" \
    "$information_schema_columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
            CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
            NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME,
            COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT,
            GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'password_history'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "mysql.password_history INFORMATION_SCHEMA.STATISTICS rows" \
    "$(printf '%b' '0\tPRIMARY\t1\tHost\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n0\tPRIMARY\t2\tUser\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n0\tPRIMARY\t3\tPassword_timestamp\tD\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL')" \
    "SELECT NON_UNIQUE, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, CARDINALITY,
            SUB_PART, PACKED, NULLABLE, INDEX_TYPE, COMMENT, INDEX_COMMENT, IS_VISIBLE,
            EXPRESSION
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'password_history'
      ORDER BY IF(INDEX_NAME = 'PRIMARY', 0, 1), INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "mysql.password_history INFORMATION_SCHEMA.TABLE_CONSTRAINTS row" \
    "$(printf '%b' 'PRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'password_history';"

expect_output \
    "mysql.password_history INFORMATION_SCHEMA.KEY_COLUMN_USAGE rows" \
    "$(printf '%b' 'PRIMARY\tHost\t1\tNULL\tNULL\tNULL\tNULL\nPRIMARY\tUser\t2\tNULL\tNULL\tNULL\tNULL\nPRIMARY\tPassword_timestamp\t3\tNULL\tNULL\tNULL\tNULL')" \
    "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION,
            POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA,
            REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'password_history'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "mysql.password_history INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS row" \
    "$(printf '%b' 'PRIMARY\tpassword_history\tNULL\tNULL')" \
    "SELECT CONSTRAINT_NAME, TABLE_NAME, ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'mysql' AND TABLE_NAME = 'password_history';"

expect_output \
    "mysql.password_history INFORMATION_SCHEMA.TABLES row" \
    "$(printf '%b' 'password_history\tBASE TABLE\tInnoDB\t10\tDynamic\t0\t0\t16384\t0\t0\t4194304\tNULL\t1\t1\t1\tutf8mb3_bin\t1\trow_format=DYNAMIC stats_persistent=0\tPassword history for user accounts')" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL, CREATE_OPTIONS,
            TABLE_COMMENT
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'password_history';"

expect_show_table_status_row

printf '%s\n' "mysql_baseline_mysql_password_history_table_expectations: ok"
