#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_stored_program_script_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_stored_program_script_placeholders_expectations: $1" >&2
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

expect_output \
    "compound stored procedure definition" \
    "" \
    "CREATE DATABASE ${DATABASE};
USE ${DATABASE};
DELIMITER //
SET sql_mode = default//
CREATE PROCEDURE p(IN x INT)
BEGIN
  DECLARE y INT DEFAULT 1;
  SELECT x + y;
END//
DROP PROCEDURE p//
DROP DATABASE ${DATABASE}//"

expect_output \
    "stored program body statement definitions" \
    "" \
    "CREATE DATABASE ${DATABASE};
USE ${DATABASE};
DELIMITER //
SET sql_mode = default//
CREATE TABLE t(id INT)//
CREATE PROCEDURE body_features()
BEGIN
  DECLARE done BOOL DEFAULT FALSE;
  DECLARE v INT DEFAULT 0;
  DECLARE no_more_rows CONDITION FOR SQLSTATE '02000';
  DECLARE cur CURSOR FOR SELECT id FROM t;
  DECLARE CONTINUE HANDLER FOR no_more_rows SET done = TRUE;
  body_label: BEGIN
    IF v = 0 THEN
      SET v = 1;
    ELSEIF v = 1 THEN
      SET v = 2;
    ELSE
      SET v = 3;
    END IF;
    CASE v
      WHEN 1 THEN SET v = 2;
      ELSE SET v = 4;
    END CASE;
    loop_label: LOOP
      SET v = v + 1;
      IF v > 4 THEN
        LEAVE loop_label;
      END IF;
      ITERATE loop_label;
    END LOOP loop_label;
    REPEAT
      SET v = v - 1;
    UNTIL v = 0 END REPEAT;
    WHILE v < 1 DO
      SET v = v + 1;
    END WHILE;
    OPEN cur;
    FETCH cur INTO v;
    CLOSE cur;
  END body_label;
END//
CREATE FUNCTION f_body() RETURNS INT DETERMINISTIC NO SQL
BEGIN
  RETURN 1;
END//
DROP FUNCTION f_body//
DROP PROCEDURE body_features//
DROP TABLE t//
DROP DATABASE ${DATABASE}//"

expect_output \
    "top-level signal warning" \
    "$(printf '%b' 'Warning\t1642\tUnhandled user-defined warning condition\n1')" \
    "SIGNAL SQLSTATE '01000'; SHOW WARNINGS; SELECT @@warning_count;"

expect_error \
    "top-level resignal without handler" \
    1645 \
    0K000 \
    "RESIGNAL when handler not active" \
    "RESIGNAL;"

printf '%s\n' "mysql_parser_corpus_stored_program_script_placeholders_expectations: ok"
