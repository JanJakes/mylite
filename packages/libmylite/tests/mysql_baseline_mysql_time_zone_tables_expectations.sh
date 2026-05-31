#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_mysql_time_zone_tables_expectations: $1" >&2
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
    table_name=$1
    expected_prefix=$2
    expected_tail=$3

    output=$(run_mysql "SHOW TABLE STATUS FROM mysql LIKE '${table_name}';")
    field_count=$(printf '%s\n' "$output" | awk -F '\t' '{print NF}')
    prefix=$(printf '%s\n' "$output" | cut -f 1-11)
    create_time=$(printf '%s\n' "$output" | cut -f 12)
    stable_tail=$(printf '%s\n' "$output" | cut -f 13-18)

    if [ "$field_count" != "18" ]; then
        fail "SHOW TABLE STATUS ${table_name}: expected 18 fields, got [$field_count]"
    fi
    if [ "$prefix" != "$expected_prefix" ]; then
        fail "SHOW TABLE STATUS ${table_name}: expected prefix [$expected_prefix], got [$prefix]"
    fi
    case "$create_time" in
        ????-??-??\ ??:??:??) ;;
        *) fail "SHOW TABLE STATUS ${table_name}: expected Create_time datetime, got [$create_time]" ;;
    esac
    if [ "$stable_tail" != "$expected_tail" ]; then
        fail "SHOW TABLE STATUS ${table_name}: expected tail [$expected_tail], got [$stable_tail]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output \
    "mysql time zone table row counts" \
    "$(printf '%b' 'time_zone\t1795\n' \
        'time_zone_leap_second\t0\n' \
        'time_zone_name\t1795\n' \
        'time_zone_transition\t118801\n' \
        'time_zone_transition_type\t10200')" \
    "SELECT 'time_zone', COUNT(*) FROM mysql.time_zone
     UNION ALL SELECT 'time_zone_leap_second', COUNT(*) FROM mysql.time_zone_leap_second
     UNION ALL SELECT 'time_zone_name', COUNT(*) FROM mysql.time_zone_name
     UNION ALL SELECT 'time_zone_transition', COUNT(*) FROM mysql.time_zone_transition
     UNION ALL SELECT 'time_zone_transition_type', COUNT(*) FROM mysql.time_zone_transition_type;"

columns_expected=$(
    printf '%b' \
        'time_zone\tTime_zone_id\t1\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\tPRI\tauto_increment\tselect,insert,update,references\t\t\n' \
        'time_zone\tUse_leap_seconds\t2\tN\tNO\tenum\t1\t3\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tenum('\''Y'\'','\''N'\'')\t\t\tselect,insert,update,references\t\t\n' \
        'time_zone_leap_second\tTransition_time\t1\tNULL\tNO\tbigint\tNULL\tNULL\t19\t0\tNULL\tNULL\tNULL\tbigint\tPRI\t\tselect,insert,update,references\t\t\n' \
        'time_zone_leap_second\tCorrection\t2\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint\t\t\tselect,insert,update,references\t\t\n' \
        'time_zone_name\tName\t1\tNULL\tNO\tchar\t64\t192\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tchar(64)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'time_zone_name\tTime_zone_id\t2\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\t\t\tselect,insert,update,references\t\t\n' \
        'time_zone_transition\tTime_zone_id\t1\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\tPRI\t\tselect,insert,update,references\t\t\n' \
        'time_zone_transition\tTransition_time\t2\tNULL\tNO\tbigint\tNULL\tNULL\t19\t0\tNULL\tNULL\tNULL\tbigint\tPRI\t\tselect,insert,update,references\t\t\n' \
        'time_zone_transition\tTransition_type_id\t3\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\t\t\tselect,insert,update,references\t\t\n' \
        'time_zone_transition_type\tTime_zone_id\t1\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\tPRI\t\tselect,insert,update,references\t\t\n' \
        'time_zone_transition_type\tTransition_type_id\t2\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\tPRI\t\tselect,insert,update,references\t\t\n' \
        'time_zone_transition_type\tOffset\t3\t0\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint\t\t\tselect,insert,update,references\t\t\n' \
        'time_zone_transition_type\tIs_DST\t4\t0\tNO\ttinyint\tNULL\tNULL\t3\t0\tNULL\tNULL\tNULL\ttinyint unsigned\t\t\tselect,insert,update,references\t\t\n' \
        'time_zone_transition_type\tAbbreviation\t5\t\tNO\tchar\t8\t24\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tchar(8)\t\t\tselect,insert,update,references\t\t'
)
expect_output \
    "mysql time zone INFORMATION_SCHEMA.COLUMNS rows" \
    "$columns_expected" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE,
            DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH,
            NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION,
            CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, EXTRA,
            PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME LIKE 'time_zone%'
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

