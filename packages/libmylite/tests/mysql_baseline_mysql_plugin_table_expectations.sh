#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_mysql_plugin_table_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

ensure_plugin_active() {
    plugin_name=$1
    plugin_library=$2

    plugin_status=$(run_mysql \
        "SELECT COALESCE(MAX(PLUGIN_STATUS), '') FROM INFORMATION_SCHEMA.PLUGINS "\
"WHERE PLUGIN_NAME = '${plugin_name}';")
    case "$plugin_status" in
        ACTIVE) return 0 ;;
        "") ;;
        *) fail "${plugin_name} plugin status is ${plugin_status}" ;;
    esac

    run_mysql "INSTALL PLUGIN ${plugin_name} SONAME '${plugin_library}';" >/dev/null

    plugin_status=$(run_mysql \
        "SELECT COALESCE(MAX(PLUGIN_STATUS), '') FROM INFORMATION_SCHEMA.PLUGINS "\
"WHERE PLUGIN_NAME = '${plugin_name}';")
    expect_value "${plugin_name} plugin active after install" "ACTIVE" "$plugin_status"
}

expect_show_table_status_row() {
    output=$(run_mysql "SHOW TABLE STATUS FROM mysql LIKE 'plugin';")
    field_count=$(printf '%s\n' "$output" | awk -F '\t' '{print NF}')
    prefix=$(printf '%s\n' "$output" | cut -f 1-4)
    storage_metrics=$(printf '%s\n' "$output" | cut -f 5-10)
    auto_increment=$(printf '%s\n' "$output" | cut -f 11)
    create_time=$(printf '%s\n' "$output" | cut -f 12)
    update_time=$(printf '%s\n' "$output" | cut -f 13)
    stable_tail=$(printf '%s\n' "$output" | cut -f 14-18)

    if [ "$field_count" != "18" ]; then
        fail "SHOW TABLE STATUS plugin: expected 18 fields, got [$field_count]"
    fi
    expected_prefix=$(printf '%b' "plugin\tInnoDB\t10\tDynamic")
    if [ "$prefix" != "$expected_prefix" ]; then
        fail "SHOW TABLE STATUS plugin: expected prefix [$expected_prefix], got [$prefix]"
    fi
    if ! printf '%s\n' "$storage_metrics" | awk -F '\t' 'NF == 6 {
        for (i = 1; i <= NF; i++) {
            if ($i !~ /^[0-9]+$/) {
                exit 1;
            }
        }
        exit 0;
    } { exit 1; }'; then
        fail "SHOW TABLE STATUS plugin: expected numeric storage metrics, got [$storage_metrics]"
    fi
    if [ "$auto_increment" != "NULL" ]; then
        fail "SHOW TABLE STATUS plugin: expected NULL Auto_increment, got [$auto_increment]"
    fi
    case "$create_time" in
        ????-??-??\ ??:??:??) ;;
        *) fail "SHOW TABLE STATUS plugin: expected Create_time datetime, got [$create_time]" ;;
    esac
    case "$update_time" in
        NULL | ????-??-??\ ??:??:??) ;;
        *) fail "SHOW TABLE STATUS plugin: expected NULL or Update_time datetime, got [$update_time]" ;;
    esac
    expected_tail=$(printf '%b' \
        "NULL\tutf8mb3_general_ci\tNULL\trow_format=DYNAMIC stats_persistent=0\tMySQL plugins")
    if [ "$stable_tail" != "$expected_tail" ]; then
        fail "SHOW TABLE STATUS plugin: expected tail [$expected_tail], got [$stable_tail]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

ensure_plugin_active "CONNECTION_CONTROL" "connection_control.so"
ensure_plugin_active "CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS" "connection_control.so"

plugin_rows=$(run_mysql "SELECT name, dl FROM mysql.plugin ORDER BY name;")
expected_plugin_rows="CONNECTION_CONTROL	connection_control.so
CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS	connection_control.so"
expect_value "mysql.plugin rows" "$expected_plugin_rows" "$plugin_rows"

expect_value \
    "mysql.plugin count" \
    "2" \
    "$(run_mysql "SELECT COUNT(*) FROM mysql.plugin;")"

expect_value \
    "mysql.plugin unqualified filtered rows" \
    "$expected_plugin_rows" \
    "$(run_mysql "USE mysql; SELECT name, dl FROM plugin WHERE name LIKE 'CONNECTION%' ORDER BY name;")"

