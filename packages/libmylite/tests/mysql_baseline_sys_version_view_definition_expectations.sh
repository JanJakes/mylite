#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_version_view_definition_expectations: $1" >&2
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

view_definition="select '2.1.3' AS \`sys_version\`,version() AS \`mysql_version\`"
qualified_show_create="CREATE ALGORITHM=UNDEFINED DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`version\` (\`sys_version\`,\`mysql_version\`) AS ${view_definition}"
unqualified_show_create="CREATE ALGORITHM=UNDEFINED DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`version\` (\`sys_version\`,\`mysql_version\`) AS ${view_definition}"

expect_output \
    "sys.version INFORMATION_SCHEMA.VIEWS row" \
    "$(printf '%b' "def\tsys\tversion\t${view_definition}\tNONE\tNO\tmysql.sys@localhost\tINVOKER\tutf8mb4\tutf8mb4_0900_ai_ci")" \
    "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, VIEW_DEFINITION, CHECK_OPTION,
            IS_UPDATABLE, DEFINER, SECURITY_TYPE, CHARACTER_SET_CLIENT,
            COLLATION_CONNECTION
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'version';"

expect_output \
    "sys.version SHOW CREATE VIEW qualified row" \
    "$(printf '%b' "version\t${qualified_show_create}\tutf8mb4\tutf8mb4_0900_ai_ci")" \
    "SHOW CREATE VIEW sys.version;"

expect_output \
    "sys.version SHOW CREATE TABLE qualified row" \
    "$(printf '%b' "version\t${qualified_show_create}\tutf8mb4\tutf8mb4_0900_ai_ci")" \
    "SHOW CREATE TABLE sys.version;"

expect_output \
    "sys.version SHOW CREATE VIEW selected-schema row" \
    "$(printf '%b' "version\t${unqualified_show_create}\tutf8mb4\tutf8mb4_0900_ai_ci")" \
    "USE sys; SHOW CREATE VIEW version;"

expect_output \
    "sys.version SHOW CREATE TABLE selected-schema row" \
    "$(printf '%b' "version\t${unqualified_show_create}\tutf8mb4\tutf8mb4_0900_ai_ci")" \
    "USE sys; SHOW CREATE TABLE version;"

status=$(run_mysql "SHOW CREATE VIEW sys.version; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys.version SHOW CREATE status: expected [0	-1], got [$status]"
fi

expect_output \
    "sys.version empty view dependency metadata" \
    "$(printf '%b' '0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
          WHERE VIEW_SCHEMA = 'sys' AND VIEW_NAME = 'version'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'version');"

printf '%s\n' "mysql_baseline_sys_version_view_definition_expectations: ok"
