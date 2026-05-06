# SHOW CHARACTER SET / SHOW CHARSET

## Scope

This feature implements the first executable slice of MySQL's character-set
introspection statement:

- `SHOW CHARACTER SET`
- `SHOW CHARSET`
- `SHOW CHAR SET`
- all forms with `LIKE 'pattern'`
- all forms with `WHERE expr`, parsed but rejected at execution time

The executable catalog exposes only character sets that MyLite can currently
accept and use through its charset/collation registry:

- `binary`
- `latin1`
- `utf8mb3`
- `utf8mb4`

Deferred surfaces:

- full MySQL character-set catalog
- executable `SHOW CHARACTER SET ... WHERE expr` filtering
- broader `INFORMATION_SCHEMA.CHARACTER_SETS` query support beyond
  `SELECT *`
- privilege filtering, if any future execution surface needs it

## Compatibility Sources

- MySQL 8.4 Reference Manual, `SHOW CHARACTER SET` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-character-set.html
- MySQL 8.4 Reference Manual, Extensions to `SHOW` Statements:
  https://dev.mysql.com/doc/refman/8.4/en/extended-show.html
- MySQL 8.4 Reference Manual, Supported Character Sets and Collations:
  https://dev.mysql.com/doc/refman/8.4/en/charset-charsets.html
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
| `SHOW CHARACTER SET` | Columns `Charset`, `Description`, `Default collation`, `Maxlen`; 41 rows ordered by `Charset`, from `armscii8` through `utf8mb4`. |
| `SHOW CHARSET` | Same rows and columns as `SHOW CHARACTER SET`. |
| `SHOW CHAR SET LIKE 'utf8%'` | Accepted even though the manual spelling is `CHARSET`; returns `utf8mb3`, then `utf8mb4`. |
| `SHOW CHARACTER SET LIKE 'utf8%'` | Returns `utf8mb3`, then `utf8mb4`. |
| `SHOW CHARACTER SET LIKE 'UTF8%'` | Returns the same rows as the lowercase pattern; matching is case-insensitive in the verified runtime. |
| `SHOW CHARACTER SET LIKE 'utf8\_mb%'` | Returns no rows because escaped `_` is literal and MySQL names are `utf8mb...`. |
| `SHOW CHARACTER SET LIKE 'utf8_mb%'` | Returns no rows because `_` consumes only `m`, then the following literal `m` does not match `b`. |
| `SHOW CHARACTER SET LIKE 'latin%'` | Returns `latin1`, `latin2`, `latin5`, `latin7` in `Charset` order. |
| `SHOW CHARACTER SET LIKE 'binary'` | Returns `binary`, `Binary pseudo charset`, `binary`, `1`. |
| `SHOW CHARACTER SET LIKE 'filename'` | Returns no rows; `filename` is internal and not displayed. |
| `SHOW CHARACTER SET WHERE Charset IN ('utf8mb4','utf8mb3','latin1','binary')` | Returns `binary`, `latin1`, `utf8mb3`, `utf8mb4` sorted by `Charset`. |
| ``SHOW CHARACTER SET WHERE `Default collation` = 'binary'`` | Returns the `binary` row. |
| `SHOW CHARACTER SET WHERE Description LIKE '%Unicode%'` | Returns Unicode character sets such as `ucs2`, `utf16`, `utf8mb3`, and `utf8mb4`. |
| `SHOW CHARACTER SET WHERE Maxlen > 3` | Returns four-byte character sets including `gb18030` and `utf8mb4`. |
| `SHOW CHARACTER SET WHERE No_such_column = 1` | Error `1054`, SQLSTATE `42S22`, unknown column. |
| `SHOW CHARACTER SET LIKE 1` | Syntax error `1064`; the `LIKE` pattern must be a string literal. |
| `SHOW CHARACTER SET LIMIT 1` | Syntax error `1064`; `LIMIT` is not part of this statement. |
| missing-table error; `SHOW COUNT(*) ERRORS`; `SHOW CHARACTER SET LIKE 'utf8%'`; `SHOW COUNT(*) ERRORS` | `SHOW CHARACTER SET` is nondiagnostic and clears the earlier error before reporting, so the final error count is `0`. |

Column metadata observed for `SHOW CHARACTER SET LIKE 'binary'`:

- `Charset`, `Description`, and `Default collation` are non-null string columns.
- `Maxlen` is a non-null unsigned integer column (`LONG`) with binary collation.

## Syntax

MyLite owns the grammar below; it is intentionally authored for MyLite's Lemon
parser rather than copied from MySQL sources:

