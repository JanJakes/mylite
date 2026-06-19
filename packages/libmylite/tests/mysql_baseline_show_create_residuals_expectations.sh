#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_show_create_residuals_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

show_create_user_header=$(run_mysql_with_headers "SHOW CREATE USER CURRENT_USER();" | head -n 1)
expect_value "show create user header" "CREATE USER for root@%" "$show_create_user_header"

show_create_user_value=$(run_mysql "SHOW CREATE USER CURRENT_USER();")
expect_value \
    "show create user value" \
    "CREATE USER \`root\`@\`%\` IDENTIFIED WITH 'caching_sha2_password' REQUIRE NONE PASSWORD EXPIRE DEFAULT ACCOUNT UNLOCK PASSWORD HISTORY DEFAULT PASSWORD REUSE INTERVAL DEFAULT PASSWORD REQUIRE CURRENT DEFAULT" \
    "$show_create_user_value"

show_create_root_value=$(run_mysql "SHOW CREATE USER root;")
expect_value "show create user omitted host" "$show_create_user_value" "$show_create_root_value"

show_create_root_host_value=$(run_mysql "SHOW CREATE USER root@'%';")
expect_value "show create user explicit host" "$show_create_user_value" "$show_create_root_host_value"

expect_error \
    "show create user unknown account" \
    1396 \
    HY000 \
    "Operation SHOW CREATE USER failed for 'no_such_mylite_user'@'%'" \
    "SHOW CREATE USER 'no_such_mylite_user'@'%';"
expect_error \
    "show create user empty host" \
    1396 \
    HY000 \
    "Operation SHOW CREATE USER failed for 'root'@''" \
    "SHOW CREATE USER root@;"

expect_error \
    "show create function no selected database" \
    1046 \
    3D000 \
    "No database selected" \
    "SHOW CREATE FUNCTION no_such_function;"
expect_error \
    "show create event no selected database" \
    1046 \
    3D000 \
    "No database selected" \
    "SHOW CREATE EVENT no_such_event;"
expect_error \
    "show create trigger no selected database" \
    1046 \
    3D000 \
    "No database selected" \
    "SHOW CREATE TRIGGER no_such_trigger;"

tmpdb="mylite_show_create_residuals_$$"
run_mysql "DROP DATABASE IF EXISTS $tmpdb; CREATE DATABASE $tmpdb; USE $tmpdb; SELECT 1;" >/dev/null
trap 'run_mysql "DROP DATABASE IF EXISTS '"$tmpdb"';" >/dev/null 2>&1 || true' EXIT HUP INT TERM

expect_error \
    "show create function missing" \
    1305 \
    42000 \
    "FUNCTION no_such_function does not exist" \
    "USE $tmpdb; SHOW CREATE FUNCTION no_such_function;"
expect_error \
    "show create function missing schema ignored" \
    1305 \
    42000 \
    "FUNCTION no_such_function does not exist" \
    "USE $tmpdb; SHOW CREATE FUNCTION no_such_schema.no_such_function;"
expect_error \
    "show create event missing" \
    1539 \
    HY000 \
    "Unknown event 'no_such_event'" \
    "USE $tmpdb; SHOW CREATE EVENT no_such_event;"
expect_error \
    "show create event missing schema ignored" \
    1539 \
    HY000 \
    "Unknown event 'no_such_event'" \
    "USE $tmpdb; SHOW CREATE EVENT no_such_schema.no_such_event;"
expect_error \
    "show create trigger missing" \
    1360 \
    HY000 \
    "Trigger does not exist" \
    "USE $tmpdb; SHOW CREATE TRIGGER no_such_trigger;"
expect_error \
    "show create trigger missing schema" \
    1049 \
    42000 \
    "Unknown database 'no_such_schema'" \
    "USE $tmpdb; SHOW CREATE TRIGGER no_such_schema.no_such_trigger;"

show_create_function_columns=$(run_mysql_with_headers \
    "USE $tmpdb; CREATE FUNCTION f_show_create() RETURNS INT DETERMINISTIC RETURN 1; SHOW CREATE FUNCTION f_show_create;" \
    | head -n 1)
expect_value \
    "show create function columns" \
    "Function	sql_mode	Create Function	character_set_client	collation_connection	Database Collation" \
    "$show_create_function_columns"

show_create_trigger_columns=$(run_mysql_with_headers \
    "USE $tmpdb; CREATE TABLE trig_t (a INT); CREATE TRIGGER trig_bi BEFORE INSERT ON trig_t FOR EACH ROW SET NEW.a = NEW.a; SHOW CREATE TRIGGER trig_bi;" \
    | head -n 1)
expect_value \
    "show create trigger columns" \
    "Trigger	sql_mode	SQL Original Statement	character_set_client	collation_connection	Database Collation	Created" \
    "$show_create_trigger_columns"

show_create_event_columns=$(run_mysql_with_headers \
    "USE $tmpdb; CREATE EVENT ev_show_create ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 HOUR DO SELECT 1; SHOW CREATE EVENT ev_show_create;" \
    | head -n 1)
expect_value \
    "show create event columns" \
    "Event	sql_mode	time_zone	Create Event	character_set_client	collation_connection	Database Collation" \
    "$show_create_event_columns"

run_mysql "DROP DATABASE IF EXISTS $tmpdb;" >/dev/null
trap - EXIT HUP INT TERM

printf '%s\n' "mysql_baseline_show_create_residuals_expectations: ok"
