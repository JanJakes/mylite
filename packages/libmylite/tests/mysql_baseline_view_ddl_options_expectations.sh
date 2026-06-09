#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_view_ddl_options_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_view_ddl_options_expectations: $1" >&2
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; "\
"CREATE TABLE t(id INT NOT NULL, name VARCHAR(20));" >/dev/null

run_mysql "USE ${DATABASE}; "\
"CREATE ALGORITHM=MERGE DEFINER='app'@'example.com' SQL SECURITY INVOKER "\
"VIEW v_invoker (view_id, label) "\
"AS SELECT id, name FROM t WITH LOCAL CHECK OPTION;" >/dev/null

show_create=$(run_mysql "USE ${DATABASE}; SHOW CREATE VIEW v_invoker;")
expect_value "create view options show create" \
    "v_invoker	CREATE ALGORITHM=MERGE DEFINER=\`app\`@\`example.com\` SQL SECURITY INVOKER VIEW \`v_invoker\` (\`view_id\`,\`label\`) AS select \`t\`.\`id\` AS \`id\`,\`t\`.\`name\` AS \`name\` from \`t\` WITH LOCAL CHECK OPTION	latin1	latin1_swedish_ci" \
    "$show_create"

views_row=$(run_mysql \
    "SELECT TABLE_NAME, VIEW_DEFINITION, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE, "\
"CHARACTER_SET_CLIENT, COLLATION_CONNECTION FROM INFORMATION_SCHEMA.VIEWS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'v_invoker';")
expect_value "create view options information schema" \
    "v_invoker	select \`${DATABASE}\`.\`t\`.\`id\` AS \`id\`,\`${DATABASE}\`.\`t\`.\`name\` AS \`name\` from \`${DATABASE}\`.\`t\`	LOCAL	YES	app@example.com	INVOKER	latin1	latin1_swedish_ci" \
    "$views_row"

run_mysql "USE ${DATABASE}; "\
"CREATE OR REPLACE ALGORITHM=TEMPTABLE VIEW v_invoker AS SELECT id FROM t;" >/dev/null
replace_row=$(run_mysql \
    "SELECT TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, SECURITY_TYPE FROM INFORMATION_SCHEMA.VIEWS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'v_invoker';")
expect_value "create or replace view metadata" "v_invoker	NONE	NO	DEFINER" "$replace_row"

run_mysql "USE ${DATABASE}; CREATE VIEW v_alter AS SELECT id FROM t;" >/dev/null
run_mysql "USE ${DATABASE}; "\
"ALTER ALGORITHM=MERGE SQL SECURITY INVOKER VIEW v_alter (x) AS SELECT id FROM t "\
"WITH CHECK OPTION;" >/dev/null
alter_show_create=$(run_mysql "USE ${DATABASE}; SHOW CREATE VIEW v_alter;")
expect_value "alter view show create" \
    "v_alter	CREATE ALGORITHM=MERGE DEFINER=\`root\`@\`%\` SQL SECURITY INVOKER VIEW \`v_alter\` (\`x\`) AS select \`t\`.\`id\` AS \`id\` from \`t\` WITH CASCADED CHECK OPTION	latin1	latin1_swedish_ci" \
    "$alter_show_create"
alter_row=$(run_mysql \
    "SELECT TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, SECURITY_TYPE FROM INFORMATION_SCHEMA.VIEWS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'v_alter';")
expect_value "alter view metadata" "v_alter	CASCADED	YES	INVOKER" "$alter_row"

warning=$(run_mysql "USE ${DATABASE}; DROP VIEW IF EXISTS v_missing RESTRICT; SHOW WARNINGS;")
expect_value "drop view restrict missing note" \
    "Note	1051	Unknown table '${DATABASE}.v_missing'" \
    "$warning"

run_mysql "USE ${DATABASE}; DROP VIEW IF EXISTS v_invoker CASCADE;" >/dev/null
replace_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME = 'v_invoker';")
expect_value "drop view cascade removed view" "0" "$replace_count"

printf '%s\n' "mysql_baseline_view_ddl_options_expectations: ok"
