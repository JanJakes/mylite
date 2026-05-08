# Text Comparison Collation

This slice defines the first MyLite runtime behavior for MySQL-compatible text
comparison under the supported charset/collation registry. It covers ordinary
comparison operators, table predicates, SELECT duplicate/sort comparison helpers,
and metadata-backed unique-key conflict detection. Full MySQL collation weight
tables remain out of scope.

## Sources

- MySQL 8.4 character set and collation documentation.
- MySQL 8.4 string comparison and `STRCMP()` documentation.
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`, using `docker exec -i mylite-mysql-849 mysql -uroot`.
- Existing MyLite character set/collation foundation and `STRCMP()` specs.

This specification is independently authored from documentation and runtime
observations. It does not copy MySQL grammar text or implementation sources.

## MySQL 8.4.9 Behavior Summary

With `SET NAMES utf8mb4`, MySQL 8.4.9 reports the connection collation
`utf8mb4_0900_ai_ci`. Focused probes showed:

| Expression | Result |
| --- | --- |
| `'a' = 'A'` | `1` |
| `'a' < 'B'` | `1` |
| `_binary'a' = _binary'A'` | `0` |
| `'a' = _binary'A'` | `0` |
| `'a' = 'a '` | `0` |
| `'a' < 'a '` | `1` |
| `STRCMP('a','A')` | `0` |
| `STRCMP('a','a ')` | `-1` |

For a `VARCHAR(10) UNIQUE` column using `utf8mb4_0900_ai_ci`, MySQL accepts
both `'a'` and `'a '`, but rejects a later `'A'` with duplicate-key error
1062. For a `VARBINARY(10) UNIQUE` column, `X'61'` and `X'41'` are distinct.

For `VARCHAR(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin UNIQUE`, MySQL
treats `'a'` and `'A'` as distinct, but treats `'a'` and `'a '` as duplicates
because `utf8mb4_bin` is a PAD SPACE collation.

Additional probes showed:

| Expression / operation | Result |
| --- | --- |
| `SET NAMES latin1; SELECT 'a' = 'a '` | `1` |
| `SET NAMES latin1; SELECT 'a' < 'a '` | `0` |
| `SET NAMES latin1; SELECT STRCMP('a','a ')` | `0` |
| `'a' COLLATE utf8mb4_bin = 'a ' COLLATE utf8mb4_bin` | `1` |
| `'a' COLLATE utf8mb4_bin = 'A' COLLATE utf8mb4_bin` | `0` |
| `latin1_swedish_ci` unique insert of `'a '` after `'a'` | duplicate error `1062` |
| `utf8mb4_bin` unique insert of `'a '` after `'a'` | duplicate error `1062` |

## Scope

This implementation slice supports ASCII case-insensitive comparison and
PAD SPACE / NO PAD trailing-space behavior for values under the supported
collation registry. It preserves byte-sensitive comparison when either operand
is explicitly binary or a table column descriptor uses a binary
charset/collation.

Covered comparison paths:

- scalar comparison operators that route through the shared expression
  comparison helper
- table-backed `WHERE`, join predicate, DML predicate, and other expression
  contexts using the shared expression helper
- SELECT row comparison helpers used for ordering and duplicate handling
- `INSERT`, `INSERT ... SET`, `INSERT ... SELECT`, `REPLACE`, and ODKU
  unique-key probing through the shared insert unique-conflict path
- single-table and joined `UPDATE` unique-key conflict probing
- existing-row validation for standalone `CREATE UNIQUE INDEX`
- existing-row validation for `ALTER TABLE ... ADD UNIQUE`

Covered column kinds:

- nonbinary character/text types: `CHAR`, `VARCHAR`, `TINYTEXT`, `TEXT`,
  `MEDIUMTEXT`, `LONGTEXT`, `ENUM`, and `SET`
- binary opt-out: `CHARACTER SET binary`, `COLLATE binary`, and collations
  whose names end in `_bin`
- PAD SPACE handling for registered collations whose pad attribute is
  `PAD SPACE`, including `latin1_swedish_ci`, `latin1_bin`,
  `utf8mb3_general_ci`, `utf8mb3_bin`, `utf8mb4_general_ci`,
  `utf8mb4_unicode_ci`, `utf8mb4_unicode_520_ci`, and `utf8mb4_bin`
- NO PAD handling for `binary` and `utf8mb4_0900_ai_ci`

## Semantics

When a comparison is nonbinary, MyLite folds ASCII `A` through `Z` to `a`
through `z` before comparing bytes. Non-ASCII bytes are compared unchanged.

For PAD SPACE collations, MyLite trims trailing ASCII space bytes from both
operands before equality or ordering comparison. This applies to case-insensitive
collations such as `latin1_swedish_ci` and binary collations such as
`utf8mb4_bin`; binary collations still compare remaining bytes case-sensitively.
NO PAD collations such as `utf8mb4_0900_ai_ci` keep trailing spaces
significant.

If either side of a comparison is binary, MyLite compares byte values without
case folding. Binary comparison is selected for:

- binary string literals and results
- `VARBINARY`, `BINARY`, and BLOB-family column descriptors
- text column descriptors whose charset/collation metadata is binary
- explicit `_bin` collations in metadata-backed unique checks

Unique-key conflict checks must use the same effective comparison as predicates.
For nonbinary text columns, generated SQLite probe SQL folds both the stored
column expression and the bound candidate expression with `lower(...)`. Prefix
unique indexes fold after applying `substr(..., 1, prefix_length)`. PAD SPACE
checks additionally wrap the effective value with `rtrim(..., ' ')`, after any
prefix extraction and before any ASCII case folding.

Nullable unique parts retain MySQL behavior: any `NULL` part prevents a unique
conflict for that candidate row.

## Metadata

Table row values loaded into expression evaluation carry enough descriptor
metadata to distinguish binary text from nonbinary text and PAD SPACE from
NO PAD text:

- binary charset id `63`
- `utf8mb4_bin` collation id `46`
- `MYLITE_FIELD_FLAG_BINARY`
- supported collation registry pad attributes by collation id

Nonbinary values may keep charset metadata for introspection, but the comparison
slice currently only implements ASCII byte folding and ASCII trailing-space
trimming, not full Unicode collation weights.

## Diagnostics

This slice does not introduce new diagnostics. Duplicate-key detection continues
to report the existing MyLite duplicate-entry diagnostics, including MySQL error
1062 where that path already records numeric diagnostics.

## Deferred Work

- full Unicode collation weights
- accent, kana, width, and locale-sensitive comparison
- full explicit `COLLATE` coercibility and illegal-mix diagnostics
- coercibility ranking beyond existing metadata/introspection slices
- storage-engine physical indexes that apply MySQL collation ordering directly
- optimizer use of collation-aware index probes

## Tests

Runtime coverage must verify:

- default `utf8mb4_0900_ai_ci` ASCII case-insensitive operators
- default NO PAD trailing-space behavior
- PAD SPACE literal behavior under `SET NAMES latin1`
- explicit `utf8mb4_bin` PAD SPACE behavior
- binary literal and mixed binary/nonbinary comparisons
- table predicates over nonbinary text columns
- binary column predicates
- unique insert and update checks for nonbinary text
- binary collation unique keys accepting case-distinct values
- PAD SPACE unique-key duplicate detection for trailing-space variants
- standalone `CREATE UNIQUE INDEX` existing-row validation
- `ALTER TABLE ... ADD UNIQUE` existing-row validation
- prefix unique indexes using case-insensitive prefix comparison
