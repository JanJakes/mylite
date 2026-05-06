# SHOW COLLATION

## Scope

This feature implements the first executable slice of MySQL's collation
introspection statement:

- `SHOW COLLATION`
- `SHOW COLLATION LIKE 'pattern'`
- `SHOW COLLATION WHERE expr`

The executable catalog exposes only collations that MyLite can currently accept
and use through its charset/collation registry:

- `binary`
- `latin1_bin`
- `latin1_swedish_ci`
- `utf8mb3_bin`
- `utf8mb3_general_ci`
- `utf8mb4_0900_ai_ci`
- `utf8mb4_bin`
- `utf8mb4_unicode_520_ci`
- `utf8mb4_unicode_ci`

Deferred surfaces:

- full MySQL collation catalog
- executable `SHOW COLLATION ... WHERE expr` filtering
- privilege filtering, if any future execution surface needs it

`INFORMATION_SCHEMA.COLLATIONS` is implemented by the separate
[INFORMATION_SCHEMA.COLLATIONS](../information-schema-collations/specs.md)
slice and shares the same supported registry.

## Compatibility Sources

- MySQL 8.4 Reference Manual, `SHOW COLLATION` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-collation.html
- MySQL 8.4 Reference Manual, Extensions to `SHOW` Statements:
  https://dev.mysql.com/doc/refman/8.4/en/extended-show.html
- MySQL 8.4 Reference Manual, Collation Issues:
  https://dev.mysql.com/doc/refman/8.4/en/charset-collations.html
- Runtime observations verified against Docker container `mylite-mysql-849`,
  MySQL `8.4.9`, using:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --show-warnings --force
