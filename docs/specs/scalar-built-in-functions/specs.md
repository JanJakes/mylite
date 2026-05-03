# Scalar built-in functions

## Scope

Task 24 adds the first MySQL-compatible scalar built-in function surface for
common application SQL. It extends the Task 16 expression foundation so function
calls can be parsed, bound, evaluated, described in result metadata, and used
consistently anywhere MyLite already evaluates scalar expressions.

In scope for the initial implementation:

- generic scalar function-call parsing for supported built-ins, including
  case-insensitive names and argument-count validation
- scalar built-ins in no-table `SELECT`, one-table projection expressions,
  `WHERE`, `ORDER BY`, `INSERT ... VALUES`, `INSERT ... SET`, and single-table
  `UPDATE` / `DELETE` expressions where the surrounding statement is already
  executable
- string functions:
  - `ASCII`
  - `CHAR`
  - `CHAR_LENGTH`, `CHARACTER_LENGTH`
  - `LENGTH`, `OCTET_LENGTH`
  - `CONCAT`, `CONCAT_WS`
  - `LEFT`, `RIGHT`
  - `SUBSTRING`, `SUBSTR`, `MID`
  - `LOCATE`, `POSITION`, `INSTR`
  - `INSERT`
  - `LOWER`, `LCASE`, `UPPER`, `UCASE`
  - `TRIM`, `LTRIM`, `RTRIM`
  - `LPAD`, `RPAD`
  - `REPEAT`, `REPLACE`, `REVERSE`
  - `ELT`, `FIELD`, `FIND_IN_SET`
  - `MAKE_SET`
  - `HEX`, `UNHEX`
  - `TO_BASE64`, `FROM_BASE64`
  - `BIN`, `OCT`
  - `CRC32`
- numeric functions:
  - `ABS`
  - `BIT_COUNT`
  - `BIT_LENGTH`
  - `INET_ATON`
  - `INET_NTOA`
  - `SIGN`
  - `ROUND`
  - `TRUNCATE`
  - `FLOOR`, `CEIL`, `CEILING`
  - `POW`, `POWER`
  - `SQRT`
  - `MOD`
  - `CONV`
  - `PI`
- temporal functions:
  - `NOW`, `LOCALTIME`, `LOCALTIMESTAMP`, `CURRENT_TIMESTAMP`
  - `CURDATE`, `CURRENT_DATE`
  - `CURTIME`, `CURRENT_TIME`
  - `UTC_DATE`, `UTC_TIME`, `UTC_TIMESTAMP`
  - `DATE`
  - `YEAR`, `MONTH`, `DAY`, `DAYOFMONTH`
  - `HOUR`, `MINUTE`, `SECOND`, `MICROSECOND`
  - `DATEDIFF`
  - `DATE_ADD`, `ADDDATE`
  - `DATE_SUB`, `SUBDATE`
  - `TIMESTAMPADD`, `TIMESTAMPDIFF`
  - `EXTRACT`
- conditional and comparison functions:
  - `IF`
  - `IFNULL`
  - `NULLIF`
  - `COALESCE`
  - searched and simple `CASE` expressions
  - `GREATEST`, `LEAST`
  - `ISNULL`
- information functions:
  - `DATABASE`, `SCHEMA`
  - `VERSION`
  - `USER`, `SESSION_USER`, `SYSTEM_USER`
  - `CURRENT_USER`
  - `CONNECTION_ID`
  - `LAST_INSERT_ID`
  - `ROW_COUNT`
  - `CHARSET`, `COLLATION`

Out of scope for the initial implementation:

- aggregate, grouping, and window functions; Task 25 owns aggregate behavior
- JSON, full-text, spatial, XML, encryption, compression, locking,
  replication, performance-schema, loadable, and internal-only functions
- regular expression functions and `SOUNDS LIKE`
- `CAST`, `CONVERT`, `BINARY`, `COLLATE`, character-set introducer expansion,
  and full collation coercibility beyond the current charset foundation
- `DATE_FORMAT`, `STR_TO_DATE`, week-numbering functions, named time zones,
  locale-sensitive day/month names, and time-zone table integration
- `RAND`, `UUID`, `UUID_TO_BIN`, `BIN_TO_UUID`, `SLEEP`, `BENCHMARK`, and other
  nondeterministic or diagnostic/performance functions not needed for the first
  application batch
- `DEFAULT(col)`, user variables, system variables, prepared-statement
  parameters, subqueries, row constructors, stored-program local variables, and
  stored or loadable function resolution
- SQL-mode variants such as `IGNORE_SPACE`, `PIPES_AS_CONCAT`, and
  `NO_BACKSLASH_ESCAPES`, except where existing Task 16 behavior already has a
  documented decision
- complete non-ASCII case folding, accent-insensitive comparisons, and
  collation-specific string transformation behavior

This task should not mark any deferred function family as supported. Unsupported
function calls should fail deterministically with a MySQL-compatible diagnostic
when possible.

## Current implementation checkpoint

The first Task 24 runtime slice implements the pure deterministic subset needed
by common scalar expressions:

- string functions: `CONCAT`, `LENGTH`, `OCTET_LENGTH`, `CHAR_LENGTH`,
  `CHARACTER_LENGTH`, `LOWER`, `LCASE`, `UPPER`, `UCASE`, `LEFT`, `RIGHT`,
  `REPLACE`, `CONCAT_WS`, `SUBSTRING`, `SUBSTR`, `MID`, `TRIM`, `LTRIM`,
  `RTRIM`, `ASCII`, `ORD`, `LOCATE`, `POSITION`, `INSTR`, `INSERT`, `QUOTE`,
  `REPEAT`, `SPACE`, `REVERSE`, `LPAD`, `RPAD`, `ELT`, `FIELD`,
  `FIND_IN_SET`, `MAKE_SET`, `HEX`, `UNHEX`, `TO_BASE64`, `FROM_BASE64`,
  `BIN`, and `OCT`; see
  `docs/specs/string-functions-substring-trim/specs.md` and
  `docs/specs/string-search-code-functions/specs.md` and
  `docs/specs/string-insert-function/specs.md` and
  `docs/specs/string-quote-function/specs.md` and
  `docs/specs/string-padding-repeat-functions/specs.md` and
  `docs/specs/string-list-index-functions/specs.md` and
  `docs/specs/string-make-set-function/specs.md` and
  `docs/specs/string-hex-unhex-functions/specs.md` and
  `docs/specs/base64-string-functions/specs.md` and
  `docs/specs/numeric-base-conversion-functions/specs.md`
- numeric functions: `ABS`, `SIGN`, `FLOOR`, `CEIL`, `CEILING`, `MOD`,
  `CONV`, `BIT_COUNT`, `BIT_LENGTH`, `CRC32`, `INET_ATON`, `INET_NTOA`, and `PI`; see
  `docs/specs/numeric-base-conversion-functions/specs.md` and
  `docs/specs/bit-utility-functions/specs.md` and
  `docs/specs/crc32-function/specs.md` and
  `docs/specs/inet-ipv4-functions/specs.md`
- conditional/comparison functions: `IF`, `IFNULL`, `NULLIF`, `COALESCE`, and
  `ISNULL`
- session information functions: `DATABASE`, `SCHEMA`, `VERSION`,
  `LAST_INSERT_ID`, and `ROW_COUNT`; see
  `docs/specs/session-information-functions/specs.md`
- session identity functions: `CONNECTION_ID`, `USER`, `SESSION_USER`,
  `SYSTEM_USER`, and `CURRENT_USER` / bare `CURRENT_USER`; see
  `docs/specs/session-identity-functions/specs.md`

These functions are implemented in no-table scalar `SELECT`, one-table
`SELECT` projection, `WHERE`, and `ORDER BY` expressions, and the existing
single-table `UPDATE` and `DELETE` expression paths. The checkpoint includes
runtime tests for scalar rows, NULL propagation, UTF-8 length and substring
handling, zero and negative `LEFT`/`RIGHT` counts, `SUBSTRING` `FROM` / `FOR`
syntax, trim direction syntax, byte-based `ASCII`, packed-byte `ORD`,
`LOCATE` / `POSITION` / `INSTR` search positions, start-position edges,
`INSERT` string splicing, `QUOTE` SQL-literal escaping, `QUOTE` numeric-source
metadata, padding, repetition, spaces, UTF-8 reversal, empty pad strings,
`CHAR()` byte emission and optional `USING` charset syntax,
hex encoding, binary-string hex decoding, embedded-NUL byte lengths for
`UNHEX()` results, invalid `UNHEX()` warnings,
Base64 encoding, Base64 decoding, long-output newline wrapping, ignored decode
whitespace, invalid `FROM_BASE64()` `NULL` results without warnings,
binary/octal/arbitrary-base conversion, invalid base ranges, base-conversion
string parsing warnings,
CRC-32 checksum values, `CRC32()` string/numeric/NULL/binary-byte conversion,
exact decimal leading-zero normalization, unsigned checksum metadata,
IPv4 network-address conversion, short IPv4 forms, invalid IPv4 warnings,
`INET_NTOA()` bounds and string-integer truncation warnings,
`MOD(..., 0)` warnings, table projection, filters, ordering, update assignment
expressions, delete predicates, unsupported functions, unsupported arity, and
selected result metadata.

This checkpoint intentionally does not yet implement `INSERT ... VALUES` or
`INSERT ... SET` function expressions, temporal functions, information
functions outside the session-state and session-identity slices,
aggregate/window functions, JSON,
regular expressions, spatial, full-text, encryption, loadable functions,
complete binary-string semantics for all scalar functions, exact
exact-versus-approximate numeric preservation for every expression path, or
exact MySQL error-code reporting for unsupported functions and argument-count
mismatches.

## Sources

- MySQL 8.4 Reference Manual, Built-In Function and Operator Reference:
  https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html
- MySQL 8.4 Reference Manual, String Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/string-functions.html
- MySQL 8.4 Reference Manual, Mathematical Functions:
  https://dev.mysql.com/doc/refman/8.4/en/mathematical-functions.html
- MySQL 8.4 Reference Manual, Date and Time Functions:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html
- MySQL 8.4 Reference Manual, Flow Control Functions:
  https://dev.mysql.com/doc/refman/8.4/en/flow-control-functions.html
- MySQL 8.4 Reference Manual, Comparison Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- MySQL 8.4 Reference Manual, Information Functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html
- MySQL 8.4 Reference Manual, Type Conversion in Expression Evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- MySQL 8.4 Reference Manual, Precision Math Rounding Behavior:
  https://dev.mysql.com/doc/refman/8.4/en/precision-math-rounding.html
