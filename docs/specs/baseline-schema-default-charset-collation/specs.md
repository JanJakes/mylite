# Baseline Schema Default Charset And Collation

## Status

This feature specifies the next narrow database-option slice for MyLite
schemas. It makes persistent schema descriptors carry mutable default character
set and collation metadata, and wires that metadata into `CREATE DATABASE`,
`ALTER DATABASE`, `SHOW CREATE DATABASE`, `INFORMATION_SCHEMA.SCHEMATA`,
`@@character_set_database`, `@@collation_database`, and future `CREATE TABLE`
defaults.

The feature is intentionally not full MySQL character-set support. It reuses
MyLite's existing admitted character-set catalog entries, admitted collation
catalog entries, descriptor-driven table charset/collation storage, and
descriptor-driven binary-default inheritance. It does not add conversion,
general collation comparison semantics, client protocol charset negotiation,
server-global charset mutation, or the full MySQL charset catalog.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline schema lifecycle:
  `docs/specs/baseline-schema-lifecycle/specs.md`
- Baseline SHOW CREATE DATABASE:
  `docs/specs/baseline-show-create-database/specs.md`
- Baseline database character-set system variables:
  `docs/specs/baseline-database-character-set-system-variables/specs.md`
- Baseline table charset/collation surface:
  `docs/specs/baseline-table-charset-collation-surface/specs.md`
- Baseline alter table default charset/collation:
  `docs/specs/baseline-alter-table-default-charset-collation/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CREATE DATABASE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-database.html
- MySQL 8.4 Reference Manual, `ALTER DATABASE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-database.html
- MySQL 8.4 Reference Manual, database character set and collation:
  https://dev.mysql.com/doc/refman/8.4/en/charset-database.html
- MySQL 8.4 Reference Manual, `SHOW CREATE DATABASE`:
  https://dev.mysql.com/doc/refman/8.4/en/show-create-database.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.SCHEMATA`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-schemata-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime:

- `CREATE DATABASE db CHARACTER SET utf8mb4` stores `utf8mb4` plus its default
  collation `utf8mb4_0900_ai_ci`.
- `CREATE DATABASE db COLLATE utf8mb4_unicode_ci` infers character set
  `utf8mb4`.
- `CREATE DATABASE db DEFAULT CHARSET = binary` stores character set `binary`
  and renders no explicit `COLLATE binary` in `SHOW CREATE DATABASE`.
- `CREATE DATABASE db DEFAULT CHARSET='utf8mb4' COLLATE='utf8mb4_bin'` accepts
  string-literal option names.
- `ALTER DATABASE db DEFAULT COLLATE utf8mb4_0900_bin` updates the schema
  defaults. Repeating the same `ALTER DATABASE` still reports success.
- `ALTER DATABASE DEFAULT COLLATE utf8mb4_bin` targets the selected default
  database. Without a selected database it fails with `1046 / 3D000`.
- `ALTER SCHEMA` is a synonym for `ALTER DATABASE`.
- `USE db` updates `@@character_set_database` and `@@collation_database`.
  Without a selected database, the variables expose server defaults. With
  `USE information_schema`, they expose `utf8mb3` and `utf8mb3_general_ci`.
- `INFORMATION_SCHEMA.SCHEMATA` returns descriptor default values in
  `DEFAULT_CHARACTER_SET_NAME` and `DEFAULT_COLLATION_NAME`.
- Future `CREATE TABLE` statements without explicit table defaults inherit the
  target schema defaults. An inherited `binary` schema default changes
  unqualified `VARCHAR` columns to `VARBINARY`.
- Repeating the same character-set option succeeds. Conflicting charset
  options, such as `CHARACTER SET utf8mb4 CHARACTER SET binary`, fail with
  `1302 / HY000`.
- Multiple same-family collation options are accepted and the last collation
  wins.
- Unknown charsets fail with `1115 / 42000`; unknown collations fail with
  `1273 / HY000`; collations from a different charset fail with
  `1253 / 42000`.
- `DEFAULT CHARACTER SET DEFAULT` and `DEFAULT COLLATE DEFAULT` are syntax
  errors.
- `CREATE DATABASE IF NOT EXISTS existing ...` succeeds as a no-op, stores a
  `Note 1007`, and does not change the existing schema defaults.
