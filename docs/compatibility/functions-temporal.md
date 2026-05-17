# Temporal functions

Date, time, timestamp, interval, time-zone, and calendar functions.

| Function or operator | Status | Notes |
| --- | --- | --- |
| `ADDDATE()` | 🟡 | Limited no-source, `FROM DUAL`, and `DO` `ADDDATE(date_or_datetime_string, INTERVAL signed_integer_or_NULL SECOND)` alias of the current `DATE_ADD()` interval-second slice; no `ADDDATE(date, days)`, other units, interval expressions, table-backed expressions, invalid-date warning semantics, or general temporal arithmetic |
| `ADDTIME()` | ❌ | Add time |
| `CONVERT_TZ()` | ❌ | Convert from one time zone to another |
| `CURDATE()` | 🟡 | Limited zero-fractional statement date in no-source, `FROM DUAL`, `DO`, row-scalar projection, compatible `DATE` DML assignment, and parenthesized generated `DATE` defaults; respects the limited session `timestamp` override and session `time_zone` offset; no named time-zone tables or general temporal expression evaluation |
| `CURRENT_DATE(), CURRENT_DATE` | 🟡 | Limited zero-fractional statement date synonyms for `CURDATE()` in the same supported positions, including parenthesized generated `DATE` defaults |
| `CURRENT_TIME(), CURRENT_TIME` | 🟡 | Limited zero-fractional statement time synonyms for `CURTIME()` in no-source, `FROM DUAL`, `DO`, row-scalar projection, compatible `TIME` DML assignment, and parenthesized generated `TIME` defaults; respects the limited session `timestamp` override and session `time_zone` offset; no fractional precision, named time-zone tables, or general temporal expression evaluation |
| `CURRENT_TIMESTAMP(), CURRENT_TIMESTAMP` | 🟡 | Limited zero-fractional statement timestamp synonym for `NOW()` in no-source, `FROM DUAL`, `DO`, row-scalar projection, supported temporal DML assignment, and `DATETIME` / `TIMESTAMP` defaults and `ON UPDATE`; respects the limited session `timestamp` override and session `time_zone` offset for materialized current values; no fractional precision, named time-zone tables, or TIMESTAMP row storage/retrieval conversion |
| `CURTIME()` | 🟡 | Limited zero-fractional statement time in the same supported positions as `CURRENT_TIME`, including parenthesized generated `TIME` defaults; respects the limited session `timestamp` override and session `time_zone` offset |
| `DATE()` | 🟡 | Limited no-source, `FROM DUAL`, `DO`, and single-table row-scalar `DATE(value)` over `NULL`, canonical date/datetime strings including admitted zero-date strings, and descriptor `DATE` / `DATETIME` / `TIMESTAMP` / string-family columns, returning date text or `NULL`; invalid temporal inputs return `NULL` with warning 1292; no `TIME` inputs, relaxed temporal parsing, predicates, DML assignments, defaults, generated columns, or general temporal expression evaluation |
| `DATE_ADD()` | 🟡 | Limited no-source, `FROM DUAL`, and `DO` `DATE_ADD(date_or_datetime_string, INTERVAL signed_integer_or_NULL SECOND)` over canonical date/datetime strings in the current storage baseline range, returning datetime text/`NULL`, with MySQL-compatible whitespace handling for the function name under `IGNORE_SPACE`; no other units, interval expressions, table-backed expressions, invalid-date warning semantics, or general temporal arithmetic |
| `DATE_FORMAT()` | 🟡 | Limited no-source, `FROM DUAL`, `DO`, and single-table row-scalar `DATE_FORMAT(value, format)` over `NULL`, canonical date/datetime strings, and descriptor `DATE` / `DATETIME` / `TIMESTAMP` / string-family columns, with a verified token subset and exact top-level numeric equality for `DATE_FORMAT(..., '%H.%i') = numeric_literal`; invalid temporal inputs return `NULL` with warning 1292; no `TIME` inputs, week/year-week tokens, locale/time-zone effects, relaxed temporal parsing, predicates, DML assignments, defaults, generated columns, or general temporal expression evaluation |
| `DATE_SUB()` | 🟡 | Limited no-source, `FROM DUAL`, and `DO` `DATE_SUB(date_or_datetime_string, INTERVAL signed_integer_or_NULL SECOND)` over the same current `DATE_ADD()` input subset, returning datetime text/`NULL`, with subtraction semantics and MySQL-compatible `DATE_SUB` whitespace/identifier handling under `IGNORE_SPACE`; no other units, interval expressions, table-backed expressions, invalid-date warning semantics, or general temporal arithmetic |
| `DATEDIFF()` | ❌ | Subtract two dates |
| `DAY()` | 🟡 | Limited synonym for the current `DAYOFMONTH()` subset |
| `DAYNAME()` | ❌ | Return name of the weekday |
| `DAYOFMONTH()` | 🟡 | Limited no-source, `FROM DUAL`, `DO`, and single-table row-scalar `DAYOFMONTH(value)` over `NULL`, canonical date/datetime strings including admitted zero-date strings, and descriptor `DATE` / `DATETIME` / `TIMESTAMP` / string-family columns, returning an integer part or `NULL`; invalid temporal inputs return `NULL` with warning 1292; no `TIME` inputs, relaxed temporal parsing, predicates, DML assignments, defaults, generated columns, or general temporal expression evaluation |
| `DAYOFWEEK()` | ❌ | Return weekday index of the argument |
| `DAYOFYEAR()` | ❌ | Return day of the year (1-366) |
| `EXTRACT()` | ❌ | Extract part of a date |
| `FROM_DAYS()` | ❌ | Convert a day number to a date |
| `FROM_UNIXTIME()` | ❌ | Format Unix timestamp as a date |
| `GET_FORMAT()` | ❌ | Return a date format string |
| `HOUR()` | 🟡 | Limited no-source, `FROM DUAL`, `DO`, and single-table row-scalar `HOUR(value)` over `NULL`, canonical time/datetime strings including negative and three-digit-hour time strings, and descriptor `TIME` / `DATETIME` / `TIMESTAMP` / string-family columns, returning an integer part or `NULL`; invalid temporal inputs return `NULL` with warning 1292; no date-only inputs, fractional seconds, relaxed temporal parsing, predicates, DML assignments, defaults, generated columns, or general temporal expression evaluation |
| `LAST_DAY` | ❌ | Return last day of the month for the argument |
| `LOCALTIME(), LOCALTIME` | 🟡 | Limited zero-fractional statement timestamp synonym for `NOW()` in the same supported positions as `CURRENT_TIMESTAMP`; respects the limited session `time_zone` offset for materialized current values; no fractional precision, named time-zone tables, or TIMESTAMP row storage/retrieval conversion |
| `LOCALTIMESTAMP, LOCALTIMESTAMP()` | 🟡 | Limited zero-fractional statement timestamp synonym for `NOW()` in the same supported positions as `CURRENT_TIMESTAMP`; respects the limited session `time_zone` offset for materialized current values; no fractional precision, named time-zone tables, or TIMESTAMP row storage/retrieval conversion |
| `MAKEDATE()` | ❌ | Create a date from the year and day of year |
| `MAKETIME()` | ❌ | Create time from hour, minute, second |
| `MICROSECOND()` | ❌ | Return microseconds from argument |
| `MINUTE()` | 🟡 | Limited no-source, `FROM DUAL`, `DO`, and single-table row-scalar `MINUTE(value)` over the same current `HOUR()` input subset, returning an integer part or `NULL` |
| `MONTH()` | 🟡 | Limited no-source, `FROM DUAL`, `DO`, and single-table row-scalar `MONTH(value)` over the same current `YEAR()` / `DAYOFMONTH()` date-input subset, returning an integer part or `NULL` |
| `MONTHNAME()` | ❌ | Return name of the month |
| `NOW()` | 🟡 | Limited zero-fractional statement timestamp in no-source, `FROM DUAL`, `DO`, row-scalar projection, supported temporal DML assignment, and `DATETIME` / `TIMESTAMP` defaults; respects the limited session `timestamp` override and session `time_zone` offset for materialized current values; no fractional precision, named time-zone tables, or TIMESTAMP row storage/retrieval conversion |
| `PERIOD_ADD()` | ❌ | Add a period to a year-month |
| `PERIOD_DIFF()` | ❌ | Return number of months between periods |
| `QUARTER()` | ❌ | Return quarter from a date argument |
| `SEC_TO_TIME()` | ❌ | Converts seconds to 'hh:mm:ss' format |
| `SECOND()` | 🟡 | Limited no-source, `FROM DUAL`, `DO`, and single-table row-scalar `SECOND(value)` over the same current `HOUR()` input subset, returning an integer part or `NULL` |
| `STR_TO_DATE()` | ❌ | Convert a string to a date |
| `SUBDATE()` | 🟡 | Limited no-source, `FROM DUAL`, and `DO` `SUBDATE(date_or_datetime_string, INTERVAL signed_integer_or_NULL SECOND)` alias of the current `DATE_SUB()` interval-second slice; no non-interval forms, other units, interval expressions, table-backed expressions, invalid-date warning semantics, or general temporal arithmetic |
| `SUBTIME()` | ❌ | Subtract times |
| `SYSDATE()` | ❌ | Return time at which the function executes |
| `TIME()` | ❌ | Extract the time portion of the expression passed |
| `TIME_FORMAT()` | ❌ | Format as time |
| `TIME_TO_SEC()` | ❌ | Return argument converted to seconds |
| `TIMEDIFF()` | ❌ | Subtract time |
| `TIMESTAMP()` | ❌ | One-arg cast or two-arg sum |
| `TIMESTAMPADD()` | ❌ | Add an interval to a datetime expression |
| `TIMESTAMPDIFF()` | ❌ | Datetime unit difference |
| `TO_DAYS()` | ❌ | Return date argument converted to days |
| `TO_SECONDS()` | ❌ | Seconds since year 0 |
| `UNIX_TIMESTAMP()` | ❌ | Return a Unix timestamp |
| `UTC_DATE()` | ❌ | Return current UTC date |
| `UTC_TIME()` | ❌ | Return current UTC time |
| `UTC_TIMESTAMP()` | ❌ | Return current UTC date and time |
| `WEEK()` | ❌ | Return week number |
| `WEEKDAY()` | ❌ | Return weekday index |
| `WEEKOFYEAR()` | ❌ | Return calendar week of the date (1-53) |
| `YEAR()` | 🟡 | Limited no-source, `FROM DUAL`, `DO`, and single-table row-scalar `YEAR(value)` over `NULL`, canonical date/datetime strings including admitted zero-date strings, and descriptor `DATE` / `DATETIME` / `TIMESTAMP` / string-family columns, returning an integer part or `NULL`; invalid temporal inputs return `NULL` with warning 1292; no `TIME` inputs, relaxed temporal parsing, predicates, DML assignments, defaults, generated columns, or general temporal expression evaluation |
| `YEARWEEK()` | ❌ | Return year and week |

[Back to compatibility overview](../../COMPATIBILITY.md)
