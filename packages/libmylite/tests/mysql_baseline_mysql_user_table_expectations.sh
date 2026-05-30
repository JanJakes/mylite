#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_mysql_user_table_expectations: $1" >&2
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

expect_show_table_status_row() {
    output=$(run_mysql "SHOW TABLE STATUS FROM mysql LIKE 'user';")
    field_count=$(printf '%s\n' "$output" | awk -F '\t' '{print NF}')
    prefix=$(printf '%s\n' "$output" | cut -f 1-11)
    create_time=$(printf '%s\n' "$output" | cut -f 12)
    stable_tail=$(printf '%s\n' "$output" | cut -f 13-18)

    if [ "$field_count" != "18" ]; then
        fail "SHOW TABLE STATUS user: expected 18 fields, got [$field_count]"
    fi
    expected_prefix=$(printf '%b' 'user\tInnoDB\t10\tDynamic\t5\t3276\t16384\t0\t0\t4194304\tNULL')
    if [ "$prefix" != "$expected_prefix" ]; then
        fail "SHOW TABLE STATUS user: expected prefix [$expected_prefix], got [$prefix]"
    fi
    case "$create_time" in
        ????-??-??\ ??:??:??) ;;
        *) fail "SHOW TABLE STATUS user: expected Create_time datetime, got [$create_time]" ;;
    esac
    expected_tail=$(
        printf '%b' \
            'NULL\tNULL\tutf8mb3_bin\tNULL\trow_format=DYNAMIC stats_persistent=0\tUsers and global privileges'
    )
    if [ "$stable_tail" != "$expected_tail" ]; then
        fail "SHOW TABLE STATUS user: expected tail [$expected_tail], got [$stable_tail]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output "mysql.user target row count" "5" "SELECT COUNT(*) FROM mysql.user;"

column_order_expected=$(
    printf '%b' \
        'Host\t1\n' \
        'User\t2\n' \
        'Select_priv\t3\n' \
        'Insert_priv\t4\n' \
        'Update_priv\t5\n' \
        'Delete_priv\t6\n' \
        'Create_priv\t7\n' \
        'Drop_priv\t8\n' \
        'Reload_priv\t9\n' \
        'Shutdown_priv\t10\n' \
        'Process_priv\t11\n' \
        'File_priv\t12\n' \
        'Grant_priv\t13\n' \
        'References_priv\t14\n' \
        'Index_priv\t15\n' \
        'Alter_priv\t16\n' \
        'Show_db_priv\t17\n' \
        'Super_priv\t18\n' \
        'Create_tmp_table_priv\t19\n' \
        'Lock_tables_priv\t20\n' \
        'Execute_priv\t21\n' \
        'Repl_slave_priv\t22\n' \
        'Repl_client_priv\t23\n' \
        'Create_view_priv\t24\n' \
        'Show_view_priv\t25\n' \
        'Create_routine_priv\t26\n' \
        'Alter_routine_priv\t27\n' \
        'Create_user_priv\t28\n' \
        'Event_priv\t29\n' \
        'Trigger_priv\t30\n' \
        'Create_tablespace_priv\t31\n' \
        'ssl_type\t32\n' \
        'ssl_cipher\t33\n' \
        'x509_issuer\t34\n' \
        'x509_subject\t35\n' \
        'max_questions\t36\n' \
        'max_updates\t37\n' \
        'max_connections\t38\n' \
        'max_user_connections\t39\n' \
        'plugin\t40\n' \
        'authentication_string\t41\n' \
        'password_expired\t42\n' \
        'password_last_changed\t43\n' \
        'password_lifetime\t44\n' \
        'account_locked\t45\n' \
        'Create_role_priv\t46\n' \
        'Drop_role_priv\t47\n' \
        'Password_reuse_history\t48\n' \
        'Password_reuse_time\t49\n' \
        'Password_require_current\t50\n' \
        'User_attributes\t51'
)
expect_output \
    "mysql.user column order" \
    "$column_order_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'user'
      ORDER BY ORDINAL_POSITION;"

