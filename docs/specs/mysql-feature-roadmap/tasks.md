# MySQL Compatibility Feature Roadmap

This task list orders the first 50 MySQL compatibility features by dependency
and practical application coverage. It starts with schema/session foundations,
then table definition, writes, reads, mutation, metadata inspection, and higher
level application surfaces.

## First 50 Tasks

1. [x] Schema selection and database lifecycle: `CREATE DATABASE`, `USE`,
       `ALTER DATABASE`, `DROP DATABASE`, and `SHOW DATABASES`.
2. [x] Core metadata catalog: internal schema/table/column/index storage plus
       `INFORMATION_SCHEMA.SCHEMATA`, `TABLES`, `COLUMNS`, `ENGINES`,
       `KEYWORDS`, and `STATISTICS`.
3. [x] Character set and collation foundation: `utf8mb4`, `utf8mb3`, `latin1`,
       `binary`, default charset/collation tracking, `SET NAMES`, and
       `SET CHARACTER SET`.
4. [x] Integer and boolean column types: `TINYINT`, `SMALLINT`, `MEDIUMINT`,
       `INT`, `BIGINT`, unsigned ranges, display-width compatibility,
       `BOOL`/`BOOLEAN`, and integer aliases.
5. [x] String and binary column types: `CHAR`, `VARCHAR`, `TEXT` family,
       `BINARY`, `VARBINARY`, `BLOB` family, length limits, and metadata.
6. [x] Exact and approximate numeric column types: `DECIMAL`, `NUMERIC`,
       `FLOAT`, `DOUBLE`, aliases, precision/scale, and metadata.
7. [x] Temporal column types: `DATE`, `TIME`, `DATETIME`, `TIMESTAMP`, `YEAR`,
       fractional seconds, zero values, and metadata.
8. [x] Column attributes: `NULL`, `NOT NULL`, `DEFAULT`, expression defaults,
       `ON UPDATE`, comments, visibility, and storage/format options.
9. [x] Primary keys and `AUTO_INCREMENT`: inline and table constraints,
       allocation, explicit values, metadata, and error cases.
10. [x] Unique and secondary indexes in `CREATE TABLE`: parse-only key parts,
       prefix lengths, index types, visibility, and comments.
11. [x] `CREATE TABLE` base execution: column definitions, constraints, table
       options, schema qualification, `IF NOT EXISTS`, warnings, and atomicity.
12. [x] `DROP TABLE`: multi-table drops, `IF EXISTS`, temporary table handling,
       warning behavior, metadata cleanup, and implicit commit behavior.
13. [x] `INSERT ... VALUES`: column lists, multi-row values, defaults,
       generated/default columns, affected rows, warnings, and insert ids.
14. [x] `INSERT ... SET`: assignment-form insert semantics, defaults, duplicate
       column diagnostics, affected rows, and insert ids.
15. [x] Table-backed `SELECT` core: projection aliases, qualified wildcards,
       base table references, schema qualification, and result metadata.
16. [x] Expression operator foundation: comparison, logical, bitwise, `IS NULL`,
       `BETWEEN`, `LIKE`, `IN`, arithmetic, precedence, and type conversion.
17. [x] `WHERE`: predicate evaluation, three-valued logic, conversion warnings,
       and name resolution. Spec: [WHERE clause](../where-clause/specs.md).
18. [x] `ORDER BY`, `LIMIT`, and `OFFSET`: aliases, ordinals, collations,
       integer conversion, prepared markers, and error cases. Spec:
       [ORDER BY, LIMIT, and OFFSET](../order-limit-offset/specs.md).
19. [x] Single-table `UPDATE`: assignment order, expressions, `WHERE`,
       `ORDER BY`, `LIMIT`, generated/default columns, affected rows, and
       warnings. Spec: [single-table UPDATE](../update-single-table/specs.md).
20. [x] Single-table `DELETE`: aliases, `WHERE`, `ORDER BY`, `LIMIT`,
       affected rows, and warnings. Spec:
       [single-table DELETE](../delete-single-table/specs.md).
21. [x] Transaction statements: `START TRANSACTION`, `BEGIN`, `COMMIT`,
       `ROLLBACK`, access modes, chaining, release options, and diagnostics.
       Spec: [transaction statements](../transaction-statements/specs.md).
