# Baseline ROLES_GRAPHML function

## Summary

This slice implements the MySQL native `ROLES_GRAPHML()` function surface for
MyLite's embedded runtime. MySQL returns a GraphML XML document describing the
in-memory role graph. MyLite does not maintain a MySQL grant-store role graph,
so it returns a deterministic GraphML document for the embedded account graph
with `root@%` and no role edges.

The function is useful because applications and diagnostics can call the MySQL
native name without receiving an unsupported-function error, while the broader
user/role/privilege system remains explicitly outside this slice.

## MySQL 8.4.9 Observed Behavior

Runtime probes against MySQL 8.4.9 established this behavior:

- `ROLES_GRAPHML()` accepts exactly zero arguments.
- Calls with one or more arguments fail with error `1582`, SQLSTATE `42000`,
  and the native-function parameter-count diagnostic.
- The return value is non-`NULL` GraphML XML beginning with the XML declaration
  and containing a `<graphml>` element and directed `<graph id="G">`.
- The exact node list depends on the server's current account and role state.
- `CHARSET(ROLES_GRAPHML())` returns `utf8mb3`, and
  `COLLATION(ROLES_GRAPHML())` returns `utf8mb3_general_ci`.
- `COERCIBILITY(ROLES_GRAPHML())` returns `3`.
- Protocol metadata reports `LONG_BLOB`, decimals `31`, no flags, connection
  collation, and display length `50331648 * connection max bytes per character`.

## MyLite Behavior

MyLite supports:

- unqualified `ROLES_GRAPHML()` in scalar `SELECT`, `SELECT ... FROM DUAL`,
  `DO`, and supported row-backed scalar projection contexts;
- zero-argument arity validation with MySQL-compatible error `1582/42000`;
- a deterministic GraphML document with the same MySQL structural envelope and
  the embedded account node `` `root`@`%` ``;
- `LONG_BLOB`-family result metadata with the connection collation, decimals
  `31`, no flags, nullable metadata, and MySQL's observed display-length base.
- `CHARSET()`, `COLLATION()`, and `COERCIBILITY()` introspection for the
  function result in the currently supported scalar and row-scalar expression
  contexts.

MyLite does not implement:

- persisted MySQL users, roles, role edges, default roles, mandatory roles, or
  privilege filtering for the graph;
- synchronization with placeholder `mysql.user` or role metadata tables;
- a mutable server-global role graph;
- role-graph-specific metadata beyond the fixed embedded account node.

## Grammar

`ROLES_GRAPHML` is resolved through MyLite's generic function production rather
than a dedicated grammar token:

```lemon
expression(A) ::= generic_function_name(N) LPAREN RPAREN(R).
expression(A) ::= generic_function_name(N) LPAREN function_argument_list(B) RPAREN(R).
```

The analyzer resolves the generic name to the native MyLite system-function
descriptor and enforces the zero-argument contract before execution.

## Runtime and Storage

The function is pure and has no catalog, transaction, storage, warning, or file
format side effects. The result is produced by the existing generic sys-function
registry and SQLite callback path so row-backed expressions use the same
implementation as scalar expressions.

## Tests

The MySQL expectation script verifies the observed MySQL 8.4.9 value shape,
metadata shape, and arity diagnostics. The runtime test verifies MyLite scalar,
`DUAL`, `DO`, row-backed projection, result metadata, and error behavior.
