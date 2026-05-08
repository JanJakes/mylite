#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_show_triggers_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"

fail() {
    printf '%s\n' "mysql_baseline_show_triggers_empty_introspection_expectations: $1" >&2
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

field_from_rows() {
    rows=$1
    row_key=$2
    field_index=$3

    printf '%s\n' "$rows" | awk -F '\t' -v key="$row_key" -v field="$field_index" '$1 == key { print $field; exit }'
}

expect_trigger_field() {
    label=$1
    rows=$2
    trigger_name=$3
    field_index=$4
    expected=$5

    actual=$(field_from_rows "$rows" "$trigger_name" "$field_index")
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
    *) fail "expected @@lower_case_table_names=0 for case-sensitive SHOW TRIGGERS LIKE probes" ;;
esac

cleanup

run_mysql \
    "CREATE DATABASE ${DATABASE};
     CREATE DATABASE ${OTHER_DATABASE};
     CREATE TABLE ${DATABASE}.account(amount INT) ENGINE=InnoDB;
     CREATE TABLE ${DATABASE}.account_mixed(amount INT) ENGINE=InnoDB;
     CREATE TABLE ${OTHER_DATABASE}.no_triggers(id INT) ENGINE=InnoDB;
     CREATE TRIGGER ${DATABASE}.ins_sum
       BEFORE INSERT ON ${DATABASE}.account
       FOR EACH ROW SET @sum = COALESCE(@sum, 0) + NEW.amount;
     CREATE TRIGGER ${DATABASE}.ins_mixed
       BEFORE INSERT ON ${DATABASE}.account_mixed
       FOR EACH ROW SET @sum = COALESCE(@sum, 0) + NEW.amount;" \
    >/dev/null

expected_headers="Trigger	Event	Table	Statement	Timing	Created	sql_mode	Definer	character_set_client	collation_connection	Database Collation"

show_output=$(run_mysql_with_headers "SHOW TRIGGERS FROM ${DATABASE};")
headers=$(printf '%s\n' "$show_output" | sed -n '1p')
rows=$(printf '%s\n' "$show_output" | sed '1d')
expect_value "show triggers headers" "$expected_headers" "$headers"
expect_trigger_field "trigger event" "$rows" "ins_sum" 2 "INSERT"
expect_trigger_field "trigger table" "$rows" "ins_sum" 3 "account"
expect_trigger_field "trigger statement" "$rows" "ins_sum" 4 "SET @sum = COALESCE(@sum, 0) + NEW.amount"
expect_trigger_field "trigger timing" "$rows" "ins_sum" 5 "BEFORE"
expect_trigger_field "trigger charset" "$rows" "ins_sum" 9 "latin1"
expect_trigger_field "trigger collation" "$rows" "ins_sum" 10 "latin1_swedish_ci"
expect_trigger_field "trigger database collation" "$rows" "ins_sum" 11 "utf8mb4_0900_ai_ci"

expect_empty_show_triggers() {
    label=$1
    sql=$2
    shift 2

    output=$(run_mysql "$sql" "$@")
    expect_value "$label rows" "" "$output"
}

expect_empty_show_triggers "empty explicit from" "SHOW TRIGGERS FROM ${OTHER_DATABASE};"
status=$(run_mysql "SHOW TRIGGERS FROM ${OTHER_DATABASE}; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "empty explicit from status" "0	-1" "$status"

expect_empty_show_triggers "empty explicit in" "SHOW TRIGGERS IN ${OTHER_DATABASE};"
expect_empty_show_triggers "empty selected" "USE ${OTHER_DATABASE}; SHOW TRIGGERS;"
expect_empty_show_triggers "empty full selected" "USE ${OTHER_DATABASE}; SHOW FULL TRIGGERS;"
expect_empty_show_triggers "empty like no match" "SHOW TRIGGERS FROM ${OTHER_DATABASE} LIKE 'missing%';"

status=$(run_mysql "SHOW TRIGGERS FROM ${DATABASE}; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "show triggers status" "0	-1" "$status"

full_output=$(run_mysql_with_headers "SHOW FULL TRIGGERS FROM ${DATABASE} LIKE 'account';")
expect_value "show full triggers headers" "$expected_headers" "$(printf '%s\n' "$full_output" | sed -n '1p')"
expect_value "show full triggers row count" "2" "$(printf '%s\n' "$full_output" | wc -l | tr -d ' ')"

like_table=$(run_mysql "SHOW TRIGGERS FROM ${DATABASE} LIKE 'account';")
expect_trigger_field "like matches table name" "$like_table" "ins_sum" 3 "account"

like_trigger_name=$(run_mysql "SHOW TRIGGERS FROM ${DATABASE} LIKE 'ins%';")
expect_value "like does not match trigger name" "" "$like_trigger_name"

like_uppercase=$(run_mysql "SHOW TRIGGERS FROM ${DATABASE} LIKE 'ACCOUNT';")
expect_value "like table matching is case-sensitive" "" "$like_uppercase"

like_escaped=$(run_mysql "SHOW TRIGGERS FROM ${DATABASE} LIKE 'account\\_%';")
expect_trigger_field "like escaped underscore" "$like_escaped" "ins_mixed" 3 "account_mixed"

where_output=$(run_mysql "SHOW TRIGGERS FROM ${DATABASE} WHERE \`Table\` = 'account';")
expect_trigger_field "where accepted upstream" "$where_output" "ins_sum" 3 "account"

expect_error \
    "missing default schema show triggers" \
    1046 \
    3D000 \
    "No database selected" \
    "SHOW TRIGGERS;"

expect_error \
    "unknown schema show triggers from" \
    1049 \
    42000 \
    "Unknown database 'missing_show_triggers_schema'" \
    "SHOW TRIGGERS FROM missing_show_triggers_schema;"

expect_error \
    "unknown schema show triggers in" \
    1049 \
    42000 \
    "Unknown database 'missing_show_triggers_schema'" \
    "SHOW TRIGGERS IN missing_show_triggers_schema;"

expect_error \
    "unsupported extended show triggers" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW EXTENDED TRIGGERS FROM ${DATABASE};"

expect_error \
    "unsupported singular show trigger" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW TRIGGER FROM ${DATABASE};"

expect_error \
    "unsupported numeric like show triggers" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW TRIGGERS FROM ${DATABASE} LIKE 1;"

expect_error \
    "unsupported null like show triggers" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW TRIGGERS FROM ${DATABASE} LIKE NULL;"

expect_error \
    "unsupported national like show triggers" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW TRIGGERS FROM ${DATABASE} LIKE N'account';"

expect_error \
    "unsupported introducer like show triggers" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW TRIGGERS FROM ${DATABASE} LIKE _utf8mb4'account';"

expect_error \
    "unsupported combined like where show triggers" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW TRIGGERS FROM ${DATABASE} LIKE 'account' WHERE \`Table\` = 'account';"

printf '%s\n' "baseline-show-triggers-empty-introspection MySQL 8.4.9 expectations verified"
