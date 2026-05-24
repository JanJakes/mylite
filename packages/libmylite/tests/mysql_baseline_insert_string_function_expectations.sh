#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_insert_string_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_insert_string_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --default-character-set=utf8mb4 "$@"
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

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
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
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_0900_ai_ci;" >/dev/null

literal_expected=$(cat <<EXPECTED
QuWhattic	Quadratic	Quadratic	Quadratic	QuWhat	QuWhatadratic	QuWhat		
NULL	NULL	NULL	NULL
éXb	éX	C3A95862
1945	0
-1	0
EXPECTED
)
expect_output \
    "literal INSERT string values" \
    "$literal_expected" \
    "SELECT INSERT('Quadratic', 3, 4, 'What'), INSERT('Quadratic', -1, 4, 'What'), "\
"INSERT('Quadratic', 0, 4, 'What'), INSERT('Quadratic', 99, 4, 'What'), "\
"INSERT('Quadratic', 3, 100, 'What'), INSERT('Quadratic', 3, 0, 'What'), "\
"INSERT('Quadratic', 3, -1, 'What'), INSERT('', 1, 1, 'x'), INSERT('', 0, 1, 'x'); "\
"SELECT INSERT(NULL,1,1,'x'), INSERT('abc',NULL,1,'x'), "\
"INSERT('abc',1,NULL,'x'), INSERT('abc',1,1,NULL); "\
"SELECT INSERT('é🙂b',2,1,'X'), INSERT('é🙂b',2,2,'X'), "\
"HEX(INSERT('é🙂b',2,1,'X')); "\
"SELECT INSERT(12345, 2, 2, 9), INSERT(TRUE, 1, 1, FALSE); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
INSERT('abc',2,1,'X')	inserted
aXc	aYc
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT INSERT('abc',2,1,'X'), INSERT('abc',2,1,'Y') AS inserted FROM DUAL;" \
    "$DATABASE"

spaced_expected=$(cat <<EXPECTED
QuWhattic	aXc
EXPECTED
)
expect_output \
    "whitespace and parenthesized arguments" \
    "$spaced_expected" \
    "SELECT INSERT ('Quadratic',3,4,'What'), INSERT(('abc'),(2),(1),('X'));" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "DO INSERT string status" \
    "$do_expected" \
    "DO INSERT('abc',2,1,'X'), INSERT(NULL,1,1,'x'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	aXef	aXc	heYYo	1X45	1X30	20X	2024-/-06	12.13:14	2024-05-06T12:13:14	2024-05-06T12:13:14
2	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table INSERT string values" \
    "$table_expected" \
    "CREATE TABLE t(id INT, v VARCHAR(20), c CHAR(5), txt TEXT, i INT, "\
"d DECIMAL(6,2), y YEAR, dt DATE, tm TIME, dttm DATETIME, ts TIMESTAMP NULL); "\
"INSERT INTO t VALUES "\
"(1,'abcdef','abc','hello',12345,12.30,2024,'2024-05-06','12:13:14',"\
"'2024-05-06 12:13:14','2024-05-06 12:13:14'),"\
"(2,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL); "\
"SELECT id, INSERT(v,2,3,'X'), INSERT(c,2,1,'X'), INSERT(txt,3,2,'YY'), "\
"INSERT(i,2,2,'X'), INSERT(d,2,2,'X'), INSERT(y,3,2,'X'), "\
"INSERT(dt,6,2,'/'), INSERT(tm,3,1,'.'), INSERT(dttm,11,1,'T'), "\
"INSERT(ts,11,1,'T') FROM t ORDER BY id;" \
    "$DATABASE"

limited_expected=$(cat <<EXPECTED
1	aXef	a${DATABASE}ef	a0ef
EXPECTED
)
expect_output \
    "table WHERE ORDER LIMIT INSERT string values" \
    "$limited_expected" \
    "DO 0; SELECT id, INSERT(v,2,3,'X'), INSERT(v,2,3,DATABASE()), "\
"INSERT(v,2,3,@@warning_count) FROM t WHERE id >= 1 ORDER BY id LIMIT 1;" \
    "$DATABASE"

expect_error \
    "INSERT string zero arguments" \
    1064 \
    "42000" \
    "You have an error in your SQL syntax" \
    "SELECT INSERT();" \
    "$DATABASE"

expect_error \
    "INSERT string one argument" \
    1064 \
    "42000" \
    "You have an error in your SQL syntax" \
    "SELECT INSERT('a');" \
    "$DATABASE"

expect_error \
    "INSERT string many arguments" \
    1064 \
    "42000" \
    "You have an error in your SQL syntax" \
    "SELECT INSERT('a',1,1,'b','c');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_insert_string_function_expectations: ok"
