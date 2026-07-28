#include "mylite_spatial_robust.h"

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    exact_coordinate_limb_count = 66,
    exact_product_limb_count = exact_coordinate_limb_count * 2,
    exact_limb_bit_count = 32,
    binary64_fraction_bit_count = 52,
    binary64_exponent_shift = 52,
    binary64_sign_shift = 63,
    binary64_exponent_mask = 0x7ff,
    binary64_max_exponent = 1024,
};

_Static_assert(sizeof(double) == sizeof(uint64_t), "robust spatial predicates require binary64");
_Static_assert(
    DBL_MANT_DIG == binary64_fraction_bit_count + 1,
    "robust spatial predicates require binary64"
);
_Static_assert(DBL_MAX_EXP == binary64_max_exponent, "robust spatial predicates require binary64");

static const uint64_t binary64_fraction_mask = UINT64_C(0x000fffffffffffff);
// This exceeds the accumulated rounding bound of the fast determinant evaluation.
static const double orientation_filter_multiplier = 16.0;

struct exact_coordinate {
    uint32_t limbs[exact_coordinate_limb_count];
    bool negative;
};

struct exact_product {
    uint32_t limbs[exact_product_limb_count];
    bool negative;
};

static bool orientation_fast_sign(
    const struct mylite_spatial_robust_point *origin,
    const struct mylite_spatial_robust_point *left,
    const struct mylite_spatial_robust_point *right,
    int *out_sign
);
static int orientation_exact_sign(
    const struct mylite_spatial_robust_point *origin,
    const struct mylite_spatial_robust_point *left,
    const struct mylite_spatial_robust_point *right
);
static struct exact_coordinate exact_coordinate_difference(double minuend, double subtrahend);
static struct exact_coordinate exact_coordinate_from_double(double value);
static void exact_coordinate_insert_significand(
    struct exact_coordinate *coordinate,
    uint64_t significand, // NOLINT(bugprone-easily-swappable-parameters)
    uint32_t shift
);
static struct exact_coordinate exact_coordinate_add(
    const struct exact_coordinate *left,
    const struct exact_coordinate *right
);
static int exact_coordinate_magnitude_compare(
    const struct exact_coordinate *left,
    const struct exact_coordinate *right
);
static void exact_coordinate_magnitude_add(
    const struct exact_coordinate *left,
    const struct exact_coordinate *right,
    struct exact_coordinate *out_sum
);
static void exact_coordinate_magnitude_subtract(
    const struct exact_coordinate *larger,
    const struct exact_coordinate *smaller,
    struct exact_coordinate *out_difference
);
static bool exact_coordinate_is_zero(const struct exact_coordinate *coordinate);
static struct exact_product exact_product_multiply(
    const struct exact_coordinate *left,
    const struct exact_coordinate *right
);
static int exact_determinant_sign(
    const struct exact_product *positive,
    const struct exact_product *negative
);
static int exact_product_magnitude_compare(
    const struct exact_product *left,
    const struct exact_product *right
);
static bool exact_product_is_zero(const struct exact_product *product);

int mylite_spatial_orientation_sign(
    const struct mylite_spatial_robust_point *origin,
    const struct mylite_spatial_robust_point *left,
    const struct mylite_spatial_robust_point *right
) {
    int sign = 0;

    if (origin == NULL || left == NULL || right == NULL || !isfinite(origin->coordinate_x) ||
        !isfinite(origin->coordinate_y) || !isfinite(left->coordinate_x) ||
        !isfinite(left->coordinate_y) || !isfinite(right->coordinate_x) ||
        !isfinite(right->coordinate_y)) {
        return 0;
    }
    if (orientation_fast_sign(origin, left, right, &sign)) {
        return sign;
    }
    return orientation_exact_sign(origin, left, right);
}

static bool orientation_fast_sign(
    const struct mylite_spatial_robust_point *origin,
    const struct mylite_spatial_robust_point *left,
    const struct mylite_spatial_robust_point *right,
    int *out_sign
) {
    double left_x = left->coordinate_x - origin->coordinate_x;
    double left_y = left->coordinate_y - origin->coordinate_y;
    double right_x = right->coordinate_x - origin->coordinate_x;
    double right_y = right->coordinate_y - origin->coordinate_y;
    double positive = left_x * right_y;
    double negative = left_y * right_x;
    double determinant = positive - negative;
    double magnitude_sum = fabs(positive) + fabs(negative);
    double error_bound = orientation_filter_multiplier * DBL_EPSILON * magnitude_sum;
    bool positive_is_exact_zero = left_x == 0.0 || right_y == 0.0;
    bool negative_is_exact_zero = left_y == 0.0 || right_x == 0.0;

    if (out_sign == NULL) {
        return false;
    }
    if (positive_is_exact_zero && negative_is_exact_zero) {
        *out_sign = 0;
        return true;
    }
    if (!isfinite(left_x) || !isfinite(left_y) || !isfinite(right_x) || !isfinite(right_y) ||
        !isfinite(positive) || !isfinite(negative) || !isfinite(determinant) ||
        !isfinite(magnitude_sum) || (positive == 0.0 && left_x != 0.0 && right_y != 0.0) ||
        (negative == 0.0 && left_y != 0.0 && right_x != 0.0) ||
        fpclassify(positive) == FP_SUBNORMAL || fpclassify(negative) == FP_SUBNORMAL ||
        fabs(determinant) <= error_bound) {
        return false;
    }
    *out_sign = determinant < 0.0 ? -1 : 1;
    return true;
}

