#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_integer_default_literals_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_integer_default_literals_expectations: $1" >&2
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
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

create_expected=$(cat <<'EXPECTED'
defaults	CREATE TABLE `defaults` (
  `a` int DEFAULT '5',
  `b` int NOT NULL DEFAULT '-7',
  `c` bigint unsigned DEFAULT '9223372036854775807',
  `d` tinyint(1) DEFAULT '1',
  `e` tinyint(1) DEFAULT '0',
  `f` int DEFAULT '9',
  `g` int DEFAULT '0',
  `h` int NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
a	int	YES		5	
b	int	NO		-7	
c	bigint unsigned	YES		9223372036854775807	
d	tinyint(1)	YES		1	
e	tinyint(1)	YES		0	
f	int	YES		9	
g	int	YES		0	
h	int	NO		NULL	
5:-7:9223372036854775807:1:0:9:0:1	1	0
5:-7:9223372036854775807:1:0:9:0:1,5:-7:9223372036854775807:1:0:9:0:2	1	0
NULL:3
EXPECTED
)
expect_output \
    "create table stores and uses integer defaults" \
    "$create_expected" \
    "CREATE TABLE defaults ("\
"a INT DEFAULT 5, b INT NOT NULL DEFAULT -7, "\
"c BIGINT UNSIGNED DEFAULT 9223372036854775807, "\
"d BOOL DEFAULT TRUE, e BOOL DEFAULT FALSE, f INT DEFAULT +9, "\
"g INT NULL DEFAULT 0, h INT NOT NULL); "\
"SHOW CREATE TABLE defaults; "\
"SHOW COLUMNS FROM defaults; "\
"INSERT INTO defaults (h) VALUES (1); "\
"SELECT CONCAT(a, ':', b, ':', c, ':', d, ':', e, ':', f, ':', g, ':', h), "\
"ROW_COUNT(), @@warning_count FROM defaults WHERE h = 1; "\
"INSERT INTO defaults SET h = 2; "\
"SELECT GROUP_CONCAT(CONCAT(a, ':', b, ':', c, ':', d, ':', e, ':', f, ':', g, ':', h) ORDER BY h), "\
"ROW_COUNT(), @@warning_count FROM defaults WHERE h IN (1, 2); "\
"INSERT INTO defaults (a, h) VALUES (NULL, 3); "\
"SELECT CONCAT(IFNULL(a, 'NULL'), ':', h) FROM defaults WHERE h = 3;" \
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
    "integer default boundaries render in metadata" \
    "$boundary_expected" \
    "CREATE TABLE boundaries ("\
"ti TINYINT DEFAULT -128, tiu TINYINT UNSIGNED DEFAULT 255, "\
"si SMALLINT DEFAULT -32768, siu SMALLINT UNSIGNED DEFAULT 65535, "\
"mi MEDIUMINT DEFAULT -8388608, miu MEDIUMINT UNSIGNED DEFAULT 16777215, "\
"i INT DEFAULT -2147483648, iu INT UNSIGNED DEFAULT 4294967295, "\
"bi BIGINT DEFAULT -9223372036854775808, "\
"bu BIGINT UNSIGNED DEFAULT 9223372036854775807); "\
"SHOW COLUMNS FROM boundaries;" \
    "$DATABASE"

add_expected=$(cat <<'EXPECTED'
0	0
1:11,2:11
1:11,2:11,3:11
0	0
1:12,2:12,3:12
EXPECTED
)
expect_output \
    "add column integer defaults backfill and apply later" \
    "$add_expected" \
    "CREATE TABLE add_target (id INT NOT NULL); INSERT INTO add_target VALUES (1), (2); "\
"ALTER TABLE add_target ADD COLUMN c INT DEFAULT 11; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', c) ORDER BY id) FROM add_target; "\
"INSERT INTO add_target (id) VALUES (3); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', c) ORDER BY id) FROM add_target; "\
"ALTER TABLE add_target ADD COLUMN nn INT NOT NULL DEFAULT 12; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', nn) ORDER BY id) FROM add_target;" \
    "$DATABASE"

modify_change_expected=$(cat <<'EXPECTED'
0	0
v	int	YES		8	
1:1,2:1,3:8
0	0
renamed	int	YES		9	
1:1,2:1,3:8,4:9
v	int	YES		NULL	
nn	int	NO		NULL	
EXPECTED
)
expect_output \
    "modify and change integer defaults affect future rows only" \
    "$modify_change_expected" \
    "CREATE TABLE mutate_target (id INT NOT NULL, v INT DEFAULT 1); "\
"INSERT INTO mutate_target (id) VALUES (1), (2); "\
"ALTER TABLE mutate_target MODIFY v INT DEFAULT 8; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM mutate_target LIKE 'v'; "\
"INSERT INTO mutate_target (id) VALUES (3); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM mutate_target; "\
"ALTER TABLE mutate_target CHANGE v renamed INT DEFAULT 9; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM mutate_target LIKE 'renamed'; "\
"INSERT INTO mutate_target (id) VALUES (4); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', renamed) ORDER BY id) FROM mutate_target; "\
"CREATE TABLE drop_default (id INT NOT NULL, v INT DEFAULT 5, nn INT NOT NULL DEFAULT 6); "\
"ALTER TABLE drop_default MODIFY v INT; "\
"SHOW COLUMNS FROM drop_default LIKE 'v'; "\
"ALTER TABLE drop_default MODIFY nn INT NOT NULL; "\
"SHOW COLUMNS FROM drop_default LIKE 'nn';" \
    "$DATABASE"

expect_output \
    "existing-table if-not-exists skips out-of-range integer default conversion" \
    "0	1" \
    "CREATE TABLE if_exists_target (id INT); "\
"CREATE TABLE IF NOT EXISTS if_exists_target (a INT DEFAULT 2147483648); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "tinyint default above range is invalid" \
    1067 \
    42000 \
    "Invalid default value for 'a'" \
    "CREATE TABLE tiny_bad (a TINYINT DEFAULT 128);" \
    "$DATABASE"

expect_error \
    "unsigned default above range is invalid" \
    1067 \
    42000 \
    "Invalid default value for 'a'" \
    "CREATE TABLE int_unsigned_bad (a INT UNSIGNED DEFAULT 4294967296);" \
    "$DATABASE"

expect_error \
    "negative unsigned default is invalid" \
    1067 \
    42000 \
    "Invalid default value for 'a'" \
    "CREATE TABLE unsigned_negative (a INT UNSIGNED DEFAULT -1);" \
    "$DATABASE"

expect_error \
    "alter add out-of-range default is invalid" \
    1067 \
    42000 \
    "Invalid default value for 'c'" \
    "CREATE TABLE alter_bad (id INT); ALTER TABLE alter_bad ADD c INT DEFAULT 2147483648;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts string default coercion outside MyLite scope" \
    "CREATE TABLE mysql_wider_string_default (a INT DEFAULT '5');" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts decimal default coercion outside MyLite scope" \
    "CREATE TABLE mysql_wider_decimal_default (a INT DEFAULT 1.0);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts hex default coercion outside MyLite scope" \
    "CREATE TABLE mysql_wider_hex_default (a INT DEFAULT 0x10);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts expression default outside MyLite scope" \
    "CREATE TABLE mysql_wider_expression_default (a INT DEFAULT (1 + 2));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts unsigned bigint default beyond MyLite physical range" \
    "CREATE TABLE mysql_wider_unsigned_bigint_default "\
"(a BIGINT UNSIGNED DEFAULT 9223372036854775808);" \
    "$DATABASE"

printf '%s\n' "baseline-integer-default-literals MySQL 8.4.9 expectations verified"
