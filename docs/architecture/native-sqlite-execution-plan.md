# Native SQLite execution plan

## Status

This is an architecture planning document. It does not mark any MySQL feature as
supported and does not change the compatibility matrix.

The goal is to move MyLite toward more native SQLite execution without giving up
MySQL semantics. "Native SQLite execution" means SQLite should do as much row
access, joining, sorting, grouping, aggregation, windowing, and expression
evaluation as it safely can. It does not mean SQLite's type system, collation
rules, diagnostics, or metadata become authoritative.

## Sources

- SQLite, Datatypes In SQLite:
  https://www.sqlite.org/datatype3.html
- SQLite, Application-Defined SQL Functions:
  https://www.sqlite.org/appfunc.html
- SQLite, Function Flags:
  https://www.sqlite.org/c3ref/c_deterministic.html
- SQLite, Define New Collating Sequences:
  https://www.sqlite.org/c3ref/create_collation.html
- SQLite, Virtual Tables:
  https://www.sqlite.org/vtab.html
- SQLite, Generated Columns:
  https://www.sqlite.org/gencol.html
- SQLite, Indexes On Expressions:
  https://www.sqlite.org/expridx.html
- SQLite, STRICT Tables:
  https://www.sqlite.org/stricttables.html
- SQLite, SQL function subtypes:
  https://www.sqlite.org/c3ref/result_subtype.html
- SQLite, Stale Expression Indexes:
  https://www.sqlite.org/staleexpridx.html
- MySQL 8.4 Reference Manual, Type Conversion in Expression Evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- MySQL 8.4 Reference Manual, Collation Coercibility in Expressions:
  https://dev.mysql.com/doc/refman/8.4/en/charset-collation-coercibility.html
- Existing MyLite specs:
  - `docs/specs/sqlite-plumbing/specs.md`
  - `docs/specs/expression-operator-foundation/specs.md`
  - `docs/specs/character-set-collation-foundation/specs.md`
  - `docs/specs/numeric-column-types/specs.md`
  - `docs/specs/select-table-core/specs.md`

This document is independently authored from official SQLite documentation,
official MySQL documentation, observed MySQL behavior already recorded in MyLite
specs, and the current MyLite codebase. It does not copy MySQL implementation
sources or restrictively licensed compatibility-layer code.

## Current direction

MyLite already has the right layering for a native SQLite execution strategy:

- SQLite is the durable storage and low-level execution engine.
- MyLite owns parsing, analysis, metadata, diagnostics, and MySQL compatibility.
- Parser output should feed later analysis and should not directly lower SQL to
  SQLite.
- Result metadata already flows through MyLite descriptors rather than relying
  only on SQLite metadata.

Relevant current implementation anchors:

- `mylite_field_descriptor` tracks logical result type, flags, length,
  decimals, charset id, and nullability.
- `mylite_expression_descriptor_*` helpers already infer many expression result
  descriptors.
- `mylite_select_plan` is the natural place to store resolved tables, outputs,
  clauses, and later translation capabilities.
- The existing generic SQLite translator is intentionally narrow. It should not
  grow into an ad hoc string rewriter.

The architecture should therefore evolve the current plan/analyzer boundary,
not replace it.

## Recommended direction

Use a descriptor-driven hybrid translator.

Every SQL statement should go through:

1. Parse MySQL syntax into a MyLite AST.
2. Analyze names, scopes, expression types, collations, SQL modes, function
   semantics, diagnostics, and metadata.
3. Build a MyLite logical plan with descriptors for every output, predicate,
   ordering expression, grouping expression, aggregate, window expression, and
   DML assignment.
4. Classify each operation by whether SQLite can execute it directly,
   execute it with MyLite hooks, execute it with an indexed helper value, or
   must fall back to MyLite runtime code.
5. Lower only the safe portions to SQLite SQL.
6. Execute through SQLite, with MyLite functions, collations, virtual tables, or
   fallback row processing where needed.
7. Return MySQL-compatible rows, metadata, diagnostics, warnings, affected-row
   counts, insert ids, and side effects from MyLite-owned state.

The important rule is that SQLite can execute a plan, but it cannot decide what
the plan means. MyLite analysis must decide that first.

