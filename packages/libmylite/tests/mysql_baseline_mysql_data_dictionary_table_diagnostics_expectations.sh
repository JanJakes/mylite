#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_mysql_data_dictionary_table_diagnostics: $1" >&2
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

expect_error() {
    label=$1
    code=$2
    state=$3
    message=$4
    sql=$5
    shift 5

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -eq 0 ]; then
        fail "$label: expected error $code/$state, command succeeded with [$output]"
    fi

    case "$output" in
        *"ERROR $code ($state)"*"$message"*) ;;
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
    esac
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

dictionary_tables="
catalogs
character_sets
check_constraints
collations
column_statistics
column_type_elements
columns
dd_properties
events
foreign_keys
foreign_key_column_usage
index_column_usage
index_partitions
index_stats
indexes
parameter_type_elements
parameters
resource_groups
routines
schemata
st_spatial_reference_systems
table_partition_values
table_partitions
table_stats
tables
tablespace_files
tablespaces
triggers
view_routine_usage
view_table_usage
"

for table_name in $dictionary_tables; do
    expect_error \
        "mysql.${table_name} direct read" \
        3554 \
        HY000 \
        "Access to data dictionary table 'mysql.${table_name}' is rejected." \
        "SELECT COUNT(*) FROM mysql.${table_name};"
done

expect_error \
    "mysql.innodb_ddl_log direct read" \
    3554 \
    HY000 \
    "Access to system table 'mysql.innodb_ddl_log' is rejected." \
    "SELECT COUNT(*) FROM mysql.innodb_ddl_log;"

expect_error \
    "mysql.innodb_dynamic_metadata direct read" \
    3554 \
    HY000 \
    "Access to system table 'mysql.innodb_dynamic_metadata' is rejected." \
    "SELECT COUNT(*) FROM mysql.innodb_dynamic_metadata;"

expect_error \
    "mysql.catalogs show columns" \
    3554 \
    HY000 \
    "Access to data dictionary table 'mysql.catalogs' is rejected." \
    "SHOW COLUMNS FROM mysql.catalogs;"
expect_error \
    "mysql.schemata show index" \
    3554 \
    HY000 \
    "Access to data dictionary table 'mysql.schemata' is rejected." \
    "SHOW INDEX FROM mysql.schemata;"
expect_error \
    "mysql.tables describe" \
    3554 \
    HY000 \
    "Access to data dictionary table 'mysql.tables' is rejected." \
    "DESC mysql.tables;"
expect_error \
    "mysql.innodb_ddl_log show columns" \
    3554 \
    HY000 \
    "Access to system table 'mysql.innodb_ddl_log' is rejected." \
    "SHOW COLUMNS FROM mysql.innodb_ddl_log;"
expect_error \
    "mysql.innodb_dynamic_metadata show index" \
    3554 \
    HY000 \
    "Access to system table 'mysql.innodb_dynamic_metadata' is rejected." \
    "SHOW INDEX FROM mysql.innodb_dynamic_metadata;"
expect_error \
    "mysql.schemata unqualified direct read" \
    3554 \
    HY000 \
    "Access to data dictionary table 'mysql.schemata' is rejected." \
    "USE mysql; SELECT COUNT(*) FROM schemata;"
expect_error \
    "mysql.innodb_dynamic_metadata unqualified direct read" \
    3554 \
    HY000 \
    "Access to system table 'mysql.innodb_dynamic_metadata' is rejected." \
    "USE mysql; SELECT COUNT(*) FROM innodb_dynamic_metadata;"

expect_output \
    "hidden dictionary tables are absent from INFORMATION_SCHEMA.TABLES" \
    "0" \
    "SELECT COUNT(*)
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'mysql'
        AND TABLE_NAME IN ('catalogs','schemata','tables','innodb_ddl_log',
                           'innodb_dynamic_metadata');"
expect_output \
    "hidden dictionary tables are absent from SHOW FULL TABLES" \
    "" \
    "SHOW FULL TABLES FROM mysql
      WHERE Tables_in_mysql IN ('catalogs','schemata','tables','innodb_ddl_log',
                                'innodb_dynamic_metadata');"
expect_output \
    "mysql.innodb_dynamic_metadata is absent from SHOW TABLE STATUS" \
    "" \
    "SHOW TABLE STATUS FROM mysql LIKE 'innodb_dynamic_metadata';"

printf '%s\n' "mysql_baseline_mysql_data_dictionary_table_diagnostics: ok"
