#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_column_drop_default_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_alter_column_drop_default_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    set +e
    output=$(printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@")
    status_code=$?
    set -e
    printf '%s\n' "$output" | sed 's/[[:blank:]]*$//'
    return "$status_code"
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
        fail "$label: expected MySQL to accept deferred syntax, got [$output]"
    fi
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

expect_error \
    "drop default without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE numbers ALTER n DROP DEFAULT;"

expect_error \
    "drop default qualified unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE ${MISSING_DATABASE}.numbers ALTER n DROP DEFAULT;"

expect_error \
    "drop default unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_numbers' doesn't exist" \
    "ALTER TABLE missing_numbers ALTER n DROP DEFAULT;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE ${DATABASE}.qualified_numbers (id INT NOT NULL, n INT DEFAULT 2); "\
"INSERT INTO ${DATABASE}.qualified_numbers (id) VALUES (1);" \
    >/dev/null
qualified_expected=$(cat <<'EXPECTED'
0	0
n	int	YES		NULL
qualified_numbers	CREATE TABLE `qualified_numbers` (
  `id` int NOT NULL,
  `n` int
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "schema-qualified drop default without selected schema" \
    "$qualified_expected" \
    "ALTER TABLE ${DATABASE}.qualified_numbers ALTER COLUMN n DROP DEFAULT; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM ${DATABASE}.qualified_numbers LIKE 'n'; "\
"SHOW CREATE TABLE ${DATABASE}.qualified_numbers;"

expect_error \
    "qualified dropped nullable default rejects omitted insert" \
    1364 \
    HY000 \
    "Field 'n' doesn't have a default value" \
    "INSERT INTO ${DATABASE}.qualified_numbers (id) VALUES (2);"

expect_output \
    "set default after qualified drop restores omitted insert" \
    "2:5" \
    "ALTER TABLE ${DATABASE}.qualified_numbers ALTER n SET DEFAULT 5; "\
"INSERT INTO ${DATABASE}.qualified_numbers (id) VALUES (2); "\
"SELECT CONCAT(id, ':', n) FROM ${DATABASE}.qualified_numbers WHERE id = 2;"

run_mysql \
    "CREATE TABLE numbers ("\
"id INT NOT NULL, v INT DEFAULT 1, nullable_i INT DEFAULT NULL, "\
"nn INT NOT NULL DEFAULT 2, u INT UNSIGNED DEFAULT 4, "\
"bu BIGINT UNSIGNED DEFAULT 5, b BOOL DEFAULT FALSE); "\
"INSERT INTO numbers (id) VALUES (1);" \
    "$DATABASE" >/dev/null

drop_expected=$(cat <<'EXPECTED'
0	0
v	int	YES		NULL
nullable_i	int	YES		NULL
nn	int	NO		NULL
b	tinyint(1)	YES		NULL
numbers	CREATE TABLE `numbers` (
  `id` int NOT NULL,
  `v` int,
  `nullable_i` int,
  `nn` int NOT NULL,
  `u` int unsigned DEFAULT '4',
  `bu` bigint unsigned DEFAULT '5',
  `b` tinyint(1)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1:1:NULL:2:0
EXPECTED
)
expect_output \
    "drop default mutates metadata only" \
    "$drop_expected" \
    "ALTER TABLE numbers ALTER v DROP DEFAULT; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE numbers ALTER nullable_i DROP DEFAULT; "\
"ALTER TABLE numbers ALTER COLUMN nn DROP DEFAULT; "\
"ALTER TABLE numbers ALTER b DROP DEFAULT; "\
"SHOW COLUMNS FROM numbers LIKE 'v'; "\
"SHOW COLUMNS FROM numbers LIKE 'nullable_i'; "\
"SHOW COLUMNS FROM numbers LIKE 'nn'; "\
"SHOW COLUMNS FROM numbers LIKE 'b'; "\
"SHOW CREATE TABLE numbers; "\
"SELECT CONCAT(id, ':', v, ':', IFNULL(nullable_i, 'NULL'), ':', nn, ':', b) FROM numbers;" \
    "$DATABASE"

expect_error \
    "dropped nullable integer default rejects omitted insert" \
    1364 \
    HY000 \
    "Field 'v' doesn't have a default value" \
    "INSERT INTO numbers (id) VALUES (2);" \
    "$DATABASE"

expect_error \
    "dropped explicit default null rejects omitted insert" \
    1364 \
    HY000 \
    "Field 'nullable_i' doesn't have a default value" \
    "INSERT INTO numbers (id, v) VALUES (2, 2);" \
    "$DATABASE"

expect_error \
    "dropped not-null default rejects omitted insert" \
    1364 \
    HY000 \
    "Field 'nn' doesn't have a default value" \
    "INSERT INTO numbers (id, v, nullable_i) VALUES (2, 2, NULL);" \
    "$DATABASE"

expect_error \
    "drop default unknown column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'numbers'" \
    "ALTER TABLE numbers ALTER missing DROP DEFAULT;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred multi-action drop default" \
    "ALTER TABLE numbers ALTER u DROP DEFAULT, ALTER bu DROP DEFAULT;" \
    "$DATABASE"
