# utf8mb4_unicode_520_ci collation

## Scope

This feature adds `utf8mb4_unicode_520_ci` to MyLite's supported
charset/collation registry and every runtime surface that uses that registry:

- `SET NAMES utf8mb4 COLLATE utf8mb4_unicode_520_ci`
- schema, table, and column charset/collation validation
- `SHOW COLLATION`
- `INFORMATION_SCHEMA.COLLATIONS`
- `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`
- result metadata and `CHARSET()` / `COLLATION()` / `COERCIBILITY()` behavior
  that derives from the connection collation
- existing `STRCMP()` pad and case behavior for supported collations

This feature does not change the default `utf8mb4` collation. It remains
`utf8mb4_0900_ai_ci`, matching MySQL 8.4.9 defaults and existing MyLite
behavior.

This feature also does not implement a full Unicode Collation Algorithm 5.2.0
weight-key engine. MyLite's current executable string-comparison surface is
narrow and already models supported collations through charset membership, pad
attribute, and binary/case-sensitive classification. `utf8mb4_unicode_520_ci`
therefore becomes usable and introspectable everywhere the registry is the
source of truth, while full UCA expansion, contraction, accent, and
supplementary-character ordering remain part of broader collation-comparison
work.

## Compatibility Sources

- MySQL 8.4 Reference Manual, Unicode Character Sets:
  https://dev.mysql.com/doc/refman/8.4/en/charset-unicode-sets.html
- MySQL 8.4 Reference Manual, `SHOW COLLATION` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-collation.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` `COLLATIONS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-collations-table.html
- MySQL 8.4 Reference Manual, Character Set and Collation Compatibility:
  https://dev.mysql.com/doc/refman/8.4/en/charset-collation-compatibility.html
- Runtime observations verified against Docker container
  `mylite-mysql-849-regexp`, MySQL `8.4.9`, using:

```sh
docker exec -e MYSQL_PWD=mylite -i mylite-mysql-849-regexp mysql -uroot --batch --raw --force
docker exec -e MYSQL_PWD=mylite -i mylite-mysql-849-regexp mysql -uroot --column-type-info -vvv --force
```

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar, implementation
sources, or restrictively licensed project code.

## MySQL 8.4.9 Runtime Observations

`utf8mb4_unicode_520_ci` is a Unicode collation for `utf8mb4` based on UCA
5.2.0 weight keys. It is not the default collation for `utf8mb4`; MySQL 8.4.9
keeps `utf8mb4_0900_ai_ci` as the default.

The verified `SHOW COLLATION` and `INFORMATION_SCHEMA.COLLATIONS` row is:

| COLLATION_NAME | CHARACTER_SET_NAME | ID | IS_DEFAULT | IS_COMPILED | SORTLEN | PAD_ATTRIBUTE |
| --- | --- | ---: | --- | --- | ---: | --- |
| `utf8mb4_unicode_520_ci` | `utf8mb4` | 246 | `` | `Yes` | 8 | `PAD SPACE` |

The following behavior was verified:

| SQL | Result |
| --- | --- |
| `SHOW COLLATION LIKE 'utf8mb4\_unicode\_520\_ci'` | One row with collation id `246`, empty default marker, sort length `8`, and `PAD SPACE`. |
| `SELECT ... FROM INFORMATION_SCHEMA.COLLATIONS WHERE COLLATION_NAME='utf8mb4_unicode_520_ci'` | Same row values as `SHOW COLLATION`. |
| `SET NAMES utf8mb4 COLLATE utf8mb4_unicode_520_ci` | Sets all connection character sets to `utf8mb4` and `collation_connection` to `utf8mb4_unicode_520_ci`. |
| `SELECT CHARSET('abc'), COLLATION('abc'), COERCIBILITY('abc')` after the `SET NAMES` statement | Returns `utf8mb4`, `utf8mb4_unicode_520_ci`, and `4`. |
| `CREATE DATABASE ... DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci` | Stores schema defaults as `utf8mb4` / `utf8mb4_unicode_520_ci`. |
| `CREATE TABLE ... VARCHAR(20) COLLATE utf8mb4_unicode_520_ci` | Stores column metadata as `utf8mb4` / `utf8mb4_unicode_520_ci`. |
| `STRCMP('a','A')` after the `SET NAMES` statement | Returns `0`. |
| `STRCMP('a','a ')` after the `SET NAMES` statement | Returns `0`, because this is a `PAD SPACE` collation. |

MySQL 8.4.9 also returns `0` for cases such as `STRCMP('ß','ss')` and
`STRCMP('e','é')` under this collation. Those require full UCA weight-key
comparison and are not claimed by this feature.

Column metadata observed for
`SELECT CHARSET('a') AS cs, COLLATION('a') AS co, COERCIBILITY('a') AS ce`
after `SET NAMES utf8mb4 COLLATE utf8mb4_unicode_520_ci`:

