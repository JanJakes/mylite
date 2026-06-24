# Baseline System Function Residuals

## Scope

This slice covers the remaining red rows in the system-function compatibility
group:

- `SLEEP(expr)`
- `NAME_CONST(name_literal, value_literal)`
- `LOAD_FILE(file_name)`
- `ExtractValue(xml_frag, xpath_expr)`
- `UpdateXML(xml_target, xpath_expr, replacement_xml)`

The feature target is MySQL 8.4.9 behavior that real applications commonly
touch while probing server capabilities or replaying framework test suites. The
implementation uses MyLite scalar evaluation plus registered SQLite scalar
callbacks for row-backed expression contexts. No SQLite fork hook is required.

Official MySQL 8.4 reference pages used for this design:

- <https://dev.mysql.com/doc/refman/8.4/en/miscellaneous-functions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/xml-functions.html>

## Verified MySQL 8.4.9 Behavior

Runtime probes were executed against the local `mysql:8.4.9` container
`mylite-mysql-849`.

`SLEEP()`:

- `SLEEP(0)` and small positive fractional values return `0`.
- In default strict mode, `SLEEP(NULL)` and `SLEEP(-1)` fail with
  `1210 / HY000 / Incorrect arguments to sleep.`
- In non-strict mode, those invalid values return `0` and append warning
  `1210 / HY000 / Incorrect arguments to sleep.`
- String values that cannot be fully converted to a double, including
  `'abc'` and `'0.001x'`, return `0` and append warning
  `1292 / 22007 / Truncated incorrect DOUBLE value`.

`NAME_CONST()`:

- The first argument must be a string literal.
- The second argument accepts literal integer, signed integer, decimal, float,
  string, binary hex, bit, and `NULL` values.
- Boolean literals, computed expressions, and dynamic first arguments fail with
  `1210 / HY000 / Incorrect arguments to NAME_CONST`.
- The result column label is the decoded first string literal.

`LOAD_FILE()`:

- `LOAD_FILE(NULL)` returns `NULL`.
- A non-existent or disallowed path returns `NULL` without a warning in the
  verified container. MySQL can return file contents only when server-side file
  privileges and filesystem policy permit it.

XML functions:

- `ExtractValue('<a>one</a>', '/a')` returns `one`.
- Multiple matching elements are joined with a single space, for example
  `/a/b` over two `<b>` children returns `one two`.
- No match returns the empty string.
- `NULL` XML or XPath arguments return `NULL`.
- `UpdateXML()` replaces the matched XML node when exactly one element matches.
- If zero or multiple elements match, `UpdateXML()` returns the original XML.
- A `NULL` replacement returns `NULL`.
- Malformed XML returns `NULL` and warning `1525 / HY000` whose message starts
  with `Incorrect XML value`.
- Invalid XPath syntax fails with `1105 / HY000 / XPATH syntax error`.

## MyLite Behavior

`SLEEP()` is implemented in scalar and row-scalar contexts. It sleeps for the
requested non-negative finite duration and returns `0`. MyLite does not yet
model MySQL's server-side interruption semantics for killed statements, but the
observable baseline value and diagnostics are MySQL-shaped.

`NAME_CONST()` is implemented for literal-value forms. It validates the first
argument and returns the second literal value while using the first argument as
the result column label. In source-backed `SELECT` lists, the row-scalar planner
validates the function and plans only the literal value expression because the
function has no runtime data dependency beyond metadata.

`LOAD_FILE()` is an embedded-design placeholder. MyLite always returns `NULL`
after evaluating its argument. This avoids arbitrary server-side filesystem
reads from an embedded library while matching MySQL's result for unavailable
files. If a future embedding API needs controlled file access, it should be a
separate opt-in capability with explicit filesystem policy.

`ExtractValue()` and `UpdateXML()` implement a small XML/XPath subset:

- XML element fragments with start/end and self-closing tags.
- Attributes are skipped for parsing but not exposed to XPath.
- XPath supports absolute element-name paths such as `/a/b` and descendant
  element-name paths such as `//b`.
- Row-backed execution covers XPath expressions that are constant for the
  statement; MySQL rejects XPath values read from source rows with
  `Only constant XPATH queries are supported`.
- Namespace, attribute, wildcard, predicate, function, arithmetic, text-node,
  and full XPath-axis behavior is intentionally out of scope for this slice.

Malformed XML and invalid XPath diagnostics match the MySQL error/warning
shape for the supported baseline.

## Grammar

No grammar change is required. MyLite already parses these calls as generic
functions:

```lemon
expression ::= identifier LPAREN function_argument_list_opt RPAREN.
```

The runtime recognizes the generic function names case-insensitively and
applies the function-specific arity and argument rules.

## Metadata

- `SLEEP()` returns a `LONGLONG`-shaped scalar descriptor.
- `NAME_CONST()` inherits the descriptor shape of the literal value expression
  and replaces the result label with the decoded first argument.
- `LOAD_FILE()`, `ExtractValue()`, and `UpdateXML()` return nullable
  connection-character-set string descriptors in the MyLite subset.

## Tests

MySQL-runtime expectation script:

- `packages/libmylite/tests/mysql_baseline_system_function_residuals_expectations.sh`

MyLite runtime test:

- `packages/libmylite/tests/runtime_system_function_residuals_test.c`

The tests cover result values, warnings, SQL errors, `NAME_CONST()` labels,
`LOAD_FILE()` placeholder behavior, XML malformed-input warnings, invalid XPath
errors, and source-backed row-scalar projections.

## Compatibility Status

- `SLEEP()` is green for baseline scalar and row-scalar behavior. Statement
  interruption during sleep remains outside the current execution model.
- `NAME_CONST()` is green for the literal forms MySQL accepts in the verified
  probes.
- `LOAD_FILE()` is green for the unavailable-file embedded placeholder behavior.
  Server-side file reads remain a separate yellow compatibility boundary.
- `ExtractValue()` and `UpdateXML()` are green for the verified simple XML
  subset. Full XML and XPath behavior remains a separate yellow compatibility
  boundary.
