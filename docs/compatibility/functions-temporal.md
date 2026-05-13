# Temporal functions

Date, time, timestamp, interval, time-zone, and calendar functions.

| Function or operator | Status | Notes |
| --- | --- | --- |
| `ADDDATE()` | ❌ | Add time values (intervals) to a date value |
| `ADDTIME()` | ❌ | Add time |
| `CONVERT_TZ()` | ❌ | Convert from one time zone to another |
| `CURDATE()` | ❌ | Return current date |
| `CURRENT_DATE(), CURRENT_DATE` | ❌ | Synonyms for CURDATE() |
| `CURRENT_TIME(), CURRENT_TIME` | ❌ | Synonyms for CURTIME() |
| `CURRENT_TIMESTAMP(), CURRENT_TIMESTAMP` | 🟡 | Limited zero-fractional statement timestamp synonym for `NOW()` in no-source, `FROM DUAL`, `DO`, row-scalar projection, supported temporal DML assignment, and `DATETIME` / `TIMESTAMP` defaults and `ON UPDATE`; fixed UTC formatting, no fractional precision, mutable time zones, or general temporal expression evaluation |
| `CURTIME()` | ❌ | Return current time |
| `DATE()` | ❌ | Extract the date part of a date or datetime expression |
| `DATE_ADD()` | 🟡 | Limited no-source, `FROM DUAL`, and `DO` `DATE_ADD(date_or_datetime_string, INTERVAL signed_integer_or_NULL SECOND)` over canonical date/datetime strings in the current storage baseline range, returning datetime text/`NULL`, with MySQL-compatible whitespace handling for the function name under `IGNORE_SPACE`; no other units, interval expressions, table-backed expressions, invalid-date warning semantics, or general temporal arithmetic |
| `DATE_FORMAT()` | 🟡 | Limited no-source, `FROM DUAL`, `DO`, and single-table row-scalar `DATE_FORMAT(value, format)` over `NULL`, canonical date/datetime strings, and descriptor `DATE` / `DATETIME` / `TIMESTAMP` / string-family columns, with a verified token subset and exact top-level numeric equality for `DATE_FORMAT(..., '%H.%i') = numeric_literal`; invalid temporal inputs return `NULL` with warning 1292; no `TIME` inputs, week/year-week tokens, locale/time-zone effects, relaxed temporal parsing, predicates, DML assignments, defaults, generated columns, or general temporal expression evaluation |
| `DATE_SUB()` | ❌ | Subtract a time value (interval) from a date |
| `DATEDIFF()` | ❌ | Subtract two dates |
| `DAY()` | ❌ | Synonym for DAYOFMONTH() |
| `DAYNAME()` | ❌ | Return name of the weekday |
| `DAYOFMONTH()` | ❌ | Return day of the month (0-31) |
| `DAYOFWEEK()` | ❌ | Return weekday index of the argument |
| `DAYOFYEAR()` | ❌ | Return day of the year (1-366) |
| `EXTRACT()` | ❌ | Extract part of a date |
| `FROM_DAYS()` | ❌ | Convert a day number to a date |
| `FROM_UNIXTIME()` | ❌ | Format Unix timestamp as a date |
| `GET_FORMAT()` | ❌ | Return a date format string |
| `HOUR()` | ❌ | Extract the hour |
| `LAST_DAY` | ❌ | Return last day of the month for the argument |
| `LOCALTIME(), LOCALTIME` | 🟡 | Limited zero-fractional statement timestamp synonym for `NOW()` in the same supported positions as `CURRENT_TIMESTAMP`; no fractional precision, mutable time zones, or general temporal expression evaluation |
| `LOCALTIMESTAMP, LOCALTIMESTAMP()` | 🟡 | Limited zero-fractional statement timestamp synonym for `NOW()` in the same supported positions as `CURRENT_TIMESTAMP`; no fractional precision, mutable time zones, or general temporal expression evaluation |
| `MAKEDATE()` | ❌ | Create a date from the year and day of year |
| `MAKETIME()` | ❌ | Create time from hour, minute, second |
| `MICROSECOND()` | ❌ | Return microseconds from argument |
| `MINUTE()` | ❌ | Return minute from the argument |
| `MONTH()` | ❌ | Return month from the date passed |
| `MONTHNAME()` | ❌ | Return name of the month |
| `NOW()` | 🟡 | Limited zero-fractional statement timestamp in no-source, `FROM DUAL`, `DO`, row-scalar projection, supported temporal DML assignment, and `DATETIME` / `TIMESTAMP` defaults; respects the limited session `timestamp` override, uses fixed UTC formatting, and has no fractional precision, mutable time zones, or general temporal expression evaluation |
| `PERIOD_ADD()` | ❌ | Add a period to a year-month |
| `PERIOD_DIFF()` | ❌ | Return number of months between periods |
| `QUARTER()` | ❌ | Return quarter from a date argument |
| `SEC_TO_TIME()` | ❌ | Converts seconds to 'hh:mm:ss' format |
| `SECOND()` | ❌ | Return second (0-59) |
| `STR_TO_DATE()` | ❌ | Convert a string to a date |
| `SUBDATE()` | ❌ | Synonym for DATE_SUB() when invoked with three arguments |
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
| `YEAR()` | ❌ | Return year |
| `YEARWEEK()` | ❌ | Return year and week |

[Back to compatibility overview](../../COMPATIBILITY.md)
