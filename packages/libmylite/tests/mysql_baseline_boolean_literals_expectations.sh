#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_boolean_literals_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_boolean_literals_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "boolean literals evaluate to one and zero" \
    "1	1	0	0" \
    "SELECT TRUE, true, FALSE, false;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE flags ("\
"id INT NOT NULL, b BOOL NULL, c BOOLEAN NOT NULL, i INT NULL, u INT UNSIGNED NULL);" \
    "$DATABASE" >/dev/null

insert_values_expected=$(cat <<'EXPECTED'
2	0
1	1	0	1	0
2	0	1	0	1
EXPECTED
)
expect_output \
    "insert values accepts boolean literals" \
    "$insert_values_expected" \
    "INSERT INTO flags VALUES (1, TRUE, FALSE, TRUE, FALSE), "\
"(2, false, true, false, true); "\
"SELECT ROW_COUNT(), @@warning_count; SELECT id,b,c,i,u FROM flags ORDER BY id;" \
    "$DATABASE"

insert_set_expected=$(cat <<'EXPECTED'
1	0
1	1	0	1	0
2	0	1	0	1
3	1	0	0	1
EXPECTED
)
expect_output \
    "insert set accepts boolean literals" \
    "$insert_set_expected" \
    "INSERT INTO flags SET id = 3, b = TRUE, c = FALSE, i = false, u = true; "\
"SELECT ROW_COUNT(), @@warning_count; SELECT id,b,c,i,u FROM flags ORDER BY id;" \
    "$DATABASE"

update_true_expected=$(cat <<'EXPECTED'
2	0
1	1
2	1
3	1
EXPECTED
)
expect_output \
    "update assignment accepts true" \
    "$update_true_expected" \
    "UPDATE flags SET i = TRUE WHERE i = FALSE; "\
"SELECT ROW_COUNT(), @@warning_count; SELECT id,i FROM flags ORDER BY id;" \
    "$DATABASE"

update_false_expected=$(cat <<'EXPECTED'
1	0
1	0
2	0
3	0
EXPECTED
)
expect_output \
    "update assignment accepts false into not null" \
    "$update_false_expected" \
    "UPDATE flags SET c = FALSE WHERE b = FALSE; "\
"SELECT ROW_COUNT(), @@warning_count; SELECT id,c FROM flags ORDER BY id;" \
    "$DATABASE"

noop_expected=$(cat <<'EXPECTED'
0	0
1	1
2	0
3	1
EXPECTED
)
expect_output \
    "boolean no-op update reports changed rows" \
    "$noop_expected" \
    "UPDATE flags SET b = TRUE WHERE b = TRUE; "\
"SELECT ROW_COUNT(), @@warning_count; SELECT id,b FROM flags ORDER BY id;" \
    "$DATABASE"

predicate_expected=$(cat <<'EXPECTED'
2
1,3
1
1,2,3
1,2,3
EXPECTED
)
expect_output \
    "predicates compare boolean literals as integers" \
    "$predicate_expected" \
    "SELECT COUNT(*) FROM flags WHERE b = TRUE; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM flags WHERE b <=> TRUE; "\
"SELECT COUNT(*) FROM flags WHERE b <> TRUE; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM flags WHERE c <= FALSE; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM flags WHERE u >= FALSE;" \
    "$DATABASE"

delete_expected=$(cat <<'EXPECTED'
1	0
1,3
EXPECTED
)
expect_output \
    "delete predicate accepts false" \
    "$delete_expected" \
    "DELETE FROM flags WHERE b = FALSE; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM flags;" \
    "$DATABASE"

ordered_limit_expected=$(cat <<'EXPECTED'
1	0
1:1,3:0
EXPECTED
)
expect_output \
    "ordered limited update uses stored integer values" \
    "$ordered_limit_expected" \
    "UPDATE flags SET b = FALSE ORDER BY id DESC LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count; SELECT GROUP_CONCAT(CONCAT(id, ':', b) ORDER BY id) "\
"FROM flags;" \
    "$DATABASE"

expect_error \
    "limit true is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT id FROM flags LIMIT TRUE;" \
    "$DATABASE"

expect_output \
    "mysql accepts unary boolean expressions outside MyLite scope" \
    "1	-1	0	0" \
    "SELECT +TRUE, -TRUE, +FALSE, -FALSE;" \
    "$DATABASE"

printf '%s\n' "baseline-boolean-literals MySQL 8.4.9 expectations verified"
