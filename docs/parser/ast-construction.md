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

The tree is not yet the final semantic MyLite AST. It is a complete generated
parse tree suitable for measuring cost and for guiding typed-node work.

## Benchmark

Prepare a NUL-separated corpus from the WordPress CSV, then run:

```sh
build-perf/mylite-parser-bench /tmp/mylite-parser-corpus.nul syntax 100
build-perf/mylite-parser-bench /tmp/mylite-parser-corpus.nul ast 100
```

Release benchmark result on May 1, 2026:

```text
mode=syntax queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=14.390437 qps=483245 mbps=36.75 avg_us=2.069
mode=ast queries=69541 iterations=100 parsed=6954100 failed=0 elapsed=17.580908 qps=395548 mbps=30.08 avg_us=2.528 avg_nodes=74.5 avg_ast_bytes=9817.5
```

Before semantic actions were generated, syntax-only parsing measured about
`711k queries/sec` on the same corpus. The current syntax-only path still runs
the generated reduce actions, but with AST building disabled, so it avoids arena
allocation while paying some action-call overhead.

## Next Work

- Replace temporary recognizer placeholder roots with real grammar productions
  or explicit typed placeholder statements.
- Add typed AST nodes for the analyzer's first statement families instead of
  exposing TiDB production details directly.
- Decide whether syntax-only builds should use a separate no-action generated
  parser if the action overhead matters.
- Add tree-shape tests for representative DDL, DML, expressions, stored
  programs, and utility statements as typed nodes are introduced.
