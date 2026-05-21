# Baseline TEXT Family Length Arguments

## Status

This feature extends the existing baseline `TEXT` family descriptor support
with the MySQL `TEXT(M)` compatibility form. The slice is deliberately narrow:
only the `TEXT` type name admits a parenthesized unsigned integer length
argument, and the argument is used only to choose the durable MyLite logical
descriptor family (`TINYTEXT`, `TEXT`, `MEDIUMTEXT`, or `LONGTEXT`) according
to the effective column character set.

The feature does not expand row-value conversion, string collations, full-text
search, indexes, defaults, or public ABI. It reuses the current `TEXT` family
storage and metadata behavior after descriptor normalization.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline text type:
  `docs/specs/baseline-text-type/specs.md`
- Baseline binary string types:
  `docs/specs/baseline-binary-string-types/specs.md`
- Baseline character set and collation support:
  `docs/compatibility/character-sets.md`,
  `docs/compatibility/collations.md`
- MySQL 8.4 Reference Manual, string data type syntax:
  https://dev.mysql.com/doc/refman/8.4/en/string-type-syntax.html
- MySQL 8.4 Reference Manual, `BLOB` and `TEXT`:
  https://dev.mysql.com/doc/refman/8.4/en/blob.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_text_family_length_arguments_expectations.sh`
records the runtime probes for this feature. Observed behavior that shapes this
slice:

- `TEXT(M)` is accepted in column definitions.
- MySQL stores and renders the normalized family name, not the original
  `TEXT(M)` spelling.
- The normalized family depends on the effective maximum bytes per character.
  With default `utf8mb4`, `TEXT(1)` maps to `tinytext`, `TEXT(255)` maps to
  `text`, `TEXT(65535)` maps to `mediumtext`, and `TEXT(16777216)` maps to
  `longtext`. With effective `ascii` or `binary`, `TEXT(255)` maps to
  `tinytext`, `TEXT(256)` maps to `text`, and `TEXT(65536)` maps to
  `mediumtext`.
- Explicit `CHARACTER SET binary`, explicit `COLLATE binary`, or table default
  `CHARSET=binary` normalize the selected text family to the corresponding
  blob family descriptor: `tinyblob`, `blob`, `mediumblob`, or `longblob`.
- `TEXT(0)` is accepted and normalizes to `tinytext`.
- `TEXT(4294967295)` is accepted and normalizes to `longtext`.
- `TEXT(4294967296)` fails with error `1439`, SQLSTATE `42000`, and a display
  width out-of-range diagnostic naming the column and maximum
  `4294967295`.
- `TEXT(-1)`, `TEXT(+1)`, and `TEXT('1')` are syntax errors.
- MySQL accepts some non-integer numeric forms such as `TEXT(1.5)` in this
  context, but MyLite defers those forms in this slice because the current
  descriptor length grammar is intentionally unsigned integer literal based.
- `TINYTEXT(M)`, `MEDIUMTEXT(M)`, and `LONGTEXT(M)` are syntax errors.

## Scope

The implementation must add:

- parser and AST support for `TEXT(unsigned_decimal_integer_literal)`;
- durable descriptor normalization from `TEXT(M)` to the smallest supported
  text family whose byte capacity can hold `M` characters for the effective
  supported character set;
- binary charset/collation normalization from the selected text family to the
  corresponding blob family;
- support on the column-definition paths that already admit bare `TEXT` family
  descriptors: `CREATE TABLE`, `ALTER TABLE ... ADD COLUMN`,
  `ALTER TABLE ... MODIFY COLUMN`, and `ALTER TABLE ... CHANGE COLUMN`;
- descriptor-backed `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `SHOW CREATE TABLE`,
  `INFORMATION_SCHEMA.COLUMNS`, `CREATE TABLE ... LIKE`, reopen persistence,
  DML row storage, and select readback through the existing normalized
  descriptor behavior;
- deterministic MySQL-compatible diagnostics for `TEXT(M)` values above
  `4294967295`;
- parser-level rejection of the explicitly deferred length syntaxes.

## Non-Goals

This feature must not implement:

- `TINYTEXT(M)`, `MEDIUMTEXT(M)`, `LONGTEXT(M)`;
- decimal, approximate, signed, quoted, parameter, expression, or subquery
  length arguments;
- preservation of the original `TEXT(M)` spelling in catalog descriptors or
  `SHOW CREATE TABLE`;
- broader text defaults, charset conversion, Unicode collation comparison,
  full-text search, streaming large-object I/O, or protocol-grade metadata;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result ownership, public misuse behavior, and cleanup.
- Statement context owns diagnostics, warnings, affected rows, and transaction
  completion. Supported `TEXT(M)` DDL records no warning.
- Lexer/parser/AST own syntax admission and preserve the `M` source span. They
  do not choose a catalog descriptor, inspect character sets, or perform
  storage conversion.
- Analyzer/planner code owns descriptor normalization. It parses the unsigned
  integer length, validates the upper bound, resolves effective charset from
  explicit column attributes or table/schema defaults, selects the normalized
  logical type, applies binary charset normalization when required, and
  validates defaults against the final descriptor.
- The catalog remains authoritative for the normalized logical and physical
  descriptor. It stores `TINYTEXT`, `TEXT`, `MEDIUMTEXT`, `LONGTEXT`,
  `TINYBLOB`, `BLOB`, `MEDIUMBLOB`, or `LONGBLOB`; it never stores `TEXT(M)`.
- Result and introspection builders continue to render descriptors from the
  catalog. SQLite schema text is not metadata authority.
- SQLite owns physical row storage using the existing generated row-storage
  tables and prepared statements. The physical type remains `TEXT` for text
  descriptors and `BLOB` for binary-normalized descriptors.
- Storage/VFS behavior is unchanged. The `.mylite` preamble remains outside the
  shifted SQLite payload and this feature writes only through existing catalog
  and row-storage paths.

## Supported SQL Grammar

The feature extends the existing limited column type grammar:

```lemon
text_type(A) ::= text_type_name(T).
text_type(A) ::= TEXT(T) LPAREN INTEGER(L) RPAREN(R).