docker exec -i mylite-mysql-849 mysql -uroot --column-type-info -vvv --force
```

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar or
implementation sources.

## MySQL 8.4.9 Runtime Observations

The following behavior was verified against MySQL 8.4.9:

| SQL | Result |
| --- | --- |
| `SHOW COLLATION` | Columns `Collation`, `Charset`, `Id`, `Default`, `Compiled`, `Sortlen`, `Pad_attribute`; rows are ordered by `Collation`. |
| `SHOW COLLATION WHERE Collation IN (...)` for the MyLite-supported names | Returns `binary`, `latin1_bin`, `latin1_swedish_ci`, `utf8mb3_bin`, `utf8mb3_general_ci`, `utf8mb4_0900_ai_ci`, `utf8mb4_bin`, `utf8mb4_unicode_520_ci`, and `utf8mb4_unicode_ci` in collation-name order. |
| `SHOW COLLATION LIKE 'utf8mb4\_%'` | Returns all displayed `utf8mb4_...` collations, including `utf8mb4_0900_ai_ci`, `utf8mb4_bin`, `utf8mb4_unicode_520_ci`, and `utf8mb4_unicode_ci`. |
| `SHOW COLLATION LIKE 'UTF8MB4\_%'` | Returns the same rows as the lowercase escaped pattern; matching is case-insensitive in the verified runtime. |
| `SHOW COLLATION LIKE 'utf8mb4\_bin'` | Returns the `utf8mb4_bin` row. |
| `SHOW COLLATION LIKE 'UTF8MB4\_BIN'` | Returns the `utf8mb4_bin` row; escaped literals are still matched case-insensitively. |
| `SHOW COLLATION LIKE 'latin1\_%'` | Returns `latin1_*` rows in collation-name order. |
| `SHOW COLLATION LIKE 'LATIN1\_%'` | Returns the same rows as the lowercase pattern. |
| `SHOW COLLATION LIKE 'binary'` | Returns `binary`, `binary`, `63`, `Yes`, `Yes`, `1`, `NO PAD`. |
| `SHOW COLLATION LIKE 'BINARY'` | Returns the same `binary` row. |
| `SHOW COLLATION WHERE Charset = 'latin1' AND Collation IN ('latin1_swedish_ci','latin1_bin')` | Returns `latin1_bin`, then `latin1_swedish_ci`. |
| ``SHOW COLLATION WHERE `Default` = 'Yes' AND Charset IN (...)`` | Returns the default supported collations for `binary`, `latin1`, `utf8mb3`, and `utf8mb4`. |
| `SHOW COLLATION WHERE Pad_attribute = 'NO PAD' AND Collation IN ('utf8mb4_0900_ai_ci','utf8mb4_bin')` | Returns only `utf8mb4_0900_ai_ci`. |
| `SHOW COLLATION WHERE Sortlen > 1 AND Charset = 'latin1'` | Returns no rows among the MyLite-supported subset because both supported `latin1` collations have `Sortlen` 1. |
| `SHOW COLLATION WHERE No_such_column = 1` | Error `1054`, SQLSTATE `42S22`, unknown column. |
| `SHOW COLLATION LIKE 1` | Syntax error `1064`; the `LIKE` pattern must be a string literal. |
| `SHOW COLLATION LIMIT 1` | Syntax error `1064`; `LIMIT` is not part of this statement. |
| missing-table error; `SHOW COUNT(*) ERRORS`; `SHOW COLLATION LIKE 'utf8mb4%'`; `SHOW COUNT(*) ERRORS` | `SHOW COLLATION` is nondiagnostic and clears the earlier error before reporting, so the final error count is `0`. |

Column metadata observed for `SHOW COLLATION LIKE 'binary'`:

| Column | Type | Collation | Length | Flags |
| --- | --- | --- | ---: | --- |
| `Collation` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL NO_DEFAULT_VALUE` |
| `Charset` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL NO_DEFAULT_VALUE` |
| `Id` | `LONGLONG` | `binary` | 20 | `NOT_NULL UNSIGNED NUM` |
| `Default` | `VAR_STRING` | `latin1_swedish_ci` | 3 | `NOT_NULL` |
| `Compiled` | `VAR_STRING` | `latin1_swedish_ci` | 3 | `NOT_NULL` |
| `Sortlen` | `LONG` | `binary` | 10 | `NOT_NULL UNSIGNED NO_DEFAULT_VALUE NUM` |
| `Pad_attribute` | `STRING` | `latin1_swedish_ci` | 9 | `NOT_NULL BINARY ENUM NO_DEFAULT_VALUE` |

## Syntax

MyLite owns the grammar below; it is intentionally authored for MyLite's Lemon
parser rather than copied from MySQL sources:

```lemon
statement ::= show_collation_statement.

show_collation_statement ::= SHOW COLLATION opt_show_collation_filter.

