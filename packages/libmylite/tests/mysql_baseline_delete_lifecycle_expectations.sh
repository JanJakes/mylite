#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_delete_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_delete_lifecycle_expectations: $1" >&2
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
    "delete without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "DELETE FROM numbers;"

run_mysql \
    "CREATE TABLE ${DATABASE}.qualified_numbers (id INT NOT NULL, i INT); "\
"INSERT INTO ${DATABASE}.qualified_numbers VALUES (1, 1), (2, 2);" >/dev/null
expect_output \
    "schema-qualified delete without selected schema" \
    "1	0	1" \
    "DELETE FROM ${DATABASE}.qualified_numbers WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM ${DATABASE}.qualified_numbers;"
run_mysql "DROP TABLE ${DATABASE}.qualified_numbers;" >/dev/null

reset_numbers
expect_output \
    "full-table delete affected rows" \
    "4	0	0" \
    "DELETE FROM numbers; SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "filtered delete comparison" \
    "1	0	2,3,4" \
    "DELETE FROM numbers WHERE i < 0; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "no-match delete affected rows" \
    "0	0	4	1,2,3,4" \
    "DELETE FROM numbers WHERE i < -100; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "null-safe predicate delete" \
    "1	0	1,3,4" \
    "DELETE FROM numbers WHERE i <=> 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "is null ordered limited delete" \
    "1	0	1,2,4" \
    "DELETE FROM numbers WHERE n IS NULL ORDER BY id DESC LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "is not null delete" \
    "2	0	1,3" \
    "DELETE FROM numbers WHERE n IS NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "default ascending order limit" \
    "2	0	2,3" \
    "DELETE FROM numbers ORDER BY i LIMIT 2; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "explicit ascending order limit" \
    "2	0	2,3" \
    "DELETE FROM numbers ORDER BY i ASC LIMIT 2; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "descending order limit" \
    "1	0	1,2,4" \
    "DELETE FROM numbers ORDER BY i DESC LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "nullable ascending order limit" \
    "2	0	2,4" \
    "DELETE FROM numbers ORDER BY n LIMIT 2; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "nullable descending order limit" \
    "2	0	1,3" \
    "DELETE FROM numbers ORDER BY n DESC LIMIT 2; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "order by without limit accepted" \
    "4	0	0" \
    "DELETE FROM numbers ORDER BY i; SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "limit zero" \
    "0	0	4	1,2,3,4" \
    "DELETE FROM numbers LIMIT 0; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "limit larger than match count" \
    "4	0	0" \
    "DELETE FROM numbers LIMIT 10; SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "signed and unsigned integer families" \
    "4	0	0" \
    "DELETE FROM numbers WHERE i >= -2; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "maximum supported MyLite limit is accepted upstream" \
    "4	0	0" \
    "DELETE FROM numbers ORDER BY id LIMIT 9223372036854775807; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "mysql accepts unsigned64 delete limit upstream" \
    "4	0	0" \
    "DELETE FROM numbers ORDER BY id LIMIT 18446744073709551615; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_error \
    "unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "DELETE FROM ${MISSING_DATABASE}.numbers;"

expect_error \
    "unknown unqualified table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "DELETE FROM missing_table;" \
    "$DATABASE"

expect_error \
    "unknown qualified table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "DELETE FROM ${DATABASE}.missing_table;" \
    "$DATABASE"

expect_error \
    "unknown predicate column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "DELETE FROM numbers WHERE missing = 1;" \
    "$DATABASE"

expect_error \
    "unknown order column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'order clause'" \
    "DELETE FROM numbers ORDER BY missing LIMIT 1;" \
    "$DATABASE"

expect_error \
    "signed plus limit upstream" \
    1064 \
    42000 \
    "near '+1'" \
    "DELETE FROM numbers ORDER BY id LIMIT +1;" \
    "$DATABASE"

expect_error \
    "signed minus limit upstream" \
    1064 \
    42000 \
    "near '-1'" \
    "DELETE FROM numbers ORDER BY id LIMIT -1;" \
    "$DATABASE"

expect_error \
    "decimal limit upstream" \
    1064 \
    42000 \
    "near '1.0'" \
    "DELETE FROM numbers ORDER BY id LIMIT 1.0;" \
    "$DATABASE"

expect_error \
    "string limit upstream" \
    1064 \
    42000 \
    "near ''1''" \
    "DELETE FROM numbers ORDER BY id LIMIT '1';" \
    "$DATABASE"

expect_error \
    "hex limit upstream" \
    1064 \
    42000 \
    "near '0x1'" \
    "DELETE FROM numbers ORDER BY id LIMIT 0x1;" \
    "$DATABASE"

expect_error \
    "bit limit upstream" \
    1064 \
    42000 \
    "near 'b'1''" \
    "DELETE FROM numbers ORDER BY id LIMIT b'1';" \
    "$DATABASE"

expect_error \
    "parameter limit upstream" \
    1064 \
    42000 \
    "near '?'" \
    "DELETE FROM numbers ORDER BY id LIMIT ?;" \
    "$DATABASE"

expect_error \
    "offset keyword form upstream" \
    1064 \
    42000 \
    "near 'OFFSET 1'" \
    "DELETE FROM numbers ORDER BY id LIMIT 1 OFFSET 1;" \
    "$DATABASE"

expect_error \
    "comma offset form upstream" \
    1064 \
    42000 \
    "near ', 1'" \
    "DELETE FROM numbers ORDER BY id LIMIT 1, 1;" \
    "$DATABASE"

expect_error \
    "unsigned64 overflow limit upstream" \
    1064 \
    42000 \
    "near '18446744073709551616'" \
    "DELETE FROM numbers ORDER BY id LIMIT 18446744073709551616;" \
    "$DATABASE"

reset_numbers
expect_output \
    "mysql accepts table-qualified order upstream" \
    "1	0	3" \
    "DELETE FROM numbers ORDER BY numbers.id LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "mysql accepts expression order upstream" \
    "1	0	3" \
    "DELETE FROM numbers ORDER BY id + 1 LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_error \
    "mysql rejects ordinal order upstream" \
    1054 \
    42S22 \
    "Unknown column '1' in 'order clause'" \
    "DELETE FROM numbers ORDER BY 1 LIMIT 1;" \
    "$DATABASE"

reset_numbers
expect_output \
    "mysql accepts multiple order keys upstream" \
    "2	0	2" \
    "DELETE FROM numbers ORDER BY n, id LIMIT 2; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "mysql accepts alias upstream" \
    "1	0	3" \
    "DELETE FROM numbers AS x WHERE x.id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "mysql accepts low priority upstream" \
    "1	0	3" \
    "DELETE LOW_PRIORITY FROM numbers WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "mysql accepts quick upstream" \
    "1	0	3" \
    "DELETE QUICK FROM numbers WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "mysql accepts ignore upstream" \
    "1	0	3" \
    "DELETE IGNORE FROM numbers WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM numbers;" \
    "$DATABASE"

printf '%s\n' "baseline-delete-lifecycle MySQL 8.4.9 expectations verified"
