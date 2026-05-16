#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_iso_temporal_predicate_literals_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_iso_temporal_predicate_literals_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

run_mysql \
    "SET time_zone = '+00:00';
     CREATE TABLE temporal_values (
         id INT NOT NULL,
         dt DATETIME,
         ts TIMESTAMP NULL,
         marker INT NOT NULL DEFAULT 0
     );
     INSERT INTO temporal_values VALUES
         (1, '2016-01-14 10:00:00', '2016-01-14 10:00:00', 0),
         (2, '2016-01-14 23:00:00', '2016-01-14 23:00:00', 0),
         (3, '2016-01-15 00:00:00', '2016-01-15 00:00:00', 0),
         (4, '2016-01-15 01:00:00', '2016-01-15 01:00:00', 0),
         (5, '2016-01-15 14:00:00', '2016-01-15 14:00:00', 0);" \
    "$DATABASE" >/dev/null

expect_output \
    "datetime T separator and numeric offsets match without warnings" \
    "3
3
3
3
3
3
3
4,5
3
1,5
1,2,4,5" \
    "SET time_zone = '+00:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE dt = '2016-01-15T00:00:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE dt = '2016-01-15T00:00:00+00:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE dt = '2016-01-15T01:00:00+01:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE dt = '2016-01-14T23:00:00-01:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE dt = '2016-01-15T14:00:00+14:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE dt = '2016-01-14T10:00:00-14:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE dt = '2016-01-15 00:00:00+00:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE dt > '2016-01-15 00:00:00+00:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE dt <=> '2016-01-15T00:00:00+00:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE dt NOT BETWEEN '2016-01-14T23:00:00+00:00'
       AND '2016-01-15T01:00:00+00:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE dt NOT IN ('2016-01-15T00:00:00+00:00');
     SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "datetime offsets use the session time zone" \
    "3
NULL" \
    "SET time_zone = '+02:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE dt = '2016-01-14T22:00:00+00:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE dt = '2016-01-15T00:00:00+00:00';
     SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "timestamp explicit offsets compare by instant" \
    "3
3
3
4,5
3
1,5
1,2,4,5
3" \
    "SET time_zone = '+00:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE ts = '2016-01-15T00:00:00+00:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE ts = '2016-01-15T01:00:00+01:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE ts = '2016-01-15 00:00:00+00:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE ts > '2016-01-15 00:00:00+00:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE ts <=> '2016-01-15T00:00:00+00:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE ts NOT BETWEEN '2016-01-14T23:00:00+00:00'
       AND '2016-01-15T01:00:00+00:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE ts NOT IN ('2016-01-15T00:00:00+00:00');
     SET time_zone = '+02:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE ts = '2016-01-15T00:00:00+00:00';
     SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "offset predicates feed update and delete row matching" \
    "1	7
1
4" \
    "SET time_zone = '+00:00';
     UPDATE temporal_values SET marker = 7
       WHERE dt = '2016-01-15T01:00:00+01:00';
     SELECT ROW_COUNT(), marker FROM temporal_values WHERE id = 3;
     DELETE FROM temporal_values
       WHERE ts = '2016-01-15T14:00:00+14:00';
     SELECT ROW_COUNT();
     SELECT COUNT(*) FROM temporal_values;" \
    "$DATABASE"

expect_error \
    "one-digit datetime offset hour is rejected" \
    1525 \
    "HY000" \
    "Incorrect DATETIME value: '2016-01-15T01:00:00+1:00'" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE dt = '2016-01-15T01:00:00+1:00';" \
    "$DATABASE"

expect_error \
    "minus zero datetime offset is rejected" \
    1525 \
    "HY000" \
    "Incorrect DATETIME value: '2016-01-15T00:00:00-00:00'" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE dt = '2016-01-15T00:00:00-00:00';" \
    "$DATABASE"

expect_error \
    "datetime offset beyond positive limit is rejected" \
    1525 \
    "HY000" \
    "Incorrect DATETIME value: '2016-01-15T14:01:00+14:01'" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE dt = '2016-01-15T14:01:00+14:01';" \
    "$DATABASE"

expect_error \
    "one-digit timestamp offset hour is rejected" \
    1525 \
    "HY000" \
    "Incorrect TIMESTAMP value: '2016-01-15T01:00:00+1:00'" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE ts = '2016-01-15T01:00:00+1:00';" \
    "$DATABASE"

expect_error \
    "minus zero timestamp offset is rejected" \
    1525 \
    "HY000" \
    "Incorrect TIMESTAMP value: '2016-01-15T00:00:00-00:00'" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE ts = '2016-01-15T00:00:00-00:00';" \
    "$DATABASE"

expect_error \
    "timestamp offset beyond positive limit is rejected" \
    1525 \
    "HY000" \
    "Incorrect TIMESTAMP value: '2016-01-15T14:01:00+14:01'" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE ts = '2016-01-15T14:01:00+14:01';" \
    "$DATABASE"

expect_output \
    "trailing Z is MySQL warning truncation behavior deferred by this slice" \
    "1
Warning	1292	Incorrect datetime value: '2016-01-14T10:00:00Z' for column 'dt' at row 1" \
    "SET time_zone = '+00:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE dt = '2016-01-14T10:00:00Z';
     SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "timestamp trailing Z is also warning truncation behavior deferred by this slice" \
    "1
Warning	1292	Incorrect datetime value: '2016-01-14T10:00:00Z' for column 'ts' at row 1" \
    "SET time_zone = '+00:00';
     SELECT GROUP_CONCAT(id ORDER BY id) FROM temporal_values
       WHERE ts = '2016-01-14T10:00:00Z';
     SHOW WARNINGS;" \
    "$DATABASE"

expect_error \
    "strict mutating trailing Z predicate can error" \
    1292 \
    "22007" \
    "Incorrect datetime value: '2016-01-14T10:00:00Z' for column 'dt' at row 1" \
    "UPDATE temporal_values SET marker = 8
       WHERE dt = '2016-01-14T10:00:00Z';" \
    "$DATABASE"
