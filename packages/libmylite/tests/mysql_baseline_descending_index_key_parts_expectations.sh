#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_descending_index_key_parts_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_descending_index_key_parts_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
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
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
    esac
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${MISSING_DATABASE};" >/dev/null 2>&1 || true
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

metadata_expected=$(cat <<\EXPECTED
0	0
0	0
0	0
t	CREATE TABLE `t` (
  `id` int NOT NULL,
  `a` int DEFAULT NULL,
  `b` int DEFAULT NULL,
  `v` varchar(20) DEFAULT NULL,
  PRIMARY KEY (`id` DESC),
  UNIQUE KEY `u_b` (`b` DESC),
  KEY `k_mix` (`a`,`b` DESC),
  KEY `k_v` (`v`(5) DESC),
  KEY `k_created` (`a` DESC,`b`),
  KEY `k_alt` (`v`(3),`a` DESC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
t	0	PRIMARY	1	id	D	0	NULL	NULL		BTREE			YES	NULL
t	0	u_b	1	b	D	0	NULL	NULL	YES	BTREE			YES	NULL
t	1	k_mix	1	a	A	0	NULL	NULL	YES	BTREE			YES	NULL
t	1	k_mix	2	b	D	0	NULL	NULL	YES	BTREE			YES	NULL
t	1	k_v	1	v	D	0	5	NULL	YES	BTREE			YES	NULL
t	1	k_created	1	a	D	0	NULL	NULL	YES	BTREE			YES	NULL
t	1	k_created	2	b	A	0	NULL	NULL	YES	BTREE			YES	NULL
t	1	k_alt	1	v	A	0	3	NULL	YES	BTREE			YES	NULL
t	1	k_alt	2	a	D	0	NULL	NULL	YES	BTREE			YES	NULL
k_alt	1	1	v	A	3	YES	BTREE	YES	NULL
k_alt	1	2	a	D	NULL	YES	BTREE	YES	NULL
k_created	1	1	a	D	NULL	YES	BTREE	YES	NULL
k_created	1	2	b	A	NULL	YES	BTREE	YES	NULL
k_mix	1	1	a	A	NULL	YES	BTREE	YES	NULL
k_mix	1	2	b	D	NULL	YES	BTREE	YES	NULL
k_v	1	1	v	D	5	YES	BTREE	YES	NULL
PRIMARY	0	1	id	D	NULL		BTREE	YES	NULL
u_b	0	1	b	D	NULL	YES	BTREE	YES	NULL
clone	CREATE TABLE `clone` (
  `id` int NOT NULL,
  `a` int DEFAULT NULL,
  `b` int DEFAULT NULL,
  `v` varchar(20) DEFAULT NULL,
  PRIMARY KEY (`id` DESC),
  UNIQUE KEY `u_b` (`b` DESC),
  KEY `k_mix` (`a`,`b` DESC),
  KEY `k_v` (`v`(5) DESC),
  KEY `k_created` (`a` DESC,`b`),
  KEY `k_alt` (`v`(3),`a` DESC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "descending key-part metadata across create table alter add key and create index" \
    "$metadata_expected" \
    "CREATE TABLE t ("\
"id INT NOT NULL, a INT, b INT, v VARCHAR(20), "\
"PRIMARY KEY (id DESC), KEY k_mix (a ASC, b DESC), "\
"KEY k_v (v(5) DESC), UNIQUE KEY u_b (b DESC)"\
"); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"CREATE INDEX k_created ON t (a DESC, b ASC); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE t ADD KEY k_alt (v(3) ASC, a DESC); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE t; "\
"SHOW INDEX FROM t; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, SUB_PART, "\
"NULLABLE, INDEX_TYPE, IS_VISIBLE, EXPRESSION FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX; "\
"CREATE TABLE clone LIKE t; "\
"SHOW CREATE TABLE clone;" \
    "$DATABASE"

alter_primary_expected=$(cat <<\EXPECTED
0	0
add_pk	CREATE TABLE `add_pk` (
  `a` int NOT NULL,
  `b` int NOT NULL,
  PRIMARY KEY (`a` DESC,`b`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
add_pk	0	PRIMARY	1	a	D	0	NULL	NULL		BTREE			YES	NULL
add_pk	0	PRIMARY	2	b	A	0	NULL	NULL		BTREE			YES	NULL
EXPECTED
)
expect_output \
    "alter table add descending primary key metadata" \
    "$alter_primary_expected" \
    "CREATE TABLE add_pk (a INT NOT NULL, b INT NOT NULL); "\
"ALTER TABLE add_pk ADD PRIMARY KEY (a DESC, b ASC); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE add_pk; "\
"SHOW INDEX FROM add_pk;" \
    "$DATABASE"

expect_error \
    "duplicate key-part column fails despite direction difference" \
    1060 \
    42S21 \
    "Duplicate column name 'a'" \
    "CREATE TABLE duplicate_part (a INT, KEY k (a DESC, a ASC));" \
    "$DATABASE"

expect_error \
    "integer prefix with descending direction fails" \
    1089 \
    HY000 \
    "Incorrect prefix key" \
    "CREATE TABLE bad_int_prefix (id INT, KEY k (id(3) DESC));" \
    "$DATABASE"

expect_error \
    "text key without prefix fails before direction matters" \
    1170 \
    42000 \
    "BLOB/TEXT column 'body' used in key specification without a key length" \
    "CREATE TABLE bad_text_desc (body TEXT, KEY k (body DESC));" \
    "$DATABASE"

expect_error \
    "qualified descending key part is syntax error" \
    1064 \
    42000 \
    "near '.a DESC))'" \
    "CREATE TABLE qualified_part (a INT, KEY k (qualified_part.a DESC));" \
    "$DATABASE"

expect_error \
    "qualified descending primary key part is syntax error" \
    1064 \
    42000 \
    "near '.a DESC))'" \
    "CREATE TABLE qualified_primary_part (a INT, PRIMARY KEY (qualified_primary_part.a DESC));" \
    "$DATABASE"
