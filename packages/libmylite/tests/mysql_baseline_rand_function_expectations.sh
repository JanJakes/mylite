#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_rand_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_rand_function_expectations: $1" >&2
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

expect_rand_status_row() {
    label=$1
    sql=$2
    shift 2

    output=$(run_mysql "$sql" "$@")
    printf '%s\n' "$output" | awk -F '\t' '
        NF != 4 { exit 1 }
        $1 !~ /^[0-9]+([.][0-9]+)?$/ { exit 1 }
        $2 !~ /^[0-9]+([.][0-9]+)?$/ { exit 1 }
        {
            left = $1 + 0
            right = $2 + 0
            if (!(left >= 0 && left < 1 && right >= 0 && right < 1)) {
                exit 1
            }
            if ($3 != "0" || $4 != "0") {
                exit 1
            }
        }
    ' || fail "$label: expected two RAND values in [0,1), warning 0, row_count 0; got [$output]"
}

expect_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept behavior, got [$output]"
    fi
}

expect_table_rand_projection_range() {
    label=$1
    sql=$2
    shift 2

    output=$(run_mysql "$sql" "$@")
    printf '%s\n' "$output" | awk -F '\t' '
        NF != 2 { exit 1 }
        $1 != NR { exit 1 }
        $2 !~ /^[0-9]+([.][0-9]+)?$/ { exit 1 }
        {
            value = $2 + 0
            if (!(value >= 0 && value < 1)) {
                exit 1
            }
        }
        END { if (NR != 5) exit 1 }
    ' || fail "$label: expected ids 1..5 with RAND values in [0,1), got [$output]"
}

expect_table_rand_order_set() {
    label=$1
    sql=$2
    shift 2

    output=$(run_mysql "$sql" "$@")
    printf '%s\n' "$output" | awk -F '\t' '
        NF != 1 { exit 1 }
        $1 !~ /^[1-5]$/ { exit 1 }
        seen[$1]++ { exit 1 }
        END {
            if (NR != 5) exit 1
            for (i = 1; i <= 5; ++i) {
                if (seen[i] != 1) exit 1
            }
        }
    ' || fail "$label: expected ids 1..5 exactly once, got [$output]"
}

