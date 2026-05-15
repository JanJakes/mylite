# Baseline Column Charset and Collation Attributes

## Status

This feature adds the next narrow DDL metadata slice for MyLite's string
descriptors: column-level `CHARACTER SET` / `CHARSET` and `COLLATE`
attributes for the currently supported `CHAR`, `VARCHAR`, and `TEXT` family
columns.

The slice is metadata-preserving only. It records admitted `utf8mb4` column
charset/collation choices in MyLite descriptors, uses them for
descriptor-driven introspection and result metadata, and preserves them through
the current descriptor-cloning paths. It does not add non-`utf8mb4` storage,
string conversion, full collation comparison/order/group/distinct semantics,
`ENUM` / `SET` collation semantics, or SQLite collation implementations.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Table charset/collation surface:
  `docs/specs/baseline-table-charset-collation-surface/specs.md`
- UTF8MB4 legacy collations:
  `docs/specs/baseline-utf8mb4-legacy-collations/specs.md`
- Character aliases:
  `docs/specs/baseline-character-aliases/specs.md`
- Text descriptors:
  `docs/specs/baseline-text-type/specs.md`
- Result column metadata:
  `docs/specs/baseline-result-column-metadata/specs.md`
- Existing parser, runtime, catalog, temporary table, `CREATE TABLE LIKE`, and
  `CREATE TABLE ... SELECT` implementation under `packages/libmylite/src/`
- SQLite fork layout: `third_party/sqlite/README.md`
- Official MySQL 8.4 Reference Manual, column character set and collation:
  https://dev.mysql.com/doc/refman/8.4/en/charset-column.html
- Official MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- Official MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_column_charset_collation_attributes_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script records the runtime probes that define this phase.
Observed MySQL behavior:

- Character columns accept `CHARACTER SET charset_name`, `CHARSET
  charset_name`, and `COLLATE collation_name` after the type and before
  nullability/default/key attributes.
- Column charset/collation attributes do not accept `=`. Forms such as
  `VARCHAR(10) CHARACTER SET=utf8mb4` and `VARCHAR(10) COLLATE=utf8mb4_bin`
  are syntax errors.
- Attribute names may be unquoted identifiers, backtick-quoted identifiers, or
  string literals under the default SQL mode.
- `CHARACTER SET utf8mb4` without `COLLATE` chooses the default
  `utf8mb4_0900_ai_ci` collation even when the table default collation is a
  different admitted `utf8mb4` collation.
- `COLLATE utf8mb4_bin` without `CHARACTER SET` implies the `utf8mb4` column
  character set.
- If a table default collation is not `utf8mb4_0900_ai_ci`, inherited string
  columns show that effective collation in `SHOW CREATE TABLE` as
  `COLLATE collation_name`.
- If a column has either an explicit charset or an explicit collation,
  `SHOW CREATE TABLE` renders both `CHARACTER SET utf8mb4` and
  `COLLATE effective_collation`, even when they match the table default.
- `SHOW FULL COLUMNS` reports the effective collation for `CHAR`, `VARCHAR`,
  and `TEXT` family columns. Non-character columns report `NULL`.
- `INFORMATION_SCHEMA.COLUMNS` reports the effective
  `CHARACTER_SET_NAME`, `COLLATION_NAME`, and ordinary type text for character
  columns. Non-character columns report `NULL` for charset/collation.
- `CREATE TABLE ... LIKE` preserves column charset/collation attributes and
  supported indexes.
- `CREATE TABLE ... SELECT` infers selected source-column charset/collation
  metadata for descriptor-backed source columns and does not copy indexes.
- `ALTER TABLE ... ADD COLUMN`, `MODIFY COLUMN`, and `CHANGE COLUMN` accept
  the same column charset/collation attributes on character columns.
- MySQL accepts `ENUM` and `SET` column charset/collation attributes, but
  those influence duplicate label/member validation and comparisons. MyLite
  defers them in this slice.
- MySQL accepts shorthand attributes such as `ASCII`, `UNICODE`, and `BINARY`
  on character columns, and accepts `CHARACTER SET binary` / `COLLATE binary`
  by changing the descriptor to a binary string type. MyLite defers those
  descriptor-changing forms in this slice.
- Non-character types reject `CHARACTER SET`. Binary string types reject
  incompatible nonbinary collations with MySQL error `1253`.
- Unknown charsets fail with error `1115`, SQLSTATE `42000`; unknown
  collations fail with error `1273`, SQLSTATE `HY000`; a collation that does
  not belong to the selected charset fails with error `1253`, SQLSTATE
  `42000`.
- Repeated column charset or repeated column collation clauses are syntax
  errors, even if the repeated values match.

## Scope

Supported:

