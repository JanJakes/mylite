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
  and parser-level value metadata for defaults, `ON UPDATE`, and comments.
- `CREATE TABLE` statements expose typed table key and constraint descriptors
  for primary keys, secondary indexes, unique indexes, fulltext indexes, spatial
  indexes, foreign keys, and check constraints.
- Key descriptors include key-part kind, expression, prefix length, ordering,
  decoded constraint/key/key-part/referenced-table/referenced-column names,
  index type/options, key-level index type, visibility, decoded key-option
  values, foreign-key match/actions, check expression, and check enforcement
  spans.
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
- `CREATE INDEX`, `DROP TABLE`, and `RENAME TABLE` statements expose semantic
  DDL views that reuse the same decoded identifier, target, key-part, and
  key-option infrastructure.
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
  storage mode, and option flags
- typed `CREATE TABLE` key descriptors with kind, full constraint/index span,
  constraint name, key name, local key parts, referenced table, referenced
  schema/name, referenced key parts, index options, key-level index type,
  visibility, decoded key-option values, foreign actions, and check expression
  details, plus decoded values for identifier-bearing fields
- typed `CREATE TABLE` table-option descriptors with kind, full option span,
  option-name span, value span, value kind, decoded string/identifier values,
  parsed unsigned integers, and list/raw payload spans
- typed `CREATE INDEX` descriptors with index name, target table, key parts,
  index type, visibility, decoded key-option values, and compact key-option
  summaries
- typed `DROP TABLE` descriptors with temporary/`IF EXISTS` flags and decoded
  table target lists
- typed `RENAME TABLE` descriptors with decoded source/destination table pairs

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
raw expression text. The next layer must resolve exact MySQL type semantics,
SQL-mode-sensitive expression and literal values, expression trees, metadata
defaults, partitions, and `CREATE TABLE ... SELECT`.

