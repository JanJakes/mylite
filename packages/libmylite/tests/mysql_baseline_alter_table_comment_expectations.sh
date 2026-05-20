#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_alter_table_comment_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_comment_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift

    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --no-defaults --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw \
                --skip-column-names --default-character-set=utf8mb4 "$@"
    fi
}

run_mysql_with_headers() {
    sql=$1
    shift

    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --no-defaults --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --default-character-set=utf8mb4 "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw \
                --default-character-set=utf8mb4 "$@"
    fi
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_contains() {
    label=$1
    haystack=$2
    needle=$3

    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle], got [$haystack]" ;;
    esac
}

expect_not_contains() {
    label=$1
    haystack=$2
    needle=$3

    case "$haystack" in
        *"$needle"*) fail "$label: expected output not to contain [$needle], got [$haystack]" ;;
        *) ;;
    esac
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

run_mysql \
    "CREATE DATABASE ${DATABASE};
     USE ${DATABASE};
     CREATE TABLE altered(id INT) ENGINE=InnoDB COMMENT='old';
     ALTER TABLE altered COMMENT='new';
     CREATE TABLE noeq(id INT) ENGINE=InnoDB;
     ALTER TABLE noeq COMMENT 'noeq';
     CREATE TABLE double_comment(id INT) ENGINE=InnoDB;
     ALTER TABLE double_comment COMMENT = \"double\";
     CREATE TABLE escaped_comment(id INT) ENGINE=InnoDB COMMENT='old';
     ALTER TABLE escaped_comment COMMENT='a\\'b\\\\c';
     CREATE TABLE algorithm_lock(id INT) ENGINE=InnoDB;
     ALTER TABLE algorithm_lock COMMENT='copy', ALGORITHM=COPY;
     ALTER TABLE algorithm_lock COMMENT='inplace', ALGORITHM=INPLACE;
     ALTER TABLE algorithm_lock COMMENT='locked', LOCK=NONE;" >/dev/null

status=$(run_mysql "USE ${DATABASE}; SELECT ROW_COUNT(), @@warning_count;")
expect_value "successful alter status" "0	0" "$status"

altered_show=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.altered;")
expect_contains "show create changed comment" "$altered_show" "COMMENT='new'"
expect_not_contains "show create old comment removed" "$altered_show" "COMMENT='old'"

noeq_show=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.noeq;")
expect_contains "comment without equals" "$noeq_show" "COMMENT='noeq'"

double_show=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.double_comment;")
expect_contains "double quoted comment value" "$double_show" "COMMENT='double'"

escaped_show=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.escaped_comment;")
expect_contains "escaped_comment altered comment" "$escaped_show" "COMMENT='a''b\\\\c'"

algorithm_show=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.algorithm_lock;")
expect_contains "algorithm lock final comment" "$algorithm_show" "COMMENT='locked'"

metadata=$(
    run_mysql \
        "SELECT TABLE_NAME, TABLE_COMMENT, CREATE_OPTIONS
         FROM INFORMATION_SCHEMA.TABLES
         WHERE TABLE_SCHEMA = '${DATABASE}'
           AND TABLE_NAME IN ('altered','noeq','double_comment','escaped_comment','algorithm_lock')
         ORDER BY TABLE_NAME;"
)
expected_metadata=$(
    printf '%s\t%s\t\n' "algorithm_lock" "locked"
    printf '%s\t%s\t\n' "altered" "new"
    printf '%s\t%s\t\n' "double_comment" "double"
    printf '%s\t%s\t\n' "escaped_comment" "a'b\\c"
    printf '%s\t%s\t\n' "noeq" "noeq"
)
expect_value "information schema table comments" "$expected_metadata" "$metadata"

status_rows=$(
    run_mysql_with_headers \
        "SHOW TABLE STATUS FROM ${DATABASE}
         WHERE Name IN ('altered','noeq','double_comment','escaped_comment','algorithm_lock');"
)
expect_contains "show table status headers include Comment" "$status_rows" "Create_options	Comment"
expect_contains "show table status changed row" "$status_rows" "altered	InnoDB"
expect_contains "show table status altered value" "$status_rows" "	new"
expect_contains "show table status escaped_comment value" "$status_rows" "a'b\\c"

