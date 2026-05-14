# Baseline SET Type

## Status

This phase adds a deliberately narrow descriptor-owned `SET` column type for
common MySQL application schemas:

```sql
CREATE TABLE settings (flags SET('active','featured') NOT NULL DEFAULT '')
INSERT INTO settings VALUES ('featured,active')
UPDATE settings SET flags = 'active' WHERE flags = 'active,featured'
```

The implementation stores the selected members as the canonical comma-separated
display string in SQLite `TEXT` columns and keeps MyLite descriptors
authoritative for the permitted member list, defaults, conversion, result
metadata, and introspection. It does not add MySQL's compact bitmap physical
storage model, set ordering, full numeric expression behavior, set indexes, or
full collation semantics.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite connection bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file-format preamble:
  `docs/specs/mylite-file-format/specs.md`
- Baseline catalog, row values, string defaults, `VARCHAR`, `TEXT`, update,
  result metadata, primary-key, prefix-index, and enum specs:
  `docs/specs/baseline-catalog-foundation/specs.md`,
  `docs/specs/baseline-row-values-lifecycle/specs.md`,
  `docs/specs/baseline-string-defaults/specs.md`,
  `docs/specs/baseline-varchar-type/specs.md`,
  `docs/specs/baseline-text-family-types/specs.md`,
  `docs/specs/baseline-update-lifecycle/specs.md`,
  `docs/specs/baseline-result-column-metadata/specs.md`,
  `docs/specs/baseline-primary-key-lifecycle/specs.md`,
  `docs/specs/baseline-index-prefix-key-parts/specs.md`,
  `docs/specs/baseline-enum-type/specs.md`
