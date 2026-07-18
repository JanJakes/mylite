#include "runtime/mylite_spatial.h"

#include "mylite_fuzzer.h"

#include <string.h>

static const unsigned char valid_point[] = {
    0x00U, 0x00U, 0x00U, 0x00U, 0x01U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0xf0U, 0x3fU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x40U,
};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    unsigned char near_valid_point[sizeof(valid_point)];
    size_t overlay_size = size < sizeof(near_valid_point) ? size : sizeof(near_valid_point);

    (void)mylite_spatial_geometry_bytes_are_valid(data, size);
    (void)mylite_spatial_geometry_bytes_type(data, size);
    (void)mylite_spatial_geometry_bytes_srid(data, size);

    memcpy(near_valid_point, valid_point, sizeof(near_valid_point));
    if (overlay_size > 0U) {
        memcpy(near_valid_point, data, overlay_size);
    }
    (void)mylite_spatial_geometry_bytes_are_valid(near_valid_point, sizeof(near_valid_point));
    (void)mylite_spatial_geometry_bytes_type(near_valid_point, sizeof(near_valid_point));
    (void)mylite_spatial_geometry_bytes_srid(near_valid_point, sizeof(near_valid_point));
    return 0;
}
