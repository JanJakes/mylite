#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_expression_collate_predicates_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_expression_collate_predicates_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
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
run_mysql \
    "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; "\
"USE ${DATABASE}; SET NAMES utf8mb4; "\
"CREATE TABLE people ("\
"id INT NOT NULL, firstname VARCHAR(20), "\
"latin1_name VARCHAR(20) CHARACTER SET latin1 COLLATE latin1_swedish_ci"\
") CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; "\
"INSERT INTO people VALUES "\
"(1, 'john', 'a'), (2, 'John', 'A'), (3, 'JOHN', 'b'), "\
"(4, 'joel', 'B'), (5, NULL, NULL); "\
"CREATE TABLE unicode_names (id INT NOT NULL, name VARCHAR(40)) "\
"CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; "\
"INSERT INTO unicode_names VALUES "\
"(1, 'é'), (2, 'e'), (3, 'é'), (4, 'A'), (5, 'a'), (6, 'a '), "\
"(7, 'ß'), (8, 'ss'), (9, 'æ'), (10, 'ae'); "\
"CREATE TABLE unique_unicode (name VARCHAR(40) UNIQUE) "\
"CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; "\
"INSERT INTO unique_unicode VALUES ('é'); "\
"CREATE TABLE unique_positioned (name VARCHAR(40) COLLATE utf8mb4_0900_as_ci UNIQUE) "\
"CHARACTER SET utf8mb4; "\
"CREATE TABLE positioned_accents (id INT, name VARCHAR(20)) "\
"CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; "\
"INSERT INTO positioned_accents VALUES (1, 'áa'), (2, 'aá'), (3, 'áa');" >/dev/null

expect_output \
    "right-side ai_ci collation equality" \
    "1,2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM people "\
"WHERE firstname = 'john' COLLATE utf8mb4_0900_ai_ci;" \
    "$DATABASE"

expect_output \
    "right-side as_cs collation equality" \
    "1" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM people "\
"WHERE firstname = 'john' COLLATE utf8mb4_0900_as_cs;" \
    "$DATABASE"

expect_output \
    "left-side as_cs collation equality" \
    "1" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM people "\
"WHERE firstname COLLATE utf8mb4_0900_as_cs = 'john';" \
    "$DATABASE"

expect_output \
    "left-side ai_ci collation equality" \
    "1,2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM people "\
"WHERE firstname COLLATE utf8mb4_0900_ai_ci = 'john';" \
    "$DATABASE"

expect_output \
    "right-side ai_ci LIKE pattern" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM people "\
"WHERE firstname LIKE 'jo%' COLLATE utf8mb4_0900_ai_ci;" \
    "$DATABASE"

expect_output \
    "left-side as_cs LIKE pattern" \
    "1,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM people "\
"WHERE firstname COLLATE utf8mb4_0900_as_cs LIKE 'jo%';" \
    "$DATABASE"

expect_output \
    "order by explicit as_cs collation" \
    "5:NULL,4:joel,1:john,2:John,3:JOHN" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', COALESCE(firstname, 'NULL')) "\
"ORDER BY firstname COLLATE utf8mb4_0900_as_cs, id SEPARATOR ',') FROM people;" \
    "$DATABASE"

expect_output \
    "latin1 converted literal collation" \
    "1,2" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM people "\
"WHERE latin1_name = CONVERT('a' USING latin1) COLLATE latin1_swedish_ci;" \
    "$DATABASE"

expect_output \
    "Unicode ai_ci accent and normalization equality" \
    "1,2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM unicode_names "\
"WHERE name = 'e' COLLATE utf8mb4_0900_ai_ci;" \
    "$DATABASE"

expect_output \
    "Unicode as_ci canonical equality" \
    "1,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM unicode_names "\
"WHERE name = 'é' COLLATE utf8mb4_0900_as_ci;" \
    "$DATABASE"

expect_output \
    "Unicode as_ci accent position" \
    "1,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM positioned_accents "\
"WHERE name = 'áa' COLLATE utf8mb4_0900_as_ci;" \
    "$DATABASE"

expect_output \
    "Unicode as_cs canonical equality" \
    "1,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM unicode_names "\
"WHERE name = 'é' COLLATE utf8mb4_0900_as_cs;" \
    "$DATABASE"

expect_output \
    "Unicode binary byte equality" \
    "1" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM unicode_names "\
"WHERE name = 'é' COLLATE utf8mb4_0900_bin;" \
    "$DATABASE"

expect_output \
    "Unicode default collation grouping" \
    "1:3,4:2,6:1,7:2,9:2" \
    "SELECT GROUP_CONCAT(CONCAT(min_id, ':', row_count) ORDER BY min_id) FROM ("\
"SELECT MIN(id) AS min_id, COUNT(*) AS row_count FROM unicode_names GROUP BY name"\
") AS grouped_names;" \
    "$DATABASE"

expect_error \
    "Unicode unique key collation" \
    1062 \
    23000 \
    "Duplicate entry 'e' for key 'unique_unicode.name'" \
    "INSERT INTO unique_unicode VALUES ('e');" \
    "$DATABASE"

run_mysql "SET NAMES utf8mb4; INSERT INTO unique_positioned VALUES ('áa');" "$DATABASE" >/dev/null
expect_error \
    "Unicode canonical accent unique key" \
    1062 \
    23000 \
    "Duplicate entry 'áa' for key 'unique_positioned.name'" \
    "SET NAMES utf8mb4; INSERT INTO unique_positioned VALUES ('áa');" \
    "$DATABASE"
run_mysql "SET NAMES utf8mb4; INSERT INTO unique_positioned VALUES ('aá');" "$DATABASE" >/dev/null

expect_error \
    "utf8mb4 literal rejects latin1 collation" \
    1253 \
    42000 \
    "COLLATION 'latin1_swedish_ci' is not valid for CHARACTER SET 'utf8mb4'" \
    "SELECT id FROM people WHERE firstname = 'john' COLLATE latin1_swedish_ci;" \
    "$DATABASE"

expect_error \
    "unknown collation" \
    1273 \
    HY000 \
    "Unknown collation: 'utf8mb4_not_real_ci'" \
    "SELECT id FROM people WHERE firstname = 'john' COLLATE utf8mb4_not_real_ci;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_expression_collate_predicates_expectations: ok"
