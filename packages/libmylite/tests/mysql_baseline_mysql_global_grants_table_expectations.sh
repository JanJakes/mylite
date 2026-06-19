#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_mysql_global_grants_table_expectations: $1" >&2
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
    output=$(run_mysql "SHOW TABLE STATUS FROM mysql LIKE 'global_grants';")
    field_count=$(printf '%s\n' "$output" | awk -F '\t' '{print NF}')
    prefix=$(printf '%s\n' "$output" | cut -f 1-4)
    storage_metrics=$(printf '%s\n' "$output" | cut -f 5-10)
    auto_increment=$(printf '%s\n' "$output" | cut -f 11)
    create_time=$(printf '%s\n' "$output" | cut -f 12)
    stable_tail=$(printf '%s\n' "$output" | cut -f 13-18)

    if [ "$field_count" != "18" ]; then
        fail "SHOW TABLE STATUS global_grants: expected 18 fields, got [$field_count]"
    fi
    expected_prefix=$(printf '%b' 'global_grants\tInnoDB\t10\tDynamic')
    if [ "$prefix" != "$expected_prefix" ]; then
        fail "SHOW TABLE STATUS global_grants: expected prefix [$expected_prefix], got [$prefix]"
    fi
    if ! printf '%s\n' "$storage_metrics" | awk -F '\t' 'NF == 6 {
        for (i = 1; i <= NF; i++) {
            if ($i !~ /^[0-9]+$/) {
                exit 1;
            }
        }
        exit 0;
    } { exit 1; }'; then
        fail "SHOW TABLE STATUS global_grants: expected numeric storage metrics, got [$storage_metrics]"
    fi
    if [ "$auto_increment" != "NULL" ]; then
        fail "SHOW TABLE STATUS global_grants: expected NULL Auto_increment, got [$auto_increment]"
    fi
    case "$create_time" in
        ????-??-??\ ??:??:??) ;;
        *) fail "SHOW TABLE STATUS global_grants: expected Create_time datetime, got [$create_time]" ;;
    esac
    expected_tail=$(
        printf '%b' \
            'NULL\tNULL\tutf8mb3_bin\tNULL\trow_format=DYNAMIC stats_persistent=0\tExtended global grants'
    )
    if [ "$stable_tail" != "$expected_tail" ]; then
        fail "SHOW TABLE STATUS global_grants: expected tail [$expected_tail], got [$stable_tail]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output \
    "mysql.global_grants target row count" \
    "96" \
    "SELECT COUNT(*) FROM mysql.global_grants;"

columns_expected=$(
    printf '%b' \
        'USER\tchar(32)\tNO\tPRI\t\t\n' \
        'HOST\tchar(255)\tNO\tPRI\t\t\n' \
        'PRIV\tchar(32)\tNO\tPRI\t\t\n' \
        'WITH_GRANT_OPTION\tenum('\''N'\'','\''Y'\'')\tNO\t\tN\t'
)
expect_output \
    "mysql.global_grants SHOW COLUMNS rows" \
    "$columns_expected" \
    "SHOW COLUMNS FROM mysql.global_grants;"

full_columns_expected=$(
    printf '%b' \
        'USER\tchar(32)\tutf8mb3_bin\tNO\tPRI\t\t\tselect,insert,update,references\t\n' \
        'HOST\tchar(255)\tascii_general_ci\tNO\tPRI\t\t\tselect,insert,update,references\t\n' \
        'PRIV\tchar(32)\tutf8mb3_general_ci\tNO\tPRI\t\t\tselect,insert,update,references\t\n' \
        'WITH_GRANT_OPTION\tenum('\''N'\'','\''Y'\'')\tutf8mb3_general_ci\tNO\t\tN\t\tselect,insert,update,references\t'
)
expect_output \
    "mysql.global_grants SHOW FULL COLUMNS rows" \
    "$full_columns_expected" \
    "SHOW FULL COLUMNS FROM mysql.global_grants;"

show_index_expected=$(
    printf '%b' \
        'global_grants\t0\tPRIMARY\t1\tUSER\tA\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'global_grants\t0\tPRIMARY\t2\tHOST\tA\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'global_grants\t0\tPRIMARY\t3\tPRIV\tA\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL'
)
show_index_actual=$(run_mysql "SHOW INDEX FROM mysql.global_grants;" | cut -f 1-6,8-15)
if [ "$show_index_actual" != "$show_index_expected" ]; then
    fail "mysql.global_grants SHOW INDEX rows: expected [$show_index_expected], got [$show_index_actual]"
fi

information_schema_columns_expected=$(
    printf '%b' \
        'USER\t1\t\tNO\tchar\t32\t96\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_bin\tchar(32)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'HOST\t2\t\tNO\tchar\t255\t255\tNULL\tNULL\tNULL\tascii\tascii_general_ci\tchar(255)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'PRIV\t3\t\tNO\tchar\t32\t96\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tchar(32)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'WITH_GRANT_OPTION\t4\tN\tNO\tenum\t1\t3\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tenum('\''N'\'','\''Y'\'')\t\t\tselect,insert,update,references\t\t'
)
expect_output \
    "mysql.global_grants INFORMATION_SCHEMA.COLUMNS rows" \
    "$information_schema_columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
            CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
            NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME,
            COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT,
            GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'global_grants'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "mysql.global_grants INFORMATION_SCHEMA.STATISTICS rows" \
    "$(printf '%b' 'PRIMARY\t1\tUSER\tA\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\nPRIMARY\t2\tHOST\tA\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\nPRIMARY\t3\tPRIV\tA\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL')" \
    "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, SUB_PART,
            PACKED, NULLABLE, INDEX_TYPE, COMMENT, INDEX_COMMENT, IS_VISIBLE,
            EXPRESSION
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'global_grants'
      ORDER BY SEQ_IN_INDEX;"

expect_output \
    "mysql.global_grants INFORMATION_SCHEMA.TABLE_CONSTRAINTS row" \
    "$(printf '%b' 'PRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'global_grants';"

expect_output \
    "mysql.global_grants INFORMATION_SCHEMA.KEY_COLUMN_USAGE rows" \
    "$(printf '%b' 'PRIMARY\tUSER\t1\tNULL\tNULL\tNULL\tNULL\nPRIMARY\tHOST\t2\tNULL\tNULL\tNULL\tNULL\nPRIMARY\tPRIV\t3\tNULL\tNULL\tNULL\tNULL')" \
    "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION,
            POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA,
            REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'global_grants'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "mysql.global_grants INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS row" \
    "$(printf '%b' 'PRIMARY\tglobal_grants\tNULL\tNULL')" \
    "SELECT CONSTRAINT_NAME, TABLE_NAME, ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'mysql' AND TABLE_NAME = 'global_grants';"

expect_output \
    "mysql.global_grants INFORMATION_SCHEMA.TABLES row" \
    "$(printf '%b' 'global_grants\tBASE TABLE\tInnoDB\t10\tDynamic\tNULL\t1\t1\t1\tutf8mb3_bin\t1\trow_format=DYNAMIC stats_persistent=0\tExtended global grants')" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL,
            CREATE_OPTIONS, TABLE_COMMENT
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'global_grants';"

expect_show_table_status_row

printf '%s\n' "mysql_baseline_mysql_global_grants_table_expectations: ok"
