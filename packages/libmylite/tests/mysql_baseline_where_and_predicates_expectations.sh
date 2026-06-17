#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_where_and_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_where_and_predicates_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw "$@"
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

reset_numbers() {
    run_mysql \
        "DROP TABLE IF EXISTS numbers; "\
"CREATE TABLE numbers ("\
"id INT NOT NULL, i INT, iu INT UNSIGNED, b BIGINT, bu BIGINT UNSIGNED, "\
"n INT NULL, nn INT NOT NULL, tie INT NULL); "\
"INSERT INTO numbers VALUES "\
"(1, -2, 0, -9223372036854775808, 0, NULL, 5, 1), "\
"(2, 1, 2, 3, 4, 9, 6, 1), "\
"(3, 2147483647, 4294967295, 9223372036854775807, 9223372036854775807, NULL, 7, 2), "\
"(4, 0, 8, 8, 8, 9, 8, 2);" \
        "$DATABASE" >/dev/null
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${MISSING_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null
reset_numbers

expect_error \
    "and predicate without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "SELECT * FROM no_default_table WHERE id = 1 AND i = 2;"

expect_error \
    "qualified select where and unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "SELECT * FROM ${MISSING_DATABASE}.numbers WHERE id = 1 AND i = 2;"

expected_labels=$(cat <<'EOF'
id	i
2	1
EOF
)
expect_output_with_headers \
    "and labels and row" \
    "$expected_labels" \
    "SELECT id, i FROM numbers WHERE i = 1 AND nn = 6;" \
    "$DATABASE"

expect_output \
    "logical and symbol row" \
    "2" \
    "SELECT id FROM numbers WHERE i = 1 && nn = 6;" \
    "$DATABASE"

expect_output \
    "hex integer equality predicate" \
    "2" \
    "SELECT id FROM numbers WHERE i = 0x1;" \
    "$DATABASE"

expect_output \
    "hex unsigned equality predicate" \
    "3" \
    "SELECT id FROM numbers WHERE iu = 0xffffffff;" \
    "$DATABASE"

expect_output \
    "hex scalar truth predicate" \
    "4" \
    "SELECT COUNT(*) FROM numbers WHERE 0x1;" \
    "$DATABASE"

expect_output \
    "hex scalar false predicate" \
    "0" \
    "SELECT COUNT(*) FROM numbers WHERE 0x0;" \
    "$DATABASE"

expect_output \
    "hex literal-left equality predicate" \
    "2" \
    "SELECT id FROM numbers WHERE 0x1 = i;" \
    "$DATABASE"

expected_hex_in_rows=$(cat <<'EOF'
2
4
EOF
)
expect_output \
    "hex in predicate" \
    "$expected_hex_in_rows" \
    "SELECT id FROM numbers WHERE i IN (0x0, 0x1) ORDER BY id;" \
    "$DATABASE"

expect_output \
    "hex between predicate" \
    "$expected_hex_in_rows" \
    "SELECT id FROM numbers WHERE i BETWEEN 0x0 AND 0x1 ORDER BY id;" \
    "$DATABASE"

expected_symbol_warnings=$(cat <<'EOF'
Warning	1287	'&&' is deprecated and will be removed in a future release. Please use AND instead
Warning	1287	'&&' is deprecated and will be removed in a future release. Please use AND instead
EOF
)
expect_output \
    "logical and symbol warning per token" \
    "$expected_symbol_warnings" \
    "DO (SELECT COUNT(*) FROM numbers WHERE id = 2 && i = 1 && nn = 6); SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "parenthesized conjunction" \
    "2" \
    "SELECT id FROM numbers WHERE (i = 1 AND nn = 6);" \
    "$DATABASE"

expect_output \
    "parenthesized atoms" \
    "2" \
    "SELECT id FROM numbers WHERE (i = 1) AND (nn = 6);" \
    "$DATABASE"

expect_output \
    "nested conjunction" \
    "2" \
    "SELECT id FROM numbers WHERE (i = 1 AND (nn = 6 AND n IS NOT NULL));" \
    "$DATABASE"

expect_output \
    "false and unknown outcomes filter out rows" \
    "" \
    "SELECT id FROM numbers WHERE n = 9 AND nn = 5;" \
    "$DATABASE"

expect_output \
    "is null and comparison" \
    "3" \
    "SELECT id FROM numbers WHERE n IS NULL AND nn = 7;" \
    "$DATABASE"

expect_output \
    "is not null and null-safe comparison" \
    "4" \
    "SELECT id FROM numbers WHERE n IS NOT NULL AND i <=> 0;" \
    "$DATABASE"

expect_output \
    "boolean literal and signed literal" \
    "1" \
    "SELECT id FROM numbers WHERE tie = TRUE AND i = -2;" \
    "$DATABASE"

expect_output \
    "unsigned boundary conjunction" \
    "3" \
    "SELECT id FROM numbers WHERE iu = 4294967295 AND bu = 9223372036854775807;" \
    "$DATABASE"

expect_output \
    "unsigned negative between predicate" \
    "1
2" \
    "SELECT id FROM numbers WHERE iu BETWEEN -1 AND 2 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "unsigned negative in predicate" \
    "2" \
    "SELECT id FROM numbers WHERE iu IN (-1, 2) ORDER BY id;" \
    "$DATABASE"

expect_output \
    "reversed unsigned negative predicate no rows" \
    "" \
    "SELECT id FROM numbers WHERE -1 = iu;" \
    "$DATABASE"

expect_output \
    "source qualified and alias conjunction" \
    "2" \
    "SELECT n.id FROM numbers AS n WHERE n.i = 1 AND n.nn = 6;" \
    "$DATABASE"

expect_output \
    "schema table qualified conjunction" \
    "2" \
    "SELECT id FROM numbers WHERE ${DATABASE}.numbers.i = 1 AND numbers.nn = 6;" \
    "$DATABASE"

expect_output \
    "distinct reuses conjunction" \
    "9" \
    "SELECT DISTINCT n FROM numbers WHERE n IS NOT NULL AND tie = 1;" \
    "$DATABASE"

expect_output \
    "count reuses conjunction" \
    "1" \
    "SELECT COUNT(*) FROM numbers WHERE i >= 0 AND n IS NULL;" \
    "$DATABASE"

expect_output \
    "column aggregate reuses conjunction" \
    "2147483647" \
    "SELECT MAX(i) FROM numbers WHERE nn >= 6 AND n IS NULL;" \
    "$DATABASE"

expect_output \
    "grouped aggregate source where conjunction" \
    "1	1" \
    "SELECT tie, COUNT(*) FROM numbers WHERE id > 1 AND nn >= 6 GROUP BY tie ORDER BY tie LIMIT 1;" \
    "$DATABASE"

expect_output \
    "empty quoted integer in predicate" \
    "4" \
    "SELECT id FROM numbers WHERE i IN ('') ORDER BY id;" \
    "$DATABASE"

expect_output \
    "empty quoted integer grouped predicate" \
    "2	1" \
    "SELECT tie, COUNT(*) FROM numbers WHERE i IN ('') GROUP BY tie ORDER BY tie;" \
    "$DATABASE"

expect_output \
    "create table select source conjunction" \
    "2:1:9" \
    "DROP TABLE IF EXISTS copy_numbers; "\
"CREATE TABLE copy_numbers SELECT id, i, n FROM numbers WHERE i = 1 AND n IS NOT NULL; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', i, ':', n) ORDER BY id) FROM copy_numbers;" \
    "$DATABASE"

expect_output \
    "insert select source conjunction" \
    "1:-2,2:1,2:1,3:2147483647,4:0" \
    "DROP TABLE IF EXISTS inserted_numbers; "\
"CREATE TABLE inserted_numbers (id INT NOT NULL, i INT); "\
"INSERT INTO inserted_numbers SELECT id, i FROM numbers; "\
"INSERT INTO inserted_numbers SELECT id, i FROM numbers WHERE i = 1 AND n IS NOT NULL; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM inserted_numbers;" \
    "$DATABASE"

expect_output \
    "replace select source conjunction" \
    "2:1" \
    "DROP TABLE IF EXISTS replaced_numbers; "\
"CREATE TABLE replaced_numbers (id INT NOT NULL, i INT); "\
"REPLACE INTO replaced_numbers SELECT id, i FROM numbers WHERE i = 1 AND n IS NOT NULL; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id) FROM replaced_numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "update conjunction affected rows" \
    "1	0	1:N,2:11,3:N,4:9" \
    "UPDATE numbers SET n = 11 WHERE i = 1 AND nn = 6; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', IFNULL(n, 'N')) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "update conjunction order limit" \
    "2	0	1:N,2:9,3:99,4:99" \
    "UPDATE numbers SET n = 99 WHERE id > 1 AND nn >= 6 ORDER BY id DESC LIMIT 2; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', IFNULL(n, 'N')) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "delete conjunction affected rows" \
    "1	0	1,2,4" \
    "DELETE FROM numbers WHERE i > 1 AND n IS NULL; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "delete conjunction order limit" \
    "1	0	1,2,3" \
    "DELETE FROM numbers WHERE id > 1 AND nn >= 6 ORDER BY id DESC LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM numbers;" \
    "$DATABASE"

expect_error \
    "unknown first predicate column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "SELECT id FROM numbers WHERE missing = 1 AND id = 1;" \
    "$DATABASE"

expect_error \
    "unknown second predicate column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "SELECT id FROM numbers WHERE id = 1 AND missing = 1;" \
    "$DATABASE"

expect_output \
    "mysql accepts or upstream" \
    "2" \
    "SELECT id FROM numbers WHERE i = 1 OR nn = 8 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts xor upstream" \
    "2" \
    "SELECT id FROM numbers WHERE i = 1 XOR nn = 8 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts not upstream" \
    "1
3" \
    "SELECT id FROM numbers WHERE NOT i = 1 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts bare truth upstream" \
    "1
2
3" \
    "SELECT id FROM numbers WHERE TRUE AND id > 0 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts expression predicate upstream" \
    "2" \
    "SELECT id FROM numbers WHERE i + 1 = 2 AND nn = 6;" \
    "$DATABASE"

reset_numbers
expect_output \
    "mysql accepts arithmetic comparison predicate upstream" \
    "3
4" \
    "SELECT id FROM numbers WHERE i + nn > 7 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts parenthesized arithmetic comparison predicate upstream" \
    "3
4" \
    "SELECT id FROM numbers WHERE (i + nn) > 7 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts nested arithmetic comparison predicate upstream" \
    "2
3
4" \
    "SELECT id FROM numbers WHERE ((i + nn) * 2) > 10 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts multiple nested arithmetic comparison predicates upstream" \
    "3
4" \
    "SELECT id FROM numbers WHERE ((i + nn) * 2) > 10 AND ((nn + tie) * 2) > 14 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts nested arithmetic equality predicate upstream" \
    "4" \
    "SELECT id FROM numbers WHERE ((i + nn) * 2) = 16 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts arithmetic predicate precedence upstream" \
    "1" \
    "SELECT id FROM numbers WHERE i + nn * 2 = 8 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts grouped parenthesized arithmetic comparison predicate upstream" \
    "4" \
    "SELECT id FROM numbers WHERE ((i + nn) > 7 AND id = 4) ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts arithmetic comparison value upstream" \
    "2" \
    "SELECT id FROM numbers WHERE i = nn - 5 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts numeric function arithmetic comparison value upstream" \
    "2" \
    "SELECT id FROM numbers WHERE i = ABS(nn - 7) ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts arithmetic comparison value with row-scalar subject upstream" \
    "1
2
3
4" \
    "SELECT id FROM numbers WHERE i + nn = nn + i ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts nested arithmetic comparison value upstream" \
    "1
2
4" \
    "SELECT id FROM numbers WHERE i < (nn + tie) * 2 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts arithmetic between predicate upstream" \
    "1
2
4" \
    "SELECT id FROM numbers WHERE i + nn BETWEEN 3 AND 8 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts parenthesized arithmetic between predicate upstream" \
    "1
2
4" \
    "SELECT id FROM numbers WHERE (i + nn) BETWEEN 3 AND 8 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts nested arithmetic between predicate upstream" \
    "1
2
4" \
    "SELECT id FROM numbers WHERE ((i + nn) * 2) BETWEEN 6 AND 16 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts arithmetic between bounds upstream" \
    "1
2" \
    "SELECT id FROM numbers WHERE i BETWEEN nn - 7 AND nn - 5 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts arithmetic between bounds with row-scalar subject upstream" \
    "1
2
4" \
    "SELECT id FROM numbers WHERE i + nn BETWEEN nn - 2 AND nn + 2 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts arithmetic not between bounds upstream" \
    "3
4" \
    "SELECT id FROM numbers WHERE i NOT BETWEEN nn - 7 AND nn - 5 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts arithmetic in predicate upstream" \
    "1
4" \
    "SELECT id FROM numbers WHERE i + nn IN (3, 8, NULL) ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts parenthesized arithmetic in predicate upstream" \
    "1
4" \
    "SELECT id FROM numbers WHERE (i + nn) IN (3, 8, NULL) ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts nested arithmetic in predicate upstream" \
    "1
4" \
    "SELECT id FROM numbers WHERE ((i + nn) * 2) IN (6, 16, NULL) ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts arithmetic in-list value upstream" \
    "2
4" \
    "SELECT id FROM numbers WHERE i IN (nn - 5, 0) ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts arithmetic not in-list value upstream" \
    "1
3" \
    "SELECT id FROM numbers WHERE i NOT IN (nn - 5, 0) ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts arithmetic in-list values with row-scalar subject upstream" \
    "1
2" \
    "SELECT id FROM numbers WHERE i + nn IN (nn - 2, nn + 1, NULL) ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts arithmetic boolean predicate upstream" \
    "1
2
3
4" \
    "SELECT id FROM numbers WHERE i + 1 IS TRUE ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts parenthesized arithmetic boolean predicate upstream" \
    "1
2
3
4" \
    "SELECT id FROM numbers WHERE (i + 1) IS TRUE ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts nested arithmetic boolean predicate upstream" \
    "1
2
3
4" \
    "SELECT id FROM numbers WHERE ((i + nn) * 2) IS TRUE ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts parenthesized arithmetic false predicate upstream" \
    "4" \
    "SELECT id FROM numbers WHERE (i + 0) IS FALSE ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts nested arithmetic false predicate upstream" \
    "1
2
3
4" \
    "SELECT id FROM numbers WHERE ((i + nn) * 0) IS FALSE ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts arithmetic mod predicate upstream" \
    "1" \
    "SELECT id FROM numbers WHERE MOD(i + 2, nn) = 0 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts parenthesized arithmetic mod predicate upstream" \
    "1" \
    "SELECT id FROM numbers WHERE (MOD(i + 2, nn)) = 0 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts nested arithmetic mod predicate upstream" \
    "1" \
    "SELECT id FROM numbers WHERE (MOD((i + 2), nn) + 1) = 1 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts column comparison upstream" \
    "" \
    "SELECT id FROM numbers WHERE i = nn AND id = 3;" \
    "$DATABASE"

expect_output \
    "mysql accepts string literal predicate upstream" \
    "2" \
    "SELECT id FROM numbers WHERE i = '1' AND nn = 6;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_where_and_predicates_expectations: ok"
