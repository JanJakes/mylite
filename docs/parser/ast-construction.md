# Parser AST Construction

The parser now has three modes:

- Syntax-only validation through `mylite_parse_sql()`.
- Generic tree construction through `mylite_parse_sql_ast()`.
- Semantic tree construction through `mylite_parse_sql_semantic_ast()`.

The AST mode is intentionally a generic concrete syntax tree for this milestone.
Every Lemon production builds a rule node, every shifted token can build a token
node, and nodes are owned by a per-parse arena. This proves ownership, spans,
coverage, and construction cost before introducing typed MyLite statement and
expression nodes.

## Preconditions Completed

- The TiDB-to-Lemon parser accepts the WordPress MySQL server query corpus.
- Syntax-only parsing remains available and does not allocate AST nodes.
- Token nodes carry byte spans from the original SQL input.
- AST mode retains a private copy of the SQL input so span-derived metadata can
  be decoded safely after parsing.
- Rule nodes carry generated rule IDs, symbol names, children, and aggregate
  spans.
- Top-level statement views classify each parsed statement and expose stable
  statement spans for the analyzer.
- Statement target descriptors expose all currently known targets for
  multi-target statements such as `DROP TABLE`, `RENAME TABLE`, multi-table
  `DELETE`, and joined `UPDATE`, including length-aware decoded target
  schema/name values.
- `CREATE TABLE` statements expose typed column descriptors for direct column
  definitions, including definition, name, type, option spans, exact type
  name/parameter/attribute spans, decoded column names, exact type kind,
  parser-level storage class, numeric type parameters, semantic type-shape
  fields, enum/set element counts and spans, decoded regular string-literal
  enum/set element values, exact type-attribute spans, decoded charset and
  collation values, selected option detail spans, CST node anchors, coarse type
  family, column option flags, declared nullability, generated storage mode,
  parser-level value metadata for defaults, `ON UPDATE`, and comments, and
  recursive expression views for default, `ON UPDATE`, generated-column, and
  inline `CHECK` expressions.
- `CREATE TABLE` statements expose typed table key and constraint descriptors
  for primary keys, secondary indexes, unique indexes, fulltext indexes, spatial
  indexes, foreign keys, and check constraints.
- Key descriptors include key-part kind, expression, prefix length, ordering,
  decoded constraint/key/key-part/referenced-table/referenced-column names,
  index type/options, key-level index type, visibility, decoded key-option
  values, foreign-key match/actions, check expression, and check enforcement
  spans, plus recursive expression views for functional key parts and table
  `CHECK` constraints.
- `CREATE TABLE` statements expose typed table-option descriptors for common
  options such as engine, charset, collation, row format, comment,
  auto-increment, tablespace, storage, directory, and attribute clauses. Table
  options now also expose a metadata-ready value kind plus decoded string,
  decoded identifier, parsed unsigned integer, list, or raw value payload.
- `CREATE TABLE` statements materialize a compact table-option summary for
  metadata-critical options: engine, charset, collation, comment, and
  auto-increment. Duplicate options are still preserved in the descriptor list;
  the summary keeps the last occurrence as the prototype metadata value.
- `CREATE TABLE` statements now also build a compact semantic view that anchors
  the decoded table target and the existing column, key, and table-option
  descriptor collections without duplicating those arrays. The view exposes
  opaque handles for navigating those descriptors directly, including nested
  column type elements, key parts, and key options.
- `CREATE DATABASE`, `CREATE VIEW`, `ALTER TABLE`, `CREATE INDEX`, `DROP
  DATABASE`, `DROP INDEX`, `DROP TABLE`, `DROP VIEW`, `RENAME TABLE`, and
  `TRUNCATE TABLE` statements expose semantic DDL views that reuse the same
  decoded identifier, target, key-part, key-option, table-option, and
  database-option infrastructure.
- `SELECT` statements expose parser-level query views with CTE/set-operation
  markers, query-block counts, projection descriptors, common clause spans, and
  recursive expression views for projection, `WHERE`, and `HAVING`
  expressions.
- Standalone `VALUES` query forms expose parser-level row/value descriptors,
  `DEFAULT` markers, recursive value expression views, and `ORDER BY` /
  `LIMIT` anchors.
- `DELETE` statements expose parser-level views for single-table, multi-table
  `FROM`, and multi-table `USING` forms, `WITH` markers, priority, `QUICK`, and
  `IGNORE` modifiers, ordered delete-target descriptors, table-reference spans,
  statement-level `WHERE` predicate expressions, and `ORDER BY` / `LIMIT`
  anchors.
- `INSERT` statements expose parser-level views for decoded target tables,
  source-form classification (`VALUES`, `SET`, or `SELECT`), priority and
  `IGNORE` modifiers, partition and duplicate-update markers, optional column
  lists, VALUES row/value descriptors, SET assignment descriptors, SELECT source
  anchors, and duplicate-update assignment descriptors. INSERT values and
  assignments reuse the recursive expression view.
- `REPLACE` statements expose parser-level views for decoded target tables,
  source-form classification (`VALUES`, `SET`, or `SELECT`), priority and
  `INTO` modifiers, partition markers, optional column lists, VALUES row/value
  descriptors, SET assignment descriptors, and SELECT source anchors. REPLACE
  values and assignments reuse the recursive expression view.
- `CALL` statements expose parser-level views for decoded routine
  schema/name, parenthesized-call markers, and ordered argument descriptors.
  CALL arguments reuse the recursive expression view.
- `DO` statements expose parser-level views for the top-level ordered
  expression list. Each expression descriptor reuses the recursive expression
  view.
- `UPDATE` statements expose parser-level views for `WITH` markers,
  single-table versus joined/multi-table target references, priority and
  `IGNORE` modifiers, ordered assignment descriptors, statement-level `WHERE`
  predicate expressions, and `ORDER BY` / `LIMIT` anchors. UPDATE assignments
  reuse the recursive expression view.
- `SET` statements expose typed statement-form and assignment descriptors for
  variable assignments, `SET NAMES`, `SET CHARACTER SET`, `SET TRANSACTION`,
  and TiDB `SET CONFIG` syntax, including value-expression CST anchors.
- SET assignment values now also expose parser-level expression views for root
  literals, identifiers, variables, parameters, function calls, `DEFAULT`,
  unary operators, binary operators, parenthesized expressions, and function
  arguments.
- `USE` statements expose a typed decoded default-database view.
- `PREPARE`, `EXECUTE`, and `DEALLOCATE` statements expose typed decoded
  prepared-statement handles, source descriptors, ordered `USING` user-variable
  descriptors, and deallocate/drop mode.
- `EXPLAIN` and `DESCRIBE` statements expose typed parser views for query,
  analyze, for-connection, and table-description forms, including format kind,
  explained statement anchors, parsed connection IDs, and decoded table/column
  names.
- `SHOW` statements expose typed parser views for common metadata probes,
  including statement kind, scope, `FULL`/`EXTENDED`/`COUNT` modifiers, decoded
  database/table targets, `LIKE` patterns, `WHERE` expressions, and `LIMIT`
  anchors.
- `LOCK TABLES`, `UNLOCK TABLES`, `LOCK INSTANCE`, and TiDB stats-lock forms
  expose typed lock statement views. `LOCK TABLES` also exposes ordered table
  lock descriptors with decoded table/schema names, optional aliases, and lock
  modes.
- `ANALYZE TABLE`, `OPTIMIZE TABLE`, `REPAIR TABLE`, and `CHECKSUM TABLE`
  expose typed table-maintenance statement views with decoded table targets and
  statement option flags.
- `KILL` statements expose typed connection/query mode, TiDB-extension marker,
  and typed target metadata for numeric connection IDs, local names, user
  variables, and builtin-function expressions.
- Grammar-backed `FLUSH` statements expose typed option/log kind, binlog/local
  alias flags, table read-lock/export markers, stats cluster marker, decoded
  table/stats targets, and decoded TiDB plugin names.
- `LOAD DATA` and MySQL `LOAD XML` statements expose typed parser views for
  file/table/charset metadata, duplicate/local/priority flags, partition
  markers, field and line clauses, XML row tags, ignored-row counts, column and
  user-variable lists, SET assignments, and LOAD DATA `WITH` options.
- `CREATE USER`, `ALTER USER`, `DROP USER`, and `SET PASSWORD` statements
  expose typed account-management parser views with decoded account names,
  explicit-host markers, auth-option kind, auth plugin/string/hash values,
  replacement passwords, statement modifiers, and option-clause presence flags.
- `GRANT`, `GRANT PROXY`, `GRANT role`, `REVOKE`, and `REVOKE role/all`
  statements expose typed privilege parser views with statement form, privilege
  and role items, object type, privilege level, decoded users, proxy user, grant
  option, resource-limit marker, and require-clause marker.
- `SET ROLE`, `SET DEFAULT ROLE`, and `SHOW GRANTS` expose typed role-state
  parser views with role option kind, decoded role names and hosts, decoded
  users, `FOR` user marker, and `USING` role marker.
- `CHANGE REPLICATION SOURCE TO`, `CHANGE MASTER TO`, replica/slave
  start/stop/reset forms, binary-log reset/purge forms, and
  replication/binary-log status `SHOW` forms expose typed replication/admin
  parser views with statement kind, decoded channel strings, `ALL` markers,
  ordered option names, value-kind classification, decoded scalar values, and
  integer-list counts.
- Procedure, stored-function, trigger, and event create/alter/drop statements
  expose typed stored-object parser views with statement/object kind, decoded
  object names, `IF EXISTS`/`IF NOT EXISTS`/`OR REPLACE` flags, trigger
  timing/event/table metadata, optional rename targets, and body/definition
  anchors.
- Semantic AST construction now materializes a separate arena-owned graph with
  program, statement, target, descriptor, clause, and expression nodes.
  Descriptor nodes preserve typed parser-view list items such as projections,
  values, assignments, DDL columns/keys/options, table locks, table-maintenance
  targets, replication options, stored objects, flush targets/plugins, LOAD
  items/options/assignments, accounts, privilege items, roles, and EXECUTE
  variables without retaining the parser CST.
- Transaction-control statements expose begin form, access mode, consistency
  modifiers, `WORK`, completion modifiers, and decoded savepoint names.
