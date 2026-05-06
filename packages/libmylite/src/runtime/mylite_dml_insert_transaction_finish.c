#include "mylite_dml_insert_transaction_finish.h"

#include "mylite_dml_insert_default.h"
#include "mylite_runtime.h"
#include "mylite_transactions.h"

int mylite_dml_finish_failed_insert_transaction(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const struct mylite_insert_table *table,
    const struct mylite_insert_execution_state *state,
    const struct mylite_statement_atomicity *atomicity,
    int original_status
) {
    uint64_t next_auto_increment = mylite_dml_insert_auto_increment_next_value(state);
    int status = MYLITE_OK;

    mylite_transaction_rollback_statement_atomicity(database, atomicity);
    if (table->has_auto_increment && next_auto_increment > table->next_auto_increment &&
        (state->accepted_row_count != 0U || state->advance_auto_increment_on_failure)) {
        status = mylite_transaction_update_table_auto_increment(
            database,
            schema_name,
            table_name,
            next_auto_increment
        );
        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (state->generated_insert_id) {
        database->last_insert_id = state->first_insert_id;
    }
    return original_status;
}

int mylite_dml_finish_successful_insert_transaction(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const struct mylite_insert_table *table,
    const struct mylite_insert_execution_state *state,
    struct mylite_statement_atomicity *atomicity,
    struct mylite_insert_transaction_result *result
) {
    int status = MYLITE_OK;

    if (table->has_auto_increment) {
        status = mylite_transaction_update_table_auto_increment(
            database,
            schema_name,
            table_name,
            mylite_dml_insert_auto_increment_next_value(state)
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_statement_atomicity(database, atomicity);
        if (status == MYLITE_OK) {
            result->affected_rows = (int64_t)state->accepted_row_count;
            if (state->generated_insert_id) {
                result->last_insert_id = state->first_insert_id;
                result->generated_insert_id = true;
            }
            return MYLITE_OK;
        }
    }

    mylite_transaction_rollback_statement_atomicity(database, atomicity);
    return status;
}

int mylite_dml_finish_successful_replace_transaction(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const struct mylite_insert_table *table,
    const struct mylite_insert_execution_state *state,
    struct mylite_statement_atomicity *atomicity,
    struct mylite_insert_transaction_result *result
) {
    int status = MYLITE_OK;

    if (table->has_auto_increment) {
        status = mylite_transaction_update_table_auto_increment(
            database,
            schema_name,
            table_name,
            mylite_dml_insert_auto_increment_next_value(state)
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_statement_atomicity(database, atomicity);
        if (status == MYLITE_OK) {
            result->affected_rows =
                (int64_t)state->accepted_row_count + (int64_t)state->duplicate_count;
            if (state->generated_insert_id) {
                result->last_insert_id = state->first_insert_id;
                result->generated_insert_id = true;
            }
            return MYLITE_OK;
        }
    }

    mylite_transaction_rollback_statement_atomicity(database, atomicity);
    return status;
}
