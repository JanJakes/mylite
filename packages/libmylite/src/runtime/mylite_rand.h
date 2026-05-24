#ifndef MYLITE_RUNTIME_MYLITE_RAND_H
#define MYLITE_RUNTIME_MYLITE_RAND_H

#include "sqlite3.h"

#include <stdint.h>

struct mylite_rand_state {
    uint32_t seed1;
    uint32_t seed2;
};

int mylite_sqlite_register_rand_functions(sqlite3 *sqlite);

double mylite_rand_unseeded_unit_double(void);
double mylite_rand_seeded_unit_double(uint32_t seed);
void mylite_rand_state_init(struct mylite_rand_state *state, uint32_t seed);
double mylite_rand_state_next_unit_double(struct mylite_rand_state *state);

#endif