- Temporary syntax recognizers produce a placeholder root node so AST mode can
  still cover the full current corpus.
- The AST is opaque in the public API and freed with `mylite_ast_free()`.

## Current API Shape

`mylite_parse_sql_ast()` returns an opaque `MyliteAst`. Callers can inspect:

- root node
- total node count
- allocated arena bytes
- node kind
- rule ID
- symbol name
- token ID
- byte span
- children
- top-level statement count
- top-level statement kind
- top-level statement grammar symbol
- top-level statement span
- top-level target kind
- top-level target, schema, and name spans where the target is known
- indexed target descriptors with role, kind, full span, schema span, and name
  span, plus decoded schema/name values
- a semantic `CREATE TABLE` view with statement span, table target/schema/name
  spans, decoded schema/name values, the `nt_create_table_stmt` CST anchor, and
  column/type-element/key/key-part/key-option/table-option descriptor counts and
  handles, plus compact table-option, column, and key summaries for common
  metadata fields
- typed `CREATE TABLE` column descriptors with definition, name, type, type
  name, type parameters, type attributes, options, defaults, `ON UPDATE`,
  generated expression/storage, comments, inline check, and inline reference
  spans, plus decoded column names, CST node anchors, type family, exact type
  kind, storage class, numeric type parameters, semantic type-shape fields,
  enum/set element counts and spans, decoded regular string-literal enum/set
  element values, exact type-attribute spans, decoded charset/collation/comment
  values, default and `ON UPDATE` value kinds, declared nullability, generated
  storage mode, option flags, and recursive expression-view handles for default,
  `ON UPDATE`, generated, and inline check expressions
- typed `CREATE TABLE` key descriptors with kind, full constraint/index span,
  constraint name, key name, local key parts, referenced table, referenced
  schema/name, referenced key parts, index options, key-level index type,
  visibility, decoded key-option values, foreign actions, and check expression
  details, plus decoded values for identifier-bearing fields and recursive
  expression-view handles for functional key parts and table checks
- typed `CREATE TABLE` table-option descriptors with kind, full option span,
  option-name span, value span, value kind, decoded string/identifier values,
  parsed unsigned integers, and list/raw payload spans
- typed `CREATE DATABASE` descriptors with decoded database name, `IF NOT
  EXISTS`, `DATABASE`/`SCHEMA` keyword marker, database-option handles, and
  compact charset/collation/encryption summaries
- typed database-option descriptors with kind, full option span, option-name
  span, value span, value kind, decoded string/identifier values, `DEFAULT`
  marker, and raw payload spans for TiDB overlay options
- typed `CREATE VIEW` descriptors with decoded view target, `OR REPLACE`,
  algorithm, SQL security, check-option kind, optional definer span, explicit
  column handles, and a CST anchor/span for the view query
- typed `SELECT` descriptors with CTE and set-operation markers, query-block
  counts, projection handles, `FROM`, `WHERE`, `GROUP BY`, `HAVING`, `ORDER
  BY`, `LIMIT`, `INTO`, and locking clause spans, plus expression-view handles
  for projection, `WHERE`, and `HAVING` expressions
- typed `SELECT` projection descriptors with expression/wildcard/table-wildcard
  kind, source spans, expression spans, decoded aliases, table-wildcard
  qualifier spans, and recursive expression views where present
- typed standalone `VALUES` descriptors with row-list spans, row/value counts,
  row/value coordinates, `DEFAULT` markers, common clause anchors, and recursive
  value-expression handles
- typed `DELETE` descriptors with statement form, `WITH` and multi-table
  markers, priority, `QUICK`, and `IGNORE` modifiers, delete-target handles,
  target-list spans, table-reference spans, statement-level `WHERE` predicate
  expression handles, and `ORDER BY` / `LIMIT` spans
- typed `DELETE` target descriptors with target/schema/name spans, decoded
  schema/name values, and wildcard markers for `tbl.*`
- typed `REPLACE` descriptors with decoded target table, source form, priority,
  `INTO`, partition, source spans, optional column handles, VALUES row/value
  handles, SET assignment handles, and SELECT source anchors
- typed `REPLACE` column, value, and assignment descriptors with decoded names,
  row/value coordinates, `DEFAULT` markers, and recursive value-expression
  handles
- typed `CALL` descriptors with decoded routine schema/name spans,
  parenthesized-call markers, argument-list spans, and ordered argument
  expression handles
- typed `DO` descriptors with expression-list spans and ordered expression
  handles
- typed `UPDATE` descriptors with `WITH` and multi-table markers, priority and
  `IGNORE` modifiers, table-reference spans, ordered assignment handles,
  statement-level `WHERE` predicate expression handles, and `ORDER BY` / `LIMIT`
  spans
- typed `UPDATE` assignment descriptors with assignment/name/value spans,
  decoded assignment names, value CST anchors, and recursive value-expression
  handles
- typed `SET` descriptors with statement form, assignment kind, variable scope,
  operator kind, decoded assignment names, value CST anchors/spans, and optional
  extended value spans for `SET NAMES ... COLLATE ...`, plus expression-view
  handles for assignment values
- expression descriptors reused by `SET` and DDL expression anchors, with kind,
  literal kind, operator kind, span, operator span, value span, decoded
  string/identifier/function-name values, raw values, parsed unsigned integer
  values where applicable, and recursive child handles
- typed `USE` descriptors with decoded database name
- typed `PREPARE` descriptors with decoded statement name, source kind, source
  span, and decoded source string or user-variable name
- typed `EXECUTE` descriptors with decoded statement name and ordered decoded
  `USING` user variables
- typed `DEALLOCATE` descriptors with decoded statement name and
  `DEALLOCATE`/`DROP PREPARE` mode
- typed `EXPLAIN`/`DESCRIBE` descriptors with statement form, format kind,
  `ANALYZE` marker, explained statement CST anchor/span, parsed connection ID,
  decoded table schema/name, and decoded optional column name
- typed `SHOW` descriptors with common show-kind classification, scope,
  modifiers, target span, decoded database/table names, decoded `LIKE` string,
  recursive `LIKE`/`WHERE` expression handles, and `LIMIT` span
- typed lock descriptors with statement form, table-lock count, decoded
  table/schema names, optional aliases, and `READ`/`READ LOCAL`/`WRITE`/`WRITE
  LOCAL`/`LOW_PRIORITY WRITE` modes
- typed table-maintenance descriptors with statement kind, ordered decoded
  table targets, `NO_WRITE_TO_BINLOG`, `QUICK`, `EXTENDED`, `CHANGED`, `FAST`,
  `MEDIUM`, and `USE_FRM` option flags
- typed `KILL` descriptors with connection/query mode, TiDB-extension marker,
  target kind, parsed connection ID, decoded local/user-variable target values,
  and recursive expression handles for builtin-function targets such as
  `CONNECTION_ID()`
- typed `FLUSH` descriptors with statement kind, log kind, no-write-to-binlog
  and local-alias flags, table read-lock/export markers, stats cluster marker,
  ordered decoded table/stats targets, wildcard stats target markers, and
  decoded TiDB plugin names
- typed LOAD descriptors with statement kind (`DATA` or `XML`), duplicate
  modifier, `LOW_PRIORITY`, `LOCAL`, partition marker, decoded file/table,
  charset, optional format, field and line strings, XML row tag, ignored-row
  count, ordered column/user-variable items, SET assignment expression handles,
  and LOAD DATA option expression handles
- typed account-management descriptors with statement kind, `IF EXISTS` /
  `IF NOT EXISTS`, `FOR` user marker, random-password marker, decoded password
  and replacement values, ordered account descriptors, current-user marker,
  explicit host marker, decoded user/host values, auth kind, auth plugin, auth
  string, hash string, replacement auth string, and presence flags for require,
  connection, password/lock, comment/attribute, and resource-group option
  clauses
- typed privilege descriptors with statement kind, object type, privilege-level
  kind, decoded privilege-level schema/table names, ordered privilege/role/
  dynamic items, item column counts, decoded account/user targets, optional
  proxy user, `WITH GRANT OPTION`, resource-limit marker, and require-clause
  marker
- typed role-state descriptors with statement kind, role option kind
  (`DEFAULT`, `NONE`, `ALL`, `ALL EXCEPT`, regular list), ordered decoded role
  names and optional hosts, decoded target users, `FOR` user marker, and `USING`
  roles marker
- typed replication/admin descriptors with statement kind, decoded channel
  strings, `ALL` markers, ordered source-option descriptors, option names,
  option value kind, decoded scalar values, raw/list values, and integer-list
  counts
- typed stored-object descriptors with statement kind, object kind, decoded
  primary object name, optional secondary rename target, optional trigger
  subject table, trigger timing/event classification, existence/replace flags,
  and body/definition CST anchor
- typed transaction-control descriptors with statement kind, begin form,
  TiDB begin mode, access mode, consistency modifiers, `WORK`, completion
  modifiers, savepoint-keyword marker, and decoded savepoint name
- typed view-column descriptors with name spans and decoded identifier values
- typed `ALTER TABLE` descriptors with decoded target table, ordered operation
  spec handles, coarse operation kind, `IF EXISTS` / `IF NOT EXISTS` flags,
  decoded primary and secondary operation names, nested table targets for
  rename/exchange-style specs, reused table-option handles, and reused
  `CREATE TABLE` column/key descriptors for single-column add/modify/change
  specs, add-constraint/index specs, and multi-item `ADD (...)` specs
- typed `CREATE INDEX` descriptors with index name, target table, key parts,
  index type, visibility, decoded key-option values, and compact key-option
  summaries
- typed `DROP INDEX` descriptors with decoded index name, target table,
  `IF EXISTS`, and TiDB hypothetical-index marker
- typed `DROP DATABASE` descriptors with decoded database name, `IF EXISTS`,
  and `DATABASE`/`SCHEMA` keyword marker
- typed `DROP TABLE` descriptors with temporary/`IF EXISTS` flags and decoded
  table target lists
- typed `DROP VIEW` descriptors with `IF EXISTS`, restrict/cascade mode, and
  decoded multi-view target lists
- typed `RENAME TABLE` descriptors with decoded source/destination table pairs
- typed `TRUNCATE TABLE` descriptors with decoded table target and optional
  `TABLE` keyword marker

