# Function- and Argument-Specific Result Metadata

## Status

Implemented and release-qualified.

## Summary

MyLite currently assigns one broad result descriptor to several function
families whose members have different MySQL protocol metadata. This phase
replaces those family defaults for the already supported spatial, temporal,
aggregate, grouped-aggregate, and window expressions. It changes result
metadata only; accepted SQL syntax and value semantics remain unchanged.

The result descriptor constructed by the execution layer is authoritative for
the native C API. The mysqli and PDO integrations, prepared statements, and
other consumers must expose that same descriptor rather than deriving a second
function-specific mapping.

## Sources and Evidence

- Existing MyLite metadata designs:
  - `docs/specs/baseline-result-column-metadata/specs.md`
  - `docs/specs/baseline-scalar-result-column-metadata/specs.md`
- Official MySQL 8.4 documentation:
  - C API `MYSQL_FIELD` data:
    <https://dev.mysql.com/doc/c-api/8.4/en/c-api-data-structures.html>
  - aggregate functions:
    <https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html>
  - window functions:
    <https://dev.mysql.com/doc/refman/8.4/en/window-function-descriptions.html>
  - spatial functions:
    <https://dev.mysql.com/doc/refman/8.4/en/spatial-function-reference.html>
  - date and time functions:
    <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Pinned MySQL 8.4.9 observations are captured by
  `packages/libmylite/tests/mysql_function_specific_result_metadata_expectations.sh`.

The specification is independently authored from official documentation,
observed MySQL 8.4.9 behavior, public SQLite interfaces, existing MyLite
documentation, and existing MyLite code. It does not copy implementation
sources or grammar from MySQL, MariaDB, Percona, SQLite, or another database.

## Scope

This phase specifies metadata for already supported:

- spatial scalar functions, including spatial conversion, constructor,
  property, measurement, validity, relation, and minimum-bounding-rectangle
  predicates;
- `CONVERT_TZ()`;
- `COUNT`, `SUM`, `AVG`, `MIN`, `MAX`, `ANY_VALUE`, bitwise, statistical,
  `GROUP_CONCAT`, JSON, and spatial aggregates;
- the same aggregate functions used with `OVER`;
- ranking, distribution, and value-navigation window functions;
- grouped descriptor projections adjacent to aggregate expressions.

The fields covered are label, schema, selected table, origin table, origin
column, protocol type, collation id, display length, decimals, flags,
signedness, and nullability.

This phase does not add unsupported functions, joins, expression kinds, stored
functions, generated columns, wire transport, optional metadata negotiation,
or pre-execution prepared-statement metadata. MyLite materializes a result
before exposing its public metadata, so window expectations use MySQL's
post-execution metadata when MySQL changes a descriptor after executing a
nonempty result.

## Common Metadata Rules

- Function and aggregate expression fields have empty schema, selected-table,
  origin-table, and origin-column strings.
- An explicit alias changes only the result label.
- Binary numeric metadata uses collation id `63`.
- Ordinary character results use the active connection collation and character
  set maximum byte width.
- Nullable expressions do not carry `NOT_NULL`.
- Protocol flags use MySQL bit values. `NUM` and `BINARY` are independent:
  aggregate results normally carry both, while post-execution window numeric
  results carry `NUM` without `BINARY`.
- No corrected metadata path emits a warning.

## Spatial Scalar Results

The supported spatial functions map by the concrete function result:

| Result family | Protocol type | Collation | Display length | Decimals | Flags |
| --- | --- | ---: | ---: | ---: | --- |
| `ST_AsText`, `ST_AsWKT` | `LONG_BLOB` (`251`) | connection | `268435456` at utf8mb4 | `31` | none |
| `ST_AsBinary`, `ST_AsWKB` | `LONG_BLOB` (`251`) | `63` | `4294967295` | `31` | `BINARY` |
| `ST_GeometryType` | `VAR_STRING` | connection | `15 * max_bytes_per_character` | `31` | none |
| `ST_GeoHash` | `VAR_STRING` | connection | `100 * max_bytes_per_character` | `31` | none |
| `ST_AsGeoJSON` | `JSON` | connection | `4294967292` | `31` | `BINARY` |
| geometry-producing scalar | `GEOMETRY` | `63` | `4294967295` | `0` | `BINARY` |
| `ST_Buffer_Strategy` | `VAR_STRING` | `63` | `16` | `31` | `BINARY` |
| dimension/count property | `LONGLONG` | `63` | `10` | `0` | `BINARY NUM` |
| validity/relation predicate | `LONGLONG` | `63` | `1` | `0` | `BINARY NUM` |
| measurement/coordinate | `DOUBLE` | `63` | `23` | `31` | `BINARY NUM` |

The dimension/count group includes `ST_SRID`, `ST_Dimension`,
`ST_NumGeometries`, and `ST_NumPoints`. The predicate group includes current
`ST_Is*`, shape-relation, and `MBR*` boolean functions. These integer results
are signed. Spatial scalar results are nullable unless an existing
function-specific rule has independently established `NOT_NULL`.

