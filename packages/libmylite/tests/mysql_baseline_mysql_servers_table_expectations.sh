#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_mysql_servers_table_expectations: $1" >&2
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
    output=$(run_mysql "SHOW TABLE STATUS FROM mysql LIKE 'servers';")
    field_count=$(printf '%s\n' "$output" | awk -F '\t' '{print NF}')
    prefix=$(printf '%s\n' "$output" | cut -f 1-11)
    create_time=$(printf '%s\n' "$output" | cut -f 12)
    stable_tail=$(printf '%s\n' "$output" | cut -f 13-18)

    if [ "$field_count" != "18" ]; then
        fail "SHOW TABLE STATUS servers: expected 18 fields, got [$field_count]"
    fi
    expected_prefix=$(printf '%b' 'servers\tInnoDB\t10\tDynamic\t0\t0\t16384\t0\t0\t4194304\tNULL')
    if [ "$prefix" != "$expected_prefix" ]; then
        fail "SHOW TABLE STATUS servers: expected prefix [$expected_prefix], got [$prefix]"
    fi
    case "$create_time" in
        ????-??-??\ ??:??:??) ;;
        *) fail "SHOW TABLE STATUS servers: expected Create_time datetime, got [$create_time]" ;;
    esac
    expected_tail=$(printf '%b' 'NULL\tNULL\tutf8mb3_general_ci\tNULL\trow_format=DYNAMIC stats_persistent=0\tMySQL Foreign Servers table')
    if [ "$stable_tail" != "$expected_tail" ]; then
        fail "SHOW TABLE STATUS servers: expected tail [$expected_tail], got [$stable_tail]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output "mysql.servers baseline row count" "0" "SELECT COUNT(*) FROM mysql.servers;"
expect_output \
    "mysql.servers direct read is empty" \
    "" \
    "SELECT Server_name, Host, Db, Username, Password, Port, Socket, Wrapper, Owner
       FROM mysql.servers
      ORDER BY Server_name;"
expect_output \
    "mysql.servers read ROW_COUNT" \
    "-1" \
    "SELECT Server_name, Host, Db, Username, Password, Port, Socket, Wrapper, Owner
       FROM mysql.servers
      ORDER BY Server_name;
     SELECT ROW_COUNT();"

columns_expected=$(
    printf '%b' \
        'Server_name\tchar(64)\tNO\tPRI\t\t\n' \
        'Host\tchar(255)\tNO\t\t\t\n' \
        'Db\tchar(64)\tNO\t\t\t\n' \
        'Username\tchar(64)\tNO\t\t\t\n' \
        'Password\tchar(64)\tNO\t\t\t\n' \
        'Port\tint\tNO\t\t0\t\n' \
        'Socket\tchar(64)\tNO\t\t\t\n' \
        'Wrapper\tchar(64)\tNO\t\t\t\n' \
        'Owner\tchar(64)\tNO\t\t\t'
)
expect_output \
    "mysql.servers SHOW COLUMNS rows" \
    "$columns_expected" \
    "SHOW COLUMNS FROM mysql.servers;"

full_columns_expected=$(
    printf '%b' \
        'Server_name\tchar(64)\tutf8mb3_general_ci\tNO\tPRI\t\t\tselect,insert,update,references\t\n' \
        'Host\tchar(255)\tascii_general_ci\tNO\t\t\t\tselect,insert,update,references\t\n' \
        'Db\tchar(64)\tutf8mb3_general_ci\tNO\t\t\t\tselect,insert,update,references\t\n' \
        'Username\tchar(64)\tutf8mb3_general_ci\tNO\t\t\t\tselect,insert,update,references\t\n' \
        'Password\tchar(64)\tutf8mb3_general_ci\tNO\t\t\t\tselect,insert,update,references\t\n' \
        'Port\tint\tNULL\tNO\t\t0\t\tselect,insert,update,references\t\n' \
        'Socket\tchar(64)\tutf8mb3_general_ci\tNO\t\t\t\tselect,insert,update,references\t\n' \
        'Wrapper\tchar(64)\tutf8mb3_general_ci\tNO\t\t\t\tselect,insert,update,references\t\n' \
        'Owner\tchar(64)\tutf8mb3_general_ci\tNO\t\t\t\tselect,insert,update,references\t'
)
expect_output \
    "mysql.servers SHOW FULL COLUMNS rows" \
    "$full_columns_expected" \
    "SHOW FULL COLUMNS FROM mysql.servers;"