statistics_expected=$(
    printf '%b' \
        'time_zone\tPRIMARY\t1\tTime_zone_id\tA\t1457\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'time_zone_leap_second\tPRIMARY\t1\tTransition_time\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'time_zone_name\tPRIMARY\t1\tName\tA\t1712\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'time_zone_transition\tPRIMARY\t1\tTime_zone_id\tA\t1252\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'time_zone_transition\tPRIMARY\t2\tTransition_time\tA\t119074\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'time_zone_transition_type\tPRIMARY\t1\tTime_zone_id\tA\t1954\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'time_zone_transition_type\tPRIMARY\t2\tTransition_type_id\tA\t10529\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL'
)
expect_output \
    "mysql time zone INFORMATION_SCHEMA.STATISTICS rows" \
    "$statistics_expected" \
    "SELECT TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION,
            CARDINALITY, SUB_PART, PACKED, NULLABLE, INDEX_TYPE, COMMENT,
            INDEX_COMMENT, IS_VISIBLE, EXPRESSION
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME LIKE 'time_zone%'
      ORDER BY TABLE_NAME, SEQ_IN_INDEX;"

expect_output \
    "mysql time zone TABLE_CONSTRAINTS rows" \
    "$(printf '%b' 'time_zone\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'time_zone_leap_second\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'time_zone_name\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'time_zone_transition\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'time_zone_transition_type\tPRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME LIKE 'time_zone%'
      ORDER BY TABLE_NAME;"

expect_output \
    "mysql time zone KEY_COLUMN_USAGE rows" \
    "$(printf '%b' 'time_zone\tPRIMARY\tTime_zone_id\t1\tNULL\tNULL\tNULL\tNULL\n' \
        'time_zone_leap_second\tPRIMARY\tTransition_time\t1\tNULL\tNULL\tNULL\tNULL\n' \
        'time_zone_name\tPRIMARY\tName\t1\tNULL\tNULL\tNULL\tNULL\n' \
        'time_zone_transition\tPRIMARY\tTime_zone_id\t1\tNULL\tNULL\tNULL\tNULL\n' \
        'time_zone_transition\tPRIMARY\tTransition_time\t2\tNULL\tNULL\tNULL\tNULL\n' \
        'time_zone_transition_type\tPRIMARY\tTime_zone_id\t1\tNULL\tNULL\tNULL\tNULL\n' \
        'time_zone_transition_type\tPRIMARY\tTransition_type_id\t2\tNULL\tNULL\tNULL\tNULL')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION,
            POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA,
            REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME LIKE 'time_zone%'
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "mysql time zone TABLE_CONSTRAINTS_EXTENSIONS rows" \
    "$(printf '%b' 'time_zone\tPRIMARY\tNULL\tNULL\n' \
        'time_zone_leap_second\tPRIMARY\tNULL\tNULL\n' \
        'time_zone_name\tPRIMARY\tNULL\tNULL\n' \
        'time_zone_transition\tPRIMARY\tNULL\tNULL\n' \
        'time_zone_transition_type\tPRIMARY\tNULL\tNULL')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'mysql' AND TABLE_NAME LIKE 'time_zone%'
      ORDER BY TABLE_NAME;"

