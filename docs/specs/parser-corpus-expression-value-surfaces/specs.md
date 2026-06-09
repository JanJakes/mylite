# Parser Corpus Expression Value Surfaces

This slice reduces high-volume MySQL server-test parser failures where MyLite
already has a general expression grammar but selected statement subgrammars still
accept only narrow literal/value lists. It also adds explicit parser markers for
full-text search and `GROUP BY ... WITH ROLLUP` constructs seen in the corpus.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/set-variable.html
- https://dev.mysql.com/doc/refman/8.4/en/user-variables.html
- https://dev.mysql.com/doc/refman/8.4/en/fulltext-natural-language.html
- https://dev.mysql.com/doc/refman/8.4/en/fulltext-boolean.html
- https://dev.mysql.com/doc/refman/8.4/en/group-by-modifiers.html
- https://dev.mysql.com/doc/refman/8.4/en/spatial-function-reference.html

## Scope

### SET expression values

MySQL permits `SET @var = expr`, and the expression may be simple or complex.
System-variable assignment also accepts expression-shaped values, subject to
variable-specific validation and all-or-nothing error handling. MyLite already
has handle-local user variables and selected mutable system variables, but its
parser value rules are narrower than the surrounding expression grammar.

This slice admits targeted expression-shaped values for user-variable and
system-variable assignment at parse time without replacing the full `SET`
value grammar with the general expression grammar. Runtime behavior remains
owned by the existing variable executor:

- supported variables and supported expression values keep their current
  behavior;
- unsupported expression values fail with deterministic MyLite diagnostics;
- no global or persisted system-variable storage is added;
- no stored-program local variable binding is added.

### Predicate value expressions

MySQL permits general expressions in comparison, `BETWEEN`, and `IN` value
positions. MyLite's predicate executor still supports only documented
descriptor-backed expression subsets, but the parser should accept more MySQL
syntax so the analyzer/runtime can produce a compatibility diagnostic instead
of a syntax error.

This slice broadens parse-time value positions conservatively for:

- decimal and approximate numeric right-hand comparison operands;
- decimal and approximate numeric `BETWEEN` lower and upper bounds;
- decimal and approximate numeric literal-list `IN` / `NOT IN` values;
- row-scalar expressions using those widened literal-list values.

The runtime remains limited to current descriptor predicates, scalar-literal
predicates, row-scalar predicate slices, and metadata predicate support. Broader
expression operands are deferred until the predicate grammar can admit them
without conflicts and without silently evaluating unsupported semantics.

### DML value expressions

The previous expression/window parser slice intentionally avoided broad
`insert_value ::= expression` and `update_value ::= expression` rules because
they introduce ambiguity in MyLite's Lemon grammar. This slice keeps that
constraint and admits targeted high-volume expression surfaces:

- generic function calls in `INSERT` / `REPLACE` row values and `SET` forms;
- generic function calls in single-table `UPDATE` assignments;
- parenthesized DML values;
- bitwise-not and bitwise-or constant value expressions;
- `ROW(...)` constructors where MySQL treats them as expressions.

Runtime support remains the current descriptor-owned DML conversion subset.
Unknown generic functions, spatial constructors, and unsupported expression
operators fail when the executor reaches them.

### Full-text MATCH AGAINST syntax

MySQL full-text search uses `MATCH(column[, ...]) AGAINST (expr [modifier])`.
Natural-language mode is the default or may be explicit. Boolean mode and query
expansion use modifiers inside `AGAINST(...)`.

MyLite already stores metadata-only `FULLTEXT` index descriptors, but it does
not implement tokenization, document-id storage, relevance ranking, or full-text
index scans. This slice admits the expression syntax as a parser placeholder.
Execution remains unsupported and must produce an explicit diagnostic if a
statement reaches runtime.

### GROUP BY WITH ROLLUP

MySQL `WITH ROLLUP` appends super-aggregate rows to grouped output. Correct
execution requires rollup grouping levels, `NULL` placeholder rows, and
`GROUPING()` behavior. MyLite's current grouped executor does not implement
those semantics.

This slice admits `GROUP BY key [, ...] WITH ROLLUP` and records a ROLLUP marker
in the AST. The grouped executor rejects the marker with a clear unsupported
diagnostic instead of treating it as an ordinary group key. The alternative
`GROUP BY ROLLUP(expr[, ...])` syntax and `GROUPING()` execution remain
deferred.