- Existing MyLite specs:
  - `docs/specs/expression-operator-foundation/specs.md`
  - `docs/specs/where-clause/specs.md`
  - `docs/specs/order-limit-offset/specs.md`
  - `docs/specs/result-metadata-expression-labels/specs.md`
  - `docs/specs/insert-values/specs.md`
  - `docs/specs/insert-set/specs.md`
  - `docs/specs/update-single-table/specs.md`
  - `docs/specs/delete-single-table/specs.md`
  - `docs/specs/character-set-collation-foundation/specs.md`
  - `docs/specs/temporal-column-types/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

- `docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --show-warnings`
- `docker exec -i mylite-mysql-849 mysql -uroot --column-type-info -vvv`
- `docker exec -i mylite-mysql-849 mysql -uroot --force --batch --raw --show-warnings`

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 behavior summary

Runtime probes used MySQL 8.4.9 with the default SQL mode:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,
ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

Unicode string probes should begin with `SET NAMES utf8mb4`, because the
verified container's default connection character set was `latin1`.

### Test fixture

Runtime probes used this fixture:

```sql
SET time_zone = '+00:00';
SET timestamp = 1700000000;
DROP DATABASE IF EXISTS mylite_task24_functions;
CREATE DATABASE mylite_task24_functions;
USE mylite_task24_functions;

CREATE TABLE f (
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  s VARCHAR(20),
  b VARBINARY(8),
  n INT,
  d DECIMAL(6,2),
  dt DATETIME(6),
  da DATE,
  ti TIME(6)
);

INSERT INTO f (s,b,n,d,dt,da,ti) VALUES
  ('alpha', X'4100', 1, 12.50,
   '2024-02-29 12:34:56.123456', '2024-02-29', '12:34:56.123456'),
  ('Beta', X'ff', -2, -2.50,
   '2001-11-01 00:00:00', '2001-11-01', '-03:04:05.000006'),
  (NULL, NULL, NULL, NULL, NULL, NULL, NULL);
