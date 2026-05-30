#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_mysql_ndb_binlog_index_expectations: $1" >&2
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
    output=$(run_mysql "SHOW TABLE STATUS FROM mysql LIKE 'ndb_binlog_index';")
    field_count=$(printf '%s\n' "$output" | awk -F '\t' '{print NF}')
    prefix=$(printf '%s\n' "$output" | cut -f 1-11)
    create_time=$(printf '%s\n' "$output" | cut -f 12)
    stable_tail=$(printf '%s\n' "$output" | cut -f 13-18)

    if [ "$field_count" != "18" ]; then
        fail "SHOW TABLE STATUS: expected 18 fields, got [$field_count]"
    fi
    if [ "$prefix" != "$(printf '%b' 'ndb_binlog_index\tInnoDB\t10\tDynamic\t0\t0\t16384\t0\t0\t4194304\tNULL')" ]; then
        fail "SHOW TABLE STATUS: unexpected prefix [$prefix]"
    fi
    case "$create_time" in
        ????-??-??\ ??:??:??) ;;
        *) fail "SHOW TABLE STATUS: expected Create_time datetime, got [$create_time]" ;;
    esac
    if [ "$stable_tail" != "$(printf '%b' 'NULL\tNULL\tlatin1_swedish_ci\tNULL\trow_format=DYNAMIC stats_persistent=0\t')" ]; then
        fail "SHOW TABLE STATUS: unexpected tail [$stable_tail]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output \
    "mysql.ndb_binlog_index row count" \
    "0" \
    "SELECT COUNT(*) FROM mysql.ndb_binlog_index;"

expect_output \
    "selected mysql.ndb_binlog_index row count" \
    "0" \
    "USE mysql; SELECT COUNT(*) FROM ndb_binlog_index;"

expect_output \
    "mysql.ndb_binlog_index SHOW COLUMNS" \
    "$(printf '%b' \
        'Position\tbigint unsigned\tNO\t\tNULL\t\n' \
        'File\tvarchar(255)\tNO\t\tNULL\t\n' \
        'epoch\tbigint unsigned\tNO\tPRI\tNULL\t\n' \
        'inserts\tint unsigned\tNO\t\tNULL\t\n' \
        'updates\tint unsigned\tNO\t\tNULL\t\n' \
        'deletes\tint unsigned\tNO\t\tNULL\t\n' \
        'schemaops\tint unsigned\tNO\t\tNULL\t\n' \
        'orig_server_id\tint unsigned\tNO\tPRI\tNULL\t\n' \
        'orig_epoch\tbigint unsigned\tNO\tPRI\tNULL\t\n' \
        'gci\tint unsigned\tNO\t\tNULL\t\n' \
        'next_position\tbigint unsigned\tNO\t\tNULL\t\n' \
        'next_file\tvarchar(255)\tNO\t\tNULL\t')" \
    "SHOW COLUMNS FROM mysql.ndb_binlog_index;"

expect_output \
    "mysql.ndb_binlog_index SHOW FULL COLUMNS" \
    "$(printf '%b' \
        'Position\tbigint unsigned\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'File\tvarchar(255)\tlatin1_swedish_ci\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'epoch\tbigint unsigned\tNULL\tNO\tPRI\tNULL\t\tselect,insert,update,references\t\n' \
        'inserts\tint unsigned\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'updates\tint unsigned\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'deletes\tint unsigned\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'schemaops\tint unsigned\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'orig_server_id\tint unsigned\tNULL\tNO\tPRI\tNULL\t\tselect,insert,update,references\t\n' \
        'orig_epoch\tbigint unsigned\tNULL\tNO\tPRI\tNULL\t\tselect,insert,update,references\t\n' \
        'gci\tint unsigned\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'next_position\tbigint unsigned\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'next_file\tvarchar(255)\tlatin1_swedish_ci\tNO\t\tNULL\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM mysql.ndb_binlog_index;"

