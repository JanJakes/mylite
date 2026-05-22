# Baseline ALTER TABLE Convert Character Set

## Status

This feature specifies a narrow `ALTER TABLE ... CONVERT TO CHARACTER SET`
slice for persistent MyLite base tables whose convertible character columns
already use `utf8mb4`. The slice updates descriptor-owned table and column
charset/collation metadata and preserves row storage. It does not implement
cross-character-set byte transcoding, `binary` conversion, string type widening,
or full collation comparison semantics.

The deliberately supported surface is:

- `ALTER TABLE table_name CONVERT TO CHARACTER SET utf8mb4`
- `ALTER TABLE table_name CONVERT TO CHARSET utf8mb4`
- `ALTER TABLE table_name CONVERT TO CHARACTER SET utf8mb4 COLLATE collation`
- `ALTER TABLE table_name CONVERT TO CHARSET utf8mb4 COLLATE collation`
- `ALTER TABLE table_name CONVERT TO CHARACTER SET DEFAULT` when the resolved
  current database character set is `utf8mb4`

Admitted collations are the current MyLite `utf8mb4` metadata collations:
`utf8mb4_0900_ai_ci`, `utf8mb4_0900_bin`, `utf8mb4_general_ci`,
`utf8mb4_bin`, `utf8mb4_unicode_ci`, and `utf8mb4_unicode_520_ci`.

## Compatibility Authority

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Table charset/collation surface:
  `docs/specs/baseline-table-charset-collation-surface/specs.md`
- Column charset/collation attributes:
  `docs/specs/baseline-column-charset-collation-attributes/specs.md`
- ALTER TABLE default charset/collation:
  `docs/specs/baseline-alter-table-default-charset-collation/specs.md`
- UTF8MB4 legacy collations:
  `docs/specs/baseline-utf8mb4-legacy-collations/specs.md`
- ASCII character set/collation:
  `docs/specs/baseline-ascii-character-set-collation/specs.md`
- Binary character set/collation:
  `docs/specs/baseline-binary-character-set-collation/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- MySQL 8.4 Reference Manual, table character set and collation:
  <https://dev.mysql.com/doc/refman/8.4/en/charset-table.html>
- MySQL 8.4 Reference Manual, column character set and collation:
  <https://dev.mysql.com/doc/refman/8.4/en/charset-column.html>
- Observed MySQL 8.4.9 runtime behavior recorded in
  `packages/libmylite/tests/mysql_baseline_alter_table_convert_character_set_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script records the runtime probes that define this phase.
Observed MySQL behavior:

- `CONVERT TO CHARACTER SET utf8mb4` and `CONVERT TO CHARSET utf8mb4`
  are accepted.
- `CONVERT TO CHARACTER SET utf8mb4 COLLATE utf8mb4_bin` is accepted.
- Charset and collation values may be unquoted identifiers, backtick-quoted
  identifiers, or string literals under the default SQL mode.
- The `=` form is not accepted after `CHARACTER SET` in this statement.
- `COLLATE DEFAULT` and `DEFAULT COLLATE ...` in this statement are syntax
  errors.
- `CONVERT TO CHARACTER SET DEFAULT` uses
  `@@character_set_database` and `@@collation_database` when no explicit
  `COLLATE` is supplied. With a selected database these values come from the
  selected schema, even when the target table is schema-qualified in a
  different schema. Without a selected database MySQL uses the server fallback
  database charset and collation values.
- Unknown charsets fail with `1115 / 42000`.
- Unknown collations fail with `1273 / HY000`.
- A collation that does not belong to the selected charset fails with
  `1253 / 42000`.
- Unqualified targets without a selected database fail with `1046 / 3D000`.
- Unknown explicit schemas fail with `1049 / 42000`.
- Unknown tables fail with `1146 / 42S02`.
- Successful same-`utf8mb4` conversions observed for empty and populated
  tables return warning count `0`, error count `0`, and `ROW_COUNT() = 0`.
- Conversion sets the table default charset/collation and makes inherited
  `CHAR`, `VARCHAR`, and `TEXT` family columns inherit the new table default.
- Character columns that had explicit charset/collation metadata before the
  conversion remain explicitly rendered after the conversion, using the new
  target charset/collation.
- MySQL also supports broader behavior outside this slice, including
  cross-character-set conversion, conversion to `binary`, type widening such as
  `TEXT` to `MEDIUMTEXT`, data conversion errors, and algorithm/lock behavior.

