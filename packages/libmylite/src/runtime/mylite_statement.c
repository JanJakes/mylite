#include "mylite_statement.h"

int64_t mylite_affected_rows(const mylite_stmt *stmt)
{
    if (stmt == NULL) {
        return -1;
    }

    return stmt->affected_rows;
}

void mylite_statement_record_row_count(mylite_stmt *stmt)
{
    if (stmt == NULL || stmt->database == NULL || stmt->previous_row_count_recorded) {
        return;
    }

    stmt->database->previous_row_count = stmt->affected_rows;
    stmt->previous_row_count_recorded = true;
}
