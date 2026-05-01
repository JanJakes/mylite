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

The tree is not yet the final semantic MyLite AST. It is a complete generated
parse tree with a typed statement classification and target descriptor layer
suitable for measuring cost and for guiding typed-node work.

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

Release benchmark result on May 1, 2026:

```text
mode=syntax queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=14.432263 qps=481844 mbps=36.64 avg_us=2.075
mode=ast queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=18.627941 qps=373316 mbps=28.39 avg_us=2.679 avg_nodes=74.5 avg_ast_bytes=9842.2 avg_statements=1.00 avg_targets=0.59
```

Before semantic actions were generated, syntax-only parsing measured about
`711k queries/sec` on the same corpus. The current syntax-only path still runs
the generated reduce actions, but with AST building disabled, so it avoids arena
allocation while paying some action-call overhead.

## Next Work

- Replace temporary recognizer placeholder roots with real grammar productions
  or explicit typed placeholder statements.
- Add typed AST nodes for the analyzer's first statement families underneath
  the statement classification and target descriptor layer.
- Expand target descriptors for multi-target statements such as `DROP TABLE`,
  `RENAME TABLE`, multi-table `DELETE`, and joined `UPDATE`.
- Decide whether syntax-only builds should use a separate no-action generated
  parser if the action overhead matters.
- Add tree-shape tests for representative DDL, DML, expressions, stored
  programs, and utility statements as typed nodes are introduced.