```lemon
statement ::= show_character_set_statement.

show_character_set_statement ::= SHOW show_character_set_keyword
                                 opt_show_character_set_filter.

show_character_set_keyword ::= CHARACTER SET.
show_character_set_keyword ::= CHAR SET.
show_character_set_keyword ::= CHARSET.

opt_show_character_set_filter ::= .
opt_show_character_set_filter ::= LIKE STRING.
opt_show_character_set_filter ::= where_clause.
```

`CHARSET` must remain available as a nonreserved identifier outside this
production through the existing fallback behavior. `CHARACTER`, `CHAR`, and
`SET` are reserved by MySQL and remain reserved in MyLite.

## AST

Add a `show_character_set_statement` AST node with one optional child:

- a string-literal `LIKE` pattern child, or
- a `WHERE` clause child

The statement span must cover `SHOW` through the last token in the statement.
No scope marker is needed.

## Runtime Semantics

Rows:

- The result set has exactly four columns in this order:
  - `Charset`
  - `Description`
  - `Default collation`
  - `Maxlen`
- `Maxlen` is numeric, not a string placeholder. The SQLite-backed statement
  should emit it as an integer expression.
- `INFORMATION_SCHEMA.CHARACTER_SETS` uses the same shared registry and emits
  the same character-set rows with uppercase information-schema column names.
- Rows are ordered by `Charset` case-insensitively, then by binary name for
  deterministic tie-breaking. This matches the current MyLite ordering policy
  used by `SHOW VARIABLES` and `SHOW STATUS`.
- Successful `SHOW CHARACTER SET` produces no warnings.
- `mylite_affected_rows()` remains `-1` for the read-only SQLite-backed result.
- `SHOW CHARACTER SET` is a nondiagnostic statement. Like MySQL, it clears
  prior diagnostics before producing rows.

Catalog for this slice:

| Charset | Description | Default collation | Maxlen |
| --- | --- | --- | --- |
| `binary` | `Binary pseudo charset` | `binary` | `1` |
| `latin1` | `cp1252 West European` | `latin1_swedish_ci` | `1` |
| `utf8mb3` | `UTF-8 Unicode` | `utf8mb3_general_ci` | `3` |
| `utf8mb4` | `UTF-8 Unicode` | `utf8mb4_0900_ai_ci` | `4` |

The MySQL internal `filename` character set is not displayed. MyLite should not
invent rows for unsupported MySQL character sets, because applications could
then select or declare character sets that the runtime cannot honor.

LIKE filtering:

- `%` matches any byte sequence.
- `_` matches one byte.
- Backslash escapes the following byte for SHOW-pattern purposes.
- Matching is case-insensitive for character-set names in the verified MySQL
  runtime.
- Filtering applies to the displayed `Charset` name.

WHERE filtering:

- `WHERE expr` is evaluated over displayed result columns, including
  backtick-quoted `Default collation`.
- The shared SHOW filter supports displayed-column identifiers, literals,
  comparison operators, `AND`/`OR`/`NOT`, `LIKE`, `IN`, unary signs,
  `IS NULL`, `IS NOT NULL`, and parentheses.
- Unknown displayed-column identifiers return MySQL error `1054`.
- Broader SHOW `WHERE` expressions remain deferred.

## Storage And Performance

This feature is read-only and requires no file format change. Runtime execution
materializes the small supported charset registry into a SQLite read statement.
No mutable process-global state or new dependency is needed.

## Tests

Parser coverage:

- `SHOW CHARACTER SET`
- `SHOW CHARSET`
- `SHOW CHAR SET`
- each spelling with `LIKE`
- `SHOW CHARACTER SET WHERE Charset = 'utf8mb4'`
- ``SHOW CHARACTER SET WHERE `Default collation` = 'binary'``
- syntax rejection for non-string `LIKE`, combined `LIKE` plus `WHERE`, and
  `SHOW CHARACTER SET LIMIT 1`
- `CHARSET` as an unquoted identifier where fallback permits it

Runtime coverage:

- exact result column names
- unfiltered catalog contains only the supported MyLite registry subset
- deterministic row ordering
- exact row values for `binary`, `latin1`, `utf8mb3`, and `utf8mb4`
- `Maxlen` text values for row comparison and at least one direct
  `mylite_column_int64()` assertion
- `SHOW CHARSET` and `SHOW CHAR SET` synonyms
- `LIKE` exact, wildcard, escaped underscore, case-insensitive, and empty
  result behavior
- `filename` returns no row
- parsed but unsupported `WHERE` diagnostic
- `LIMIT` syntax rejection through prepare
- clearing of previous warnings/errors
- affected rows remain `-1`

## Known Incompatibilities

- MyLite exposes the supported registry subset instead of MySQL's full 41-row
  character-set catalog. This is intentional for the first executable slice.
- `WHERE` filtering is parsed but not executed yet, matching the current
  `SHOW VARIABLES` and `SHOW STATUS` compatibility pattern.