opt_show_collation_filter ::= .
opt_show_collation_filter ::= LIKE STRING.
opt_show_collation_filter ::= where_clause.
```

The `LIKE` branch accepts only string literals. Numeric, identifier, expression,
or parameter-marker patterns remain syntax errors until MySQL-compatible
prepared statements require a broader surface.

## AST

Add a `show_collation_statement` AST node with one optional child:

- a string-literal `LIKE` pattern child, or
- a `WHERE` clause child

The statement span must cover `SHOW` through the last token in the statement.
No scope marker is needed.

## Runtime Semantics

Rows:

- The result set has exactly seven columns in this order:
  - `Collation`
  - `Charset`
  - `Id`
  - `Default`
  - `Compiled`
  - `Sortlen`
  - `Pad_attribute`
- `Id` and `Sortlen` are numeric, not string placeholders.
- `Default` is `Yes` for the default collation of its character set and an
  empty string otherwise.
- `Compiled` is `Yes` for every supported collation in this slice.
- Rows are ordered by `Collation` case-insensitively, then by binary name for
  deterministic tie-breaking. This matches the current MyLite ordering policy
  used by `SHOW VARIABLES`, `SHOW STATUS`, and `SHOW CHARACTER SET`.
- Successful `SHOW COLLATION` produces no warnings.
- `mylite_affected_rows()` remains `-1` for the read-only SQLite-backed result.
- `SHOW COLLATION` is a nondiagnostic statement. Like MySQL, it clears prior
  diagnostics before producing rows.

Catalog for this slice:

| Collation | Charset | Id | Default | Compiled | Sortlen | Pad_attribute |
| --- | --- | ---: | --- | --- | ---: | --- |
| `binary` | `binary` | 63 | `Yes` | `Yes` | 1 | `NO PAD` |
| `latin1_bin` | `latin1` | 47 | `` | `Yes` | 1 | `PAD SPACE` |
| `latin1_swedish_ci` | `latin1` | 8 | `Yes` | `Yes` | 1 | `PAD SPACE` |
| `utf8mb3_bin` | `utf8mb3` | 83 | `` | `Yes` | 1 | `PAD SPACE` |
| `utf8mb3_general_ci` | `utf8mb3` | 33 | `Yes` | `Yes` | 1 | `PAD SPACE` |
| `utf8mb4_0900_ai_ci` | `utf8mb4` | 255 | `Yes` | `Yes` | 0 | `NO PAD` |
| `utf8mb4_bin` | `utf8mb4` | 46 | `` | `Yes` | 1 | `PAD SPACE` |
| `utf8mb4_unicode_520_ci` | `utf8mb4` | 246 | `` | `Yes` | 8 | `PAD SPACE` |
| `utf8mb4_unicode_ci` | `utf8mb4` | 224 | `` | `Yes` | 8 | `PAD SPACE` |

MyLite should not invent rows for unsupported MySQL collations, because
applications could then select or declare collations that the runtime cannot
honor.

LIKE filtering:

- `%` matches any byte sequence.
- `_` matches one byte.
- Backslash escapes the following byte for SHOW-pattern purposes.
- Matching is case-insensitive for collation names in the verified MySQL
  runtime.
- Filtering applies to the displayed `Collation` name.

WHERE filtering:

- `WHERE expr` is evaluated over displayed result columns, including quoted
  `Default`.
- The shared SHOW filter supports displayed-column identifiers, literals,
  comparison operators, `AND`/`OR`/`NOT`, `LIKE`, `IN`, unary signs,
  `IS NULL`, `IS NOT NULL`, and parentheses.
- Unknown displayed-column identifiers return MySQL error `1054`.
- Broader SHOW `WHERE` expressions remain deferred.

## Storage And Performance

This feature is read-only and requires no file format change. Runtime execution
materializes the small supported collation registry into a SQLite read
statement. No mutable process-global state or new dependency is needed.

The charset/collation registry should expose collation iteration in addition to
lookup so `SHOW COLLATION` uses the same source of truth as DDL validation.

## Tests

Parser coverage:

- `SHOW COLLATION`
- `SHOW COLLATION LIKE 'utf8mb4%'`
- `SHOW COLLATION WHERE Charset = 'utf8mb4'`
- ``SHOW COLLATION WHERE `Default` = 'Yes'``
- syntax rejection for non-string `LIKE`, combined `LIKE` plus `WHERE`, and
  `SHOW COLLATION LIMIT 1`

Runtime coverage:

- exact result column names
- unfiltered catalog contains only the supported MyLite registry subset
- deterministic row ordering
- exact row values for all supported collations
- `Id` and `Sortlen` text values for row comparison, plus direct
  `mylite_column_int64()` assertions
- `LIKE` exact, wildcard, escaped underscore, case-insensitive, and empty
  result behavior
- parsed but unsupported `WHERE` diagnostic
- `LIMIT` syntax rejection through prepare
- clearing of previous warnings/errors
- affected rows remain `-1`

## Known Incompatibilities

- MyLite exposes the supported registry subset instead of MySQL's full
  collation catalog through both `SHOW COLLATION` and
  `INFORMATION_SCHEMA.COLLATIONS` and
  `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`. This is
  intentional for the first executable slice.
- `WHERE` filtering is parsed but not executed yet, matching the current
  `SHOW VARIABLES`, `SHOW STATUS`, and `SHOW CHARACTER SET` compatibility
  pattern.