## Translation safety classes

Each expression or clause should have a translation class. These classes are
not user-visible. They guide plan lowering and testing.

### Class 0: direct SQLite

Use direct SQLite syntax when MySQL and SQLite semantics are proven equivalent
for the resolved descriptors and SQL mode.

Examples:

- signed integer equality against a pre-coerced signed integer parameter
- simple column projection where storage encoding matches output needs
- rowid or primary-key lookup where MySQL and SQLite nullability and range
  behavior match
- simple `LIMIT` and `OFFSET` after MySQL syntax and range handling

This class preserves index use and should be the fast path.

### Class 1: decorated SQLite

Use SQLite syntax with explicit MyLite-controlled decorations when direct
syntax is almost equivalent but needs resolved metadata.

Examples:

- `ORDER BY name COLLATE mylite_utf8mb4_0900_ai_ci`
- explicit casts or literal coercions inserted by MyLite
- generated aliases or hidden projections for MySQL-compatible `ORDER BY`
- direct aggregate or window calls where argument types and null behavior are
  proven compatible

This class can still use SQLite indexes when the emitted expression and
collation match an index.

### Class 2: MyLite hook

Lower to MyLite scalar, aggregate, window, comparison, cast, or key-generation
functions registered with SQLite.

Examples:

```sql
WHERE _mylite_eq_string(name, ?, 255)
ORDER BY _mylite_weight_string(name, 255)
SELECT _mylite_decimal_add(price, tax, 10, 2)
```

This class keeps SQLite in charge of scanning, joining, grouping, and sorting,
but MyLite owns the semantic operation. It is a correctness fallback for many
cases. It often blocks ordinary index use unless paired with expression indexes
or generated helper columns.

Specialized typed functions are usually preferable to one fully generic
`_mylite_equals(...)` function. Generic dispatch is useful as a bring-up path,
but typed hooks make capability checks, indexing, diagnostics, and performance
clearer.

### Class 3: indexed helper value

Use generated columns, shadow columns, expression indexes, or sidecar tables to
materialize a MySQL-compatible search, sort, or grouping key.

Examples:

- decimal numeric sort key
- collation weight string
- temporal normalized key
- JSON path extraction key
- case/accent-insensitive text key for a specific collation

This class is the performance bridge for semantics SQLite cannot natively
index. It introduces maintenance cost and must be used selectively.

### Class 4: MyLite runtime fallback

Use MyLite's own rowset or statement runtime when SQLite pushdown would change
observable behavior.

Examples:

- expressions with user variables or side effects
- operations where warning timing depends on MySQL evaluation order
- SQL modes whose exact behavior is not yet modeled in translation
- unsupported collation or character set conversion
- optimizer-sensitive cases where SQLite might reorder evaluation in a way that
  changes MySQL-visible diagnostics
- any operation not covered by MySQL-runtime-verified tests

Fallback should be explicit, tested, and treated as a compatibility boundary,
not as a silent failure of the translator.

## Type system model

SQLite does not provide custom persisted MySQL-style types. A value in SQLite
has one of SQLite's storage classes, and column declarations primarily determine
affinity. SQLite `STRICT` tables add useful enforcement for SQLite's own type
names, but they do not create MySQL `DECIMAL`, `DATETIME`, `ENUM`, unsigned
integer, character-set, or collation semantics.

MyLite should therefore keep a two-layer model:

1. Logical MySQL descriptor.
2. Physical SQLite encoding.

The logical descriptor is authoritative. It carries MySQL type, flags,
nullability, length, decimals, charset id, collation id, coercibility,
temporal precision, enum/set metadata, JSON state, and any other MySQL-facing
metadata needed by analysis, execution, and public APIs.

The physical encoding is an implementation detail. It records how values are
stored in SQLite and which operations are safe on that encoding.

### Storage encoding candidates

The specific encodings should be specified per type feature, but the likely
direction is:

