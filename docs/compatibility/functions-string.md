# String functions

String, byte, collation, pattern, base encoding, and regular-expression helpers.

| Function or operator | Status | Notes |
| --- | --- | --- |
| `ASCII()` | ❌ | Return numeric value of left-most character |
| `BIT_LENGTH()` | ❌ | Return length of argument in bits |
| `CHAR()` | ❌ | Return character for each integer passed |
| `CHAR_LENGTH()` | ❌ | Return number of characters in argument |
| `CHARACTER_LENGTH()` | ❌ | Synonym for CHAR_LENGTH() |
| `CHARSET()` | ❌ | Return character set of the argument |
| `COERCIBILITY()` | ❌ | Return collation coercibility value of the string argument |
| `COLLATION()` | ❌ | Return collation of the string argument |
| `CONCAT()` | 🟡 | Limited scalar projection support with one or more non-`CONCAT()` arguments over string, integer, boolean, `NULL`, existing session scalar values such as `DATABASE()` / `SCHEMA()` / supported `@@...` system variables, limited no-source/`DUAL` scalar subqueries, and descriptor-backed integer, exact `DECIMAL`, string, baseline `TEXT` family, and temporal columns in no-source, `DUAL`, and single-table row-scalar `SELECT` forms; any `NULL` argument returns `NULL`. No nested `CONCAT()`, `CONCAT_WS()`, binary-string result typing, non-ASCII collation semantics, approximate numeric formatting, table-backed use outside the limited row-scalar SELECT path, DML assignments, predicates, ordering expressions, table-backed/DML subqueries, or expression metadata |
| `CONCAT_WS()` | ❌ | Return concatenate with separator |
| `ELT()` | ❌ | Return string at index number |
| `EXPORT_SET()` | ❌ | Bitmask-to-string mapping |
| `FIELD()` | 🟡 | Limited no-source, `DUAL`, `DO`, and single-table row-scalar `SELECT` projection over flat all-string or all-integer argument lists with `NULL`, ASCII string literals, signed-64 integer/boolean literals, and supported descriptor columns; returns the 1-based first-match position or `0`; no mixed domains, binary strings, non-ASCII collation parity, predicates, DML assignments, ordering/grouping expressions, or general expression metadata |
| `FIND_IN_SET()` | ❌ | Index (position) of first argument within second argument |
| `FROM_BASE64()` | ❌ | Decode base64 encoded string and return result |
| `HEX()` | ❌ | Hexadecimal representation of decimal or string value |
| `INSERT()` | ❌ | Insert substring by position |
| `INSTR()` | ❌ | Return index of the first occurrence of substring |
| `LCASE()` | ❌ | Synonym for LOWER() |
| `LEFT()` | ❌ | Return leftmost number of characters as specified |
| `LENGTH()` | ❌ | Return length of a string in bytes |
| `LOCATE()` | ❌ | Return position of the first occurrence of substring |
| `LOWER()` | ❌ | Return argument in lowercase |
| `LPAD()` | ❌ | Left-padding behavior |
| `LTRIM()` | ❌ | Remove leading spaces |
| `MAKE_SET()` | ❌ | Bitmask-selected string set |
| `MID()` | ❌ | Return a substring starting from the specified position |
| `OCTET_LENGTH()` | ❌ | Synonym for LENGTH() |
| `ORD()` | ❌ | Return character code for leftmost character of the argument |
| `POSITION()` | ❌ | Synonym for LOCATE() |
| `QUOTE()` | ❌ | Escape the argument for use in an SQL statement |
| `REGEXP_INSTR()` | ❌ | Starting index of substring matching regular expression |
| `REGEXP_LIKE()` | ❌ | Whether string matches regular expression |
| `REGEXP_REPLACE()` | ❌ | Replace substrings matching regular expression |
| `REGEXP_SUBSTR()` | ❌ | Return substring matching regular expression |
| `REPEAT()` | ❌ | Repeat a string the specified number of times |
| `REPLACE()` | ❌ | Replace occurrences of a specified string |
| `REVERSE()` | ❌ | Reverse the characters in a string |
| `RIGHT()` | ❌ | Return specified rightmost number of characters |
| `RPAD()` | ❌ | Append string the specified number of times |
| `RTRIM()` | ❌ | Remove trailing spaces |
| `SOUNDEX()` | ❌ | Return a soundex string |
| `SPACE()` | ❌ | Return a string of the specified number of spaces |
| `STRCMP()` | ❌ | Compare two strings |
| `SUBSTR()` | ❌ | Return substring as specified |
| `SUBSTRING()` | ❌ | Return substring as specified |
| `SUBSTRING_INDEX()` | ❌ | Delimiter-count substring |
| `TO_BASE64()` | ❌ | Return argument converted to a base-64 string |
| `TRIM()` | ❌ | Remove leading and trailing spaces |
| `UCASE()` | ❌ | Synonym for UPPER() |
| `UNHEX()` | ❌ | Return a string containing hex representation of a number |
| `UPPER()` | ❌ | Convert to uppercase |
| `WEIGHT_STRING()` | ❌ | Return weight string for a string |

[Back to compatibility overview](../../COMPATIBILITY.md)