22. [x] Savepoints: `SAVEPOINT`, `ROLLBACK TO SAVEPOINT`,
       `RELEASE SAVEPOINT`, replacement, nesting, and error behavior.
       Spec: [savepoints](../savepoints/specs.md).
23. [x] Result metadata and expression labels: type, length, flags, origin
       metadata, alias labels, duplicate labels, and nullability. Spec:
       [result metadata and expression labels](../result-metadata-expression-labels/specs.md).
24. [ ] Scalar built-in functions used by common applications: string, numeric,
       temporal, conditional, comparison, information, and compatibility
       functions with MySQL conversion behavior. Started; spec:
       [scalar built-in functions](../scalar-built-in-functions/specs.md).
25. [x] Aggregate functions and grouping: `COUNT`, `SUM`, `AVG`, `MIN`, `MAX`,
       `GROUP BY`, `HAVING`, aliases, ordinals, and `ONLY_FULL_GROUP_BY`.
       Spec: [aggregate functions and grouping](../aggregate-grouping/specs.md).
26. [x] Inner joins and comma joins: join precedence, aliases, `ON`, `USING`,
       name resolution, and result metadata. Spec:
       [inner joins](../inner-joins/specs.md).
27. [x] Outer joins: `LEFT`/`RIGHT` joins, null extension, predicate placement,
       and metadata. Spec: [outer joins](../outer-joins/specs.md).
28. [x] `DISTINCT` and `DISTINCTROW`: duplicate elimination, collation,
       metadata, and interaction with ordering/limits. Spec:
       [SELECT DISTINCT](../select-distinct/specs.md).
29. [ ] Subqueries: scalar, row, `EXISTS`, `IN`, quantified comparisons,
       correlation, cardinality errors, and metadata. Started; spec:
       [subqueries](../subqueries/specs.md).
30. [x] `UNION`: `ALL`/`DISTINCT`, column names/types, ordering, limits,
       parentheses, and metadata.
31. [ ] `INSERT IGNORE`: duplicate, conversion, and constraint warning-demotion
       rules. Partially implemented for duplicate-key and required-column
       demotion on supported `INSERT ... VALUES`/`INSERT ... SET` forms; full
       conversion/range/truncation/temporal demotion remains deferred. Spec:
       [INSERT IGNORE](../insert-ignore/specs.md).
32. [ ] `INSERT ... ON DUPLICATE KEY UPDATE`: conflict selection, assignment
       evaluation, affected rows, insert ids, aliases, and warnings. Partially
       implemented for supported `VALUES`/`SET` forms and scoped `IGNORE`;
       insert-select, partitions, generated columns, triggers, foreign keys,
       full conversion demotion, and broad expressions remain deferred. Spec:
       [INSERT ... ON DUPLICATE KEY UPDATE](../insert-on-duplicate-key-update/specs.md).
33. [ ] `REPLACE`: values, set, and select forms; delete-then-insert semantics;
       cascades; triggers; affected rows; and auto-increment behavior.
       Partially implemented for supported `VALUES`/`VALUE`/`VALUES ROW(...)`
       and `SET` forms, including optional `INTO`, column lists, explicit
       conflict delete loop, source-order multi-row semantics, affected rows,
       `LOW_PRIORITY`, `DELAYED` warning 3005, rollback, and auto-increment/
       last-insert-id behavior. Query-source replace, partitions, cascades,
       triggers, generated columns, and broad conversion fidelity remain
       deferred. Spec: [REPLACE](../replace/specs.md).
34. [ ] `CREATE INDEX` and `DROP INDEX`: standalone index creation/removal,
       options, metadata, warnings, and implicit commit behavior.
       First executable slice is implemented for metadata-backed ordinary and
       unique standalone indexes plus `DROP INDEX name ON table`, including
       parser coverage, statistics metadata, duplicate validation, warnings,
       and write-path conflict effects. Full-text, spatial, functional,
       multi-valued, optimizer, physical-index, primary-key dependency, and
       implicit-commit surfaces remain. Spec:
       [standalone CREATE INDEX and DROP INDEX](../create-drop-index/specs.md).
