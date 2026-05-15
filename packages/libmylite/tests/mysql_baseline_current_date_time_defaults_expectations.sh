#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_current_date_time_defaults_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_current_date_time_defaults_expectations: $1" >&2
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

show_create_expected=$(cat <<\EXPECTED
generated_temporals	CREATE TABLE `generated_temporals` (
  `id` int DEFAULT NULL,
  `d1` date DEFAULT (curdate()),
  `d2` date DEFAULT (curdate()),
  `d3` date DEFAULT (curdate()),
  `tm1` time DEFAULT (curtime()),
  `tm2` time DEFAULT (curtime()),
  `tm3` time DEFAULT (curtime())
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "create table renders generated date and time defaults" \
    "$show_create_expected" \
    "SET time_zone = '+00:00'; "\
"CREATE TABLE generated_temporals ("\
"id INT, "\
"d1 DATE DEFAULT (CURDATE()), "\
"d2 DATE DEFAULT (CURRENT_DATE), "\
"d3 DATE DEFAULT (CURRENT_DATE()), "\
"tm1 TIME DEFAULT (CURTIME()), "\
"tm2 TIME DEFAULT (CURRENT_TIME), "\
"tm3 TIME DEFAULT (CURRENT_TIME())); "\
"SHOW CREATE TABLE generated_temporals;" \
    "$DATABASE"

show_columns_expected=$(
    printf 'id\tint\tYES\t\tNULL\t\n'
    printf 'd1\tdate\tYES\t\tcurdate()\tDEFAULT_GENERATED\n'
    printf 'd2\tdate\tYES\t\tcurdate()\tDEFAULT_GENERATED\n'
    printf 'd3\tdate\tYES\t\tcurdate()\tDEFAULT_GENERATED\n'
    printf 'tm1\ttime\tYES\t\tcurtime()\tDEFAULT_GENERATED\n'
    printf 'tm2\ttime\tYES\t\tcurtime()\tDEFAULT_GENERATED\n'
    printf 'tm3\ttime\tYES\t\tcurtime()\tDEFAULT_GENERATED'
)
expect_output \
    "show columns renders generated date and time defaults" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM generated_temporals;" \
    "$DATABASE"

information_schema_expected=$(cat <<\EXPECTED
id	int	NULL	YES		NULL
d1	date	curdate()	YES	DEFAULT_GENERATED	NULL
d2	date	curdate()	YES	DEFAULT_GENERATED	NULL
d3	date	curdate()	YES	DEFAULT_GENERATED	NULL
tm1	time	curtime()	YES	DEFAULT_GENERATED	0
tm2	time	curtime()	YES	DEFAULT_GENERATED	0
tm3	time	curtime()	YES	DEFAULT_GENERATED	0
EXPECTED
)
expect_output \
    "information schema renders generated date and time defaults" \
    "$information_schema_expected" \
    "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_DEFAULT, IS_NULLABLE, EXTRA, DATETIME_PRECISION "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME = 'generated_temporals' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

expect_output \
    "dml materializes generated date and time defaults" \
    "after_insert	1	0	1	2023-11-14	22:13:20
after_values_default	1	0	2	2023-11-14	22:14:20
after_insert_set	1	0	3	2023-11-14	22:15:20
after_replace	1	0	4	2023-11-14	22:16:20
after_update_default	1	0	1	2023-11-14	22:17:20
after_update_noop	0	0	1	2023-11-14	22:17:20" \
    "SET time_zone = '+00:00'; "\
"CREATE TABLE dml_defaults (id INT, d DATE DEFAULT (CURDATE()), tm TIME DEFAULT (CURTIME())); "\
"SET timestamp = 1700000000; INSERT INTO dml_defaults(id) VALUES (1); "\
"SELECT 'after_insert', ROW_COUNT(), @@warning_count, id, d, tm FROM dml_defaults WHERE id = 1; "\
"SET timestamp = 1700000060; INSERT INTO dml_defaults VALUES (2, DEFAULT, DEFAULT); "\
"SELECT 'after_values_default', ROW_COUNT(), @@warning_count, id, d, tm "\
"FROM dml_defaults WHERE id = 2; "\
"SET timestamp = 1700000120; INSERT INTO dml_defaults SET id = 3, d = DEFAULT, tm = DEFAULT; "\
"SELECT 'after_insert_set', ROW_COUNT(), @@warning_count, id, d, tm "\
"FROM dml_defaults WHERE id = 3; "\
"SET timestamp = 1700000180; REPLACE INTO dml_defaults VALUES (4, DEFAULT, DEFAULT); "\
"SELECT 'after_replace', ROW_COUNT(), @@warning_count, id, d, tm FROM dml_defaults WHERE id = 4; "\
"SET timestamp = 1700000240; UPDATE dml_defaults SET d = DEFAULT, tm = DEFAULT WHERE id = 1; "\
"SELECT 'after_update_default', ROW_COUNT(), @@warning_count, id, d, tm "\
"FROM dml_defaults WHERE id = 1; "\
"UPDATE dml_defaults SET d = DEFAULT, tm = DEFAULT WHERE id = 1; "\
"SELECT 'after_update_noop', ROW_COUNT(), @@warning_count, id, d, tm "\
"FROM dml_defaults WHERE id = 1;" \
    "$DATABASE"

alter_expected=$(cat <<\EXPECTED
altered	CREATE TABLE `altered` (
  `id` int DEFAULT NULL,
  `d` date NOT NULL DEFAULT (curdate()),
  `tm` time NOT NULL DEFAULT (curtime())
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1	2023-11-14	22:13:20
2	2023-11-14	22:13:20
d	curdate()	DEFAULT_GENERATED
tm	curtime()	DEFAULT_GENERATED
3	2023-11-14	22:14:20
EXPECTED
)
expect_output \
    "alter add and set generated date and time defaults" \
    "$alter_expected" \
    "SET time_zone = '+00:00'; CREATE TABLE altered (id INT); INSERT INTO altered VALUES (1), (2); "\
"SET timestamp = 1700000000; "\
"ALTER TABLE altered ADD COLUMN d DATE NOT NULL DEFAULT (CURDATE()), "\
"ADD COLUMN tm TIME NOT NULL DEFAULT (CURTIME()); "\
"SHOW CREATE TABLE altered; SELECT * FROM altered ORDER BY id; "\
"SET timestamp = 1700000060; "\
"ALTER TABLE altered ALTER COLUMN d SET DEFAULT (CURRENT_DATE), "\
"ALTER COLUMN tm SET DEFAULT (CURRENT_TIME); "\
"SELECT COLUMN_NAME, COLUMN_DEFAULT, EXTRA FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'altered' AND COLUMN_NAME IN ('d', 'tm') "\
"ORDER BY ORDINAL_POSITION; "\
"INSERT INTO altered(id) VALUES (3); SELECT * FROM altered WHERE id = 3;" \
    "$DATABASE"

like_expected=$(cat <<\EXPECTED
clone	CREATE TABLE `clone` (
  `id` int DEFAULT NULL,
  `d` date DEFAULT (curdate()),
  `tm` time DEFAULT (curtime())
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1	2023-11-14	22:13:20
EXPECTED
)
expect_output \
    "create table like preserves generated defaults" \
    "$like_expected" \
    "SET time_zone = '+00:00'; SET timestamp = 1700000000; "\
"CREATE TABLE source_like (id INT, d DATE DEFAULT (CURDATE()), tm TIME DEFAULT (CURTIME())); "\
"CREATE TABLE clone LIKE source_like; SHOW CREATE TABLE clone; "\
"INSERT INTO clone(id) VALUES (1); SELECT * FROM clone;" \
    "$DATABASE"

expect_error \
    "date current default must be parenthesized" \
    1064 \
    "42000" \
    "right syntax" \
    "CREATE TABLE bad_date_syntax (d DATE DEFAULT CURRENT_DATE);" \
    "$DATABASE"

expect_error \
    "time current default must be parenthesized" \
    1064 \
    "42000" \
    "right syntax" \
    "CREATE TABLE bad_time_syntax (tm TIME DEFAULT CURRENT_TIME);" \
    "$DATABASE"

wider_expression_expected=$(cat <<\EXPECTED
bad_int	CREATE TABLE `bad_int` (
  `i` int DEFAULT (curdate())
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
bad_date_now	CREATE TABLE `bad_date_now` (
  `d` date DEFAULT (now())
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
bad_time_timestamp	CREATE TABLE `bad_time_timestamp` (
  `tm` time DEFAULT (now())
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
bad_time_fsp	CREATE TABLE `bad_time_fsp` (
  `tm` time DEFAULT (curtime())
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "wider mysql expression defaults remain deferred by mylite" \
    "$wider_expression_expected" \
    "CREATE TABLE bad_int (i INT DEFAULT (CURDATE())); "\
"SHOW CREATE TABLE bad_int; "\
"CREATE TABLE bad_date_now (d DATE DEFAULT (NOW())); "\
"SHOW CREATE TABLE bad_date_now; "\
"CREATE TABLE bad_time_timestamp (tm TIME DEFAULT (CURRENT_TIMESTAMP)); "\
"SHOW CREATE TABLE bad_time_timestamp; "\
"CREATE TABLE bad_time_fsp (tm TIME DEFAULT (CURRENT_TIME(0))); "\
"SHOW CREATE TABLE bad_time_fsp;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_current_date_time_defaults_expectations: ok"
