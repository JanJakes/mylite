# Baseline ALTER TABLE ADD UNIQUE Lifecycle

## Summary

This phase adds the next focused index/constraint DDL building block for
persistent MyLite base tables:

```sql
ALTER TABLE table_name ADD UNIQUE [INDEX|KEY] [index_name] (column_name)
```

The supported form creates one descriptor-owned single-column unique secondary
index after table creation, validates existing row values before mutating the
catalog, creates one generated SQLite unique index, and exposes the new key
through the existing descriptor-driven DML, `SHOW`, `CREATE TABLE ... LIKE`,
and limited `INFORMATION_SCHEMA` paths.

This is intentionally not full MySQL unique-constraint DDL. It does not add
named `CONSTRAINT` syntax, composite or prefix keys, descending or functional
key parts, index options, multiple alter actions, foreign-key dependency
handling, or optimizer guarantees.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Baseline unique index lifecycle:
  `docs/specs/baseline-unique-index-lifecycle/specs.md`
- Baseline `ALTER TABLE ... ADD INDEX` lifecycle:
  `docs/specs/baseline-alter-table-add-index-lifecycle/specs.md`
- Baseline `CREATE INDEX` lifecycle:
  `docs/specs/baseline-create-index-lifecycle/specs.md`
- Baseline `DROP INDEX` lifecycle:
  `docs/specs/baseline-drop-index-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, `CREATE INDEX` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-index.html
- MySQL 8.4 Reference Manual, `SHOW INDEX` statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-index.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-constraints-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-key-column-usage-table.html
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_alter_table_add_unique_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or other restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `ALTER TABLE t ADD UNIQUE u_v (v)`,
  `ALTER TABLE t ADD UNIQUE KEY u_v (v)`, and
  `ALTER TABLE t ADD UNIQUE INDEX u_v (v)` succeed for the tested InnoDB
  tables, report `ROW_COUNT() == 0`, and leave `@@warning_count == 0`.
- Omitting the unique-index name derives a name from the key column. If that
  name collides with an existing index on a different column, MySQL appends
  `_2` in the tested subset and reports no warnings. Redundant duplicate-index
  warning behavior for equivalent keys remains deferred.
- `SHOW CREATE TABLE` renders added unique secondary indexes as
  `UNIQUE KEY`. `SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` report
  `NON_UNIQUE = 0`. `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` and
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` expose one unique constraint named after
  the index.
- Integer-family, exact decimal, canonical temporal, `CHAR(1..255)`, and
  `VARCHAR(1..255)` columns can be used as one-part unique keys for this
  subset. `TEXT` without a prefix fails with `1170 / 42000`, and
  `CHAR(0)` / `VARCHAR(0)` fail with `1167 / 42000`.
- Existing duplicate non-`NULL` values fail with `1062 / 23000`; duplicate
  `NULL` values are permitted.
- For the default `utf8mb4_0900_ai_ci` collation, `VARCHAR` unique keys are
  case-insensitive, `CHAR` values compare after stored `CHAR` canonicalization,
  and `VARCHAR` trailing spaces remain distinct for the current no-pad
  collation behavior.
- A duplicate explicit index name fails with `1061 / 42000`, and this
  duplicate-name check outranks missing key-column diagnostics in the tested
  subset.
- A quoted index name `PRIMARY` fails with `1280 / 42000`; unquoted
  `PRIMARY` in the index-name position is a syntax error.
- An unknown key column fails with `1072 / 42000`.
- Missing default schema, unknown schema, and unknown table use the existing
  MySQL diagnostics for `ALTER TABLE` target resolution.
- MySQL accepts wider forms such as `ADD CONSTRAINT ... UNIQUE`, multiple alter
  actions, composite key parts, prefix lengths, descending parts, and index
  options. They remain deferred here.

## Scope

Supported:

- persistent MyLite base tables only;
- one `ALTER TABLE table_name ADD UNIQUE [index_name] (column_name)` action;
- one `ALTER TABLE table_name ADD UNIQUE KEY [index_name] (column_name)`
  action;
- one `ALTER TABLE table_name ADD UNIQUE INDEX [index_name] (column_name)`
  action;
- unqualified and schema-qualified target table names using the existing
  selected/default schema policy;