- `ALTER DATABASE missing ...` fails with `3503 / 42Y07` and message
  `Database '<name>' doesn't exist`.
- `ALTER DATABASE information_schema ...` fails with `1044 / 42000`.

The official MySQL 8.4 documentation describes database characteristics as
data-dictionary metadata. It also documents that database defaults become the
default table character set and collation when a `CREATE TABLE` statement omits
explicit table options.

## Scope

The implementation must add:

- schema descriptor storage for default charset and default collation;
- catalog migration from the existing schema-descriptor shape;
- parser and AST support for supported `CREATE DATABASE` options;
- parser and AST support for supported `ALTER DATABASE` / `ALTER SCHEMA`;
- descriptor-driven validation and normalization of admitted schema option
  values;
- descriptor-driven `SHOW CREATE DATABASE` rendering from schema defaults;
- descriptor-driven `INFORMATION_SCHEMA.SCHEMATA` default rows;
- descriptor-driven `@@character_set_database` and `@@collation_database`
  session reads;
- descriptor-driven default inheritance for `CREATE TABLE` and
  `CREATE TABLE ... SELECT`; `CREATE TABLE ... LIKE` continues to clone the
  source table descriptor defaults;
- MySQL-compatible diagnostics for the supported subset;
- fast C tests plus a MySQL 8.4.9 expectation artifact.

The admitted charset/collation catalog for this slice is MyLite's current
limited catalog:

- character sets: `utf8mb4`, `binary`;
- collations: `utf8mb4_0900_ai_ci`, `utf8mb4_0900_bin`,
  `utf8mb4_general_ci`, `utf8mb4_bin`, `utf8mb4_unicode_ci`,
  `utf8mb4_unicode_520_ci`, and `binary`.

## Non-Goals

This feature must not implement:

- schema `ENCRYPTION` mutation or persistent encryption options;
- full MySQL character-set or collation catalogs;
- mutable server-global `character_set_server` or `collation_server`;
- `SET character_set_database`, `SET collation_database`, startup options, or
  persisted system variables;
- collation coercibility, conversion, literal introducers, full Unicode
  comparison/order/group/distinct semantics, or protocol-grade charset
  metadata;
- privileges beyond the existing synthetic `information_schema` access-denied
  behavior;
- filesystem directories, schema files, or SQLite attached databases;
- SQLite fork patches.

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` owns public call validation,
  result ownership, diagnostics, and statement row-count state.
- Statement context owns diagnostics reset, previous diagnostics snapshot,
  affected rows, warning count, and transaction completion.
- Lexer/parser/AST own syntax admission and source spans. They create
  schema-option AST nodes using existing table-option node kinds where the
  syntax and runtime validation are identical, and a dedicated
  `ALTER_DATABASE` statement node for database mutation.
- Analyzer/planner copies identifiers and option names from AST spans, rejects
  reserved names and unsupported grammar, validates option consistency, and
  computes normalized descriptor values.
- Catalog owns `_mylite_catalog_schemas`, descriptor versions, catalog
  generation, descriptor-cache invalidation, and migration. Schema default
  changes are catalog mutations; they do not touch table descriptors or
  physical rows.
- Runtime execution builds `SHOW CREATE DATABASE`,
  `INFORMATION_SCHEMA.SCHEMATA`, system-variable values, and table-create
  defaults from MyLite descriptors. SQLite schema text is not the authority.
- SQLite owns physical row storage. This feature generates no physical SQLite
  SQL except the catalog DDL/migration SQL controlled by the catalog module and
  the existing table-create SQL already generated for `CREATE TABLE`.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload. Catalog
  migration and descriptor changes operate only inside the shifted SQLite
  payload and must not touch the MyLite preamble.

## Supported SQL Grammar

Supported subset:

```sql
CREATE DATABASE [IF NOT EXISTS] schema_name [schema_option] ...
CREATE SCHEMA [IF NOT EXISTS] schema_name [schema_option] ...

ALTER DATABASE [schema_name] [schema_option] ...
ALTER SCHEMA [schema_name] [schema_option] ...

schema_option:
  [DEFAULT] CHARACTER SET [=] option_name
  [DEFAULT] CHARSET [=] option_name
  [DEFAULT] COLLATE [=] option_name

