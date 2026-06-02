static int rename_table_from_plan_with_policy(
    struct mylite_db *database,
    const struct planned_rename_table *plan,
    bool allow_same_object_noop,
    const char *unsupported_object_message
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    struct mylite_catalog_table_descriptor source = {0};
    int rc = MYLITE_OK;

    if (allow_same_object_noop) {
        rc = mylite_catalog_read_table_by_name(
            database,
            plan->source.schema.schema_id,
            plan->source.table_name,
            &source
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                plan->source.schema.name,
                plan->source.table_name
            );
            return MYLITE_ERROR;
        }
        if (source.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
            set_unsupported_error(database, unsupported_object_message);
            return MYLITE_ERROR;
        }
        if (source.schema_id == plan->target.schema.schema_id &&
            text_equals_ascii_case_insensitive(source.name, plan->target.table_name)) {
            return MYLITE_OK;
        }
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK) {
        rc = rename_table_pair_in_mutation(
            database,
            &mutation,
            plan,
            allow_same_object_noop,
            unsupported_object_message
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to rename table descriptor");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static int rename_table_pair_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_rename_table *plan,
    bool allow_same_object_noop,
    const char *unsupported_object_message
) {
    struct mylite_catalog_table_descriptor source = {0};
    struct mylite_catalog_table_descriptor target = {0};
    bool found = false;
    int rc = mylite_catalog_read_table_by_name(
        database,
        plan->source.schema.schema_id,
        plan->source.table_name,
        &source
    );

    if (rc != MYLITE_OK) {
        set_table_does_not_exist_error(database, plan->source.schema.name, plan->source.table_name);
        return MYLITE_ERROR;
    }
    if (source.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, unsupported_object_message);
        return MYLITE_ERROR;
    }
    if (allow_same_object_noop && source.schema_id == plan->target.schema.schema_id &&
        text_equals_ascii_case_insensitive(source.name, plan->target.table_name)) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_try_read_table_by_name(
        database,
        plan->target.schema.schema_id,
        plan->target.table_name,
        &target,
        &found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read target table descriptor");
        return rc;
    }
    if (found) {
        set_table_exists_error(database, plan->target.table_name);
        return MYLITE_ERROR;
    }
    rc = reject_rename_table_check_constraint_collisions(database, &source, plan);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_update_table_identity_in_mutation(
        database,
        mutation,
        source.table_id,
        plan->target.schema.schema_id,
        plan->target.table_name,
        NULL
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_rename_generated_check_constraints_in_mutation(
            database,
            mutation,
            source.table_id,
            plan->target.table_name
        );
    }

    return rc;
}

static int reject_rename_table_check_constraint_collisions(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *source,
    const struct planned_rename_table *plan
) {
    struct loaded_check_constraint_info *checks = NULL;
    size_t check_count = 0U;
    char check_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = mylite_execution_load_table_check_constraint_infos(database, source->table_id, &checks, &check_count);

    for (size_t index = 0U; rc == MYLITE_OK && index < check_count; ++index) {
        rc = build_renamed_check_constraint_name(
            database,
            plan,
            &checks[index].check_constraint,
            check_name,
            sizeof(check_name)
        );
        for (size_t previous = 0U; rc == MYLITE_OK && previous < index; ++previous) {
            char previous_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

            rc = build_renamed_check_constraint_name(
                database,
                plan,
                &checks[previous].check_constraint,
                previous_name,
                sizeof(previous_name)
            );
            if (rc == MYLITE_OK && text_equals_ascii_case_insensitive(check_name, previous_name)) {
                set_duplicate_check_constraint_error(database, check_name);
                rc = MYLITE_ERROR;
            }
        }
        if (rc == MYLITE_OK) {
            rc = reject_rename_table_check_constraint_schema_collision(
                database,
                source,
                plan,
                check_name
            );
        }
    }

    mylite_execution_loaded_check_constraint_infos_deinit(&checks, &check_count);
    return rc;
}

static int build_renamed_check_constraint_name(
    struct mylite_db *database,
    const struct planned_rename_table *plan,
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    char *destination,
    size_t destination_size
) {
    int written = 0;

    if (!check_constraint->name_is_generated) {
        written = snprintf(destination, destination_size, "%s", check_constraint->name);
    } else {
        written = snprintf(
            destination,
            destination_size,
            "%s_chk_%" PRId64,
            plan->target.table_name,
            check_constraint->generated_ordinal
        );
    }
    if (written < 0 || (size_t)written >= destination_size) {
        set_runtime_error(database, "renamed CHECK constraint name is too long");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int reject_rename_table_check_constraint_schema_collision(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *source,
    const struct planned_rename_table *plan,
    const char *check_constraint_name
) {
    struct check_constraint_name_collision_context context = {
        .name = check_constraint_name,
        .excluded_table_id = source->table_id,
        .found = false,
    };
    int rc = mylite_catalog_for_each_check_constraint_in_schema(
        database,
        plan->target.schema.schema_id,
        reject_rename_table_check_constraint_schema_collision_callback,
        &context
    );

    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read CHECK constraint descriptors");
        return rc;
    }
    if (context.found) {
        set_duplicate_check_constraint_error(database, check_constraint_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int reject_rename_table_check_constraint_schema_collision_callback(
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    void *user_data
) {
    struct check_constraint_name_collision_context *context = user_data;

    if (check_constraint == NULL || context == NULL || context->name == NULL) {
        return MYLITE_MISUSE;
    }
    if (check_constraint->table_id != context->excluded_table_id &&
        text_equals_ascii_case_insensitive(check_constraint->name, context->name)) {
        context->found = true;
    }

    return MYLITE_OK;
}
