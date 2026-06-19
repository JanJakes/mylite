#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_statistical_aggregates_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_statistical_aggregates_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
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
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE};
     CREATE TABLE t(
       id INT NOT NULL,
       g INT NULL,
       n INT NULL,
       nn INT NOT NULL,
       s VARCHAR(20) NULL
     ) ENGINE=InnoDB;
     CREATE TABLE empty_t(id INT NOT NULL, n INT NULL) ENGINE=InnoDB;
     CREATE TABLE all_null_t(id INT NOT NULL, n INT NULL) ENGINE=InnoDB;
     INSERT INTO t VALUES
       (1, NULL, NULL, 5, '1'),
       (2, 1, 10, 7, '2'),
       (3, 2, 20, 8, 'x'),
       (4, 2, 30, 9, NULL);
     INSERT INTO all_null_t VALUES (1, NULL), (2, NULL);" >/dev/null

core=$(run_mysql \
    "USE ${DATABASE};
     SELECT STD(n), STDDEV(n), STDDEV_POP(n), STDDEV_SAMP(n), VAR_POP(n),
            VAR_SAMP(n), VARIANCE(n)
       FROM t;
     SELECT STD(nn), STDDEV_POP(nn), STDDEV_SAMP(nn), VAR_POP(nn), VAR_SAMP(nn)
       FROM t;"
)
expect_value \
    "nullable statistical aliases" \
    "8.16496580927726	8.16496580927726	8.16496580927726	10	66.66666666666667	100	66.66666666666667" \
    "$(printf '%s\n' "$core" | sed -n '1p')"
expect_value \
    "not-null statistical values" \
    "1.479019945774904	1.479019945774904	1.707825127659933	2.1875	2.9166666666666665" \
    "$(printf '%s\n' "$core" | sed -n '2p')"

nulls=$(run_mysql \
    "USE ${DATABASE};
     SELECT STD(n), STDDEV_POP(n), STDDEV_SAMP(n), VAR_POP(n), VAR_SAMP(n)
       FROM empty_t;
     SELECT STD(n), STDDEV_POP(n), STDDEV_SAMP(n), VAR_POP(n), VAR_SAMP(n)
       FROM all_null_t;
     SELECT STDDEV_POP(n), STDDEV_SAMP(n), VAR_POP(n), VAR_SAMP(n)
       FROM t WHERE id = 2;
     SELECT STDDEV_POP(n), VAR_POP(n) FROM t WHERE id > 99;"
)
expect_value "empty statistical aggregates" "NULL	NULL	NULL	NULL	NULL" \
    "$(printf '%s\n' "$nulls" | sed -n '1p')"
expect_value "all-null statistical aggregates" "NULL	NULL	NULL	NULL	NULL" \
    "$(printf '%s\n' "$nulls" | sed -n '2p')"
expect_value "single non-null statistical aggregates" "0	NULL	0	NULL" \
    "$(printf '%s\n' "$nulls" | sed -n '3p')"
expect_value "where no-match statistical aggregates" "NULL	NULL" \
    "$(printf '%s\n' "$nulls" | sed -n '4p')"

grouped=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, STDDEV_POP(n), STDDEV_SAMP(n), VAR_POP(n), VAR_SAMP(n)
       FROM t GROUP BY g ORDER BY g;"
)
expect_value "group null statistical row" "NULL	NULL	NULL	NULL	NULL" \
    "$(printf '%s\n' "$grouped" | sed -n '1p')"
expect_value "group one statistical row" "1	0	NULL	0	NULL" \
    "$(printf '%s\n' "$grouped" | sed -n '2p')"
expect_value "group two statistical row" "2	5	7.0710678118654755	25	50" \
    "$(printf '%s\n' "$grouped" | sed -n '3p')"

expressions=$(run_mysql \
    "USE ${DATABASE};
     SELECT STDDEV_POP(n + 1), VAR_POP(n + 1) FROM t;
     SELECT STDDEV_POP(1), STDDEV_SAMP(1), VAR_POP(1), VAR_SAMP(1), STD(1),
            VARIANCE(1);
     SELECT STDDEV_POP(NULL), STDDEV_SAMP(NULL), VAR_POP(NULL), VAR_SAMP(NULL);"
)
expect_value "row-scalar statistical arguments" "8.16496580927726	66.66666666666667" \
    "$(printf '%s\n' "$expressions" | sed -n '1p')"
expect_value "tableless statistical arguments" "0	NULL	0	NULL	0	0" \
    "$(printf '%s\n' "$expressions" | sed -n '2p')"
expect_value "tableless null statistical arguments" "NULL	NULL	NULL	NULL" \
    "$(printf '%s\n' "$expressions" | sed -n '3p')"

window=$(run_mysql \
    "USE ${DATABASE};
     SELECT STDDEV_POP(n) OVER () FROM t ORDER BY id;"
)
expect_value "window syntax first row" "8.16496580927726" "$(printf '%s\n' "$window" | sed -n '1p')"
expect_value "window syntax fourth row" "8.16496580927726" "$(printf '%s\n' "$window" | sed -n '4p')"

expect_error \
    "distinct statistical aggregate" \
    1064 \
    42000 \
    "near 'DISTINCT n) FROM t'" \
    "USE ${DATABASE}; SELECT STDDEV_POP(DISTINCT n) FROM t;"
expect_error \
    "missing statistical argument" \
    1064 \
    42000 \
    "near ')'" \
    "USE ${DATABASE}; SELECT STDDEV_POP();"

printf '%s\n' "mysql_baseline_statistical_aggregates_expectations: ok"