- MySQL lexer and parser scaffold:
  `docs/specs/mysql-lexer/specs.md`,
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SET` type:
  https://dev.mysql.com/doc/refman/8.4/en/set.html
- MySQL 8.4 Reference Manual, string type syntax:
  https://dev.mysql.com/doc/refman/8.4/en/string-type-syntax.html
- MySQL 8.4 Reference Manual, data type storage requirements:
  https://dev.mysql.com/doc/refman/8.4/en/storage-requirements.html
- MySQL 8.4 Reference Manual, data type default values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

Runtime probes were run against local container `mylite-mysql-849` using:

```sh
docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names --default-character-set=utf8mb4
```

Observed behavior shaping this slice:

- `SET('a','b','c')` stores and reads comma-separated member labels, while
  numeric context exposes a bitmap. The first member has bit value `1`, the
  second `2`, the third `4`, and so on.
- `SHOW COLUMNS` and `SHOW CREATE TABLE` render lower-case
  `set('label',...)` type text and preserve member lettercase from the
  definition.
- `INFORMATION_SCHEMA.COLUMNS` reports `DATA_TYPE = set`, `COLUMN_TYPE` as the
  full set definition, `CHARACTER_MAXIMUM_LENGTH` as the longest possible
  display string length including commas, `CHARACTER_OCTET_LENGTH` as that
  length multiplied by four under `utf8mb4`, and the table default character
  set/collation.
- C API metadata for table-backed set columns reports field type `STRING`, the
  connection collation, display length equal to the longest possible display
  string bytes under the connection character set, and the `SET` flag.
- A `SET` definition must contain at least one quoted string member. The MySQL
  documented maximum is 64 distinct members.
- Definition members have trailing spaces removed when the table is created.
- Duplicate members after trailing-space removal and default collation matching
  fail with `1291 / HY000` under the default strict mode.
- Members containing commas fail during table definition with `1367 / 22007`.
- Under `utf8mb4_0900_ai_ci`, member matching for assignments is ASCII
  case-insensitive in the current MyLite subset, while readback uses the
  definition member's original lettercase.
- Assigning a string list such as `'b,a,a'` stores each selected member once and
  displays selected members in definition order. For `SET('a','b','c')`,
  assigning `'b,a,a'` stores `a,b`.
- Assigning `''` stores the empty set and numeric value `0`.
- Assigning a numeric literal stores members selected by the literal's bitmap.
  For `SET('a','b','c')`, assigning `5` stores `a,c`.
- Assigning a quoted numeric string first tries to match the literal as a
  member name. If there is no member match and the literal is a valid decimal
  bitmap, MySQL stores the bitmap-selected members. For
  `SET('0','1','2')`, assigning `'0'` stores member `0`, while assigning `'3'`
  stores `0,1`.
- Numeric defaults such as `DEFAULT 3`, invalid string defaults, and
  comma-containing invalid defaults fail with `1067 / 42000`.
- Nullable omitted/default values store `NULL`. `NOT NULL` columns with no
  explicit default have no implicit usable default under the default strict
  mode; omitted/default DML fails with `1364 / HY000`.
- `NULL` into a `NOT NULL` set column fails with `1048 / 23000`.
- Invalid row assignment members, negative numeric assignments, and numeric
  bitmaps with bits outside the definition fail under the default strict mode
  with `1265 / 01000`.
- Equality predicates with numeric right operands compare bitmap values.
  Equality predicates with string right operands compare the display string
  under collation semantics: `v = 'A,B'` matches stored `a,b`, while
  `v = 'b,a'`, `v = 'a,b,b'`, and `v = '3'` do not match stored `a,b`.
- `ORDER BY set_col` sorts by numeric bitmap with `NULL` before non-`NULL`
  values for ascending order. This is deferred because this slice stores
  canonical display strings as SQLite `TEXT`.
- MySQL accepts full primary and secondary indexes over `SET` columns, while
  prefix key parts such as `KEY k(v(1))` fail with `1089 / HY000`. MyLite
  defers set keys until bitmap ordering, collation, and duplicate checks are
  designed.

## Scope

Supported:

- persistent and shadowing temporary base tables where the existing DDL/DML
  paths already support the statement class;
- `SET('member'[, ...])` column definitions in `CREATE TABLE`,
  `CREATE TABLE ... LIKE`, and `ALTER TABLE ... ADD [COLUMN]`, plus
  catalog-only `ALTER TABLE ... ALTER [COLUMN] set_col SET DEFAULT ...` and
  `DROP DEFAULT` through the existing default lifecycle;
- one to 64 ordinary single- or double-quoted string literal members, decoded
  through the current MyLite SQL string literal policy;
- nonempty members with valid non-`NUL` UTF-8 bytes;
- trailing-space removal from definition members before duplicate checks and
  descriptor serialization;
- rejection of comma-containing members with MySQL-compatible diagnostics;
- duplicate detection with the current MyLite ASCII case-insensitive
  `utf8mb4_0900_ai_ci` subset;
- durable logical descriptor text `SET('member',...)` and physical descriptor
  text `TEXT`;
- row values for `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`,
  `REPLACE ... SET`, and one-assignment `UPDATE`;
- set row assignments from `NULL`, `DEFAULT`, `''`, a comma-separated member
  string list, an in-range nonnegative decimal integer bitmap, or a quoted
  decimal string that has no member match and names an in-range bitmap;
- nullable omitted/default values as `NULL`, explicit string defaults as the
  canonical definition-order display string, and strict no-default behavior for
  omitted/default `NOT NULL` no-explicit-default columns;
- strict invalid-value diagnostics for unsupported members, negative bitmaps,
  bitmaps with no corresponding member bits, invalid defaults, and unsupported
  literal kinds;
- `NULL` into `NOT NULL` diagnostics using the existing row-values policy;
- descriptor-backed `WHERE` predicates for `IS NULL`, `IS NOT NULL`, `=`,
  `<=>`, `<>`, and `!=` with a decimal-integer bitmap right operand, a string
  right operand, or `<=> NULL`;
- descriptor-backed result metadata for table-backed set columns, including a
  public set flag bit matching MySQL protocol flag values;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, limited
  `INFORMATION_SCHEMA.COLUMNS`, and `CREATE TABLE ... LIKE` descriptor
  rendering;
- reopen persistence, table rename/drop behavior, `.mylite` preamble
  preservation, and independent file-backed handle isolation.

Deferred:

- empty-string set members, because the current display-text storage cannot
  preserve MySQL's distinct empty-member bitmap;
- MySQL's compact bitmap physical storage;
- non-strict invalid set insertion, `INSERT IGNORE` invalid set demotion, and
  warning-producing invalid conversion;
- set `ORDER BY`, set `GROUP BY`, set `DISTINCT`, numeric set expressions such
  as `set_col + 0`, casts, functions, aggregates, expression defaults,
  generated columns, or parameters;
- set primary keys, unique keys, nonunique secondary indexes, foreign-key
  participation, and optimizer/index-use guarantees;
- column-level `CHARACTER SET`, `CHARSET`, `COLLATE`, `BINARY`, national
  string, introducer, adjacent literal concatenation, and full non-ASCII
  collation semantics;
- `ALTER TABLE ... MODIFY [COLUMN]` / `CHANGE [COLUMN]` conversion to or from
  set descriptors;
- set values in `INSERT ... SELECT`, scalar subquery assignment, arbitrary
  expression assignment, and general cross-descriptor conversion.

## Ownership Boundary

- Public API: no new functions. `mylite_execute()` owns public misuse behavior,
  result lifetime, and cleanup. The public result-column flag mask gains the
  stable `MYLITE_RESULT_COLUMN_FLAG_SET` bit used by metadata accessors.
- Statement context: owns diagnostics, warning count, affected rows, insert id,
  and statement transaction completion. Successful supported set operations
  report `warning_count == 0`.
- Lexer/parser/AST: admits set type syntax and stores string-member literal
  nodes. It does not resolve members, defaults, table descriptors, or physical
  storage.
- Analyzer/planner/runtime: normalizes members, resolves defaults and row
  values against set descriptors, validates strict conversion, maps compatible
  predicate values, and rejects unsupported set contexts before generated
  SQLite SQL is built.
- Catalog: MyLite catalog descriptors are authoritative for the set member
  list, logical type, physical type, nullability, defaults, visibility, and
  column order. SQLite schema text is never consulted as logical metadata.
- Result and introspection builders: render set descriptors from catalog text
  and compute metadata lengths from decoded descriptor members.
- SQLite physical row storage: stores the selected set members as the canonical
  comma-separated display string in the stable generated physical user table.
  MyLite binds text bytes and `NULL` values with prepared statements.
- Storage/VFS: unchanged. Set data lives inside the shifted SQLite payload and
  must not mutate the `.mylite` preamble.

No SQLite fork patch is required for this slice. The implementation uses
MyLite-side descriptor translation and public SQLite prepared statements.

## Supported SQL Grammar

Supported column type surface:

```sql
SET ( string_literal [, string_literal ...] )
```

`SET` remains usable as an identifier in non-type contexts according to the
current lexer keyword policy. The parser must reject empty member lists,
non-string member tokens, expressions, parameters, user variables, subqueries,
and type attributes outside the existing column-attribute grammar.

MyLite Lemon-syntax sketch:

```lemon
column_type ::= set_type.

