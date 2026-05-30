#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_conditional_table_absence: $1" >&2
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

conditional_tables="
MYSQL_FIREWALL_USERS
MYSQL_FIREWALL_WHITELIST
ndb_transid_mysql_connection_map
TP_THREAD_GROUP_STATE
TP_THREAD_GROUP_STATS
TP_THREAD_STATE
"

for table_name in $conditional_tables; do
    expect_error \
        "${table_name} direct read" \
        1109 \
        42S02 \
        "Unknown table '${table_name}' in information_schema" \
        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.${table_name};"
    expect_error \
        "${table_name} show columns" \
        1109 \
        42S02 \
        "Unknown table '${table_name}' in information_schema" \
        "SHOW COLUMNS FROM INFORMATION_SCHEMA.${table_name};"
done

expect_error \
    "unqualified selected information_schema read" \
    1109 \
    42S02 \
    "Unknown table 'MYSQL_FIREWALL_USERS' in information_schema" \
    "USE information_schema; SELECT COUNT(*) FROM MYSQL_FIREWALL_USERS;"

expect_output \
    "conditional tables absent from INFORMATION_SCHEMA.TABLES" \
    "0" \
    "SELECT COUNT(*)
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'information_schema'
        AND TABLE_NAME IN ('MYSQL_FIREWALL_USERS',
                           'MYSQL_FIREWALL_WHITELIST',
                           'ndb_transid_mysql_connection_map',
                           'TP_THREAD_GROUP_STATE',
                           'TP_THREAD_GROUP_STATS',
                           'TP_THREAD_STATE');"
expect_output \
    "conditional tables absent from INFORMATION_SCHEMA.COLUMNS" \
    "0" \
    "SELECT COUNT(*)
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'information_schema'
        AND TABLE_NAME IN ('MYSQL_FIREWALL_USERS',
                           'MYSQL_FIREWALL_WHITELIST',
                           'ndb_transid_mysql_connection_map',
                           'TP_THREAD_GROUP_STATE',
                           'TP_THREAD_GROUP_STATS',
                           'TP_THREAD_STATE');"
expect_output \
    "conditional tables absent from SHOW FULL TABLES" \
    "" \
    "SHOW FULL TABLES FROM information_schema
      WHERE Tables_in_information_schema IN ('MYSQL_FIREWALL_USERS',
                                             'MYSQL_FIREWALL_WHITELIST',
                                             'ndb_transid_mysql_connection_map',
                                             'TP_THREAD_GROUP_STATE',
                                             'TP_THREAD_GROUP_STATS',
                                             'TP_THREAD_STATE');"

printf '%s\n' "mysql_baseline_information_schema_conditional_table_absence: ok"