- `CREATE TABLE` and `CREATE TEMPORARY TABLE` explicit column definitions for
  current `CHAR`, `VARCHAR`, and `TEXT` family descriptors;
- `ALTER TABLE ... ADD [COLUMN]` for persistent base tables and current
  `CHAR`, `VARCHAR`, and `TEXT` family descriptors;
- `ALTER TABLE ... MODIFY [COLUMN]` and `CHANGE [COLUMN]` where the current
  replacement-column slice already supports the string descriptor being
  replaced;
- descriptor cloning through `CREATE TABLE ... LIKE`;
- descriptor inference through the current `CREATE TABLE ... SELECT` source
  column subset;
- explicit `CHARACTER SET utf8mb4`, `CHARSET utf8mb4`, and admitted
  `COLLATE` values:
  - `utf8mb4_0900_ai_ci`;
  - `utf8mb4_general_ci`;
  - `utf8mb4_bin`;
  - `utf8mb4_unicode_ci`;
  - `utf8mb4_unicode_520_ci`;
- unquoted identifiers, backtick-quoted identifiers, single-quoted strings,
  and double-quoted strings for admitted names under the existing table-option
  name decoding policy;
- ASCII case-insensitive matching of admitted charset and collation names, with
  canonical lowercase descriptor storage;
- descriptor-driven `SHOW CREATE TABLE`, `SHOW COLUMNS`, `SHOW FULL COLUMNS`,
  `DESCRIBE`, `EXPLAIN table`, limited `INFORMATION_SCHEMA.COLUMNS`, and
  public descriptor-backed result metadata;
- row storage, row DML, indexes, and prefix indexes using the same physical
  SQLite `TEXT` columns and existing MyLite validation/conversion paths;
- warning count `0` and existing non-row result conventions for successful
  supported DDL;
- reopen persistence, independent file-backed handles, temporary descriptor
  cleanup, descriptor-cache invalidation, table rename/drop, and `.mylite`
  preamble preservation through existing storage boundaries.

Deferred:

- non-`utf8mb4` column charsets, including valid MySQL charsets such as
  `latin1`;
- non-admitted `utf8mb4` collations;
- `DEFAULT` as a column charset/collation name;
- `ASCII`, `UNICODE`, and `BINARY` shorthand attributes;
- `CHARACTER SET binary`, `COLLATE binary`, and descriptor-changing charset
  clauses;
- `ENUM` and `SET` column charset/collation attributes;
- national `CHAR` / `VARCHAR` aliases combined with explicit column
  charset/collation attributes;
- binary string, `BIT`, JSON, numeric, temporal, and other non-character
  column charset/collation attributes;
- full collation-aware comparison, ordering, grouping, distinct, duplicate-key,
  `LIKE`, regex, `FIELD`, `GROUP_CONCAT`, aggregate, or expression semantics;
- charset conversion for existing rows, `ALTER TABLE ... CONVERT TO CHARACTER
  SET`, database defaults, mutable server/database charset defaults, and
  protocol negotiation;
- generated columns, views, triggers, privileges, and SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns statement boundaries, result
  ownership, misuse behavior, and cleanup.
- Statement context: owns diagnostics reset, warning count, affected rows, and
  descriptor-cache visibility. Column charset metadata does not require new
  statement state.
- Lexer/parser/AST: admits the narrow column-attribute grammar and preserves
  the source token text for runtime validation. Parser ordering must reject
  `=` forms, duplicated column charset/collation attributes, charset/collation
  attributes after nullability/default/key attributes, and `COLLATE ...`
  followed by `CHARACTER SET ...`.
- Analyzer/planner/runtime: resolves column type, validates that admitted
  charset/collation attributes target supported character descriptors, applies
  canonical effective metadata, and rejects unsupported combinations before any
  catalog row or SQLite SQL is generated.
- Catalog: remains the durable authority for logical types, nullability,
  defaults, visibility, and column charset/collation metadata. This feature
  requires a catalog schema bump for explicit column charset and collation
  descriptor fields. SQLite schema text is never metadata authority.
- Temporary catalog: mirrors the same descriptor fields in session-local
  temporary table descriptors without writing durable catalog rows.
- Result and introspection builders: render effective descriptor metadata into
  MySQL-like `SHOW`, `INFORMATION_SCHEMA`, and public result metadata.
- SQLite physical storage: unchanged. Character values remain stored in
  MyLite-generated SQLite `TEXT` columns. MyLite does not rely on SQLite
  collations for this metadata slice.
- Storage/VFS: unchanged. The feature writes only inside the shifted SQLite
  payload and must not touch the `.mylite` preamble or SQLite fork patch set.

## Supported SQL Grammar

Independently authored MyLite Lemon-syntax snippets:

```lemon
column_definition:
    identifier column_type column_charset_attribute_opt column_collation_attribute_opt
        column_attribute_list_opt

column_charset_attribute_opt:
    /* empty */
  | CHARACTER SET option_name
  | CHARSET option_name

column_collation_attribute_opt:
    /* empty */
  | COLLATE option_name

column_attribute:
    nullability
  | column_default
  | ON UPDATE current_timestamp_value
  | PRIMARY KEY
  | UNIQUE
  | UNIQUE KEY
  | AUTO_INCREMENT
```

The actual parser may keep these nodes in the existing flattened column
definition child list, but the admitted order is semantic: optional column
charset, optional column collation, then existing column attributes. MyLite
must not admit `CHARACTER SET = name`, `CHARSET = name`,
`COLLATE = name`, repeated charset/collation clauses, or `COLLATE name
CHARACTER SET name`.

## Descriptor Semantics

Durable column descriptors gain two optional canonical metadata fields:

- `character_set_name`: nonempty only when the column definition explicitly
  used `CHARACTER SET` or `CHARSET`;
- `collation_name`: nonempty only when the column definition explicitly used
  `COLLATE`.

Effective metadata is derived as follows:

1. National `CHAR` / `VARCHAR` aliases keep their existing `utf8mb3` metadata
   and cannot combine with explicit column charset/collation attributes in this
   slice.
2. If `character_set_name` is nonempty and `collation_name` is nonempty, the
   effective character set is `character_set_name` and the effective collation
   is `collation_name`.
3. If only `character_set_name` is nonempty, the effective character set is
   `utf8mb4` and the effective collation is `utf8mb4_0900_ai_ci`.
4. If only `collation_name` is nonempty, the effective character set is the
   charset associated with that admitted collation, currently always
   `utf8mb4`, and the effective collation is `collation_name`.
5. If neither is explicit, the effective character set/collation come from the
   table descriptor defaults, currently always `utf8mb4` plus the table default
   collation.

Descriptor cloning:

- `CREATE TABLE ... LIKE` copies the optional column charset and collation
  descriptor fields exactly.
- `CREATE TABLE ... SELECT` copies effective source-column explicit metadata
  only for descriptor-backed source columns selected directly. It does not copy
  indexes, matching the current CTAS slice.
- `ALTER TABLE ... MODIFY` / `CHANGE` replaces the optional metadata exactly
  from the replacement column definition. Omitting both attributes makes the
  replacement column inherit the table defaults.

Catalog and cache behavior:

- Creating, replacing, or cloning a descriptor with column charset/collation
  metadata is a normal catalog mutation and increments the catalog generation
  exactly once with the surrounding DDL.
- Row-only DML does not mutate column charset/collation metadata.
- Existing v15 catalogs migrate by adding empty explicit charset/collation
  fields to all columns; effective metadata then remains table-default or
  national-alias driven.

## Rendering and Metadata

`SHOW CREATE TABLE` column rendering:

- Explicit column charset or collation renders as
  `CHARACTER SET utf8mb4 COLLATE effective_collation`.
- Inherited `utf8mb4_0900_ai_ci` renders no column charset/collation clause.
- Inherited non-default admitted table collations render
  `COLLATE table_default_collation` on `CHAR`, `VARCHAR`, and `TEXT` family
  columns.
- National alias rendering remains unchanged:
  `CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci`.
- Existing type, default, nullability, key, engine, and table-option rendering
  remains descriptor-driven.

`SHOW FULL COLUMNS`, `DESCRIBE`, `EXPLAIN table`, and
`INFORMATION_SCHEMA.COLUMNS`:

- `CHAR`, `VARCHAR`, and `TEXT` family columns report effective
  `CHARACTER_SET_NAME` and `COLLATION_NAME`.
- Non-character columns report `NULL` for charset/collation fields.
- `COLUMN_TYPE`, length, default, visibility, nullability, key, and extra
  fields are unchanged except for the effective collation column.

Public result metadata:

- Descriptor-backed direct table columns use the effective column collation ID
  for `charset_id` and `collation_id` when the collation is an admitted
  `utf8mb4` collation.
- National aliases keep the existing `utf8mb3_general_ci` ID.
- Binary strings and non-character descriptors keep existing binary metadata.
- This does not add complete expression-result or protocol metadata fidelity.

## Physical SQLite Handling

The generated SQLite schema remains descriptor-built and stable:

- MyLite user tables remain SQLite rowid tables with generated physical table
  and column names.
- Physical `CHAR`, `VARCHAR`, and `TEXT` family storage remains SQLite `TEXT`.
- No generated SQLite identifier or literal comes from unvalidated user text.
- DML continues to bind row values through prepared statements and existing
  MyLite string validation/conversion.
