# Parser Corpus DDL Constraint and Option Placeholders

This slice reduces repeated MySQL server-test parser failures around extended
`CREATE TABLE` / `ALTER TABLE` constraint syntax, storage-engine option
surfaces, column storage attributes, and CHECK expressions that are outside
MyLite's current executable CHECK subset.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- https://dev.mysql.com/doc/refman/8.4/en/create-table-check-constraints.html

## MySQL 8.4.9 Observations

The focused MySQL probe used a `mysql:8.4.9` runtime. It confirmed:

- `CREATE TABLE ... ENCRYPTION='N'` succeeds on the default InnoDB setup.
- `CREATE TABLE ... COLUMN_FORMAT ...` and column `STORAGE DISK|MEMORY` syntax
  is accepted.
- `CREATE TABLE ... DATA DIRECTORY=... INDEX DIRECTORY=...` is accepted by the
  parser and may fail later with an environment/path diagnostic.
- `SECONDARY_ENGINE=...`, `SECONDARY_ENGINE ...`, and column `NOT SECONDARY`
  syntax is accepted by the MySQL parser.
- bare `CONSTRAINT PRIMARY KEY (...)` and bare
  `CONSTRAINT FOREIGN KEY (...) REFERENCES ...` are accepted; MySQL treats the
  omitted symbol as an auto-generated name.
- `ALTER TABLE ... ALTER CONSTRAINT name [NOT] ENFORCED` is accepted for CHECK
  constraints and maps to the same enforcement state as MySQL's
  `ALTER CHECK` form.
- CHECK expressions may use ordinary expression forms such as `IN`, `NOT IN`,
  and interval arithmetic. MyLite does not yet execute that full expression
  surface.

## Scope

### Executable constraint syntax

MyLite now parses these MySQL 8.4 forms through the normal Lemon grammar and
routes them to existing descriptor-owned runtime paths:

- `CONSTRAINT PRIMARY KEY (...)` in `CREATE TABLE`;
- `ALTER TABLE ... ADD CONSTRAINT PRIMARY KEY (...)`;
- `CONSTRAINT FOREIGN KEY (...) REFERENCES ...` in `CREATE TABLE`;
- `ALTER TABLE ... ADD CONSTRAINT FOREIGN KEY (...) REFERENCES ...`;
- `ALTER TABLE ... ALTER CONSTRAINT name ENFORCED`;
- `ALTER TABLE ... ALTER CONSTRAINT name NOT ENFORCED`;
- the `ALTER CONSTRAINT` form inside the limited multi-action parser surface.

The primary-key, foreign-key, and CHECK enforcement runtime limits remain the
existing MyLite limits. This slice only admits the omitted-symbol constraint
spelling and the standard `ALTER CONSTRAINT` spelling when the target operation
already maps to an implemented runtime operation.

### Unsupported option placeholders

MySQL accepts storage-engine and table-option syntax that MyLite cannot execute
without creating misleading metadata or storage behavior. These are accepted
only after the normal parser rejects the statement, and then become
`unsupported_utility_statement` nodes:

- table `ENCRYPTION`;
- table `SECONDARY_ENGINE`;
- table `DATA DIRECTORY` / `INDEX DIRECTORY`;
- column `STORAGE DISK|MEMORY`;
- column `COLUMN_FORMAT FIXED|DYNAMIC|DEFAULT`;
- column `NOT SECONDARY`;
- CHECK expressions with currently unsupported parser forms such as `IN`,
  `NOT IN`, or interval expressions.

Runtime execution returns the standard unsupported utility diagnostic and must
not mutate descriptors, data, auto-increment state, physical SQLite schema, or
transactions. This is deliberate: returning success would hide storage-engine
and CHECK semantics that applications might depend on.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned Lemon grammar shape and do
not copy MySQL grammar.

```lemon
named_primary_key_definition ::= CONSTRAINT primary_key_definition.

constraint_name_opt ::= CONSTRAINT.
```

```lemon
alter_table_alter_check_statement ::=
    ALTER TABLE table_name ALTER CONSTRAINT identifier check_enforcement_required.

alter_table_multi_action ::=
    ALTER CONSTRAINT identifier check_enforcement_required.
```

The extended option and broad CHECK-expression forms are intentionally handled
by the post-parse placeholder classifier in this slice:

```lemon
unsupported_utility_statement ::=
    CREATE TABLE table_name create_table_tail_containing_extended_option.

unsupported_utility_statement ::=
    ALTER TABLE table_name alter_table_tail_containing_extended_option.
```

## Runtime Behavior

No SQLite fork hook is required. Executable constraint syntax reuses the current
MyLite primary-key, foreign-key, and CHECK descriptor paths. Unsupported option
and broad CHECK-expression surfaces use the existing unsupported utility
statement runtime path, which returns `1064 / 42000` with a MyLite-owned
diagnostic.

Placeholder statements are admitted only when all of these are true:

- the statement starts as `CREATE [TEMPORARY] TABLE` or `ALTER TABLE`;
- parentheses are balanced;
- the token tail is not obviously incomplete;
- a known extended-option or unsupported CHECK-expression marker is present.

Malformed statements such as `ENCRYPTION =`, `COLUMN_FORMAT`, or
`ALTER CONSTRAINT` without a name remain syntax errors.

## Tests

The MySQL expectation script verifies representative MySQL 8.4.9 behavior for
the admitted syntax and later-runtime diagnostics. MyLite tests cover:

- parser acceptance and runtime behavior for bare constraint primary keys and
  foreign keys;
- parser acceptance and runtime behavior for `ALTER CONSTRAINT`;
- parser acceptance as unsupported utility placeholders for engine/column
  option forms and broader CHECK expressions;
- malformed-tail syntax-error guards;
- runtime unsupported diagnostics without durable schema mutation for
  placeholder DDL.

The parser corpus benchmark over the WordPress mysql-on-sqlite
`mysql-server-tests-queries.csv` improved from 690 parse failures before this
slice to 639 after the focused implementation.

## Compatibility Status

This slice improves MySQL syntax compatibility without claiming new storage
engine behavior, secondary-engine behavior, column-storage placement,
data-directory/index-directory storage, or full CHECK expression execution.