| MySQL domain | Candidate SQLite encoding | Notes |
| --- | --- | --- |
| `TINYINT` to signed `BIGINT` | `INTEGER` | Enforce range and warnings on write and cast. |
| unsigned integers | `INTEGER` where representable; special encoding above `INT64_MAX` | Full unsigned `BIGINT` cannot be represented as SQLite signed integer. |
| `DECIMAL` | canonical text, scaled integer, or BCD blob | Do not use `REAL` for exact values. Add comparison and arithmetic hooks. |
| `FLOAT` / `DOUBLE` | `REAL` | Preserve MySQL conversion, invalid value, and metadata behavior in MyLite. |
| `DATE` / `TIME` / `DATETIME` / `TIMESTAMP` | canonical text or integer microsecond key | Must preserve zero-date and incomplete-date behavior where SQL mode allows it. |
| `YEAR` | `INTEGER` | Coercion rules remain MySQL-owned. |
| `CHAR` / `VARCHAR` / `TEXT` | `TEXT` | Store normalized connection/column charset representation and use MySQL collations. |
| `BINARY` / `VARBINARY` / `BLOB` | `BLOB` | Compare bytewise, not through text collation. |
| `JSON` | canonical text or binary blob | Validate and normalize through MyLite JSON functions. |
| `ENUM` | string plus metadata, or integer code plus lookup metadata | MySQL comparison and display rules decide final encoding. |
| `SET` | bitset plus metadata, or normalized string plus metadata | Needs MySQL-compatible coercion and display. |
| spatial | deferred | Likely requires dedicated blob encoding and functions. |

Write paths must coerce before storage. Read paths must expose MySQL values and
metadata regardless of SQLite's runtime storage class.

## Expression types and casts

Expression lowering must happen after MyLite resolves result descriptors and
comparison contexts. The same raw expression can mean different things in
different MySQL contexts.

Examples:

```sql
int_col = '42'
varchar_col = 42
decimal_col = double_col
date_col = '2024-01-01'
binary_col = text_col
name COLLATE utf8mb4_bin = other_name
```

A string-based translator cannot lower these correctly. The analyzer must
attach:

- operand descriptors
- comparison domain
- result descriptor
- collation and coercibility result
- SQL mode dependencies
- warning behavior
- whether native SQLite evaluation is safe

Then the translator chooses among native SQL, casts, collations, typed MyLite
functions, materialized helper keys, or fallback runtime code.

`WHERE x = y` can be lowered to a MyLite comparator, but that should be a
fallback or a typed special case, not the default for all equality. Wrapping an
indexed column in `_mylite_equals(x, ?)` usually prevents ordinary index use.
The preferred order is:

1. pre-coerce constants and parameters, then emit native `x = ?` when safe
2. emit `x COLLATE mylite_collation = ?` when a SQLite collation captures the
   exact MySQL string comparison
3. emit typed functions such as `_mylite_eq_decimal(...)` or
   `_mylite_eq_temporal(...)` when native comparison is not safe
4. use generated or expression-indexed helper keys for hot paths
5. fall back to the MyLite runtime when warning timing or side effects matter

`<=>`, `IS`, `IS NOT`, `BETWEEN`, `IN`, row comparisons, and quantified
subquery comparisons need their own lowering rules. Plain `=` and `<=>` are not
interchangeable because `NULL` behavior is part of the operator semantics.

## Collations and character sets

SQLite custom collations are useful, but they are not a full MySQL character
set system.

Use registered SQLite collations when the resolved MySQL collation can be
implemented as a pure comparison callback over the stored text representation:

```sql
ORDER BY name COLLATE mylite_utf8mb4_0900_ai_ci
WHERE name COLLATE mylite_utf8mb4_0900_ai_ci = ?
```

This is the best path for index use because SQLite indexes can be created with
a collation and can satisfy matching comparisons or ordering.

Use weight-string or collation-key functions when a pure SQLite collation is
not enough, or when we need reusable sort/group keys:

```sql
ORDER BY _mylite_weight_string(name, 255)
```

For performance, hot collation keys may need generated or shadow columns:

```sql
_mylite_name_utf8mb4_0900_ai_ci_key
```

Open issues:

- MySQL collation coercibility must be resolved before translation.
- SQLite collation callbacks operate on SQLite text encodings, so MyLite must
  decide and enforce stored text representation.
- Character-set conversion, invalid byte sequences, pad attributes, binary
  strings, and repertoire rules remain MyLite-owned.
