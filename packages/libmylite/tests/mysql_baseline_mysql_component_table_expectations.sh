#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_mysql_component_table_expectations: $1" >&2
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
    output=$(run_mysql "SHOW TABLE STATUS FROM mysql LIKE 'component';")
    field_count=$(printf '%s\n' "$output" | awk -F '\t' '{print NF}')
    prefix=$(printf '%s\n' "$output" | cut -f 1-11)
    create_time=$(printf '%s\n' "$output" | cut -f 12)
    stable_tail=$(printf '%s\n' "$output" | cut -f 13-18)

    if [ "$field_count" != "18" ]; then
        fail "SHOW TABLE STATUS component: expected 18 fields, got [$field_count]"
    fi
    expected_prefix=$(printf '%b' 'component\tInnoDB\t10\tDynamic\t0\t0\t16384\t0\t0\t4194304\t1')
    if [ "$prefix" != "$expected_prefix" ]; then
        fail "SHOW TABLE STATUS component: expected prefix [$expected_prefix], got [$prefix]"
    fi
    case "$create_time" in
        ????-??-??\ ??:??:??) ;;
        *) fail "SHOW TABLE STATUS component: expected Create_time datetime, got [$create_time]" ;;
    esac
    expected_tail=$(printf '%b' 'NULL\tNULL\tutf8mb3_general_ci\tNULL\trow_format=DYNAMIC\tComponents')
    if [ "$stable_tail" != "$expected_tail" ]; then
        fail "SHOW TABLE STATUS component: expected tail [$expected_tail], got [$stable_tail]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output "mysql.component baseline row count" "0" "SELECT COUNT(*) FROM mysql.component;"
expect_output \
    "mysql.component direct read is empty" \
    "" \
    "SELECT component_id, component_group_id, component_urn
       FROM mysql.component
      ORDER BY component_id;"
expect_output \
    "mysql.component read ROW_COUNT" \
    "-1" \
    "SELECT component_id, component_group_id, component_urn
       FROM mysql.component
      ORDER BY component_id;
     SELECT ROW_COUNT();"

columns_expected=$(
    printf '%b' \
        'component_id\tint unsigned\tNO\tPRI\tNULL\tauto_increment\n' \
        'component_group_id\tint unsigned\tNO\t\tNULL\t\n' \
        'component_urn\ttext\tNO\t\tNULL\t'
)
expect_output \
    "mysql.component SHOW COLUMNS rows" \
    "$columns_expected" \
    "SHOW COLUMNS FROM mysql.component;"

full_columns_expected=$(
    printf '%b' \
        'component_id\tint unsigned\tNULL\tNO\tPRI\tNULL\tauto_increment\tselect,insert,update,references\t\n' \
        'component_group_id\tint unsigned\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'component_urn\ttext\tutf8mb3_general_ci\tNO\t\tNULL\t\tselect,insert,update,references\t'
)
expect_output \
    "mysql.component SHOW FULL COLUMNS rows" \
    "$full_columns_expected" \
    "SHOW FULL COLUMNS FROM mysql.component;"

show_index_expected=$(
    printf '%b' \
        'component\t0\tPRIMARY\t1\tcomponent_id\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL'
)
expect_output \
    "mysql.component SHOW INDEX row" \
    "$show_index_expected" \
    "SHOW INDEX FROM mysql.component;"

information_schema_columns_expected=$(
    printf '%b' \
        'component_id\t1\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\tPRI\tauto_increment\tselect,insert,update,references\t\t\n' \
        'component_group_id\t2\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\t\t\tselect,insert,update,references\t\t\n' \
        'component_urn\t3\tNULL\tNO\ttext\t65535\t65535\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\ttext\t\t\tselect,insert,update,references\t\t'
)
expect_output \
    "mysql.component INFORMATION_SCHEMA.COLUMNS rows" \
    "$information_schema_columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
            CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
            NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME,
            COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT,
            GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'component'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "mysql.component INFORMATION_SCHEMA.TABLE_CONSTRAINTS row" \
    "$(printf '%b' 'PRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'component';"

expect_output \
    "mysql.component INFORMATION_SCHEMA.KEY_COLUMN_USAGE row" \
    "$(printf '%b' 'PRIMARY\tcomponent_id\t1\tNULL\tNULL\tNULL\tNULL')" \
    "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION,
            POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA,
            REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'component';"

expect_output \
    "mysql.component INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS row" \
    "$(printf '%b' 'PRIMARY\tcomponent\tNULL\tNULL')" \
    "SELECT CONSTRAINT_NAME, TABLE_NAME, ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'mysql' AND TABLE_NAME = 'component';"

expect_output \
    "mysql.component INFORMATION_SCHEMA.STATISTICS row" \
    "$(printf '%b' 'PRIMARY\t1\tcomponent_id\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL')" \
    "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, CARDINALITY, SUB_PART,
            PACKED, NULLABLE, INDEX_TYPE, COMMENT, INDEX_COMMENT, IS_VISIBLE,
            EXPRESSION
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'component';"

expect_output \
    "mysql.component INFORMATION_SCHEMA.TABLES row" \
    "$(printf '%b' 'component\tBASE TABLE\tInnoDB\t10\tDynamic\t0\t0\t16384\t0\t0\t4194304\t1\t1\t1\t1\tutf8mb3_general_ci\t1\trow_format=DYNAMIC\tComponents')" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL, CREATE_OPTIONS,
            TABLE_COMMENT
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'component';"

expect_show_table_status_row

printf '%s\n' "mysql_baseline_mysql_component_table_expectations: ok"