### Spatial function syntax

MySQL exposes many spatial constructors, conversion functions, predicates, and
operators. MyLite has spatial column descriptors and spatial index metadata
placeholders, but no geometry value implementation. Because the expression
grammar already has generic function nodes, this slice focuses on allowing those
generic calls in `SET` and DML value positions. Runtime support for geometry
values, SRIDs, spatial comparisons, spatial indexes, and SRS diagnostics remains
unsupported.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned Lemon grammar shape and do
not copy MySQL grammar.

```lemon
set_system_variable_value ::= set_expression_value.
user_variable_set_value ::= set_expression_value.

set_expression_value ::= dml_function_call.
set_expression_value ::= LOG10 LPAREN expression RPAREN PLUS set_expression_literal.
set_expression_value ::= UNIX_TIMESTAMP LPAREN expression RPAREN.
set_expression_value ::= cast_convert_expression.
set_expression_value ::= LPAREN select_statement RPAREN.

predicate_comparison_value ::= DECIMAL.
predicate_comparison_value ::= FLOAT.
predicate_range_value ::= DECIMAL.
predicate_range_value ::= FLOAT.
predicate_in_value ::= DECIMAL.
predicate_in_value ::= FLOAT.

predicate_atom ::= predicate_row_scalar_expression IN LPAREN predicate_in_value_list RPAREN.
predicate_atom ::= predicate_row_scalar_expression NOT IN LPAREN predicate_in_value_list RPAREN.

insert_value ::= dml_expression_value.
update_value ::= dml_expression_value.

dml_expression_value ::= IDENTIFIER LPAREN RPAREN.
dml_expression_value ::= IDENTIFIER LPAREN function_argument_list RPAREN.
dml_expression_value ::= ROW LPAREN function_argument_list RPAREN.
dml_expression_value ::= LPAREN dml_function_call RPAREN.
dml_expression_value ::= BITWISE_NOT integer_literal.
dml_expression_value ::= integer_literal BITWISE_OR integer_literal.
```

```lemon
expression ::= match_against_expression.
predicate_atom ::= match_against_expression.
predicate_atom ::= match_against_expression predicate_comparison_operator predicate_comparison_value.

match_against_expression ::=
    MATCH LPAREN match_column_list RPAREN AGAINST LPAREN expression
    fulltext_search_modifier_opt RPAREN.

match_column_list ::= qualified_identifier.
match_column_list ::= match_column_list COMMA qualified_identifier.

fulltext_search_modifier_opt ::= .
fulltext_search_modifier_opt ::= IN NATURAL LANGUAGE MODE.
fulltext_search_modifier_opt ::= IN NATURAL LANGUAGE MODE WITH QUERY EXPANSION.
fulltext_search_modifier_opt ::= IN BOOLEAN MODE.
fulltext_search_modifier_opt ::= WITH QUERY EXPANSION.
```

```lemon
group_clause_opt ::= GROUP BY group_key_list WITH ROLLUP.
```

## Runtime Behavior

This is primarily a parser-compatibility slice. It should not claim support for
full expression execution where MyLite does not yet implement MySQL-equivalent
semantics.

- SET and DML behavior uses the existing variable and descriptor conversion
  runtime paths.
- Unsupported DML expressions, generic functions, spatial values, full-text
  searches, and ROLLUP queries fail with deterministic unsupported diagnostics.
- No SQLite fork hook is needed. The implementation is MyLite wrapper/parser
  work plus runtime guards.

## Tests

MySQL 8.4.9 syntax expectations are verified with a focused shell script that
checks representative statements parse and execute or fail for semantic reasons
in MySQL. MyLite parser tests cover the AST shapes and parse acceptance for the
same syntax classes:

- complex `SET` expression values;
- comparison, range, and `IN` expression values;
- targeted DML expression values and spatial/generic function calls;
- `MATCH ... AGAINST` modifier forms;
- `GROUP BY ... WITH ROLLUP` marker parsing.

The parser corpus benchmark over the WordPress mysql-on-sqlite
`mysql-server-tests-queries.csv` must be rerun before commit to measure accepted
query movement.

## Compatibility Status

This slice moves several syntax surfaces from unsupported syntax to parser
acceptance or explicit runtime placeholder diagnostics. It does not mark broad
expression DML, full-text search, spatial function semantics, or ROLLUP
execution as supported.
