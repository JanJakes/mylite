#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_show_engine_status_expectations: $1" >&2
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

expect_success() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected success, got status $status_code with [$output]"
    fi
}

expect_innodb_status_result() {
    label=$1
    sql=$2

    output=$(run_mysql_with_headers "$sql")
    headers=$(printf '%s\n' "$output" | sed -n '1p')
    type=$(printf '%s\n' "$output" | sed -n '2p' | cut -f 1)
    name=$(printf '%s\n' "$output" | sed -n '2p' | cut -f 2)
    status=$(printf '%s\n' "$output" | sed -n '2,$p' | cut -f 3-)

    expect_value "$label headers" "Type	Name	Status" "$headers"
    expect_value "$label type" "InnoDB" "$type"
    expect_value "$label name" "" "$name"

    case "$status" in
        *"INNODB MONITOR OUTPUT"*) ;;
        *) fail "$label status: expected live InnoDB monitor text, got [$status]" ;;
    esac
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_innodb_status_result "show engine innodb status" "SHOW ENGINE InnoDB STATUS;"
expect_innodb_status_result "show engine lower status" "SHOW ENGINE innodb STATUS;"
expect_innodb_status_result "show engine quoted identifier status" "SHOW ENGINE \`InnoDB\` STATUS;"
expect_innodb_status_result "show engine string status" "SHOW ENGINE 'InnoDB' STATUS;"

status=$(run_mysql "SHOW ENGINE InnoDB STATUS; SELECT ROW_COUNT(), @@warning_count, @@error_count;" | tail -n 1)
expect_value "show engine innodb status diagnostics" "-1	0	0" "$status"

expect_error \
    "unknown ndb engine" \
    1286 \
    42000 \
    "Unknown storage engine 'NDB'" \
    "SHOW ENGINE NDB STATUS;"

expect_error \
    "show engine status like syntax" \
    1064 \
    42000 \
    "near 'LIKE '%''" \
    "SHOW ENGINE InnoDB STATUS LIKE '%';"

expect_error \
    "show full engine status syntax" \
    1064 \
    42000 \
    "near 'ENGINE InnoDB STATUS'" \
    "SHOW FULL ENGINE InnoDB STATUS;"

expect_success "show engine myisam status accepted by reference runtime" "SHOW ENGINE MyISAM STATUS;"
expect_success "show engine logs accepted by reference runtime" "SHOW ENGINE InnoDB LOGS;"

mutex_output=$(run_mysql_with_headers "SHOW ENGINE InnoDB MUTEX;")
expect_value "show engine mutex headers" "Type	Name	Status" "$(printf '%s\n' "$mutex_output" | sed -n '1p')"