columns_expected=$(
    printf '%b' \
        'name\tvarchar(64)\tNO\tPRI\t\t\n' \
        'dl\tvarchar(128)\tNO\t\t\t'
)
expect_value \
    "mysql.plugin SHOW COLUMNS rows" \
    "$columns_expected" \
    "$(run_mysql "SHOW COLUMNS FROM mysql.plugin;")"

full_columns_expected=$(
    printf '%b' \
        'name\tvarchar(64)\tutf8mb3_general_ci\tNO\tPRI\t\t\tselect,insert,update,references\t\n' \
        'dl\tvarchar(128)\tutf8mb3_general_ci\tNO\t\t\t\tselect,insert,update,references\t'
)
expect_value \
    "mysql.plugin SHOW FULL COLUMNS rows" \
    "$full_columns_expected" \
    "$(run_mysql "SHOW FULL COLUMNS FROM mysql.plugin;")"

expect_value \
    "mysql.plugin unqualified DESC rows" \
    "$columns_expected" \
    "$(run_mysql "USE mysql; DESC plugin;")"

show_index_expected=$(
    printf '%b' \
        'plugin\t0\tPRIMARY\t1\tname\tA\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL'
)
expect_value \
    "mysql.plugin SHOW INDEX row" \
    "$show_index_expected" \
    "$(run_mysql "SHOW INDEX FROM mysql.plugin;" | cut -f 1-6,8-15)"

expect_value \
    "mysql.plugin unqualified SHOW KEYS row" \
    "$show_index_expected" \
    "$(run_mysql "USE mysql; SHOW KEYS FROM plugin WHERE Key_name = 'PRIMARY';" | cut -f 1-6,8-15)"

columns_metadata_expected=$(
    printf '%b' \
        'name\t1\t\tNO\tvarchar\t64\t192\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tvarchar(64)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'dl\t2\t\tNO\tvarchar\t128\t384\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tvarchar(128)\t\t\tselect,insert,update,references\t\t'
)
expect_value \
    "mysql.plugin INFORMATION_SCHEMA.COLUMNS rows" \
    "$columns_metadata_expected" \
    "$(run_mysql "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
            CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
            NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME,
            COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT,
            GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'plugin'
      ORDER BY ORDINAL_POSITION;")"

expect_value \
    "mysql.plugin TABLE_CONSTRAINTS row" \
    "PRIMARY	PRIMARY KEY	YES" \
    "$(run_mysql "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'plugin';")"

expect_value \
    "mysql.plugin KEY_COLUMN_USAGE row" \
    "PRIMARY	name	1	NULL	NULL	NULL	NULL" \
    "$(run_mysql "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION,
            POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA,
            REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'plugin';")"

expect_value \
    "mysql.plugin TABLE_CONSTRAINTS_EXTENSIONS row" \
    "PRIMARY	plugin	NULL	NULL" \
    "$(run_mysql "SELECT CONSTRAINT_NAME, TABLE_NAME, ENGINE_ATTRIBUTE,
            SECONDARY_ENGINE_ATTRIBUTE
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'mysql' AND TABLE_NAME = 'plugin';")"

expect_value \
    "mysql.plugin INFORMATION_SCHEMA.STATISTICS row" \
    "PRIMARY	1	name	A	NULL	NULL		BTREE			YES	NULL" \
    "$(run_mysql "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION,
            SUB_PART, PACKED, NULLABLE, INDEX_TYPE, COMMENT, INDEX_COMMENT,
            IS_VISIBLE, EXPRESSION
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'plugin';")"

tables_expected=$(
    printf '%b' \
        'plugin\tBASE TABLE\tInnoDB\t10\tDynamic\tNULL\t1\t1\tutf8mb3_general_ci\t1\trow_format=DYNAMIC stats_persistent=0\tMySQL plugins'
)
expect_value \
    "mysql.plugin INFORMATION_SCHEMA.TABLES row" \
    "$tables_expected" \
    "$(run_mysql "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, CHECK_TIME IS NULL,
            TABLE_COLLATION, CHECKSUM IS NULL, CREATE_OPTIONS,
            TABLE_COMMENT
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'plugin';")"

expect_show_table_status_row

status=$(run_mysql "SELECT name, dl FROM mysql.plugin ORDER BY name LIMIT 1; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "mysql.plugin warning and row count status" "0	-1" "$status"

printf '%s\n' "mysql_baseline_mysql_plugin_table_expectations: ok"