```

### String functions

Representative runtime results with `SET NAMES utf8mb4`:

| SQL expression | Result |
| --- | --- |
| `ASCII('')` | `0` |
| `ASCII(NULL)` | `NULL` |
| `HEX(CHAR(65,66))` | `4142` |
| `HEX(CHAR(65,NULL,66))` | `4142` |
| `CHAR(77,121,83,81,'76' USING utf8mb4)` | `MySQL` |
| `CHAR_LENGTH('海豚')` | `2` |
| `LENGTH('海豚')` | `6` |
| `CONCAT('My', NULL, 'QL')` | `NULL` |
| `CONCAT_WS(',', 'a', NULL, '', 'b')` | `a,,b` |
| `LEFT('abcdef', 2)` | `ab` |
| `RIGHT('abcdef', 3)` | `def` |
| `SUBSTRING('abcdef', 2, 3)` | `bcd` |
| `SUBSTRING('abcdef', -2)` | `ef` |
| `LOCATE('pha', 'alpha')` | `3` |
| `POSITION('ph' IN 'alpha')` | `3` |
| `INSTR('alpha', 'z')` | `0` |
| `INSERT('Quadratic', 3, 4, 'What')` | `QuWhattic` |
| `REPLACE('a.b.b', 'b', 'x')` | `a.x.x` |
| `REPEAT('xy', 3)` | `xyxyxy` |
| `REPEAT('xy', -1)` | empty string |
| `SPACE(3)` | three spaces |
| `REVERSE('海豚猫')` | `猫豚海` |
| `LPAD('hi', 5, '.')` | `...hi` |
| `RPAD('hi', 5, '.')` | `hi...` |
| `TRIM(BOTH 'x' FROM 'xxhix')` | `hi` |
| `TRIM(LEADING 'x' FROM 'xxhix')` | `hix` |
| `LOWER('AbC')` | `abc` |
| `UPPER('AbC')` | `ABC` |
| `HEX('Az')` | `417A` |
| `UNHEX('417a')` | `Az` |

Initial MyLite string support should implement byte-accurate ASCII and UTF-8
length handling under the current charset foundation. `LOWER` and `UPPER` may
initially be limited to ASCII case mapping unless and until broader collation
and Unicode case rules are implemented and tested.

### Numeric functions

Representative runtime results:

| SQL expression | Result | Warnings/errors |
| --- | --- | --- |
| `ABS(-12.5)` | `12.5` | none |
| `SIGN(-12.5)` | `-1` | none |
| `ROUND(2.5)` | `3` | none |
| `ROUND(25E-1)` | `2` on the verified runtime | approximate rounding is C-library-sensitive |
| `ROUND(123.456, 2)` | `123.46` | none |
| `FLOOR(-1.2)` | `-2` | none |
| `CEILING(-1.2)` | `-1` | none |
| `POW(2, 10)` | `1024` | none |
| `SQRT(9)` | `3` | none |
| `SQRT(-1)` | `NULL` | none |
| `MOD(7, 3)` | `1` | none |
| `SQRT('foo')` | `0` | warning 1292, truncated incorrect double value |
| `ABS(-9223372036854775808)` | error | 1690 / `22003`, out of range |

`ROUND` must preserve MySQL's distinction between exact and approximate input.
Exact values round half away from zero. Approximate values can follow the host C
library result, but tests should assert the observed MySQL 8.4.9 result on the
same supported platform before claiming parity.

### Conditional and comparison functions

Representative runtime results:

| SQL expression | Result |
| --- | --- |
| `IF(0, 'yes', 'no')` | `no` |
| `IF(NULL, 'yes', 'no')` | `no` |
| `IFNULL(NULL, 'fallback')` | `fallback` |
| `NULLIF('a', 'a')` | `NULL` |
| `NULLIF('a', 'b')` | `a` |
| `COALESCE(NULL, NULL, 'x')` | `x` |
| `GREATEST('11','45','2')` | `45` |
| `GREATEST(11,45,2)` | `45` |
| `LEAST('11','45','2')` | `11` |
| `GREATEST(1,NULL,2)` | `NULL` |
| `LEAST(1,NULL,2)` | `NULL` |
| `ISNULL(NULL)` | `1` |
| `CASE WHEN n > 0 THEN 'pos' WHEN n < 0 THEN 'neg' ELSE 'nil' END` | `pos`, `neg`, `nil` over fixture rows |

`IF` and `COALESCE` short-circuit unchosen branches. The verified query:

```sql
SELECT IF(0, 1/0, 1), IF(1, 1, 1/0), COALESCE(1, 1/0);
```

returned `1`, `1`, and `1.0000` without division warnings. `NULLIF` follows
MySQL's documented expression behavior and can evaluate its first argument more
than once; `SELECT NULLIF(1/0, 2)` produced `NULL` and two warning 1365
records on the verified runtime.

### Temporal functions

With `SET time_zone = '+00:00'` and `SET timestamp = 1700000000`,
representative runtime results were:

| SQL expression | Result |
| --- | --- |
| `NOW(6)` | `2023-11-14 22:13:20.000000` |
| `NOW() = CURRENT_TIMESTAMP` | `1` |
| `CURDATE()` | `2023-11-14` |
| `CURTIME(6)` | `22:13:20.000000` |
| `UTC_TIMESTAMP()` | `2023-11-14 22:13:20` |
| `CURRENT_DATE` | `2023-11-14` |
| `CURRENT_TIME` | `22:13:20` |
| `CURRENT_TIMESTAMP` | `2023-11-14 22:13:20` |
| `DATE('2024-02-29 12:34:56')` | `2024-02-29` |
| `YEAR('2024-02-29')` | `2024` |
| `MONTH('2024-02-29')` | `2` |
| `DAYOFMONTH('2001-11-00')` | `0` |
| `HOUR('-03:04:05.000006')` | `3` |
| `MICROSECOND('12:34:56.123456')` | `123456` |
| `DATEDIFF('2024-03-01','2024-02-28')` | `2` |
| `TIMESTAMPDIFF(DAY,'2024-02-28','2024-03-01')` | `2` |
| `TIMESTAMPDIFF(HOUR,'2024-02-28 00:00:00','2024-02-29 12:00:00')` | `36` |
| `DATE_ADD('2024-02-29', INTERVAL 1 DAY)` | `2024-03-01` |
| `DATE_SUB('2024-03-01', INTERVAL 1 DAY)` | `2024-02-29` |
| `EXTRACT(YEAR_MONTH FROM '2024-02-29 12:34:56')` | `202402` |
| `TIMESTAMPADD(DAY, 2, '2024-02-28')` | `2024-03-01` |

`NOW`, `CURDATE`, `CURTIME`, UTC variants, and their synonyms are evaluated
once per statement. MyLite should add a statement timestamp to expression
evaluation context rather than calling the host clock at every function
evaluation.

Incomplete and zero-date behavior is function-specific. The verified query:

```sql
SELECT DATE_ADD('2006-05-00', INTERVAL 1 DAY), DAYNAME('2006-05-00');
```

returned `NULL`, `NULL` and produced two warning 1292 records for incorrect
datetime value. Initial MyLite support should implement zero-day extraction for
`DAY`, `DAYOFMONTH`, `MONTH`, and `YEAR`, but should return `NULL` plus
warnings for strict complete-date functions in scope.

### Information functions

Representative runtime results:

| SQL expression | Result expectation |
| --- | --- |
| `DATABASE()` | selected schema, or `NULL` when no default schema is selected |
| `SCHEMA()` | synonym for `DATABASE()` |
| `VERSION()` | MyLite runtime version from `mylite_version()` |
| `USER()` | MyLite embedded client identity; see the session identity spec |
| `SESSION_USER()` | synonym for `USER()` |
| `SYSTEM_USER()` | synonym for `USER()` |
| `CURRENT_USER()`, `CURRENT_USER` | MyLite embedded authenticated identity; currently same value as `USER()` |
| `LAST_INSERT_ID()` | first automatically generated id from the most recent successful insert |
| `CONNECTION_ID() IS NOT NULL` | `1` |
| `ROW_COUNT()` after the fixture insert | `3` |
| `ROW_COUNT()` immediately after a `SELECT` | `-1` in MySQL client-observable semantics |
| `CHARSET('abc')` after `SET NAMES utf8mb4` | `utf8mb4` |
| `COLLATION('abc')` after `SET NAMES utf8mb4` | `utf8mb4_0900_ai_ci` |

MyLite should derive these values from handle/session state, not process-global
state. `CONNECTION_ID()` can be a stable per-handle unsigned integer; exact
server thread identity is not meaningful in an embedded runtime.

## Errors and warnings

Function diagnostics must use MySQL-compatible error codes, SQLSTATE values,
and warning records where they have been verified.

Verified cases:

| SQL | MySQL behavior |
| --- | --- |
| `SELECT CONCAT()` | error 1582 / `42000`, incorrect parameter count in native function `CONCAT` |
| `SELECT IF(1,2)` | syntax error 1064 / `42000` |
| `SELECT DATE_ADD('2024-01-01', INTERVAL 1 BOGUS)` | syntax error 1064 / `42000` |
| `SELECT ABS(-9223372036854775808)` | error 1690 / `22003` |
| `SELECT SQRT('foo')` | result `0`, warning 1292 |
| `SELECT DATE_ADD('2006-05-00', INTERVAL 1 DAY)` | result `NULL`, warning 1292 |

MyLite should validate argument counts during binding for ordinary function
calls. Special syntactic forms such as `IF`, `POSITION`, `TRIM`, `EXTRACT`,
`DATE_ADD`, and `TIMESTAMPDIFF` can reject malformed syntax in the parser when
the form cannot be represented as a valid expression.

Warnings belong to expression evaluation. Metadata-only paths such as
`LIMIT 0` must not evaluate expressions solely to infer metadata and must not
emit conversion warnings.

## Result metadata

Task 24 extends Task 23's expression metadata inference. Function descriptors
must provide conservative MySQL-compatible metadata without evaluating row
values.

Verified `mysql --column-type-info -vvv` examples:

| Expression alias | Type | Length | Decimals | Collation | Flags |
| --- | --- | ---: | ---: | --- | --- |
| `CONCAT('a','b') AS concat_value` | `VAR_STRING` | `8` | `31` | `utf8mb4_0900_ai_ci` | none |
| `HEX('Az') AS hex_text` | `VAR_STRING` | `64` | `31` | `utf8mb4_0900_ai_ci` | none |
| `UNHEX('417a') AS unhex_text` | `VAR_STRING` | `8` | `31` | `binary` | `BINARY` |
| `LENGTH('海豚') AS byte_len` | `LONGLONG` | `10` | `0` | `binary` | `NOT_NULL BINARY NUM` |
| `CHAR_LENGTH('海豚') AS char_len` | `LONGLONG` | `10` | `0` | `binary` | `NOT_NULL BINARY NUM` |
| `BIT_LENGTH('abc') AS bit_len` | `LONGLONG` | `10` | `0` | `binary` | `NOT_NULL BINARY NUM` |
| `BIT_COUNT(7) AS bit_count` | `LONGLONG` | `21` | `0` | `binary` | `NOT_NULL BINARY NUM` |
| `SUBSTRING('abcdef',2,3) AS substr_value` | `VAR_STRING` | `12` | `31` | `utf8mb4_0900_ai_ci` | none |
| `ABS(-12.5) AS abs_decimal` | `NEWDECIMAL` | `5` | `1` | `binary` | `NOT_NULL BINARY NUM` |
| `ROUND(123.456,2) AS round_scale` | `NEWDECIMAL` | `8` | `2` | `binary` | `NOT_NULL BINARY NUM` |
| `POW(2,10) AS pow_value` | `DOUBLE` | `23` | `31` | `binary` | `BINARY NUM` |
| `SQRT(-1) AS sqrt_domain` | `DOUBLE` | `23` | `31` | `binary` | `BINARY NUM` |
| `IF(1,'yes','no') AS if_value` | `VAR_STRING` | `12` | `31` | `utf8mb4_0900_ai_ci` | `NOT_NULL` |
| `IFNULL(NULL,'fallback') AS ifnull_value` | `VAR_STRING` | `32` | `31` | `utf8mb4_0900_ai_ci` | `NOT_NULL` |
| `COALESCE(NULL,1.25) AS coalesce_value` | `NEWDECIMAL` | `5` | `2` | `binary` | `NOT_NULL BINARY NUM` |
| `GREATEST(11,45,2) AS greatest_value` | `LONGLONG` | `3` | `0` | `binary` | `NOT_NULL BINARY NUM` |
| `NOW(6) AS now6` | `DATETIME` | `26` | `6` | `binary` | `NOT_NULL BINARY` |
| `CURDATE() AS curdate_value` | `DATE` | `10` | `0` | `binary` | `NOT_NULL BINARY` |
| `DATEDIFF(...) AS datediff_value` | `LONGLONG` | `9` | `0` | `binary` | `BINARY NUM` |
| `DATE_ADD('2024-02-29', INTERVAL 1 DAY) AS date_add_value` | `STRING` | `116` | `31` | `utf8mb4_0900_ai_ci` | none |
| `DATABASE() AS database_value` | `VAR_STRING` | `256` | `31` | `utf8mb4_0900_ai_ci` | nullable |
| `VERSION() AS version_value` | `VAR_STRING` | `20` | `31` | `utf8mb4_0900_ai_ci` | `NOT_NULL` |
| `LAST_INSERT_ID() AS last_insert_id_value` | `LONGLONG` | `21` | `0` | `binary` | `NOT_NULL UNSIGNED BINARY NUM` |
| `CONNECTION_ID() AS connection_id_value` | `LONGLONG` | `21` | `0` | `binary` | `NOT_NULL UNSIGNED BINARY NUM` |
| `USER() AS user_value` | `VAR_STRING` | `1152` | `31` | `utf8mb4_0900_ai_ci` | nullable |
| `CURRENT_USER AS current_user_value` | `VAR_STRING` | `1152` | `31` | `utf8mb4_0900_ai_ci` | nullable |

Implementation should assert metadata through symbolic MyLite/MySQL field
types, flags, decimals, charset id, and nullability rather than relying on raw
client-library numeric constants unless the public ABI intentionally mirrors
those constants.

## MyLite grammar and AST notes

Task 24 should add function calls to the shared expression grammar, not to
individual statement grammars. The following Lemon-style snippets describe the
intended MyLite grammar shape and are independently authored:

```lemon
primary_expression ::= literal.
primary_expression ::= qualified_identifier.
primary_expression ::= current_temporal_function.
primary_expression ::= scalar_function_call.
primary_expression ::= case_expression.
primary_expression ::= LPAREN expression RPAREN.