35. [ ] `ALTER TABLE` column operations: `ADD`, `DROP`, `RENAME`, `CHANGE`,
       `MODIFY`, defaults, positioning, validation, and metadata rewrite.
       First executable slice is implemented for existing MyLite base tables,
       including parser coverage, multi-action atomic shadow rewrites,
       default backfill, column positioning, column/index metadata rewrites,
       duplicate-index warnings, and rejection of non-default `ALGORITHM` and
       `LOCK` before mutation. Inline key/constraint creation, generated
       column dependencies, foreign-key dependencies, and full MySQL
       conversion fidelity remain deferred. Spec:
       [ALTER TABLE column operations](../alter-table-column-operations/specs.md).
36. [ ] `ALTER TABLE` key and constraint operations: primary, unique, index,
       check, and foreign-key add/drop/rename/toggle behavior. First
       executable slice is implemented for metadata-backed `ADD PRIMARY KEY`,
       `DROP PRIMARY KEY`, `ADD UNIQUE`, `ADD INDEX` / `ADD KEY`,
       `DROP INDEX` / `DROP KEY`, `RENAME INDEX` / `RENAME KEY`, and
       `ALTER INDEX` visibility on supported MyLite base tables, including
       parser coverage, duplicate validation, write-path conflict effects,
       statistics metadata, warnings, and Task 35 mixed-action atomicity.
       FULLTEXT, SPATIAL, CHECK, and FOREIGN KEY ALTER actions parse and return
       deterministic unsupported diagnostics before mutation. Full constraint
       catalogs, full-text/spatial runtime, optimizer use, and implicit-commit
       behavior remain. Spec:
       [ALTER TABLE key and constraint operations](../alter-table-key-constraint-operations/specs.md).
37. [ ] `RENAME TABLE` and `ALTER TABLE ... RENAME TO`: atomic multi-table
       rename, metadata rewrite, temporary table handling, and errors. First
       executable slice is implemented for supported MyLite base tables,
       including parser coverage, selected-schema and schema-qualified
       resolution, cross-schema moves, physical table rename, table/column/index
       metadata rewrites, `INFORMATION_SCHEMA.TABLES`, `COLUMNS`, and
       `STATISTICS` visibility, existing-target/missing-source/missing-schema
       diagnostics, and all-or-nothing multi-pair atomicity. Temporary tables,
       views, triggers, foreign keys, privileges, metadata locks, and implicit
       commits remain deferred. Spec: [RENAME TABLE](../rename-table/specs.md).
38. [ ] `TRUNCATE TABLE`: DDL-like delete, auto-increment reset, foreign-key
       restrictions, implicit commits, and affected-row behavior. First
       executable slice is implemented for supported MyLite base tables,
       including parser coverage, selected-schema and schema-qualified
       resolution, physical row removal while preserving the table object,
       table/column/index metadata preservation, `AUTO_INCREMENT` reset,
       affected rows `0`, deterministic diagnostics, and statement atomicity.
       Temporary tables, foreign-key restrictions, triggers, locks, partitions,
       privileges, Performance Schema summary-table behavior, and implicit
       commits remain deferred. Spec:
       [TRUNCATE TABLE](../truncate-table/specs.md).