Geometry constructors and geometry-valued operators carry `BINARY`, but not
the `BLOB` flag. `ST_AsText()` and `ST_AsBinary()` use `LONG_BLOB`, requiring
the public type-id surface to expose protocol ids `249`, `250`, and `251` in
addition to the existing `BLOB` id `252`.

## `CONVERT_TZ()` Result

`CONVERT_TZ()` reports `DATETIME`, binary collation `63`, `BINARY`, and
nullable. Its decimals and display length follow the first argument:

- date, `DATETIME(0)`, `TIMESTAMP(0)`, a whole-second temporal literal, or
  `NULL`: decimals `0`, length `19`;
- `DATETIME(fsp)` or `TIMESTAMP(fsp)`: decimals `fsp`, length `20 + fsp`;
- a temporal literal with fractional digits: decimals equal the admitted
  fractional digit count, length `20 + fsp`;
- a character or text descriptor whose runtime temporal precision is not known
  at planning time: decimals `6`, length `26`.

The existing value-level boundary and time-zone rules remain unchanged.

## Aggregate Results

### Numeric aggregates

`COUNT()` reports signed `LONGLONG`, length `21`, decimals `0`,
`NOT_NULL BINARY NUM`.

For exact integer arguments, `SUM()` and `AVG()` report `NEWDECIMAL`,
`BINARY NUM`, and nullable. Let `p` be the argument's integer decimal
precision: `3`, `5`, `8`, `10`, `19`, or `20` for the supported tiny, small,
medium, signed int, signed bigint, and unsigned bigint shapes.

- `SUM`: length `p + 23`, decimals `0`;
- `AVG`: length `p + 6`, decimals `4`.

The existing result rules for exact decimal and approximate arguments remain
argument-specific: exact inputs produce `NEWDECIMAL`; approximate inputs
produce `DOUBLE`. Metadata precision must agree with the value evaluator's
chosen result family.

`BIT_AND`, `BIT_OR`, and `BIT_XOR` report unsigned `LONGLONG`, length `21`,
decimals `0`, and `NOT_NULL UNSIGNED BINARY NUM`. Supported variance and
standard-deviation aggregates report nullable `DOUBLE`, length `23`, decimals
`31`, and `BINARY NUM`.

### Value-preserving aggregates

`MIN()`, `MAX()`, and `ANY_VALUE()` preserve the argument protocol type,
collation, display length, decimals, and unsigned bit. Their expression result
has empty origin fields and is nullable, so source `NOT_NULL`, key, default,
and part-key flags do not propagate. Numeric arguments keep `BINARY NUM`;
ordinary character arguments have no numeric or binary flag.

### String, JSON, and spatial aggregates

`GROUP_CONCAT()` uses the active connection collation and is nullable.

- If `group_concat_max_len <= 512`, it reports `VAR_STRING` with display length
  `group_concat_max_len * max_bytes_per_character`.
- If `group_concat_max_len > 512`, it reports `LONG_BLOB`, decimals `31`, and
  display length
  `group_concat_max_len * 16 * max_bytes_per_character`, capped at the public
  metadata length limit.
- A binary argument makes the result binary and selects the equivalent binary
  collation and flag.

`JSON_ARRAYAGG()` and `JSON_OBJECTAGG()` report nullable `JSON`, active
connection collation, length `4294967292`, decimals `31`, and `BINARY`.

`ST_Collect()` reports nullable `GEOMETRY`, binary collation `63`, length
`16777216`, decimals `31`, and `BINARY` without `BLOB`.

### Grouped projections

A descriptor column projected as a group key retains the same schema, selected
table or alias, origin table, origin column, protocol type, collation, length,
decimals, signedness, nullability, and catalog flags as a non-grouped
descriptor projection. Aggregate expressions alongside it follow the
expression rules above and have empty origin fields.

## Window Results

For MyLite's post-execution result descriptor:

- `ROW_NUMBER`, `RANK`, `DENSE_RANK`, and `NTILE` report unsigned
  `LONGLONG`, length `21`, decimals `0`, `NOT_NULL UNSIGNED NUM`;
- `PERCENT_RANK` and `CUME_DIST` report `DOUBLE`, length `23`, decimals `31`,
  `NOT_NULL NUM`;
- window `COUNT` reports signed `LONGLONG`, length `21`, decimals `0`,
  `NOT_NULL NUM`;
- window `SUM` and `AVG` use the aggregate type, length, and decimal rules but
  carry `NUM` without `BINARY`;
- window `MIN`, `MAX`, navigation functions, and value functions preserve the
  argument type family, collation, display length, decimals, and unsigned bit,
  clear origin and source constraint/key flags, and are nullable;
- window bitwise aggregates report unsigned `LONGLONG`, length `21`, decimals
  `0`, `NOT_NULL UNSIGNED NUM`;
- window statistical aggregates report nullable `DOUBLE`, length `23`,
  decimals `31`, and `NUM`;
