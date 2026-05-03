# Parser AST Construction

The parser now has two modes:

- Syntax-only validation through `mylite_parse_sql()`.
- Generic tree construction through `mylite_parse_sql_ast()`.

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

The tree is not yet the final semantic MyLite AST. It is a complete generated
parse tree with a typed statement classification and target descriptor layer
suitable for measuring cost and for guiding typed-node work.

The semantic `CREATE TABLE` view is the first statement-level AST object layered
on top of the descriptor work. It does not copy column, key, or option
descriptors; instead it gives the next builder a stable object anchored to
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
```

## Benchmark

Prepare a NUL-separated corpus from the WordPress CSV, then run:

```sh
build-perf/mylite-parser-bench /tmp/mylite-parser-corpus.nul syntax 100
build-perf/mylite-parser-bench /tmp/mylite-parser-corpus.nul ast 100
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

Before semantic actions were generated, syntax-only parsing measured about
`711k queries/sec` on the same corpus. The current syntax-only path still runs
the generated reduce actions, but with AST building disabled, so it avoids arena
allocation while paying some action-call overhead.

Current release build size on the same machine:

```text
generated parser C: 72,852 lines, 5,637,339 bytes
generated parser object: 996K on disk, 905,398 bytes text/data/other
parser support object: 291K on disk, 170,330 bytes text/data/other
lexer object: 74K on disk, 39,564 bytes text/data/other
libmylite_parser.a: 1.4M on disk
mylite-parse: 1.2M on disk
```

## Next Work

- Replace temporary recognizer placeholder roots with real grammar productions
  or explicit typed placeholder statements.
- Extend semantic `CREATE TABLE` AST nodes from the current view handles into
  concrete column, key, table-option, data-type, and semantic expression
  metadata nodes.
- Normalize parser-derived DDL summaries into MySQL metadata-ready structures,
  including generated names for unnamed constraints and SQL-mode-sensitive
  literal/expression handling.
- Extend the `ALTER TABLE` view into partition action payloads,
  generated/check/default expression descriptors, position clauses, validation
  clauses, and final metadata operations.
- Add typed AST nodes for the next analyzer statement families underneath the
  statement classification and indexed target descriptor layer.
- Split the parser-level `SELECT` view into semantic query-expression,
  query-block, table-reference, projection, and clause objects with scoped name
  resolution.
- Extend executable-statement parser views into the next high-value DML and
  utility statements, reusing the expression-view infrastructure where
  statement payloads carry targets, assignments, predicates, ordering, and
  limits.
- Decide whether syntax-only builds should use a separate no-action generated
  parser if the action overhead matters.
- Add tree-shape tests for representative DDL, DML, expressions, stored
  programs, and utility statements as typed nodes are introduced.
