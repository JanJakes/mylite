#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_table_algorithm_lock_clauses_$$"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_algorithm_lock_clauses_expectations: $1" >&2
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

expect_upstream_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept deferred behavior, got [$output]"
    fi
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

option_success_expected=$(cat <<\EXPECTED
0	0
0	0
0	0
k_renamed
0	0
2:200
2	0
2	0
0	0
EXPECTED
)
expect_output \
    "supported alter algorithm and lock option tails" \
    "$option_success_expected" \
    "CREATE TABLE add_col (id INT PRIMARY KEY); "\
"ALTER TABLE add_col ADD COLUMN v INT, ALGORITHM=INSTANT, LOCK=DEFAULT; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"CREATE TABLE idx (id INT PRIMARY KEY, a INT, b INT, KEY k_b (b)); "\
"ALTER TABLE idx DROP INDEX k_b, ALGORITHM=INPLACE, LOCK=NONE; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE idx ADD INDEX k_a (a), ALGORITHM=INPLACE, LOCK=NONE; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE idx RENAME INDEX k_a TO k_renamed, LOCK=NONE; "\
"SELECT INDEX_NAME FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'idx' AND INDEX_NAME <> 'PRIMARY'; "\
"CREATE TABLE p (id INT PRIMARY KEY); "\
"CREATE TABLE c (id INT PRIMARY KEY, p_id INT, KEY k_p (p_id), "\
"CONSTRAINT fk_c_p FOREIGN KEY (p_id) REFERENCES p(id)); "\
"ALTER TABLE c DROP FOREIGN KEY fk_c_p, ALGORITHM=INPLACE, LOCK=NONE; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"INSERT INTO c VALUES (2, 200); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', p_id) ORDER BY id) FROM c WHERE id = 2; "\
"CREATE TABLE force_t (id INT, v INT); INSERT INTO force_t VALUES (1, 10), (2, 20); "\
"ALTER TABLE force_t FORCE, ALGORITHM=COPY; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"CREATE TABLE pk_t (id INT PRIMARY KEY, v INT); INSERT INTO pk_t VALUES (1, 10), (2, 20); "\
"ALTER TABLE pk_t DROP PRIMARY KEY, ALGORITHM=COPY, LOCK=EXCLUSIVE; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE force_t DROP COLUMN v, ALGORITHM=DEFAULT, LOCK=NONE; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "instant algorithm conflicts with non-default lock" \
    1221 \
    HY000 \
    "Incorrect usage of ALGORITHM=INSTANT and LOCK=NONE/SHARED/EXCLUSIVE" \
    "CREATE TABLE bad_lock (id INT, v INT); "\
"ALTER TABLE bad_lock DROP COLUMN v, ALGORITHM=INSTANT, LOCK=NONE;" \
    "$DATABASE"

expect_error \
    "instant algorithm rejected for drop index" \
    1845 \
    0A000 \
    "ALGORITHM=INSTANT is not supported for this operation" \
    "CREATE TABLE bad_algorithm (id INT PRIMARY KEY, v INT, KEY k_v (v)); "\
"ALTER TABLE bad_algorithm DROP INDEX k_v, ALGORITHM=INSTANT;" \
    "$DATABASE"

expect_error \
    "missing comma before lock is syntax error" \
    1064 \
    42000 \
    "You have an error" \
    "CREATE TABLE bad_comma (id INT); ALTER TABLE bad_comma FORCE LOCK=NONE;" \
    "$DATABASE"

expect_upstream_accepts \
    "standalone option-only alter remains deferred by MyLite" \
    "CREATE TABLE standalone_options (id INT); ALTER TABLE standalone_options ALGORITHM=INSTANT;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_alter_table_algorithm_lock_clauses_expectations: ok"