scalar_function_call ::= function_name LPAREN opt_function_argument_list RPAREN.
function_name ::= identifier.
function_name ::= nonreserved_function_keyword.

opt_function_argument_list ::= .
opt_function_argument_list ::= function_argument_list.

function_argument_list ::= expression.
function_argument_list ::= function_argument_list COMMA expression.

current_temporal_function ::= CURRENT_DATE opt_empty_parens.
current_temporal_function ::= CURRENT_TIME opt_temporal_fsp_parens.
current_temporal_function ::= CURRENT_TIMESTAMP opt_temporal_fsp_parens.
current_temporal_function ::= LOCALTIME opt_temporal_fsp_parens.
current_temporal_function ::= LOCALTIMESTAMP opt_temporal_fsp_parens.

opt_empty_parens ::= .
opt_empty_parens ::= LPAREN RPAREN.
opt_temporal_fsp_parens ::= .
opt_temporal_fsp_parens ::= LPAREN RPAREN.
opt_temporal_fsp_parens ::= LPAREN INTEGER RPAREN.

scalar_function_call ::= POSITION LPAREN expression IN expression RPAREN.
scalar_function_call ::= EXTRACT LPAREN interval_unit FROM expression RPAREN.
scalar_function_call ::= TIMESTAMPADD LPAREN interval_unit COMMA expression COMMA expression RPAREN.
scalar_function_call ::= TIMESTAMPDIFF LPAREN interval_unit COMMA expression COMMA expression RPAREN.
scalar_function_call ::= date_arithmetic_name LPAREN expression COMMA INTERVAL expression interval_unit RPAREN.

