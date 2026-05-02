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
- Rule nodes carry generated rule IDs, symbol names, children, and aggregate
  spans.
- Top-level statement views classify each parsed statement and expose stable
  statement spans for the analyzer.
- Statement target descriptors expose all currently known targets for
  multi-target statements such as `DROP TABLE`, `RENAME TABLE`, multi-table
  `DELETE`, and joined `UPDATE`.
- `CREATE TABLE` statements expose typed column descriptors for direct column
  definitions, including definition, name, type, option spans, coarse type
  family, and column option flags.
- `CREATE TABLE` statements expose typed table key and constraint descriptors
  for primary keys, secondary indexes, unique indexes, fulltext indexes, spatial
  indexes, foreign keys, and check constraints.
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
  span
- typed `CREATE TABLE` column descriptors with definition, name, type, and
  options spans, type family, and option flags
- typed `CREATE TABLE` key descriptors with kind, full constraint/index span,
  constraint name, key name, local key parts, referenced table, referenced
  schema/name, and referenced key parts

The original single-target accessors remain compatibility helpers and mirror
the first indexed target. The indexed API should be used by new analyzer and
typed-AST code so statements that naturally mention several targets do not lose
information. Target roles are currently:

- `primary` for ordinary DDL/DML table, database, view, routine, account, or
  variable targets
- `source` and `destination` for each `RENAME TABLE source TO destination` pair

The tree is not yet the final semantic MyLite AST. It is a complete generated
parse tree with a typed statement classification and target descriptor layer
suitable for measuring cost and for guiding typed-node work.

The `CREATE TABLE` column view is the first typed statement-specific layer. It
now classifies the coarse type family (`numeric`, `string`, `temporal`, `json`,
`enum`, `set`, or `spatial`) and exposes flags for common column attributes:
nullability, defaults, auto-increment, inline key markers, comments, generated
columns, `ON UPDATE`, references, checks, unsigned, zerofill, character set, and
collation. It does not yet classify exact MySQL data type names, default
expressions, table options, partitions, or `CREATE TABLE ... SELECT`.

The `CREATE TABLE` key view covers table-level primary keys, indexes, unique
keys, fulltext keys, spatial keys, foreign keys, and check constraints. It
preserves both constraint names and key names because MySQL syntax can carry
both independently, and it records foreign-key referenced table and referenced
columns. It does not yet classify index options, key-part order/prefix lengths,
foreign-key match/update/delete actions, check expressions, or generated
constraint names.

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
mode=syntax queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=14.022573 qps=495922 mbps=37.72 avg_us=2.016
mode=ast queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=20.866257 qps=333270 mbps=25.35 avg_us=3.001 avg_nodes=74.5 avg_ast_bytes=9892.5 avg_statements=1.00 avg_targets=0.59 avg_columns=0.29 avg_keys=0.06 avg_key_columns=0.09
```

Before semantic actions were generated, syntax-only parsing measured about
`711k queries/sec` on the same corpus. The current syntax-only path still runs
the generated reduce actions, but with AST building disabled, so it avoids arena
allocation while paying some action-call overhead.

Current release build size on the same machine:

```text
generated parser C: 72,852 lines, 5,637,339 bytes
generated parser object: 996K on disk, 905,398 bytes text/data/other
parser support object: 64K on disk, 45,259 bytes text/data/other
lexer object: 74K on disk, 39,564 bytes text/data/other
libmylite_parser.a: 1.1M on disk
mylite-parse: 995K on disk
```

## Next Work

- Replace temporary recognizer placeholder roots with real grammar productions
  or explicit typed placeholder statements.
- Expand `CREATE TABLE` typed descriptors to classify exact data types,
  defaults, generated expressions, index options, foreign-key actions, check
  expressions, and table options.
- Add typed AST nodes for the next analyzer statement families underneath the
  statement classification and indexed target descriptor layer.
- Decide whether syntax-only builds should use a separate no-action generated
  parser if the action overhead matters.
- Add tree-shape tests for representative DDL, DML, expressions, stored
  programs, and utility statements as typed nodes are introduced.
