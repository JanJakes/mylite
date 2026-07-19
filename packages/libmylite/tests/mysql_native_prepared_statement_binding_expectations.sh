#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_native_prepared_binding_expectations_$$"

fail() {
    printf '%s\n' "mysql_native_prepared_statement_binding_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
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

trap cleanup EXIT HUP INT TERM

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

run_mysql "DROP DATABASE IF EXISTS ${DATABASE}; CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "sql-mode-independent parameter data" \
    "2" \
    "CREATE TABLE t (id INT PRIMARY KEY, value VARCHAR(100));
     INSERT INTO t VALUES (1, 'alpha'), (2, 'beta');
     PREPARE s FROM 'SELECT id FROM t WHERE value = ? ORDER BY id';
     SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES';
     SET @p = CONVERT(0x6d697373696e6727204f5220313d31202d2d20 USING utf8mb4);
     EXECUTE s USING @p;
     SELECT COUNT(*) FROM t;
     DEALLOCATE PREPARE s;" \
    "$DATABASE"

expect_output \
    "marker recognition ignores quoted and commented question marks" \
    "?	7" \
    "SET SESSION sql_mode = '';
     PREPARE s FROM 'SELECT ''?'' AS literal_value, ? AS bound_value /* ? */';
     SET @p = 7;
     EXECUTE s USING @p;
     DEALLOCATE PREPARE s;" \
    "$DATABASE"

expect_output \
    "binary parameter preserves embedded nul" \
    "3	610062" \
    "PREPARE s FROM 'SELECT LENGTH(?), HEX(?)';
     SET @p = UNHEX('610062');
     EXECUTE s USING @p, @p;
     DEALLOCATE PREPARE s;" \
    "$DATABASE"

expect_output \
    "limit and offset parameters" \
    "2" \
    "PREPARE s FROM 'SELECT id FROM t ORDER BY id LIMIT ? OFFSET ?';
     SET @limit_value = 1, @offset_value = 1;
     EXECUTE s USING @limit_value, @offset_value;
     DEALLOCATE PREPARE s;" \
    "$DATABASE"

expect_output \
    "repeated execution accepts input type changes" \
    "31	0
610062	0
NULL	1" \
    "PREPARE s FROM 'SELECT HEX(CONCAT('''', ?)), ? IS NULL';
     SET @value = 1;
     EXECUTE s USING @value, @value;
     SET @value = UNHEX('610062');
     EXECUTE s USING @value, @value;
     SET @value = NULL;
     EXECUTE s USING @value, @value;
     DEALLOCATE PREPARE s;" \
    "$DATABASE"

expect_output \
    "schema change reparses prepared metadata" \
    "1
1" \
    "CREATE TABLE metadata_t (value INT);
     INSERT INTO metadata_t VALUES (1);
     PREPARE s FROM 'SELECT value FROM metadata_t WHERE value = ?';
     SET @value = 1;
     EXECUTE s USING @value;
     ALTER TABLE metadata_t ADD COLUMN extra INT;
     EXECUTE s USING @value;
     DEALLOCATE PREPARE s;" \
    "$DATABASE"

expect_error \
    "all parameters must be supplied" \
    1210 \
    HY000 \
    "Incorrect arguments to EXECUTE" \
    "PREPARE s FROM 'SELECT ?'; EXECUTE s;" \
    "$DATABASE"

expect_error \
    "parameter cannot replace an identifier" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "PREPARE s FROM 'SELECT * FROM ?';" \
    "$DATABASE"

command -v php >/dev/null 2>&1 || fail "php is required for mysqli packet expectations"
mysql_host=$(
    docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' \
        "$MYSQL_CONTAINER"
)
[ -n "$mysql_host" ] || fail "could not resolve MySQL container address"

MYLITE_MYSQL_EXPECTATION_HOST="$mysql_host" php <<'PHP'
<?php

mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);

$host = getenv('MYLITE_MYSQL_EXPECTATION_HOST');
$admin = new mysqli($host, 'root', '');
$admin->query('SET GLOBAL max_allowed_packet = 1048576');
$admin->close();

$connection = new mysqli($host, 'root', '');
$statement = $connection->prepare('SELECT LENGTH(?), LENGTH(?)');
$first = str_repeat('a', 700000);
$second = str_repeat('b', 700000);
$statement->bind_param('ss', $first, $second);

try {
    $statement->execute();
    throw new RuntimeException('aggregate oversized prepared packet unexpectedly succeeded');
} catch (mysqli_sql_exception $exception) {
    if ($exception->getCode() !== 1153 || $exception->getSqlState() !== '08S01') {
        throw $exception;
    }
}

$statement->close();
$connection->close();
PHP

printf '%s\n' "mysql_native_prepared_statement_binding_expectations: ok"