39. [ ] `SHOW TABLES`, `SHOW COLUMNS`, `SHOW INDEX`, `DESCRIBE`,
       `SHOW CREATE TABLE`, and `SHOW ENGINES`: result-set shape, filtering,
       metadata formatting, and warnings. First `SHOW TABLES` slice is
       implemented for supported
       MyLite base tables and existing `information_schema` metadata views,
       including `EXTENDED`, `FULL`, `FROM`/`IN`, `LIKE`, `WHERE` grammar,
       result column names, table type, empty results, and
       selected/missing-schema diagnostics. `EXTENDED` is currently a no-op
       because MyLite has no hidden failed-ALTER table catalog; `WHERE`
       filtering is parsed and rejected as unsupported until SHOW expression
       filtering lands. The first `SHOW TABLE STATUS` slice is specified and
       implemented for supported persistent MyLite base tables and existing
       `information_schema` metadata views, including `FROM`/`IN`, `LIKE`,
       `WHERE` grammar, exact MySQL status columns, catalog-backed table
       metadata, deterministic `0` placeholders for user base-table storage
       counters until physical statistics are maintained, empty results,
       information-schema pattern normalization, and selected/missing-schema
       diagnostics. `SHOW TABLE STATUS WHERE` is parsed but currently returns
       an unsupported diagnostic. The first `SHOW COLUMNS` / `SHOW FIELDS` slice is also
       specified and implemented for supported persistent base tables, including
       `EXTENDED`, `FULL`, `FROM`/`IN`, `db.table`, `LIKE`, `WHERE` grammar,
       `FIELDS` synonym support, non-`FULL` and `FULL` result-set shapes,
       catalog-backed column metadata, and selected-schema, missing-schema, and
       missing-table diagnostics. `SHOW COLUMNS WHERE` and system-view column
       descriptions are parsed but currently return unsupported diagnostics.
       The first `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS` slice is specified
       and implemented for persistent base-table indexes in
       `__mylite_index_catalog`, including `EXTENDED`, `FROM`/`IN`, `db.table`,
       explicit-schema override, `WHERE` grammar, synonym support,
       MySQL-compatible result column names/order, catalog-backed primary,
       unique, nonunique, multi-part, prefix, collation, comment, visibility,
       nullable, and index-type metadata, empty no-index results, known
       `information_schema` table behavior, and selected-schema,
       missing-schema, missing-table, and unknown-system-table diagnostics.
       `SHOW INDEX WHERE` is parsed but currently returns an unsupported
       diagnostic; `EXTENDED` is a visible-catalog no-op until hidden
       storage-engine key parts exist. Temporary tables, user views, privilege
       filtering, hidden storage-engine columns, generated invisible primary key
       rows, functional key parts, and broader metadata statements remain
       deferred. The first `DESCRIBE` / `DESC` slice is specified and
       implemented for supported persistent base tables, including the
       `EXPLAIN tbl_name [col_name | wild]` table-description synonym,
       schema-qualified targets, optional identifier/string-literal column and
       wildcard filtering, the standard six-column `SHOW COLUMNS` result shape,
       catalog-backed values, and selected-schema, missing-schema,
       missing-table, and unknown-system-table diagnostics. `information_schema`
       table descriptions currently return an unsupported diagnostic, and
       broader query-plan `EXPLAIN` syntax remains deferred. The first
       `SHOW CREATE DATABASE` / `SHOW CREATE SCHEMA` slice is specified and
       implemented for schema catalog rows, including the `IF NOT EXISTS`
       display modifier, exact `Database` / `Create Database` result columns,
       catalog-backed charset/collation/encryption rendering, system-schema
       rows, backtick identifier quoting, no selected-schema requirement, and
       unknown-database diagnostics. `LIKE` and `WHERE` suffixes are
       syntactically rejected for now. The first `SHOW CREATE TABLE` slice is
       specified and implemented for supported persistent MyLite base tables,
       including selected-schema and
       schema-qualified targets, two-column `Table` / `Create Table` result
       shape, catalog-backed deterministic CREATE text for the supported
       column, primary-key, unique-key, nonunique-key, visibility, comments,
       charset/collation, auto-increment, and table-option subset, backtick
       identifier quoting, and selected-schema, missing-schema, missing-table,
       known-system-table, and unknown-system-table diagnostics. Views,
       temporary tables, foreign keys, checks, generated columns, partitions,
       functional indexes, storage-engine-specific options, privilege
       filtering, `SHOW CREATE VIEW`, and `sql_quote_show_create = 0` remain
       deferred. Specs: [SHOW CREATE DATABASE](../show-create-database/specs.md),
       [SHOW CREATE TABLE](../show-create-table/specs.md). The first
       `SHOW ENGINES` slice is specified for
       `SHOW [STORAGE] ENGINES`, including exact six-column metadata,
       nondiagnostic clearing, `InnoDB` as the default MyLite SQLite-backed
       transactional facade, common unsupported MySQL engines reported as
       `Support=NO` with nullable capability columns, and MySQL-compatible
       syntax rejection for `LIKE`, `WHERE`, and `LIMIT`. The first
       `INFORMATION_SCHEMA.ENGINES` slice shares that registry for wildcard
       selection with exact uppercase columns and case-insensitive quoted name
       resolution; projections, filters, ordering, limits, aliases, joins, and
       aggregates remain deferred. The first `INFORMATION_SCHEMA.CHARACTER_SETS`
       slice shares the supported character-set registry used by
       `SHOW CHARACTER SET`, exposes exact uppercase columns and numeric
       `MAXLEN`, and preserves the same wildcard-only information-schema query
       policy. The first `INFORMATION_SCHEMA.COLLATIONS` slice shares the
       supported collation registry used by `SHOW COLLATION`, exposes exact
       uppercase columns with numeric `ID` and `SORTLEN`, and preserves the
       same wildcard-only information-schema query policy. The first
       `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` slice reuses
       that collation registry and exposes the collation-to-character-set
       mapping with the same wildcard-only policy. The first
       `INFORMATION_SCHEMA.KEYWORDS` slice exposes MyLite's lexer-supported
       keyword catalog with `WORD` and numeric `RESERVED` columns under the
       same wildcard-only policy. The first
       `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` slice exposes primary-key and
       unique constraints from `__mylite_index_catalog`, including the exact
       seven-column result shape, deterministic row ordering, supported key-DDL
       side effects, and the same wildcard-only policy. The first
       `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` slice exposes primary-key and
       unique key-part rows from `__mylite_index_catalog`, including composite
       key ordinals, supported key-DDL side effects, nonunique/expression-only
       exclusion, and the same wildcard-only policy. The first
       `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` slice exposes the exact
       four-column system-view shape and intentionally returns zero rows until
       CHECK DDL/catalog/enforcement support exists. The first
       `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` slice exposes the exact
       eleven-column system-view shape and intentionally returns zero rows
       until foreign-key DDL/catalog/enforcement support exists. Full
       build-dependent engine catalog breadth, full MySQL character-set
       catalog breadth, full MySQL collation catalog breadth, exact MySQL
       keyword catalog completeness, actual CHECK rows, actual foreign-key
       constraint rows, and general information-schema query
       processing remain deferred. Specs:
       [SHOW TABLES](../show-tables/specs.md),
       [SHOW TABLE STATUS](../show-table-status/specs.md),
       [SHOW COLUMNS](../show-columns/specs.md),
       [SHOW INDEX](../show-index/specs.md),
       [DESCRIBE / DESC table metadata](../describe-table/specs.md),
       [SHOW CREATE TABLE](../show-create-table/specs.md),
       [SHOW ENGINES](../show-engines/specs.md),
       [INFORMATION_SCHEMA.ENGINES](../information-schema-engines/specs.md),
       [INFORMATION_SCHEMA.CHARACTER_SETS](../information-schema-character-sets/specs.md),
       [INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY](../information-schema-collation-character-set-applicability/specs.md),
       [INFORMATION_SCHEMA.COLLATIONS](../information-schema-collations/specs.md),
       [INFORMATION_SCHEMA.CHECK_CONSTRAINTS](../information-schema-check-constraints/specs.md),
       [INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS](../information-schema-referential-constraints/specs.md),
       [INFORMATION_SCHEMA.TABLE_CONSTRAINTS](../information-schema-table-constraints/specs.md),
       [INFORMATION_SCHEMA.KEY_COLUMN_USAGE](../information-schema-key-column-usage/specs.md),
       [INFORMATION_SCHEMA.KEYWORDS](../information-schema-keywords/specs.md).
