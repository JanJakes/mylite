# SHOW WHERE Filtering

## Scope

This slice implements executable `WHERE` filtering for MyLite SHOW statements
that already expose deterministic SQLite-backed result sets:

- `SHOW VARIABLES`
- `SHOW STATUS`
- `SHOW CHARACTER SET` / `SHOW CHARSET` / `SHOW CHAR SET`
- `SHOW COLLATION`
- `SHOW TABLES`
- `SHOW TABLE STATUS`
- `SHOW COLUMNS` / `SHOW FIELDS`
- `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS`

`SHOW DATABASES` filtering remains deferred because that statement does not yet
parse a filter. SHOW statements that MySQL does not filter, such as
`SHOW ENGINES`, `SHOW WARNINGS`, and `SHOW ERRORS`, remain syntax errors for
`LIKE` or `WHERE` where that is their existing MyLite behavior.

## Semantics

SHOW `WHERE` predicates are evaluated over the displayed result columns, not
the underlying catalog column names. The supported expression subset is:

- displayed-column identifiers, including backtick-quoted identifiers such as
  `` `Default collation` ``
- string, numeric, boolean, and `NULL` literals
- `=`, `<>`, `<`, `<=`, `>`, `>=`
- `AND`, `OR`, and unary `NOT`
- `LIKE` and `NOT LIKE`, including a literal `ESCAPE` expression
- `IN` and `NOT IN`
- unary `+` and `-`
- `IS NULL` and `IS NOT NULL`
- parentheses

Unknown displayed-column identifiers return MySQL error `1054` with an unknown
column diagnostic in the `where clause`. Expressions outside the supported
subset return a deterministic unsupported diagnostic for the specific SHOW
statement.

`LIKE` and `WHERE` remain mutually exclusive in the grammar for the covered
SHOW statements.

## Runtime

The implementation renders the supported predicate subset into the SQLite query
that already materializes each SHOW result set. This keeps filtering close to
the compact metadata query, avoids per-row C-side materialization, and preserves
existing ordering and metadata behavior.

For statements with dynamic display labels, the predicate uses the display
label:

- `SHOW TABLES` accepts `Tables_in_<schema>` and, for `FULL`, `Table_type`.
- `SHOW TABLES ... LIKE` still uses `Tables_in_<schema> (<pattern>)`, but
  `LIKE` and `WHERE` cannot appear together.

`SHOW COLUMNS` only exposes `FULL`-only labels such as `Collation`,
`Privileges`, and `Comment` when `FULL` is present.

## Tests

Runtime coverage includes successful filtering for each covered SHOW family,
including generated `SHOW TABLES` display-column names, backtick-quoted
character-set and collation labels, `IN`, `AND`, numeric comparisons, and
unknown-column diagnostics.