## Scope

Supported:

- persistent base tables only;
- unqualified or schema-qualified target table names using the existing
  selected/default schema policy;
- `CHARACTER SET` and `CHARSET` target forms without `=`;
- target charset `utf8mb4`;
- target charset `DEFAULT` only when the resolved current database character
  set is `utf8mb4`;
- optional `COLLATE` using the currently admitted `utf8mb4` collation catalog;
- name decoding from identifiers, quoted identifiers, and string literals with
  NUL-byte rejection;
- descriptor updates for table default charset/collation;
- descriptor updates for non-national `CHAR`, `VARCHAR`, and `TEXT` family
  columns that already have explicit charset/collation metadata;
- inherited `CHAR`, `VARCHAR`, and `TEXT` family columns continue to inherit the
  new table default instead of receiving explicit column metadata;
- non-character columns and binary string/BLOB columns are left unchanged;
- existing row values remain stored in SQLite without data rewrite;
- `SHOW CREATE TABLE`, `SHOW COLUMNS`, `SHOW FULL COLUMNS`,
  `INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.TABLES`, result metadata,
  and `CREATE TABLE ... LIKE` observe the updated descriptors through existing
  descriptor-driven paths;
- successful statements return through the existing non-row result convention
  with `affected_rows == 0` and `warning_count == 0`;
- reopen persistence, table rename/drop interaction, independent file-backed
  handles, and `.mylite` preamble preservation through existing storage
  boundaries.

Deferred:

- cross-character-set conversion from `ascii`, `binary`, `utf8mb3`, or any
  other source charset to `utf8mb4`;
- target charsets other than resolved `utf8mb4`, including `ascii`, `binary`,
  `utf8mb3`, and `latin1`;
- `CONVERT TO CHARACTER SET binary`, which changes MySQL character columns into
  binary string/BLOB column families;
- byte transcoding, invalid-byte diagnostics, lossy conversion warnings, or row
  value rewrites;
- text-family widening such as `TEXT` to `MEDIUMTEXT`;
- `ENUM` and `SET` conversion;
- national `CHAR` / `VARCHAR` alias conversion;
- explicit `ALGORITHM` / `LOCK` tails for this action;
- temporary tables, views, generated-column dependency rewrites, triggers,
  privileges, metadata locks, online DDL scheduling, binary logging, and
  implicit commit fidelity beyond the current ALTER TABLE baseline;
- SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns the statement boundary,
  result-handle lifetime, and public misuse behavior.
- Statement context: owns diagnostics reset, warning count, affected-row state,
  and descriptor-cache visibility.
- Lexer/parser/AST: admits the narrow convert grammar and preserves target
  charset/collation tokens for runtime validation. Parser code remains
  independent of catalog, runtime, storage, and SQLite.
- Analyzer/runtime planner: resolves the target table, validates object kind
  and supported descriptor families, resolves target charset/collation, rejects
  deferred conversions before mutation, and builds the final descriptor update.
- Catalog: remains authoritative for schemas, table defaults, column
  charset/collation metadata, descriptor versions, and catalog generations.
  SQLite schema text is never the metadata authority.
- Metadata builders: `SHOW`, `INFORMATION_SCHEMA`, and result-column metadata
  render from descriptors after the catalog mutation.
- SQLite physical storage: unchanged. No user table SQL is generated for the
  supported same-charset metadata conversion.
- Storage/VFS: unchanged. The operation writes only catalog rows inside the
  shifted SQLite payload and must not touch the `.mylite` preamble or SQLite
  fork patch set.

## Supported SQL Grammar

The feature adds one single-action `ALTER TABLE` grammar branch:

```sql
ALTER TABLE table_name
  CONVERT TO {CHARACTER SET | CHARSET} convert_charset_name
  [COLLATE convert_collation_name]

convert_charset_name:
    identifier
  | quoted_identifier
  | string_literal
  | DEFAULT

convert_collation_name:
    identifier
  | quoted_identifier
  | string_literal
```

`table_name` may be unqualified or schema-qualified. `=` after
`CHARACTER SET` / `CHARSET` is not admitted for this statement.

MyLite Lemon-syntax snippets:

