#ifndef MYLITE_FUZZ_MYLITE_FUZZER_H
#define MYLITE_FUZZ_MYLITE_FUZZER_H

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

#endif