40. [ ] `SHOW VARIABLES`, `SHOW STATUS`, `SHOW WARNINGS`, `SHOW ERRORS`, and
       count variants: session state, diagnostics, filtering, and metadata.
       The diagnostic `SHOW WARNINGS` / `SHOW ERRORS` slice is specified for
       current MyLite warning/error state, row and count result shapes,
       supported `LIMIT` forms, and diagnostic lifecycle preservation. The
       first `SHOW VARIABLES` slice is specified for
       `SHOW [GLOBAL|SESSION|LOCAL] VARIABLES [LIKE 'pattern' | WHERE expr]`,
       including `Variable_name` / `Value` metadata, `LOCAL` as a session
       synonym, case-insensitive `LIKE` filtering with escapes, a practical
       catalog of charset/collation, diagnostics, autocommit, transaction,
       SQL-mode, and version variables, session/default scope behavior, and a
       clear unsupported diagnostic for parsed `WHERE` filters until shared
       SHOW filtering lands. The first `SHOW STATUS` slice is specified for
       `SHOW [GLOBAL|SESSION|LOCAL] STATUS [LIKE 'pattern' | WHERE expr]`,
       including `Variable_name` / `Value` metadata, `LOCAL` as a session
       synonym, case-insensitive `LIKE` filtering with escapes, embedded
       connection/thread/uptime rows, documented zero placeholders for common
       `Questions` and `Com_*` counters, same-value session/global behavior
       where MyLite has no separate mutable global state, and a clear
       unsupported diagnostic for parsed `WHERE` filters. The first
       `SHOW CHARACTER SET` / `SHOW CHARSET` slice is specified for
       `SHOW {CHARACTER SET|CHARSET}` plus the MySQL-runtime-accepted
       `SHOW CHAR SET` spelling, including exact `Charset` / `Description` /
       `Default collation` / `Maxlen` metadata, supported MyLite charset
       registry rows, case-insensitive `LIKE` filtering with escapes, numeric
       `Maxlen`, nondiagnostic clearing, and a clear unsupported diagnostic
       for parsed `WHERE` filters. The first `SHOW COLLATION` slice is
       specified for `SHOW COLLATION [LIKE 'pattern' | WHERE expr]`, including
       exact `Collation` / `Charset` / `Id` / `Default` / `Compiled` /
       `Sortlen` / `Pad_attribute` metadata, supported MyLite collation
       registry rows, MySQL-verified default flags, sort lengths, pad
       attributes, case-insensitive `LIKE` filtering with escapes, numeric
       `Id` and `Sortlen`, nondiagnostic clearing, and a clear unsupported
       diagnostic for parsed `WHERE` filters. Specs:
       [SHOW diagnostics](../show-diagnostics/specs.md),
       [SHOW VARIABLES](../show-variables/specs.md),
       [SHOW STATUS](../show-status/specs.md),
       [SHOW CHARACTER SET](../show-character-set/specs.md),
       [SHOW COLLATION](../show-collation/specs.md).
