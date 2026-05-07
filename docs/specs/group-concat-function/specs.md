# `GROUP_CONCAT()`

## Scope

MyLite implements `GROUP_CONCAT()` as a MySQL-compatible aggregate over the
existing aggregate query engine. It must work anywhere current aggregate calls
work: no-table aggregate `SELECT`, table-backed implicit groups, explicit
`GROUP BY`, joined row sources, `HAVING`, and `ORDER BY`.

The first executable slice includes:

- one or more argument expressions
- aggregate-local `DISTINCT`
- aggregate-local `ORDER BY` over argument ordinals, column names, and
  expressions in the current row source
- string-literal `SEPARATOR`
- `NULL` skipping, empty-group `NULL`, output metadata, and warning 1260 on
  truncation
- session/local `group_concat_max_len` control of truncation and metadata

Out of scope for this slice:

- window-function `OVER (...)`
- global or persisted `group_concat_max_len` mutation
- binary-string display fidelity for embedded NUL bytes
- full charset/collation derivation beyond the current binary-vs-connection
  descriptor split
- optimizer pushdown or SQLite aggregate delegation

## Sources

- MySQL 8.4 Reference Manual, Aggregate Function Descriptions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- MySQL 8.4 Reference Manual, Server System Variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- Existing MyLite specs:
  - `docs/specs/aggregate-grouping/specs.md`
  - `docs/specs/count-distinct-aggregate/specs.md`
  - `docs/specs/joined-group-by/specs.md`
  - `docs/specs/result-metadata-expression-labels/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

- `docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --show-warnings --force`
- `docker exec -i mylite-mysql-849 mysql -uroot --column-type-info -vvv --force`

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 Behavior Summary

`GROUP_CONCAT()` returns a string made from the non-`NULL` argument tuples in a
group. With one argument, each non-`NULL` value contributes its string form. With
multiple arguments, the argument string forms are joined directly for each input
row, then row strings are joined with the separator. The default row separator
is comma. If any argument in a row is `NULL`, that row does not contribute to
the result. If no row contributes, the result is `NULL`.

`DISTINCT` removes duplicate expression tuples before concatenation. It compares
the tuple of argument values, not only the final per-row concatenated string.
For example, rows `('a','bc')` and `('ab','c')` both contribute `abc`, but they
remain distinct tuples.

Aggregate-local `ORDER BY` sorts contributing rows before string assembly.
Unsigned integer order terms refer to the corresponding `GROUP_CONCAT()`
argument, so `ORDER BY 1 DESC` sorts by the first aggregate argument descending.
`ORDER BY 0` and positive integer terms greater than the aggregate argument
count fail with unknown-column error 1054 in `order clause`. Column names and
expressions resolve against the aggregate input row source.

`SEPARATOR` accepts a string literal. `SEPARATOR ''` removes row separators.
Runtime probes showed `SEPARATOR NULL`, numeric separator tokens, and function
calls in the separator position are syntax errors in MySQL 8.4.9.

The result length is capped by `group_concat_max_len`, which defaults to 1024
bytes and is mutable per session. When output is cut, it appends warning 1260
with the message `Row N was cut by GROUP_CONCAT()`, where `N` is the one-based
contributing row that first exceeded the cap.

## Representative Runtime Expectations

Fixture:

```sql
CREATE TABLE t (
  id INT PRIMARY KEY,
  grp VARCHAR(10),
  txt VARCHAR(20),
  n INT
);

INSERT INTO t VALUES
  (1,'a','alpha',2),
  (2,'a','beta',1),
  (3,'a','alpha',3),
  (4,'b',NULL,1),
  (5,'b','delta',2),
  (6,NULL,'nil',NULL);
