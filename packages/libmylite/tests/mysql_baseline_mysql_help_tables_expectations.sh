#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_mysql_help_tables_expectations: $1" >&2
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
    "mysql help table row counts" \
    "$(printf '%b' 'help_category\t53\n' \
        'help_keyword\t962\n' \
        'help_relation\t1985\n' \
        'help_topic\t695')" \
    "SELECT 'help_category', COUNT(*) FROM mysql.help_category
     UNION ALL SELECT 'help_keyword', COUNT(*) FROM mysql.help_keyword
     UNION ALL SELECT 'help_relation', COUNT(*) FROM mysql.help_relation
     UNION ALL SELECT 'help_topic', COUNT(*) FROM mysql.help_topic;"

columns_expected=$(
    printf '%b' \
        'help_category\thelp_category_id\t1\tNULL\tNO\tsmallint\tNULL\tNULL\t5\t0\tNULL\tNULL\tNULL\tsmallint unsigned\tPRI\t\tselect,insert,update,references\t\t\n' \
        'help_category\tname\t2\tNULL\tNO\tchar\t64\t192\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tchar(64)\tUNI\t\tselect,insert,update,references\t\t\n' \
        'help_category\tparent_category_id\t3\tNULL\tYES\tsmallint\tNULL\tNULL\t5\t0\tNULL\tNULL\tNULL\tsmallint unsigned\t\t\tselect,insert,update,references\t\t\n' \
        'help_category\turl\t4\tNULL\tNO\ttext\t65535\t65535\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\ttext\t\t\tselect,insert,update,references\t\t\n' \
        'help_keyword\thelp_keyword_id\t1\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\tPRI\t\tselect,insert,update,references\t\t\n' \
        'help_keyword\tname\t2\tNULL\tNO\tchar\t64\t192\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tchar(64)\tUNI\t\tselect,insert,update,references\t\t\n' \
        'help_relation\thelp_topic_id\t1\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\tPRI\t\tselect,insert,update,references\t\t\n' \
        'help_relation\thelp_keyword_id\t2\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\tPRI\t\tselect,insert,update,references\t\t\n' \
        'help_topic\thelp_topic_id\t1\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\tPRI\t\tselect,insert,update,references\t\t\n' \
        'help_topic\tname\t2\tNULL\tNO\tchar\t64\t192\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tchar(64)\tUNI\t\tselect,insert,update,references\t\t\n' \
        'help_topic\thelp_category_id\t3\tNULL\tNO\tsmallint\tNULL\tNULL\t5\t0\tNULL\tNULL\tNULL\tsmallint unsigned\t\t\tselect,insert,update,references\t\t\n' \
        'help_topic\tdescription\t4\tNULL\tNO\ttext\t65535\t65535\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\ttext\t\t\tselect,insert,update,references\t\t\n' \
        'help_topic\texample\t5\tNULL\tNO\ttext\t65535\t65535\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\ttext\t\t\tselect,insert,update,references\t\t\n' \
        'help_topic\turl\t6\tNULL\tNO\ttext\t65535\t65535\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\ttext\t\t\tselect,insert,update,references\t\t'
)
expect_output \
    "mysql help INFORMATION_SCHEMA.COLUMNS rows" \
    "$columns_expected" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE,
            DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH,
            NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION,
            CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, EXTRA,
            PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('help_category', 'help_keyword', 'help_relation', 'help_topic')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

statistics_expected=$(
    printf '%b' \
        'help_category\tname\t1\tname\tA\t53\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'help_category\tPRIMARY\t1\thelp_category_id\tA\t53\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'help_keyword\tname\t1\tname\tA\t551\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'help_keyword\tPRIMARY\t1\thelp_keyword_id\tA\t551\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'help_relation\tPRIMARY\t1\thelp_keyword_id\tA\t1393\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'help_relation\tPRIMARY\t2\thelp_topic_id\tA\t2258\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'help_topic\tname\t1\tname\tA\t596\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'help_topic\tPRIMARY\t1\thelp_topic_id\tA\t596\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL'
)
expect_output \
    "mysql help INFORMATION_SCHEMA.STATISTICS rows" \
    "$statistics_expected" \
    "SELECT TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION,
            CARDINALITY, SUB_PART, PACKED, NULLABLE, INDEX_TYPE, COMMENT,
            INDEX_COMMENT, IS_VISIBLE, EXPRESSION
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('help_category', 'help_keyword', 'help_relation', 'help_topic')
      ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "mysql help TABLE_CONSTRAINTS rows" \
    "$(printf '%b' 'help_category\tname\tUNIQUE\tYES\n' \
        'help_category\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'help_keyword\tname\tUNIQUE\tYES\n' \
        'help_keyword\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'help_relation\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'help_topic\tname\tUNIQUE\tYES\n' \
        'help_topic\tPRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('help_category', 'help_keyword', 'help_relation', 'help_topic')
      ORDER BY TABLE_NAME, CONSTRAINT_NAME;"

