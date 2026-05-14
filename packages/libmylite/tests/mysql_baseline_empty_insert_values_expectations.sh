#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_empty_insert_values_$$"

fail() {
    printf '%s\n' "mysql_baseline_empty_insert_values_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
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

defaults_expected=$(cat <<'EXPECTED'
1	0
10	NULL	x	1.25	2024-01-02
1	0
10	NULL	x	1.25	2024-01-02
2	0
2	10:x,10:x
EXPECTED
)
expect_output \
    "explicit and omitted empty values insert defaults" \
    "$defaults_expected" \
    "CREATE TABLE defaults_t ("\
"id INT NOT NULL DEFAULT 10, "\
"n INT NULL DEFAULT NULL, "\
"s VARCHAR(10) NOT NULL DEFAULT 'x', "\
"d DECIMAL(5,2) DEFAULT 1.25, "\
"dt DATE DEFAULT '2024-01-02') ENGINE=InnoDB; "\
"INSERT INTO defaults_t () VALUES (); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, IFNULL(n, 'NULL'), s, d, dt FROM defaults_t; "\
"TRUNCATE defaults_t; "\
"INSERT INTO defaults_t VALUES (); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, IFNULL(n, 'NULL'), s, d, dt FROM defaults_t; "\
"TRUNCATE defaults_t; "\
"INSERT INTO defaults_t () VALUES (), (); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COUNT(*), GROUP_CONCAT(CONCAT(id, ':', s) ORDER BY id, s) FROM defaults_t;" \
    "$DATABASE"

auto_increment_expected=$(cat <<'EXPECTED'
2	0	1
1	5
2	5
1	3
1	5
2	5
3	5
EXPECTED
)
expect_output \
    "empty values generate auto increment" \
    "$auto_increment_expected" \
    "CREATE TABLE ai_t ("\
"id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "\
"v INT NOT NULL DEFAULT 5) ENGINE=InnoDB; "\
"INSERT INTO ai_t () VALUES (), (); "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"SELECT id, v FROM ai_t ORDER BY id; "\
"INSERT INTO ai_t VALUES (); "\
"SELECT ROW_COUNT(), LAST_INSERT_ID(); "\
"SELECT id, v FROM ai_t ORDER BY id;" \
    "$DATABASE"

expression_default_expected=$(cat <<'EXPECTED'
3	7	NULL
EXPECTED
)
expect_output \
    "empty values materialize expression defaults" \
    "$expression_default_expected" \
    "CREATE TABLE expr_t ("\
"a INT NOT NULL DEFAULT (1 + 2), "\
"b INT DEFAULT ((2 * 3) + 1), "\
"c INT DEFAULT (NULL)) ENGINE=InnoDB; "\
"INSERT INTO expr_t () VALUES (); "\
"SELECT a, b, IFNULL(c, 'NULL') FROM expr_t;" \
    "$DATABASE"

expect_error \
    "strict no default explicit empty values" \
    1364 \
    HY000 \
    "Field 'id' doesn't have a default value" \
    "CREATE TABLE no_default_t(id INT NOT NULL, n INT NULL) ENGINE=InnoDB; "\
"INSERT INTO no_default_t () VALUES ();" \
    "$DATABASE"

ignore_expected=$(cat <<'EXPECTED'
1	3	0	NULL	[]	0000-00-00
EXPECTED
)
expect_output \
    "insert ignore empty values adjustments" \
    "$ignore_expected" \
    "CREATE TABLE ignore_t ("\
"id INT NOT NULL, n INT NULL, s VARCHAR(5) NOT NULL, d DATE NOT NULL) ENGINE=InnoDB; "\
"INSERT IGNORE INTO ignore_t () VALUES (); "\
"SELECT ROW_COUNT(), @@warning_count, id, IFNULL(n, 'NULL'), CONCAT('[', s, ']'), d "\
"FROM ignore_t;" \
    "$DATABASE"

ignore_warnings_expected="Warning	1364	Field 'id' doesn't have a default value
Warning	1364	Field 's' doesn't have a default value
Warning	1364	Field 'd' doesn't have a default value"
expect_output \
    "insert ignore empty values warnings" \
    "$ignore_warnings_expected" \
    "CREATE TABLE ignore_warnings_t ("\
"id INT NOT NULL, n INT NULL, s VARCHAR(5) NOT NULL, d DATE NOT NULL) ENGINE=InnoDB; "\
"INSERT IGNORE INTO ignore_warnings_t () VALUES (); SHOW WARNINGS;" \
    "$DATABASE"

replace_expected=$(cat <<'EXPECTED'
1	0	10	x
1	0	2	10:x,10:x
EXPECTED
)
expect_output \
    "replace empty values no-key insert equivalent" \
    "$replace_expected" \
    "CREATE TABLE replace_t ("\
"id INT NOT NULL DEFAULT 10, s VARCHAR(5) NOT NULL DEFAULT 'x') ENGINE=InnoDB; "\
"REPLACE INTO replace_t () VALUES (); "\
"SELECT ROW_COUNT(), @@warning_count, id, s FROM replace_t; "\
"REPLACE INTO replace_t VALUES (); "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), "\
"GROUP_CONCAT(CONCAT(id, ':', s) ORDER BY id, s) FROM replace_t;" \
    "$DATABASE"