expect_output \
    "mysql time zone INFORMATION_SCHEMA.TABLES rows" \
    "$(printf '%b' 'time_zone\tBASE TABLE\tInnoDB\t10\tDynamic\t1457\t56\t81920\t0\t0\t4194304\t1796\t1\t1\t1\tutf8mb3_general_ci\t1\trow_format=DYNAMIC stats_persistent=0\tTime zones\n' \
        'time_zone_leap_second\tBASE TABLE\tInnoDB\t10\tDynamic\t0\t0\t16384\t0\t0\t4194304\tNULL\t1\t1\t1\tutf8mb3_general_ci\t1\trow_format=DYNAMIC stats_persistent=0\tLeap seconds information for time zones\n' \
        'time_zone_name\tBASE TABLE\tInnoDB\t10\tDynamic\t1712\t153\t262144\t0\t0\t4194304\tNULL\t1\t1\t1\tutf8mb3_general_ci\t1\trow_format=DYNAMIC stats_persistent=0\tTime zone names\n' \
        'time_zone_transition\tBASE TABLE\tInnoDB\t10\tDynamic\t119074\t39\t4734976\t0\t0\t4194304\tNULL\t1\t1\t1\tutf8mb3_general_ci\t1\trow_format=DYNAMIC stats_persistent=0\tTime zone transitions\n' \
        'time_zone_transition_type\tBASE TABLE\tInnoDB\t10\tDynamic\t10529\t45\t475136\t0\t0\t4194304\tNULL\t1\t1\t1\tutf8mb3_general_ci\t1\trow_format=DYNAMIC stats_persistent=0\tTime zone transition types')" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH,
            DATA_FREE, AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL,
            CREATE_OPTIONS, TABLE_COMMENT
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME LIKE 'time_zone%'
      ORDER BY TABLE_NAME;"

expect_show_table_status_row \
    "time_zone" \
    "$(printf '%b' 'time_zone\tInnoDB\t10\tDynamic\t1457\t56\t81920\t0\t0\t4194304\t1796')" \
    "$(printf '%b' 'NULL\tNULL\tutf8mb3_general_ci\tNULL\trow_format=DYNAMIC stats_persistent=0\tTime zones')"
expect_show_table_status_row \
    "time_zone_leap_second" \
    "$(printf '%b' 'time_zone_leap_second\tInnoDB\t10\tDynamic\t0\t0\t16384\t0\t0\t4194304\tNULL')" \
    "$(printf '%b' 'NULL\tNULL\tutf8mb3_general_ci\tNULL\trow_format=DYNAMIC stats_persistent=0\tLeap seconds information for time zones')"
expect_show_table_status_row \
    "time_zone_name" \
    "$(printf '%b' 'time_zone_name\tInnoDB\t10\tDynamic\t1712\t153\t262144\t0\t0\t4194304\tNULL')" \
    "$(printf '%b' 'NULL\tNULL\tutf8mb3_general_ci\tNULL\trow_format=DYNAMIC stats_persistent=0\tTime zone names')"
expect_show_table_status_row \
    "time_zone_transition" \
    "$(printf '%b' 'time_zone_transition\tInnoDB\t10\tDynamic\t119074\t39\t4734976\t0\t0\t4194304\tNULL')" \
    "$(printf '%b' 'NULL\tNULL\tutf8mb3_general_ci\tNULL\trow_format=DYNAMIC stats_persistent=0\tTime zone transitions')"
expect_show_table_status_row \
    "time_zone_transition_type" \
    "$(printf '%b' 'time_zone_transition_type\tInnoDB\t10\tDynamic\t10529\t45\t475136\t0\t0\t4194304\tNULL')" \
    "$(printf '%b' 'NULL\tNULL\tutf8mb3_general_ci\tNULL\trow_format=DYNAMIC stats_persistent=0\tTime zone transition types')"

expect_output \
    "mysql loaded named time zones can be selected" \
    "$(printf '%b' 'Europe/Helsinki\nUTC')" \
    "SET time_zone = 'Europe/Helsinki'; SELECT @@SESSION.time_zone;
     SET time_zone = 'UTC'; SELECT @@SESSION.time_zone;"

printf '%s\n' "mysql_baseline_mysql_time_zone_tables_expectations: ok"