show_index_expected=$(
    printf '%b' \
        'servers\t0\tPRIMARY\t1\tServer_name\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL'
)
expect_output \
    "mysql.servers SHOW INDEX row" \
    "$show_index_expected" \
    "SHOW INDEX FROM mysql.servers;"

information_schema_columns_expected=$(
    printf '%b' \
        'Server_name\t1\t\tNO\tchar\t64\t192\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tchar(64)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'Host\t2\t\tNO\tchar\t255\t255\tNULL\tNULL\tNULL\tascii\tascii_general_ci\tchar(255)\t\t\tselect,insert,update,references\t\t\n' \
        'Db\t3\t\tNO\tchar\t64\t192\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tchar(64)\t\t\tselect,insert,update,references\t\t\n' \
        'Username\t4\t\tNO\tchar\t64\t192\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tchar(64)\t\t\tselect,insert,update,references\t\t\n' \
        'Password\t5\t\tNO\tchar\t64\t192\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tchar(64)\t\t\tselect,insert,update,references\t\t\n' \
        'Port\t6\t0\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint\t\t\tselect,insert,update,references\t\t\n' \
        'Socket\t7\t\tNO\tchar\t64\t192\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tchar(64)\t\t\tselect,insert,update,references\t\t\n' \
        'Wrapper\t8\t\tNO\tchar\t64\t192\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tchar(64)\t\t\tselect,insert,update,references\t\t\n' \
        'Owner\t9\t\tNO\tchar\t64\t192\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tchar(64)\t\t\tselect,insert,update,references\t\t'
)
expect_output \
    "mysql.servers INFORMATION_SCHEMA.COLUMNS rows" \
    "$information_schema_columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
            CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
            NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME,
            COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT,
            GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'servers'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "mysql.servers INFORMATION_SCHEMA.TABLE_CONSTRAINTS row" \
    "$(printf '%b' 'PRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'servers';"

expect_output \
    "mysql.servers INFORMATION_SCHEMA.KEY_COLUMN_USAGE row" \
    "$(printf '%b' 'PRIMARY\tServer_name\t1\tNULL\tNULL\tNULL\tNULL')" \
    "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION,
            POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA,
            REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'servers';"

expect_output \
    "mysql.servers INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS row" \
    "$(printf '%b' 'PRIMARY\tservers\tNULL\tNULL')" \
    "SELECT CONSTRAINT_NAME, TABLE_NAME, ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'mysql' AND TABLE_NAME = 'servers';"

expect_output \
    "mysql.servers INFORMATION_SCHEMA.STATISTICS row" \
    "$(printf '%b' 'PRIMARY\t1\tServer_name\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL')" \
    "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, CARDINALITY, SUB_PART,
            PACKED, NULLABLE, INDEX_TYPE, COMMENT, INDEX_COMMENT, IS_VISIBLE,
            EXPRESSION
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'servers';"

expect_output \
    "mysql.servers INFORMATION_SCHEMA.TABLES row" \
    "$(printf '%b' 'servers\tBASE TABLE\tInnoDB\t10\tDynamic\t0\t0\t16384\t0\t0\t4194304\tNULL\t1\t1\t1\tutf8mb3_general_ci\t1\trow_format=DYNAMIC stats_persistent=0\tMySQL Foreign Servers table')" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL, CREATE_OPTIONS,
            TABLE_COMMENT
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'servers';"

expect_show_table_status_row

printf '%s\n' "mysql_baseline_mysql_servers_table_expectations: ok"