set_type ::= SET LPAREN set_member_list RPAREN.

set_member_list ::= string_literal.
set_member_list ::= set_member_list COMMA string_literal.
```

The analyzer owns member decoding, trailing-space trimming, comma rejection,
duplicate checks, descriptor-size checks, and MySQL-compatible diagnostics.

## Descriptor and Conversion Semantics

The logical descriptor text is canonicalized as:

```sql
SET('member1','member2')
```

Members are serialized with single-quote escaping. The physical descriptor text
is `TEXT`. The logical type text must fit MyLite's durable descriptor capacity.

Definition members are decoded with the session string-literal policy active at
parse time, trimmed for trailing ASCII spaces, rejected when they become empty
or contain a comma or `NUL`, validated as UTF-8, and checked for ASCII
case-insensitive duplicates. MySQL allows an empty-string member, but this
baseline defers it because display-string storage cannot distinguish MySQL's
empty-member bitmap from the empty set.

The selected value is stored as a canonical display string:

- `NULL` remains SQL `NULL`;
- bitmap `0` and an empty string assignment store `''`;
- selected nonempty members are emitted once, in definition order, separated by
  literal commas;
- readback returns that canonical display string.

String row-value conversion:

1. Decode the source SQL string literal with existing MyLite string-literal
   rules.
2. Trim trailing ASCII spaces from the whole assignment string, matching the
   current MySQL-observed `SET` conversion envelope.
3. If the string is empty, store the empty set.
4. Split the string on commas. Commas are not allowed inside definition
   members in this slice, matching MySQL DDL diagnostics.
5. Match each member ASCII case-insensitively against descriptor members.
6. If every member matches, set the matching bits and store the canonical
   definition-order display string.
7. If there is exactly one nonmatching member and it is a valid unsigned
   decimal integer literal, treat it as a bitmap assignment.
8. Otherwise fail with data-truncated diagnostics for the target column and
   row.

Numeric row-value conversion accepts only decimal integer literals with
optional unary `+`, no fractional/approximate/hex/bit forms, and no negative
values. A numeric value is valid only when all set bits are inside the
descriptor's member count. Valid numeric `0` stores the empty set.

Defaults use the same supported string conversion as row values, except numeric
default literals and expression defaults are not admitted for set columns in
this slice because MySQL rejects numeric `SET` defaults under the default
strict mode. This applies to `CREATE TABLE`, `ALTER TABLE ... ADD COLUMN`, and
catalog-only `ALTER TABLE ... ALTER [COLUMN] set_col SET DEFAULT ...`.
`DEFAULT NULL` is allowed only for nullable columns. `ALTER ... DROP DEFAULT`
stores the existing dropped-default descriptor state; later omitted and
explicit `DEFAULT` DML fail with the current `Field '<column>' doesn't have a
default value` path, even for nullable set columns. `NOT NULL` set columns
without an explicit default retain the same no-explicit-default descriptor
state.

Predicate conversion:

- `IS NULL` and `IS NOT NULL` use existing descriptor-null predicate handling.
- Numeric right operands for `=`, `<=>`, `<>`, and `!=` convert through the
  same nonnegative bitmap rules as row values, then compare the canonical
  display string.
- String right operands are decoded, trailing-space trimmed, split on commas,
  and each recognized member is rewritten to the descriptor spelling while
  preserving the right operand's member order and duplicates. Unrecognized
  members remain in the comparison string. This gives MySQL-compatible behavior
  for the supported ASCII collation subset: `A,B` matches stored `a,b`, while
  `b,a`, `a,b,b`, and unknown members do not.
- Quoted numeric strings in predicates remain strings; they do not fall back to
  bitmap comparison.

## Metadata and Introspection

`SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, and
`INFORMATION_SCHEMA.COLUMNS` render `set('member',...)` from the logical
descriptor. The `DATA_TYPE` value is `set`.

