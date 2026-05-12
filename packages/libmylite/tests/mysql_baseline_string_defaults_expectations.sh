#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_string_defaults_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_string_defaults_expectations: $1" >&2
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
        fail "$label: expected MySQL to accept deferred behavior, got [$output]"
    fi
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

case "$(run_mysql "SELECT @@sql_mode;")" in
    *STRICT_TRANS_TABLES*) ;;
    *) fail "expected strict default sql_mode" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

show_columns_expected=$(cat <<\EXPECTED
id	int	NO		NULL	
vc	varchar(3)	YES		ab	
vnn	varchar(3)	NO			
v0	varchar(0)	YES			
c	char(3)	YES		xy	
cnn	char(3)	NO		q	
EXPECTED
)
expect_output \
    "show columns renders char and varchar defaults" \
    "$show_columns_expected" \
    "CREATE TABLE defaults ("\
"id INT NOT NULL, "\
"vc VARCHAR(3) DEFAULT 'ab', "\
"vnn VARCHAR(3) NOT NULL DEFAULT '', "\
"v0 VARCHAR(0) DEFAULT '', "\
"c CHAR(3) DEFAULT 'xy ', "\
"cnn CHAR(3) NOT NULL DEFAULT 'q'); "\
"SHOW COLUMNS FROM defaults;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
defaults	CREATE TABLE `defaults` (
  `id` int NOT NULL,
  `vc` varchar(3) DEFAULT 'ab',
  `vnn` varchar(3) NOT NULL DEFAULT '',
  `v0` varchar(0) DEFAULT '',
  `c` char(3) DEFAULT 'xy',
  `cnn` char(3) NOT NULL DEFAULT 'q'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create renders char and varchar defaults" \
    "$show_create_expected" \
    "SHOW CREATE TABLE defaults;" \
    "$DATABASE"

information_schema_expected=$(cat <<\EXPECTED
id	int	NULL	NULL	NULL	NO	int
vc	varchar	3	12	ab	YES	varchar(3)
vnn	varchar	3	12		NO	varchar(3)
v0	varchar	0	0		YES	varchar(0)
c	char	3	12	xy	YES	char(3)
cnn	char	3	12	q	NO	char(3)
EXPECTED
)
expect_output \
    "information schema renders char and varchar defaults" \
    "$information_schema_expected" \
    "SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "\
"COLUMN_DEFAULT, IS_NULLABLE, COLUMN_TYPE "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='defaults' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

dml_defaults_expected=$(cat <<\EXPECTED
1	0
1	[ab]	2	[]	0	[]	0	[xy]	2	[q]	1
2	[ab]	2	[]	0	[]	0	[xy]	2	[q]	1
EXPECTED
)
expect_output \
    "omitted columns and explicit defaults materialize string defaults" \
    "$dml_defaults_expected" \
    "INSERT INTO defaults (id) VALUES (1); "\
"INSERT INTO defaults VALUES (2, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, CONCAT('[', vc, ']'), LENGTH(vc), CONCAT('[', vnn, ']'), LENGTH(vnn), "\
"CONCAT('[', v0, ']'), LENGTH(v0), CONCAT('[', c, ']'), LENGTH(c), "\
"CONCAT('[', cnn, ']'), LENGTH(cnn) FROM defaults ORDER BY id;" \
    "$DATABASE"

update_replace_expected=$(cat <<\EXPECTED
1	0	[]
1	0	3	[ab]	[]
EXPECTED
)
expect_output \
    "update and replace materialize string defaults" \
    "$update_replace_expected" \
    "UPDATE defaults SET vnn='zz' WHERE id=1; "\
"UPDATE defaults SET vnn=DEFAULT WHERE id=1; "\
"SELECT ROW_COUNT(), @@warning_count, CONCAT('[', vnn, ']') FROM defaults WHERE id=1; "\
"REPLACE INTO defaults (id) VALUES (3); "\
"SELECT ROW_COUNT(), @@warning_count, id, CONCAT('[', vc, ']'), CONCAT('[', vnn, ']') "\
"FROM defaults WHERE id=3;" \
    "$DATABASE"

alter_add_expected=$(cat <<\EXPECTED
0	0
1	[hey]	3
2	[hey]	3
3	[hey]	3
EXPECTED
)
expect_output \
    "alter add column backfills string default" \
    "$alter_add_expected" \
    "ALTER TABLE defaults ADD COLUMN added VARCHAR(3) NOT NULL DEFAULT 'hey'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, CONCAT('[', added, ']'), LENGTH(added) FROM defaults ORDER BY id;" \
    "$DATABASE"

alter_set_drop_expected=$(cat <<\EXPECTED
vc	varchar(3)	YES		zz	
4	[zz]	2
vc	varchar(3)	YES		NULL	
EXPECTED
)
expect_output \
    "alter set and drop string default" \
    "$alter_set_drop_expected" \
    "ALTER TABLE defaults ALTER COLUMN vc SET DEFAULT 'zz'; "\
"SHOW COLUMNS FROM defaults LIKE 'vc'; "\
"INSERT INTO defaults (id) VALUES (4); "\
"SELECT id, CONCAT('[', vc, ']'), LENGTH(vc) FROM defaults WHERE id=4; "\
"ALTER TABLE defaults ALTER COLUMN vc DROP DEFAULT; "\
"SHOW COLUMNS FROM defaults LIKE 'vc';" \
    "$DATABASE"

expect_error \
    "omitted dropped nullable default fails under strict mode" \
    1364 \
    HY000 \
    "Field 'vc' doesn't have a default value" \
    "INSERT INTO defaults (id) VALUES (5);" \
    "$DATABASE"

clone_expected=$(cat <<\EXPECTED
like_defaults	CREATE TABLE `like_defaults` (
  `id` int NOT NULL,
  `vc` varchar(3),
  `vnn` varchar(3) NOT NULL DEFAULT '',
  `v0` varchar(0) DEFAULT '',
  `c` char(3) DEFAULT 'xy',
  `cnn` char(3) NOT NULL DEFAULT 'q',
  `added` varchar(3) NOT NULL DEFAULT 'hey'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ctas_defaults	CREATE TABLE `ctas_defaults` (
  `vc` varchar(3),
  `vnn` varchar(3) NOT NULL DEFAULT '',
  `c` char(3) DEFAULT 'xy'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "create table like and ctas preserve string defaults" \
    "$clone_expected" \
    "CREATE TABLE like_defaults LIKE defaults; "\
"SHOW CREATE TABLE like_defaults; "\
"CREATE TABLE ctas_defaults AS SELECT vc, vnn, c FROM defaults; "\
"SHOW CREATE TABLE ctas_defaults;" \
    "$DATABASE"

escape_expected=$(cat <<\EXPECTED
esc	CREATE TABLE `esc` (
  `v` varchar(10) DEFAULT 'a''b',
  `w` varchar(10) DEFAULT 'x\ny'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
612762	780A79
EXPECTED
)
expect_output \
    "quote and backslash escapes survive defaults" \
    "$escape_expected" \
    "CREATE TABLE esc (v VARCHAR(10) DEFAULT 'a''b', w VARCHAR(10) DEFAULT 'x\\ny'); "\
"SHOW CREATE TABLE esc; "\
"INSERT INTO esc VALUES(DEFAULT, DEFAULT); "\
"SELECT HEX(v), HEX(w) FROM esc;" \
    "$DATABASE"

trailing_space_expected=$(cat <<\EXPECTED
Note	1265	Data truncated for column 'v' at row 1
vt	CREATE TABLE `vt` (
  `v` varchar(3) DEFAULT 'abc'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "mysql accepts trailing-space overlength varchar defaults with note" \
    "$trailing_space_expected" \
    "CREATE TABLE vt (v VARCHAR(3) DEFAULT 'abc '); "\
"SHOW WARNINGS; "\
"SHOW CREATE TABLE vt;" \
    "$DATABASE"

expect_error \
    "not null default null is invalid" \
    1067 \
    42000 \
    "Invalid default value for 'v'" \
    "CREATE TABLE bad_null (v VARCHAR(3) NOT NULL DEFAULT NULL);" \
    "$DATABASE"

expect_error \
    "overlength varchar default is invalid" \
    1067 \
    42000 \
    "Invalid default value for 'v'" \
    "CREATE TABLE bad_over (v VARCHAR(3) DEFAULT 'abcd');" \
    "$DATABASE"

expect_error \
    "zero-length varchar nonempty default is invalid" \
    1067 \
    42000 \
    "Invalid default value for 'v'" \
    "CREATE TABLE bad_v0 (v VARCHAR(0) DEFAULT 'x');" \
    "$DATABASE"

expect_error \
    "zero-length char nonempty default is invalid" \
    1067 \
    42000 \
    "Invalid default value for 'c'" \
    "CREATE TABLE bad_c0 (c CHAR(0) DEFAULT 'x');" \
    "$DATABASE"

expect_error \
    "alter set overlength string default is invalid" \
    1067 \
    42000 \
    "Invalid default value for 'vc'" \
    "ALTER TABLE defaults ALTER COLUMN vc SET DEFAULT 'abcd';" \
    "$DATABASE"

expect_error \
    "text literal default remains deferred" \
    1101 \
    42000 \
    "BLOB, TEXT, GEOMETRY or JSON column 't' can't have a default value" \
    "CREATE TABLE bad_text_literal (t TEXT DEFAULT 'abc');" \
    "$DATABASE"

expect_upstream_accepts \
    "text expression default is deferred by MyLite" \
    "CREATE TABLE text_expression_default (t TEXT DEFAULT ('abc'));" \
    "$DATABASE"

cleanup

printf '%s\n' "mysql_baseline_string_defaults_expectations: ok"
