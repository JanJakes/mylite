#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_insert_modifier_$$"

fail() {
    printf '%s\n' "mysql_baseline_insert_modifier_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_force() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names --force "$@"
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

run_mysql \
    "CREATE DATABASE ${DATABASE};
     USE ${DATABASE};
     CREATE TABLE src(id INT NOT NULL, n INT NULL) ENGINE=InnoDB;
     CREATE TABLE dst(id INT NOT NULL, n INT NULL) ENGINE=InnoDB;
     INSERT INTO src VALUES (10, 100), (20, NULL);
     INSERT LOW_PRIORITY INTO dst VALUES (1, 10);
     SELECT 'low_values', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT HIGH_PRIORITY INTO dst VALUES (2, 20);
     SELECT 'high_values', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT LOW_PRIORITY dst VALUES (3, 30);
     SELECT 'low_values_no_into', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT dst VALUES (4, 40);
     SELECT 'values_no_into', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT LOW_PRIORITY INTO dst SET id = 5, n = 50;
     SELECT 'low_set', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT HIGH_PRIORITY dst SET id = 6, n = 60;
     SELECT 'high_set_no_into', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT LOW_PRIORITY INTO dst(id, n) SELECT id, n FROM src ORDER BY id LIMIT 1;
     SELECT 'low_select', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT HIGH_PRIORITY dst(id, n) SELECT id, n FROM src ORDER BY id DESC LIMIT 1;
     SELECT 'high_select_no_into', ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, n FROM dst ORDER BY id, n;" \
    >"/tmp/${DATABASE}_priority.out"

expect_value \
    "priority forms" \
    "low_values	1	0	0
high_values	1	0	0
low_values_no_into	1	0	0
values_no_into	1	0	0
low_set	1	0	0
high_set_no_into	1	0	0
low_select	1	0	0
high_select_no_into	1	0	0
1	10
2	20
3	30
4	40
5	50
6	60
10	100
20	NULL" \
    "$(cat "/tmp/${DATABASE}_priority.out")"

run_mysql \
    "USE ${DATABASE};
     INSERT DELAYED INTO dst VALUES (100, 100);
     SELECT 'delayed_values', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT DELAYED dst SET id = 101, n = 101;
     SELECT 'delayed_set_no_into', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT DELAYED INTO dst(id, n) SELECT id, n FROM src ORDER BY id;
     SELECT 'delayed_select', ROW_COUNT(), @@warning_count, @@error_count;" \
    >"/tmp/${DATABASE}_delayed_status.out"

expect_value \
    "delayed status rows" \
    "delayed_values	1	1	0
delayed_set_no_into	1	1	0
delayed_select	2	1	0" \
    "$(cat "/tmp/${DATABASE}_delayed_status.out")"

run_mysql \
    "USE ${DATABASE};
     INSERT DELAYED INTO dst VALUES (102, 102);
     SHOW COUNT(*) WARNINGS;
     SHOW WARNINGS;" \
    >"/tmp/${DATABASE}_delayed_warning.out"

expect_value \
    "delayed warning row" \
    "1
Warning	3005	INSERT DELAYED is no longer supported. The statement was converted to INSERT." \
    "$(cat "/tmp/${DATABASE}_delayed_warning.out")"

set +e
forced_output=$(
    run_mysql_force \
        "USE ${DATABASE};
         INSERT DELAYED INTO dst VALUES (NULL, 10);
         SHOW WARNINGS;" \
        2>&1
)
set -e
case "$forced_output" in
    *"ERROR 1048 (23000)"*"Column 'id' cannot be null"*\
*"Warning	3005	INSERT DELAYED is no longer supported. The statement was converted to INSERT."*\
*"Error	1048	Column 'id' cannot be null"*) ;;
    *) fail "delayed warning plus error diagnostics: got [$forced_output]" ;;
esac

expect_error \
    "mixed modifiers low high" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; INSERT LOW_PRIORITY HIGH_PRIORITY INTO dst VALUES (7, 70);"

expect_error \
    "mixed modifiers high low" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; INSERT HIGH_PRIORITY LOW_PRIORITY INTO dst VALUES (7, 70);"

expect_error \
    "mixed modifiers low delayed" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; INSERT LOW_PRIORITY DELAYED INTO dst VALUES (7, 70);"

expect_error \
    "modifier after into" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; INSERT INTO LOW_PRIORITY dst VALUES (7, 70);"

expect_error \
    "ignore before priority" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; INSERT IGNORE LOW_PRIORITY INTO dst VALUES (7, 70);"

low_ignore_status=$(
    run_mysql \
        "USE ${DATABASE};
         INSERT LOW_PRIORITY IGNORE INTO dst VALUES (7, 70);
         SELECT ROW_COUNT(), @@warning_count, @@error_count;"
)
expect_value \
    "mysql accepts priority before ignore outside this slice" \
    "1	0	0" \
    "$(printf '%s\n' "$low_ignore_status" | tail -n 1)"

rm -f "/tmp/${DATABASE}_priority.out" \
    "/tmp/${DATABASE}_delayed_status.out" \
    "/tmp/${DATABASE}_delayed_warning.out"
