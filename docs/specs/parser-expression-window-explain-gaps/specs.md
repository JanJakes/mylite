# Parser expression, window, and EXPLAIN gaps

This slice broadens MyLite's parser surface for MySQL 8.4.9 expression,
function, type-conversion, and window-function syntax seen in real application
and MySQL server-test corpora. It also adds an embedded-compatible `EXPLAIN`
query placeholder. Runtime execution remains limited to semantics MyLite can
currently implement correctly through its descriptor planner and SQLite-backed
execution layer.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/explain.html
- https://dev.mysql.com/doc/refman/8.4/en/window-functions.html
- https://dev.mysql.com/doc/refman/8.4/en/cast-functions.html
- https://dev.mysql.com/doc/refman/8.4/en/charset-introducer.html
- https://dev.mysql.com/doc/refman/8.4/en/create-trigger.html
- https://dev.mysql.com/doc/refman/8.4/en/create-procedure.html

## Scope

### EXPLAIN

MySQL supports `EXPLAIN` for query statements and table-description shorthand.
MyLite already treats `EXPLAIN table_name` as a `SHOW COLUMNS`/`DESCRIBE`
alias. This slice adds query-form acceptance:

- `EXPLAIN SELECT ...`
- `EXPLAIN FORMAT=TRADITIONAL SELECT ...`
- `EXPLAIN FORMAT=JSON SELECT ...`
- `EXPLAIN FORMAT=TREE SELECT ...`
- `EXPLAIN ANALYZE SELECT ...`
- `EXPLAIN ANALYZE FORMAT=TREE SELECT ...`
- `EXPLAIN INSERT ...`
- `EXPLAIN REPLACE ...`
- `EXPLAIN UPDATE ...`
- `EXPLAIN DELETE ...`

The result is an explicit MyLite-owned placeholder, not an optimizer-cost
model. Traditional output uses MySQL's standard tabular column labels:

`id`, `select_type`, `table`, `partitions`, `type`, `possible_keys`, `key`,
`key_len`, `ref`, `rows`, `filtered`, `Extra`.

`FORMAT=JSON` and `FORMAT=TREE` return one column named `EXPLAIN`. `ANALYZE`
does not execute the child query in this slice; it returns a deterministic
placeholder explaining that MyLite does not expose runtime plan analysis yet.
`EXPLAIN ANALYZE FORMAT=JSON` and `EXPLAIN ANALYZE FORMAT=TRADITIONAL` are
rejected because MySQL restricts `EXPLAIN ANALYZE` to tree output. MyLite keeps
`EXPLAIN FORMAT=JSON ANALYZE` as a syntax error.

The placeholder does not validate whether referenced user tables exist or
whether the child statement would be executable. `INTO`, `FOR SCHEMA`,
`FOR DATABASE`, and `FOR CONNECTION` remain unsupported.

### Window grammar

Existing runtime support executes projection-only ranking, distribution,
navigation, and frame-value functions for a narrow row-scalar envelope. This
slice expands parsing for MySQL-shaped window syntax that the runtime may
diagnose later when it falls outside that envelope:

- `OVER window_name`
- `WINDOW window_name AS (window_spec)`
- inherited named-window specs such as `OVER (window_name ORDER BY expr)`
- `PARTITION BY expr [, expr ...]`
- `ORDER BY expr [ASC|DESC] [, expr [ASC|DESC] ...]`
- `ROWS` and `RANGE` frame clauses with `UNBOUNDED`, `CURRENT ROW`,
  `expr PRECEDING`, and `expr FOLLOWING` bounds

The current executor continues to support only inline `OVER (...)` specs with
at most one descriptor-column partition key and one descriptor-column order
key. Named windows, frames, expression keys, multiple keys, joins/grouped
contexts, and aggregate windows are accepted only as parser-level coverage and
must fail with clear unsupported diagnostics if execution reaches them.

### Expression and function grammar

The first implementation pass keeps DML row values and assignment values on
their current conflict-free grammar. A broad `insert_value ::= expression` or
`update_value ::= expression` rule introduces large ambiguity against MyLite's
existing statement grammar and needs a dedicated DML-expression slice.

This slice broadens parser coverage where it is safe:

- `GROUP_CONCAT(expr ORDER BY expr [ASC|DESC] [, ...] SEPARATOR 'literal')`
  parses expression-shaped value and order-key syntax.
- Existing single-column `GROUP_CONCAT(column ORDER BY order_column)` keeps the
  current runtime-compatible AST shape.

Runtime support remains limited by the current aggregate planner: value and
order keys must still be descriptor columns in the documented supported
execution envelope.

### Type and literal grammar

This slice admits parser gaps that do not require new storage semantics:

- `CAST(expr AS YEAR)` and `CONVERT(expr, YEAR)` parse as character-family
  conversion placeholders until real `YEAR` cast semantics are implemented.
- `CHARSET` is accepted as a synonym for `CHARACTER SET` in character cast
  targets.
- Character-set introducers before ordinary string, hex, and bit literals
  parse as introduced-literal expressions. The value bytes are not transcoded
  by the parser.
- Standard temporal literal forms such as `DATE 'YYYY-MM-DD'`,
  `TIME 'HH:MM:SS'`, and `TIMESTAMP 'YYYY-MM-DD HH:MM:SS'` parse as typed
  literal expressions. Descriptor storage and predicate conversion remain the
  existing documented subsets.

### Triggers and stored programs

User-created triggers, stored functions, and general stored programs can be
supported, but not as a small parser gap. Correct support needs descriptor
storage, metadata, `OLD`/`NEW` row binding, trigger firing during
`INSERT`/`UPDATE`/`DELETE`, ordering, recursion and error behavior, interaction
with foreign keys, statement atomicity, routine variables, handlers, cursors,
definers, privileges, and persistent routine/trigger catalogs.

