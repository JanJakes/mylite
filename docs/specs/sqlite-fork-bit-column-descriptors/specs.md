# SQLite Fork BIT Column Descriptors

## Status

Implemented for the first executable SQLite-fork foundation slice. Remaining
work is limited to the deferred compatibility items listed below.

## References

- MySQL 8.4 Reference Manual, Bit-Value Type - BIT:
  https://dev.mysql.com/doc/en/bit-type.html
- MySQL 8.4 Reference Manual, Bit-Value Literals:
  https://dev.mysql.com/doc/refman/8.4/en/bit-value-literals.html
- MySQL 8.4 Reference Manual, Bit Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/bit-functions.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from official MySQL
documentation, observed MySQL 8.4.9 behavior, and the current MyLite codebase.
It does not copy MySQL grammar, documentation prose, or implementation sources.

## Scope

Implement the first executable `BIT` column-storage slice:

- parse `BIT` and `BIT(M)` column declarations;
- default omitted `M` to `1`;
- validate `M` in the MySQL range `1..64`;
- write MySQL-compatible catalog metadata for `DATA_TYPE`, `COLUMN_TYPE`, and
  `NUMERIC_PRECISION`;
- store supported values compactly as SQLite integers through the fork
  descriptor path;
- coerce assignment values at the VDBE record-construction boundary;
- read values back as fixed-width binary strings with a numeric-context side
  channel;
- rehydrate read descriptors from MyLite catalog metadata before public
  table-backed `SELECT` statements after reopen;
- support public MyLite `INSERT`, `UPDATE`, `DELETE`, `TRUNCATE`, and
  `DROP TABLE` CRUD paths for tables containing `BIT` columns.

Out of scope for this slice:

- non-strict clipping and warning demotion;
- exact unsigned 64-bit result rendering for `BIT(64)+0` values above
  `INT64_MAX`;
- direct SQLite parser support for MySQL `BIT` syntax;
- expression planner/index ordering changes beyond the current materialized
  value path.

## MySQL Semantics

`BIT(M)` stores bit values with `M` in `1..64`. `BIT` without a width is
equivalent to `BIT(1)`.

Assignment behavior in strict mode:

- numeric values are rounded to an integer and must fit the unsigned range
  `0..2^M-1`;
- negative numeric values are out of range;
- values greater than `2^M-1` are too long for the column;
- binary-string values are interpreted as big-endian bytes and must represent a
  value that fits the declared width;
- empty binary strings and empty ordinary strings store zero;
- shorter values are left-padded conceptually with zero bits.

Read behavior:

- selected `BIT` values are binary strings;
- `LENGTH(bit_col)` returns the fixed storage byte width
  `ceil(M / 8)`;
- `BIT_LENGTH(bit_col)` returns `8 * ceil(M / 8)`;
- numeric context, such as `bit_col + 0`, returns the stored integer value;
- ordering over a single `BIT(M)` column follows the stored numeric value,
  which is equivalent to fixed-width binary-byte ordering.

## SQLite Fork Design

The existing column-descriptor fork point is the correct integration point.
The public SQLite extension surface cannot implement this transparently because
assignment conversion must run before record assembly, and readback needs one
physical storage value to expose both binary-string display and numeric
context.

Add a `BIT` descriptor kind to `MyliteColumnType` and the public fork descriptor
API. The descriptor stores the declared bit width in the existing precision
field. `OP_MyliteTypeCheck` coerces values before `OP_MakeRecord`, and
`OP_MyliteColumnReadType` converts stored integers into fixed-width binary
strings while preserving the numeric value in the VDBE `Mem` integer slot.

MyLite's materialized SELECT loader treats `BIT` like value-list types for the
purpose of preserving a numeric-context side channel on the expression value.
Public MyLite DML now carries byte lengths for binary string literals so
`b'...'`, `0b...`, and `X'...'` values with embedded zero bytes reach the fork
descriptor intact.

## MyLite SQL Integration

The MyLite parser adds a `bit_column_type` production:

```lemon
column_type ::= bit_column_type.
bit_column_type ::= BIT opt_column_length.
```

The descriptor layer writes:

- `DATA_TYPE = 'bit'`;
- `COLUMN_TYPE = 'bit(M)'`;
- `NUMERIC_PRECISION = M`;
- null character-set, collation, scale, and datetime precision fields.

Physical SQLite tables use `INTEGER` affinity for `BIT` columns. The catalog
loader maps `DATA_TYPE='bit'` back into the fork descriptor for public MyLite
write paths, public table-backed read paths, and `MYSQL_TYPE_BIT` result
metadata.

## Fixture

The MySQL 8.4.9 fixture in
`docs/specs/sqlite-fork-bit-column-descriptors/mysql-bit-column-crud.sql`
captures the supported metadata, assignment, readback, update, delete,
truncate, and drop behavior.

It intentionally keeps numeric readback within signed 63-bit range for the
first MyLite runtime test. `BIT(64)` storage and fixed-width display are
included in direct fork tests, while full unsigned 64-bit expression rendering
is deferred to the expression-value numeric-context extension.

## Compatibility Status

MyLite now has partial executable `BIT` support: parser/catalog integration,
fork assignment checks, fixed-width binary readback, numeric context,
information-schema metadata, CRUD coverage, and SELECT-time descriptor
hydration after reopen are implemented. Non-strict demotion, direct SQLite
parser MySQL syntax, full unsigned 64-bit expression rendering, and broader
optimizer/index interactions remain deferred.
