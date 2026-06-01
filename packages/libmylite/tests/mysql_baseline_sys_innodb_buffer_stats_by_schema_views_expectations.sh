#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_innodb_buffer_stats_by_schema_views_expectations: $1" >&2
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

expect_contains() {
    label=$1
    needle=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    case "$output" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle], got [$output]" ;;
    esac
}

expect_show_table_status_row() {
    table_name=$1
    output=$(run_mysql "SHOW TABLE STATUS FROM sys LIKE '$table_name';")
    field_count=$(printf '%s\n' "$output" | awk -F '\t' '{print NF}')
    prefix=$(printf '%s\n' "$output" | cut -f 1-11)
    create_time=$(printf '%s\n' "$output" | cut -f 12)
    suffix=$(printf '%s\n' "$output" | cut -f 13-18)

    if [ "$field_count" != "18" ]; then
        fail "SHOW TABLE STATUS sys.$table_name: expected 18 fields, got [$field_count]"
    fi
    expected_prefix=$(printf '%b' "$table_name\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL")
    if [ "$prefix" != "$expected_prefix" ]; then
        fail "SHOW TABLE STATUS sys.$table_name: expected prefix [$expected_prefix], got [$prefix]"
    fi
    case "$create_time" in
        ????-??-??\ ??:??:??) ;;
        *) fail "SHOW TABLE STATUS sys.$table_name: expected Create_time datetime, got [$create_time]" ;;
    esac
    expected_suffix=$(printf '%b' 'NULL\tNULL\tNULL\tNULL\tNULL\tVIEW')
    if [ "$suffix" != "$expected_suffix" ]; then
        fail "SHOW TABLE STATUS sys.$table_name: expected suffix [$expected_suffix], got [$suffix]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

formatted_show_columns=$(
    printf '%b' \
'object_schema\ttext\tYES\t\tNULL\t
allocated\tvarchar(11)\tYES\t\tNULL\t
data\tvarchar(11)\tYES\t\tNULL\t
pages\tbigint\tNO\t\t0\t
pages_hashed\tbigint\tNO\t\t0\t
pages_old\tbigint\tNO\t\t0\t
rows_cached\tdecimal(45,0)\tYES\t\tNULL\t'
)

raw_show_columns=$(
    printf '%b' \
'object_schema\ttext\tYES\t\tNULL\t
allocated\tdecimal(44,0)\tYES\t\tNULL\t
data\tdecimal(44,0)\tYES\t\tNULL\t
pages\tbigint\tNO\t\t0\t
pages_hashed\tbigint\tNO\t\t0\t
pages_old\tbigint\tNO\t\t0\t
rows_cached\tdecimal(45,0)\tNO\t\t0\t'
)

expect_output \
    "sys innodb buffer stats by schema SHOW COLUMNS" \
    "$formatted_show_columns" \
    "SHOW COLUMNS FROM sys.innodb_buffer_stats_by_schema;"

expect_output \
    "sys x innodb buffer stats by schema SHOW COLUMNS" \
    "$raw_show_columns" \
    "SHOW COLUMNS FROM sys.\`x\$innodb_buffer_stats_by_schema\`;"

expect_output \
    "sys innodb buffer stats by schema SHOW FULL columns" \
    "$(printf '%b' 'object_schema\ttext\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\nallocated\tvarchar(11)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\ndata\tvarchar(11)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\npages\tbigint\tNULL\tNO\t\t0\t\tselect,insert,update,references\t\npages_hashed\tbigint\tNULL\tNO\t\t0\t\tselect,insert,update,references\t\npages_old\tbigint\tNULL\tNO\t\t0\t\tselect,insert,update,references\t\nrows_cached\tdecimal(45,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM sys.innodb_buffer_stats_by_schema;"

expect_output \
    "sys x innodb buffer stats by schema SHOW FULL columns" \
    "$(printf '%b' 'object_schema\ttext\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\nallocated\tdecimal(44,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\ndata\tdecimal(44,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\npages\tbigint\tNULL\tNO\t\t0\t\tselect,insert,update,references\t\npages_hashed\tbigint\tNULL\tNO\t\t0\t\tselect,insert,update,references\t\npages_old\tbigint\tNULL\tNO\t\t0\t\tselect,insert,update,references\t\nrows_cached\tdecimal(45,0)\tNULL\tNO\t\t0\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM sys.\`x\$innodb_buffer_stats_by_schema\`;"

