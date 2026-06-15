#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_row_scalar_numeric_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_row_scalar_numeric_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --binary-as-hex=1 --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --binary-as-hex=1 "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    expect_value "$label" "$expected" "$output"
}

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
    expect_value "$label" "$expected" "$output"
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};
           CREATE TABLE numeric_rows(
               id INT PRIMARY KEY,
               i INT,
               mask INT,
               angle INT,
               nullable INT
           );
           INSERT INTO numeric_rows(id, i, mask, angle, nullable) VALUES
               (1, -4, 3, 0, NULL),
               (2, 0, 8, 1, 5),
               (3, 9, 15, 2, NULL);" \
    >/dev/null

expect_output_with_headers \
    "integer numeric function projection" \
    "id	ABS(i)	SIGN(i)	CEIL(i)	CEILING(i)	FLOOR(i)	ROUND(i)	ROUND(i, -1)	SQRT(ABS(i))	BIT_COUNT(mask)
1	4	-1	-4	-4	-4	-4	0	2	2
2	0	0	0	0	0	0	0	0	1
3	9	1	9	9	9	9	10	3	4" \
    "SELECT id, ABS(i), SIGN(i), CEIL(i), CEILING(i), FLOOR(i),
            ROUND(i), ROUND(i, -1), SQRT(ABS(i)), BIT_COUNT(mask)
     FROM numeric_rows ORDER BY id;" \
    "$DATABASE"

expect_output_with_headers \
    "rounded floating numeric function projection" \
    "id	ROUND(DEGREES(RADIANS(angle)))	ROUND(ACOS(1))	ROUND(ASIN(0))	ROUND(ATAN(0))	ROUND(ATAN2(0, 1))	ROUND(SIN(angle))	ROUND(COS(angle - angle))	ROUND(TAN(0))	ROUND(COT(1))	ROUND(EXP(0))	ROUND(LN(1))	ROUND(LOG(10, 100))	ROUND(LOG10(100))	ROUND(LOG2(8))	ROUND(POW(id, 2))	ROUND(POWER(id, 2))
1	0	0	0	0	0	0	1	0	1	1	0	2	2	3	1	1
2	1	0	0	0	0	1	1	0	1	1	0	2	2	3	4	4
3	2	0	0	0	0	1	1	0	1	1	0	2	2	3	9	9" \
    "SELECT id, ROUND(DEGREES(RADIANS(angle))), ROUND(ACOS(1)),
            ROUND(ASIN(0)), ROUND(ATAN(0)), ROUND(ATAN2(0, 1)),
            ROUND(SIN(angle)), ROUND(COS(angle - angle)), ROUND(TAN(0)),
            ROUND(COT(1)), ROUND(EXP(0)), ROUND(LN(1)),
            ROUND(LOG(10, 100)), ROUND(LOG10(100)), ROUND(LOG2(8)),
            ROUND(POW(id, 2)), ROUND(POWER(id, 2))
     FROM numeric_rows ORDER BY id;" \
    "$DATABASE"

expect_output \
    "numeric predicates and order by" \
    "1
3" \
    "SELECT id FROM numeric_rows
     WHERE ABS(i) >= 4 AND (BIT_COUNT(mask) = 2 OR BIT_COUNT(mask) = 4)
     ORDER BY ABS(i), id;" \
    "$DATABASE"

expect_output \
    "nested numeric predicate" \
    "2
3" \
    "SELECT id FROM numeric_rows WHERE ROUND(SIN(angle)) = 1 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "sqrt domain predicate" \
    "1" \
    "SELECT id FROM numeric_rows WHERE SQRT(i) IS NULL ORDER BY id;" \
    "$DATABASE"

expect_output \
    "log domain predicate" \
    "1
2" \
    "SELECT id FROM numeric_rows WHERE LOG(i) IS NULL ORDER BY id;" \
    "$DATABASE"

expect_output \
    "acos domain predicate" \
    "1
3" \
    "SELECT id FROM numeric_rows WHERE ACOS(ABS(i)) IS NULL ORDER BY id;" \
    "$DATABASE"

expect_output_with_headers \
    "numeric arithmetic and control flow operands" \
    "id	ABS(i) + 1	IFNULL(SIGN(nullable), 0)	IF(ABS(i), ROUND(POWER(id, 2)), 100)	CASE WHEN SIGN(i) = 0 THEN 0 ELSE SIGN(i) END
1	5	0	1	-1
2	1	1	100	0
3	10	0	9	1" \
    "SELECT id, ABS(i) + 1, IFNULL(SIGN(nullable), 0),
            IF(ABS(i), ROUND(POWER(id, 2)), 100),
            CASE WHEN SIGN(i) = 0 THEN 0 ELSE SIGN(i) END
     FROM numeric_rows ORDER BY id;" \
    "$DATABASE"

expect_error \
    "cot zero row error" \
    1690 \
    22003 \
    "DOUBLE value is out of range in 'cot(0)'" \
    "SELECT COT(0) FROM numeric_rows;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_row_scalar_numeric_functions_expectations: ok"