expect_rand_header_and_range() {
    label=$1
    expected_header=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
    header=$(printf '%s\n' "$output" | sed -n '1p')
    values=$(printf '%s\n' "$output" | sed -n '2p')

    if [ "$header" != "$expected_header" ]; then
        fail "$label: expected header [$expected_header], got [$header]"
    fi
    printf '%s\n' "$values" | awk -F '\t' '
        NF < 1 { exit 1 }
        {
            for (i = 1; i <= NF; ++i) {
                if ($i !~ /^[0-9]+([.][0-9]+)?$/) {
                    exit 1
                }
                value = $i + 0
                if (!(value >= 0 && value < 1)) {
                    exit 1
                }
            }
        }
    ' || fail "$label: expected RAND values in [0,1), got [$values]"
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

expect_rand_status_row \
    "RAND values and status" \
    "DO 0; SELECT RAND(), RAND(), @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_rand_header_and_range \
    "RAND labels and DUAL" \
    "RAND ()	r" \
    "SELECT RAND (), RAND() AS r FROM DUAL;" \
    "$DATABASE"

expect_output \
    "RAND DO status" \
    "0	0" \
    "DO RAND(), RAND(); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "bare RAND identifier" \
    1054 \
    "42S22" \
    "Unknown column 'RAND' in 'field list'" \
    "SELECT RAND;" \
    "$DATABASE"

expect_accepts \
    "RAND identifier table" \
    "DROP TABLE IF EXISTS rand; CREATE TABLE rand (id INT); SELECT COUNT(*) FROM rand;" \
    "$DATABASE"

expect_accepts \
    "seeded RAND accepted by MySQL" \
    "SELECT RAND(3), RAND(NULL), RAND(TRUE), RAND(FALSE), RAND(-1);" \
    "$DATABASE"

expect_output \
    "coerced numeric RAND seeds" \
    "0.9057697559760601	0.15595286540310166	0.15595286540310166	0.40467110313910165	0.15448799371206015	0.15448799371206015	0.40540353712197724	0.9050373219931845	0.15522042769493574	0.15522042769493574	0.15522042769493574	0.40540353712197724	0.15595286540310166	0.40540353712197724	0.6548542125661431	0.15595286540310166	0.15595286540310166" \
    "SELECT RAND(3.4), RAND(3.5), RAND(3.9), RAND(-3.4), RAND(-3.5), "\
"RAND(-3.9), RAND(+TRUE), RAND(-TRUE), RAND(+NULL), RAND(-NULL), "\
"RAND(NULLIF(1, 1)), RAND(NULLIF(1, 2)), RAND(3.9e0), RAND(1e0), "\
"RAND(-2e0), RAND(4.5e0), RAND(4.4e0);" \
    "$DATABASE"

expect_error \
    "out-of-range approximate RAND seed rejected by MySQL" \
    1367 \
    "22007" \
    "Illegal double '1e309' value found during parsing" \
    "SELECT RAND(1e309);" \
    "$DATABASE"

expect_output \
    "coerced string RAND seed warnings" \
    "0.40613597483014313	0.9057697559760601	0.15522042769493574	0.6548542125661431
Warning	1292	Truncated incorrect INTEGER value: '3.9'
Warning	1292	Truncated incorrect INTEGER value: 'abc'
Warning	1292	Truncated incorrect INTEGER value: '  -2x'" \
    "SELECT RAND('5'), RAND('3.9'), RAND('abc'), RAND('  -2x'); SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "coerced RAND same-statement warning count" \
    "0.9057697559760601	0
Warning	1292	Truncated incorrect INTEGER value: '3.9'" \
    "DO 0; SELECT RAND('3.9'), @@warning_count; SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "coerced RAND DUAL warnings" \
    "0.9057697559760601
Warning	1292	Truncated incorrect INTEGER value: '3.9'" \
    "SELECT RAND('3.9') FROM DUAL; SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "coerced RAND DO warnings" \
    "Warning	1292	Truncated incorrect INTEGER value: '3.9'" \
    "DO RAND('3.9'); SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "coerced CAST and CONVERT RAND seed warnings" \
    "0.40613597483014313	0.9057697559760601	0.15522042769493574	0.15522042769493574	0.40613597483014313	0.9057697559760601	0.15522042769493574
Warning	1292	Truncated incorrect INTEGER value: '3.9'
Warning	1292	Truncated incorrect INTEGER value: 'abc'
Warning	1292	Truncated incorrect INTEGER value: '3.9'" \
    "SELECT RAND(CAST('5' AS SIGNED)), RAND(CAST('3.9' AS SIGNED)), "\
"RAND(CAST('abc' AS SIGNED)), RAND(CAST(NULL AS UNSIGNED)), "\
"RAND(CONVERT('5', SIGNED)), RAND(CONVERT('3.9', UNSIGNED)), "\
"RAND(CONVERT(NULL, SIGNED)); SHOW WARNINGS;" \
    "$DATABASE"

run_mysql \
    "DROP TABLE IF EXISTS t; CREATE TABLE t(id INT, k INT NULL); "\
"INSERT INTO t VALUES (1, NULL), (2, 2), (3, 2), (4, 4), (5, NULL);" \
    "$DATABASE" >/dev/null

expect_table_rand_projection_range \
    "table-backed RAND projection" \
    "SELECT id, RAND() FROM t ORDER BY id;" \
    "$DATABASE"

expect_table_rand_order_set \
    "table-backed ORDER BY RAND rowset" \
    "SELECT id FROM t ORDER BY RAND() LIMIT 5;" \
    "$DATABASE"

expect_error \
    "RAND too many arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'RAND'" \
    "SELECT RAND(1, 2);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_rand_function_expectations: ok"