clear_show=$(
    run_mysql \
        "USE ${DATABASE};
         ALTER TABLE altered COMMENT='';
         SHOW CREATE TABLE altered;"
)
expect_not_contains "empty alter comment clears show create suffix" "$clear_show" "COMMENT=''"
cleared_metadata=$(
    run_mysql \
        "SELECT TABLE_COMMENT
         FROM INFORMATION_SCHEMA.TABLES
         WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'altered';"
)
expect_value "empty alter comment visible in metadata" "" "$cleared_metadata"

qualified=$(
    run_mysql \
        "ALTER TABLE ${DATABASE}.altered COMMENT='qualified';
         SELECT ROW_COUNT(), @@warning_count;
         SHOW CREATE TABLE ${DATABASE}.altered;"
)
expect_contains "qualified alter without selected database status" "$qualified" "0	0"
expect_contains "qualified alter without selected database comment" "$qualified" "COMMENT='qualified'"

temp_show=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TEMPORARY TABLE temp_comment(id INT) ENGINE=InnoDB COMMENT='temp old';
         ALTER TABLE temp_comment COMMENT='temp new';
         SHOW CREATE TABLE temp_comment;"
)
expect_contains "temporary altered comment rendered" "$temp_show" "CREATE TEMPORARY TABLE"
expect_contains "temporary altered comment value" "$temp_show" "COMMENT='temp new'"

run_mysql \
    "USE ${DATABASE};
     SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES';
     CREATE TABLE no_backslash_comment(id INT) ENGINE=InnoDB COMMENT='old';
     ALTER TABLE no_backslash_comment COMMENT='a\\\\b';" >/dev/null
no_backslash_show=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.no_backslash_comment;")
expect_contains \
    "NO_BACKSLASH_ESCAPES altered comment" \
    "$no_backslash_show" \
    "COMMENT='a\\\\\\\\b'"
no_backslash_hex=$(
    run_mysql \
        "SELECT HEX(TABLE_COMMENT)
         FROM INFORMATION_SCHEMA.TABLES
         WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'no_backslash_comment';"
)
expect_value "NO_BACKSLASH_ESCAPES stored literal backslashes" "615C5C62" "$no_backslash_hex"

comment2048=$(printf 'a%.0s' $(seq 1 2048))
run_mysql "USE ${DATABASE}; CREATE TABLE max_comment(id INT); ALTER TABLE max_comment COMMENT='${comment2048}';" >/dev/null
max_length=$(
    run_mysql \
        "SELECT LENGTH(TABLE_COMMENT)
         FROM INFORMATION_SCHEMA.TABLES
         WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'max_comment';"
)
expect_value "2048 character comment accepted" "2048" "$max_length"

comment2049="${comment2048}a"
expect_error \
    "2049 character comment rejected" \
    1628 \
    HY000 \
    "Comment for table 'max_comment' is too long (max = 2048)" \
    "USE ${DATABASE}; ALTER TABLE max_comment COMMENT='${comment2049}';"

expect_error \
    "numeric comment syntax" \
    1064 \
    42000 \
    "near '123'" \
    "USE ${DATABASE}; ALTER TABLE max_comment COMMENT=123;"

expect_error \
    "null comment syntax" \
    1064 \
    42000 \
    "near 'NULL'" \
    "USE ${DATABASE}; ALTER TABLE max_comment COMMENT=NULL;"

expect_error \
    "identifier comment syntax" \
    1064 \
    42000 \
    "near 'abc'" \
    "USE ${DATABASE}; ALTER TABLE max_comment COMMENT=abc;"

expect_error \
    "missing default schema" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE max_comment COMMENT='x';"

expect_error \
    "unknown schema" \
    1049 \
    42000 \
    "Unknown database 'unknown_alter_comment_schema'" \
    "ALTER TABLE unknown_alter_comment_schema.max_comment COMMENT='x';"

expect_error \
    "unknown table" \
    1146 \
    42S02 \
    "doesn't exist" \
    "USE ${DATABASE}; ALTER TABLE missing_comment COMMENT='x';"

expect_error \
    "instant algorithm rejected" \
    1845 \
    0A000 \
    "ALGORITHM=INSTANT is not supported for this operation" \
    "USE ${DATABASE}; ALTER TABLE max_comment COMMENT='instant', ALGORITHM=INSTANT;"

expect_error \
    "ANSI_QUOTES double quoted comment syntax" \
    1064 \
    42000 \
    "near '\"quoted\"'" \
    "USE ${DATABASE}; SET SESSION sql_mode = 'ANSI_QUOTES';
     ALTER TABLE max_comment COMMENT \"quoted\";"