The original single-target accessors remain compatibility helpers and mirror
the first indexed target. The indexed API should be used by new analyzer and
typed-AST code so statements that naturally mention several targets do not lose
information. Target schema/name values follow the same identifier normalization
policy as column names: unquoted names point into the retained SQL copy, while
quoted names are arena-backed and have doubled quote characters collapsed.
Target roles are currently:

- `primary` for ordinary DDL/DML table, database, view, routine, account, or
  variable targets
- `source` and `destination` for each `RENAME TABLE source TO destination` pair

The tree is not yet the final semantic MyLite AST. The parser AST is a complete
generated parse tree with typed statement classification and parser-view
descriptor layers suitable for measuring cost and for guiding typed-node work.

`mylite_parse_sql_semantic_ast()` now returns the first separate semantic graph
as an opaque `MyliteSemanticAst`. This graph materializes program -> statement
-> target/descriptor/clause/expression nodes, copies decoded target,
descriptor, clause expression, and expression values into its own arena, and
frees the parser CST. Callers can inspect semantic node kind, statement kind,
target kind/role, descriptor kind, clause kind, expression kind/literal/operator,
spans, copied values, children, node count, statement count, and allocated
bytes. Descriptor nodes own the obvious expression payload for their parser-view
item, including projection, VALUES, assignment, column default/generated/check,
key check/key-part, LOAD assignment, and LOAD option expressions. Clause nodes
own statement-level expression roots for `WHERE`, `HAVING`, `SHOW ...
LIKE/WHERE`, `KILL CONNECTION_ID()`, `CALL` arguments, and `DO` expressions,
and also preserve zero-child structural spans for `FROM`, `GROUP BY`, `ORDER
BY`, `LIMIT`, `INTO`, locking, and DML table-reference clauses.
Statement-specific typed AST objects, name resolution, query-block objects,
table-reference objects, DDL metadata nodes, and execution semantics remain
future layers.

The parser-level semantic `CREATE TABLE` view was the first statement-level AST
object layered on top of the descriptor work. It does not copy column, key, or
option descriptors; instead it gives the next builder a stable object anchored to
`nt_create_table_stmt`, with the normalized table name, descriptor counts, and
opaque descriptor handles. Handle-level accessors expose column type spans,
numeric parameters, enum/set elements, type attributes, option, default,
generated, check, and reference spans, CST anchors, decoded column names, key
kind/name/reference/check/action spans, key-part prefix/order/name details,
key-option spans, column and key metadata summaries, and table-option
kind/value spans needed by the next typed AST pass. This keeps construction
cheap while separating final DDL AST work from the generic CST.

