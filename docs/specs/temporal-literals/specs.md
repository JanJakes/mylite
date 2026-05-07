# Temporal literals

## Scope

This slice implements MySQL-compatible temporal literal expression syntax for
the currently supported scalar expression paths:

- `DATE 'YYYY-MM-DD'`
- `TIME 'HH:MM:SS[.fraction]'`
- `TIMESTAMP 'YYYY-MM-DD HH:MM:SS[.fraction]'`

The first implementation covers parser acceptance, AST representation,
evaluation as typed temporal text, result labels, result metadata, supported
DML source expressions, and SQL-mode interaction for double-quoted strings.

Out of scope:

- exhaustive temporal literal validation and diagnostics
- fractional rounding beyond the already stored literal text
- ODBC escape temporal literals
- time-zone conversion
- temporal comparison and arithmetic conversion beyond the existing expression
  evaluator behavior

## Sources

- MySQL 8.4 Reference Manual, Date and Time Literals:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-literals.html
- MySQL 8.4 Reference Manual, Date and Time Data Types:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-types.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`, using `docker exec -i mylite-mysql-849 mysql -uroot`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 Behavior

Observed values:

```sql
SELECT DATE '2024-02-29',
       TIME '12:34:56.123456',
       TIMESTAMP '2024-02-29 12:34:56.123456';
-- 2024-02-29, 12:34:56.123456, 2024-02-29 12:34:56.123456
```

Observed result metadata:

| Expression | Type | Length | Decimals | Charset | Flags |
| --- | --- | --- | --- | --- | --- |
| `DATE '2024-02-29'` | DATE | 10 | 0 | binary / 63 | NOT_NULL, BINARY |
| `TIME '12:34:56'` | TIME | 10 | 0 | binary / 63 | NOT_NULL, BINARY |
| `TIME '12:34:56.123456'` | TIME | 17 | 6 | binary / 63 | NOT_NULL, BINARY |
| `TIMESTAMP '2024-02-29 12:34:56'` | DATETIME | 19 | 0 | binary / 63 | NOT_NULL, BINARY |
| `TIMESTAMP '2024-02-29 12:34:56.123456'` | DATETIME | 26 | 6 | binary / 63 | NOT_NULL, BINARY |

Unaliased result column names preserve the source expression text, such as
`DATE '2024-02-29'`.

Adjacent keyword/string tokens are accepted:

```sql
SELECT DATE'2024-02-29', TIME'12:34:56';
```

Double-quoted temporal literal strings are accepted when `ANSI_QUOTES` is not
active and rejected as syntax when `ANSI_QUOTES` is active because the double
quoted text becomes an identifier token.

Invalid temporal values such as `DATE '2024-02-30'`, date-only `TIMESTAMP`
literals, and `TIME '838:59:59.999999'` return error 1525 in the observed
runtime. MyLite leaves exhaustive value validation to a follow-up slice.

## MyLite Design

The parser represents temporal literals as `MYLITE_SQL_AST_LITERAL` nodes with
dedicated literal kinds:

- `MYLITE_SQL_AST_LITERAL_DATE`
- `MYLITE_SQL_AST_LITERAL_TIME`
- `MYLITE_SQL_AST_LITERAL_TIMESTAMP`

The temporal literal node span covers the full source expression so result
labels match MySQL. The node owns the underlying string literal as its first
child so runtime evaluation can reuse existing string decoding and SQL-mode
escape behavior.

Evaluation returns a text expression value with `temporal_type` set to DATE,
TIME, or TIMESTAMP. `TIMESTAMP` literals intentionally use DATETIME field
metadata, matching MySQL's observed column definition packets for literal
expressions.

## Lemon Grammar Snippets

```lemon
primary_expression ::= temporal_literal.

temporal_literal ::= DATE_LITERAL STRING.
temporal_literal ::= TIME_LITERAL STRING.
temporal_literal ::= TIMESTAMP_LITERAL STRING.
```

The parser maps `DATE`, `TIME`, and `TIMESTAMP` keywords to the
`*_LITERAL` parser tokens only when the next non-comment token is a string
literal. This preserves ordinary keyword-as-identifier behavior such as
selecting a column named `time` while accepting adjacent temporal literals.
`STRING` follows the active lexical SQL mode. With `ANSI_QUOTES`,
double-quoted text does not match the temporal literal grammar.

## Tests

Runtime tests cover:

- scalar values for `DATE`, `TIME`, and `TIMESTAMP` temporal literals
- unaliased expression labels
- MySQL-compatible result metadata
- adjacent keyword/string token parsing
- default-mode double-quoted temporal literal strings
- `ANSI_QUOTES` rejection for double-quoted temporal literal strings
- insertion into supported DATE, TIME, DATETIME, and TIMESTAMP columns

Parser tests cover the AST literal kinds and source spans for the three
temporal literal forms.

## Compatibility Gaps

- Error 1525 value validation for malformed or out-of-range temporal literals
  is deferred.
- Fractional rounding for more than six fractional digits is deferred.
- ODBC escape literals such as `{d '2024-02-29'}` are deferred.
- Full temporal type conversion, comparison, and arithmetic semantics remain
  tracked by the broader type-conversion and temporal-function tasks.
