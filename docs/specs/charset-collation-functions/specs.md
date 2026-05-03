# Charset and collation introspection functions

This feature implements MySQL-compatible `CHARSET(expr)`, `COLLATION(expr)`,
and `COERCIBILITY(expr)` for MyLite's current expression surface.

The implementation is intentionally scoped to introspection. It does not add
general collation coercion, character-set introducer syntax, `COLLATE`
expression syntax, charset conversion, or broader expression value charset
metadata.

## Sources

Behavior is based on:

- official MySQL 8.4 documentation for information functions and collation
  coercibility
- runtime probes against MySQL 8.4.9 in the `mylite-mysql-849` Docker
  container
- MyLite's existing charset/collation foundation, scalar function, `CHAR()`,
  `HEX()`/`UNHEX()`, and base-conversion function specifications

This specification is independently authored for MyLite and does not copy
MySQL grammar or implementation text.

## Syntax

The three functions are unary scalar functions:

```sql
CHARSET(expr)
COLLATION(expr)
COERCIBILITY(expr)
```

MySQL 8.4.9 reports native syntax errors for wrong `CHARSET()` and
`COLLATION()` arity, and error 1582 for wrong `COERCIBILITY()` arity. MyLite
uses the existing scalar function arity validation. Exact native parse-error
placement and error-code differences for unsupported arity remain deferred.

### MyLite Lemon grammar sketch

The existing generic function-call grammar is sufficient for these functions:

```lemon
expr(A) ::= identifier(N) LP function_argument_list(Args) RP. {
  A = myliteAstFunctionCall(pParse, &N, Args);
}

function_argument_list(A) ::= expr(E). {
  A = myliteAstFunctionArguments(pParse, E);
}
```

`CHARSET`, `COLLATION`, and `COERCIBILITY` are then recognized by scalar
function name resolution and require exactly one argument.

## Semantics

`CHARSET(expr)` returns the character set name associated with `expr`.

`COLLATION(expr)` returns the collation name associated with `expr`.

`COERCIBILITY(expr)` returns MySQL's numeric coercibility rank for `expr`:

| Expression category | Result |
| --- | ---: |
| explicit character/collation surfaces currently visible to MyLite, including `CAST(... AS CHAR)` and `CAST(... AS BINARY)` | 2 |
| table columns | 2 |
| string literals and string-producing scalar functions | 4 |
| numeric expressions | 5 |
| `NULL` expressions with no non-null string result source | 6 |

The result is independent of the runtime value for table columns. A nullable
string column still reports its declared charset/collation and coercibility `2`
when the current row value is `NULL`, matching MySQL 8.4.9.

## Supported expression categories

The feature supports the current scalar expression call sites:

- no-table `SELECT`
- one-table `SELECT` projections, `WHERE`, and `ORDER BY`
- supported `UPDATE` assignment, `WHERE`, and `ORDER BY` expressions
- supported `DELETE` predicate expressions
- scalar expressions nested in already-supported subquery paths where the
  expression descriptor is available

The supported introspection sources are:

- ordinary string literals under the current `character_set_connection` and
  `collation_connection`
- `NULL`, integer, decimal, real, boolean, and string literals that contain
  temporal-looking text
- table `CHAR`, `VARCHAR`, `TEXT`, `BINARY`, `VARBINARY`, and `BLOB` columns
  where the catalog descriptor carries charset/collation information
- `CHAR()` with no `USING`, and `USING binary`, `latin1`, `utf8mb4`,
  `utf8mb3`/`utf8`, and `ascii`
- `HEX()`, `UNHEX()`, `BIN()`, `OCT()`, and `CONV()`
- supported string functions including `CONCAT()`, `CONCAT_WS()`, `QUOTE()`,
  `MAKE_SET()`, `ELT()`, `IF()`, `IFNULL()`, `COALESCE()`, and `NULLIF()`
- `CAST(... AS CHAR)` and `CAST(... AS BINARY)`

## Charset and collation mapping

MyLite reports only charsets and collations present in, or already exposed by,
the current charset foundation:

| Surface | `CHARSET()` | `COLLATION()` |
| --- | --- | --- |
| default connection literal after startup or `SET NAMES utf8mb4` | `utf8mb4` | `utf8mb4_0900_ai_ci` |
| `SET NAMES utf8mb4 COLLATE utf8mb4_bin` literal | `utf8mb4` | `utf8mb4_bin` |
| `SET NAMES latin1` literal | `latin1` | `latin1_swedish_ci` |
| `SET NAMES latin1 COLLATE latin1_bin` literal | `latin1` | `latin1_bin` |
| numeric expression | `binary` | `binary` |
| `NULL` expression | `binary` | `binary` |
| no-`USING` `CHAR()` | `binary` | `binary` |
| `CHAR(... USING binary)` | `binary` | `binary` |
| `CHAR(... USING latin1)` | `latin1` | `latin1_swedish_ci` |
| `CHAR(... USING utf8mb4)` | `utf8mb4` | `utf8mb4_0900_ai_ci` |
| `CHAR(... USING utf8mb3)` or `CHAR(... USING utf8)` | `utf8mb3` | `utf8mb3_general_ci` |
| `CHAR(... USING ascii)` | `ascii` | `ascii_general_ci` |
| `UNHEX()` | `binary` | `binary` |
| `HEX()`, `BIN()`, `OCT()`, `CONV()` | current connection charset | current connection collation |
| `QUOTE()` over a binary or all-`NULL` source | current connection charset | current connection collation |
| `QUOTE()` over a numeric source | `latin1` | `latin1_swedish_ci` |