41. [ ] User and system variables: `SET`, user-variable storage, system-variable
       validation, scope, charset/collation metadata, and expression use.
42. [ ] Prepared statements: `PREPARE`, `EXECUTE`, `DEALLOCATE PREPARE`,
       parameter markers, metadata, and diagnostics.
43. [ ] `CREATE TABLE ... LIKE`: metadata cloning, indexes, defaults, generated
       columns, temporary tables, and atomicity.
44. [ ] `CREATE TABLE ... SELECT` and `INSERT ... SELECT`: type inference,
       metadata, defaults, locking, warnings, and atomicity.
45. [ ] `CREATE TEMPORARY TABLE`: session lifecycle, name shadowing, metadata,
       and cleanup.
46. [ ] Views: `CREATE VIEW`, `ALTER VIEW`, `DROP VIEW`, `SHOW CREATE VIEW`,
       algorithms, definers, security, column names, and check options.
47. [ ] Triggers: `CREATE TRIGGER`, `DROP TRIGGER`, `SHOW TRIGGERS`,
       `SHOW CREATE TRIGGER`, timing, ordering, body execution, and metadata.
48. [ ] Foreign keys: definition, validation, cascades, `RESTRICT`/`NO ACTION`,
       `SET NULL`, metadata, and enforcement.
49. [ ] JSON type and functions: validation, extraction, paths, comparison,
       mutation, generated-column interactions, and metadata.
50. [ ] Administrative compatibility placeholders: `LOCK TABLES`,
       `UNLOCK TABLES`, maintenance statements, account/privilege statements,
       replication-only statements, and embedded-compatible diagnostics.

## Execution Rule

Each task is executed as an end-to-end feature: independently authored spec,
MySQL 8.4.9 runtime verification, grammar support, AST/analyzer/runtime work,
tests, compatibility-matrix updates, review, and a focused commit.