| Column | Type | Collation | Length | Decimals | Flags |
| --- | --- | --- | ---: | ---: | --- |
| `cs` | `VAR_STRING` | `utf8mb4_unicode_520_ci` (`246`) | 256 | 31 | none |
| `co` | `VAR_STRING` | `utf8mb4_unicode_520_ci` (`246`) | 256 | 31 | none |
| `ce` | `LONGLONG` | `binary` (`63`) | 10 | 0 | `NOT_NULL BINARY NUM` |

## Syntax

No new grammar is required. `utf8mb4_unicode_520_ci` is accepted through
existing identifier and string-literal charset/collation value grammar.

Relevant MyLite-owned Lemon surfaces already exist:

```lemon
set_names_statement ::= SET NAMES charset_value opt_set_names_collation.
opt_set_names_collation ::= COLLATE charset_value.

schema_option ::= opt_default CHARACTER SET charset_value.
schema_option ::= opt_default COLLATE charset_value.

column_type_attribute ::= CHARACTER SET charset_value.
column_type_attribute ::= COLLATE charset_value.

table_option ::= opt_default CHARSET charset_value.
table_option ::= opt_default COLLATE charset_value.

show_collation_statement ::= SHOW COLLATION opt_show_collation_filter.
```

## Runtime Semantics

Add one registry entry:

| Collation | Charset | Id | Default | Compiled | Sortlen | Pad_attribute |
| --- | --- | ---: | --- | --- | ---: | --- |
| `utf8mb4_unicode_520_ci` | `utf8mb4` | 246 | `` | `Yes` | 8 | `PAD SPACE` |

The entry must be available through the existing registry iteration and lookup
functions:

- `mylite_collation_count()`
- `mylite_collation_at()`
- `mylite_collation_lookup()`
- `mylite_charset_collation_match()`

Consequences:

- Case-insensitive collation lookup normalizes
  `UTF8MB4_UNICODE_520_CI` to `utf8mb4_unicode_520_ci`.
- `SET NAMES utf8mb4 COLLATE utf8mb4_unicode_520_ci` succeeds.
- `SET NAMES utf8mb3 COLLATE utf8mb4_unicode_520_ci` fails with the existing
  incompatible charset/collation diagnostic.
- Schema, table, and column DDL accept the collation when paired with
  `utf8mb4`, reject it with other character sets, and store normalized names.
- `SHOW COLLATION LIKE 'utf8mb4\_%'` includes the new row in deterministic
  collation-name order.
- `INFORMATION_SCHEMA.COLLATIONS` and
  `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` include the row
  because they share the registry.
- `mylite_expression_descriptor_collation_lookup_id(246)` resolves to this
  collation, so field descriptors and charset/collation introspection functions
  can report it.
- Existing `STRCMP()` option derivation treats it as case-insensitive and
  `PAD SPACE`, because it is not the `binary` collation and does not end in
  `_bin`.

## Storage And Performance

This feature is a static registry expansion. It requires no `.mylite` file
format change, no mutable process-global state, and no new dependency.

The runtime cost is unchanged except for one additional row in the immutable
collation registry scans used by lookup and introspection SQL generation.

## Tests

Runtime tests should verify:

- `SET NAMES utf8mb4 COLLATE utf8mb4_unicode_520_ci` updates connection state.
- Uppercase collation spelling normalizes to `utf8mb4_unicode_520_ci`.
- Incompatible charset/collation pairing is rejected.
- Schema DDL stores `utf8mb4_unicode_520_ci` defaults.
- Table and column DDL store and expose the collation through existing metadata
  and `SHOW FULL COLUMNS`.
- `CHARSET()`, `COLLATION()`, and result metadata report collation id `246`
  after the connection collation is set.
- `STRCMP('a','A')` and `STRCMP('a','a ')` return `0` under the collation.
- `SHOW COLLATION` and `SHOW COLLATION LIKE` include the exact new row values:
  id `246`, empty default marker, compiled `Yes`, sort length `8`, and
  `PAD SPACE`.
- `mylite_column_int64()` returns numeric `246` and `8` for `Id` and
  `Sortlen`.
- `INFORMATION_SCHEMA.COLLATIONS` includes the exact row.
- `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` includes the
  mapping to `utf8mb4`.

## Known Incompatibilities

- MyLite still exposes only the supported registry subset instead of MySQL's
  full collation catalog.
- Full UCA 5.2.0 comparison behavior is not implemented. In particular,
  expansion, contraction, accent-insensitive comparison, and supplementary
  character weight ordering remain deferred to broader string-collation
  runtime work.
- General expression comparison operators still use the current MyLite scalar
  comparison machinery and are not made collation-aware by this registry
  expansion.

## Compatibility Integration Note

Suggested parent compatibility text:

`utf8mb4_unicode_520_ci` is implemented in the supported charset/collation
registry for `utf8mb4`, including id `246`, sort length `8`, `PAD SPACE`
metadata, session/schema/table/column validation, `SHOW COLLATION`,
`INFORMATION_SCHEMA.COLLATIONS`,
`INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`, charset/collation
introspection metadata, and existing `STRCMP()` case/pad behavior. Full UCA
5.2.0 weight-key comparison remains deferred with the broader collation
runtime.