duplicate_expected=$(cat <<'EXPECTED'
insert	1	0
update_literal	2	0	20
update_default	2	0	10
update_values	0	1	10
EXPECTED
)
expect_output \
    "duplicate update with empty values" \
    "$duplicate_expected" \
    "CREATE TABLE duplicate_t ("\
"id INT NOT NULL DEFAULT 1 PRIMARY KEY, "\
"v INT NOT NULL DEFAULT 10) ENGINE=InnoDB; "\
"INSERT INTO duplicate_t () VALUES (); "\
"SELECT 'insert', ROW_COUNT(), @@warning_count; "\
"INSERT INTO duplicate_t () VALUES () ON DUPLICATE KEY UPDATE v = 20; "\
"SELECT 'update_literal', ROW_COUNT(), @@warning_count, v FROM duplicate_t; "\
"INSERT INTO duplicate_t VALUES () ON DUPLICATE KEY UPDATE v = DEFAULT; "\
"SELECT 'update_default', ROW_COUNT(), @@warning_count, v FROM duplicate_t; "\
"INSERT INTO duplicate_t () VALUES () ON DUPLICATE KEY UPDATE v = VALUES(v); "\
"SELECT 'update_values', ROW_COUNT(), @@warning_count, v FROM duplicate_t;" \
    "$DATABASE"

expect_error \
    "explicit empty target nonempty row mismatch" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 1" \
    "INSERT INTO defaults_t () VALUES (1);" \
    "$DATABASE"

expect_error \
    "omitted target empty then nonempty row mismatch" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 2" \
    "INSERT INTO defaults_t VALUES (), (1, NULL, 'y', 2.00, '2024-01-03');" \
    "$DATABASE"

expect_error \
    "omitted target nonempty then empty row mismatch" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 2" \
    "INSERT INTO defaults_t VALUES (1, NULL, 'y', 2.00, '2024-01-03'), ();" \
    "$DATABASE"

expect_error \
    "explicit empty insert select mismatch" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 1" \
    "INSERT INTO defaults_t () SELECT 1;" \
    "$DATABASE"

value_synonym_expected=$(cat <<'EXPECTED'
1	0	10	x
EXPECTED
)
expect_output \
    "value synonym upstream behavior" \
    "$value_synonym_expected" \
    "TRUNCATE defaults_t; INSERT INTO defaults_t () VALUE (); "\
"SELECT ROW_COUNT(), @@warning_count, id, s FROM defaults_t;" \
    "$DATABASE"

row_constructor_expected=$(cat <<'EXPECTED'
1	0	10	x
EXPECTED
)
expect_output \
    "row constructor upstream behavior" \
    "$row_constructor_expected" \
    "TRUNCATE defaults_t; INSERT INTO defaults_t VALUES ROW(); "\
"SELECT ROW_COUNT(), @@warning_count, id, s FROM defaults_t;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_empty_insert_values_expectations: ok"
