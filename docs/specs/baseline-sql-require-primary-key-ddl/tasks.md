# Baseline SQL Require Primary Key DDL Enforcement Tasks

## Checklist

1. Research and design
   - Verify MySQL 8.4.9 assignment forms, rejected values, CREATE behavior,
     `IF NOT EXISTS`, `LIKE`, CTAS, temporary tables, supported single-action
     table changes on existing no-primary-key tables, `CREATE INDEX`,
     `DROP PRIMARY KEY`, and current multi-action final-state behavior.
   - Specify MyLite's session-local enforcement and fixed global limitation.

2. Runtime state and variable readback
   - Add handle-local `sql_require_primary_key` session state.
   - Support admitted session/local/unqualified `SET` forms.
   - Keep global reads fixed at `0` and global assignment limited to no-op
     disabled values.
   - Update scalar `SELECT` and `SHOW VARIABLES` readback.

3. DDL enforcement
   - Enforce primary-key presence for supported persistent and temporary
     `CREATE TABLE` paths after `IF NOT EXISTS` no-op handling.
   - Enforce cloned primary-key presence for supported `LIKE` paths.
   - Reject supported CTAS paths while enabled because they infer no primary
     key in the current subset.
   - Reject supported single-action table-changing ALTER/CREATE INDEX forms
     against existing no-primary-key tables unless the statement adds a primary
     key or is the observed `ALTER TABLE ... RENAME` exception.
   - Reject simple `DROP PRIMARY KEY` and quoted-primary `DROP CONSTRAINT`
     while enabled.
   - Reject supported multi-action ALTER final states that leave no primary
     key.

4. Tests and expectations
   - Add MySQL 8.4.9 expectation script.
   - Add focused C runtime tests for assignment, readback, successful and
     failing DDL, temporary tables, `LIKE`, CTAS, `IF NOT EXISTS`, simple,
     constraint, and multi-action primary-key drops, existing no-primary-key
     table ALTER/CREATE INDEX enforcement, independent handles, and file
     reopen.
   - Register or extend the relevant CTest entry.

5. Documentation
   - Update `COMPATIBILITY.md`.
   - Update detailed compatibility docs.
   - Keep wording partial and limited.

6. Verification
   - Run focused tests and the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for architecture boundaries, descriptor authority,
     diagnostics, scope control, and compatibility accuracy.