Column descriptors keep direct CST node anchors for the type node, option list,
default value, `ON UPDATE` value, generated expression/storage, comment option,
inline check expression/enforcement, and inline reference. These anchors let the
next semantic AST builder walk the relevant subtree directly instead of
searching the whole column definition again. The semantic `CREATE TABLE` view
now exposes those column details through the column handle, including nested
type-element handles for `ENUM` and `SET`, typed default and `ON UPDATE` value
metadata, declared nullability, generated storage mode, and decoded option
values.

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
check constraints expose expression and enforcement spans. The view does not
yet normalize these details into final metadata records or generate missing
constraint names. The semantic `CREATE TABLE` view now exposes key-part and
key-option handles directly from each key handle, so typed AST construction no
longer needs to re-index through the statement-level compatibility accessors.

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
the analyzer without requiring expression trees. `CREATE INDEX` reuses the
table-key descriptors for key parts and key options while adding the decoded
index name and target table. `DROP TABLE` exposes `TEMPORARY`, `IF EXISTS`, and
all table targets. `RENAME TABLE` exposes each source/destination pair as
decoded target values. These views are still parser-level metadata: they do not
perform existence checks, generate implicit names, validate engine-specific
rules, or apply runtime effects.

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
mode=syntax queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=14.301015 qps=486266 mbps=36.98 avg_us=2.056
mode=ast queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=22.964486 qps=302820 mbps=23.03 avg_us=3.302 avg_nodes=74.5 avg_ast_bytes=10208.3 avg_statements=1.00 avg_targets=0.59 avg_target_schema_values=0.02 avg_target_name_values=0.59 avg_columns=0.29 avg_keys=0.06 avg_create_table_views=0.13 avg_create_table_view_schema_values=0.00 avg_create_table_view_name_values=0.13 avg_create_table_view_summary_engines=0.03 avg_create_table_view_summary_comments=0.00 avg_create_table_view_summary_auto_increments=0.00 avg_create_table_view_columns=0.29 avg_create_table_view_keys=0.06 avg_create_table_view_options=0.04 avg_create_table_view_column_handles=0.29 avg_create_table_view_known_column_types=0.29 avg_create_table_view_column_type_numeric_params=0.11 avg_create_table_view_column_type_element_handles=0.05 avg_create_table_view_column_type_element_values=0.05 avg_create_table_view_column_type_lengths=0.09 avg_create_table_view_column_type_unsigned_attrs=0.01 avg_create_table_view_column_type_charset_values=0.01 avg_create_table_view_column_type_collation_values=0.00 avg_create_table_view_column_option_spans=0.12 avg_create_table_view_column_defaults=0.05 avg_create_table_view_column_default_values=0.05 avg_create_table_view_column_default_unsigned_values=0.00 avg_create_table_view_column_on_update_values=0.00 avg_create_table_view_column_comments=0.00 avg_create_table_view_column_comment_values=0.00 avg_create_table_view_column_checks=0.00 avg_create_table_view_column_nullabilities=0.07 avg_create_table_view_column_generated_storage_kinds=0.00 avg_create_table_view_column_type_nodes=0.29 avg_create_table_view_column_options_nodes=0.12 avg_create_table_view_key_handles=0.06 avg_create_table_view_named_keys=0.02 avg_create_table_view_key_index_types=0.00 avg_create_table_view_key_visibilities=0.00 avg_create_table_view_key_comments=0.00 avg_create_table_view_key_parsers=0.00 avg_create_table_view_key_block_sizes=0.00 avg_create_table_view_key_column_handles=0.09 avg_create_table_view_named_key_columns=0.09 avg_create_table_view_ordered_key_columns=0.00 avg_create_table_view_prefixed_key_columns=0.00 avg_create_table_view_expression_key_columns=0.00 avg_create_table_view_referenced_column_handles=0.00 avg_create_table_view_named_referenced_columns=0.00 avg_create_table_view_key_option_handles=0.00 avg_create_table_view_key_option_values=0.00 avg_create_table_view_key_option_identifier_values=0.00 avg_create_table_view_key_option_string_values=0.00 avg_create_table_view_key_option_unsigned_integer_values=0.00 avg_create_table_view_key_option_index_type_values=0.00 avg_create_table_view_option_handles=0.04 avg_create_table_view_option_values=0.04 avg_create_table_view_option_identifier_values=0.04 avg_create_table_view_option_string_values=0.00 avg_create_table_view_option_unsigned_integer_values=0.00 avg_create_table_view_option_list_values=0.00 avg_create_index_views=0.00 avg_create_index_name_values=0.00 avg_create_index_table_name_values=0.00 avg_create_index_columns=0.00 avg_create_index_options=0.00 avg_create_index_comments=0.00 avg_create_index_key_block_sizes=0.00 avg_drop_table_views=0.02 avg_drop_table_tables=0.03 avg_drop_table_if_exists=0.00 avg_rename_table_views=0.00 avg_rename_table_pairs=0.00 avg_key_constraint_name_values=0.00 avg_key_name_values=0.02 avg_key_referenced_table_schema_values=0.00 avg_key_referenced_table_name_values=0.00 avg_key_columns=0.09 avg_key_column_name_values=0.09 avg_key_referenced_column_name_values=0.00 avg_key_options=0.00 avg_options=0.04 avg_column_name_values=0.29 avg_column_defaults=0.05 avg_column_on_updates=0.00 avg_column_generated=0.00 avg_column_checks=0.00 avg_column_references=0.00 avg_column_known_types=0.29 avg_column_storage_classes=0.29 avg_column_type_numeric_params=0.11 avg_column_type_elements=0.05 avg_column_type_element_values=0.05 avg_column_type_lengths=0.09 avg_column_type_precisions=0.01 avg_column_type_scales=0.01 avg_column_type_fsps=0.01 avg_column_type_unsigned_attrs=0.01 avg_column_type_zerofill_attrs=0.00 avg_column_type_binary_attrs=0.00 avg_column_type_charsets=0.01 avg_column_type_collations=0.00 avg_column_value_roots=0.06
```

Before semantic actions were generated, syntax-only parsing measured about
`711k queries/sec` on the same corpus. The current syntax-only path still runs
the generated reduce actions, but with AST building disabled, so it avoids arena
allocation while paying some action-call overhead.

Current release build size on the same machine:

```text
generated parser C: 72,852 lines, 5,637,339 bytes
generated parser object: 996K on disk, 905,398 bytes text/data/other
parser support object: 157K on disk, 95,163 bytes text/data/other
lexer object: 74K on disk, 39,564 bytes text/data/other
libmylite_parser.a: 1.2M on disk
mylite-parse: 1.1M on disk
```

## Next Work

- Replace temporary recognizer placeholder roots with real grammar productions
  or explicit typed placeholder statements.
- Extend semantic `CREATE TABLE` AST nodes from the current view handles into
  concrete column, key, table-option, default/generated/check expression, and
  data-type metadata nodes.
- Normalize parser-derived DDL summaries into MySQL metadata-ready structures,
  including generated names for unnamed constraints and SQL-mode-sensitive
  literal/expression handling.
- Add typed `ALTER TABLE` views for table targets, operation lists, column
  changes, index changes, constraint changes, table options, partition clauses,
  and validation/algorithm/lock clauses.
- Add typed AST nodes for the next analyzer statement families underneath the
  statement classification and indexed target descriptor layer.
- Decide whether syntax-only builds should use a separate no-action generated
  parser if the action overhead matters.
- Add tree-shape tests for representative DDL, DML, expressions, stored
  programs, and utility statements as typed nodes are introduced.
