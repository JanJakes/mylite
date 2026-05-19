#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_table_comment_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_create_table_comment_option_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --default-character-set=utf8mb4 "$@"
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

run_mysql \
    "CREATE DATABASE ${DATABASE};
     USE ${DATABASE};
     CREATE TABLE empty_comment(id INT) ENGINE=InnoDB COMMENT='';
     CREATE TABLE plain_comment(id INT) ENGINE=InnoDB COMMENT='plain';
     CREATE TABLE double_comment(id INT) ENGINE=InnoDB COMMENT=\"double\";
     CREATE TABLE escaped_comment(id INT) ENGINE=InnoDB COMMENT='a\\'b\\\\c';
     CREATE TABLE dup_comment(id INT) ENGINE=InnoDB COMMENT='first' COMMENT='second';
     CREATE TABLE like_comment LIKE escaped_comment;" >/dev/null

plain_show=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.plain_comment;")
expect_contains "plain SHOW CREATE TABLE comment" "$plain_show" "COMMENT='plain'"

empty_show=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.empty_comment;")
expect_not_contains "empty SHOW CREATE TABLE omits comment" "$empty_show" "COMMENT=''"

double_show=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.double_comment;")
expect_contains "double-quoted input renders single-quoted" "$double_show" "COMMENT='double'"

escaped_show=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.escaped_comment;")
expect_contains "escaped SHOW CREATE TABLE comment" "$escaped_show" "COMMENT='a''b\\\\c'"

dup_show=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.dup_comment;")
expect_contains "duplicate COMMENT last wins" "$dup_show" "COMMENT='second'"
expect_not_contains "duplicate COMMENT first omitted" "$dup_show" "COMMENT='first'"

like_show=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.like_comment;")
expect_contains "LIKE clones table comment" "$like_show" "COMMENT='a''b\\\\c'"

temp_show=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TEMPORARY TABLE temp_comment(id INT) ENGINE=InnoDB COMMENT='temp';
         SHOW CREATE TABLE temp_comment;"
)
expect_contains "temporary SHOW CREATE TABLE comment" "$temp_show" "CREATE TEMPORARY TABLE"
expect_contains "temporary comment rendered" "$temp_show" "COMMENT='temp'"

metadata=$(
    run_mysql \
        "SELECT TABLE_NAME, TABLE_COMMENT, CREATE_OPTIONS
         FROM INFORMATION_SCHEMA.TABLES
         WHERE TABLE_SCHEMA = '${DATABASE}'
           AND TABLE_NAME IN
               ('empty_comment','plain_comment','escaped_comment','dup_comment','like_comment')
         ORDER BY TABLE_NAME;"
)
expected_metadata=$(
    printf '%s\t%s\t\n' "dup_comment" "second"
    printf '%s\t%s\t\n' "empty_comment" ""
    printf '%s\t%s\t\n' "escaped_comment" "a'b\\c"
    printf '%s\t%s\t\n' "like_comment" "a'b\\c"
    printf '%s\t%s\t\n' "plain_comment" "plain"
)
expect_value "information_schema table comments" "$expected_metadata" "$metadata"

status_rows=$(
    run_mysql_with_headers \
        "SHOW TABLE STATUS FROM ${DATABASE}
         WHERE Name IN
             ('empty_comment','plain_comment','escaped_comment','dup_comment','like_comment');"
)
expect_contains "show table status headers include Comment" "$status_rows" "Create_options	Comment"
expect_contains "show table status plain comment" "$status_rows" "plain_comment"
expect_contains "show table status plain comment value" "$status_rows" "	plain"
expect_contains "show table status escaped comment value" "$status_rows" "a'b\\c"
expect_not_contains "temporary table omitted from status" "$status_rows" "temp_comment"

status=$(run_mysql "USE ${DATABASE}; CREATE TABLE status_ok(id INT) COMMENT='ok'; SELECT ROW_COUNT(), @@warning_count;")
expect_value "successful status" "0	0" "$status"

run_mysql \
    "USE ${DATABASE};
     SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES';
     CREATE TABLE no_backslash_comment(id INT) COMMENT='a\\\\b';" >/dev/null
no_backslash_show=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.no_backslash_comment;")
expect_contains "NO_BACKSLASH_ESCAPES SHOW CREATE TABLE comment" \
    "$no_backslash_show" "COMMENT='a\\\\\\\\b'"
no_backslash_hex=$(
    run_mysql \
        "SELECT HEX(TABLE_COMMENT)
         FROM INFORMATION_SCHEMA.TABLES
         WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'no_backslash_comment';"
)
expect_value "NO_BACKSLASH_ESCAPES stored literal backslashes" "615C5C62" "$no_backslash_hex"

comment2048=$(printf 'a%.0s' $(seq 1 2048))
run_mysql "USE ${DATABASE}; CREATE TABLE max_comment(id INT) COMMENT='${comment2048}';" >/dev/null
max_length=$(
    run_mysql \
        "SELECT LENGTH(TABLE_COMMENT)
         FROM INFORMATION_SCHEMA.TABLES
         WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'max_comment';"
)
expect_value "2048 character ASCII comment accepted" "2048" "$max_length"

comment2048_two_byte=$(printf '\303\251%.0s' $(seq 1 2048))
run_mysql \
    "USE ${DATABASE}; CREATE TABLE max_multibyte_comment(id INT) COMMENT='${comment2048_two_byte}';" \
    >/dev/null
max_multibyte_lengths=$(
    run_mysql \
        "SELECT LENGTH(TABLE_COMMENT), CHAR_LENGTH(TABLE_COMMENT)
         FROM INFORMATION_SCHEMA.TABLES
         WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'max_multibyte_comment';"
)
expect_value "2048 character multibyte comment accepted" "4096	2048" "$max_multibyte_lengths"

comment2049="${comment2048}a"
expect_error \
    "2049 character comment rejected" \
    1628 \
    HY000 \
    "Comment for table 'too_long' is too long (max = 2048)" \
    "USE ${DATABASE}; CREATE TABLE too_long(id INT) COMMENT='${comment2049}';"

expect_error \
    "numeric comment syntax" \
    1064 \
    42000 \
    "near '123'" \
    "USE ${DATABASE}; CREATE TABLE numeric_comment(id INT) COMMENT=123;"

expect_error \
    "identifier comment syntax" \
    1064 \
    42000 \
    "near 'abc'" \
    "USE ${DATABASE}; CREATE TABLE identifier_comment(id INT) COMMENT=abc;"

expect_error \
    "ANSI_QUOTES double quoted comment syntax" \
    1064 \
    42000 \
    "near '\"quoted\"'" \
    "USE ${DATABASE}; SET SESSION sql_mode = 'ANSI_QUOTES';
     CREATE TABLE ansi_comment(id INT) COMMENT \"quoted\";"