- User-visible `WEIGHT_STRING()`, `COLLATION()`, `CHARSET()`, and related
  functions should use the same internal registry and key builders.

## SQLite mechanisms we can use

### Application-defined functions

Use scalar, aggregate, and window functions for MySQL built-ins, casts,
operators, comparison helpers, JSON helpers, temporal functions, decimal
arithmetic, and internal sort-key generation.

Requirements:

- Register functions per SQLite connection.
- Keep internal function names private and reserved.
- Tag functions with SQLite flags accurately.
- Mark only truly deterministic functions as deterministic.
- Mark only audited harmless functions as innocuous.
- Do not expose internal helper functions as MySQL user-callable functions
  unless they are intentionally part of the MySQL surface.

Function subtypes can help pass small tags between MyLite functions inside one
SQLite expression, but they are not a durable or general type system. They
should not be used as the source of truth for table values or result metadata.

### Collations

Use SQLite collations for MySQL text ordering and equality whenever a collation
callback can exactly model the resolved MySQL collation over the stored text
encoding.

Indexes should be created with the same collation name that translated queries
emit. A query using a different explicit collation should not be assumed to use
the same index.

### Generated columns

Generated columns can materialize MyLite helper values, especially STORED sort
or search keys. They are useful for:

- collation weight keys
- decimal sort keys
- normalized temporal keys
- JSON path indexes
- generated values that must stay transactionally aligned with base rows

Limitations:

- generated column expressions are restricted to deterministic scalar
  functions and same-row data
- STORED generated columns cannot be added with a simple `ALTER TABLE ADD
  COLUMN`
- generated columns affect database compatibility with older SQLite versions
- the generated column's declared type and collation are separate from the
  expression that computes it

Generated columns should be an optimization tool, not a prerequisite for basic
correctness.

### Expression indexes

Expression indexes can make function-based rewrites indexable, for example:

```sql
CREATE INDEX ... ON t(_mylite_weight_string(name, 255));
```

They are attractive but fragile:

- SQLite matches expression indexes by expression text, not algebraic
  equivalence.
- Indexed functions must be deterministic.
- If MyLite changes the implementation of an indexed key function, affected
  indexes may need rebuilds.
- Dynamic session state must not affect expression-indexed functions.

This means MyLite needs stable canonical SQL generation for internal indexed
expressions and a versioning/reindex strategy for helper functions.

### STRICT tables

STRICT tables can catch some accidental SQLite type drift, but they are not a
MySQL type system. They may be useful for internal catalog tables or simple
physical encodings. They should not be relied on for MySQL coercion, warnings,
unsigned ranges, decimal exactness, temporal edge cases, or collations.

### Virtual tables

Virtual tables let SQLite delegate scans, constraint handling, updates, and
ordering to a MyLite module. They are powerful, but using them for ordinary user
tables would amount to implementing a storage engine inside SQLite.

Good uses:

- `information_schema`
- `performance_schema` placeholders
- `mysql.*` compatibility tables that are generated from MyLite metadata
- table-valued functions
- `JSON_TABLE`-style surfaces later
- introspection and internal debug tables
- specialized future access modules after profiling

Avoid as the default storage for ordinary user tables. Ordinary tables should
use SQLite b-trees first, with MyLite metadata, functions, collations, helper
keys, and fallback execution layered above them.

### Views and triggers

SQLite views and triggers may help implement some metadata and DML behavior,
but they are risky for compatibility if they hide MySQL diagnostics or warning
timing. They also interact with trusted-schema security and internal function
exposure.

Use them sparingly. Prefer explicit MyLite statement planning for user-visible
semantics until a behavior is proven equivalent.

### Hooks and authorizers

SQLite authorizer, update, preupdate, commit, rollback, trace, and progress
hooks can help with observability, invalidation, catalog maintenance, and
defensive checks. They should not be treated as the main semantic layer for
MySQL expression behavior.

Potential uses:

- invalidate prepared statement caches when schema changes
- track affected rows and row changes where SQLite semantics match MySQL
- maintain auxiliary metadata or helper indexes
- prevent accidental direct access to internal SQLite objects
- profile native translation coverage

