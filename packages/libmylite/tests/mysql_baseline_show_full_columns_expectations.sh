#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_show_full_columns_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"

fail() {
    printf '%s\n' "mysql_baseline_show_full_columns_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw "$@"
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

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE}; DROP DATABASE IF EXISTS ${OTHER_DATABASE};" \
        >/dev/null 2>&1 || true
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
     CREATE DATABASE ${OTHER_DATABASE};
     USE ${DATABASE};
     CREATE TABLE numbers(
       id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
       i INTEGER NULL,
       dec_col DECIMAL(5,2) NULL,
       f FLOAT NULL,
       y YEAR NULL,
       d DATE NULL,
       tm TIME NULL,
       dt DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
       ts TIMESTAMP NULL DEFAULT CURRENT_TIMESTAMP,
       c CHAR(3) NOT NULL,
       v VARCHAR(10) DEFAULT 'x',
       txt TEXT,
       b BINARY(2),
       vb VARBINARY(3),
       bits BIT(3),
       e ENUM('a','b'),
       s SET('a','b'),
       j JSON,
       inv INT INVISIBLE,
       KEY idx_v (v(3))
     ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;" >/dev/null

expected_columns="Field	Type	Collation	Null	Key	Default	Extra	Privileges	Comment"
tab=$(printf '\t')
empty_comment="<empty>"
expected_rows="id	int	NULL	NO	PRI	NULL	auto_increment	select,insert,update,references	${empty_comment}
i	int	NULL	YES		NULL		select,insert,update,references	${empty_comment}
dec_col	decimal(5,2)	NULL	YES		NULL		select,insert,update,references	${empty_comment}
f	float	NULL	YES		NULL		select,insert,update,references	${empty_comment}
y	year	NULL	YES		NULL		select,insert,update,references	${empty_comment}
d	date	NULL	YES		NULL		select,insert,update,references	${empty_comment}
tm	time	NULL	YES		NULL		select,insert,update,references	${empty_comment}
dt	datetime	NULL	YES		CURRENT_TIMESTAMP	DEFAULT_GENERATED on update CURRENT_TIMESTAMP	select,insert,update,references	${empty_comment}
ts	timestamp	NULL	YES		CURRENT_TIMESTAMP	DEFAULT_GENERATED	select,insert,update,references	${empty_comment}
c	char(3)	utf8mb4_0900_ai_ci	NO		NULL		select,insert,update,references	${empty_comment}
v	varchar(10)	utf8mb4_0900_ai_ci	YES	MUL	x		select,insert,update,references	${empty_comment}
txt	text	utf8mb4_0900_ai_ci	YES		NULL		select,insert,update,references	${empty_comment}
b	binary(2)	NULL	YES		NULL		select,insert,update,references	${empty_comment}
vb	varbinary(3)	NULL	YES		NULL		select,insert,update,references	${empty_comment}
bits	bit(3)	NULL	YES		NULL		select,insert,update,references	${empty_comment}
e	enum('a','b')	utf8mb4_0900_ai_ci	YES		NULL		select,insert,update,references	${empty_comment}
s	set('a','b')	utf8mb4_0900_ai_ci	YES		NULL		select,insert,update,references	${empty_comment}
j	json	NULL	YES		NULL		select,insert,update,references	${empty_comment}
inv	int	NULL	YES		NULL	INVISIBLE	select,insert,update,references	${empty_comment}"

normalize_empty_comment() {
    sed "s/${tab}\$/${tab}${empty_comment}/"
}

check_show_output() {
    label=$1
    sql=$2

    output=$(run_mysql_with_headers "$sql")
    headers=$(printf '%s\n' "$output" | sed -n '1p')
    rows=$(printf '%s\n' "$output" | sed '1d' | normalize_empty_comment)

    expect_value "$label headers" "$expected_columns" "$headers"
    expect_value "$label rows" "$expected_rows" "$rows"
}

check_show_output "show full columns from table" "USE ${DATABASE}; SHOW FULL COLUMNS FROM numbers;"
check_show_output "show full columns in table" "USE ${DATABASE}; SHOW FULL COLUMNS IN numbers;"
check_show_output "show full fields from table" "USE ${DATABASE}; SHOW FULL FIELDS FROM numbers;"
check_show_output "show full fields in table" "USE ${DATABASE}; SHOW FULL FIELDS IN numbers;"
check_show_output "schema-qualified show full columns" "SHOW FULL COLUMNS FROM ${DATABASE}.numbers;"
check_show_output "show full columns from table from schema" \
    "SHOW FULL COLUMNS FROM numbers FROM ${DATABASE};"
check_show_output "show full columns from table in schema" \
    "SHOW FULL COLUMNS FROM numbers IN ${DATABASE};"
check_show_output "show full columns in table from schema" \
    "SHOW FULL COLUMNS IN numbers FROM ${DATABASE};"
check_show_output "show full columns in table in schema" \
    "SHOW FULL COLUMNS IN numbers IN ${DATABASE};"
check_show_output "show full fields from table from schema" \
    "SHOW FULL FIELDS FROM numbers FROM ${DATABASE};"
check_show_output "show full fields from table in schema" \
    "SHOW FULL FIELDS FROM numbers IN ${DATABASE};"
check_show_output "show full fields in table from schema" \
    "SHOW FULL FIELDS IN numbers FROM ${DATABASE};"
check_show_output "show full fields in table in schema" \
    "SHOW FULL FIELDS IN numbers IN ${DATABASE};"

like_output=$(run_mysql_with_headers "USE ${DATABASE}; SHOW FULL COLUMNS FROM numbers LIKE 'v%';")
expect_value "show full like headers" "$expected_columns" "$(printf '%s\n' "$like_output" | sed -n '1p')"
expect_value "show full like rows" \
    "v	varchar(10)	utf8mb4_0900_ai_ci	YES	MUL	x		select,insert,update,references	${empty_comment}
vb	varbinary(3)	NULL	YES		NULL		select,insert,update,references	${empty_comment}" \
    "$(printf '%s\n' "$like_output" | sed '1d' | normalize_empty_comment)"

status=$(run_mysql "USE ${DATABASE}; SHOW FULL COLUMNS FROM numbers; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "show full columns status" "0	-1" "$status"

run_mysql "CREATE TABLE ${OTHER_DATABASE}.numbers(other_id BIGINT NULL);" >/dev/null
other_output=$(run_mysql_with_headers "SHOW FULL COLUMNS FROM ${DATABASE}.numbers FROM ${OTHER_DATABASE};")
expect_value "trailing schema wins headers" "$expected_columns" "$(printf '%s\n' "$other_output" | sed -n '1p')"
expect_value "trailing schema wins rows" \
    "other_id	bigint	NULL	YES		NULL		select,insert,update,references	${empty_comment}" \
    "$(printf '%s\n' "$other_output" | sed '1d' | normalize_empty_comment)"

where_output=$(run_mysql_with_headers "USE ${DATABASE}; SHOW FULL COLUMNS FROM numbers WHERE Field = 'v';")
expect_value "where accepted headers" "$expected_columns" "$(printf '%s\n' "$where_output" | sed -n '1p')"
expect_value "where accepted row" \
    "v	varchar(10)	utf8mb4_0900_ai_ci	YES	MUL	x		select,insert,update,references	${empty_comment}" \
    "$(printf '%s\n' "$where_output" | sed '1d' | normalize_empty_comment)"

run_mysql "USE ${DATABASE}; CREATE TABLE extended_only(x INT);" >/dev/null
extended_output=$(run_mysql_with_headers "USE ${DATABASE}; SHOW EXTENDED FULL COLUMNS FROM extended_only;")
expect_value "extended full headers" "$expected_columns" "$(printf '%s\n' "$extended_output" | sed -n '1p')"
expect_value "extended includes hidden row id" \
    "DB_ROW_ID" \
    "$(printf '%s\n' "$extended_output" | awk -F '\t' '$1 == "DB_ROW_ID" { print $1; exit }')"

expect_error \
    "missing default schema show full columns" \
    1046 \
    3D000 \
    "No database selected" \
    "SHOW FULL COLUMNS FROM numbers;"

expect_error \
    "unknown schema qualified show full columns" \
    1049 \
    42000 \
    "Unknown database 'missing_schema'" \
    "SHOW FULL COLUMNS FROM missing_schema.numbers;"

expect_error \
    "unknown table show full columns" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "SHOW FULL COLUMNS FROM missing_table;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_show_full_columns_expectations: ok"
