# Baseline spatial reference system DDL placeholders

This slice accepts MySQL 8.4.9 spatial reference system catalog DDL as an
explicit embedded utility placeholder. MyLite already exposes an empty
`INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS` catalog and supports the
current SRID checks used by spatial functions. It does not yet have a mutable
SRS dictionary, WKT definition parser, dependency tracking, or transformation
engine.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/spatial-reference-systems.html
- https://dev.mysql.com/doc/refman/8.4/en/create-spatial-reference-system.html
- https://dev.mysql.com/doc/refman/8.4/en/drop-spatial-reference-system.html

## MySQL behavior

MySQL stores spatial reference systems in the data dictionary and exposes them
through `INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS`. `CREATE SPATIAL
REFERENCE SYSTEM` accepts an SRID plus SRS attributes such as `NAME`,
`DEFINITION`, `ORGANIZATION ... IDENTIFIED BY ...`, and `DESCRIPTION`; `CREATE
OR REPLACE` and `IF NOT EXISTS` alter duplicate-SRID behavior. `DROP SPATIAL
REFERENCE SYSTEM` removes an SRS by SRID and supports `IF EXISTS`.

Both statements require server privileges and enforce dictionary constraints:
reserved SRID warnings, required attributes, unique names and organizations,
SRID range rules, SRS definition validation, and dependency errors when a table
column depends on the SRID.

## MyLite behavior

MyLite accepts the following statement families after normal grammar parsing
fails:

- `CREATE SPATIAL REFERENCE SYSTEM ...`
- `CREATE OR REPLACE SPATIAL REFERENCE SYSTEM ...`
- `DROP SPATIAL REFERENCE SYSTEM ...`

The accepted placeholder must be one single statement, must have balanced
parentheses, must not end in an obviously incomplete token, and must include
tokens after the `SYSTEM` keyword. Attribute validation is intentionally not
implemented in this slice because the statement has no storage side effect.

Runtime behavior is the existing utility no-op contract:

- return success with no columns and no rows;
- report affected rows `0`;
- set `ROW_COUNT()` to `0`;
- append one warning, code `1105`, SQLSTATE `HY000`, message
  `MyLite accepted this utility statement as an embedded no-op`;
- leave the SRS catalog, table descriptors, spatial function SRID handling,
  user data, and active user transactions unchanged.

This is intentionally not marked MySQL-equivalent SRS DDL support. It is a
compatibility placeholder for applications and schema tools that issue
server-catalog DDL even though MyLite cannot yet mutate an SRS dictionary.

## MyLite grammar snippets

These snippets describe the intended MyLite-owned placeholder surface rather
than copying MySQL grammar.

```text
utility_noop_statement:
    CREATE SPATIAL REFERENCE SYSTEM ...
  | CREATE OR REPLACE SPATIAL REFERENCE SYSTEM ...
  | DROP SPATIAL REFERENCE SYSTEM ...
```

## Tests

- MySQL expectation script verifies that MySQL 8.4.9 accepts a representative
  custom SRS create/drop lifecycle and mutates
  `INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS`.
- Parser tests verify the accepted MyLite placeholder forms map to
  `MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT`.
- Runtime tests verify success shape, warning count, `ROW_COUNT()`, and
  transaction preservation through the utility no-op path.

## Non-goals

- mutable `INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS` rows;
- `mysql.st_spatial_reference_systems` dictionary mutation;
- SRS WKT parsing or validation;
- SRID reserved-range warnings and dependency diagnostics;
- custom SRID support in spatial computations;
- privilege checks, binary logging, or MySQL implicit-commit behavior.