### SQLite internals and custom types

Do not fork SQLite to add MySQL custom storage classes. SQLite's type model is
deeply tied to record format, VDBE values, affinity, comparison, sorting,
indexes, planner assumptions, and public APIs. A fork would increase
maintenance cost and could compromise ordinary SQLite tooling compatibility for
the SQLite payload inside `.mylite` files.

Targeted SQLite patches may still be justified for embedding constraints, file
offset handling, or proven performance issues. They should be narrow,
documented, and independent of MySQL type semantics.

## Options considered

### Keep the current custom runtime as the main engine

Benefits:

- maximum control over MySQL semantics
- easiest place to preserve warning timing and side effects
- no dependence on SQLite optimizer behavior

Costs:

- poor performance for scans, joins, sorts, grouping, and window operations
- duplicates work SQLite already does well
- increases MyLite implementation surface rapidly

Use this as fallback, not as the long-term default.

### Naive SQL-to-SQLite translation

Benefits:

- fast to implement for simple statements
- uses SQLite optimizer and indexes naturally

Costs:

- incorrect for type conversion, collations, metadata, diagnostics, warnings,
  temporal values, decimal values, SQL modes, and many functions
- errors appear as subtle compatibility bugs

Reject as a general strategy. Direct translation is acceptable only after
descriptor-driven safety checks.

### Descriptor-driven hybrid translation

Benefits:

- keeps MySQL semantics in MyLite
- lets SQLite execute safe row access and relational operators
- supports incremental optimization
- allows correctness-first fallback

Costs:

- requires a real analyzer, descriptor propagation, and capability classifier
- requires a disciplined test matrix
- must account for index use explicitly

This is the recommended direction.

### Virtual-table storage engine

Benefits:

- MyLite can own typing and comparisons behind SQLite's planner interface
- `xBestIndex` can consume constraints and ordering for custom access paths

Costs:

- effectively a new storage engine
- duplicates SQLite b-tree, transaction, constraint, and index behavior unless
  it wraps ordinary tables
- callback overhead and complexity are high

Use virtual tables for system and generated surfaces first. Revisit for user
tables only with profiling data and a specific unsolved problem.

### SQLite fork with custom types

Benefits:

- could theoretically make MySQL types visible deeper in SQLite

Costs:

- large maintenance burden
- likely invasive changes across storage, VDBE, planner, indexes, and APIs
- risks compatibility with upstream SQLite tools and future SQLite releases

Reject for type-system work.

### Shadow columns and sidecar indexes

Benefits:

- preserves SQLite b-tree performance for hard MySQL semantics
- useful for collations, exact decimals, temporal values, JSON paths, and
  expression indexes

Costs:

- schema complexity
- migration complexity
- function versioning and reindex requirements
- possible file-size growth

Use selectively for high-value indexed operations.

## Gaps we should account for

### Warning timing

MySQL warnings can depend on which rows and expressions are evaluated. SQLite's
optimizer may reorder predicates, avoid evaluating expressions, or evaluate
expressions at a different time than MySQL. Warning-bearing conversions and
functions need explicit tests before pushdown.

Pure helper functions used in indexes must not depend on session state and
should not emit warnings. Warning-producing user-visible functions should not be
used in generated columns or expression indexes.

### SQL mode and session state

Translation depends on session state:

- SQL modes
- time zone
- character-set and collation variables
- `group_concat_max_len`
- current database
- statement timestamp for temporal functions
- user variables
- warning state

Prepared SQLite statements must be invalidated or parameterized when relevant
session state changes.

### Determinism and function versioning

SQLite generated columns and expression indexes depend on deterministic
functions. MyLite must define which internal functions are stable enough for
stored helper values.

If a function's output changes because MyLite fixes a compatibility bug,
affected generated columns or expression indexes may need rebuilds. Store helper
function versions in MyLite metadata and add a reindex path before relying on
them broadly.

### Internal function security

The SQLite database file may contain schema text. If internal functions are
registered on a connection, a malicious or corrupted schema could try to invoke
them from views, triggers, CHECK constraints, defaults, generated columns,
expression indexes, or partial indexes.

Mitigations:

- keep trusted schema disabled where possible
- mark only audited safe helpers as innocuous
- mark side-effecting helpers as direct-only
- reserve `_mylite_*` names and block user-authored references unless
  intentionally exposed
- keep schema-level helper functions small, pure, and deterministic

### Metadata

SQLite result metadata is not sufficient for MySQL clients. MyLite must return
metadata from logical descriptors:

- field type
- flags
- length and max length
- decimals
- charset id
- nullability
- table and origin metadata
- expression labels

SQLite may execute the statement, but MyLite should describe it.

### Diagnostics

SQLite error codes and messages do not match MySQL. The native execution path
must map or replace diagnostics with MySQL-compatible errors, SQLSTATE values,
warnings, row counts, and insert ids.

### Optimizer-dependent semantics

The translator must know when optimizer freedom is safe. Examples that need
care:

- user variables
- non-deterministic functions
- warning-bearing casts
- short-circuiting expressions
- `ORDER BY` expressions that are not selected
- `DISTINCT` with hidden ordering keys
- aggregate and window function evaluation order
- functions whose result depends on connection state

### File and schema compatibility

Using generated columns, expression indexes, STRICT tables, or helper schema
objects affects what older SQLite versions can read. MyLite bundles SQLite, but
the `.mylite` format should still document which SQLite features are used
inside the shifted payload and how migrations handle them.

## Query features affected

### SELECT projection

Simple projections should be lowered directly where possible. Expression
projections need descriptor-driven lowering and MyLite result metadata. Hidden
internal projections may be added for ordering, grouping, or distinctness, but
must not leak to clients.

### WHERE and JOIN predicates

Predicate lowering determines both correctness and performance. The planner
should prefer native indexed predicates when safe, then collation-decorated
predicates, then typed MyLite comparator functions, then helper keys, then
fallback.

Join predicates follow the same rules as `WHERE`; unsafe comparison semantics
can make a native join wrong even when table access itself is safe.

### ORDER BY

`ORDER BY` can use:

- native column ordering
- SQLite collation ordering
- MyLite weight-string functions
- generated/shadow sort keys
- fallback rowset sorting

For MySQL-compatible `DISTINCT` plus `ORDER BY` on expressions not in the
select list, MyLite should add hidden order expressions to the SQLite query or
use a derived query shape that preserves MySQL-visible columns while giving
SQLite the keys it needs.

### GROUP BY and DISTINCT

Grouping and distinctness depend on equality semantics, collation, null
handling, and expression descriptors. Direct SQLite grouping is safe only when
the grouping key equality matches MySQL. Otherwise use collations, key
functions, helper keys, or fallback grouping.

### Aggregates and windows

SQLite supports aggregate and window functions, and application-defined window
functions can be registered. MyLite should prefer SQLite execution for frame
management, partitioning, sorting, and aggregate iteration once argument and
result semantics are covered.

MyLite-specific aggregate/window functions are needed when MySQL semantics
differ. Result descriptors and metadata still come from MyLite.

### DML

`INSERT`, `UPDATE`, `REPLACE`, and `DELETE` should use SQLite for physical row
changes, indexes, transactions, and constraints where possible. MyLite must
still own:

- assignment coercion
- defaults and generated values
- strict vs non-strict diagnostics
- warnings
- duplicate-key semantics
- `AUTO_INCREMENT`
- affected-row counts
- `LAST_INSERT_ID()`
- trigger behavior when implemented

## Phased migration plan

### Phase 1: Translation policy and capability classifier

- Define translation safety classes in code.
- Add an internal plan annotation for native-safe, hook-safe, helper-key, and
  fallback operations.
- Make unsupported fallback reasons observable in debug/test builds.
- Add tests that assert selected plans for representative statements.

### Phase 2: Logical and physical descriptor split

- Extend descriptors so every expression has a logical MySQL descriptor.
- Add physical storage descriptors for table columns.
- Record collation id and coercibility in expression analysis.
- Make result metadata come only from descriptors.
- Add MySQL-runtime tests for type metadata across translated and fallback
  paths.

### Phase 3: Write-path coercion

- Centralize insert/update/default coercion before SQLite storage.
- Enforce ranges, truncation, nullability, charset conversion, temporal
  parsing, enum/set normalization, JSON validation, and warnings in MyLite.
