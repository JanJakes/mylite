#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_column_comments_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_column_comments_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "${MYSQL_BIN:-mysql}" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --default-character-set=utf8mb4 --batch --raw --skip-column-names "$@"
        return
    fi
    if [ -n "$MYSQL_BIN" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot --default-character-set=utf8mb4 \
                --batch --raw --skip-column-names "$@"
        return
    fi
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --default-character-set=utf8mb4 \
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
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_0900_ai_ci;" >/dev/null

create_expected=$(cat <<\EXPECTED
0	0
t	CREATE TABLE `t` (
  `a` int DEFAULT NULL COMMENT 'alpha',
  `b` varchar(5) DEFAULT 'x' COMMENT 'bee',
  `c` int DEFAULT NULL COMMENT 'second',
  `d` datetime DEFAULT NULL COMMENT 'time',
  `id` bigint unsigned NOT NULL AUTO_INCREMENT COMMENT 'identifier',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
a	int	NULL	YES		NULL		select,insert,update,references	alpha
b	varchar(5)	utf8mb4_0900_ai_ci	YES		x		select,insert,update,references	bee
c	int	NULL	YES		NULL		select,insert,update,references	second
d	datetime	NULL	YES		NULL		select,insert,update,references	time
id	bigint unsigned	NULL	NO	PRI	NULL	auto_increment	select,insert,update,references	identifier
a	alpha
b	bee
c	second
d	time
id	identifier
EXPECTED
)
expect_output \
    "create column comments metadata" \
    "$create_expected" \
    "CREATE TABLE t ("\
"a INT COMMENT 'alpha', "\
"b VARCHAR(5) DEFAULT 'x' COMMENT 'bee', "\
"c INT COMMENT 'first' COMMENT 'second', "\
"d DATETIME COMMENT 'time', "\
"id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'identifier', "\
"PRIMARY KEY (id)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE t; "\
"SHOW FULL COLUMNS FROM t; "\
"SELECT COLUMN_NAME, COLUMN_COMMENT FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

clone_expected=$(cat <<\EXPECTED
clone	CREATE TABLE `clone` (
  `a` int DEFAULT NULL COMMENT 'alpha',
  `b` varchar(5) DEFAULT 'x' COMMENT 'bee',
  `c` int DEFAULT NULL COMMENT 'second',
  `d` datetime DEFAULT NULL COMMENT 'time',
  `id` bigint unsigned NOT NULL AUTO_INCREMENT COMMENT 'identifier',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
a	alpha
b	bee
c	second
d	time
id	identifier
EXPECTED
)
expect_output \
    "create table like copies column comments" \
    "$clone_expected" \
    "CREATE TABLE clone LIKE t; "\
"SHOW CREATE TABLE clone; "\
"SELECT COLUMN_NAME, COLUMN_COMMENT FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'clone' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

ctas_expected=$(cat <<\EXPECTED
ctas	CREATE TABLE `ctas` (
  `a` int DEFAULT NULL COMMENT 'alpha',
  `b` varchar(5) DEFAULT 'x' COMMENT 'bee'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
a	alpha
b	bee
EXPECTED
)
expect_output \
    "create table select copies direct column comments" \
    "$ctas_expected" \
    "CREATE TABLE ctas AS SELECT a, b FROM t; "\
"SHOW CREATE TABLE ctas; "\
"SELECT COLUMN_NAME, COLUMN_COMMENT FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'ctas' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

alter_expected=$(cat <<\EXPECTED
0	0
0	0
0	0
altered	CREATE TABLE `altered` (
  `d` int DEFAULT NULL COMMENT 'dee',
  `a` bigint DEFAULT NULL COMMENT 'modified',
  `bb` varchar(7) DEFAULT NULL COMMENT 'changed',
  `c` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
d	dee
a	modified
bb	changed
c	
EXPECTED
)
expect_output \
    "alter column comments" \
    "$alter_expected" \
    "CREATE TABLE altered (a INT COMMENT 'alpha', b VARCHAR(5) COMMENT 'bee', "\
"c INT COMMENT 'clear'); "\
"ALTER TABLE altered ADD COLUMN d INT COMMENT 'dee' FIRST; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE altered MODIFY COLUMN a BIGINT COMMENT 'modified'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE altered CHANGE COLUMN b bb VARCHAR(7) COMMENT 'changed'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE altered MODIFY COLUMN c INT; "\
"SHOW CREATE TABLE altered; "\
"SELECT COLUMN_NAME, COLUMN_COMMENT FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'altered' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

order_expected=$(cat <<\EXPECTED
notnull	CREATE TABLE `notnull` (
  `a` int NOT NULL COMMENT 'x'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ascii_comment	CREATE TABLE `ascii_comment` (
  `a` varchar(5) CHARACTER SET ascii COLLATE ascii_general_ci DEFAULT NULL COMMENT 'x'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
unique_comment	CREATE TABLE `unique_comment` (
  `a` int DEFAULT NULL COMMENT 'x',
  UNIQUE KEY `a` (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "column comment ordering around other attributes" \
    "$order_expected" \
    "CREATE TABLE notnull (a INT COMMENT 'x' NOT NULL); "\
"CREATE TABLE ascii_comment (a VARCHAR(5) CHARACTER SET ascii COMMENT 'x'); "\
"CREATE TABLE unique_comment (a INT UNIQUE COMMENT 'x'); "\
"SHOW CREATE TABLE notnull; "\
"SHOW CREATE TABLE ascii_comment; "\
"SHOW CREATE TABLE unique_comment;" \
    "$DATABASE"

ndb_expected=$(cat <<\EXPECTED
ndb_column_comment	CREATE TABLE `ndb_column_comment` (
  `id` int DEFAULT NULL,
  `body` text COMMENT 'NDB_COLUMN=BLOB_INLINE_SIZE=4096,MAX_BLOB_PART_SIZE'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
body	text	utf8mb4_0900_ai_ci	YES		NULL		select,insert,update,references	NDB_COLUMN=BLOB_INLINE_SIZE=4096,MAX_BLOB_PART_SIZE
body	NDB_COLUMN=BLOB_INLINE_SIZE=4096,MAX_BLOB_PART_SIZE
EXPECTED
)
expect_output \
    "NDB-shaped column comment is ordinary metadata" \
    "$ndb_expected" \
    "CREATE TABLE ndb_column_comment ("\
"id INT, "\
"body TEXT COMMENT 'NDB_COLUMN=BLOB_INLINE_SIZE=4096,MAX_BLOB_PART_SIZE'); "\
"SHOW CREATE TABLE ndb_column_comment; "\
"SHOW FULL COLUMNS FROM ndb_column_comment WHERE Field = 'body'; "\
"SELECT COLUMN_NAME, COLUMN_COMMENT FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'ndb_column_comment' "\
"AND COLUMN_NAME = 'body';" \
    "$DATABASE"

long_ok_expected=$(cat <<\EXPECTED
0	0
EXPECTED
)
expect_output \
    "1024 character column comment succeeds" \
    "$long_ok_expected" \
    "SET @sql_ok = CONCAT(\"CREATE TABLE long_ok (a INT COMMENT '\", REPEAT('x', 1024), \"')\"); "\
"PREPARE stmt_ok FROM @sql_ok; "\
"EXECUTE stmt_ok; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"DEALLOCATE PREPARE stmt_ok;" \
    "$DATABASE"

expect_error \
    "column comment equal sign is syntax error" \
    1064 \
    42000 \
    "near '='x')'" \
    "CREATE TABLE equal_comment (a INT COMMENT='x');" \
    "$DATABASE"

expect_error \
    "numeric column comment is syntax error" \
    1064 \
    42000 \
    "near '123)'" \
    "CREATE TABLE numeric_comment (a INT COMMENT 123);" \
    "$DATABASE"

expect_error \
    "null column comment is syntax error" \
    1064 \
    42000 \
    "near 'NULL)'" \
    "CREATE TABLE null_comment (a INT COMMENT NULL);" \
    "$DATABASE"

expect_error \
    "charset after comment is syntax error" \
    1064 \
    42000 \
    "near 'CHARACTER SET ascii)'" \
    "CREATE TABLE bad_order (a VARCHAR(5) COMMENT 'x' CHARACTER SET ascii);" \
    "$DATABASE"

expect_error \
    "1025 character column comment fails" \
    1629 \
    HY000 \
    "Comment for field 'a' is too long (max = 1024)" \
    "SET @sql_bad = CONCAT(\"CREATE TABLE long_bad (a INT COMMENT '\", "\
"REPEAT('x', 1025), \"')\"); "\
"PREPARE stmt_bad FROM @sql_bad; "\
"EXECUTE stmt_bad;" \
    "$DATABASE"
