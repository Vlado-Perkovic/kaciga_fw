#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <stdint.h> // For uint8_t etc.

// --- Emergency Types ---
typedef enum {
    EMERGENCY_TYPE_NONE = 0,
    EMERGENCY_TYPE_DANGER,
    EMERGENCY_TYPE_FALL
} emergency_type_t;

// Add any other shared type definitions here

#endif // COMMON_TYPES_H

