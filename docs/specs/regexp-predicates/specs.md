# REGEXP Predicates

## Scope

This feature adds MySQL-compatible regular-expression predicates to MyLite:

- `expr REGEXP pat`
- `expr RLIKE pat`
- `expr NOT REGEXP pat`
- `expr NOT RLIKE pat`
- `REGEXP_LIKE(expr, pat)`
- `REGEXP_LIKE(expr, pat, match_type)`

`REGEXP` and `RLIKE` are synonyms. `NOT REGEXP` and `NOT RLIKE` negate the
same match result. The predicate form behaves like `REGEXP_LIKE(expr, pat)`.

## Sources

The behavior is specified from the MySQL 8.4 Reference Manual regular
expression operator documentation and MySQL 8.4.9 runtime observations. The
implementation is independently authored and does not use external
implementation sources.

Official references:

- https://dev.mysql.com/doc/refman/8.4/en/regexp.html
- https://dev.mysql.com/doc/refman/8.4/en/non-typed-operators.html
- https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html

## MySQL 8.4.9 Observations

The following probes were run against `mysql:8.4.9`:

```sql
SELECT
  'abc' REGEXP 'a',
  'abc' REGEXP '^a.c$',
  'abc' RLIKE '^[a-z]+$',
  'abc' NOT REGEXP 'z',
  NULL REGEXP 'a',
  'abc' REGEXP NULL,
  REGEXP_LIKE('abc','A'),
  REGEXP_LIKE('abc','A','c'),
  REGEXP_LIKE('abc','A','i'),
  REGEXP_LIKE('a\nb','^b','m'),
  REGEXP_LIKE('a\nb','a.b'),
  REGEXP_LIKE('a\nb','a.b','n'),
  REGEXP_LIKE('abc','[[:alpha:]]+'),
  REGEXP_LIKE('abc','[a-z]{3}'),
  REGEXP_LIKE('abc','a|z');
```

Result:

```text
1 1 1 1 NULL NULL 1 0 1 1 0 1 1 1 1
```

Additional observations:

- `REGEXP_LIKE('abc','a','ic')` returns `1`; `REGEXP_LIKE('abc','A','ic')`
  returns `0`; `REGEXP_LIKE('abc','A','ci')` returns `1`. The rightmost `c`
  or `i` flag wins.
- POSIX case classes honor case-insensitive matching. `REGEXP_LIKE('A',
  '[[:lower:]]')` and `REGEXP_LIKE('a','[[:upper:]]')` returned `1`; adding
  match type `c` returned `0`.
- Invalid regular-expression syntax is an execution error. A mismatched
  parenthesis returned error `3691 (HY000)` with message
  `Mismatched parenthesis in regular expression.`
- An empty pattern is an execution error. `REGEXP_LIKE('abc','')` and
  `'abc' REGEXP ''` returned error `3685 (HY000)` with message
  `Illegal argument to a regular expression.`
- Invalid `match_type` text is an execution error. An unknown flag returned
  error `1210 (HY000)` with message `Incorrect arguments to regexp_like`.
- Projection metadata for non-nullable predicate/function results is
  `LONGLONG`, collation `binary (63)`, length `1`, decimals `0`, flags
  `NOT_NULL BINARY NUM`. Nullable operands clear `NOT_NULL`.

## Syntax

MyLite Lemon grammar additions:

```lemon
%left EQ NULL_SAFE_EQ NE LT LE GT GE IS LIKE REGEXP RLIKE IN.

comparison_expression(A) ::= comparison_expression(B) REGEXP(T) bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_REGEXP, C);
}
comparison_expression(A) ::= comparison_expression(B) RLIKE(T) bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_REGEXP, C);
}
comparison_expression(A) ::= comparison_expression(B) NOT(T) REGEXP bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_NOT_REGEXP, C);
}
comparison_expression(A) ::= comparison_expression(B) NOT(T) RLIKE bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_NOT_REGEXP, C);
}
```

`REGEXP_LIKE()` uses the existing ordinary scalar function-call grammar.

## Semantics

Evaluation order follows the existing MyLite binary operator and scalar
function evaluation model:

1. Evaluate operands left-to-right.
2. If the subject, pattern, or `match_type` argument is `NULL`, return `NULL`.
3. Convert non-NULL arguments to strings using the same value-to-string rules as
   other string predicates.
4. Compile the pattern. Invalid pattern syntax raises an execution error.
5. Match against the subject. `REGEXP` and `RLIKE` return `1` for match and `0`
   for no match. `NOT REGEXP` and `NOT RLIKE` invert only non-NULL match
   results.

`REGEXP_LIKE(expr, pat, match_type)` accepts:

- `c`: case-sensitive matching
- `i`: case-insensitive matching
- `m`: multi-line mode for `^` and `$`
- `n`: dot matches newline
- `u`: accepted for MySQL surface compatibility; MyLite's byte matcher has no
  separate Unix-line-ending mode yet

If both `c` and `i` are present, the rightmost of those flags controls case
sensitivity.

## Pattern Support

The first MyLite implementation uses an internal portable matcher rather than
adding an ICU or PCRE dependency. It supports the pattern constructs expected by
common MySQL application predicates:

- literal characters
- `.`
- `^` and `$`
- alternation with `|`
- parenthesized groups
- `*`, `+`, `?`, `{m}`, `{m,}`, and `{m,n}` repetition
- bracket classes, ranges, and negation: `[abc]`, `[a-z]`, `[^0-9]`
- POSIX-style classes inside brackets for common classes such as
  `[[:alpha:]]`, `[[:digit:]]`, `[[:alnum:]]`, and `[[:space:]]`
- escape classes `\d`, `\D`, `\w`, `\W`, `\s`, and `\S`
- escaped metacharacters

MySQL uses ICU regular expressions and collation-aware character comparison.
MyLite's initial matcher is byte-oriented with ASCII case folding. Non-ASCII
case, accent, equivalence class, and full ICU behavior remain compatibility gaps
to close when collation support is broader.

## Metadata

Predicate and `REGEXP_LIKE()` results use the existing boolean descriptor:

- field type: `LONGLONG`
- charset: binary
- display length: `1`
- decimals: `0`
- numeric and binary flags set
- nullable if any evaluated argument may be nullable

## Errors And Warnings

- Empty patterns return execution error `3685`.
- Invalid non-empty pattern syntax returns execution error `3691`.
- Invalid `REGEXP_LIKE()` `match_type` text returns execution error `1210`.
- The predicates do not produce warnings for ordinary no-match results.

## Tests

Runtime coverage must compare against the MySQL 8.4.9 observations above and
cover:

- parser acceptance for `REGEXP`, `RLIKE`, `NOT REGEXP`, and `NOT RLIKE`
- scalar function arity for two and three argument `REGEXP_LIKE()`
- no-table SELECT projection behavior
- WHERE predicate behavior over table rows
- nullable operands
- case flags, multi-line anchors, dot-newline mode
- empty-pattern, invalid-pattern, and invalid-match-type diagnostics
- result metadata for nullable and non-nullable predicate/function results
