#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_replace_modifier_$$"

fail() {
    printf '%s\n' "mysql_baseline_replace_modifier_lifecycle_expectations: $1" >&2
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
     INSERT INTO src VALUES (1, 10), (2, NULL);
     REPLACE LOW_PRIORITY INTO dst VALUES (1, 10);
     SELECT 'low_values', ROW_COUNT(), @@warning_count, @@error_count;
     REPLACE LOW_PRIORITY INTO dst SET id = 2, n = 20;
     SELECT 'low_set', ROW_COUNT(), @@warning_count, @@error_count;
     REPLACE LOW_PRIORITY INTO dst(id, n) SELECT id, n FROM src ORDER BY id LIMIT 1;
     SELECT 'low_select', ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, n FROM dst ORDER BY id, n;" \
    >"/tmp/${DATABASE}_low.out"

expect_value \
    "low priority forms" \
    "low_values	1	0	0
low_set	1	0	0
low_select	1	0	0
1	10
1	10
2	20" \
    "$(cat "/tmp/${DATABASE}_low.out")"

run_mysql \
    "USE ${DATABASE};
     REPLACE DELAYED INTO dst VALUES (3, 30);
     SHOW WARNINGS;" \
    >"/tmp/${DATABASE}_delayed_warning.out"
expect_value \
    "delayed warning row" \
    "Warning	3005	REPLACE DELAYED is no longer supported. The statement was converted to REPLACE." \
    "$(cat "/tmp/${DATABASE}_delayed_warning.out")"

run_mysql \
    "USE ${DATABASE};
     REPLACE DELAYED INTO dst VALUES (4, 40);
     SELECT 'delayed_values', ROW_COUNT(), @@warning_count, @@error_count;
     REPLACE DELAYED INTO dst SET id = 5, n = 50;
     SELECT 'delayed_set', ROW_COUNT(), @@warning_count, @@error_count;
     REPLACE DELAYED INTO dst(id, n) SELECT id, n FROM src ORDER BY id DESC LIMIT 1;
     SELECT 'delayed_select', ROW_COUNT(), @@warning_count, @@error_count;" \
    >"/tmp/${DATABASE}_delayed_status.out"
expect_value \
    "delayed status rows" \
    "delayed_values	1	1	0
delayed_set	1	1	0
delayed_select	1	1	0" \
    "$(cat "/tmp/${DATABASE}_delayed_status.out")"

set +e
forced_output=$(
    run_mysql_force \
        "USE ${DATABASE};
         REPLACE DELAYED INTO dst(id, n) VALUES (NULL, 10);
         SHOW WARNINGS;" \
        2>&1
)
set -e
case "$forced_output" in
    *"ERROR 1048 (23000)"*"Column 'id' cannot be null"*) ;;
    *) fail "delayed client error diagnostics: got [$forced_output]" ;;
esac
case "$forced_output" in
    *"Warning	3005	REPLACE DELAYED is no longer supported. The statement was converted to REPLACE."*) ;;
    *) fail "delayed conversion warning diagnostics: got [$forced_output]" ;;
esac
case "$forced_output" in
    *"Error	1048	Column 'id' cannot be null"*) ;;
    *) fail "delayed server error diagnostics: got [$forced_output]" ;;
esac

expect_error \
    "mixed modifiers low delayed" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; REPLACE LOW_PRIORITY DELAYED INTO dst VALUES (6, 60);"

expect_error \
    "mixed modifiers delayed low" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; REPLACE DELAYED LOW_PRIORITY INTO dst VALUES (6, 60);"

expect_error \
    "unsupported high priority" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; REPLACE HIGH_PRIORITY INTO dst VALUES (6, 60);"

expect_error \
    "modifier after into" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; REPLACE INTO LOW_PRIORITY dst VALUES (6, 60);"

rm -f "/tmp/${DATABASE}_low.out" \
    "/tmp/${DATABASE}_delayed_warning.out" \
    "/tmp/${DATABASE}_delayed_status.out"