expect_output \
    "sys innodb buffer stats by schema INFORMATION_SCHEMA.COLUMNS" \
    "$(printf '%b' 'innodb_buffer_stats_by_schema\tobject_schema\t1\tYES\ttext\tutf8mb3\tutf8mb3_general_ci\t65535\t65535\tNULL\tNULL\ninnodb_buffer_stats_by_schema\tallocated\t2\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\t11\t33\tNULL\tNULL\ninnodb_buffer_stats_by_schema\tdata\t3\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\t11\t33\tNULL\tNULL\ninnodb_buffer_stats_by_schema\tpages\t4\tNO\tbigint\tNULL\tNULL\tNULL\tNULL\t19\t0\ninnodb_buffer_stats_by_schema\tpages_hashed\t5\tNO\tbigint\tNULL\tNULL\tNULL\tNULL\t19\t0\ninnodb_buffer_stats_by_schema\tpages_old\t6\tNO\tbigint\tNULL\tNULL\tNULL\tNULL\t19\t0\ninnodb_buffer_stats_by_schema\trows_cached\t7\tYES\tdecimal(45,0)\tNULL\tNULL\tNULL\tNULL\t45\t0\nx$innodb_buffer_stats_by_schema\tobject_schema\t1\tYES\ttext\tutf8mb3\tutf8mb3_general_ci\t65535\t65535\tNULL\tNULL\nx$innodb_buffer_stats_by_schema\tallocated\t2\tYES\tdecimal(44,0)\tNULL\tNULL\tNULL\tNULL\t44\t0\nx$innodb_buffer_stats_by_schema\tdata\t3\tYES\tdecimal(44,0)\tNULL\tNULL\tNULL\tNULL\t44\t0\nx$innodb_buffer_stats_by_schema\tpages\t4\tNO\tbigint\tNULL\tNULL\tNULL\tNULL\t19\t0\nx$innodb_buffer_stats_by_schema\tpages_hashed\t5\tNO\tbigint\tNULL\tNULL\tNULL\tNULL\t19\t0\nx$innodb_buffer_stats_by_schema\tpages_old\t6\tNO\tbigint\tNULL\tNULL\tNULL\tNULL\t19\t0\nx$innodb_buffer_stats_by_schema\trows_cached\t7\tNO\tdecimal(45,0)\tNULL\tNULL\tNULL\tNULL\t45\t0')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COALESCE(CHARACTER_SET_NAME, 'NULL'), COALESCE(COLLATION_NAME, 'NULL'),
            COALESCE(CHARACTER_MAXIMUM_LENGTH, 'NULL'),
            COALESCE(CHARACTER_OCTET_LENGTH, 'NULL'),
            COALESCE(NUMERIC_PRECISION, 'NULL'), COALESCE(NUMERIC_SCALE, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('innodb_buffer_stats_by_schema', 'x\$innodb_buffer_stats_by_schema')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "sys innodb buffer stats by schema INFORMATION_SCHEMA.TABLES" \
    "$(printf '%b' 'sys\tinnodb_buffer_stats_by_schema\tVIEW\tNULL\tNULL\tNULL\tVIEW\nsys\tx$innodb_buffer_stats_by_schema\tVIEW\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, COALESCE(ENGINE, 'NULL'),
            COALESCE(TABLE_ROWS, 'NULL'), COALESCE(DATA_LENGTH, 'NULL'), TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('innodb_buffer_stats_by_schema', 'x\$innodb_buffer_stats_by_schema')
      ORDER BY TABLE_NAME;"

expect_show_table_status_row "innodb_buffer_stats_by_schema"
expect_show_table_status_row "x\$innodb_buffer_stats_by_schema"

expect_output \
    "sys innodb buffer stats by schema INFORMATION_SCHEMA.VIEWS" \
    "$(printf '%b' 'sys\tinnodb_buffer_stats_by_schema\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nsys\tx$innodb_buffer_stats_by_schema\tNONE\tNO\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('innodb_buffer_stats_by_schema', 'x\$innodb_buffer_stats_by_schema')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys innodb buffer stats by schema table dependency metadata" \
    "$(printf '%b' 'sys\tinnodb_buffer_stats_by_schema\tinformation_schema\tINNODB_BUFFER_PAGE\nsys\tx$innodb_buffer_stats_by_schema\tinformation_schema\tINNODB_BUFFER_PAGE')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME IN ('innodb_buffer_stats_by_schema', 'x\$innodb_buffer_stats_by_schema')
      ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME;"

expect_output \
    "sys innodb buffer stats by schema empty routine dependency metadata" \
    "" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('innodb_buffer_stats_by_schema', 'x\$innodb_buffer_stats_by_schema')
      ORDER BY TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME;"

expect_output \
    "sys innodb buffer stats by schema empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0\t0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'innodb_buffer_stats_by_schema'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$innodb_buffer_stats_by_schema'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'innodb_buffer_stats_by_schema'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$innodb_buffer_stats_by_schema'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'innodb_buffer_stats_by_schema'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$innodb_buffer_stats_by_schema'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'innodb_buffer_stats_by_schema'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'x\$innodb_buffer_stats_by_schema');"

expect_contains \
    "sys innodb buffer stats by schema SHOW CREATE VIEW" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`innodb_buffer_stats_by_schema\`" \
    "SHOW CREATE VIEW sys.innodb_buffer_stats_by_schema;"

expect_contains \
    "sys innodb buffer stats by schema SHOW CREATE source" \
    "from \`information_schema\`.\`INNODB_BUFFER_PAGE\` \`ibp\` where" \
    "SHOW CREATE VIEW sys.innodb_buffer_stats_by_schema;"

expect_contains \
    "sys innodb buffer stats by schema SHOW CREATE grouping" \
    "group by \`object_schema\` order by sum(if((\`ibp\`.\`COMPRESSED_SIZE\` = 0),16384,\`ibp\`.\`COMPRESSED_SIZE\`)) desc" \
    "SHOW CREATE VIEW sys.innodb_buffer_stats_by_schema;"

expect_contains \
    "sys x innodb buffer stats by schema SHOW CREATE VIEW" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`x\$innodb_buffer_stats_by_schema\`" \
    "SHOW CREATE VIEW sys.\`x\$innodb_buffer_stats_by_schema\`;"

expect_contains \
    "sys innodb buffer stats by schema SHOW CREATE TABLE selected schema" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`innodb_buffer_stats_by_schema\`" \
    "USE sys; SHOW CREATE TABLE innodb_buffer_stats_by_schema;"

expect_contains \
    "sys x innodb buffer stats by schema SHOW CREATE TABLE selected schema" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`x\$innodb_buffer_stats_by_schema\`" \
    "USE sys; SHOW CREATE TABLE \`x\$innodb_buffer_stats_by_schema\`;"

expect_output \
    "sys innodb buffer stats by schema has runtime rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.innodb_buffer_stats_by_schema;"

expect_output \
    "sys x innodb buffer stats by schema has runtime rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.\`x\$innodb_buffer_stats_by_schema\`;"

expect_output \
    "sys innodb buffer stats by schema selected rows" \
    "1" \
    "USE sys; SELECT COUNT(*) > 0 FROM innodb_buffer_stats_by_schema;"

expect_output \
    "sys x innodb buffer stats by schema selected rows" \
    "1" \
    "USE sys; SELECT COUNT(*) > 0 FROM \`x\$innodb_buffer_stats_by_schema\`;"

status=$(run_mysql "SELECT COUNT(*) FROM sys.innodb_buffer_stats_by_schema; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys innodb buffer stats by schema SELECT status: expected [0	-1], got [$status]"
fi

status=$(run_mysql "SELECT COUNT(*) FROM sys.\`x\$innodb_buffer_stats_by_schema\`; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys x innodb buffer stats by schema SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_innodb_buffer_stats_by_schema_views_expectations: ok"
