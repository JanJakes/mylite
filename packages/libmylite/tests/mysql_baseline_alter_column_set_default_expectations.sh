#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_column_set_default_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_alter_column_set_default_expectations: $1" >&2
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
    "set default without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE numbers ALTER n SET DEFAULT 1;"

expect_error \
    "set default qualified unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE ${MISSING_DATABASE}.numbers ALTER n SET DEFAULT 1;"

expect_error \
    "set default unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_numbers' doesn't exist" \
    "ALTER TABLE missing_numbers ALTER n SET DEFAULT 1;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE ${DATABASE}.qualified_numbers (id INT NOT NULL, n INT DEFAULT 2); "\
"INSERT INTO ${DATABASE}.qualified_numbers (id) VALUES (1);" \
    >/dev/null
qualified_expected=$(cat <<'EXPECTED'
0	0
n	int	YES		5
1:2,2:5
EXPECTED
)
expect_output \
    "schema-qualified set default without selected schema" \
    "$qualified_expected" \
    "ALTER TABLE ${DATABASE}.qualified_numbers ALTER COLUMN n SET DEFAULT 5; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM ${DATABASE}.qualified_numbers LIKE 'n'; "\
"INSERT INTO ${DATABASE}.qualified_numbers (id) VALUES (2); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', n) ORDER BY id) FROM ${DATABASE}.qualified_numbers;"

run_mysql \
    "CREATE TABLE numbers ("\
"id INT NOT NULL, v INT DEFAULT 1, nullable_i INT DEFAULT 3, "\
"nn INT NOT NULL DEFAULT 2, u INT UNSIGNED DEFAULT 4, "\
"bu BIGINT UNSIGNED DEFAULT 5, b BOOL DEFAULT FALSE); "\
"INSERT INTO numbers (id) VALUES (1), (2);" \
    "$DATABASE" >/dev/null

mutation_expected=$(cat <<'EXPECTED'
0	0
v	int	YES		8
1:1:3:2:4:5:0,2:1:3:2:4:5:0,3:8:3:2:4:5:0
0	0
b	tinyint(1)	YES		1
0	0
nullable_i	int	YES		NULL
0	0
nn	int	NO		7
1:1:3:2:4:5:0,2:1:3:2:4:5:0,3:8:3:2:4:5:0,4:8:NULL:7:4:5:1
EXPECTED
)
expect_output \
    "set default mutates metadata and future omitted inserts only" \
    "$mutation_expected" \
    "ALTER TABLE numbers ALTER v SET DEFAULT +8; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM numbers LIKE 'v'; "\
"INSERT INTO numbers (id) VALUES (3); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v, ':', IFNULL(nullable_i, 'NULL'), ':', nn, ':', u, ':', bu, ':', b) ORDER BY id) FROM numbers; "\
"ALTER TABLE numbers ALTER COLUMN b SET DEFAULT TRUE; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM numbers LIKE 'b'; "\
"ALTER TABLE numbers ALTER nullable_i SET DEFAULT NULL; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM numbers LIKE 'nullable_i'; "\
"ALTER TABLE numbers ALTER COLUMN nn SET DEFAULT 7; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM numbers LIKE 'nn'; "\
"INSERT INTO numbers (id) VALUES (4); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v, ':', IFNULL(nullable_i, 'NULL'), ':', nn, ':', u, ':', bu, ':', b) ORDER BY id) FROM numbers;" \
    "$DATABASE"

boundary_expected=$(cat <<'EXPECTED'
ti	tinyint	YES		-128
tiu	tinyint unsigned	YES		255
si	smallint	YES		-32768
siu	smallint unsigned	YES		65535
mi	mediumint	YES		-8388608
miu	mediumint unsigned	YES		16777215
i	int	YES		-2147483648
iu	int unsigned	YES		4294967295
bi	bigint	YES		-9223372036854775808
bu	bigint unsigned	YES		9223372036854775807
EXPECTED
)
expect_output \
    "set default integer boundaries render in metadata" \
    "$boundary_expected" \
    "CREATE TABLE boundaries ("\
"ti TINYINT, tiu TINYINT UNSIGNED, si SMALLINT, siu SMALLINT UNSIGNED, "\
"mi MEDIUMINT, miu MEDIUMINT UNSIGNED, i INT, iu INT UNSIGNED, "\
"bi BIGINT, bu BIGINT UNSIGNED); "\
"ALTER TABLE boundaries ALTER ti SET DEFAULT -128; "\
"ALTER TABLE boundaries ALTER tiu SET DEFAULT 255; "\
"ALTER TABLE boundaries ALTER si SET DEFAULT -32768; "\
"ALTER TABLE boundaries ALTER siu SET DEFAULT 65535; "\
"ALTER TABLE boundaries ALTER mi SET DEFAULT -8388608; "\
"ALTER TABLE boundaries ALTER miu SET DEFAULT 16777215; "\
"ALTER TABLE boundaries ALTER i SET DEFAULT -2147483648; "\
"ALTER TABLE boundaries ALTER iu SET DEFAULT 4294967295; "\
"ALTER TABLE boundaries ALTER bi SET DEFAULT -9223372036854775808; "\
"ALTER TABLE boundaries ALTER bu SET DEFAULT 9223372036854775807; "\
"SHOW COLUMNS FROM boundaries;" \
    "$DATABASE"

expect_error \
    "set default unknown column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'numbers'" \
    "ALTER TABLE numbers ALTER missing SET DEFAULT 1;" \
    "$DATABASE"

expect_error \
    "set null default on not null column" \
    1067 \
    42000 \
    "Invalid default value for 'nn'" \
    "ALTER TABLE numbers ALTER nn SET DEFAULT NULL;" \
    "$DATABASE"

expect_error \
    "set negative unsigned default" \
    1067 \
    42000 \
    "Invalid default value for 'u'" \
    "ALTER TABLE numbers ALTER u SET DEFAULT -1;" \
    "$DATABASE"

expect_error \
    "set int default above range" \
    1067 \
    42000 \
    "Invalid default value for 'v'" \
    "ALTER TABLE numbers ALTER v SET DEFAULT 2147483648;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred expression defaults" \
    "ALTER TABLE numbers ALTER v SET DEFAULT (1 + 1);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred string-coerced defaults" \
    "ALTER TABLE numbers ALTER v SET DEFAULT '12';" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred multi-action alter" \
    "ALTER TABLE numbers ALTER v SET DEFAULT 13, ALTER nullable_i SET DEFAULT 14;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred drop default" \
    "ALTER TABLE numbers ALTER nullable_i DROP DEFAULT;" \
    "$DATABASE"