- window JSON aggregates report nullable `JSON`, binary collation `63`, length
  `4294967295`, decimals `0`, and `BLOB BINARY`.

MySQL may expose a source-like descriptor before execution and a promoted
descriptor after executing a nonempty window result. Native MyLite, mysqli, and
PDO must agree on the single post-execution descriptor that MyLite owns.

## Architecture and Ownership

- Parser and AST: unchanged.
- Planner: retains the aggregate or window function kind and its argument
  expression. Descriptor arguments are resolved through the existing catalog
  and selected-table context.
- Result builder: one shared metadata mapper constructs a
  `mylite_result_column_descriptor` from the function kind, argument
  descriptor, connection collation, and relevant session limit.
- Native C API: continues to expose the stored result descriptor through the
  existing accessors.
- mysqli: consumes public type ids and flags without its own function-family
  inference.
- PDO: maps the expanded protocol type ids to stable native type names and PDO
  string/integer parameter categories; it does not infer lengths or precision.
- Prepared execution: reuses the result descriptor produced by ordinary
  execution.
- `SHOW` and other synthetic results: unchanged, but continue to use the same
  descriptor representation and accessors.
- Catalog, schema generation, SQLite physical SQL, storage, VFS, and the
  `.mylite` file format: unchanged.

The public enum gains named constants for MySQL protocol ids `TINY_BLOB`
(`249`), `MEDIUM_BLOB` (`250`), and `LONG_BLOB` (`251`). This is an additive
source-level change and adds no symbol or structure field.

## Diagnostics and Failure Handling

Metadata inference adds no SQL warning or error. Unsupported expressions retain
the existing unknown descriptor until separately specified.

Descriptor lookup or result-column allocation failure uses the existing
statement error and cleanup path. A partially constructed result is not
published. Metadata inference never evaluates the function a second time and
does not inspect unbounded row values.

## Performance

All inference is performed once per result column. Function-kind mapping is
constant time. Argument-specific mapping reuses planned descriptor information
and current session state; it does not scan rows, parse geometry/JSON payloads,
or add SQLite work. The change adds no dependency and no persistent data.

## Test Plan

Pinned MySQL expectations cover:

- representative spatial text, binary, JSON, geometry, string, integer,
  predicate, and double results;
- `CONVERT_TZ()` literal and descriptor fractional precision;
- aggregate integer precision formulas, value-preserving descriptors,
  bitwise/statistical, `GROUP_CONCAT` threshold, JSON, spatial, and grouped
  origin metadata;
- nonempty ranking, distribution, numeric aggregate, value-navigation, string,
  and JSON window results;
- direct and post-execution prepared mysqli parity;
- PDO direct and native-prepared type, length, precision, flag, and label
  observations.

Native tests must assert every public metadata accessor. Existing spatial,
temporal, aggregate/window, result metadata, statement, mysqli, and PDO suites
must remain green. Release qualification additionally runs the ABI, formatting,
static-analysis, sanitizer, install, package-config, compatibility-ledger, and
production-size gates.

## Qualification

The independently authored specification and pinned MySQL 8.4.9 fixture landed
at `c71e670f2`. Spatial and `CONVERT_TZ()` descriptors landed at `6463c151e`;
aggregate, grouped, and window descriptors landed at `a6511e120`; PDO now
expects typed aggregate values at `da9d43748`; and the additive public type ids
were reviewed against the ABI-0 contract at `2e00f221d`.

Release qualification covered:

- all 706 Development tests, including fault, crash, concurrency, install
  consumer, and pkg-config coverage;
- the 16 affected aggregate, window, spatial, and result-metadata suites in
  Debug, Release, and ASan/UBSan builds, with leak detection enabled for the
  sanitizer run;
- all 53 governed MySQL fixtures selected for the affected spatial, aggregate,
  window, scalar-result, metadata-connection, result-metadata, and
  `CONVERT_TZ()` surfaces, including the exact field-metadata matrix in this
  specification's pinned fixture;
- all 16 PHP core, mysqli, and PDO tests;
- Doctrine DBAL 4.4.3 with two tests and 22 assertions, and Doctrine ORM 3.6.7
  with one test and eight assertions, on PHP 8.4.23;
- LLVM 19 static analysis across all 931 first-party translation units,
  first-party formatting, whitespace, compatibility-ledger, and exact
  shared-library symbol/header checks;
- a 12,386,856-byte native production archive against the 15,000,000-byte
  ceiling, plus PHP-production artifacts of 12,382,104 bytes for the archive,
  8,945,320 bytes for `mylite.so`, 242,800 bytes for `mysqli.so`, and 33,488
  bytes for `pdo_mylite.so`, all below their configured ceilings.

Self-review confirmed that each corrected descriptor agrees with the existing
value family and is shared by native, mysqli, PDO, and prepared execution.
The change adds no dependency, persistent state, catalog or file-format
mutation, SQLite-fork patch, row scan, or second function evaluation. Broader
joined, `SHOW`, and synthetic result metadata remains outside this claim.