date_arithmetic_name ::= DATE_ADD.
date_arithmetic_name ::= ADDDATE.
date_arithmetic_name ::= DATE_SUB.
date_arithmetic_name ::= SUBDATE.

scalar_function_call ::= TRIM LPAREN expression RPAREN.
scalar_function_call ::= TRIM LPAREN trim_operands RPAREN.
trim_operands ::= trim_direction trim_source.
trim_operands ::= trim_source.
trim_source ::= expression FROM expression.
trim_source ::= FROM expression.
trim_direction ::= BOTH.
trim_direction ::= LEADING.
trim_direction ::= TRAILING.

case_expression ::= CASE simple_case_operand case_when_list opt_case_else END.
case_expression ::= CASE searched_case_when_list opt_case_else END.
simple_case_operand ::= expression.

case_when_list ::= case_when_list WHEN expression THEN expression.
case_when_list ::= WHEN expression THEN expression.
searched_case_when_list ::= searched_case_when_list WHEN expression THEN expression.
searched_case_when_list ::= WHEN expression THEN expression.

opt_case_else ::= .
opt_case_else ::= ELSE expression.

interval_unit ::= MICROSECOND.
interval_unit ::= SECOND.
interval_unit ::= MINUTE.
interval_unit ::= HOUR.
interval_unit ::= DAY.
interval_unit ::= WEEK.
interval_unit ::= MONTH.
interval_unit ::= QUARTER.
interval_unit ::= YEAR.
interval_unit ::= SECOND_MICROSECOND.
interval_unit ::= MINUTE_MICROSECOND.
interval_unit ::= MINUTE_SECOND.
interval_unit ::= HOUR_MICROSECOND.
interval_unit ::= HOUR_SECOND.
interval_unit ::= HOUR_MINUTE.
interval_unit ::= DAY_MICROSECOND.
interval_unit ::= DAY_SECOND.
interval_unit ::= DAY_MINUTE.
interval_unit ::= DAY_HOUR.
interval_unit ::= YEAR_MONTH.
```

The actual `mylite_parse.y` file may use precedence declarations and smaller
productions, but the accepted language and AST shape must match this contract.

AST additions should include:

- `MYLITE_SQL_AST_FUNCTION_CALL`
- `MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST`
- `MYLITE_SQL_AST_CASE_EXPRESSION`
- `MYLITE_SQL_AST_CASE_WHEN_LIST`
- `MYLITE_SQL_AST_CASE_WHEN`
- `MYLITE_SQL_AST_INTERVAL_EXPR`
- trim-spec metadata on `MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST`
- a normalized built-in function enum for supported built-ins
- an interval-unit enum shared by temporal function evaluation
- source spans and original function-name spelling for diagnostics and default
  expression labels

The binder should normalize built-in names case-insensitively and map aliases
such as `SUBSTR`, `MID`, `LCASE`, `UCASE`, `CEIL`, `POWER`, and `SCHEMA` to the
same runtime descriptors as their canonical implementations while preserving
the source expression label when no alias is supplied.

## Runtime design

Add a small built-in function registry rather than dispatching by ad hoc string
comparisons in statement code. Each descriptor should include:

- canonical function id and accepted names
- minimum and maximum argument count, or a special-form validator
- evaluation callback
- metadata inference callback
- determinism and statement-timestamp requirements
- null propagation policy where simple
- whether the function can short-circuit argument evaluation

Generic functions can evaluate arguments left to right before invoking the
callback. Short-circuiting forms must own argument evaluation:

- `IF` evaluates its condition, then only the selected branch
- `IFNULL` evaluates the first expression, then the second only when needed
- `COALESCE` evaluates left to right until the first non-`NULL` result
- `CASE` evaluates conditions in order and only the selected result expression
- `NULLIF` must preserve MySQL's special behavior, including the verified
  double evaluation of the first argument when appropriate

The expression evaluation context should grow to include:

- selected schema name
- connection charset/collation
- statement timestamp and session time zone
- user/current-user strings
- per-handle connection id
- last insert id
- previous statement affected-row/row-count state
- current row values for row-dependent expressions

Where SQLite has a similarly named built-in, MyLite should still route through
the MyLite function registry unless a function has been proven to match MySQL
for results, metadata, warnings, errors, type conversion, and edge cases.

## Compatibility decisions

- `VERSION()` returns `mylite_version()` for the embedded runtime. A later
  server/protocol task can decide whether to expose additional MyLite build
  metadata in `version_comment` or related variables.
- `CONNECTION_ID()` returns a stable per-handle unsigned integer. It need not
  represent an operating-system thread.
- `CURRENT_USER()` / bare `CURRENT_USER` and `USER()` use the same documented
  embedded identity until MyLite has authentication, definer, and invoker
  state. The distinction remains modeled in the function registry for future
  protocol/auth work.
- `CHARSET()`, `COLLATION()`, and `COERCIBILITY()` are implemented by the
  dedicated charset/collation introspection slice for the current descriptor
  and AST-visible subset. Full collation coercion, introducers, and explicit
  expression `COLLATE` remain deferred; see
  [charset and collation introspection functions](../charset-collation-functions/specs.md).
- Date and time functions support the session time zone offset needed by the
  current session model. Named time zones and MySQL time-zone tables are
  deferred.
- String case conversion is ASCII-only until broader collation and Unicode case
  semantics are specified and verified.

## MySQL-runtime-verified implementation tests

These tests are suitable for the implementation phase. Expected values below
were verified against MySQL 8.4.9 unless noted as platform-sensitive.

### Parser and binding

| SQL | Expected behavior |
| --- | --- |
| `SELECT CONCAT('a','b')` | parses and returns `ab` |
| `SELECT CONCAT ('a','b')` | parses and returns `ab` on the verified default SQL mode |
| `SELECT CONCAT()` | error 1582 / `42000` |
| `SELECT IF(1,2)` | syntax error 1064 / `42000` |
| `SELECT DATE_ADD('2024-01-01', INTERVAL 1 BOGUS)` | syntax error 1064 / `42000` |
| `SELECT NO_SUCH_FUNCTION(1)` after selecting a schema | error 1305 / `42000`, function does not exist |

### String results

```sql
SET NAMES utf8mb4;
SELECT
  ASCII('') AS ascii_empty,
  ASCII(NULL) AS ascii_null,
  CHAR_LENGTH('海豚') AS char_len_utf8,
  LENGTH('海豚') AS byte_len_utf8,
  CONCAT('My', NULL, 'QL') AS concat_null,
  CONCAT_WS(',', 'a', NULL, '', 'b') AS concat_ws_skip_null,
  LEFT('abcdef', 2) AS left_two,
  RIGHT('abcdef', 3) AS right_three,
  SUBSTRING('abcdef', 2, 3) AS substr_mid,
  SUBSTRING('abcdef', -2) AS substr_neg,
  POSITION('ph' IN 'alpha') AS position_value,
  INSTR('alpha', 'z') AS instr_miss,
  REPLACE('a.b.b', 'b', 'x') AS replaced,
  QUOTE('Don''t') AS quoted_text,
  QUOTE(NULL) AS quoted_null,
  REPEAT('xy', -1) AS repeat_neg,
  LPAD('hi', 5, '.') AS lpad_value,
  TRIM(LEADING 'x' FROM 'xxhix') AS trim_leading,
  HEX('Az') AS hex_text,
  UNHEX('417a') AS unhex_text;
