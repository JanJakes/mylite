#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_alter_change_modify_text_family_$$"

fail() {
    printf '%s\n' "mysql_baseline_alter_change_modify_text_family_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        fi
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    fi
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
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;" \
    >/dev/null

run_mysql \
    "CREATE TABLE widen ("\
"id INT NOT NULL, v VARCHAR(10) NOT NULL DEFAULT 'd', c CHAR(5) DEFAULT 'xy', body TEXT NULL); "\
"INSERT INTO widen VALUES (1, 'abc', 'z', 'old text'), (2, 'def', NULL, NULL);" \
    "$DATABASE" >/dev/null

modify_expected=$(cat <<\EXPECTED
2	0
id	int	NO	NULL
v	text	NO	NULL
c	char(5)	YES	xy
body	text	YES	NULL
1	abc	z	old text
2	def	NULL	NULL
EXPECTED
)
expect_output \
    "varchar to text modify preserves rows and metadata" \
    "$modify_expected" \
    "ALTER TABLE widen MODIFY v TEXT NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='widen' ORDER BY ORDINAL_POSITION; "\
"SELECT id, v, IF(c IS NULL, 'NULL', c), IF(body IS NULL, 'NULL', body) "\
"FROM widen ORDER BY id;" \
    "$DATABASE"

change_expected=$(cat <<\EXPECTED
2	0
c_new	longtext	YES	NULL
1	z
2	NULL
EXPECTED
)
expect_output \
    "char to longtext change renames and preserves rows" \
    "$change_expected" \
    "ALTER TABLE widen CHANGE c c_new LONGTEXT NULL; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='widen' AND COLUMN_NAME='c_new'; "\
"SELECT id, IF(c_new IS NULL, 'NULL', c_new) FROM widen ORDER BY id;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE hyphen_table (id INT, \`with-hyphen\` VARCHAR(255) NOT NULL DEFAULT ''); "\
"INSERT INTO hyphen_table VALUES (1, 'abc');" \
    "$DATABASE" >/dev/null
hyphen_expected=$(cat <<\EXPECTED
1	0
with-hyphen	text	NO	NULL
abc
EXPECTED
)
expect_output \
    "backtick hyphenated change to text" \
    "$hyphen_expected" \
    "ALTER TABLE hyphen_table CHANGE \`with-hyphen\` \`with-hyphen\` TEXT NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='hyphen_table' AND COLUMN_NAME='with-hyphen'; "\
"SELECT \`with-hyphen\` FROM hyphen_table;" \
    "$DATABASE"

text_family_expected=$(cat <<\EXPECTED
2	0
body	longtext	YES	NULL
1	short
2	NULL
EXPECTED
)
expect_output \
    "text family to wider text family" \
    "$text_family_expected" \
    "CREATE TABLE body_table (id INT, body TEXT NULL); "\
"INSERT INTO body_table VALUES (1, 'short'), (2, NULL); "\
"ALTER TABLE body_table MODIFY body LONGTEXT NULL; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='body_table' AND COLUMN_NAME='body'; "\
"SELECT id, IF(body IS NULL, 'NULL', body) FROM body_table ORDER BY id;" \
    "$DATABASE"

default_modify_expected=$(cat <<\EXPECTED
2	0
1	NULL
2	old
3	fresh
EXPECTED
)
expect_output \
    "text default after modify" \
    "$default_modify_expected" \
    "CREATE TABLE default_modify (id INT, body TEXT NULL); "\
"INSERT INTO default_modify VALUES (1, NULL), (2, 'old'); "\
"ALTER TABLE default_modify MODIFY body LONGTEXT DEFAULT ('fresh'); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"INSERT INTO default_modify (id) VALUES (3); "\
"SELECT id, IF(body IS NULL, 'NULL', body) FROM default_modify ORDER BY id;" \
    "$DATABASE"

default_change_expected=$(cat <<\EXPECTED
2	0
1	abc
2	def
3	fresh
EXPECTED
)
expect_output \
    "text default after change" \
    "$default_change_expected" \
    "CREATE TABLE default_change (id INT, v VARCHAR(20)); "\
"INSERT INTO default_change VALUES (1, 'abc'), (2, 'def'); "\
"ALTER TABLE default_change CHANGE v v TEXT DEFAULT ('fresh'); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"INSERT INTO default_change (id) VALUES (3); "\
"SELECT id, v FROM default_change ORDER BY id;" \
    "$DATABASE"

national_expected=$(cat <<\EXPECTED
2	0
2	0
n	text	YES	NULL
nv_text	mediumtext	YES	NULL
1	aa	bb
2	NULL	NULL
EXPECTED
)
expect_output \
    "national string sources to text family" \
    "$national_expected" \
    "CREATE TABLE national_source (id INT, n NCHAR(2), nv NVARCHAR(5)); "\
"INSERT INTO national_source VALUES (1, 'aa', 'bb'), (2, NULL, NULL); "\
"ALTER TABLE national_source MODIFY n TEXT; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE national_source CHANGE nv nv_text MEDIUMTEXT; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='national_source' "\
"AND COLUMN_NAME IN ('n', 'nv_text') ORDER BY ORDINAL_POSITION; "\
"SELECT id, IF(n IS NULL, 'NULL', n), IF(nv_text IS NULL, 'NULL', nv_text) "\
"FROM national_source ORDER BY id;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE null_text (v VARCHAR(20) NULL); INSERT INTO null_text VALUES ('a'), (NULL);" \
    "$DATABASE" >/dev/null
expect_error \
    "null source into text not null" \
    1265 \
    01000 \
    "Data truncated for column 'v' at row 2" \
    "ALTER TABLE null_text MODIFY v TEXT NOT NULL;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE too_long (v LONGTEXT); INSERT INTO too_long VALUES (REPEAT('x', 300));" \
    "$DATABASE" >/dev/null
expect_error \
    "longtext to tinytext overflow" \
    1406 \
    22001 \
    "Data too long for column 'v' at row 1" \
    "ALTER TABLE too_long MODIFY v TINYTEXT;" \
    "$DATABASE"

run_mysql "CREATE TABLE full_key (v VARCHAR(20), KEY k_v (v));" "$DATABASE" >/dev/null
expect_error \
    "full key rejects text replacement" \
    1170 \
    42000 \
    "BLOB/TEXT column 'v' used in key specification without a key length" \
    "ALTER TABLE full_key MODIFY v TEXT;" \
    "$DATABASE"

prefix_expected=$(cat <<\EXPECTED
0	0
v	text	YES	NULL
k_v	v	10	BTREE
EXPECTED
)
expect_output \
    "prefix index survives text replacement" \
    "$prefix_expected" \
    "CREATE TABLE prefix_t (v VARCHAR(20), KEY k_v (v(10))); "\
"ALTER TABLE prefix_t MODIFY v TEXT; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='prefix_t' AND COLUMN_NAME='v'; "\
"SELECT INDEX_NAME, COLUMN_NAME, SUB_PART, INDEX_TYPE "\
"FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='prefix_t' ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

fulltext_expected=$(cat <<\EXPECTED
0	0
v	text	YES	NULL
ft_v	v	NULL	FULLTEXT
EXPECTED
)
expect_output \
    "fulltext index survives text replacement" \
    "$fulltext_expected" \
    "CREATE TABLE fulltext_t (v VARCHAR(20), FULLTEXT KEY ft_v (v)); "\
"ALTER TABLE fulltext_t MODIFY v TEXT; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='fulltext_t' AND COLUMN_NAME='v'; "\
"SELECT INDEX_NAME, COLUMN_NAME, SUB_PART, INDEX_TYPE "\
"FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='fulltext_t' ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"