```lemon
statement(A) ::= alter_table_convert_character_set_statement(B). {
    A = B;
}

alter_table_convert_character_set_statement(A) ::=
    ALTER(A1) TABLE table_name(T) CONVERT TO CHARACTER(C) SET
    option_name(N) convert_character_set_collate_opt(O). {
    A = mylite_sql_parser_make_alter_table_convert_character_set_statement(
        state,
        A1,
        T,
        mylite_sql_parser_append_table_option(
            state,
            mylite_sql_parser_make_table_option_list(
                state,
                mylite_sql_parser_make_table_charset_option(state, C, N)),
            O));
}
alter_table_convert_character_set_statement(A) ::=
    ALTER(A1) TABLE table_name(T) CONVERT TO CHARACTER(C) SET
    BINARY(N) convert_character_set_collate_opt(O). {
    A = mylite_sql_parser_make_alter_table_convert_character_set_statement(
        state,
        A1,
        T,
        mylite_sql_parser_append_table_option(
            state,
            mylite_sql_parser_make_table_option_list(
                state,
                mylite_sql_parser_make_table_charset_option(
                    state,
                    C,
                    mylite_sql_parser_make_identifier(state, N))),
            O));
}
alter_table_convert_character_set_statement(A) ::=
    ALTER(A1) TABLE table_name(T) CONVERT TO CHARSET(C)
    option_name(N) convert_character_set_collate_opt(O). {
    A = mylite_sql_parser_make_alter_table_convert_character_set_statement(
        state,
        A1,
        T,
        mylite_sql_parser_append_table_option(
            state,
            mylite_sql_parser_make_table_option_list(
                state,
                mylite_sql_parser_make_table_charset_option(state, C, N)),
            O));
}
alter_table_convert_character_set_statement(A) ::=
    ALTER(A1) TABLE table_name(T) CONVERT TO CHARSET(C)
    BINARY(N) convert_character_set_collate_opt(O). {
    A = mylite_sql_parser_make_alter_table_convert_character_set_statement(
        state,
        A1,
        T,
        mylite_sql_parser_append_table_option(
            state,
            mylite_sql_parser_make_table_option_list(
                state,
                mylite_sql_parser_make_table_charset_option(
                    state,
                    C,
                    mylite_sql_parser_make_identifier(state, N))),
            O));
}
alter_table_convert_character_set_statement(A) ::=
    ALTER(A1) TABLE table_name(T) CONVERT TO CHARACTER(C) SET DEFAULT(D). {
    A = mylite_sql_parser_make_alter_table_convert_character_set_statement(
        state,
        A1,
        T,
        mylite_sql_parser_make_table_option_list(
            state,
            mylite_sql_parser_make_table_charset_option(
                state,
                C,
                mylite_sql_parser_make_identifier(state, D))));
}
alter_table_convert_character_set_statement(A) ::=
    ALTER(A1) TABLE table_name(T) CONVERT TO CHARSET(C) DEFAULT(D). {
    A = mylite_sql_parser_make_alter_table_convert_character_set_statement(
        state,
        A1,
        T,
        mylite_sql_parser_make_table_option_list(
            state,
            mylite_sql_parser_make_table_charset_option(
                state,
                C,
                mylite_sql_parser_make_identifier(state, D))));
}

convert_character_set_collate_opt(A) ::= . {
    A = NULL;
}
convert_character_set_collate_opt(A) ::= COLLATE(C) option_name(N). {
    A = mylite_sql_parser_make_table_collation_option(state, C, N);
}
convert_character_set_collate_opt(A) ::= COLLATE(C) BINARY(N). {
    A = mylite_sql_parser_make_table_collation_option(
        state,
        C,
        mylite_sql_parser_make_identifier(state, N));
}
```

The helper constructs a table-option list containing the charset option and,
when present, the collation option. The runtime treats this list as convert
metadata, not as ordinary table-default option syntax.

## Resolution And Validation

Target table resolution follows the current writable-table policy:

- unqualified table names require a selected schema;
- schema-qualified names resolve the explicit schema without requiring a
  selected default schema;
- unknown schemas and unknown tables use existing MySQL-shaped diagnostics;
- `_mylite_*` schema or table names are rejected before any SQL is generated;
- only persistent base tables are supported.

Target charset/collation resolution:

- `utf8mb4` is matched ASCII case-insensitively and stored canonically as
  `utf8mb4`;