```

Expected row:

```text
0, NULL, 2, 6, NULL, a,,b, ab, def, bcd, ef, 3, 0,
a.x.x, 'Don\'t', NULL text, empty string, ...hi, hix, 417A, Az
```

### Numeric results and warnings

```sql
SELECT
  ABS(-12.5),
  SIGN(-12.5),
  ROUND(2.5),
  ROUND(25E-1),
  ROUND(123.456, 2),
  FLOOR(-1.2),
  CEILING(-1.2),
  POW(2, 10),
  SQRT(9),
  SQRT(-1),
  MOD(7, 3);
```

Expected row:

```text
12.5, -1, 3, 2, 123.46, -2, -1, 1024, 3, NULL, 1
```

`ROUND(25E-1)` is approximate-input behavior. Keep a focused MySQL runtime
comparison for the supported CI platforms instead of assuming all C libraries
round the same way.

```sql
SELECT SQRT('foo') AS sqrt_foo;
SHOW WARNINGS;
```

Expected result `0` and warning 1292, `Truncated incorrect DOUBLE value:
'foo'`.

### Conditional and comparison results

```sql
SELECT
  IF(0, 'yes', 'no'),
  IF(NULL, 'yes', 'no'),
  IFNULL(NULL, 'fallback'),
  NULLIF('a', 'a'),
  NULLIF('a', 'b'),
  COALESCE(NULL, NULL, 'x'),
  GREATEST('11','45','2'),
  GREATEST(11,45,2),
  LEAST('11','45','2'),
  GREATEST(1,NULL,2),
  ISNULL(NULL);