text_type_name(A) ::= TINYTEXT(T).
text_type_name(A) ::= TEXT(T).
text_type_name(A) ::= MEDIUMTEXT(T).
text_type_name(A) ::= LONGTEXT(T).
text_type_name(A) ::= LONG(T).
text_type_name(A) ::= LONG(T) VARCHAR(V).
```

Only the second production carries a length span and only for the `TEXT` token.
`TEXT(-1)`, `TEXT(+1)`, `TEXT('1')`, `TEXT(?)`, `TEXT(1 + 1)`,
`TINYTEXT(1)`, `MEDIUMTEXT(1)`, and `LONGTEXT(1)` remain outside the grammar.

## Descriptor Normalization

The planner parses `M` as an unsigned decimal integer literal. Values larger
than `4294967295` fail with MySQL error `1439`, SQLSTATE `42000`, and a
display-width message naming the column and maximum `4294967295`.

For supported effective character sets, the planner computes a saturating byte
requirement:

- `binary`: `M * 1`
- `ascii`: `M * 1`
- `utf8mb4`: `M * 4`

The selected descriptor is:

- `TINYTEXT` when the byte requirement is at most `255`;
- `TEXT` when it is at most `65535`;
- `MEDIUMTEXT` when it is at most `16777215`;
- `LONGTEXT` otherwise.

The calculation saturates above `4294967295` and selects `LONGTEXT`; it does
not reject `TEXT(4294967295)` under `utf8mb4`.

If the effective charset/collation is binary, the selected text descriptor is
then normalized through the existing binary string descriptor policy:

- `TINYTEXT` -> `TINYBLOB`
- `TEXT` -> `BLOB`
- `MEDIUMTEXT` -> `MEDIUMBLOB`
- `LONGTEXT` -> `LONGBLOB`

Explicit supported column `CHARACTER SET` / `CHARSET` / `COLLATE` attributes
take precedence over table defaults. Table defaults take precedence over schema
defaults. Existing unsupported charset/collation diagnostics remain unchanged.

## Metadata and DDL Rendering

After normalization, every metadata surface behaves as though the user had
declared the normalized family directly:

- `SHOW COLUMNS` and `SHOW FULL COLUMNS` render the normalized lowercase type;
- `SHOW CREATE TABLE` renders the normalized lowercase type without a `(M)`
  suffix;
- `INFORMATION_SCHEMA.COLUMNS` reports normalized `DATA_TYPE`, `COLUMN_TYPE`,
  character/octet lengths, charset, and collation using the existing descriptor
  rules;
- `CREATE TABLE ... LIKE` clones only the normalized descriptor.

## Diagnostics

The feature preserves existing diagnostics unless explicitly listed:

- syntax errors for unsupported grammar forms use the existing parser syntax
  diagnostic;
- `TEXT(M)` above `4294967295` returns MySQL error `1439`, SQLSTATE `42000`;
- unsupported or unknown charset/collation names use the current MyLite/MySQL
  diagnostics for those attributes;
- allocation failures use existing MyLite allocation diagnostics;
- SQLite/catalog failures use existing runtime diagnostics.

## Performance

`TEXT(M)` is a DDL-time descriptor decision. It adds no per-row expression
evaluation and no query-time materialization. DML and query execution stay on
the existing descriptor-driven SQLite prepared-statement paths after the column
descriptor has been normalized.

## Tests

The C tests must cover:

- parser admission for `TEXT(0)`, boundary integer values, and ALTER ADD /
  MODIFY / CHANGE column definitions;
- parser rejection for signed, quoted, family-specific, expression, and
  parameter length forms;
- `CREATE TABLE` metadata for default `utf8mb4`, explicit/table default
  `ascii`, and explicit/table default `binary`;
- `ALTER TABLE ... ADD`, `MODIFY`, and `CHANGE` preserving the normalized
  descriptor;
- `CREATE TABLE ... LIKE`, close/reopen persistence, row insert/readback, and
  `.mylite` preamble preservation through the existing text/blob storage paths;
- exact diagnostics for out-of-range `TEXT(M)`.

The MySQL expectation script records the MySQL 8.4.9 behavior for the same
user-visible syntax, metadata, and error surfaces.