static int orientation_exact_sign(
    const struct mylite_spatial_robust_point *origin,
    const struct mylite_spatial_robust_point *left,
    const struct mylite_spatial_robust_point *right
) {
    struct exact_coordinate left_x =
        exact_coordinate_difference(left->coordinate_x, origin->coordinate_x);
    struct exact_coordinate left_y =
        exact_coordinate_difference(left->coordinate_y, origin->coordinate_y);
    struct exact_coordinate right_x =
        exact_coordinate_difference(right->coordinate_x, origin->coordinate_x);
    struct exact_coordinate right_y =
        exact_coordinate_difference(right->coordinate_y, origin->coordinate_y);
    struct exact_product positive = exact_product_multiply(&left_x, &right_y);
    struct exact_product negative = exact_product_multiply(&left_y, &right_x);

    return exact_determinant_sign(&positive, &negative);
}

static struct exact_coordinate exact_coordinate_difference(double minuend, double subtrahend) {
    struct exact_coordinate left = exact_coordinate_from_double(minuend);
    struct exact_coordinate right = exact_coordinate_from_double(subtrahend);

    if (!exact_coordinate_is_zero(&right)) {
        right.negative = !right.negative;
    }
    return exact_coordinate_add(&left, &right);
}

static struct exact_coordinate exact_coordinate_from_double(double value) {
    struct exact_coordinate coordinate = {0};
    uint64_t bits = 0U;
    uint64_t significand = 0U;
    uint32_t exponent = 0U;
    uint32_t shift = 0U;

    memcpy(&bits, &value, sizeof(bits));
    exponent = (uint32_t)((bits >> binary64_exponent_shift) & binary64_exponent_mask);
    significand = bits & binary64_fraction_mask;
    if (exponent != 0U) {
        significand |= UINT64_C(1) << binary64_fraction_bit_count;
        shift = exponent - 1U;
    }
    coordinate.negative = ((bits >> binary64_sign_shift) & UINT64_C(1)) != 0U;
    exact_coordinate_insert_significand(&coordinate, significand, shift);
    if (exact_coordinate_is_zero(&coordinate)) {
        coordinate.negative = false;
    }
    return coordinate;
}

static void exact_coordinate_insert_significand(
    struct exact_coordinate *coordinate,
    uint64_t significand, // NOLINT(bugprone-easily-swappable-parameters)
    uint32_t shift
) {
    size_t limb_index = (size_t)(shift / exact_limb_bit_count);
    uint32_t bit_offset = shift % exact_limb_bit_count;
    uint32_t low = (uint32_t)significand;
    uint32_t high = (uint32_t)(significand >> exact_limb_bit_count);

    if (coordinate == NULL || significand == 0U || limb_index >= exact_coordinate_limb_count) {
        return;
    }
    coordinate->limbs[limb_index] |= low << bit_offset;
    if (bit_offset == 0U) {
        if (limb_index + 1U < exact_coordinate_limb_count) {
            coordinate->limbs[limb_index + 1U] |= high;
        }
        return;
    }
    if (limb_index + 1U < exact_coordinate_limb_count) {
        coordinate->limbs[limb_index + 1U] |=
            (low >> (exact_limb_bit_count - bit_offset)) | (high << bit_offset);
    }
    if (limb_index + 2U < exact_coordinate_limb_count) {
        coordinate->limbs[limb_index + 2U] |= high >> (exact_limb_bit_count - bit_offset);
    }
}

static struct exact_coordinate exact_coordinate_add(
    const struct exact_coordinate *left,
    const struct exact_coordinate *right
) {
    struct exact_coordinate sum = {0};

    if (left->negative == right->negative) {
        exact_coordinate_magnitude_add(left, right, &sum);
        sum.negative = left->negative;
    } else {
        int comparison = exact_coordinate_magnitude_compare(left, right);

        if (comparison > 0) {
            exact_coordinate_magnitude_subtract(left, right, &sum);
            sum.negative = left->negative;
        } else if (comparison < 0) {
            exact_coordinate_magnitude_subtract(right, left, &sum);
            sum.negative = right->negative;
        }
    }
    if (exact_coordinate_is_zero(&sum)) {
        sum.negative = false;
    }
    return sum;
}

