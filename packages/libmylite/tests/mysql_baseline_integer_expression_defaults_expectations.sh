#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_integer_expression_defaults_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_integer_expression_defaults_expectations: $1" >&2
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

show_columns_expected=$(cat <<\EXPECTED
a	int	NO		(1 + 2)	DEFAULT_GENERATED
b	int	YES		((2 * 3) + 1)	DEFAULT_GENERATED
c	int	YES		(5 DIV 2)	DEFAULT_GENERATED
d	int	YES		(7 % 3)	DEFAULT_GENERATED
e	int	YES		(7 % 3)	DEFAULT_GENERATED
f	int	YES		(7 % 3)	DEFAULT_GENERATED
g	int	YES		-(-(7))	DEFAULT_GENERATED
h	int	YES		NULL	DEFAULT_GENERATED
EXPECTED
)
expect_output \
    "SHOW COLUMNS expression defaults" \
    "$show_columns_expected" \
    "CREATE TABLE expr_defaults ("\
"a INT NOT NULL DEFAULT (1 + 2), "\
"b INT DEFAULT ((2 * 3) + 1), "\
"c INT DEFAULT (5 DIV 2), "\
"d INT DEFAULT (7 % 3), "\
"e INT DEFAULT (7 MOD 3), "\
"f INT DEFAULT (MOD(7,3)), "\
"g INT DEFAULT (-(-7)), "\
"h INT DEFAULT (NULL)); "\
"SHOW COLUMNS FROM expr_defaults;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
expr_defaults	CREATE TABLE `expr_defaults` (
  `a` int NOT NULL DEFAULT ((1 + 2)),
  `b` int DEFAULT (((2 * 3) + 1)),
  `c` int DEFAULT ((5 DIV 2)),
  `d` int DEFAULT ((7 % 3)),
  `e` int DEFAULT ((7 % 3)),
  `f` int DEFAULT ((7 % 3)),
  `g` int DEFAULT (-(-(7))),
  `h` int DEFAULT (NULL)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "SHOW CREATE TABLE expression defaults" \
    "$show_create_expected" \
    "SHOW CREATE TABLE expr_defaults;" \
    "$DATABASE"

information_schema_expected=$(cat <<\EXPECTED
a	(1 + 2)	DEFAULT_GENERATED
b	((2 * 3) + 1)	DEFAULT_GENERATED
c	(5 DIV 2)	DEFAULT_GENERATED
d	(7 % 3)	DEFAULT_GENERATED
e	(7 % 3)	DEFAULT_GENERATED
f	(7 % 3)	DEFAULT_GENERATED
g	-(-(7))	DEFAULT_GENERATED
h	NULL	DEFAULT_GENERATED
EXPECTED
)
expect_output \
    "INFORMATION_SCHEMA expression defaults" \
    "$information_schema_expected" \
    "SELECT COLUMN_NAME, COLUMN_DEFAULT, EXTRA FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='expr_defaults' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

default_values_expected=$(cat <<\EXPECTED
3	7	2	1	1	1	7	NULL	0	1
EXPECTED
)
expect_output \
    "omitted column expression defaults materialize" \
    "$default_values_expected" \
    "INSERT INTO expr_defaults () VALUES (); "\
"SELECT a,b,c,d,e,f,g,h,@@warning_count,ROW_COUNT() FROM expr_defaults;" \
    "$DATABASE"

integer_family_expected=$(cat <<\EXPECTED
127	-128	255	2147483647	4294967295	9223372036854775807	0
EXPECTED
)
expect_output \
    "integer family expression defaults" \
    "$integer_family_expected" \
    "CREATE TABLE integer_family ("\
"a TINYINT DEFAULT (127), "\
"b TINYINT DEFAULT (-128), "\
"c TINYINT UNSIGNED DEFAULT (255), "\
"d INTEGER DEFAULT (2147483647), "\
"e INT UNSIGNED DEFAULT (4294967295), "\
"f BIGINT UNSIGNED DEFAULT (9223372036854775807), "\
"g BIGINT DEFAULT (1 - 1)); "\
"INSERT INTO integer_family () VALUES (); "\
"SELECT a,b,c,d,e,f,g FROM integer_family;" \
    "$DATABASE"

alter_expected=$(cat <<\EXPECTED
t	CREATE TABLE `t` (
  `a` int DEFAULT ((2 * 5))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
10	0	1
EXPECTED
)
expect_output \
    "ALTER SET DEFAULT expression" \
    "$alter_expected" \
    "CREATE TABLE t (a INT); "\
"ALTER TABLE t ALTER a SET DEFAULT (2 * 5); "\
"SHOW CREATE TABLE t; "\
"INSERT INTO t () VALUES (); "\
"SELECT a, @@warning_count, ROW_COUNT() FROM t;" \
    "$DATABASE"

alter_definition_paths_expected=$(cat <<\EXPECTED
1	1	5
2	1	5
added	int	YES		(2 + 3)	DEFAULT_GENERATED
1	1
2	1
3	20
v	int	YES		(4 * 5)	DEFAULT_GENERATED
1	1
2	1
3	20
4	13
changed	int	YES		(6 + 7)	DEFAULT_GENERATED
EXPECTED
)
expect_output \
    "ALTER ADD MODIFY CHANGE expression defaults" \
    "$alter_definition_paths_expected" \
    "CREATE TABLE alter_definition_paths (id INT NOT NULL, v INT DEFAULT 1); "\
"INSERT INTO alter_definition_paths (id) VALUES (1); "\
"ALTER TABLE alter_definition_paths ADD COLUMN added INT DEFAULT (2 + 3); "\
"INSERT INTO alter_definition_paths (id) VALUES (2); "\
"SELECT id, v, added FROM alter_definition_paths ORDER BY id; "\
"SHOW COLUMNS FROM alter_definition_paths LIKE 'added'; "\
"ALTER TABLE alter_definition_paths MODIFY v INT DEFAULT (4 * 5); "\
"INSERT INTO alter_definition_paths (id) VALUES (3); "\
"SELECT id, v FROM alter_definition_paths ORDER BY id; "\
"SHOW COLUMNS FROM alter_definition_paths LIKE 'v'; "\
"ALTER TABLE alter_definition_paths CHANGE v changed INT DEFAULT (6 + 7); "\
"INSERT INTO alter_definition_paths (id) VALUES (4); "\
"SELECT id, changed FROM alter_definition_paths ORDER BY id; "\
"SHOW COLUMNS FROM alter_definition_paths LIKE 'changed';" \
    "$DATABASE"

ctas_expected=$(printf '%s\n%s\t\n%s' \
    "a	int	YES		(1 + 2)	DEFAULT_GENERATED" \
    "b	int	YES		NULL" \
    "3	9")
expect_output \
    "CREATE TABLE SELECT expression default metadata" \
    "$ctas_expected" \
    "CREATE TABLE ctas_src (a INT DEFAULT (1 + 2), b INT); "\
"INSERT INTO ctas_src (b) VALUES (9); "\
"CREATE TABLE ctas_copy AS SELECT a, b FROM ctas_src; "\
"SHOW COLUMNS FROM ctas_copy; "\
"SELECT a, b FROM ctas_copy;" \
    "$DATABASE"

broader_mysql_expression_defaults_expected=$(cat <<\EXPECTED
a	int	YES		`base`	DEFAULT_GENERATED
b	int	YES		_latin1\'1\'	DEFAULT_GENERATED
c	int	YES		1.2	DEFAULT_GENERATED
d	int	YES		1e0	DEFAULT_GENERATED
e	int	YES		0x01	DEFAULT_GENERATED
f	int	YES		0x01	DEFAULT_GENERATED
g	int	YES		abs(-(1))	DEFAULT_GENERATED
h	int	YES		cast(1 as signed)	DEFAULT_GENERATED
i	int	YES		((0 <> 1) and (0 <> 1))	DEFAULT_GENERATED
4	4	1	1	1	1	1	1	1	1
EXPECTED
)
expect_output \
    "MySQL accepts broader expression defaults deferred by MyLite" \
    "$broader_mysql_expression_defaults_expected" \
    "CREATE TABLE broader_mysql_defaults ("\
"base INT DEFAULT 4, "\
"a INT DEFAULT (base), "\
"b INT DEFAULT ('1'), "\
"c INT DEFAULT (1.2), "\
"d INT DEFAULT (1e0), "\
"e INT DEFAULT (0x1), "\
"f INT DEFAULT (b'1'), "\
"g INT DEFAULT (ABS(-1)), "\
"h INT DEFAULT (CAST(1 AS SIGNED)), "\
"i INT DEFAULT (1 AND 1)); "\
"SHOW COLUMNS FROM broader_mysql_defaults WHERE Field <> 'base'; "\
"INSERT INTO broader_mysql_defaults () VALUES (); "\
"SELECT base,a,b,c,d,e,f,g,h,i FROM broader_mysql_defaults;" \
    "$DATABASE"

not_null_null_expected=$(cat <<\EXPECTED
a	int	NO		NULL	DEFAULT_GENERATED
EXPECTED
)
expect_output \
    "NOT NULL DEFAULT NULL expression metadata" \
    "$not_null_null_expected" \
    "CREATE TABLE not_null_null (a INT NOT NULL DEFAULT (NULL)); "\
"SHOW COLUMNS FROM not_null_null;" \
    "$DATABASE"

expect_error \
    "NOT NULL DEFAULT NULL expression materialization" \
    1048 \
    "23000" \
    "Column 'a' cannot be null" \
    "INSERT INTO not_null_null () VALUES ();" \
    "$DATABASE"

expect_error \
    "unparenthesized expression default syntax" \
    1064 \
    "42000" \
    "near '+ 2)' at line 1" \
    "CREATE TABLE bad_syntax (a INT DEFAULT 1 + 2);" \
    "$DATABASE"

expect_output \
    "MySQL accepts out-of-range expression default at DDL time" \
    "a	tinyint	YES		(127 + 1)	DEFAULT_GENERATED" \
    "CREATE TABLE out_range (a TINYINT DEFAULT (127 + 1)); SHOW COLUMNS FROM out_range;" \
    "$DATABASE"

expect_error \
    "MySQL rejects out-of-range expression default on materialization" \
    1264 \
    "22003" \
    "Out of range value for column 'a' at row 1" \
    "INSERT INTO out_range () VALUES ();" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_integer_expression_defaults_expectations: ok"