- optional explicit index names using the existing descriptor identifier
  policy;
- omitted index names generated from the key column with MySQL-compatible
  `_N` suffixes for non-redundant name collisions in the admitted subset;
- exactly one unqualified descriptor column in the key-part list;
- supported unique target descriptors:
  - integer-family and integer aliases, including `BOOL` / `BOOLEAN`;
  - exact `DECIMAL` / `NUMERIC` / `FIXED`;
  - canonical `DATE`, `DATETIME`, and `TIMESTAMP`;
  - ASCII-valued `CHAR(1..255)` and `VARCHAR(1..255)`;
- nullable and `NOT NULL` key target columns;
- empty and nonempty tables, with existing-row duplicate validation;
- duplicate `NULL` values;
- descriptor-backed `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`,
  `CREATE TABLE ... LIKE`, DML duplicate checks,
  `ALTER TABLE ... DROP INDEX|KEY`, standalone `DROP INDEX`, and limited
  `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`, and
  `KEY_COLUMN_USAGE` after the add;
- row-value preservation, reopen persistence, independent file-backed handles,
  and `.mylite` preamble preservation;
- no-result DDL result shape with `affected_rows == 0` and
  `warning_count == 0` for supported successful forms.

Deferred:

- `ADD CONSTRAINT [name] UNIQUE ...` and separate constraint-name storage;
- `ADD PRIMARY KEY`, `ADD INDEX` / `ADD KEY` nonunique behavior, `ADD FULLTEXT`,
  `ADD SPATIAL`, `ADD FOREIGN KEY`, and check constraints beyond their existing
  feature slices;
- `DROP INDEX` / `DROP KEY`, `RENAME INDEX` / `RENAME KEY`, and index
  visibility changes beyond already supported drop forms;
- multi-action `ALTER TABLE`;
- multiple key parts, duplicate key parts, prefix lengths, descending key
  parts, functional key parts, expression key parts, table-qualified key parts,
  ordinal key parts, and string-literal key parts;
- index type clauses, comments, parser options, `KEY_BLOCK_SIZE`, visibility,
  engine attributes, algorithms, locks, partitions, temporary tables, views,
  foreign keys, cascades, triggers, privileges, and implicit-commit emulation;
- `TEXT` family key parts until prefix-length semantics are implemented;
- non-ASCII string key values and full collation-aware comparison;
- warnings for redundant duplicate unique indexes that MySQL accepts but marks
  with a warning;
- optimizer/index-use guarantees beyond creating a physical SQLite unique index
  that SQLite may use.

## Ownership Boundaries

- Public API: no new public ABI. Callers continue to use `mylite_execute()` and
  existing result/diagnostic accessors.
- Statement context: owns diagnostics reset, warning count, affected rows,
  `ROW_COUNT()`, and statement cleanup.
- Lexer/parser/AST: owns syntax admission for the narrow `ALTER TABLE` action
  and preserves index name, optional keyword spelling, and key-column nodes
  without depending on runtime or SQLite.
- Analyzer/planner/runtime: resolves schema, table, index names, and key
  columns against MyLite descriptors before generating any SQLite SQL.
- Catalog: MyLite index descriptors and index-column descriptors are
  authoritative for logical metadata. SQLite schema text is not used to infer
  logical indexes or constraints.
- Result builder/introspection: existing `SHOW` and `INFORMATION_SCHEMA`
  surfaces render from descriptors, including unique constraint rows.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.
- SQLite physical storage: MyLite creates one ordinary SQLite unique index
  using stable generated physical table, index, and column identifiers. No
  SQLite fork patch is required.

## Supported SQL Grammar

MyLite admits only one single-table unique-index action:

```sql
ALTER TABLE table_name ADD UNIQUE [index_name] (column_name)
ALTER TABLE table_name ADD UNIQUE KEY [index_name] (column_name)
ALTER TABLE table_name ADD UNIQUE INDEX [index_name] (column_name)
```

The target table may be unqualified or schema-qualified. The index name, when
present, is one identifier or quoted identifier. The key column is one
unqualified identifier or quoted identifier.

MyLite Lemon-syntax sketch:

```lemon
statement(A) ::= alter_table_add_index_statement(B). {
    A = B;
}

alter_table_add_index_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD unique_index_definition(I). {
    A = mylite_sql_parser_make_alter_table_add_index_statement(
        state, A1, T, I);
}

unique_index_definition(A) ::=
    UNIQUE(U) unique_index_keyword_opt index_name_opt(N)
    LPAREN secondary_index_part_list(L) RPAREN(R). {
    A = mylite_sql_parser_make_unique_index_definition(state, U, N, L, R);
}

unique_index_keyword_opt ::= .
unique_index_keyword_opt ::= KEY.
unique_index_keyword_opt ::= INDEX.

index_name_opt ::= .
index_name_opt ::= identifier.

secondary_index_part_list ::= secondary_index_part.
secondary_index_part ::= identifier.
```

The implementation may share the existing `ALTER TABLE ... ADD INDEX`
statement node. Runtime planning must distinguish secondary nonunique and
unique index definition children and set the planned index uniqueness from the
child node kind.

## Resolution Semantics

Target table resolution follows the existing policy:

- an unqualified table requires the selected/default schema;
- a schema-qualified table uses the explicit schema and does not require a
  selected schema;
- unknown schemas fail with MySQL-compatible `1049 / 42000`;
- unknown tables fail with MySQL-compatible `1146 / 42S02`;
- reserved `_mylite_*` schema or table names are rejected before physical SQL
  is generated.

The target object must be a persistent base table. Future non-base descriptors
must fail with the matching MySQL diagnostic for this supported surface.

Index name resolution is table-local:

- explicit names must not collide case-insensitively with any existing primary,
  unique, or nonunique index descriptor;
- duplicate explicit names fail with `1061 / 42000`;
- quoted `PRIMARY` fails with `1280 / 42000`;
- unquoted `PRIMARY` remains a syntax error in the admitted grammar;
- omitted names derive from the target column name and append `_2`, `_3`, ...
  until no descriptor name collides in the table-local index namespace. This
  phase covers non-redundant name collisions, not MySQL's warning-producing
  duplicate equivalent-index case.

Column resolution is descriptor-owned:

- the key column must be an existing unqualified descriptor column;
- unknown columns fail with `1072 / 42000`;
- supported invisible columns may be explicitly indexed because existing
  descriptor DML and metadata paths already allow explicit references;
- `TEXT` family columns fail with the MySQL `1170 / 42000` no-prefix
  diagnostic;
- zero-length `CHAR(0)` and `VARCHAR(0)` columns fail with
  `1167 / 42000`;
- all other unsupported descriptor types fail with a deterministic MyLite
  unsupported diagnostic until their MySQL-compatible key semantics are
  specified.

Current descriptor identifier matching remains case-insensitive ASCII matching,
consistent with the existing catalog foundation and index lifecycle slices.

## Existing-Row Validation

Adding a unique index to a nonempty table validates existing physical rows
before catalog descriptors are inserted and before a SQLite unique index is
created:

- `NULL` key values are ignored for duplicate detection and multiple `NULL`
  values are allowed;
- non-`NULL` duplicate key values fail with `1062 / 23000` using the target
  table name, resolved unique-index name, and formatted duplicate key tuple;
- supported string key values must be ASCII so MyLite's current
  `utf8mb4_0900_ai_ci` compatibility collation remains deterministic;
- non-ASCII values in supported string key columns fail with the existing
  MyLite string-key diagnostic;
- `CHAR` duplicate validation uses stored `CHAR` canonicalization, and
  `VARCHAR` validation preserves no-pad trailing-space behavior in the current
  ASCII subset.

Validation must use descriptor-built SQLite `SELECT` statements with quoted
physical table and column identifiers. It must not query SQLite metadata or
rely on SQLite's diagnostic message as the compatibility result.

## Physical SQLite Handling

The supported action is implemented as MyLite wrapper/translation code using
public SQLite SQL execution against the shifted SQLite payload. No SQLite fork
hook is needed.

Execution shape:

1. Resolve table, column, and index names from MyLite descriptors.
2. For unique indexes, choose the existing rowid alias required by MyLite's
   duplicate-validation query shape.
3. Validate existing string values and duplicate non-`NULL` values before
   beginning the catalog mutation that inserts the new index rows.
