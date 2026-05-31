#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
SQL_MODE="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION"
ACTION_STATEMENT=$(printf '%b' 'BEGIN\n    IF @sys.ignore_sys_config_triggers != true AND NEW.set_by IS NULL THEN\n        SET NEW.set_by = USER();\n    END IF;\nEND')

fail() {
    printf '%s\n' "mysql_baseline_sys_sys_config_triggers_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
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

normalize_created_timestamps() {
    sed -E 's/[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{2}/<created>/g'
}

expect_show_triggers_output() {
    label=$1
    sql=$2

    output=$(run_mysql "$sql" | normalize_created_timestamps)
    expected=$(
        {
            printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
                "sys_config_insert_set_user" \
                "INSERT" \
                "sys_config" \
                "$ACTION_STATEMENT" \
                "BEFORE" \
                "<created>" \
                "$SQL_MODE" \
                "mysql.sys@localhost" \
                "utf8mb4" \
                "utf8mb4_0900_ai_ci" \
                "utf8mb4_0900_ai_ci"
            printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
                "sys_config_update_set_user" \
                "UPDATE" \
                "sys_config" \
                "$ACTION_STATEMENT" \
                "BEFORE" \
                "<created>" \
                "$SQL_MODE" \
                "mysql.sys@localhost" \
                "utf8mb4" \
                "utf8mb4_0900_ai_ci" \
                "utf8mb4_0900_ai_ci"
        }
    )
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output \
    "sys trigger count" \
    "2" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA = 'sys';"

triggers_expected=$(
    {
        printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
            "sys_config_insert_set_user" \
            "INSERT" \
            "sys_config" \
            "1" \
            "NULL" \
            "$ACTION_STATEMENT" \
            "ROW" \
            "BEFORE" \
            "OLD" \
            "NEW" \
            "1" \
            "$SQL_MODE" \
            "mysql.sys@localhost" \
            "utf8mb4" \
            "utf8mb4_0900_ai_ci" \
            "utf8mb4_0900_ai_ci"
        printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
            "sys_config_update_set_user" \
            "UPDATE" \
            "sys_config" \
            "1" \
            "NULL" \
            "$ACTION_STATEMENT" \
            "ROW" \
            "BEFORE" \
            "OLD" \
            "NEW" \
            "1" \
            "$SQL_MODE" \
            "mysql.sys@localhost" \
            "utf8mb4" \
            "utf8mb4_0900_ai_ci" \
            "utf8mb4_0900_ai_ci"
    }
)
expect_output \
    "sys trigger INFORMATION_SCHEMA rows" \
    "$triggers_expected" \
    "SELECT TRIGGER_NAME, EVENT_MANIPULATION, EVENT_OBJECT_TABLE, ACTION_ORDER,
            ACTION_CONDITION, ACTION_STATEMENT, ACTION_ORIENTATION, ACTION_TIMING,
            ACTION_REFERENCE_OLD_ROW, ACTION_REFERENCE_NEW_ROW, CREATED IS NOT NULL,
            SQL_MODE, DEFINER, CHARACTER_SET_CLIENT, COLLATION_CONNECTION,
            DATABASE_COLLATION
       FROM INFORMATION_SCHEMA.TRIGGERS
      WHERE TRIGGER_SCHEMA = 'sys' AND EVENT_OBJECT_TABLE = 'sys_config'
      ORDER BY TRIGGER_NAME;"

expect_output \
    "trigger-name LIKE query" \
    "1" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TRIGGERS
      WHERE TRIGGER_SCHEMA = 'sys' AND TRIGGER_NAME LIKE 'sys_config_insert%';"

expect_show_triggers_output \
    "SHOW TRIGGERS FROM sys LIKE sys_config" \
    "SHOW TRIGGERS FROM sys LIKE 'sys_config';"

expect_show_triggers_output \
    "SHOW FULL TRIGGERS FROM sys LIKE sys_config" \
    "SHOW FULL TRIGGERS FROM sys LIKE 'sys_config';"

expect_show_triggers_output \
    "selected-schema SHOW TRIGGERS LIKE sys_config" \
    "USE sys; SHOW TRIGGERS LIKE 'sys_config';"

expect_output \
    "SHOW TRIGGERS LIKE trigger name is empty" \
    "" \
    "SHOW TRIGGERS FROM sys LIKE 'sys_config_insert%';"

printf '%s\n' "mysql_baseline_sys_sys_config_triggers_expectations: ok"
