# NO_BACKSLASH_ESCAPES SQL mode

## Scope

This feature aligns MyLite string-literal decoding and `LIKE` pattern escaping
with MySQL 8.4.9 when `NO_BACKSLASH_ESCAPES` is present in the connection SQL
mode.

In scope:

- parsing prepared statements using the current connection SQL mode
- preserving the SQL-mode flag on string literal AST nodes
- scalar string literal evaluation
- DML string literal storage paths
- scalar `LIKE` and `NOT LIKE`
- explicit `LIKE ... ESCAPE`
- `SHOW ... LIKE` filters that use string literals

Out of scope:

- non-ASCII collation equivalence in `LIKE`
- full character-set conversion
- SQL modes whose token boundaries are not affected by string quote handling

## Sources

- MySQL 8.4 Reference Manual, String Literals:
  https://dev.mysql.com/doc/refman/8.4/en/string-literals.html
- MySQL 8.4 Reference Manual, Pattern Matching:
  https://dev.mysql.com/doc/refman/8.4/en/pattern-matching.html
- MySQL 8.4 Reference Manual, Server SQL Modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`, using `docker exec -i mylite-mysql-849 mysql -uroot`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 Behavior

With the default SQL mode, backslash escape sequences inside string literals
are decoded before expression evaluation. The observed probe:

```sql
SET SESSION sql_mode = '';
SELECT HEX('a\0b'), LENGTH('a\0b'),
       'a_c' LIKE 'a\_c',
       'a\_c' LIKE 'a\_c';
```

returns:

| Expression | Result |
| --- | --- |
| `HEX('a\0b')` | `610062` |
| `LENGTH('a\0b')` | `3` |
| `'a_c' LIKE 'a\_c'` | `1` |
| `'a\_c' LIKE 'a\_c'` | `0` |

With `NO_BACKSLASH_ESCAPES`, backslash is ordinary string content and is not
the default `LIKE` escape character. The observed probe:

```sql
SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES';
SELECT HEX('a\0b'), LENGTH('a\0b'),
       'a_c' LIKE 'a\_c',
       'a\_c' LIKE 'a\_c',
       'a_c' LIKE 'a\_c' ESCAPE CHAR(92);
```

returns:

| Expression | Result |
| --- | --- |
| `HEX('a\0b')` | `615C3062` |
| `LENGTH('a\0b')` | `4` |
| `'a_c' LIKE 'a\_c'` | `0` |
| `'a\_c' LIKE 'a\_c'` | `1` |
| `'a_c' LIKE 'a\_c' ESCAPE CHAR(92)` | `1` |

`SHOW ... LIKE` filters follow the same pattern rule. Under the default SQL
mode, `SHOW VARIABLES LIKE 'character\_set\_%'` treats backslash as the escape
character and returns the character-set variables. Under
`NO_BACKSLASH_ESCAPES`, the same pattern searches for literal backslashes and
returns no rows; `SHOW VARIABLES LIKE 'character_set_%'` matches normally.

## MyLite Design

Statement preparation must derive parser flags from the connection SQL mode.
The parser marks string literal and `LIKE` expression nodes that were parsed
while `NO_BACKSLASH_ESCAPES` was active.

Runtime decoding must respect that AST flag:

- scalar expression evaluation leaves backslash sequences unchanged
- DML storage paths copy the literal bytes without converting `\0` to a NUL
- `LIKE` uses no default escape character when the pattern node was parsed
  with `NO_BACKSLASH_ESCAPES`
- explicit `ESCAPE` expressions still provide the escape character
- `SHOW ... LIKE` generated SQLite filters add `ESCAPE '\'` only for patterns
  parsed without `NO_BACKSLASH_ESCAPES`

The lexer still owns token-boundary behavior. Runtime layers own value
decoding because SQL modes can affect how the same quoted bytes become scalar
values.

## Lemon Grammar Snippet

The grammar for `LIKE` does not change:

```text
comparison_expression ::= comparison_expression LIKE bit_or_expression opt_like_escape.
comparison_expression ::= comparison_expression NOT LIKE bit_or_expression opt_like_escape.
opt_like_escape ::= .
opt_like_escape ::= ESCAPE bit_or_expression.
```

The mode-sensitive behavior is carried as AST metadata rather than as a
separate grammar production.

## Tests

Runtime tests compare MyLite against MySQL 8.4.9-observed behavior for:

- scalar literal `HEX()` and `LENGTH()` with `\0`
- default-mode and `NO_BACKSLASH_ESCAPES` `LIKE` results
- explicit `LIKE ... ESCAPE CHAR(92)`
- inserted string and binary values containing literal backslash-zero bytes
- `SHOW VARIABLES LIKE` escaped and unescaped patterns under both modes