- Store values using documented physical encodings.
- Ensure SQLite cannot silently accept values MySQL would reject or warn about.

### Phase 4: Hook registry

- Register private MyLite scalar, aggregate, window, cast, comparison, and key
  functions per SQLite connection.
- Register initial MySQL-compatible collations.
- Define function flags: deterministic, innocuous, direct-only, subtype use.
- Block or reject user-authored calls to private helper names.
- Add tests for security-sensitive schema contexts.

### Phase 5: Native SELECT expansion

- Lower safe projections, predicates, ordering, grouping, distinctness,
  aggregates, windows, CTEs, query expressions, and joins through SQLite.
- Use hidden projections and derived query shapes for MySQL-compatible result
  visibility.
- Keep fallback runtime for unsafe expression semantics.
- Track how much of each statement is native vs fallback.

### Phase 6: Index-aware helper keys

- Add canonical internal SQL generation for helper expressions.
- Add generated or shadow key support for selected high-value cases:
  collations, decimals, temporal values, JSON path indexes.
- Store helper function versions and add rebuild/reindex handling.
- Add planner rules that use helper keys only when their metadata matches the
  resolved MySQL semantics.

### Phase 7: System virtual tables

- Implement `information_schema`, selected `mysql.*` tables, and placeholder
  metadata surfaces as virtual tables or eponymous-only table-valued functions
  where that keeps the code simpler.
- Use `xBestIndex` to consume common filters such as schema/table names.
- Avoid using virtual tables for ordinary user storage until a specific
  performance or semantic need is proven.

### Phase 8: Profiling and targeted SQLite patches

- Profile representative application workloads.
- Identify cases where native translation is correct but slow.
- Prefer SQL shape improvements, indexes, helper keys, and function
  specialization before SQLite patches.
- Keep any SQLite patch narrow and documented.

## Testing strategy

Native execution is only safe when tested against MySQL behavior and against
the selected SQLite lowering.

Required test layers:

- MySQL-runtime golden tests for semantics, metadata, warnings, diagnostics,
  affected rows, and side effects.
- Translator classification tests for each expression/operator/type pair.
- Dual-path tests where the same query can run through native SQLite and
  MyLite fallback, comparing MyLite-visible results.
- Index-use tests for native, collation, expression-index, and helper-key
  plans.
- SQL mode and session-variable invalidation tests.
- Security tests for private functions in views, triggers, CHECK constraints,
  generated columns, expression indexes, and partial indexes.
- Performance smoke tests for scans, indexed predicates, joins, sorting,
  grouping, and window functions.

Compatibility tests should include metadata and warnings, not only result rows.

## Near-term recommendations

1. Do not expand the generic string translator directly.
2. Add a typed lowering layer fed by `mylite_select_plan` and expression
   descriptors.
3. Introduce a small internal capability enum before adding many rewrites.
4. Make direct SQLite lowering the fast path for simple, proven-safe cases.
5. Add MyLite UDF/collation fallback for correctness before optimizing every
   path.
6. Keep `_mylite_*` helper names private and reserve them in the parser or
   analyzer.
7. Start with ordinary SQLite tables for user data.
8. Use virtual tables for system schemas and table-valued functions first.
9. Use generated columns and expression indexes only after the helper function
   and versioning policy exists.
10. Avoid SQLite internal type-system patches.

## Open design questions

- What physical encoding should MyLite choose for full-range unsigned
  `BIGINT`?
- What physical encoding should MyLite choose for exact `DECIMAL` values?
- Should temporal values use canonical text, integer microseconds, or both?
- Which MySQL collations should be implemented as SQLite collations first?
- Which internal helper functions are safe enough to be deterministic and
  innocuous?
- How should helper function versions and reindex requirements be stored in
  `.mylite` metadata?
- How much fallback runtime should remain after native SELECT translation
  expands?
- Should MyLite expose a debug `EXPLAIN MYLITE`-style surface showing native,
  hook, helper-key, and fallback plan regions?

These questions should be answered with MySQL-runtime probes, small targeted
specs, and measured application workloads before broad implementation.
