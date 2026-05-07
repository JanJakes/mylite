# REGEXP Scalar Functions

## Scope

This feature adds the scalar regular-expression functions that complement the
already supported `REGEXP_LIKE()` predicate family:

- `REGEXP_INSTR(expr, pat[, pos[, occurrence[, return_option[, match_type]]]])`
- `REGEXP_SUBSTR(expr, pat[, pos[, occurrence[, match_type]]])`
- `REGEXP_REPLACE(expr, pat, repl[, pos[, occurrence[, match_type]]])`

The first implementation targets the same byte/ASCII regular-expression matcher
already used by `REGEXP`, `RLIKE`, and `REGEXP_LIKE()`. Full ICU Unicode,
collation-aware matching, locale-sensitive character classes, and binary-string
rejection remain compatibility gaps shared with the existing predicate support.

## Sources

The behavior is specified from the MySQL 8.4 Reference Manual regular
expression documentation and MySQL 8.4.9 runtime observations. The
implementation is independently authored and does not use external
implementation sources.

Official reference:

- https://dev.mysql.com/doc/refman/8.4/en/regexp.html

## MySQL 8.4.9 Observations

The following probes were run against `mysql:8.4.9`:

```sql
SELECT
  REGEXP_INSTR('dog cat dog','dog'),
  REGEXP_INSTR('dog cat dog','dog',2),
  REGEXP_INSTR('dog cat dog','dog',1,2),
  REGEXP_INSTR('dog cat dog','dog',1,2,1),
  REGEXP_INSTR('abc','z'),
  REGEXP_SUBSTR('abc def ghi','[a-z]+'),
  REGEXP_SUBSTR('abc def ghi','[a-z]+',1,3),
  REGEXP_SUBSTR('abc','z'),
  REGEXP_REPLACE('a b c','b','X'),
  REGEXP_REPLACE('abc def ghi','[a-z]+','X',1,3),
  REGEXP_REPLACE('abc def ghi','[a-z]+','X');
```

Result:

```text
1 9 9 12 0 abc ghi NULL "a X c" "abc def X" "X X X"
```

Additional runtime observations:

- Any `NULL` argument, including optional `pos`, `occurrence`,
  `return_option`, and `match_type`, makes the result `NULL`.
- `REGEXP_INSTR()` returns a one-based start position, `0` when there is no
  match, and a one-based position after the match when `return_option` is `1`.
- `REGEXP_SUBSTR()` returns `NULL` for no match.
- `REGEXP_REPLACE()` returns the original string for no match.
- `REGEXP_REPLACE(..., occurrence => 0)` replaces all matches. Negative
  occurrence values behave like occurrence `1`.
- `REGEXP_INSTR()` and `REGEXP_SUBSTR()` treat occurrence values less than or
  equal to zero as occurrence `1`.
- Matches do not overlap. `REGEXP_INSTR('aaaa','aa',1,2)` returns `3`.
- Patterns that match an empty string are valid. `REGEXP_SUBSTR('bbb','a*')`
  returns the empty string and `REGEXP_REPLACE('bbb','a*','X')` returns
  `XbXbXbX`; repeated searches advance by one character after a zero-length
  match.
- Greedy repetition is visible in returned substrings:
  `REGEXP_SUBSTR('aaa','a+')` returns `aaa`.
- Alternation keeps branch order: `REGEXP_SUBSTR('ab','a|ab')` returns `a`,
  while `REGEXP_SUBSTR('ab','ab|a')` returns `ab`.
- Invalid `match_type` text is an execution error `1210` with a function-
  specific message such as `Incorrect arguments to regexp_substr`.
- Empty patterns return error `3685`; malformed patterns return error `3691`.
- Invalid `return_option` values for `REGEXP_INSTR()` return error `1210`.
- `REGEXP_INSTR()` rejects `pos` outside the searchable string with error
  `3686`. `REGEXP_SUBSTR()` and `REGEXP_REPLACE()` allow `pos = length + 1`
  but reject larger positions with error `3686`; `pos <= 0` uses error `1583`
  for those two functions.
- Metadata for `REGEXP_INSTR()` is nullable `LONGLONG`, binary collation,
  display length `21`, decimals `0`, and numeric/binary flags.
- Metadata for `REGEXP_SUBSTR()` is a nullable character string using the input
  expression's character semantics and source length where known. Under the
  default `utf8mb4` result character set, `REGEXP_SUBSTR('abc','a')` reports
  length `12`, and `REGEXP_SUBSTR(varchar_32_column,'[a-z]+')` reports length
  `128`.
