# Parser Corpus ALTER TABLE Option-Tail Surfaces

This slice extends the existing `ALTER TABLE ... ALGORITHM` / `LOCK` support to
additional MySQL 8.4-shaped alter actions that appear in the server-test query
corpus. MyLite already has normalized AST storage for these options and action
specific runtime validators for many alter paths; the remaining gap is parser
admission for several standalone productions.

Primary MySQL reference:

- https://dev.mysql.com/doc/refman/8.4/en/alter-table.html

## Scope

Admit the existing comma-separated option tail:

```sql
, ALGORITHM [=] DEFAULT|INSTANT|INPLACE|COPY
, LOCK [=] DEFAULT|NONE|SHARED|EXCLUSIVE
```

on these standalone `ALTER TABLE` forms:

- `RENAME [TO|AS] table_name`
- `ALTER [COLUMN] column SET DEFAULT value`
- `ALTER [COLUMN] column DROP DEFAULT`
- `ALTER [COLUMN] column SET VISIBLE|INVISIBLE`
- `ALTER TABLE ...` default character set/collation actions
- `CONVERT TO CHARACTER SET|CHARSET ...`
- `ADD CHECK`, `DROP CHECK`, and `ALTER CHECK`

The slice also admits `ALTER TABLE table_name ALGORITHM=...` / `LOCK=...`
statements with known option values as utility no-op placeholders when no actual
alter action is present, matching the current embedded placeholder policy for
server-managed utility statements.

## Semantics

For real alter actions, `ALGORITHM` and `LOCK` remain compatibility assertions
only. They are stored on the existing AST node, then the existing runtime path
decides whether the requested combination is accepted, rejected with a
MySQL-shaped diagnostic, or rejected with a deterministic MyLite unsupported
diagnostic.

This slice does not add MySQL online-DDL scheduling, metadata locks, concurrent
DML behavior, or new physical algorithms. It also does not widen the semantic
support of the underlying alter actions.

Action-less option-only statements are accepted as utility no-ops with the
standard embedded utility warning because they express server scheduling
preferences without a table mutation. They do not validate table existence or
mutate catalogs.

## MyLite Grammar Snippets

These Lemon-style snippets are independently authored for MyLite:

```lemon
alter_table_rename_statement ::=
    ALTER TABLE table_name RENAME table_rename_connector_opt table_name
    alter_table_option_tail_opt.

alter_table_set_default_statement ::=
    ALTER TABLE table_name ALTER column_keyword_opt identifier SET DEFAULT value
    alter_table_option_tail_opt.

alter_table_drop_default_statement ::=
    ALTER TABLE table_name ALTER column_keyword_opt identifier DROP DEFAULT
    alter_table_option_tail_opt.

alter_table_column_visibility_statement ::=
    ALTER TABLE table_name ALTER column_keyword_opt identifier SET VISIBLE
    alter_table_option_tail_opt.

alter_table_default_charset_collation_statement ::=
    ALTER TABLE table_name alter_table_default_charset_collation_option_list
    alter_table_option_tail_opt.

alter_table_convert_character_set_statement ::=
    ALTER TABLE table_name CONVERT TO charset_target alter_table_option_tail_opt.

alter_table_check_statement ::=
    ALTER TABLE table_name ADD check_constraint_definition alter_table_option_tail_opt.
alter_table_check_statement ::=
    ALTER TABLE table_name DROP CHECK identifier alter_table_option_tail_opt.
alter_table_check_statement ::=
    ALTER TABLE table_name ALTER CHECK identifier check_enforcement_required
    alter_table_option_tail_opt.
```

## Tests

Coverage includes parser tests for each newly admitted action and a corpus
benchmark rerun to confirm the `ALGORITHM` bucket moves. Runtime behavior is
covered where existing action validators already define the result; placeholder
option-only statements are covered by utility no-op tests.

## Compatibility Status

This moves selected valid MySQL syntax from parser errors to parser acceptance
or embedded no-op placeholders. It does not mark broad online-DDL behavior as
supported.