```

| SQL | Result |
| --- | --- |
| `SELECT GROUP_CONCAT(txt) FROM t WHERE grp='a'` | `alpha,beta,alpha` |
| `SELECT GROUP_CONCAT(txt ORDER BY n DESC SEPARATOR '|') FROM t WHERE grp='a'` | `alpha|alpha|beta` |
| `SELECT GROUP_CONCAT(DISTINCT txt ORDER BY txt DESC SEPARATOR '|') FROM t WHERE grp='a'` | `beta|alpha` |
| `SELECT GROUP_CONCAT(txt, ':', n ORDER BY n SEPARATOR ';') FROM t WHERE grp='a'` | `beta:1;alpha:2;alpha:3` |
| `SELECT grp, GROUP_CONCAT(txt ORDER BY n SEPARATOR '|') FROM t GROUP BY grp ORDER BY grp IS NULL, grp` | `('a','beta|alpha|alpha')`, `('b','delta')`, `(NULL,'nil')` |
| `SELECT GROUP_CONCAT(txt) FROM empty_t` | `NULL` |
| `SELECT GROUP_CONCAT(txt) FROM t WHERE grp='b' AND txt IS NULL` | `NULL` |
| `SELECT GROUP_CONCAT('a','b')` | `ab` |
| `SELECT GROUP_CONCAT('a',NULL,'b')` | `NULL` |

With `SET SESSION group_concat_max_len = 4`, MySQL returns `alph` for
`GROUP_CONCAT(txt ORDER BY id SEPARATOR ',')` over the `grp='a'` rows and emits
warning 1260.

## Metadata

With MySQL's default `group_concat_max_len=1024`, text inputs return a
`LONG_BLOB`-family descriptor with the connection/table collation and nullable
result. When the session value is `512` bytes or lower, MySQL reports a
`VAR_STRING`-family descriptor for nonbinary inputs. Binary inputs return a
binary descriptor. MyLite maps nonbinary results to nullable `BLOB`
metadata when the session limit is above `512`, and nullable `VAR_STRING`
metadata when it is `512` or lower. Declared nonbinary length is the session
limit multiplied by the connection charset maxlen, decimals are `31`, and the
current connection charset is reported.

If any concatenated argument expression has binary string metadata, MyLite
reports a binary result: charset id `63`, `BINARY` flag, nullable, decimals
`31`, and declared byte length equal to `group_concat_max_len`. Numeric
arguments do not make the result binary even though numeric descriptors use the
binary charset. A binary separator alone does not make nonbinary arguments
report binary metadata.

## MyLite Grammar

The parser accepts a dedicated aggregate syntax instead of treating the options
as scalar function arguments:

```lemon
primary_expression ::= group_concat_call.

group_concat_call ::= function_name LPAREN group_concat_body RPAREN.
group_concat_body ::= opt_distinct expression_list opt_group_concat_order opt_group_concat_separator.
opt_group_concat_order ::= .
opt_group_concat_order ::= order_by_clause.
opt_group_concat_separator ::= .
opt_group_concat_separator ::= SEPARATOR STRING.
```

The semantic action must reject any `function_name` other than
`GROUP_CONCAT`. Existing aggregate forms continue to reject `GROUP_CONCAT(*)`,
`GROUP_CONCAT()`, and unsupported separator token types at parse time.

## Runtime Design

Each aggregate binding stores the aggregate call, argument expression list,
optional aggregate-local `ORDER BY` clause, and optional separator literal.
During row aggregation, MyLite evaluates all argument expressions left to right.
If any argument is `NULL`, the input row is skipped after preserving warnings
from expressions already evaluated. Otherwise, argument values are copied for
`DISTINCT` comparison, converted to text for the row payload, and aggregate
order expressions are evaluated.

At finalization, MyLite sorts stored items when an aggregate-local `ORDER BY`
exists, removes duplicate tuples for `DISTINCT`, joins row payloads with the
separator, enforces the current session `group_concat_max_len` byte cap, and
returns `NULL` if no item remains. All heap-owned argument values, order values,
row text, separator text, and final buffers are statement-owned and released
through aggregate-state deinit.

No storage-format changes are required.

## Test Plan

Tests cover:

- parser acceptance for ordinary, `DISTINCT`, aggregate-local `ORDER BY`,
  multi-expression, and `SEPARATOR` forms
- parser rejection for empty argument lists, `*`, non-string separators, and
  malformed option order
- no-table aggregate results, including multi-expression and `NULL` skipping
- table-backed implicit groups and explicit `GROUP BY`
- aggregate-local ordering by column/expression and by argument ordinal
- unknown-column diagnostics for invalid aggregate-local order ordinals and
  missing order columns
- `DISTINCT` over single and multi-expression tuples
- empty input and all-`NULL` input
- `HAVING` and statement `ORDER BY` over a `GROUP_CONCAT()` alias
- metadata for the default text descriptor
- metadata for binary arguments, mixed binary/text arguments, numeric
  arguments, and short binary results under low `group_concat_max_len`
- truncation at the default 1024-byte cap and warning 1260
- truncation and metadata after `SET SESSION group_concat_max_len = 4`