4. Begin a MyLite catalog mutation.
5. Allocate an index id and derive `_mylite_user_index_<index_id>`.
6. Insert one secondary index descriptor with `is_unique = true`.
7. Insert one index-column descriptor with `seq_in_index = 1`.
8. Execute one generated SQLite statement:

   ```sql
   CREATE UNIQUE INDEX "_mylite_user_index_<index_id>"
   ON "_mylite_user_table_<table_id>"("_mylite_user_column_<column_id>" COLLATE mylite_utf8mb4_0900_ai_ci)
   ```

   The collation annotation appears only for descriptor string key parts.
9. Update table identity/schema generation metadata using the existing index
   lifecycle path and commit the catalog mutation.
10. Increment only the session SQLite schema generation after a successful
    physical schema change.

Every generated SQLite identifier must be double-quoted through existing
helpers. There are no SQL literal values to interpolate in the physical
`CREATE UNIQUE INDEX` statement. Validation statements use descriptor-built
identifiers and no caller-supplied literal interpolation.

## Result Semantics

A successful supported `ALTER TABLE ... ADD UNIQUE`:

- returns the existing public non-row statement result shape;
- reports `affected_rows == 0`;
- sets `warning_count == 0`;
- produces no result rows;
- preserves table rows, column descriptors, primary-key descriptors, other
  secondary indexes, descriptor generations except the expected table/index
  metadata generation updates, and the `.mylite` preamble.

After success, existing descriptor-driven DML must enforce the added unique
index for current `INSERT ... VALUES`, `INSERT ... SET`, `INSERT IGNORE`, and
single-assignment `UPDATE` paths. `DROP INDEX` / `DROP KEY` must be able to
remove the added unique secondary index through the already supported drop
paths.

## Diagnostics

Diagnostics for the supported surface:

- syntax errors and unsupported grammar: existing parser syntax diagnostics or
  deterministic MyLite unsupported diagnostics;
- missing default schema: `1046 / 3D000`;
- unknown schema: `1049 / 42000`;
- unknown target table: `1146 / 42S02`;
- reserved `_mylite_*` schema/table target names: existing reserved-name
  diagnostics before physical SQL generation;
- unsupported object kind: deterministic unsupported diagnostic;
- duplicate explicit index name: `1061 / 42000`;
- explicit quoted `PRIMARY` unique-index name: `1280 / 42000`;
- unknown key column: `1072 / 42000`;
- `TEXT` family key column without prefix: `1170 / 42000`;
- zero-length `CHAR(0)` / `VARCHAR(0)` key column: `1167 / 42000`;
- unsupported key column type: deterministic MyLite unsupported diagnostic;
- duplicate existing non-`NULL` values: `1062 / 23000`;
- unsupported string key value: existing MyLite string-key diagnostic;
- allocation failure: `MYLITE_NOMEM` plus handle-owned diagnostic;
- physical SQLite failure: MyLite internal/SQLite failure diagnostic without
  leaking generated SQL as user-visible contract;
- public API misuse: unchanged existing public execution/result misuse
  behavior.

Unsupported but MySQL-accepted forms remain intentionally deferred and should
be rejected deterministically when parsed, or remain syntax errors when not
admitted by the parser:

- `ADD CONSTRAINT [name] UNIQUE ...`;
- multiple alter actions;
- composite, prefix, descending, functional, expression, ordinal, or
  table-qualified key parts;
- `USING`, comments, visibility, `KEY_BLOCK_SIZE`, algorithms, locks,
  partitions, fulltext/spatial indexes, foreign keys, check constraints,
  temporary tables, views, and query modifiers.

## Metadata and Interactions

Descriptor-owned metadata must update all existing surfaces that already read
index descriptors:

- `SHOW CREATE TABLE` renders added unique secondary indexes as `UNIQUE KEY`;
- `SHOW INDEX` renders `Non_unique = 0`;
- `SHOW COLUMNS` renders `UNI` for columns that are only unique and `PRI` for
  current primary-key columns;
