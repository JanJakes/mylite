#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_ddl_options_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_ddl_option_surfaces_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names "$@"
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

expect_success() {
    label=$1
    sql=$2
    shift 2

    if ! run_mysql "$sql" "$@" >/dev/null; then
        fail "$label: command failed"
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
    status=$?
    set -e

    if [ "$status" -eq 0 ]; then
        fail "$label: expected error $code/$state, command succeeded with [$output]"
    fi
    case "$output" in
        *"ERROR $code ($state)"*"$message"*) ;;
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

expect_contains() {
    label=$1
    needle=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    case "$output" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle], got [$output]" ;;
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; "\
"CREATE TABLE base_a (id INT) ENGINE=MyISAM; "\
"CREATE TABLE base_b (id INT) ENGINE=MyISAM;" >/dev/null

expect_success \
    "MERGE table options" \
    "USE ${DATABASE}; CREATE TABLE merge_t (id INT) ENGINE=MERGE "\
"UNION=(base_a,base_b) INSERT_METHOD=NO;"
expect_contains \
    "MERGE SHOW CREATE union" \
    'UNION=(`base_a`,`base_b`)' \
    "USE ${DATABASE}; SHOW CREATE TABLE merge_t;"
expect_success \
    "MERGE whitespace union form" \
    "USE ${DATABASE}; CREATE TABLE merge_space_t (id INT) ENGINE=MRG_MYISAM "\
"UNION (base_a) INSERT_METHOD=FIRST;"
expect_contains \
    "MERGE SHOW CREATE insert method" \
    "INSERT_METHOD=FIRST" \
    "USE ${DATABASE}; SHOW CREATE TABLE merge_space_t;"

expect_success \
    "InnoDB tablespace and storage options" \
    "USE ${DATABASE}; CREATE TABLE opt_t (id INT) TABLESPACE innodb_file_per_table "\
"STORAGE DISK ENGINE=InnoDB; "\
"ALTER TABLE opt_t TABLESPACE innodb_file_per_table STORAGE DISK, ENGINE=InnoDB; "\
"ALTER TABLE opt_t STORAGE MEMORY;"

expect_output \
    "inline invisible column metadata" \
    "INVISIBLE" \
    "USE ${DATABASE}; CREATE TABLE visible_t (id INT VISIBLE, hidden_col INT INVISIBLE); "\
"SELECT EXTRA FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() "\
"AND TABLE_NAME = 'visible_t' AND COLUMN_NAME = 'hidden_col';"

expect_contains \
    "spatial SRID SHOW CREATE" \
    "SRID 0" \
    "USE ${DATABASE}; CREATE TABLE srid_t (g GEOMETRY SRID 0); SHOW CREATE TABLE srid_t;"
expect_error \
    "all invisible columns rejected" \
    4028 \
    HY000 \
    "A table must have at least one visible column" \
    "USE ${DATABASE}; CREATE TABLE invalid_all_invisible (only_col INT INVISIBLE);"
expect_error \
    "non-geometry SRID rejected" \
    1221 \
    HY000 \
    "Incorrect usage of SRID and non-geometry column" \
    "USE ${DATABASE}; CREATE TABLE invalid_srid (id INT SRID 0);"

expect_success \
    "multi-action rename and online option" \
    "USE ${DATABASE}; CREATE TABLE multi_t (id INT PRIMARY KEY, c INT, KEY ix_c(c)); "\
"ALTER TABLE multi_t ADD COLUMN d INT, RENAME TO multi_t2, ALGORITHM=INPLACE;"
expect_output \
    "multi-action renamed table exists" \
    "multi_t2" \
    "USE ${DATABASE}; SHOW TABLES LIKE 'multi_t2';"
expect_success \
    "multi-action rename column and add column" \
    "USE ${DATABASE}; ALTER TABLE multi_t2 RENAME COLUMN d TO e, ADD COLUMN f INT;"
expect_output \
    "multi-action renamed column exists" \
    "e" \
    "USE ${DATABASE}; SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'multi_t2' AND COLUMN_NAME = 'e';"
expect_success \
    "multi-action rename index and disable keys" \
    "USE ${DATABASE}; ALTER TABLE multi_t2 RENAME INDEX ix_c TO ix_c2, DISABLE KEYS;"
expect_output \
    "multi-action renamed index exists" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'multi_t2' AND INDEX_NAME = 'ix_c2';"
expect_success \
    "multi-action table options" \
    "USE ${DATABASE}; ALTER TABLE multi_t2 AUTO_INCREMENT = 10, COMMENT='hello';"
expect_output \
    "multi-action table comment metadata" \
    "hello" \
    "USE ${DATABASE}; SELECT TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'multi_t2';"

cleanup

printf '%s\n' "mysql_parser_corpus_ddl_option_surfaces_expectations: ok"