`CHARACTER_MAXIMUM_LENGTH` is the longest possible display string: the sum of
member character lengths plus one comma between every selected nonempty member
in definition order.

`CHARACTER_OCTET_LENGTH` is that length multiplied by `4` for the current
`utf8mb4` baseline. The result metadata column type is
`MYLITE_RESULT_COLUMN_TYPE_STRING`, charset/collation follow the current table
default baseline, display length is the longest possible display bytes, and
flags include `MYLITE_RESULT_COLUMN_FLAG_SET`.

## Diagnostics

Supported diagnostics:

- syntax errors and empty member lists: existing parser `1064 / 42000`;
- empty-string members: MyLite unsupported `1064 / 42000`,
  `SET empty-string members are not yet supported`;
- duplicate member: `1291 / HY000`,
  `Column '<column>' has duplicated value '<member>' in SET`;
- comma-containing member: `1367 / 22007`,
  `Illegal set '<member>' value found during parsing`;
- unsupported member literal kind, invalid UTF-8, descriptor too large, or
  unsupported set context: deterministic MyLite unsupported diagnostics until
  the exact MySQL surface is implemented;
- invalid default: `1067 / 42000`;
- omitted/default value for a no-explicit-default `NOT NULL` set column:
  `1364 / HY000`;
- `NULL` into `NOT NULL`: `1048 / 23000`;
- invalid assignment member, negative bitmap, or out-of-range bitmap:
  `1265 / 01000`, `Data truncated for column '<column>' at row <n>`;
