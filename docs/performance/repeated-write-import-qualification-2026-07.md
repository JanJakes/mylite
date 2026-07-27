# Repeated-Write and Import Qualification, July 2026

## Objective

Close the largest remaining large-dataset performance gap: retained one-row
writes and bulk import. The work covers transaction/catalog overhead, retained
INSERT execution, multi-row statements, foreign-key checks, and `LOAD DATA`
parsing. Comparisons use the bundled SQLite build, equivalent logical data,
explicit transactions, alternating engine order, and result or affected-row
verification.

## Completed Work

1. Reuse one catalog snapshot synchronization for an active user transaction.
2. Coalesce repeated table `updated_time` writes within the same wall-clock
   second, with rollback and cross-handle invalidation.
3. Retain prepared INSERT analysis, lowered SQL, row/value storage, parameter
   conversion state, and command results across resets.
4. Reuse INSERT plans across safe parameter type changes while regenerating all
   dynamic values and diagnostics.
5. Execute simple one-row and multi-row INSERTs with SQLite statement atomicity
   when no MyLite-owned multi-statement behavior requires a savepoint.
6. Lower simple multi-row INSERTs to one SQLite statement, with the established
   row-at-a-time path retained as the constraint-diagnostic fallback.
7. Cache immutable parent lookup SQL and child-column mappings for complex
   foreign-key paths.
8. Lower a simple non-self-referencing child INSERT to one guarded SQLite
   statement. The parent lookup and insert share one snapshot; zero inserted
   rows produce the MySQL no-referenced-row diagnostic.
9. Borrow already-valid prepared text bindings and `LOAD DATA` text fields
   through the immediate SQLite step rather than copying them into transient
   planning storage.
10. Reuse the `LOAD DATA` converted-value array across rows and avoid a second
    decode allocation for fields without escapes.
11. Add profiler counters for retained DML plans, allocations, statement-cache
    use, and the complete large-dataset seed.
12. Add deterministic zero-index and five-index `LOAD DATA` scenarios to the
    large-dataset smoke and qualification suite.

Self-referencing foreign keys remain on post-insert validation because the new
row may satisfy its own reference. `IGNORE`, `REPLACE`, duplicate-key updates,
generated auto-increment values, and constraint failures retain their
specialized paths. These boundaries are correctness requirements, not missed
fast paths.

## Mixed Repeated-Write Results

The seed workload inserts accounts, indexed items, item-tag bridges, upsert
targets, composite keys, and foreign-key fan-out support rows with retained
one-row statements inside transactions.

| Fact rows | Logical writes | Revision | MyLite | SQLite | MyLite/write | Ratio |
| ---: | ---: | --- | ---: | ---: | ---: | ---: |
| 100K | 403,399 | Before | 31.78 s | 2.81 s | 78.8 us | 11.29x |
| 100K | 403,399 | Final | 14.72 s | 2.31 s | 36.5 us | 6.37x |
| 1M | 4,030,399 | Before | 357.31 s | 46.33 s | 88.7 us | 7.71x |
| 1M | 4,030,399 | Final | 174.18 s | 47.45 s | 43.2 us | 3.67x |

MyLite wall time improved 53.7% at 100K and 51.3% at 1M. The final per-write
cost remains bounded as cardinality grows.

The final profiled 100K load performed only 11 DML plans for 403,399 writes,
with 403,388 plan hits, 298 instrumented allocations, and 403,748 SQLite steps.
SQLite stepping accounted for 12.49 of 14.80 profiled seconds. Planning,
catalog synchronization, allocation, and multi-program FK validation are no
longer the dominant cost.

## `LOAD DATA` Results

Each sample imports a deterministic three-integer TSV file in one transaction
and rolls it back after timing. The SQLite baseline reads the same file and
uses one retained native INSERT VM. Three measured samples follow one warmup.

| Rows | Indexes | MyLite | SQLite | MyLite/row | SQLite/row | Ratio |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 100K | 0 | 252.4 ms | 113.6 ms | 2.52 us | 1.14 us | 2.22x |
| 1M | 0 | 2.589 s | 1.125 s | 2.59 us | 1.13 us | 2.30x |
| 100K | 5 | 1.559 s | 1.084 s | 15.59 us | 10.84 us | 1.44x |
| 1M | 5 | 51.676 s | 34.876 s | 51.68 us | 34.88 us | 1.48x |

The indexed cost rises for both engines as the five index trees deepen. The
ratio remains stable, so there is no MyLite-specific scaling defect. Heap
import retains about 1.4 microseconds/row of parser, MySQL conversion, and
diagnostic overhead.

## Verification

The native regression suite covers catalog snapshot refresh, timestamp
coalescing and rollback, retained-plan invalidation, changing parameter types,
multi-row constraint fallback, string/numeric coercion, all INSERT modifiers,
foreign-key variants, self-reference, and `LOAD DATA` diagnostics. A trace
regression requires a simple child insert to use one guarded physical INSERT
and no standalone parent lookup.

The large-dataset smoke executes both new import scenarios for both engines and
checks their affected-row hashes. Final qualification includes 100K and 1M
mixed loads plus 100K and 1M heap/indexed imports.

## Residual Cost

The remaining mixed-load ratio is mostly SQLite VM work for MySQL-compatible
physical indexes, collations, and guarded foreign-key statements. Direct
SQLite uses its native schema and constraint implementation. Removing that
work would change compatibility or physical invariants.

Batching remains the highest-leverage application-controlled option: a
ten-row INSERT amortizes the public statement lifecycle and uses one SQLite
step. For one-row statements, this work removes the avoidable repeated
planning, catalog, allocation, and transaction programs. No remaining
write/import result shows superlinear MyLite behavior or an unprofiled
compatibility-layer bottleneck.