- `INFORMATION_SCHEMA.STATISTICS` exposes one row with `NON_UNIQUE = 0`;
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` exposes a `UNIQUE` constraint named
  after the index;
- `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` exposes the one key column with
  ordinal position `1`;
- `CREATE TABLE ... LIKE` clones the added unique descriptor and resets any
  cloned auto-increment counter per the existing clone policy;
- `CREATE TABLE ... SELECT` omits indexes;
- table rename, schema-qualified references, reopen, independent handles, and
  table drop follow existing descriptor authority;
- `ALTER TABLE ... DROP INDEX|KEY` and standalone `DROP INDEX` can remove the
  added unique secondary index unless the existing auto-increment key policy
  rejects the drop.

## Performance and Storage Notes

The feature stays on the same efficient path as existing index DDL:

- existing-row validation runs one or two SQLite aggregate scans only for
  unique indexes and only before the index is added;
- row data is not materialized into MyLite memory for duplicate detection;
- after the physical unique index exists, ongoing DML relies on descriptor
  metadata and generated SQLite-backed checks from existing paths;
- no `.mylite` file-format version bump or preamble change is required;
- no new dependency or SQLite fork patch is required.

## Tests

Add a fast C runtime test, preferably
`packages/libmylite/tests/runtime_alter_table_add_unique_test.c`, registered as
`libmylite.runtime.alter_table_add_unique`.

Coverage must include:

- successful `ADD UNIQUE`, `ADD UNIQUE KEY`, `ADD UNIQUE INDEX`, and omitted
  name forms;
- integer-family, `BIGINT UNSIGNED`, exact `DECIMAL`, `DATE`, `DATETIME`,
  `TIMESTAMP`, `CHAR`, and `VARCHAR` supported target descriptors;
- metadata through `SHOW CREATE TABLE`, `SHOW COLUMNS`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`, and
  `KEY_COLUMN_USAGE`;
- existing-row duplicate rejection, duplicate `NULL` acceptance, ASCII
  `CHAR`/`VARCHAR` duplicate behavior, and later DML duplicate enforcement;
- schema-qualified and unqualified table resolution, missing default schema,
  unknown schema, unknown table, reserved target names, duplicate explicit
  names, quoted and unquoted `PRIMARY`, unknown columns, `TEXT`, `CHAR(0)`,
  `VARCHAR(0)`, and unsupported syntax;
- `CREATE TABLE ... LIKE`, `CREATE TABLE ... SELECT`, table rename,
  `ALTER TABLE ... DROP INDEX|KEY`, standalone `DROP INDEX`, table drop,
  reopen persistence, independent file-backed handles, and preamble safety;
- zero-initialized cleanup for new or reused planner objects;
- regression coverage that existing parser, alter-add-index, create-index,
  unique-index, char/varchar key, drop-index, auto-increment, and
  information-schema tests still pass.

MySQL-runtime expectations live in
`packages/libmylite/tests/mysql_baseline_alter_table_add_unique_expectations.sh`
and must run against MySQL 8.4.9 before implementation expectations are
accepted.

## Compatibility Documentation

Update `COMPATIBILITY.md`,
`docs/compatibility/sql-indexes-constraints.md`, and
`docs/compatibility/sql-table-ddl.md` with limited wording for exactly the
supported `ALTER TABLE ... ADD UNIQUE [INDEX|KEY] [name] (column)` subset.

Do not claim full `ADD UNIQUE`, named constraints, composite keys, prefix keys,
descending keys, functional keys, non-ASCII string keys, index options,
visibility, algorithms, locks, temporary tables, views, foreign keys,
privileges, full collation behavior, or optimizer guarantees.

## Verification

Before marking the feature done:

1. `cmake --build --preset dev`
2. Focused CTest entries:
   - `libmylite.parser`
   - `libmylite.runtime.alter_table_add_unique`
   - `libmylite.runtime.alter_table_add_index`
   - `libmylite.runtime.create_index_lifecycle`
   - `libmylite.runtime.unique_index_lifecycle`
   - `libmylite.runtime.char_varchar_key_lifecycle`
   - `libmylite.runtime.drop_index_lifecycle`
   - relevant auto-increment and information-schema runtime tests
3. `packages/libmylite/tests/mysql_baseline_alter_table_add_unique_expectations.sh`
4. `cmake --workflow --preset check`

Review the final diff for MySQL behavior, independently authored grammar/spec
text, catalog authority, descriptor-driven physical SQL generation, unique
duplicate validation, string-key subset correctness, cleanup on failure,
file-format safety, VFS preservation, performance, scope control,
compatibility-matrix accuracy, and test relevance.