- Prefix indexes and string unique/primary-key validation continue to use the
  existing fixed ASCII `utf8mb4_0900_ai_ci` subset until a later collation
  semantics slice changes key behavior deliberately.

No SQLite fork patch, SQLite collation registration, trigger, generated column,
or optional SQLite syntax is required for this phase.

## Diagnostics

MyLite should prefer existing MySQL-compatible diagnostics where already
available and otherwise use deterministic MyLite unsupported diagnostics:

- syntax errors:
  - `CHARACTER SET = name`, `CHARSET = name`, `COLLATE = name`;
  - `CHARACTER SET DEFAULT`, `COLLATE DEFAULT`;
  - repeated column charset/collation attributes;
  - column charset/collation attributes after nullability/default/key
    attributes;
  - `COLLATE name CHARACTER SET name`;
- unsupported target descriptor:
  - non-character descriptors;
  - national character aliases with explicit attrs;
  - `ENUM` / `SET` attrs in this slice;
  - binary-changing shorthand attrs;
- unknown charset: MySQL-like `1115 / 42000`, `Unknown character set: 'name'`;
- unknown collation: MySQL-like `1273 / HY000`, `Unknown collation: 'name'`;
- known nonmatching charset/collation pair: MySQL-like `1253 / 42000`;
- decoded option names containing NUL bytes: deterministic unsupported
  diagnostic;
- identifier names exceeding MyLite descriptor capacity: existing identifier
  length diagnostics;
- allocation, catalog, SQLite, and public API misuse failures: existing
  runtime diagnostics.

Successful supported forms produce `warning_count == 0`.

## Tests

Add a focused fast C runtime test, preferably
`runtime_column_charset_collation_attributes_test.c`, and register it with a
dotted CTest name.

Coverage must include:

- parser acceptance for admitted column charset/collation syntax in `CREATE
  TABLE`, `CREATE TEMPORARY TABLE`, `ADD COLUMN`, `MODIFY COLUMN`, and
  `CHANGE COLUMN`;
- parser rejection for `=` forms, duplicate attributes, reversed
  charset/collation ordering, misplaced attributes, and `DEFAULT` names;
- successful `CREATE TABLE` over `CHAR`, `VARCHAR`, and `TEXT` family columns
  with explicit charset-only, collation-only, both attributes, uppercase names,
  quoted names, and table default collations;
- `SHOW CREATE TABLE`, `SHOW FULL COLUMNS`, `SHOW COLUMNS`, and limited
  `INFORMATION_SCHEMA.COLUMNS` metadata for explicit and inherited columns;
- `CREATE TEMPORARY TABLE` metadata and close-time cleanup;
- `CREATE TABLE ... LIKE` cloning of column metadata and supported indexes;
- `CREATE TABLE ... SELECT` descriptor inference for selected source columns;
- `ALTER TABLE ... ADD COLUMN`, `MODIFY COLUMN`, and `CHANGE COLUMN`
  replacement semantics;
- reopen persistence and independent file-backed handles;
- inserted and updated row values remain readable and do not change
  charset/collation metadata;
- table rename/drop behavior;
- `.mylite` preamble preservation;
- public result metadata collation IDs for admitted column collations;
- deterministic unsupported diagnostics for non-character targets, national
  alias combinations, `ENUM` / `SET` attrs, `ASCII`, `UNICODE`, `BINARY`,
  `CHARACTER SET binary`, and `COLLATE binary`;
- MySQL-like diagnostics for unknown charset, unknown collation, and mismatched
  charset/collation;
- zero-initialized cleanup for new planner/catalog/result objects;
- existing lexer, parser, runtime lifecycle, table charset/collation, string
  descriptor, temporary table, result metadata, catalog, storage/VFS, and full
  check workflow coverage.

Run before completion:

1. `cmake --build --preset dev`
2. Focused CTest entries for the new test plus parser, table charset/collation,
   string/text, temporary table, `CREATE TABLE LIKE`, CTAS, and result metadata
   lifecycle tests.
3. `packages/libmylite/tests/mysql_baseline_column_charset_collation_attributes_expectations.sh`
4. `cmake --workflow --preset check`

## Compatibility Documentation

Update only the exact supported subset:

- `COMPATIBILITY.md`
- `docs/compatibility/sql-table-ddl.md`
- `docs/compatibility/character-sets.md`
- `docs/compatibility/collations.md`
- `docs/compatibility/error-warning-result-semantics.md` only if result
  metadata wording needs the narrower effective-column-collation statement.

Do not overclaim full charset/collation support, non-`utf8mb4` charsets,
`ENUM`/`SET` collation semantics, string comparison/order/group/distinct
semantics, collation-aware indexes, database defaults, conversion, protocol
negotiation, or SQLite collation implementations.