show_columns_expected=$(
    printf '%b' \
        'Host\tchar(255)\tNO\tPRI\t\t\n' \
        'User\tchar(32)\tNO\tPRI\t\t\n' \
        'Select_priv\tenum('\''N'\'','\''Y'\'')\tNO\t\tN\t\n' \
        'ssl_type\tenum('\'''\'','\''ANY'\'','\''X509'\'','\''SPECIFIED'\'')\tNO\t\t\t\n' \
        'ssl_cipher\tblob\tNO\t\tNULL\t\n' \
        'max_questions\tint unsigned\tNO\t\t0\t\n' \
        'plugin\tchar(64)\tNO\t\tcaching_sha2_password\t\n' \
        'authentication_string\ttext\tYES\t\tNULL\t\n' \
        'password_last_changed\ttimestamp\tYES\t\tNULL\t\n' \
        'Password_require_current\tenum('\''N'\'','\''Y'\'')\tYES\t\tNULL\t\n' \
        'User_attributes\tjson\tYES\t\tNULL\t'
)
expect_output \
    "mysql.user representative SHOW COLUMNS rows" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM mysql.user
      WHERE Field IN ('Host','User','Select_priv','ssl_type','ssl_cipher',
                      'max_questions','plugin','authentication_string',
                      'password_last_changed','Password_require_current',
                      'User_attributes');"

full_columns_expected=$(
    printf '%b' \
        'Host\tchar(255)\tascii_general_ci\tNO\tPRI\t\t\tselect,insert,update,references\t\n' \
        'User\tchar(32)\tutf8mb3_bin\tNO\tPRI\t\t\tselect,insert,update,references\t\n' \
        'Select_priv\tenum('\''N'\'','\''Y'\'')\tutf8mb3_general_ci\tNO\t\tN\t\tselect,insert,update,references\t\n' \
        'ssl_type\tenum('\'''\'','\''ANY'\'','\''X509'\'','\''SPECIFIED'\'')\tutf8mb3_general_ci\tNO\t\t\t\tselect,insert,update,references\t\n' \
        'ssl_cipher\tblob\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\n' \
        'max_questions\tint unsigned\tNULL\tNO\t\t0\t\tselect,insert,update,references\t\n' \
        'plugin\tchar(64)\tutf8mb3_bin\tNO\t\tcaching_sha2_password\t\tselect,insert,update,references\t\n' \
        'authentication_string\ttext\tutf8mb3_bin\tYES\t\tNULL\t\tselect,insert,update,references\t\n' \
        'password_last_changed\ttimestamp\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\n' \
        'Password_require_current\tenum('\''N'\'','\''Y'\'')\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\n' \
        'User_attributes\tjson\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t'
)
expect_output \
    "mysql.user representative SHOW FULL COLUMNS rows" \
    "$full_columns_expected" \
    "SHOW FULL COLUMNS FROM mysql.user
      WHERE Field IN ('Host','User','Select_priv','ssl_type','ssl_cipher',
                      'max_questions','plugin','authentication_string',
                      'password_last_changed','Password_require_current',
                      'User_attributes');"

show_index_expected=$(
    printf '%b' \
        'user\t0\tPRIMARY\t1\tHost\tA\t2\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\n' \
        'user\t0\tPRIMARY\t2\tUser\tA\t5\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL'
)
expect_output "mysql.user SHOW INDEX rows" "$show_index_expected" "SHOW INDEX FROM mysql.user;"

