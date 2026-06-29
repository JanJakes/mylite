# Baseline Stored Program Body Placeholders

This slice covers stored-program body constructs that MySQL 8.4.9 supports but
MyLite does not execute yet. The goal is not to implement stored-program
semantics; it is to make MyLite recognize representative stored-program
definitions and fail them with the existing deterministic unsupported
stored-program diagnostic instead of leaving the compatibility rows as raw
missing parser surface.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/sql-compound-statements.html
- https://dev.mysql.com/doc/refman/8.4/en/create-procedure.html
- https://dev.mysql.com/doc/refman/8.4/en/create-function.html
- https://dev.mysql.com/doc/refman/8.4/en/declare-local-variable.html
- https://dev.mysql.com/doc/refman/8.4/en/declare-condition.html
- https://dev.mysql.com/doc/refman/8.4/en/declare-handler.html
- https://dev.mysql.com/doc/refman/8.4/en/cursors.html

## MySQL 8.4.9 Observations

MySQL accepts compound stored-program definitions that contain:

- `BEGIN ... END` blocks and statement labels;
- `DECLARE` local variables, named conditions, cursors, and handlers in the
  documented declaration order;
- stored-program `CASE`, `IF`, `LOOP`, `REPEAT`, and `WHILE` control flow;
- `ITERATE`, `LEAVE`, `OPEN`, `FETCH`, `CLOSE`, and stored-function `RETURN`.

These observations are verified by the extended
`mysql_parser_corpus_stored_program_script_placeholders_expectations.sh` script.

## MyLite Scope

In scope:

- preserve the existing limited no-argument, single-`SELECT` stored-procedure
  bridge;
- classify representative `CREATE PROCEDURE` and `CREATE FUNCTION` definitions
  that contain the body constructs above as
  `unsupported_stored_program_statement`;
- return the existing unsupported stored-program runtime diagnostic:
  `1064 / 42000`, message containing `not supported`;
- leave catalog descriptors, user rows, transactions, and warnings untouched.

Out of scope:

- executing stored programs with parameters, local variables, handlers, cursors,
  labels, loops, branches, or return values;
- persisting general stored procedure, stored function, trigger, or event
  descriptors;
- implementing stored-program variable scope, condition propagation, cursor
  lifecycle, loop control transfer, or routine result metadata;
- parsing arbitrary body-only fragments such as `END IF` as top-level
  statements.

## Parser Direction

The existing placeholder classifier remains responsible for this slice. Normal
Lemon grammar first handles supported SQL, including the limited procedure
bridge. When normal parsing fails, the stored-program placeholder classifier
recognizes statement-level routine DDL:

```text
unsupported_stored_program_statement:
    CREATE PROCEDURE ... BEGIN ... END
  | CREATE FUNCTION ... RETURNS ... BEGIN ... RETURN ... END
```

The classifier intentionally keys on stored-program DDL, not individual body
tokens. Body-only fragments remain syntax errors.

## Compatibility Status

Rows for individual stored-program body statements move from `❌` to `⚪`.
That status means MyLite has a deliberate placeholder/error boundary for the
stored-program surface, not that body semantics are implemented.