static int exact_coordinate_magnitude_compare(
    const struct exact_coordinate *left,
    const struct exact_coordinate *right
) {
    for (size_t index = exact_coordinate_limb_count; index > 0U; --index) {
        size_t limb_index = index - 1U;

        if (left->limbs[limb_index] < right->limbs[limb_index]) {
            return -1;
        }
        if (left->limbs[limb_index] > right->limbs[limb_index]) {
            return 1;
        }
    }
    return 0;
}

static void exact_coordinate_magnitude_add(
    const struct exact_coordinate *left,
    const struct exact_coordinate *right,
    struct exact_coordinate *out_sum
) {
    uint64_t carry = 0U;

    for (size_t index = 0U; index < exact_coordinate_limb_count; ++index) {
        uint64_t value = (uint64_t)left->limbs[index] + right->limbs[index] + carry;

        out_sum->limbs[index] = (uint32_t)value;
        carry = value >> exact_limb_bit_count;
    }
}

static void exact_coordinate_magnitude_subtract(
    const struct exact_coordinate *larger,
    const struct exact_coordinate *smaller,
    struct exact_coordinate *out_difference
) {
    uint64_t borrow = 0U;

    for (size_t index = 0U; index < exact_coordinate_limb_count; ++index) {
        uint64_t minuend = larger->limbs[index];
        uint64_t subtrahend = (uint64_t)smaller->limbs[index] + borrow;

        out_difference->limbs[index] = (uint32_t)(minuend - subtrahend);
        borrow = minuend < subtrahend ? 1U : 0U;
    }
}

static bool exact_coordinate_is_zero(const struct exact_coordinate *coordinate) {
    for (size_t index = 0U; index < exact_coordinate_limb_count; ++index) {
        if (coordinate->limbs[index] != 0U) {
            return false;
        }
    }
    return true;
}

static struct exact_product exact_product_multiply(
    const struct exact_coordinate *left,
    const struct exact_coordinate *right
) {
    struct exact_product product = {0};

    if (exact_coordinate_is_zero(left) || exact_coordinate_is_zero(right)) {
        return product;
    }
    for (size_t left_index = 0U; left_index < exact_coordinate_limb_count; ++left_index) {
        uint64_t carry = 0U;

        for (size_t right_index = 0U; right_index < exact_coordinate_limb_count; ++right_index) {
            size_t product_index = left_index + right_index;
            uint64_t value = ((uint64_t)left->limbs[left_index] * right->limbs[right_index]) +
                             product.limbs[product_index] + carry;

            product.limbs[product_index] = (uint32_t)value;
            carry = value >> exact_limb_bit_count;
        }
        product.limbs[left_index + exact_coordinate_limb_count] = (uint32_t)carry;
    }
    product.negative = left->negative != right->negative;
    return product;
}

static int exact_determinant_sign(
    const struct exact_product *positive,
    const struct exact_product *negative
) {
    bool positive_zero = exact_product_is_zero(positive);
    bool negative_zero = exact_product_is_zero(negative);
    bool positive_term_negative = positive->negative && !positive_zero;
    bool negative_term_negative = !negative->negative && !negative_zero;

    if (positive_zero && negative_zero) {
        return 0;
    }
    if (positive_zero) {
        return negative_term_negative ? -1 : 1;
    }
    if (negative_zero) {
        return positive_term_negative ? -1 : 1;
    }
    if (positive_term_negative == negative_term_negative) {
        return positive_term_negative ? -1 : 1;
    }
    {
        int comparison = exact_product_magnitude_compare(positive, negative);

        if (comparison == 0) {
            return 0;
        }
        if (comparison > 0) {
            return positive_term_negative ? -1 : 1;
        }
        return negative_term_negative ? -1 : 1;
    }
}

static int exact_product_magnitude_compare(
    const struct exact_product *left,
    const struct exact_product *right
) {
    for (size_t index = exact_product_limb_count; index > 0U; --index) {
        size_t limb_index = index - 1U;

        if (left->limbs[limb_index] < right->limbs[limb_index]) {
            return -1;
        }
        if (left->limbs[limb_index] > right->limbs[limb_index]) {
            return 1;
        }
    }
    return 0;
}

static bool exact_product_is_zero(const struct exact_product *product) {
    for (size_t index = 0U; index < exact_product_limb_count; ++index) {
        if (product->limbs[index] != 0U) {
            return false;
        }
    }
    return true;
}
