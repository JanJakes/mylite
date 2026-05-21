#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_update_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_update_lifecycle_expectations: $1" >&2
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

reset_numbers() {
    run_mysql \
        "DROP TABLE IF EXISTS numbers; "\
"CREATE TABLE numbers ("\
"id INT NOT NULL, i INT, ii INTEGER, iu INT UNSIGNED, integeru INTEGER UNSIGNED, "\
"b BIGINT, bu BIGINT UNSIGNED, n INT NULL, nn INT NOT NULL, tie INT NULL); "\
"INSERT INTO numbers VALUES "\
"(1, -2, -3, 0, 7, -9223372036854775808, 0, NULL, 5, 1), "\
"(2, 1, 5, 2, 8, 3, 4, 9, 6, 1), "\
"(3, 2147483647, 6, 4294967295, 9, 9223372036854775807, 9223372036854775807, NULL, 7, 2), "\
"(4, 0, 7, 8, 10, 8, 8, 9, 8, 2);" \
        "$DATABASE" >/dev/null
}

reset_strings() {
    run_mysql \
        "DROP TABLE IF EXISTS strings; "\
"CREATE TABLE strings (id INT NOT NULL, k VARCHAR(16) NULL, v VARCHAR(16) NULL); "\
"INSERT INTO strings VALUES (1, 'b', 'old'), (2, 'a', 'old'), "\
"(3, 'c', 'old'), (4, NULL, 'old');" \
        "$DATABASE" >/dev/null
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${OTHER_DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${MISSING_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; CREATE DATABASE ${OTHER_DATABASE};" >/dev/null

expect_error \
    "update without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "UPDATE numbers SET i = 1;"

run_mysql \
    "CREATE TABLE ${DATABASE}.qualified_numbers (id INT NOT NULL, i INT); "\
"INSERT INTO ${DATABASE}.qualified_numbers VALUES (1, 1), (2, 2);" >/dev/null
expect_output \
    "schema-qualified update without selected schema" \
    "1	0	1:9,2:2" \
    "UPDATE ${DATABASE}.qualified_numbers SET i = 9 WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) "\
"FROM ${DATABASE}.qualified_numbers;"
run_mysql "DROP TABLE ${DATABASE}.qualified_numbers;" >/dev/null

reset_numbers
expect_output \
    "full-table update changed rows" \
    "4	0	1:5,2:5,3:5,4:5" \
    "UPDATE numbers SET i = 5; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM numbers;" \
    "$DATABASE"

expect_output \
    "no-op update changed rows" \
    "0	0	1:5,2:5,3:5,4:5" \
    "UPDATE numbers SET i = 5; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "nullable null no-op assignment" \
    "0	0	1:N,2:9,3:N,4:9" \
    "UPDATE numbers SET n = NULL WHERE n IS NULL; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', IFNULL(n, 'N')) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "nullable null changed assignment" \
    "2	0	1:N,2:N,3:N,4:N" \
    "UPDATE numbers SET n = NULL WHERE n IS NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', IFNULL(n, 'N')) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_error \
    "null into not null" \
    1048 \
    23000 \
    "Column 'nn' cannot be null" \
    "UPDATE numbers SET nn = NULL;" \
    "$DATABASE"

reset_numbers
expect_output \
    "filtered update comparison" \
    "1	0	1:-10,2:1,3:2147483647,4:0" \
    "UPDATE numbers SET i = -10 WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "null-safe predicate update" \
    "1	0	1:-2,2:12,3:2147483647,4:0" \
    "UPDATE numbers SET i = 12 WHERE i <=> 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "is null update" \
    "2	0	1:11,2:1,3:11,4:0" \
    "UPDATE numbers SET i = 11 WHERE n IS NULL; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "is not null update" \
    "2	0	1:-2,2:13,3:2147483647,4:13" \
    "UPDATE numbers SET i = 13 WHERE n IS NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "default ascending order limit" \
    "2	0	1:100,2:1,3:2147483647,4:100" \
    "UPDATE numbers SET i = 100 ORDER BY i LIMIT 2; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "explicit ascending order limit" \
    "2	0	1:100,2:1,3:2147483647,4:100" \
    "UPDATE numbers SET i = 100 ORDER BY i ASC LIMIT 2; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "descending order limit" \
    "1	0	1:-2,2:1,3:100,4:0" \
    "UPDATE numbers SET i = 100 ORDER BY i DESC LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "nullable ascending order limit" \
    "2	0	1:100,2:1,3:100,4:0" \
    "UPDATE numbers SET i = 100 ORDER BY n LIMIT 2; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "nullable descending order limit" \
    "2	0	1:-2,2:100,3:2147483647,4:100" \
    "UPDATE numbers SET i = 100 ORDER BY n DESC LIMIT 2; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "order by without limit accepted" \
    "4	0	1:100,2:100,3:100,4:100" \
    "UPDATE numbers SET i = 100 ORDER BY i; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_strings
expect_output \
    "string where order limit" \
    "1	0	1:b:old,2:a:x,3:c:old,4:N:old" \
    "UPDATE strings SET v = 'x' WHERE k = 'a' ORDER BY k LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', IFNULL(k, 'N'), ':', IFNULL(v, 'N')) ORDER BY id) "\
"FROM strings;" \
    "$DATABASE"

reset_strings
expect_output \
    "nullable string ascending order limit" \
    "1	0	1:b:old,2:a:old,3:c:old,4:N:first" \
    "UPDATE strings SET v = 'first' ORDER BY k ASC LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', IFNULL(k, 'N'), ':', IFNULL(v, 'N')) ORDER BY id) "\
"FROM strings;" \
    "$DATABASE"

reset_strings
expect_output \
    "nullable string descending order limit" \
    "1	0	1:b:old,2:a:old,3:c:last,4:N:old" \
    "UPDATE strings SET v = 'last' ORDER BY k DESC LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', IFNULL(k, 'N'), ':', IFNULL(v, 'N')) ORDER BY id) "\
"FROM strings;" \
    "$DATABASE"

reset_numbers
expect_output \
    "limit zero" \
    "0	0	1:-2,2:1,3:2147483647,4:0" \
    "UPDATE numbers SET i = 100 LIMIT 0; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "limit larger than match count" \
    "4	0	1:100,2:100,3:100,4:100" \
    "UPDATE numbers SET i = 100 LIMIT 10; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "matched limit consumes no-op row" \
    "0	0	1:100,2:2,3:3" \
    "DROP TABLE numbers; CREATE TABLE numbers (id INT NOT NULL, i INT); "\
"INSERT INTO numbers VALUES (1, 100), (2, 2), (3, 3); "\
"UPDATE numbers SET i = 100 ORDER BY id LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "signed and unsigned integer families" \
    "4	0	1:9:9:9:9:9:9,2:9:9:9:9:9:9,3:9:9:9:9:9:9,4:9:9:9:9:9:9" \
    "UPDATE numbers SET ii = 9; UPDATE numbers SET iu = 9; UPDATE numbers SET integeru = 9; "\
"UPDATE numbers SET b = 9; UPDATE numbers SET bu = 9; UPDATE numbers SET nn = 9; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', ii, ':', iu, ':', integeru, ':', b, ':', bu, ':', nn) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "bigint minimum assignment" \
    "3	0	-9223372036854775808" \
    "UPDATE numbers SET b = -9223372036854775808; "\
"SELECT ROW_COUNT(), @@warning_count, MIN(b) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "bigint unsigned maximum supported assignment" \
    "3	0	9223372036854775807" \
    "UPDATE numbers SET bu = 9223372036854775807; "\
"SELECT ROW_COUNT(), @@warning_count, MAX(bu) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_error \
    "unknown assignment column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "UPDATE numbers SET missing = 1;" \
    "$DATABASE"

expect_error \
    "unknown where column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "UPDATE numbers SET i = 1 WHERE missing = 1;" \
    "$DATABASE"

expect_error \
    "unknown order column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'order clause'" \
    "UPDATE numbers SET i = 1 ORDER BY missing LIMIT 1;" \
    "$DATABASE"

expect_error \
    "unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "UPDATE ${MISSING_DATABASE}.numbers SET i = 1;"

expect_error \
    "unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "UPDATE missing_table SET i = 1;" \
    "$DATABASE"

reset_numbers
expect_output \
    "not-null assignment skipped by no-match predicate" \
    "0	0	1:5,2:6,3:7,4:8" \
    "UPDATE numbers SET nn = NULL WHERE id = 999; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', nn) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "not-null assignment skipped by limit zero" \
    "0	0	1:5,2:6,3:7,4:8" \
    "UPDATE numbers SET nn = NULL LIMIT 0; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', nn) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "signed int out-of-range assignment skipped by no-match predicate" \
    "0	0	1:-2,2:1,3:2147483647,4:0" \
    "UPDATE numbers SET i = 2147483648 WHERE id = 999; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "signed int out-of-range assignment skipped by limit zero" \
    "0	0	1:-2,2:1,3:2147483647,4:0" \
    "UPDATE numbers SET i = 2147483648 LIMIT 0; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "unsigned int out-of-range assignment skipped by no-match predicate" \
    "0	0	1:0,2:2,3:4294967295,4:8" \
    "UPDATE numbers SET iu = -1 WHERE id = 999; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', iu) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "unsigned int out-of-range assignment skipped by limit zero" \
    "0	0	1:0,2:2,3:4294967295,4:8" \
    "UPDATE numbers SET iu = -1 LIMIT 0; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', iu) ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_error \
    "signed int assignment out of range" \
    1264 \
    22003 \
    "Out of range value for column 'i' at row 1" \
    "UPDATE numbers SET i = 2147483648;" \
    "$DATABASE"

reset_numbers
expect_error \
    "unsigned int assignment out of range" \
    1264 \
    22003 \
    "Out of range value for column 'iu' at row 1" \
    "UPDATE numbers SET iu = -1;" \
    "$DATABASE"

reset_numbers
expect_output \
    "mysql accepts bigint unsigned above signed64 assignment upstream" \
    "4	0	9223372036854775808" \
    "UPDATE numbers SET bu = 9223372036854775808; "\
"SELECT ROW_COUNT(), @@warning_count, MAX(bu) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "maximum supported MyLite limit is accepted upstream" \
    "4	0	0" \
    "UPDATE numbers SET i = 100 ORDER BY id LIMIT 9223372036854775807; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM numbers WHERE i <> 100;" \
    "$DATABASE"

reset_numbers
expect_output \
    "mysql accepts signed64 overflow update limit upstream" \
    "4	0	0" \
    "UPDATE numbers SET i = 100 ORDER BY id LIMIT 9223372036854775808; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM numbers WHERE i <> 100;" \
    "$DATABASE"

reset_numbers
expect_output \
    "mysql accepts unsigned64 update limit upstream" \
    "4	0	0" \
    "UPDATE numbers SET i = 100 ORDER BY id LIMIT 18446744073709551615; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM numbers WHERE i <> 100;" \
    "$DATABASE"

expect_error \
    "update limit too large upstream" \
    1064 \
    42000 \
    "18446744073709551616" \
    "UPDATE numbers SET i = 100 ORDER BY id LIMIT 18446744073709551616;" \
    "$DATABASE"

expect_error \
    "signed positive limit rejected" \
    1064 \
    42000 \
    "+1" \
    "UPDATE numbers SET i = 100 ORDER BY id LIMIT +1;" \
    "$DATABASE"

expect_error \
    "signed negative limit rejected" \
    1064 \
    42000 \
    "-1" \
    "UPDATE numbers SET i = 100 ORDER BY id LIMIT -1;" \
    "$DATABASE"

expect_error \
    "offset keyword limit rejected" \
    1064 \
    42000 \
    "OFFSET 1" \
    "UPDATE numbers SET i = 100 ORDER BY id LIMIT 1 OFFSET 1;" \
    "$DATABASE"

expect_error \
    "comma limit rejected" \
    1064 \
    42000 \
    ", 1" \
    "UPDATE numbers SET i = 100 ORDER BY id LIMIT 1, 1;" \
    "$DATABASE"

expect_output \
    "mysql accepts wider single-table update syntax upstream" \
    "2	0	11	13" \
    "DROP TABLE IF EXISTS wider; CREATE TABLE wider (id INT NOT NULL, i INT, n INT); "\
"INSERT INTO wider VALUES (1, 1, 5), (2, 2, 6); "\
"UPDATE wider AS x SET x.i = x.n, n = n + 1 ORDER BY x.id DESC LIMIT 2; "\
"SELECT ROW_COUNT(), @@warning_count, SUM(i), SUM(n) FROM wider;" \
    "$DATABASE"
