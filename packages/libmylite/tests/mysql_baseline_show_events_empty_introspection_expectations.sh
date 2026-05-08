#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_show_events_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"

fail() {
    printf '%s\n' "mysql_baseline_show_events_empty_introspection_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw "$@"
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

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

field_from_event_rows() {
    rows=$1
    event_name=$2
    field_index=$3

    printf '%s\n' "$rows" | awk -F '\t' -v event="$event_name" -v field="$field_index" '$2 == event { print $field; exit }'
}

expect_event_field() {
    label=$1
    rows=$2
    event_name=$3
    field_index=$4
    expected=$5

    actual=$(field_from_event_rows "$rows" "$event_name" "$field_index")
    expect_value "$label" "$expected" "$actual"
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE}; DROP DATABASE IF EXISTS ${OTHER_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

case "$(run_mysql 'SELECT @@lower_case_table_names;')" in
    0) ;;
    *) fail "expected @@lower_case_table_names=0 for SHOW EVENTS LIKE probes" ;;
esac

cleanup

run_mysql \
    "CREATE DATABASE ${DATABASE};
     CREATE DATABASE ${OTHER_DATABASE};
     CREATE EVENT ${DATABASE}.daily_event
       ON SCHEDULE EVERY 1 DAY
       DO SET @event_probe = 1;
     CREATE EVENT ${DATABASE}.event_mixed_name
       ON SCHEDULE EVERY 1 DAY
       DO SET @event_probe = 2;
     CREATE EVENT ${DATABASE}.MixedCaseEvent
       ON SCHEDULE EVERY 1 DAY
       DO SET @event_probe = 3;" \
    >/dev/null

expected_headers="Db	Name	Definer	Time zone	Type	Execute at	Interval value	Interval field	Starts	Ends	Status	Originator	character_set_client	collation_connection	Database Collation"

show_output=$(run_mysql_with_headers "SHOW EVENTS FROM ${DATABASE};")
headers=$(printf '%s\n' "$show_output" | sed -n '1p')
rows=$(printf '%s\n' "$show_output" | sed '1d')
expect_value "show events headers" "$expected_headers" "$headers"
expect_event_field "event database" "$rows" "daily_event" 1 "$DATABASE"
expect_event_field "event definer" "$rows" "daily_event" 3 "root@%"
expect_event_field "event time zone" "$rows" "daily_event" 4 "SYSTEM"
expect_event_field "event type" "$rows" "daily_event" 5 "RECURRING"
expect_event_field "event execute at" "$rows" "daily_event" 6 "NULL"
expect_event_field "event interval value" "$rows" "daily_event" 7 "1"
expect_event_field "event interval field" "$rows" "daily_event" 8 "DAY"
expect_event_field "event ends" "$rows" "daily_event" 10 "NULL"
expect_event_field "event status" "$rows" "daily_event" 11 "ENABLED"
expect_event_field "event charset" "$rows" "daily_event" 13 "latin1"
expect_event_field "event collation" "$rows" "daily_event" 14 "latin1_swedish_ci"
expect_event_field "event database collation" "$rows" "daily_event" 15 "utf8mb4_0900_ai_ci"

expect_empty_show_events() {
    label=$1
    sql=$2
    shift 2

    output=$(run_mysql "$sql" "$@")
    expect_value "$label rows" "" "$output"
}

expect_empty_show_events "empty explicit from" "SHOW EVENTS FROM ${OTHER_DATABASE};"
status=$(run_mysql "SHOW EVENTS FROM ${OTHER_DATABASE}; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "empty explicit from status" "0	-1" "$status"

expect_empty_show_events "empty explicit in" "SHOW EVENTS IN ${OTHER_DATABASE};"
expect_empty_show_events "empty selected" "USE ${OTHER_DATABASE}; SHOW EVENTS;"
expect_empty_show_events "empty like no match" "SHOW EVENTS FROM ${OTHER_DATABASE} LIKE 'missing%';"

expect_empty_show_events "unknown explicit from" "SHOW EVENTS FROM missing_show_events_schema;"
status=$(run_mysql "SHOW EVENTS FROM missing_show_events_schema; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "unknown explicit from status" "0	-1" "$status"

expect_empty_show_events "unknown explicit in" "SHOW EVENTS IN missing_show_events_schema;"
expect_empty_show_events \
    "unknown explicit like" \
    "SHOW EVENTS FROM missing_show_events_schema LIKE 'missing%';"

status=$(run_mysql "SHOW EVENTS FROM ${DATABASE}; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "show events status" "0	-1" "$status"

like_event=$(run_mysql "SHOW EVENTS FROM ${DATABASE} LIKE 'daily%';")
expect_event_field "like matches event name" "$like_event" "daily_event" 2 "daily_event"

like_not_table=$(run_mysql "SHOW EVENTS FROM ${DATABASE} LIKE 'account%';")
expect_value "like does not match table names" "" "$like_not_table"

like_uppercase=$(run_mysql "SHOW EVENTS FROM ${DATABASE} LIKE 'MIXEDCASEEVENT';")
expect_event_field "like event matching is case-insensitive" "$like_uppercase" "MixedCaseEvent" 2 "MixedCaseEvent"

like_escaped=$(run_mysql "SHOW EVENTS FROM ${DATABASE} LIKE 'event\\_%';")
expect_event_field "like escaped underscore" "$like_escaped" "event_mixed_name" 2 "event_mixed_name"

where_output=$(run_mysql "SHOW EVENTS FROM ${DATABASE} WHERE Name = 'daily_event';")
expect_event_field "where accepted upstream" "$where_output" "daily_event" 2 "daily_event"

expect_error \
    "missing default schema show events" \
    1046 \
    3D000 \
    "No database selected" \
    "SHOW EVENTS;"

expect_error \
    "missing default schema show events like" \
    1046 \
    3D000 \
    "No database selected" \
    "SHOW EVENTS LIKE 'daily%';"

expect_error \
    "unsupported full show events" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW FULL EVENTS FROM ${DATABASE};"

expect_error \
    "unsupported extended show events" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW EXTENDED EVENTS FROM ${DATABASE};"

expect_error \
    "unsupported singular show event" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW EVENT FROM ${DATABASE};"

expect_error \
    "unsupported numeric like show events" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW EVENTS FROM ${DATABASE} LIKE 1;"

expect_error \
    "unsupported null like show events" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW EVENTS FROM ${DATABASE} LIKE NULL;"

expect_error \
    "unsupported national like show events" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW EVENTS FROM ${DATABASE} LIKE N'daily%';"

expect_error \
    "unsupported introducer like show events" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW EVENTS FROM ${DATABASE} LIKE _utf8mb4'daily%';"

expect_error \
    "unsupported combined like where show events" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW EVENTS FROM ${DATABASE} LIKE 'daily%' WHERE Name = 'daily_event';"

printf '%s\n' "baseline-show-events-empty-introspection MySQL 8.4.9 expectations verified"