SQLite native triggers are not a sufficient drop-in mechanism for broad MySQL
trigger compatibility. They may be useful as an implementation detail for
carefully constrained trigger bodies later, but MyLite must own the MySQL
semantic boundary.

This slice keeps:

- metadata-only built-in `sys.sys_config` trigger rows,
- empty user trigger introspection,
- current session-local no-argument single-`SELECT` procedure bridge.

It does not add `CREATE TRIGGER`, `DROP TRIGGER`, stored functions, routine
parameters, local variables, compound stored-program execution, or trigger
execution.

## MyLite grammar snippets

These snippets describe the intended MyLite-owned Lemon grammar shape.

```lemon
statement ::= explain_query_statement.

explain_query_statement ::=
    EXPLAIN explain_format_opt explainable_statement.
explain_query_statement ::=
    EXPLAIN ANALYZE explain_format_opt explainable_analyze_statement.

explain_format_opt ::= .
explain_format_opt ::= FORMAT EQUAL explain_format_name.

explain_format_name ::= identifier.
explain_format_name ::= JSON.

explainable_statement ::= select_statement.
explainable_statement ::= compound_select_statement.
explainable_statement ::= table_statement.
explainable_statement ::= values_statement.
explainable_statement ::= insert_values_statement.
explainable_statement ::= insert_select_statement.
explainable_statement ::= insert_set_statement.
explainable_statement ::= replace_values_statement.
explainable_statement ::= replace_select_statement.
explainable_statement ::= replace_set_statement.
explainable_statement ::= update_statement.
explainable_statement ::= delete_statement.

explainable_analyze_statement ::= select_statement.
explainable_analyze_statement ::= compound_select_statement.
explainable_analyze_statement ::= table_statement.
```

```lemon
over_clause ::= OVER LPAREN window_spec_opt RPAREN.
over_clause ::= OVER identifier.

window_clause_opt ::= .
window_clause_opt ::= WINDOW named_window_definition_list.

named_window_definition ::= identifier AS LPAREN window_spec_opt RPAREN.

window_spec_opt ::= .
window_spec_opt ::= window_name.
window_spec_opt ::= window_name window_partition_clause.
window_spec_opt ::= window_name window_order_clause.
window_spec_opt ::= window_name window_frame_clause.
window_spec_opt ::= window_name window_partition_clause window_order_clause.
window_spec_opt ::= window_name window_partition_clause window_frame_clause.
window_spec_opt ::= window_name window_order_clause window_frame_clause.
window_spec_opt ::= window_name window_partition_clause window_order_clause window_frame_clause.
window_spec_opt ::= window_partition_clause.
window_spec_opt ::= window_order_clause.
window_spec_opt ::= window_frame_clause.
window_spec_opt ::= window_partition_clause window_order_clause.
window_spec_opt ::= window_partition_clause window_frame_clause.
window_spec_opt ::= window_order_clause window_frame_clause.
window_spec_opt ::= window_partition_clause window_order_clause window_frame_clause.

window_partition_clause ::= PARTITION BY expression_list.
window_order_clause ::= ORDER BY window_order_item_list.
window_frame_clause ::= ROWS window_frame_extent.
window_frame_clause ::= RANGE window_frame_extent.
window_frame_extent ::= window_frame_bound.
window_frame_extent ::= BETWEEN window_frame_bound AND window_frame_bound.
window_frame_bound ::= UNBOUNDED PRECEDING.
window_frame_bound ::= UNBOUNDED FOLLOWING.
window_frame_bound ::= CURRENT ROW.
window_frame_bound ::= expression PRECEDING.
window_frame_bound ::= expression FOLLOWING.
```

```lemon
cast_basic_target ::= YEAR.
cast_character_set_opt ::= CHARSET option_name.
cast_character_set_opt ::= CHARSET BINARY.

expression ::= charset_introducer literal.
expression ::= DATE STRING.
expression ::= TIME STRING.
expression ::= TIMESTAMP STRING.
```

## Tests

Focused parser tests cover:

- query `EXPLAIN` forms and table `EXPLAIN` preservation,
- expanded window syntax including named windows and frames,
- `GROUP_CONCAT()` expression arguments,
- `CAST(... AS YEAR)`, `CHARSET` cast targets, introducers, and typed temporal
  literal syntax,
- rejected or unsupported trigger/routine forms remain documented.

Focused runtime tests cover:

- `EXPLAIN SELECT` traditional placeholder columns,
- `EXPLAIN FORMAT=JSON SELECT` and `EXPLAIN FORMAT=TREE SELECT` single-column
  placeholder output,
- `EXPLAIN INSERT` traditional placeholder output without executing the child
  statement,
- `EXPLAIN ANALYZE FORMAT=TREE SELECT` placeholder output without executing
  the query,
- `EXPLAIN ANALYZE FORMAT=JSON SELECT` and
  `EXPLAIN ANALYZE FORMAT=TRADITIONAL SELECT` rejection,
- `EXPLAIN table_name` still returns column metadata.

Parser corpus benchmarks over the WordPress mysql-on-sqlite server-test query
CSV should be rerun before and after the slice to measure accepted-query
movement and to identify remaining high-volume grammar classes.

## Compatibility status

The slice moves query `EXPLAIN` from unsupported to placeholder support.
Expanded aggregate-expression, type, literal, and window grammar is parser
support only unless an existing runtime path already supports the expression.
General DML expression values, trigger DDL/execution, and general
stored-program runtime support remain unsupported.
