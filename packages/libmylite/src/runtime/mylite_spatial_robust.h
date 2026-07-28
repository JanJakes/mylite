#ifndef MYLITE_SPATIAL_ROBUST_H
#define MYLITE_SPATIAL_ROBUST_H

struct mylite_spatial_robust_point {
    double coordinate_x;
    double coordinate_y;
};

int mylite_spatial_orientation_sign(
    const struct mylite_spatial_robust_point *origin,
    const struct mylite_spatial_robust_point *left,
    const struct mylite_spatial_robust_point *right
);

#endif
