# Baseline Constant ORDER BY Keys

## Status

This slice accepts constant `ORDER BY` keys in supported `SELECT` statements
and treats them as no-op ordering terms:

```sql
SELECT column FROM source ORDER BY NULL;
SELECT column FROM source ORDER BY 'constant' DESC;
SELECT column FROM source ORDER BY NULL, real_order_key;
```

It builds on the existing descriptor-backed `SELECT ORDER BY` planner and the
parser-corpus SELECT clause residual slice.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MySQL 8.4 Reference Manual, `SELECT` syntax:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_parser_corpus_select_clause_residuals_expectations.sh`

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes verify:

- `ORDER BY NULL` is accepted on a table-backed `SELECT` and does not sort by a
  descriptor column.
- `ORDER BY 'a' DESC` is accepted and the direction on the constant key does
  not change row ordering.
- When a constant key is followed by a real key, the constant does not affect
  ordering and the real key determines the sort.

## Supported Surface

Supported constant keys:

```sql
ORDER BY NULL [ASC | DESC]
ORDER BY 'ordinary string literal' [ASC | DESC]
```

These keys are supported in the same `SELECT` envelopes that already accept
descriptor, alias, ordinal, or documented expression order keys. A constant key
may appear alone or before/after other already-supported order keys.

The MyLite Lemon grammar for this slice is:

```lemon
select_order_key ::= NULL.
select_order_key ::= STRING.
```

## Runtime Semantics

The order planner recognizes `NULL` and ordinary string-literal order keys as
constant no-op keys. It does not append them to the planned order list. If all
order keys are constants, MyLite emits no SQLite `ORDER BY`. If a clause mixes
constant keys with real supported keys, MyLite emits only the real keys in their
original relative order.

This keeps SQLite responsible for real sorting while avoiding MyLite-side row
materialization. No SQLite fork hook is needed.

## Non-Goals

This slice does not add:

- general expression order keys;
- numeric constants beyond existing ordinal behavior;
- boolean, binary, introduced, hex, bit, decimal, float, temporal, parameter,
  system-variable, or function constant order keys;
- MySQL optimizer behavior tied to `ORDER BY NULL`;
- deterministic ordering when all order keys are constants; or
- new catalog, file-format, public ABI, VFS, or SQLite fork changes.

Unsupported forms continue to use deterministic MyLite diagnostics.

## Validation

Coverage includes:

- MySQL 8.4.9 expectation rows for parser-corpus `ORDER BY NULL` and
  `ORDER BY 'a' DESC` probes;
- fast C runtime movement for those parser-corpus probes; and
- focused runtime coverage that constant keys are skipped while later
  descriptor keys still sort.