```

Expected row:

```text
no, no, fallback, NULL, a, x, 45, 45, 11, NULL, 1
```

Short-circuit test:

```sql
SELECT IF(0, 1/0, 1), IF(1, 1, 1/0), COALESCE(1, 1/0);
SHOW WARNINGS;
```

Expected row `1, 1, 1.0000` and no warnings.

`NULLIF` evaluation test:

```sql
SELECT NULLIF(1/0, 2);
SHOW WARNINGS;
```

Expected result `NULL` and two warning 1365 records.

### Temporal results

```sql
SET time_zone = '+00:00';
SET timestamp = 1700000000;
SELECT
  NOW(6),
  NOW() = CURRENT_TIMESTAMP,
  CURDATE(),
  CURTIME(6),
  UTC_TIMESTAMP(),
  DATE('2024-02-29 12:34:56'),
  YEAR('2024-02-29'),
  MONTH('2024-02-29'),
  DAYOFMONTH('2001-11-00'),
  HOUR('-03:04:05.000006'),
  MICROSECOND('12:34:56.123456'),
  DATEDIFF('2024-03-01','2024-02-28'),
  TIMESTAMPDIFF(DAY,'2024-02-28','2024-03-01'),
  DATE_ADD('2024-02-29', INTERVAL 1 DAY),
  DATE_SUB('2024-03-01', INTERVAL 1 DAY),
  EXTRACT(YEAR_MONTH FROM '2024-02-29 12:34:56'),
  TIMESTAMPADD(DAY, 2, '2024-02-28');
```

Expected row:

```text
2023-11-14 22:13:20.000000, 1, 2023-11-14, 22:13:20.000000,
2023-11-14 22:13:20, 2024-02-29, 2024, 2, 0, 3, 123456,
2, 2, 2024-03-01, 2024-02-29, 202402, 2024-03-01
```

Incomplete date warning test:

```sql
SELECT DATE_ADD('2006-05-00', INTERVAL 1 DAY), DAYNAME('2006-05-00');
SHOW WARNINGS;
```

Expected result `NULL, NULL` and two warning 1292 records. `DAYNAME` itself is
deferred, but this test documents the complete-date rejection behavior that
in-scope strict temporal arithmetic should follow.

### Information and statement-state results

```sql
USE mylite_task24_functions;
SELECT DATABASE(), SCHEMA(), VERSION(), USER(), CURRENT_USER(),
       LAST_INSERT_ID(), CONNECTION_ID() IS NOT NULL;
```

Expected values on the verified container:

```text
mylite_task24_functions, mylite_task24_functions, 8.4.9,
root@localhost, root@localhost, 1, 1
```

Implementation tests should avoid hard-coding the MySQL container's exact user
for MyLite unless the embedded session identity is intentionally set to that
value. They should assert the MyLite session identity configured by the test.

Scalar functions in row expressions:

```sql
SELECT id, CONCAT(s, ':', n) AS label
FROM f
WHERE ABS(n) >= 2 OR IFNULL(s,'') = 'alpha'
ORDER BY LENGTH(s), id;
```

Expected rows:

```text
2, Beta:-2
1, alpha:1
```

Scalar functions in writes:

```sql
UPDATE f SET s = UPPER(s) WHERE id = 1;
SELECT ROW_COUNT(), s FROM f WHERE id = 1;

SET timestamp = 1700000000;
INSERT INTO f (s,n,d,dt,da,ti)
VALUES (CONCAT('row', 4), ROUND(2.5), ABS(-3.50),
        DATE_ADD('2024-01-01', INTERVAL 1 DAY), CURDATE(), CURTIME(6));
SELECT ROW_COUNT(), LAST_INSERT_ID();
```

Expected first result `1, ALPHA`. Expected insert state is one affected row, a
new last insert id, `s='row4'`, `n=3`, `d=3.50`,
`dt='2024-01-02 00:00:00.000000'`, and statement-time `CURDATE`/`CURTIME`
values.

### Metadata tests

Use:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --column-type-info -vvv
```

and `LIMIT 0` queries for:

- string functions: `CONCAT`, `CONCAT_WS`, `LENGTH`, `CHAR_LENGTH`,
  `SUBSTRING`, `SUBSTR`, `MID`, `TRIM`, `LTRIM`, `RTRIM`
- numeric functions: `ABS`, `ROUND`, `POW`, `SQRT`
- conditional/comparison functions: `IF`, `IFNULL`, `COALESCE`, `GREATEST`
- temporal functions: `NOW(6)`, `CURDATE`, `DATEDIFF`, `DATE_ADD`
- information functions: `DATABASE`, `SCHEMA`, `VERSION`, `LAST_INSERT_ID`,
  `ROW_COUNT`, `CONNECTION_ID`

Expected metadata is listed in the result metadata section above.

## Implementation handoff

Suggested implementation order:

1. Add parser tokens and AST nodes for generic calls, special syntactic forms,
   `CASE`, interval units, and trim specs.
2. Add a built-in function registry with argument-count validation and metadata
   descriptors.
3. Extend `mylite_expression_eval_with_context` so all current expression
   call sites can evaluate supported function calls.
4. Add statement timestamp, session timezone, user/current-user, connection id,
   last-insert-id, and row-count access to the expression context. The
   session-information-functions slice implements selected-schema,
   last-insert-id, and previous-row-count access.
5. Extend expression metadata inference for each in-scope function.
6. Add MySQL-runtime comparison tests before claiming support in
   `COMPATIBILITY.md`.

Primary risk areas:

- metadata inference without accidental expression evaluation
- warning count and warning order for conversion and temporal parsing
- short-circuit evaluation and `NULLIF` double-evaluation behavior
- temporal parsing of incomplete dates and fractional seconds
- string byte length vs character length under connection charset state
- accidental reliance on SQLite functions with incompatible MySQL edge cases