- Under the default `utf8mb4` result character set, `REGEXP_REPLACE()` reports
  `LONG_BLOB`, collation `utf8mb4_0900_ai_ci` (`255`), length `67108864`,
  decimals `31`, no flags, and nullable metadata. Replacements can grow the
  input, so the descriptor is wider than the source expression descriptor.

## Syntax

No new grammar production is required. The functions use MyLite's ordinary
function-call path:

```lemon
scalar_expression(A) ::= function_name(B) LP opt_function_arguments(C) RP. {
    A = mylite_sql_parser_make_function_call(state, B, C);
}
```

The arity rules are:

- `REGEXP_INSTR`: 2 to 6 arguments
- `REGEXP_SUBSTR`: 2 to 5 arguments
- `REGEXP_REPLACE`: 3 to 6 arguments

## Semantics

Evaluation follows the existing scalar function model:

1. Evaluate arguments left-to-right.
2. If any supplied argument is `NULL`, return `NULL`.
3. Convert `expr`, `pat`, `repl`, and `match_type` to strings using existing
   MyLite string coercion.
4. Convert `pos`, `occurrence`, and `return_option` to signed integers using
   existing integer coercion, preserving conversion warnings.
5. Validate function-specific positional arguments.
6. Compile the pattern once for the function call and search the subject string
   from the requested one-based position.

`match_type` uses the same option handling as `REGEXP_LIKE()`:

- `c`: case-sensitive matching
- `i`: case-insensitive matching
- `m`: multi-line `^` and `$`
- `n`: `.` matches newline
- `u`: accepted for surface compatibility

If both `c` and `i` are present, the rightmost flag controls case sensitivity.

`REGEXP_INSTR()` returns:

- `NULL` when any supplied argument is `NULL`
- `0` when no requested occurrence exists
- the match start position when `return_option` is omitted or `0`
- the position following the match when `return_option` is `1`

`REGEXP_SUBSTR()` returns:

- `NULL` when any supplied argument is `NULL`
- `NULL` when no requested occurrence exists
- the matched substring otherwise

`REGEXP_REPLACE()` returns:

- `NULL` when any supplied argument is `NULL`
- the original string when no requested occurrence exists
- a copy with all matches replaced when `occurrence` is `0`
- a copy with only the requested occurrence replaced otherwise

Replacement text is copied literally. MySQL 8.4.9 runtime probes with
backslash-number text returned literal digits rather than captured groups for
the tested expression; capture-group replacement remains outside the first
implemented surface unless a broader MySQL replacement-token rule is added.

## Metadata

`REGEXP_INSTR()`:

- field type: `LONGLONG`
- charset: binary
- display length: `21`
- decimals: `0`
- numeric and binary flags set
- nullable if any supplied argument may be nullable

`REGEXP_SUBSTR()`:

- field type: character string
- charset/collation: connection or source-compatible charset used by current
  expression descriptor rules
- length: source expression length when known, such as `12` for an `utf8mb4`
  three-character literal and `128` for an `utf8mb4 VARCHAR(32)` column
- decimals: `31`
- nullable: always, because no match returns `NULL`

`REGEXP_REPLACE()`:

- field type: `LONG_BLOB` for the covered `utf8mb4` result-character-set path
- charset/collation: current connection result character set/collation
- length: `67108864` bytes under `utf8mb4`
- decimals: `31`
- flags: none for the covered `utf8mb4` path
- nullable: yes

## Errors And Warnings

- Empty patterns return error `3685`.
- Malformed patterns return error `3691`.
- Invalid `match_type` returns error `1210` with the function name in the
  message.
- Invalid `REGEXP_INSTR()` `return_option` returns error `1210`.
- Out-of-bounds search positions return error `3686`.
- `REGEXP_SUBSTR()` and `REGEXP_REPLACE()` `pos <= 0` return error `1583`.
- String-to-integer conversion warnings on numeric optional arguments use the
  existing MyLite conversion warning path.

## Tests

Runtime coverage must include:

- parser acceptance through ordinary scalar calls
- no-table `SELECT` projection for all three functions
- table projection, `WHERE`, `ORDER BY`, `UPDATE`, and `DELETE` expression paths
- `NULL` propagation across required and optional arguments
- occurrence, starting-position, and `return_option` behavior
- replace-all and replace-one behavior
- no-match behavior for all three functions
- zero-length match progression
- greedy repetition and branch-order behavior for returned spans
- invalid pattern, empty pattern, invalid match type, invalid return option,
  and invalid position diagnostics
- result metadata for numeric and text results
