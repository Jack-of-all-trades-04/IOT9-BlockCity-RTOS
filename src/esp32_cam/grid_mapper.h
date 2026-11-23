#pragma once

#include "city_protocol.h"
#include <stdint.h>

// ============================================================================
// 8-Slot Overhead Baseplate ROI Grid Mapper & Frame Debouncer
// Left Half:  Land A (A1..A4)
// Right Half: Land B (B1..B4)
// ============================================================================

struct SlotRoi {
    float minX;
    float maxX;
    float minY;
    float maxY;
};

// Calibrated normalized ROI rectangles [0.0 .. 1.0] for two side-by-side 16x16 baseplates
static constexpr SlotRoi DEFAULT_SLOT_ROIS[NUM_SLOTS] = {
    // Land A (Left baseplate: X = 0.05..0.48)
    {0.05f, 0.26f, 0.08f, 0.48f}, // 0: A1 (Top-Left)
    {0.27f, 0.48f, 0.08f, 0.48f}, // 1: A2 (Top-Right)
    {0.05f, 0.26f, 0.52f, 0.92f}, // 2: A3 (Bottom-Left)
    {0.27f, 0.48f, 0.52f, 0.92f}, // 3: A4 (Bottom-Right)

    // Land B (Right baseplate: X = 0.52..0.95)
    {0.52f, 0.73f, 0.08f, 0.48f}, // 4: B1 (Top-Left)
    {0.74f, 0.95f, 0.08f, 0.48f}, // 5: B2 (Top-Right)
    {0.52f, 0.73f, 0.52f, 0.92f}, // 6: B3 (Bottom-Left)
    {0.74f, 0.95f, 0.52f, 0.92f}  // 7: B4 (Bottom-Right)
};

inline int mapCentroidToSlot(float cxNorm, float cyNorm, const SlotRoi* rois = DEFAULT_SLOT_ROIS) {
    if (cxNorm < 0.0f || cxNorm > 1.0f || cyNorm < 0.0f || cyNorm > 1.0f) return -1;
    for (int i = 0; i < NUM_SLOTS; ++i) {
        if (cxNorm >= rois[i].minX && cxNorm <= rois[i].maxX &&
            cyNorm >= rois[i].minY && cyNorm <= rois[i].maxY) {
            return i;
        }
    }
    return -1;
}

class SlotDebouncer {
public:
    explicit SlotDebouncer(uint8_t requiredFrames = 3)
        : required_(requiredFrames < 1 ? 1 : requiredFrames) {
        reset();
    }

    void reset() {
        for (int i = 0; i < NUM_SLOTS; ++i) {
            stable_[i] = BLDG_EMPTY;
            candidate_[i] = BLDG_EMPTY;
            streak_[i] = 0;
        }
    }

    // Returns true if the stable state of slotIdx changed on this frame
    bool update(int slotIdx, BuildingType observed) {
        if (slotIdx < 0 || slotIdx >= NUM_SLOTS) return false;
        if (observed == candidate_[slotIdx]) {
            if (streak_[slotIdx] < 255) streak_[slotIdx]++;
        } else {
            candidate_[slotIdx] = observed;
            streak_[slotIdx] = 1;
        }

        if (streak_[slotIdx] >= required_ && stable_[slotIdx] != candidate_[slotIdx]) {
            stable_[slotIdx] = candidate_[slotIdx];
            return true;
        }
        return false;
    }

    BuildingType stableType(int slotIdx) const {
        if (slotIdx < 0 || slotIdx >= NUM_SLOTS) return BLDG_EMPTY;
        return stable_[slotIdx];
    }

private:
    uint8_t required_;
    BuildingType stable_[NUM_SLOTS];
    BuildingType candidate_[NUM_SLOTS];
    uint8_t streak_[NUM_SLOTS];
};