The `CREATE TABLE` column view is the first typed statement-specific layer. It
now classifies the coarse type family (`numeric`, `string`, `temporal`, `json`,
`enum`, `set`, or `spatial`) and exposes flags for common column attributes:
nullability, defaults, auto-increment, inline key markers, comments, generated
columns, `ON UPDATE`, references, checks, unsigned, zerofill, character set, and
collation. It also exposes exact source spans for column names, type names, type
parameter lists, type attributes, default values, `ON UPDATE` values, generated
expressions and storage mode, comments, inline check expressions/enforcement,
and inline references. Column names are exposed as length-aware values; unquoted
names point into the retained SQL copy, while quoted names are arena-backed and
have doubled quote characters collapsed. The column type view now classifies the
exact grammar type kind for MySQL numeric, string, temporal, JSON, enum/set, and
spatial type tokens, including common aliases such as `INT8`, `FLOAT8`,
`NATIONAL VARCHAR`, `LONG`, `LONG VARCHAR`, and `LONG VARBINARY`. It also
exposes a compact
storage-class bucket (`integer`, `decimal`, `float`, `bit`, fixed/variable
character string, binary string, blob, text, enum, set, JSON, temporal, spatial,
or vector) derived from the exact type kind. `LONG` and `LONG VARCHAR` are
classified as text while `LONG VARBINARY` is classified as blob, matching
MySQL's compatibility mapping to `MEDIUMTEXT` and `MEDIUMBLOB` documented in
[Using Data Types from Other Database Engines](https://dev.mysql.com/doc/mysql/en/other-vendor-data-types.html).
These are parser-derived metadata summaries, not yet final MyLite runtime
metadata. The column view decodes unsigned numeric type parameters from the
retained SQL copy, so `DECIMAL(10,2)`, `VARCHAR(50)`, `DATETIME(6)`, and
`VECTOR(3)` expose their parsed parameter counts and integer values. It also
maps those raw parameters into semantic type-shape fields: length for integer
display width, bit/string/binary/vector lengths, precision and scale for
decimal and floating types, and fractional-seconds precision for temporal
types. `ENUM` and `SET` expose top-level element counts, element source spans,
and decoded values for regular string-literal elements, including doubled
quotes and default MySQL backslash escapes. Hex and bit literal elements retain
spans but do not yet expose decoded byte values. Type attributes now expose
exact spans for `UNSIGNED`, `ZEROFILL`, `BINARY`, charset clauses and values,
and collation clauses and values. `CHARACTER SET binary` is recorded as a
charset value, not as the `BINARY` attribute. Column options now summarize
declared nullability, generated storage mode, decoded charset/collation/comment
values, and value kinds for defaults and `ON UPDATE`, including `NULL`,
unsigned decimal integers, `CURRENT_TIMESTAMP`, decoded string literals, and
raw expression text. Defaults, `ON UPDATE`, generated expressions, and inline
checks also expose recursive parser-level expression views for common
literal/identifier/function-call/unary/binary/parenthesized shapes. The next
layer must resolve exact MySQL type semantics, SQL-mode-sensitive expression
and literal values, semantic expression nodes, metadata defaults, partitions,
and `CREATE TABLE ... SELECT`.

Column descriptors keep direct CST node anchors for the type node, option list,
default value, `ON UPDATE` value, generated expression/storage, comment option,
inline check expression/enforcement, and inline reference. These anchors let the
next semantic AST builder walk the relevant subtree directly instead of
searching the whole column definition again. The semantic `CREATE TABLE` view
now exposes those column details through the column handle, including nested
type-element handles for `ENUM` and `SET`, typed default and `ON UPDATE` value
metadata, declared nullability, generated storage mode, and decoded option
values. It also exposes parser-level expression handles for default, `ON
UPDATE`, generated-column, and inline check expressions so typed AST
construction can reuse the recursive expression view instead of rescanning the
CST.

The `CREATE TABLE` key view covers table-level primary keys, indexes, unique
keys, fulltext keys, spatial keys, foreign keys, and check constraints. It
preserves both constraint names and key names because MySQL syntax can carry
both independently, and it records foreign-key referenced table and referenced
columns. Key parts now distinguish column and functional-expression parts and
carry prefix length and `ASC`/`DESC` spans. Constraint names, key names, column
key-part names, referenced table schema/name parts, and referenced column names
are exposed as length-aware decoded identifier values using the same zero-copy
unquoted / arena-backed quoted policy as table targets and columns. Key options
expose index type, decoded `KEY_BLOCK_SIZE`, decoded comments, decoded parser
names, visibility, secondary-engine attributes, and `WHERE` spans. Key handles
also expose compact summaries for key-level index type, visibility, comment,
parser, and key-block-size so metadata construction can avoid rescanning options
for common cases. Foreign keys expose `MATCH`, `ON DELETE`, and `ON UPDATE`;
check constraints expose expression and enforcement spans. Functional key parts
and table checks also expose recursive parser-level expression views. The view
does not yet normalize these details into final metadata records or generate
missing constraint names. The semantic `CREATE TABLE` view now exposes key-part
and key-option handles directly from each key handle, so typed AST construction
no longer needs to re-index through the statement-level compatibility accessors.

The `CREATE TABLE` table-option view covers the option list after the table
element list. It classifies common MySQL table options and exposes the original
full option text, keyword/name span, and value span. It also classifies option
values as decoded identifiers, decoded strings, unsigned integers, lists, raw
payloads, or unknown values. Numeric option values are parsed into
`unsigned long long` when the grammar span is a single unsigned decimal token.
The semantic `CREATE TABLE` view also materializes last-seen summary fields for
engine, charset, collation, comment, and auto-increment so metadata construction
can avoid scanning the option list for common cases. It intentionally does not
yet validate mutually exclusive options or apply MySQL metadata defaults.

The first non-`CREATE TABLE` DDL views cover statement shapes that are useful to
the analyzer without requiring expression trees. `ALTER TABLE` exposes the
decoded target table, ordered operation specs, common operation kind, decoded
operation names, optional nested table targets for rename/exchange specs, and
reused table-option descriptors for clauses such as `ENGINE` and
`AUTO_INCREMENT`. Single-column add/modify/change specs also expose the same
column descriptor used by `CREATE TABLE`; add-constraint/index specs expose the
same key descriptor used by `CREATE TABLE`, including decoded key names, key
parts, options, and summary metadata. Multi-item `ADD (...)` specs expose
indexed column and key descriptor arrays in statement order by descriptor type.
`CREATE DATABASE` exposes the decoded database target, `IF NOT EXISTS`,
`DATABASE`/`SCHEMA` keyword choice, database-option descriptors, and compact
charset/collation/encryption summaries. `CREATE VIEW` exposes the decoded view
target, `OR REPLACE`, algorithm, SQL security, check-option kind, optional
definer span, explicit column descriptors, and the query CST anchor. `CREATE
INDEX` reuses the table-key descriptors for key parts and key options while
adding the decoded index name and target table. `DROP DATABASE` exposes the
decoded database target, `IF EXISTS`, and keyword choice. `DROP TABLE` exposes
`TEMPORARY`, `IF EXISTS`, and all table targets. `DROP VIEW` exposes `IF
EXISTS`, restrict/cascade mode, and all view targets. `DROP INDEX` exposes the
decoded index name, target table, `IF EXISTS`, and the TiDB hypothetical-index
syntax marker. `RENAME TABLE` exposes each source/destination pair as decoded
target values. `TRUNCATE TABLE` exposes the decoded table target and whether the
optional `TABLE` keyword was present. These views are still parser-level
metadata: they do not perform existence checks, generate implicit names,
validate engine-specific rules, or apply runtime effects.

The first parser-level `SELECT` view is intentionally structural rather than a
final query AST. It records whether the statement has a `WITH` clause or set
operation, counts query blocks, exposes all projection fields found in statement
order, distinguishes expression projections from `*` and table-qualified
wildcards, decodes projection aliases, and anchors the first visible `FROM`,
`WHERE`, `GROUP BY`, `HAVING`, `ORDER BY`, `LIMIT`, `INTO`, and locking clauses.
Projection, `WHERE`, and `HAVING` expressions reuse the same recursive
expression view as DDL and `SET`. The view keeps nested CTE/subquery/union
projections in statement order for now; the later semantic query AST must split
them into query-expression and query-block objects with scope-aware name
resolution.

The first parser-level `DELETE` view covers all TiDB grammar-level DELETE
shapes currently used by the MySQL corpus: single-table `DELETE FROM table`,
multi-table `DELETE target_list FROM table_refs`, and multi-table `DELETE FROM
target_list USING table_refs`. It records `WITH`, statement form, priority,
`QUICK`, `IGNORE`, ordered delete targets with decoded schema/name values and
`tbl.*` markers, table-reference spans, statement-level `WHERE` predicate
expression handles, and `ORDER BY` / `LIMIT` anchors for single-table forms.
The view does not yet implement partition pruning, alias resolution, joined
delete target filtering, affected-row semantics, warning demotion, or runtime
diagnostics.

The first parser-level `INSERT` view covers the statement forms needed before
semantic insert execution can be built. It records the decoded target table,
classifies the source as `VALUES`, `SET`, or `SELECT`, records priority and
`IGNORE`, preserves partition and `ON DUPLICATE KEY UPDATE` markers, decodes
optional column lists, exposes VALUES row/value coordinates, exposes SET and
duplicate-update assignments, and anchors INSERT SELECT sources. Value and
assignment payloads reuse the recursive expression view, so defaults, literals,
parameters, identifiers, binary/unary operators, and common function calls are
available without a second CST traversal. The view is still parser-level:
generated-column handling, defaults, duplicate-key resolution, affected-row
counts, warnings, insert ids, and row-alias ODKU semantics remain runtime work.
Newer INSERT row-alias forms that currently use the temporary recognized
statement placeholder do not yet have structured descriptors.

The first parser-level `REPLACE` view mirrors the INSERT source forms that
MySQL accepts for REPLACE while keeping a statement-specific API. It records the
decoded target table, source kind (`VALUES`, `SET`, or `SELECT`), priority,
`INTO`, partition spans, optional column list, VALUES row/value coordinates,
SET assignments, and SELECT source anchor. Value and assignment payloads reuse
the recursive expression view. The view does not yet implement delete-then-insert
semantics, cascades, triggers, affected rows, warnings, auto-increment behavior,
or query/replace metadata integration.

The first parser-level `CALL` view records the routine span, decoded optional
schema/name pair, whether the parsed form used parentheses, the argument-list
span, and ordered argument descriptors. Argument payloads reuse the recursive
expression view, so literals, user variables, and assignment expressions are
available to the next AST layer. Stored-procedure lookup, invocation, OUT/INOUT
parameter binding, result-set streaming, diagnostics, and security context
remain runtime work.

The first parser-level standalone `VALUES` view hangs off `nt_select_stmt`
because TiDB parses `VALUES ROW(...)` as a SELECT-like query form. The
top-level statement kind remains `MYLITE_STATEMENT_SELECT`, while
`mylite_ast_values_statement_view()` exposes the VALUES-specific row list,
row/value coordinates, `DEFAULT` markers, recursive value expression views, and
`ORDER BY` / `LIMIT` / `INTO` / lock anchors. Query result production,
metadata inference, ordering, limiting, and locking semantics remain later
query-engine work.

The first parser-level `DO` view records the statement expression-list span and
ordered top-level expression descriptors. Each descriptor anchors the original
CST expression and exposes the shared recursive expression view, covering
function calls, variable assignment expressions, literals, identifiers, and
operators. Runtime expression evaluation, result discarding, diagnostics, and
warning propagation remain later execution-layer work.

The first parser-level `UPDATE` view covers the statement shape needed before
semantic update execution. It records whether a `WITH` clause is present,
anchors the table-reference subtree, marks joined/multi-table references,
records `LOW_PRIORITY` and `IGNORE`, exposes ordered assignments with decoded
left-hand names, and anchors statement-level `WHERE`, `ORDER BY`, and `LIMIT`
clauses. Assignment values and the `WHERE` predicate reuse the recursive
expression view. The view does not yet implement assignment evaluation order,
generated-column validation, joined-update target filtering, affected rows,
warning demotion, or runtime diagnostics.

Session-level statement views now cover `SET` and `USE`. `SET` records the
statement form, ordered assignments, assignment kind, variable scope, assignment
operator, decoded assignment name, value CST anchor/span, and optional extended
value span for `SET NAMES ... COLLATE ...`. Assignment values also expose the
first recursive parser-level expression view: root expression kind, literal
kind, operator kind/span, source span, value span, decoded
string/identifier/function-name value, raw value, parsed unsigned integer where
available, and child expression handles for common unary/binary/parenthesized
forms plus function arguments. This is still not the final semantic expression
AST: unsupported expression shapes keep their CST anchor/span for later
normalization. `USE` records the decoded default database target.

Prepared-statement views now cover the SQL-level prepared statement surface.
`PREPARE` records the decoded statement handle and distinguishes string-literal
sources from user-variable sources, exposing the decoded SQL string or decoded
user-variable name. `EXECUTE` records the decoded statement handle and ordered
decoded `USING` user variables. `DEALLOCATE PREPARE` and `DROP PREPARE` record
the decoded statement handle and mode. These are parser-level descriptors only;
they do not yet validate parameter marker rules, maintain the per-connection
prepared-statement registry, or execute prepared statements.

Diagnostic parser views now cover `EXPLAIN` and its `DESCRIBE`/`DESC`
synonyms. The view distinguishes ordinary explained statements,
`EXPLAIN ANALYZE`, `EXPLAIN FOR CONNECTION`, and table-description forms. It
records `FORMAT=TRADITIONAL|JSON|TREE`, anchors the explained statement subtree
without classifying that nested statement yet, parses the connection ID for the
for-connection form, and decodes table schema/name plus optional column name
for `DESCRIBE table [column]`. The view intentionally matches only direct
`EXPLAIN` children for modifiers, so keywords inside the explained query do not
change the outer diagnostic statement classification.

Metadata inspection views now cover common `SHOW` statements. The view
classifies broad statement families such as databases, tables, columns, indexes,
variables, status, warnings/errors, grants, processlist, `SHOW CREATE ...`, and
other metadata probes. It records session/global scope, `FULL`, `EXTENDED`, and
`COUNT(*)` modifiers, decoded database/table targets, decoded string-literal
`LIKE` values, recursive expression handles for `LIKE` and `WHERE`, and `LIMIT`
spans. This is intentionally still a parser view: result-set columns, privilege
checks, warning table contents, status-variable lookup, and information-schema
backing rows remain semantic/runtime work.

Lock parser views cover MySQL table locks, unlocks, and instance backup locks,
plus TiDB stats-lock statement forms at the classifier level. `LOCK TABLES`
materializes ordered table-lock descriptors with decoded target schema/name,
optional aliases from `AS alias` and bare-alias forms, and lock mode
classification for `READ`, `READ LOCAL`, `WRITE`, `WRITE LOCAL`, and
`LOW_PRIORITY WRITE`. Runtime lock compatibility, implicit commits, metadata
locking, connection-level ownership, and stats-lock effects remain later
semantic/runtime work.

Table-maintenance parser views cover `ANALYZE TABLE`, `OPTIMIZE TABLE`,
`REPAIR TABLE`, and `CHECKSUM TABLE`. The view records statement kind, ordered
decoded target tables, and grammar-level option flags including
`NO_WRITE_TO_BINLOG`, `QUICK`, `EXTENDED`, `CHANGED`, `FAST`, `MEDIUM`, and
`USE_FRM`. This stays at the parser layer: table existence, storage-engine
behavior, statistics refresh, checksum result rows, binlog effects, and warning
or error semantics remain runtime work.

`KILL` parser views cover the MySQL `KILL [CONNECTION|QUERY] target` shape and
the TiDB `KILL TIDB` extension marker. The view normalizes omitted mode to
connection kill, parses numeric connection IDs, decodes local identifier and
user-variable targets, and exposes builtin-function targets through the shared
recursive expression view. Session lookup, privilege checks, active-query
cancellation, connection teardown, and diagnostics remain runtime work.

Grammar-backed `FLUSH` parser views cover privileges, status, hosts, log
variants, table flushes, table export, client error summaries, stats delta,
optimizer costs, user resources, and TiDB plugin forms. The view records
`NO_WRITE_TO_BINLOG`/`LOCAL`, log type, table read-lock/export flags, stats
cluster marker, ordered table and stats targets, wildcard stats targets, and
decoded plugin names. Some comma-list FLUSH forms are still accepted through the
temporary recognizer and therefore do not yet produce a typed view; replacing
those recognizers with real grammar productions remains parser-port work.

LOAD parser views cover TiDB-ported `LOAD DATA` plus the MySQL `LOAD XML`
productions added by the Lemon port. The view records `LOW_PRIORITY`, `LOCAL`,
`IGNORE`/`REPLACE`, partition markers, decoded file path, decoded target table,
optional `FORMAT`, charset, field/column clauses (`TERMINATED`, `ENCLOSED`,
`OPTIONALLY ENCLOSED`, `ESCAPED`, `DEFINED NULL`), line clauses, ignored row
count, XML row tag, ordered load column/user-variable items, SET assignments,
and LOAD DATA `WITH` options. Assignment and option values reuse the recursive
parser-level expression view. File access, LOCAL protocol negotiation, row
import, conversion warnings, duplicate handling, and security restrictions
remain semantic/runtime work.

Account-management parser views cover `CREATE USER`, `ALTER USER`, `DROP USER`,
and `SET PASSWORD`. The view records statement kind, `IF EXISTS` /
`IF NOT EXISTS`, `FOR` user, random-password use, decoded password and
replacement values, ordered account descriptors, current-user/builtin-user
accounts, explicit hosts, decoded account user/host values, auth-option kind,
decoded auth plugin, decoded auth string, hash string, and replacement auth
string. It also records whether TLS require clauses, connection limits,
password/lock clauses, comments/attributes, and resource-group clauses are
present. Account metadata changes, authentication semantics, generated random
password values, privilege checks, and diagnostics remain semantic/runtime work.

Privilege parser views cover TiDB-ported grant/revoke grammar for privilege
grants, role grants, proxy grants, privilege revokes, role revokes, and MySQL's
`REVOKE ALL PRIVILEGES, GRANT OPTION` form. The view records statement form,
ordered privilege/role/dynamic items, item column counts, object type, privilege
level kind, decoded level schema/table names, decoded target users with auth
options, optional proxy user, `WITH GRANT OPTION`, resource-limit `WITH`
markers, and TLS require-clause presence. Privilege catalog mutations, role
membership, partial revokes, dynamic-privilege validation, proxy semantics,
authorization, warnings, and result diagnostics remain semantic/runtime work.

Role-state parser views cover `SET ROLE`, `SET DEFAULT ROLE`, and `SHOW GRANTS`
role clauses. The view records statement form, role option kind (`DEFAULT`,
`NONE`, `ALL`, `ALL EXCEPT`, or explicit role list), ordered decoded role names,
optional role hosts, decoded target users, `SHOW GRANTS FOR` user marker, and
`SHOW GRANTS ... USING` role marker. Active-role state, default-role catalog
changes, privilege checks, result-set construction, warnings, and diagnostics
remain semantic/runtime work.

Replication/admin parser views cover `CHANGE REPLICATION SOURCE TO`, legacy
`CHANGE MASTER TO`, `START`/`STOP` replica and slave forms, `RESET` master,
binary-log/GTID, replica/slave/source forms, `PURGE BINARY`/`MASTER LOGS`, and
replication/binary-log status `SHOW` forms. The view records statement kind,
decoded `FOR CHANNEL` strings, `ALL` markers for reset forms, ordered
replication option descriptors, decoded option names, scalar value kind,
decoded string/identifier values, raw numeric/list values, and integer-list
counts. Replication metadata, binary-log state, channel handling, privilege
checks, result-set construction, warnings, and diagnostics remain
semantic/runtime work.

Stored-object parser views cover procedure, stored-function, trigger, and event
create/alter/drop statements. The view records statement kind, object kind,
decoded primary object names, `IF EXISTS`/`IF NOT EXISTS`/`OR REPLACE` flags,
body or definition anchors, optional event rename targets, and for triggers,
decoded subject table plus trigger timing/event classification. Stored-program
body semantics, metadata catalog changes, scheduling, trigger execution,
security context, privilege checks, warnings, and diagnostics remain
semantic/runtime work.

Transaction-control views cover `BEGIN`, `START TRANSACTION`, `COMMIT`,
`ROLLBACK`, `SAVEPOINT`, and `RELEASE SAVEPOINT`. `BEGIN`/`START TRANSACTION`
records the begin form, optional TiDB pessimistic/optimistic markers, MySQL
access mode, `WITH CONSISTENT SNAPSHOT`, and TiDB causal-consistency syntax.
`COMMIT` and `ROLLBACK` record `WORK` plus chain/no-chain and
release/no-release completion modifiers. Savepoint statements and rollback to
savepoint record decoded savepoint names and whether the `SAVEPOINT` keyword
was explicit. These descriptors do not yet implement transaction state,
implicit commits, savepoint stack behavior, or diagnostics.

For development inspection:

```sh
printf 'SELECT 1\n' | build/mylite-parse --statements
printf 'SELECT 1\n' | build/mylite-parse --ast
printf 'SELECT 1\n' | build/mylite-parse --semantic
```

## Benchmark

Prepare a NUL-separated corpus from the WordPress CSV, then run:

```sh
build-perf/mylite-parser-bench /tmp/mylite-parser-corpus.nul syntax 100
build-perf/mylite-parser-bench /tmp/mylite-parser-corpus.nul ast-only 100
build-perf/mylite-parser-bench /tmp/mylite-parser-corpus.nul ast 100
build-perf/mylite-parser-bench /tmp/mylite-parser-corpus.nul semantic 100
```

`ast-only` builds and frees the parser CST and parser-view summaries without the
large benchmark coverage walk. `ast` keeps the historical public-view coverage
counters. `semantic` parses through the parser AST, materializes the first
semantic graph, frees the parser AST, and then counts the semantic graph.

Latest semantic-AST construction run on May 3, 2026:

```text
mode=syntax queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=2.769838 qps=502130 mbps=38.19 avg_us=1.992
mode=ast-only queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.157245 qps=194323 mbps=14.78 avg_us=5.146 avg_nodes=74.5 avg_ast_bytes=11091.3 avg_statements=1.00
mode=ast queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.286471 qps=190877 mbps=14.52 avg_us=5.239 avg_nodes=74.5 avg_ast_bytes=11091.3
mode=semantic queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.784954 qps=178655 mbps=13.59 avg_us=5.597 avg_semantic_nodes=8.2 avg_semantic_bytes=4363.2 avg_semantic_statements=1.00 avg_semantic_targets=0.60 avg_semantic_descriptors=2.57 avg_semantic_clauses=0.35 avg_semantic_structural_clauses=0.24 avg_semantic_expressions=2.68 avg_semantic_expression_operators=0.22 avg_semantic_expression_leaf_values=2.04 avg_semantic_descriptor_expressions=1.81 avg_semantic_clause_expressions=0.11 avg_semantic_statement_expressions=0.00
```

Latest EXPLAIN/DESCRIBE parser-view run on May 3, 2026:

```text
mode=ast queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.310846 qps=190241 mbps=14.47 avg_us=5.257 avg_nodes=74.5 avg_ast_bytes=10905.4 avg_explain_statement_views=0.05 avg_explain_statement_query_forms=0.05 avg_explain_statement_analyze_forms=0.00 avg_explain_statement_for_connection_forms=0.00 avg_explain_statement_table_forms=0.00 avg_explain_statement_format_values=0.01 avg_explain_statement_statement_nodes=0.05 avg_explain_statement_connection_ids=0.00 avg_explain_statement_table_name_values=0.00 avg_explain_statement_column_values=0.00
mode=semantic queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.491807 qps=185645 mbps=14.12 avg_us=5.387 avg_semantic_nodes=5.3 avg_semantic_bytes=4268.0 avg_semantic_statements=1.00 avg_semantic_targets=0.60 avg_semantic_expressions=2.67 avg_semantic_expression_operators=0.22 avg_semantic_expression_leaf_values=2.03
```

Latest SHOW parser-view run on May 3, 2026:

```text
mode=ast queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.446338 qps=186779 mbps=14.20 avg_us=5.354 avg_nodes=74.5 avg_ast_bytes=10907.9 avg_show_statement_views=0.02 avg_show_statement_table_forms=0.00 avg_show_statement_column_forms=0.00 avg_show_statement_variable_forms=0.00 avg_show_statement_warning_forms=0.00 avg_show_statement_create_forms=0.00 avg_show_statement_scopes=0.00 avg_show_statement_full_modifiers=0.00 avg_show_statement_extended_modifiers=0.00 avg_show_statement_count_modifiers=0.00 avg_show_statement_database_values=0.00 avg_show_statement_table_name_values=0.01 avg_show_statement_like_values=0.00 avg_show_statement_like_expressions=0.00 avg_show_statement_where_expressions=0.00 avg_show_statement_limit_clauses=0.00 avg_show_statement_expression_tree_nodes=0.01 avg_show_statement_expression_tree_operators=0.00 avg_show_statement_expression_tree_leaf_values=0.01
mode=semantic queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.616669 qps=182602 mbps=13.89 avg_us=5.476 avg_semantic_nodes=5.3 avg_semantic_bytes=4268.0 avg_semantic_statements=1.00 avg_semantic_targets=0.60 avg_semantic_expressions=2.67 avg_semantic_expression_operators=0.22 avg_semantic_expression_leaf_values=2.03
```

Latest lock parser-view run on May 3, 2026:

```text
mode=ast queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.391203 qps=188172 mbps=14.31 avg_us=5.314 avg_nodes=74.5 avg_ast_bytes=10912.5 avg_lock_statement_views=0.00 avg_lock_statement_lock_tables=0.00 avg_lock_statement_unlock_tables=0.00 avg_lock_statement_instance_forms=0.00 avg_lock_statement_stats_forms=0.00 avg_lock_statement_table_locks=0.00 avg_lock_statement_table_name_values=0.00 avg_lock_statement_alias_values=0.00 avg_lock_statement_known_modes=0.00
mode=semantic queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.510324 qps=185188 mbps=14.08 avg_us=5.400 avg_semantic_nodes=5.3 avg_semantic_bytes=4268.0 avg_semantic_statements=1.00 avg_semantic_targets=0.60 avg_semantic_expressions=2.67 avg_semantic_expression_operators=0.22 avg_semantic_expression_leaf_values=2.03
```

Latest table-maintenance parser-view run on May 3, 2026:

```text
mode=ast queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.645747 qps=181908 mbps=13.83 avg_us=5.497 avg_nodes=74.5 avg_ast_bytes=10915.5 avg_table_maintenance_statement_views=0.01 avg_table_maintenance_statement_targets=0.01 avg_table_maintenance_statement_target_name_values=0.01 avg_table_maintenance_statement_option_flags=0.00
mode=semantic queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.848676 qps=177204 mbps=13.48 avg_us=5.643 avg_semantic_nodes=5.3 avg_semantic_bytes=4268.0 avg_semantic_statements=1.00 avg_semantic_targets=0.60 avg_semantic_expressions=2.67 avg_semantic_expression_operators=0.22 avg_semantic_expression_leaf_values=2.03
```

Latest KILL parser-view run on May 3, 2026:

```text
mode=ast queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.179944 qps=193709 mbps=14.73 avg_us=5.162 avg_nodes=74.5 avg_ast_bytes=10942.9 avg_kill_statement_views=0.00 avg_kill_statement_queries=0.00 avg_kill_statement_tidb_extensions=0.00 avg_kill_statement_connection_ids=0.00 avg_kill_statement_target_values=0.00 avg_kill_statement_target_expressions=0.00 avg_kill_statement_expression_tree_nodes=0.00 avg_kill_statement_expression_tree_operators=0.00 avg_kill_statement_expression_tree_leaf_values=0.00
mode=semantic queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.294432 qps=190669 mbps=14.50 avg_us=5.245 avg_semantic_nodes=5.3 avg_semantic_bytes=4268.0 avg_semantic_statements=1.00 avg_semantic_targets=0.60 avg_semantic_expressions=2.67 avg_semantic_expression_operators=0.22 avg_semantic_expression_leaf_values=2.04
```

Latest FLUSH parser-view run on May 3, 2026:

```text
mode=ast queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.204600 qps=193046 mbps=14.68 avg_us=5.180 avg_nodes=74.5 avg_ast_bytes=10998.9 avg_flush_statement_views=0.00 avg_flush_statement_log_forms=0.00 avg_flush_statement_table_forms=0.00 avg_flush_statement_stats_forms=0.00 avg_flush_statement_no_write_to_binlog=0.00 avg_flush_statement_read_locks=0.00 avg_flush_statement_for_exports=0.00 avg_flush_statement_cluster_flags=0.00 avg_flush_statement_targets=0.00 avg_flush_statement_target_name_values=0.00 avg_flush_statement_target_wildcards=0.00 avg_flush_statement_plugins=0.00 avg_flush_statement_plugin_name_values=0.00
mode=semantic queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.431519 qps=187152 mbps=14.23 avg_us=5.343 avg_semantic_nodes=5.3 avg_semantic_bytes=4268.0 avg_semantic_statements=1.00 avg_semantic_targets=0.60 avg_semantic_expressions=2.67 avg_semantic_expression_operators=0.22 avg_semantic_expression_leaf_values=2.04
```

Latest LOAD DATA/XML parser-view run on May 3, 2026:

```text
mode=syntax queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=2.770828 qps=501951 mbps=38.17 avg_us=1.992
mode=ast queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.378734 qps=188490 mbps=14.33 avg_us=5.305 avg_nodes=74.5 avg_ast_bytes=11049.7 avg_load_statement_views=0.00 avg_load_statement_data_forms=0.00 avg_load_statement_xml_forms=0.00 avg_load_statement_local_flags=0.00 avg_load_statement_file_values=0.00 avg_load_statement_table_name_values=0.00 avg_load_statement_field_clauses=0.00 avg_load_statement_line_clauses=0.00 avg_load_statement_ignore_rows=0.00 avg_load_statement_row_tags=0.00 avg_load_statement_items=0.00 avg_load_statement_item_values=0.00 avg_load_statement_assignments=0.00 avg_load_statement_assignment_expressions=0.00 avg_load_statement_options=0.00 avg_load_statement_option_values=0.00 avg_load_statement_expression_tree_nodes=0.00 avg_load_statement_expression_tree_operators=0.00 avg_load_statement_expression_tree_leaf_values=0.00
mode=semantic queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.536452 qps=184546 mbps=14.03 avg_us=5.419 avg_semantic_nodes=5.3 avg_semantic_bytes=4268.0 avg_semantic_statements=1.00 avg_semantic_targets=0.60 avg_semantic_expressions=2.67 avg_semantic_expression_operators=0.22 avg_semantic_expression_leaf_values=2.04
```

Latest account-management parser-view run on May 3, 2026:

```text
mode=syntax queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=2.760947 qps=503747 mbps=38.31 avg_us=1.985
mode=ast queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.279281 qps=191066 mbps=14.53 avg_us=5.234 avg_nodes=74.5 avg_ast_bytes=11074.0 avg_account_statement_views=0.02 avg_account_statement_create_users=0.01 avg_account_statement_alter_users=0.00 avg_account_statement_drop_users=0.01 avg_account_statement_set_passwords=0.00 avg_account_statement_if_exists=0.00 avg_account_statement_if_not_exists=0.00 avg_account_statement_for_users=0.00 avg_account_statement_random_passwords=0.00 avg_account_statement_password_values=0.00 avg_account_statement_replacement_password_values=0.00 avg_account_statement_accounts=0.03 avg_account_statement_current_users=0.00 avg_account_statement_explicit_hosts=0.02 avg_account_statement_user_values=0.03 avg_account_statement_host_values=0.02 avg_account_statement_auth_options=0.01 avg_account_statement_auth_plugins=0.00 avg_account_statement_auth_strings=0.01 avg_account_statement_hash_strings=0.00 avg_account_statement_replacement_auth_strings=0.00
mode=semantic queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.417214 qps=187512 mbps=14.26 avg_us=5.333 avg_semantic_nodes=5.3 avg_semantic_bytes=4268.0 avg_semantic_statements=1.00 avg_semantic_targets=0.60 avg_semantic_expressions=2.67 avg_semantic_expression_operators=0.22 avg_semantic_expression_leaf_values=2.04
```

Latest privilege parser-view run on May 3, 2026:

```text
mode=syntax queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=2.813765 qps=494291 mbps=37.59 avg_us=2.023
mode=ast queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.239667 qps=192111 mbps=14.61 avg_us=5.205 avg_nodes=74.5 avg_ast_bytes=11077.0 avg_privilege_statement_views=0.02 avg_privilege_statement_grants=0.02 avg_privilege_statement_grant_proxies=0.00 avg_privilege_statement_grant_roles=0.00 avg_privilege_statement_revokes=0.00 avg_privilege_statement_revoke_roles=0.00 avg_privilege_statement_revoke_alls=0.00 avg_privilege_statement_with_grant_options=0.00 avg_privilege_statement_resource_limits=0.00 avg_privilege_statement_require_clauses=0.00 avg_privilege_statement_object_types=0.00 avg_privilege_statement_level_schemas=0.01 avg_privilege_statement_level_names=0.01 avg_privilege_statement_items=0.03 avg_privilege_statement_privilege_items=0.02 avg_privilege_statement_role_items=0.00 avg_privilege_statement_dynamic_items=0.00 avg_privilege_statement_item_columns=0.00 avg_privilege_statement_users=0.02 avg_privilege_statement_proxy_users=0.00
mode=semantic queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.466737 qps=186269 mbps=14.17 avg_us=5.369 avg_semantic_nodes=5.3 avg_semantic_bytes=4268.0 avg_semantic_statements=1.00 avg_semantic_targets=0.60 avg_semantic_expressions=2.67 avg_semantic_expression_operators=0.22 avg_semantic_expression_leaf_values=2.04
```

Latest role-state parser-view run on May 3, 2026:

```text
mode=syntax queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=2.864718 qps=485500 mbps=36.92 avg_us=2.060
mode=ast queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.437276 qps=187007 mbps=14.22 avg_us=5.347 avg_nodes=74.5 avg_ast_bytes=11080.8 avg_role_statement_views=0.00 avg_role_statement_set_roles=0.00 avg_role_statement_set_default_roles=0.00 avg_role_statement_show_grants=0.00 avg_role_statement_default_options=0.00 avg_role_statement_none_options=0.00 avg_role_statement_all_options=0.00 avg_role_statement_all_except_options=0.00 avg_role_statement_regular_options=0.00 avg_role_statement_for_users=0.00 avg_role_statement_using_roles=0.00 avg_role_statement_roles=0.00 avg_role_statement_role_hosts=0.00 avg_role_statement_role_name_values=0.00 avg_role_statement_role_host_values=0.00 avg_role_statement_users=0.00
mode=semantic queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.643262 qps=181967 mbps=13.84 avg_us=5.496 avg_semantic_nodes=5.3 avg_semantic_bytes=4268.0 avg_semantic_statements=1.00 avg_semantic_targets=0.60 avg_semantic_expressions=2.67 avg_semantic_expression_operators=0.22 avg_semantic_expression_leaf_values=2.04
```

Latest replication/admin parser-view run on May 3, 2026:

```text
mode=syntax queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=2.786763 qps=499081 mbps=37.96 avg_us=2.004
mode=ast queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.337179 qps=189558 mbps=14.42 avg_us=5.275 avg_nodes=74.5 avg_ast_bytes=11083.8 avg_replication_statement_views=0.00 avg_replication_statement_change_sources=0.00 avg_replication_statement_change_masters=0.00 avg_replication_statement_start_forms=0.00 avg_replication_statement_stop_forms=0.00 avg_replication_statement_reset_forms=0.00 avg_replication_statement_show_forms=0.00 avg_replication_statement_purge_forms=0.00 avg_replication_statement_channels=0.00 avg_replication_statement_all_flags=0.00 avg_replication_statement_options=0.00 avg_replication_statement_option_name_values=0.00 avg_replication_statement_option_string_values=0.00 avg_replication_statement_option_integer_lists=0.00
mode=semantic queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.424682 qps=187324 mbps=14.25 avg_us=5.338 avg_semantic_nodes=5.3 avg_semantic_bytes=4268.0 avg_semantic_statements=1.00 avg_semantic_targets=0.60 avg_semantic_expressions=2.67 avg_semantic_expression_operators=0.22 avg_semantic_expression_leaf_values=2.04
```

Latest stored-object parser-view run on May 3, 2026:

```text
mode=syntax queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=2.823285 qps=492625 mbps=37.46 avg_us=2.030
mode=ast queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.551822 qps=184170 mbps=14.01 avg_us=5.430 avg_nodes=74.5 avg_ast_bytes=11091.3 avg_stored_object_statement_views=0.06 avg_stored_object_statement_create_procedures=0.02 avg_stored_object_statement_drop_procedures=0.01 avg_stored_object_statement_alter_procedures=0.00 avg_stored_object_statement_create_functions=0.01 avg_stored_object_statement_drop_functions=0.01 avg_stored_object_statement_alter_functions=0.00 avg_stored_object_statement_triggers=0.01 avg_stored_object_statement_events=0.00 avg_stored_object_statement_if_exists=0.01 avg_stored_object_statement_if_not_exists=0.00 avg_stored_object_statement_or_replace=0.00 avg_stored_object_statement_name_values=0.06 avg_stored_object_statement_secondary_name_values=0.00 avg_stored_object_statement_table_name_values=0.01 avg_stored_object_statement_definitions=0.03
mode=semantic queries=69541 iterations=20 parsed=1390820 failed=0 elapsed=7.742466 qps=179635 mbps=13.66 avg_us=5.567 avg_semantic_nodes=5.3 avg_semantic_bytes=4268.0 avg_semantic_statements=1.00 avg_semantic_targets=0.60 avg_semantic_expressions=2.67 avg_semantic_expression_operators=0.22 avg_semantic_expression_leaf_values=2.04
```

Release benchmark result on May 2, 2026:

```text
mode=syntax queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=14.173586 qps=490638 mbps=37.31 avg_us=2.038
mode=ast queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=23.488291 qps=296067 mbps=22.52 avg_us=3.378 avg_nodes=74.5 avg_ast_bytes=10249.3 avg_statements=1.00 avg_targets=0.60 avg_target_schema_values=0.02 avg_target_name_values=0.60 avg_columns=0.29 avg_keys=0.06 avg_create_table_views=0.13 avg_create_table_view_schema_values=0.00 avg_create_table_view_name_values=0.13 avg_create_table_view_summary_engines=0.03 avg_create_table_view_summary_comments=0.00 avg_create_table_view_summary_auto_increments=0.00 avg_create_table_view_columns=0.29 avg_create_table_view_keys=0.06 avg_create_table_view_options=0.04 avg_create_table_view_column_handles=0.29 avg_create_table_view_known_column_types=0.29 avg_create_table_view_column_type_numeric_params=0.11 avg_create_table_view_column_type_element_handles=0.05 avg_create_table_view_column_type_element_values=0.05 avg_create_table_view_column_type_lengths=0.09 avg_create_table_view_column_type_unsigned_attrs=0.01 avg_create_table_view_column_type_charset_values=0.01 avg_create_table_view_column_type_collation_values=0.00 avg_create_table_view_column_option_spans=0.12 avg_create_table_view_column_defaults=0.05 avg_create_table_view_column_default_values=0.05 avg_create_table_view_column_default_unsigned_values=0.00 avg_create_table_view_column_on_update_values=0.00 avg_create_table_view_column_comments=0.00 avg_create_table_view_column_comment_values=0.00 avg_create_table_view_column_checks=0.00 avg_create_table_view_column_nullabilities=0.07 avg_create_table_view_column_generated_storage_kinds=0.00 avg_create_table_view_column_type_nodes=0.29 avg_create_table_view_column_options_nodes=0.12 avg_create_table_view_key_handles=0.06 avg_create_table_view_named_keys=0.02 avg_create_table_view_key_index_types=0.00 avg_create_table_view_key_visibilities=0.00 avg_create_table_view_key_comments=0.00 avg_create_table_view_key_parsers=0.00 avg_create_table_view_key_block_sizes=0.00 avg_create_table_view_key_column_handles=0.09 avg_create_table_view_named_key_columns=0.09 avg_create_table_view_ordered_key_columns=0.00 avg_create_table_view_prefixed_key_columns=0.00 avg_create_table_view_expression_key_columns=0.00 avg_create_table_view_referenced_column_handles=0.00 avg_create_table_view_named_referenced_columns=0.00 avg_create_table_view_key_option_handles=0.00 avg_create_table_view_key_option_values=0.00 avg_create_table_view_key_option_identifier_values=0.00 avg_create_table_view_key_option_string_values=0.00 avg_create_table_view_key_option_unsigned_integer_values=0.00 avg_create_table_view_key_option_index_type_values=0.00 avg_create_table_view_option_handles=0.04 avg_create_table_view_option_values=0.04 avg_create_table_view_option_identifier_values=0.04 avg_create_table_view_option_string_values=0.00 avg_create_table_view_option_unsigned_integer_values=0.00 avg_create_table_view_option_list_values=0.00 avg_alter_table_views=0.03 avg_alter_table_schema_values=0.00 avg_alter_table_name_values=0.03 avg_alter_table_specs=0.04 avg_alter_table_named_specs=0.02 avg_alter_table_secondary_named_specs=0.00 avg_alter_table_renamed_tables=0.00 avg_alter_table_column_payloads=0.01 avg_alter_table_column_known_types=0.01 avg_alter_table_key_payloads=0.01 avg_alter_table_key_columns=0.02 avg_alter_table_options=0.00 avg_alter_table_if_exists=0.00 avg_alter_table_if_not_exists=0.00 avg_create_database_views=0.00 avg_create_database_name_values=0.00 avg_create_database_options=0.00 avg_create_database_option_values=0.00 avg_create_database_charset_values=0.00 avg_create_database_collation_values=0.00 avg_create_database_encryption_values=0.00 avg_create_database_if_not_exists=0.00 avg_create_database_schema_keywords=0.00 avg_create_index_views=0.00 avg_create_index_name_values=0.00 avg_create_index_table_name_values=0.00 avg_create_index_columns=0.00 avg_create_index_options=0.00 avg_create_index_comments=0.00 avg_create_index_key_block_sizes=0.00 avg_create_view_views=0.01 avg_create_view_schema_values=0.00 avg_create_view_name_values=0.01 avg_create_view_columns=0.00 avg_create_view_column_values=0.00 avg_create_view_or_replace=0.00 avg_create_view_algorithms=0.00 avg_create_view_sql_securities=0.00 avg_create_view_check_options=0.00 avg_create_view_select_nodes=0.01 avg_drop_database_views=0.00 avg_drop_database_name_values=0.00 avg_drop_database_if_exists=0.00 avg_drop_database_schema_keywords=0.00 avg_drop_index_views=0.00 avg_drop_index_name_values=0.00 avg_drop_index_table_name_values=0.00 avg_drop_index_if_exists=0.00 avg_drop_table_views=0.02 avg_drop_table_tables=0.03 avg_drop_table_if_exists=0.00 avg_drop_view_views=0.00 avg_drop_view_view_targets=0.01 avg_drop_view_if_exists=0.00 avg_drop_view_modes=0.00 avg_set_statement_views=0.08 avg_set_statement_assignments=0.08 avg_set_assignment_name_values=0.08 avg_set_assignment_scopes=0.03 avg_set_assignment_value_nodes=0.08 avg_set_assignment_extend_value_nodes=0.00 avg_set_assignment_system_variables=0.06 avg_set_assignment_user_variables=0.02 avg_set_assignment_names=0.00 avg_set_assignment_character_sets=0.00 avg_set_assignment_transaction_characteristics=0.00 avg_set_assignment_configs=0.00 avg_rename_table_views=0.00 avg_rename_table_pairs=0.00 avg_truncate_table_views=0.00 avg_truncate_table_name_values=0.00 avg_truncate_table_table_keywords=0.00 avg_use_database_views=0.00 avg_use_database_name_values=0.00 avg_key_constraint_name_values=0.00 avg_key_name_values=0.02 avg_key_referenced_table_schema_values=0.00 avg_key_referenced_table_name_values=0.00 avg_key_columns=0.09 avg_key_column_name_values=0.09 avg_key_referenced_column_name_values=0.00 avg_key_options=0.00 avg_options=0.04 avg_column_name_values=0.29 avg_column_defaults=0.05 avg_column_on_updates=0.00 avg_column_generated=0.00 avg_column_checks=0.00 avg_column_references=0.00 avg_column_known_types=0.29 avg_column_storage_classes=0.29 avg_column_type_numeric_params=0.11 avg_column_type_elements=0.05 avg_column_type_element_values=0.05 avg_column_type_lengths=0.09 avg_column_type_precisions=0.01 avg_column_type_scales=0.01 avg_column_type_fsps=0.01 avg_column_type_unsigned_attrs=0.01 avg_column_type_zerofill_attrs=0.00 avg_column_type_binary_attrs=0.00 avg_column_type_charsets=0.01 avg_column_type_collations=0.00 avg_column_value_roots=0.06
```

Latest expression-summary run on the same corpus:

```text
mode=ast queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=23.544499 qps=295360 mbps=22.46 avg_us=3.386 avg_nodes=74.5 avg_ast_bytes=10249.3 avg_set_assignment_value_expressions=0.08 avg_set_assignment_expression_values=0.07 avg_set_assignment_expression_unsigned_integers=0.02 avg_set_assignment_expression_literals=0.05 avg_set_assignment_expression_function_calls=0.00 avg_set_assignment_expression_defaults=0.01
```

Latest prepared-statement view run on the same corpus:

```text
mode=syntax queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=13.927726 qps=499299 mbps=37.97 avg_us=2.003
mode=ast queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=23.597245 qps=294700 mbps=22.41 avg_us=3.393 avg_nodes=74.5 avg_ast_bytes=10257.2 avg_prepare_statement_views=0.02 avg_prepare_statement_name_values=0.02 avg_prepare_statement_string_sources=0.02 avg_prepare_statement_user_variable_sources=0.00 avg_prepare_statement_source_values=0.02 avg_execute_statement_views=0.01 avg_execute_statement_name_values=0.01 avg_execute_statement_using_variables=0.01 avg_execute_statement_using_variable_name_values=0.01 avg_deallocate_statement_views=0.00 avg_deallocate_statement_name_values=0.00 avg_deallocate_statement_modes=0.00
```

Latest transaction-control view run on the same corpus:

```text
mode=syntax queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=13.895676 qps=500451 mbps=38.06 avg_us=1.998
mode=ast queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=23.614253 qps=294487 mbps=22.40 avg_us=3.396 avg_nodes=74.5 avg_ast_bytes=10258.4 avg_transaction_statement_views=0.00 avg_transaction_statement_begins=0.00 avg_transaction_statement_commits=0.00 avg_transaction_statement_rollbacks=0.00 avg_transaction_statement_savepoints=0.00 avg_transaction_statement_release_savepoints=0.00 avg_transaction_statement_work_keywords=0.00 avg_transaction_statement_access_modes=0.00 avg_transaction_statement_consistent_snapshots=0.00 avg_transaction_statement_completion_modifiers=0.00 avg_transaction_statement_savepoint_names=0.00
```

Latest recursive SET-expression view run on the same corpus:

```text
mode=syntax queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=14.220037 qps=489035 mbps=37.19 avg_us=2.045
mode=ast queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=23.858651 qps=291471 mbps=22.17 avg_us=3.431 avg_nodes=74.5 avg_ast_bytes=10258.6 avg_set_assignment_value_expressions=0.08 avg_set_assignment_expression_values=0.07 avg_set_assignment_expression_unsigned_integers=0.02 avg_set_assignment_expression_literals=0.05 avg_set_assignment_expression_function_calls=0.00 avg_set_assignment_expression_defaults=0.01 avg_set_assignment_expression_tree_nodes=0.09 avg_set_assignment_expression_tree_operators=0.00 avg_set_assignment_expression_tree_leaf_values=0.08
```

Latest DDL expression-view run on the same corpus:

```text
mode=syntax queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=14.064650 qps=494438 mbps=37.60 avg_us=2.022
mode=ast queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=23.975336 qps=290052 mbps=22.06 avg_us=3.448 avg_nodes=74.5 avg_ast_bytes=10458.0 avg_create_table_view_column_expression_roots=0.06 avg_create_table_view_column_expression_tree_nodes=0.07 avg_create_table_view_column_expression_tree_operators=0.00 avg_create_table_view_column_expression_tree_leaf_values=0.06 avg_create_table_view_key_expression_roots=0.00 avg_create_table_view_key_expression_tree_nodes=0.00 avg_create_table_view_key_expression_tree_operators=0.00 avg_create_table_view_key_expression_tree_leaf_values=0.00 avg_set_assignment_expression_tree_nodes=0.09 avg_set_assignment_expression_tree_operators=0.00 avg_set_assignment_expression_tree_leaf_values=0.08
```

Latest SELECT parser-view run on the same corpus:

```text
mode=syntax queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=14.589427 qps=476653 mbps=36.25 avg_us=2.098
mode=ast queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=33.844687 qps=205471 mbps=15.63 avg_us=4.867 avg_nodes=74.5 avg_ast_bytes=10654.3 avg_select_statement_views=0.25 avg_select_statement_query_blocks=0.27 avg_select_statement_projections=0.41 avg_select_statement_projection_expressions=0.36 avg_select_statement_where_expressions=0.06 avg_select_statement_having_expressions=0.01 avg_select_statement_expression_tree_nodes=1.02 avg_select_statement_expression_tree_operators=0.16 avg_select_statement_expression_tree_leaf_values=0.51
```

Latest INSERT parser-view run on the same corpus:

```text
mode=syntax queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=14.639612 qps=475019 mbps=36.13 avg_us=2.105
mode=ast queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=36.629721 qps=189849 mbps=14.44 avg_us=5.267 avg_nodes=74.5 avg_ast_bytes=10870.9 avg_insert_statement_views=0.19 avg_insert_statement_values_sources=0.18 avg_insert_statement_set_sources=0.00 avg_insert_statement_select_sources=0.01 avg_insert_statement_columns=0.04 avg_insert_statement_value_rows=0.58 avg_insert_statement_values=1.27 avg_insert_statement_set_assignments=0.01 avg_insert_statement_duplicate_assignments=0.00 avg_insert_statement_expression_tree_nodes=1.34 avg_insert_statement_expression_tree_operators=0.01 avg_insert_statement_expression_tree_leaf_values=1.29
```

Latest UPDATE parser-view run on the same corpus:

```text
mode=syntax queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=13.970734 qps=497762 mbps=37.86 avg_us=2.009
mode=ast queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=36.136379 qps=192440 mbps=14.64 avg_us=5.196 avg_nodes=74.5 avg_ast_bytes=10876.5 avg_update_statement_views=0.03 avg_update_statement_with_clauses=0.00 avg_update_statement_multi_table=0.00 avg_update_statement_priorities=0.00 avg_update_statement_ignores=0.00 avg_update_statement_assignments=0.03 avg_update_statement_assignment_name_values=0.03 avg_update_statement_where_expressions=0.02 avg_update_statement_order_by_clauses=0.00 avg_update_statement_limit_clauses=0.00 avg_update_statement_expression_tree_nodes=0.10 avg_update_statement_expression_tree_operators=0.02 avg_update_statement_expression_tree_leaf_values=0.07
```

Latest DELETE parser-view run on the same corpus:

```text
mode=syntax queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=13.847471 qps=502193 mbps=38.19 avg_us=1.991
mode=ast queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=35.768205 qps=194421 mbps=14.79 avg_us=5.143 avg_nodes=74.5 avg_ast_bytes=10879.6 avg_delete_statement_views=0.01 avg_delete_statement_with_clauses=0.00 avg_delete_statement_multi_table=0.00 avg_delete_statement_multi_table_from=0.00 avg_delete_statement_multi_table_using=0.00 avg_delete_statement_priorities=0.00 avg_delete_statement_quicks=0.00 avg_delete_statement_ignores=0.00 avg_delete_statement_targets=0.01 avg_delete_statement_target_schema_values=0.00 avg_delete_statement_target_name_values=0.01 avg_delete_statement_target_wildcards=0.00 avg_delete_statement_where_expressions=0.01 avg_delete_statement_order_by_clauses=0.00 avg_delete_statement_limit_clauses=0.00 avg_delete_statement_expression_tree_nodes=0.02 avg_delete_statement_expression_tree_operators=0.01 avg_delete_statement_expression_tree_leaf_values=0.01
```

Latest REPLACE parser-view run on the same corpus:

```text
mode=syntax queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=13.965277 qps=497956 mbps=37.87 avg_us=2.008
mode=ast queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=34.939524 qps=199032 mbps=15.14 avg_us=5.024 avg_nodes=74.5 avg_ast_bytes=10885.9 avg_replace_statement_views=0.00 avg_replace_statement_values_sources=0.00 avg_replace_statement_set_sources=0.00 avg_replace_statement_select_sources=0.00 avg_replace_statement_priorities=0.00 avg_replace_statement_into_clauses=0.00 avg_replace_statement_partition_clauses=0.00 avg_replace_statement_columns=0.00 avg_replace_statement_column_name_values=0.00 avg_replace_statement_value_rows=0.00 avg_replace_statement_values=0.00 avg_replace_statement_default_values=0.00 avg_replace_statement_set_assignments=0.00 avg_replace_statement_assignment_name_values=0.00 avg_replace_statement_expression_tree_nodes=0.00 avg_replace_statement_expression_tree_operators=0.00 avg_replace_statement_expression_tree_leaf_values=0.00
```

Latest DO parser-view run on the same corpus:

```text
mode=syntax queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=14.046980 qps=495060 mbps=37.65 avg_us=2.020
mode=ast queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=35.306811 qps=196962 mbps=14.98 avg_us=5.077 avg_nodes=74.5 avg_ast_bytes=10888.5 avg_do_statement_views=0.00 avg_do_statement_expressions=0.00 avg_do_statement_expression_tree_nodes=0.01 avg_do_statement_expression_tree_operators=0.00 avg_do_statement_expression_tree_leaf_values=0.00
```

Latest CALL parser-view run on the same corpus:

```text
mode=syntax queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=13.797887 qps=503997 mbps=38.33 avg_us=1.984
mode=ast queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=35.733288 qps=194611 mbps=14.80 avg_us=5.138 avg_nodes=74.5 avg_ast_bytes=10890.7 avg_call_statement_views=0.01 avg_call_statement_schema_values=0.00 avg_call_statement_name_values=0.01 avg_call_statement_parentheses=0.01 avg_call_statement_arguments=0.01 avg_call_statement_expression_tree_nodes=0.01 avg_call_statement_expression_tree_operators=0.00 avg_call_statement_expression_tree_leaf_values=0.01
```

Latest standalone VALUES parser-view run on the same corpus:

```text
mode=syntax queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=13.899991 qps=500295 mbps=38.05 avg_us=1.999
mode=ast queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=35.143866 qps=197875 mbps=15.05 avg_us=5.054 avg_nodes=74.5 avg_ast_bytes=10893.3 avg_values_statement_views=0.00 avg_values_statement_rows=0.00 avg_values_statement_values=0.00 avg_values_statement_default_values=0.00 avg_values_statement_order_by_clauses=0.00 avg_values_statement_limit_clauses=0.00 avg_values_statement_into_clauses=0.00 avg_values_statement_lock_clauses=0.00 avg_values_statement_expression_tree_nodes=0.00 avg_values_statement_expression_tree_operators=0.00 avg_values_statement_expression_tree_leaf_values=0.00
```

Before semantic actions were generated, syntax-only parsing measured about
`711k queries/sec` on the same corpus. The current syntax-only path still runs
the generated reduce actions, but with AST building disabled, so it avoids arena
allocation while paying some action-call overhead.

Current release build size on the same machine:

```text
generated parser C: 72,876 lines, 5,639,543 bytes
generated parser object: 997K on disk, 905,630 bytes text/data/other
parser support object: 408K on disk, 235,023 bytes text/data/other
semantic AST object: 54K on disk, 24,478 bytes text/data/other
lexer object: 74K on disk, 39,564 bytes text/data/other
libmylite_parser.a: 1.6M on disk
mylite-parse: 1.3M on disk
mylite-parser-bench: 1.3M on disk
```

## Next Work

- Replace temporary recognizer placeholder roots with real grammar productions
  or explicit typed placeholder statements.
- Expand `MyliteSemanticAst` beyond statement/target/expression nodes into
  concrete `CREATE TABLE` column, key, table-option, data-type, and expression
  metadata nodes.
- Normalize parser-derived DDL summaries into MySQL metadata-ready structures,
  including generated names for unnamed constraints and SQL-mode-sensitive
  literal/expression handling.
- Extend the `ALTER TABLE` view into partition action payloads,
  generated/check/default expression descriptors, position clauses, validation
  clauses, and final metadata operations.
- Add typed AST nodes for the next analyzer statement families underneath the
  statement classification and indexed target descriptor layer.
- Split the parser-level `SELECT` view beyond the current semantic clause nodes
  into semantic query-expression, query-block, table-reference, and projection
  objects with scoped name resolution.
- Extend executable-statement parser views into the next high-value DML and
  utility statements, reusing the expression-view infrastructure where
  statement payloads carry targets, assignments, predicates, ordering, and
  limits.
- Decide whether syntax-only builds should use a separate no-action generated
  parser if the action overhead matters.
- Add tree-shape tests for representative DDL, DML, expressions, stored
  programs, and utility statements as typed nodes are introduced.