expect_output \
    "mysql help KEY_COLUMN_USAGE rows" \
    "$(printf '%b' 'help_category\tname\tname\t1\tNULL\tNULL\tNULL\tNULL\n' \
        'help_category\tPRIMARY\thelp_category_id\t1\tNULL\tNULL\tNULL\tNULL\n' \
        'help_keyword\tname\tname\t1\tNULL\tNULL\tNULL\tNULL\n' \
        'help_keyword\tPRIMARY\thelp_keyword_id\t1\tNULL\tNULL\tNULL\tNULL\n' \
        'help_relation\tPRIMARY\thelp_keyword_id\t1\tNULL\tNULL\tNULL\tNULL\n' \
        'help_relation\tPRIMARY\thelp_topic_id\t2\tNULL\tNULL\tNULL\tNULL\n' \
        'help_topic\tname\tname\t1\tNULL\tNULL\tNULL\tNULL\n' \
        'help_topic\tPRIMARY\thelp_topic_id\t1\tNULL\tNULL\tNULL\tNULL')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION,
            POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA,
            REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('help_category', 'help_keyword', 'help_relation', 'help_topic')
      ORDER BY TABLE_NAME, CONSTRAINT_NAME, ORDINAL_POSITION;"

expect_output \
    "mysql help TABLE_CONSTRAINTS_EXTENSIONS rows" \
    "$(printf '%b' 'help_category\tname\tNULL\tNULL\n' \
        'help_category\tPRIMARY\tNULL\tNULL\n' \
        'help_keyword\tname\tNULL\tNULL\n' \
        'help_keyword\tPRIMARY\tNULL\tNULL\n' \
        'help_relation\tPRIMARY\tNULL\tNULL\n' \
        'help_topic\tname\tNULL\tNULL\n' \
        'help_topic\tPRIMARY\tNULL\tNULL')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('help_category', 'help_keyword', 'help_relation', 'help_topic')
      ORDER BY TABLE_NAME, CONSTRAINT_NAME;"

expect_output \
    "mysql help INFORMATION_SCHEMA.TABLES rows" \
    "$(printf '%b' 'help_category\tBASE TABLE\tInnoDB\t10\tDynamic\t53\t309\t16384\t0\t16384\t4194304\tNULL\t1\t1\t1\tutf8mb3_general_ci\t1\trow_format=DYNAMIC stats_persistent=0\thelp categories\n' \
        'help_keyword\tBASE TABLE\tInnoDB\t10\tDynamic\t1142\t114\t131072\t0\t147456\t4194304\tNULL\t1\t1\t1\tutf8mb3_general_ci\t1\trow_format=DYNAMIC stats_persistent=0\thelp keywords\n' \
        'help_relation\tBASE TABLE\tInnoDB\t10\tDynamic\t1608\t50\t81920\t0\t0\t4194304\tNULL\t1\t1\t1\tutf8mb3_general_ci\t1\trow_format=DYNAMIC stats_persistent=0\tkeyword-topic relation\n' \
        'help_topic\tBASE TABLE\tInnoDB\t10\tDynamic\t902\t1761\t1589248\t0\t98304\t4194304\tNULL\t1\t1\t1\tutf8mb3_general_ci\t1\trow_format=DYNAMIC stats_persistent=0\thelp topics')" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH,
            DATA_FREE, AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL,
            CREATE_OPTIONS, TABLE_COMMENT
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('help_category', 'help_keyword', 'help_relation', 'help_topic')
      ORDER BY TABLE_NAME;"

expect_show_table_status_row \
    "help_category" \
    "$(printf '%b' 'help_category\tInnoDB\t10\tDynamic\t53\t309\t16384\t0\t16384\t4194304\tNULL')" \
    "$(printf '%b' 'NULL\tNULL\tutf8mb3_general_ci\tNULL\trow_format=DYNAMIC stats_persistent=0\thelp categories')"
expect_show_table_status_row \
    "help_keyword" \
    "$(printf '%b' 'help_keyword\tInnoDB\t10\tDynamic\t1142\t114\t131072\t0\t147456\t4194304\tNULL')" \
    "$(printf '%b' 'NULL\tNULL\tutf8mb3_general_ci\tNULL\trow_format=DYNAMIC stats_persistent=0\thelp keywords')"
expect_show_table_status_row \
    "help_relation" \
    "$(printf '%b' 'help_relation\tInnoDB\t10\tDynamic\t1608\t50\t81920\t0\t0\t4194304\tNULL')" \
    "$(printf '%b' 'NULL\tNULL\tutf8mb3_general_ci\tNULL\trow_format=DYNAMIC stats_persistent=0\tkeyword-topic relation')"
expect_show_table_status_row \
    "help_topic" \
    "$(printf '%b' 'help_topic\tInnoDB\t10\tDynamic\t902\t1761\t1589248\t0\t98304\t4194304\tNULL')" \
    "$(printf '%b' 'NULL\tNULL\tutf8mb3_general_ci\tNULL\trow_format=DYNAMIC stats_persistent=0\thelp topics')"

printf '%s\n' "mysql_baseline_mysql_help_tables_expectations: ok"
