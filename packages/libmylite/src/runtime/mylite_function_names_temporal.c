#include "mylite_function_names.h"

#include "mylite_function_name_match.h"

bool mylite_function_name_is_date_extraction(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"DATE"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_datediff(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"DATEDIFF"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_last_day(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"LAST_DAY"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_timestampdiff(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"TIMESTAMPDIFF"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_to_days(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"TO_DAYS"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_to_seconds(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"TO_SECONDS"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_from_days(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"FROM_DAYS"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_from_unixtime(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"FROM_UNIXTIME"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_time_extraction(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"TIME"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_time_to_sec(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"TIME_TO_SEC"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_sec_to_time(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"SEC_TO_TIME"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_year_part(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"YEAR"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_month_part(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"MONTH"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_day_part(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"DAY", "DAYOFMONTH"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_dayofweek_part(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"DAYOFWEEK"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_dayofyear_part(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"DAYOFYEAR"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_quarter_part(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"QUARTER"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_hour_part(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"HOUR"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_minute_part(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"MINUTE"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_second_part(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"SECOND"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_microsecond_part(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"MICROSECOND"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_extract(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"EXTRACT"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_date_interval_arithmetic(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {
        "TIMESTAMPADD", "DATE_ADD", "DATE_SUB", "ADDDATE", "SUBDATE",
    };

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}
