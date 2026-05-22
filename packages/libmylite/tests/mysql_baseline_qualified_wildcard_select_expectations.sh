#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_qualified_wildcard_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_qualified_wildcard_select_expectations: $1" >&2
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

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_contains() {
    label=$1
    haystack=$2
    needle=$3

    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$label: expected output containing [$needle], got [$haystack]" ;;
    esac
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
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE};
     CREATE TABLE t(id INT NOT NULL, n INT NULL, hidden INT INVISIBLE);
     CREATE TABLE u(id INT NOT NULL, t_id INT NOT NULL, v INT NULL);
     INSERT INTO t(id,n,hidden) VALUES (1,10,100),(2,NULL,200);
     INSERT INTO u VALUES (7,1,70),(8,2,80);" >/dev/null

table_wildcard=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT t.* FROM t ORDER BY t.id;"
)
expect_value "table wildcard labels" "id	n" "$(printf '%s\n' "$table_wildcard" | sed -n '1p')"
expect_value "table wildcard row 1" "1	10" "$(printf '%s\n' "$table_wildcard" | sed -n '2p')"
expect_value "table wildcard row 2" "2	NULL" "$(printf '%s\n' "$table_wildcard" | sed -n '3p')"

schema_wildcard=$(run_mysql \
    "USE ${DATABASE};
     SELECT ${DATABASE}.t.* FROM ${DATABASE}.t ORDER BY ${DATABASE}.t.id;"
)
expect_value "schema wildcard row 1" "1	10" "$(printf '%s\n' "$schema_wildcard" | sed -n '1p')"
expect_value "schema wildcard row 2" "2	NULL" "$(printf '%s\n' "$schema_wildcard" | sed -n '2p')"

alias_wildcard=$(run_mysql \
    "USE ${DATABASE};
     SELECT a.* FROM t AS a ORDER BY a.id;"
)
expect_value "alias wildcard row 1" "1	10" "$(printf '%s\n' "$alias_wildcard" | sed -n '1p')"
expect_value "alias wildcard row 2" "2	NULL" "$(printf '%s\n' "$alias_wildcard" | sed -n '2p')"

mixed_wildcard=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT id, t.* FROM t ORDER BY id;
     SELECT t.*, t.id FROM t ORDER BY t.id;"
)
expect_value "mixed leading column labels" "id	id	n" "$(printf '%s\n' "$mixed_wildcard" | sed -n '1p')"
expect_value "mixed leading column row 1" "1	1	10" "$(printf '%s\n' "$mixed_wildcard" | sed -n '2p')"
expect_value "mixed trailing column labels" "id	n	id" "$(printf '%s\n' "$mixed_wildcard" | sed -n '4p')"
expect_value "mixed trailing column row 2" "2	NULL	2" "$(printf '%s\n' "$mixed_wildcard" | sed -n '6p')"

joined_wildcard=$(run_mysql \
    "USE ${DATABASE};
     SELECT a.*, u.v FROM t AS a JOIN u ON a.id = u.t_id ORDER BY u.id;
     SELECT a.*, b.* FROM t AS a, u AS b WHERE a.id = b.t_id ORDER BY b.id;
     SELECT a.*, b.id FROM t AS a LEFT JOIN u AS b ON a.id = b.t_id ORDER BY a.id;
     SELECT @@warning_count, ROW_COUNT();"
)
expect_value "joined wildcard row 1" "1	10	70" "$(printf '%s\n' "$joined_wildcard" | sed -n '1p')"
expect_value "joined wildcard row 2" "2	NULL	80" "$(printf '%s\n' "$joined_wildcard" | sed -n '2p')"
expect_value "comma wildcard row 1" "1	10	7	1	70" "$(printf '%s\n' "$joined_wildcard" | sed -n '3p')"
expect_value "comma wildcard row 2" "2	NULL	8	2	80" "$(printf '%s\n' "$joined_wildcard" | sed -n '4p')"
expect_value "left wildcard row 1" "1	10	7" "$(printf '%s\n' "$joined_wildcard" | sed -n '5p')"
expect_value "left wildcard row 2" "2	NULL	8" "$(printf '%s\n' "$joined_wildcard" | sed -n '6p')"
expect_value "joined wildcard warnings row count" "0	-1" "$(printf '%s\n' "$joined_wildcard" | sed -n '7p')"

expect_error \
    "table qualifier hidden by alias" \
    1051 \
    "42S02" \
    "Unknown table 't'" \
    "USE ${DATABASE}; SELECT t.* FROM t AS a;"

expect_error \
    "schema qualifier hidden by alias" \
    1051 \
    "42S02" \
    "Unknown table '${DATABASE}.t'" \
    "USE ${DATABASE}; SELECT ${DATABASE}.t.* FROM t AS a;"

expect_error \
    "missing qualifier source" \
    1051 \
    "42S02" \
    "Unknown table 'missing'" \
    "USE ${DATABASE}; SELECT missing.* FROM t;"

expect_error \
    "qualified wildcard alias syntax" \
    1064 \
    "42000" \
    "near 'AS x FROM t'" \
    "USE ${DATABASE}; SELECT t.* AS x FROM t;"