information_schema_columns_expected=$(
    printf '%b' \
        'Host\t1\t\tNO\tchar\t255\t255\tNULL\tNULL\tNULL\tascii\tascii_general_ci\tchar(255)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'User\t2\t\tNO\tchar\t32\t96\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_bin\tchar(32)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'Select_priv\t3\tN\tNO\tenum\t1\t3\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tenum('\''N'\'','\''Y'\'')\t\t\tselect,insert,update,references\t\t\n' \
        'ssl_type\t32\t\tNO\tenum\t9\t27\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tenum('\'''\'','\''ANY'\'','\''X509'\'','\''SPECIFIED'\'')\t\t\tselect,insert,update,references\t\t\n' \
        'ssl_cipher\t33\tNULL\tNO\tblob\t65535\t65535\tNULL\tNULL\tNULL\tNULL\tNULL\tblob\t\t\tselect,insert,update,references\t\t\n' \
        'max_questions\t36\t0\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint unsigned\t\t\tselect,insert,update,references\t\t\n' \
        'plugin\t40\tcaching_sha2_password\tNO\tchar\t64\t192\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_bin\tchar(64)\t\t\tselect,insert,update,references\t\t\n' \
        'authentication_string\t41\tNULL\tYES\ttext\t65535\t65535\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_bin\ttext\t\t\tselect,insert,update,references\t\t\n' \
        'password_last_changed\t43\tNULL\tYES\ttimestamp\tNULL\tNULL\tNULL\tNULL\t0\tNULL\tNULL\ttimestamp\t\t\tselect,insert,update,references\t\t\n' \
        'Password_require_current\t50\tNULL\tYES\tenum\t1\t3\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tenum('\''N'\'','\''Y'\'')\t\t\tselect,insert,update,references\t\t\n' \
        'User_attributes\t51\tNULL\tYES\tjson\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tjson\t\t\tselect,insert,update,references\t\t'
)
expect_output \
    "mysql.user representative INFORMATION_SCHEMA.COLUMNS rows" \
    "$information_schema_columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
            CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
            NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME,
            COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT,
            GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'user'
        AND COLUMN_NAME IN ('Host','User','Select_priv','ssl_type','ssl_cipher',
                            'max_questions','plugin','authentication_string',
                            'password_last_changed','Password_require_current',
                            'User_attributes')
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "mysql.user INFORMATION_SCHEMA.STATISTICS rows" \
    "$(printf '%b' 'PRIMARY\t1\tHost\tA\t2\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL\nPRIMARY\t2\tUser\tA\t5\tNULL\tNULL\t\tBTREE\t\t\tYES\tNULL')" \
    "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, CARDINALITY, SUB_PART,
            PACKED, NULLABLE, INDEX_TYPE, COMMENT, INDEX_COMMENT, IS_VISIBLE,
            EXPRESSION
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'user'
      ORDER BY INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "mysql.user INFORMATION_SCHEMA.TABLE_CONSTRAINTS row" \
    "$(printf '%b' 'PRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'user';"

expect_output \
    "mysql.user INFORMATION_SCHEMA.KEY_COLUMN_USAGE rows" \
    "$(printf '%b' 'PRIMARY\tHost\t1\tNULL\tNULL\tNULL\tNULL\nPRIMARY\tUser\t2\tNULL\tNULL\tNULL\tNULL')" \
    "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION,
            POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA,
            REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'user'
      ORDER BY CONSTRAINT_NAME, ORDINAL_POSITION;"

expect_output \
    "mysql.user INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS row" \
    "$(printf '%b' 'PRIMARY\tuser\tNULL\tNULL')" \
    "SELECT CONSTRAINT_NAME, TABLE_NAME, ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'mysql' AND TABLE_NAME = 'user';"

expect_output \
    "mysql.user INFORMATION_SCHEMA.TABLES row" \
    "$(printf '%b' 'user\tBASE TABLE\tInnoDB\t10\tDynamic\t5\t3276\t16384\t0\t0\t4194304\tNULL\t1\t1\t1\tutf8mb3_bin\t1\trow_format=DYNAMIC stats_persistent=0\tUsers and global privileges')" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL, CREATE_OPTIONS,
            TABLE_COMMENT
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'user';"

expect_show_table_status_row

printf '%s\n' "mysql_baseline_mysql_user_table_expectations: ok"