- unsupported set ordering, keys, scalar subquery assignment, `INSERT ...
  SELECT`, `ALTER MODIFY`/`CHANGE`, and expression contexts: deterministic
  MyLite unsupported diagnostics;
- physical SQLite, allocation, and public API misuse diagnostics follow the
  existing runtime policies.

Successful supported statements report `warning_count == 0`.

## Physical SQLite Handling

Generated SQLite DDL uses the existing stable physical table naming policy,
with set columns created as quoted `TEXT` columns. Generated DML never
interpolates assignment or predicate literals. MyLite converts set inputs to
either `NULL` or a canonical text value before binding through prepared
statements.

Because this slice stores display text rather than bitmap integers, it must not
claim MySQL set ordering, numeric expressions, or key ordering. Those behaviors
need either a different physical representation or explicit generated
expression/key support in a later slice.

## Test Plan

Add `packages/libmylite/tests/mysql_baseline_set_type_expectations.sh` with
MySQL 8.4.9 probes for:

- descriptor rendering and metadata;
- member trailing-space normalization, duplicate rejection, and comma-member
  rejection;
- defaults, `ALTER COLUMN SET DEFAULT`, `ALTER COLUMN DROP DEFAULT`,
  no-default `NOT NULL` behavior, explicit empty defaults, and canonical
  default ordering;
- `INSERT`, `INSERT ... SET`, `REPLACE`, and `UPDATE` conversion from string
  lists, numeric bitmaps, quoted numeric strings, `''`, `NULL`, and `DEFAULT`;
- assignment diagnostics for unknown members, negative bitmaps, out-of-range
  bitmaps, invalid defaults, and `NULL` into `NOT NULL`;
- equality, null-safe equality, inequality, `IS NULL`, and `IS NOT NULL`
  predicates;
- MySQL-observed but deferred empty-string member, `ORDER BY`, and key
  behavior.

Add fast C tests under `packages/libmylite/tests/`, preferably
`runtime_set_type`, covering:

- parser acceptance and rejection for set type syntax;
- `CREATE TABLE`, `ALTER TABLE ... ADD COLUMN`, `ALTER COLUMN SET DEFAULT`,
  `ALTER COLUMN DROP DEFAULT`, and `CREATE TABLE ... LIKE`;
- DML conversion, affected rows, warning counts, absence of row result sets,
  and remaining rows after updates;
- descriptor-backed predicates over string and numeric right operands;
- metadata through `SHOW`, `INFORMATION_SCHEMA`, and public result accessors;
- reopen persistence, table rename/drop, `.mylite` preamble preservation, and
  independent file-backed handles;
- deterministic unsupported diagnostics for ordering, keys, scalar subquery
  assignment, `INSERT ... SELECT`, `ALTER MODIFY`/`CHANGE`, expression
  defaults, empty-string members, non-string member syntax, and unsupported
  literal forms;
- zero-initialized cleanup for new descriptor/conversion helpers.

Verification before completion:

```sh
cmake --build --preset dev
ctest --preset dev -R 'libmylite\\.(parser|runtime_set_type|runtime_enum_type|runtime_update|runtime_insert|runtime_replace|runtime_show|runtime_information_schema)'
packages/libmylite/tests/mysql_baseline_set_type_expectations.sh
cmake --workflow --preset check
```