option_name:
  identifier
  string_literal
  BINARY
```

MyLite Lemon-syntax snippet:

```lemon
statement ::= create_schema_statement.
statement ::= alter_schema_statement.

create_schema_statement ::=
    CREATE DATABASE create_schema_if_not_exists_opt identifier schema_option_list_opt.
create_schema_statement ::=
    CREATE SCHEMA create_schema_if_not_exists_opt identifier schema_option_list_opt.

alter_schema_statement ::= ALTER DATABASE alter_schema_name_opt schema_option_list.
alter_schema_statement ::= ALTER SCHEMA alter_schema_name_opt schema_option_list.

alter_schema_name_opt ::= .
alter_schema_name_opt ::= identifier.

schema_option_list_opt ::= .
schema_option_list_opt ::= schema_option_list.
schema_option_list ::= schema_option.
schema_option_list ::= schema_option_list schema_option.

schema_option ::= default_opt CHARSET equal_opt option_name.
schema_option ::= default_opt CHARACTER SET equal_opt option_name.
schema_option ::= default_opt COLLATE equal_opt option_name.
```

The parser must continue to reject unsupported schema forms as syntax errors,
including `ENCRYPTION`, `DEFAULT CHARACTER SET DEFAULT`, `DEFAULT COLLATE
DEFAULT`, qualified schema names, aliases, arbitrary expressions, parameters,
subqueries, and other database options.

## Schema Resolution And Access

`CREATE DATABASE` and `CREATE SCHEMA` create a persistent MyLite schema
descriptor and do not select it. A new descriptor stores normalized
`default_charset` and `default_collation`. With no options, the values are the
server defaults `utf8mb4` and `utf8mb4_0900_ai_ci`.

`CREATE DATABASE IF NOT EXISTS existing ...` follows the existing no-op path:
it validates supported option values and statement-local conflicts, emits a
database-exists note, and leaves the descriptor unchanged.

`ALTER DATABASE schema_name ...` and `ALTER SCHEMA schema_name ...` resolve the
named MyLite schema descriptor and mutate only that descriptor. Unknown schemas
fail with MySQL-compatible `3503 / 42Y07`. Reserved `_mylite_*` names are
rejected before catalog mutation. `information_schema` fails with the existing
access-denied diagnostic.

`ALTER DATABASE ...` with no explicit name targets the selected default schema.
If no schema is selected, it fails with `1046 / 3D000`. If
`information_schema` is selected, it fails with access denied.

Schema-name matching preserves the current MyLite catalog comparison policy.
This slice does not add filesystem-dependent MySQL case folding.

## Option Validation And Normalization

Option names are copied through existing identifier and string-literal decoding
rules. Raw or decoded NUL bytes are rejected with deterministic MyLite
diagnostics. Matching is ASCII case-insensitive.

For each statement, MyLite evaluates option tokens in source order:

- charset option `utf8mb4` selects charset `utf8mb4`;
- charset option `binary` selects charset `binary`;
- collation option `binary` selects charset `binary` and collation `binary`;
- admitted `utf8mb4_*` collation options select charset `utf8mb4` and the
  named collation;
- charset-only statements use the default collation for the selected charset;
- collation-only statements infer the charset from the collation;
- repeated identical charset declarations are accepted;
- conflicting charset declarations fail with `1302 / HY000`;
- repeated compatible collation declarations are accepted and the last
  compatible collation wins;
- a collation incompatible with an explicit charset fails with `1253 / 42000`;
- unknown or unsupported charset names fail with `1115 / 42000`;
- unknown or unsupported collation names fail with `1273 / HY000`.

The explicit MyLite limitation is that MySQL-supported character sets outside
`utf8mb4` and `binary` are rejected using the existing unknown/unsupported
diagnostics until their catalog and conversion semantics are implemented.

## Runtime Semantics

Successful `CREATE DATABASE` and `ALTER DATABASE` return non-row results,
`affected_rows == 1`, and `warning_count == 0` except for the existing
`IF NOT EXISTS` note path. `ALTER DATABASE` reports success even when the
requested defaults are already stored.

`SHOW CREATE DATABASE` renders descriptor defaults:

- `utf8mb4` schemas render
  `/*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE <collation> */`;
- `binary` schemas render
  `/*!40100 DEFAULT CHARACTER SET binary */`;
- encryption remains the fixed MySQL 8.4.9 placeholder
  `/*!80016 DEFAULT ENCRYPTION='N' */`.

`INFORMATION_SCHEMA.SCHEMATA` uses schema descriptor defaults for MyLite
schemas and keeps the synthetic `information_schema` row as `utf8mb3` /
`utf8mb3_general_ci`.

`@@global.character_set_database` and `@@global.collation_database` continue to
expose server-default values. Unscoped, `@@session`, and `@@local` reads expose
the selected schema defaults; if no schema is selected they expose server
defaults; if `information_schema` is selected they expose `utf8mb3` and
`utf8mb3_general_ci`.

`CREATE TABLE schema.t (...)` and unqualified `CREATE TABLE t (...)` inherit
the target schema descriptor defaults when the statement omits explicit table
charset/collation options. Explicit table options override schema defaults.
`CREATE TABLE ... LIKE` clones the source table's descriptor defaults.

Schema default changes do not mutate existing table descriptors, existing
column descriptors, table rows, descriptor caches other than the schema cache,
or physical SQLite schema objects.

## Catalog And File Format

The catalog schema version must increase. `_mylite_catalog_schemas` gains
`default_charset TEXT NOT NULL` and `default_collation TEXT NOT NULL` columns.
Migration from older catalog versions fills existing schemas with
`utf8mb4` / `utf8mb4_0900_ai_ci`.

Catalog APIs must support:

- creating a schema with explicit normalized defaults;
- updating schema defaults inside a mutation;
- reading schema defaults into `struct mylite_catalog_schema_descriptor`;
- iterating schemas with defaults for `SHOW DATABASES` and information schema.

The migration must preserve descriptor ids, names, descriptor versions, catalog
generations, table descriptors, physical names, row storage, and the `.mylite`
preamble. No SQLite fork patch is required.

## Diagnostics

Diagnostics must be deterministic:

- syntax errors and unsupported grammar: `1064 / 42000`;
- no selected schema for unnamed `ALTER DATABASE`: `1046 / 3D000`;
- unknown schema in `ALTER DATABASE`: `3503 / 42Y07`;
- duplicate schema without `IF NOT EXISTS`: existing `1007 / HY000`;
- existing schema with `IF NOT EXISTS`: existing `Note 1007`;
- `information_schema` create/alter/drop targets: existing `1044 / 42000`;
- reserved `_mylite_*` names: existing MyLite reserved-name diagnostic;
- unknown or unsupported charset: `1115 / 42000`;
- unknown or unsupported collation: `1273 / HY000`;
- charset/collation mismatch: `1253 / 42000`;
- conflicting charset declarations: `1302 / HY000`;
- option-name NUL bytes or overlong decoded names: deterministic MyLite
  diagnostics;
- physical SQLite/catalog migration failure: internal error with rollback;
- allocation failure: `HY001`;
- public API misuse: unchanged existing public API misuse behavior.

## Tests

Add fast C coverage for:

- `CREATE DATABASE` / `CREATE SCHEMA` default options and aliases;
- `ALTER DATABASE` / `ALTER SCHEMA` named and selected-schema forms;
- omitted name with no selected schema and selected `information_schema`;
- `SHOW CREATE DATABASE` rendering for default, admitted `utf8mb4` collations,
  and `binary`;
- `INFORMATION_SCHEMA.SCHEMATA` default columns;
- `@@character_set_database` and `@@collation_database` global/session/local
  values across no selection, selected MyLite schema, selected
  `information_schema`, close/reopen, and independent handles;
- future table inheritance and explicit table-option override;
- `CREATE TABLE ... LIKE` descriptor cloning after schema default changes;
- duplicate, conflicting, unknown, unsupported, and mismatched option
  diagnostics;
- `CREATE DATABASE IF NOT EXISTS` no-op preserving existing defaults;
- catalog generation, SQLite schema generation, preamble preservation, and
  zero-initialized cleanup.

Existing lexer, parser, runtime handle, diagnostics, statement context,
schema lifecycle, show-create-database, information-schema, show-variables,
table charset/collation, alter-table-default-charset/collation, file-backed
opening, VFS, catalog migration, and full `check` workflow tests must continue
to pass.