The `ascii` mapping is limited to `CHAR(... USING ascii)` introspection because
`CHAR()` already accepts that charset. It does not add `ascii` to global schema
or column charset support.

## MySQL 8.4.9 probe results

Representative runtime observations:

| SQL shape under `SET NAMES utf8mb4` | `CHARSET()` | `COLLATION()` | `COERCIBILITY()` |
| --- | --- | --- | ---: |
| `'abc'` | `utf8mb4` | `utf8mb4_0900_ai_ci` | 4 |
| `NULL` | `binary` | `binary` | 6 |
| `123`, `12.34`, `12.34E0` | `binary` | `binary` | 5 |
| `CAST('abc' AS CHAR)` | `utf8mb4` | `utf8mb4_0900_ai_ci` | 2 |
| `CAST('abc' AS BINARY)` | `binary` | `binary` | 2 |
| `CHAR(65)` | `binary` | `binary` | 4 |
| `CHAR(65 USING utf8mb4)` | `utf8mb4` | `utf8mb4_0900_ai_ci` | 4 |
| `CHAR(65 USING latin1)` | `latin1` | `latin1_swedish_ci` | 4 |
| `UNHEX('41')` | `binary` | `binary` | 4 |
| `HEX('Az')` | `utf8mb4` | `utf8mb4_0900_ai_ci` | 4 |
| `CONCAT('a','b')`, `QUOTE('a')`, `MAKE_SET(1,'a')` | `utf8mb4` | `utf8mb4_0900_ai_ci` | 4 |
| `QUOTE(NULL)` | `utf8mb4` | `utf8mb4_0900_ai_ci` | 6 |
| `QUOTE(42)` | `latin1` | `latin1_swedish_ci` | 5 |
| `QUOTE(UNHEX('41'))` | `utf8mb4` | `utf8mb4_0900_ai_ci` | 4 |
| `COALESCE(NULL,NULL)` | `binary` | `binary` | 6 |
| nullable `VARCHAR`/`CHAR`/`TEXT` column | declared charset | declared collation | 2 |
| nullable `BINARY`/`VARBINARY`/`BLOB` column | `binary` | `binary` | 2 |

Under `SET NAMES latin1`, ordinary literals and connection-charset string
functions report `latin1` and `latin1_swedish_ci`. Under explicit
`SET NAMES ... COLLATE ...`, literals and connection-charset string functions
report the explicit connection collation.

## Metadata

`CHARSET(expr)` and `COLLATION(expr)` return `VAR_STRING`, nullable metadata,
decimals `31`, and the current connection result collation for the result text.
Observed metadata:

| Session | Type | Charset/collation id | Length | Decimals | Flags |
| --- | --- | ---: | ---: | ---: | --- |
| `SET NAMES utf8mb4` | `VAR_STRING` | `255` | `256` | `31` | none |
| `SET NAMES utf8mb4 COLLATE utf8mb4_bin` | `VAR_STRING` | `46` | `256` | `31` | none |
| `SET NAMES latin1` | `VAR_STRING` | `8` | `64` | `31` | none |

`COERCIBILITY(expr)` returns `LONGLONG`, length `10`, decimals `0`, charset id
`63`, and `NOT_NULL | BINARY | NUM` flags.

## Errors and warnings

The functions do not themselves emit warnings for supported inputs. Warnings
from evaluating nested expressions are still produced by those expressions.
For example, `CHARSET(CHAR('x'))` may inherit existing `CHAR()` conversion
warnings.

Unsupported expressions should return MyLite's normal unsupported-expression
diagnostic for the current statement context. Invalid `CHAR(... USING name)`
continues to use the existing unknown-character-set diagnostic.

## SQL mode

No SQL mode changes are introduced. Strict DML warning promotion remains owned
by the nested expression that produced the warning, not by the introspection
function.

## Storage and runtime impact

The feature is read-only and does not alter `.mylite` file contents. Runtime
introspection derives charset/collation/coercibility from the AST, session
charset/collation state, and existing field descriptors. Expression values do
not gain persistent charset/collation fields in this feature.

## Deferred work

The following remain explicitly deferred:

- full MySQL collation aggregation and coercibility conflict rules
- `_charset` introducer syntax and expression-level `COLLATE`
- charset conversion beyond existing `CHAR(... USING charset)` behavior
- exact native diagnostics for every wrong-arity parse shape
- exact charset/collation propagation for all mixed binary/nonbinary scalar
  expression combinations
- global `ascii` charset/column support beyond `CHAR(... USING ascii)`