expect_output \
    "mysql.ndb_binlog_index SHOW INDEX" \
    "$(printf '%b' \
        'ndb_binlog_index\t0\tPRIMARY\t1\tepoch\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'ndb_binlog_index\t0\tPRIMARY\t2\torig_server_id\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'ndb_binlog_index\t0\tPRIMARY\t3\torig_epoch\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL')" \
    "SHOW INDEX FROM mysql.ndb_binlog_index;"

columns_expected=$(
    printf '%b' \
        'Position\t1\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t\t\n' \
        'File\t2\tNULL\tNO\tvarchar\t255\t255\tNULL\tNULL\tNULL\tlatin1\tlatin1_swedish_ci\tvarchar(255)\t\t\tselect,insert,update,references\t\t\n' \
        'epoch\t3\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tNULL\tbigint unsigned\tPRI\t\tselect,insert,update,references\t\t\n' \
        'inserts\t4\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\t\t\tselect,insert,update,references\t\t\n' \
        'updates\t5\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\t\t\tselect,insert,update,references\t\t\n' \
        'deletes\t6\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\t\t\tselect,insert,update,references\t\t\n' \
        'schemaops\t7\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\t\t\tselect,insert,update,references\t\t\n' \
        'orig_server_id\t8\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\tPRI\t\tselect,insert,update,references\t\t\n' \
        'orig_epoch\t9\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tNULL\tbigint unsigned\tPRI\t\tselect,insert,update,references\t\t\n' \
        'gci\t10\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\t\t\tselect,insert,update,references\t\t\n' \
        'next_position\t11\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t\t\n' \
        'next_file\t12\tNULL\tNO\tvarchar\t255\t255\tNULL\tNULL\tNULL\tlatin1\tlatin1_swedish_ci\tvarchar(255)\t\t\tselect,insert,update,references\t\t'
)
expect_output \
    "mysql.ndb_binlog_index INFORMATION_SCHEMA.COLUMNS rows" \
    "$columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE,
            DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH,
            NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION,
            CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, EXTRA,
            PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'ndb_binlog_index'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "mysql.ndb_binlog_index INFORMATION_SCHEMA.STATISTICS rows" \
    "$(printf '%b' \
        'PRIMARY\t1\tepoch\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'PRIMARY\t2\torig_server_id\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'PRIMARY\t3\torig_epoch\tA\t0\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL')" \
    "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, CARDINALITY,
            SUB_PART, PACKED, NULLABLE, INDEX_TYPE, COMMENT, INDEX_COMMENT,
            IS_VISIBLE, EXPRESSION
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'ndb_binlog_index'
      ORDER BY INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "mysql.ndb_binlog_index TABLE_CONSTRAINTS row" \
    "$(printf '%b' 'PRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'ndb_binlog_index';"

expect_output \
    "mysql.ndb_binlog_index KEY_COLUMN_USAGE rows" \
    "$(printf '%b' 'PRIMARY\tepoch\t1\tNULL\tNULL\tNULL\tNULL\n' \
        'PRIMARY\torig_server_id\t2\tNULL\tNULL\tNULL\tNULL\n' \
        'PRIMARY\torig_epoch\t3\tNULL\tNULL\tNULL\tNULL')" \
    "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION,
            POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA,
            REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'ndb_binlog_index'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "mysql.ndb_binlog_index TABLE_CONSTRAINTS_EXTENSIONS row" \
    "$(printf '%b' 'PRIMARY\tNULL\tNULL')" \
    "SELECT CONSTRAINT_NAME, ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'mysql' AND TABLE_NAME = 'ndb_binlog_index';"

expect_output \
    "mysql.ndb_binlog_index INFORMATION_SCHEMA.TABLES row" \
    "$(printf '%b' 'ndb_binlog_index\tBASE TABLE\tInnoDB\t10\tDynamic\t0\t0\t16384\t0\t0\t4194304\tNULL\t1\t1\t1\tlatin1_swedish_ci\t1\trow_format=DYNAMIC stats_persistent=0\t')" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH,
            DATA_FREE, AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL,
            CREATE_OPTIONS, TABLE_COMMENT
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'ndb_binlog_index';"

expect_show_table_status_row

printf '%s\n' "mysql_baseline_mysql_ndb_binlog_index_expectations: ok"