- `DEFAULT` resolves to the current database character set and, when no
  explicit `COLLATE` is supplied, the current database collation. MyLite uses
  the selected schema defaults when a schema is selected, including
  schema-qualified targets in a different schema, and the fixed server
  fallback `utf8mb4` / `utf8mb4_0900_ai_ci` when no schema is selected;
- explicit `COLLATE` must name a known admitted `utf8mb4` collation;
- if no explicit `COLLATE` is supplied for explicit `utf8mb4`, the target
  collation is `utf8mb4_0900_ai_ci`;
- unknown charset, unknown collation, and charset/collation mismatch reuse the
  current charset/collation diagnostics where possible.

Source table eligibility:

- every non-national `CHAR`, `VARCHAR`, and `TEXT` family column must already
  have effective character set `utf8mb4`;
- non-character columns and binary string/BLOB columns do not participate;
- national `CHAR` / `VARCHAR`, `ENUM`, and `SET` descriptors make this convert
  action unsupported for the table in this slice;
- any participating column with effective charset other than `utf8mb4` makes
  the action unsupported because full byte conversion and type widening are
  deferred.

## Descriptor Mutation

On success, MyLite mutates descriptors in one catalog transaction:

1. The table default charset becomes the target charset.
2. The table default collation becomes the target collation.
3. Each non-national `CHAR`, `VARCHAR`, or `TEXT` family column with an
   explicit charset/collation descriptor before the statement receives explicit
   target charset/collation descriptors.
4. Each participating column without explicit charset/collation descriptors
   keeps empty explicit metadata and therefore inherits the new table default.
5. Non-participating descriptors are preserved byte-for-byte.

The table descriptor version and updated catalog generation advance when the
table default changes. Replaced explicit column descriptors advance their own
descriptor versions and updated catalog generations. No SQLite schema
generation changes are made because physical SQLite storage is unchanged.

## Diagnostics

Required diagnostics:

- syntax errors and unsupported grammar use the current parser diagnostic;
- missing selected schema: `1046 / 3D000`;
- unknown schema: `1049 / 42000`;
- unknown table: `1146 / 42S02`;
- reserved `_mylite_*` target names: existing reserved-name diagnostic;
- unsupported object kind: MyLite-specific unsupported diagnostic;
- unknown charset: `1115 / 42000`;
- unknown collation: `1273 / HY000`;
- target collation not valid for target charset: `1253 / 42000`;
- target charset other than resolved `utf8mb4`: MyLite-specific unsupported
  diagnostic;
- participating source column not already using `utf8mb4`: MyLite-specific
  unsupported diagnostic;
- national `CHAR` / `VARCHAR`, `ENUM`, or `SET` source columns:
  MyLite-specific unsupported diagnostic;
- allocation failure: existing `MYLITE_NOMEM` path;
- physical SQLite/catalog failure: existing internal-error path.

Supported successful statements produce warning count `0`.

## Tests

Fast C tests must cover:

- parser admission for `CHARACTER SET`, `CHARSET`, optional `COLLATE`, quoted
  names, string names, uppercase names, and `DEFAULT`;
- parser rejection for `=`, `COLLATE DEFAULT`, `DEFAULT COLLATE`, missing
  charset name, missing collation name, option tails, and mixed actions;
- successful conversion of same-`utf8mb4` `CHAR`, `VARCHAR`, and `TEXT` family
  descriptors with inherited and explicit metadata;
- `DEFAULT` resolution from a selected schema whose default charset/collation
  is `utf8mb4`, from a schema-qualified target with no selected schema, and
  from a schema-qualified target while a different schema is selected;
- schema-qualified targets without a selected default schema;
- table with no character columns updating only the table default;
- `SHOW CREATE TABLE`, `SHOW FULL COLUMNS`, `INFORMATION_SCHEMA.COLUMNS`, and
  `INFORMATION_SCHEMA.TABLES` after conversion;
- row preservation and result metadata after conversion;
- affected rows `0`, warning count `0`, and no result rows;
- missing default schema, unknown schema, unknown table, reserved target names,
  unsupported object kinds, unknown charset, unknown collation, collation
  mismatch, non-`utf8mb4` target charset, cross-character-set source table,
  national aliases, and `ENUM` / `SET` source descriptors;
- reopen persistence, table rename then convert, drop after convert, independent
  file-backed handles, and `.mylite` preamble preservation.

The MySQL expectation script must verify MySQL 8.4.9 behavior for the admitted
syntax and metadata effects, plus the broader accepted behaviors that this
slice intentionally defers.
