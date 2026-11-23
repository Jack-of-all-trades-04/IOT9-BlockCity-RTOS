#pragma once

#include "city_protocol.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ============================================================================
// Pure Portable C++17 Simulation Engine (shared by ESP32-1 and Host Unit Tests)
// ============================================================================

struct LandTelemetry {
    float tempC;
    float humPct;
    uint16_t mqRaw;
    float mqAqi;
    float ldrPct;
    bool valid;
};

struct GridSlot {
    BuildingType permittedType;
    BuildingType detectedType;
    SlotStatus status;
};

struct CityState {
    int32_t money;
    bool isBlockBUnlocked;
    bool isBlockBOnline;
    bool mayorOnSite;
    uint8_t sunPhase;        // 0..100: 0-20 Dawn, 21-55 Noon, 56-75 Dusk, 76-100 Night
    bool manualSunOverride;
    bool isNight;
    float cityAqi;
    float effectiveTempA;
    float effectiveTempB;
    int32_t lastTickIncome;
    uint32_t tickCount;
    GridSlot slots[NUM_SLOTS];
    LandTelemetry landA;
    LandTelemetry landB;
    char lastAlert[128];
};

inline int strCaseEq(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        char ca = (*a >= 'a' && *a <= 'z') ? (*a - 32) : *a;
        char cb = (*b >= 'a' && *b <= 'z') ? (*b - 32) : *b;
        if (ca != cb) return 0;
        ++a;
        ++b;
    }
    return (*a == '\0' && *b == '\0');
}

inline void setCityAlert(CityState& s, const char* msg) {
    if (!msg) return;
    snprintf(s.lastAlert, sizeof(s.lastAlert), "%s", msg);
}

inline void initCityState(CityState& s) {
    memset(&s, 0, sizeof(CityState));
    s.money = STARTING_MONEY;
    s.sunPhase = 35; // Start at bright Noon so Solar Farm works immediately
    s.landA = {27.0f, 60.0f, 350, 25.0f, 80.0f, false};
    s.landB = {27.0f, 60.0f, 350, 25.0f, 80.0f, false};
    s.effectiveTempA = 27.0f;
    s.effectiveTempB = 27.0f;
    s.cityAqi = 25.0f;
    setCityAlert(s, "BlockCity RTOS ready. Land A active, Land B locked.");
}

inline int slotNameToIndex(const char* name) {
    if (!name || strlen(name) != 2) return -1;
    char land = (name[0] >= 'a' && name[0] <= 'z') ? (name[0] - 32) : name[0];
    int num = name[1] - '1';
    if (num < 0 || num > 3) return -1;
    if (land == 'A') return num;
    if (land == 'B') return 4 + num;
    return -1;
}

inline const char* indexToSlotName(int idx) {
    static const char* NAMES[NUM_SLOTS] = {"A1", "A2", "A3", "A4", "B1", "B2", "B3", "B4"};
    if (idx < 0 || idx >= NUM_SLOTS) return "??";
    return NAMES[idx];
}

inline BuildingType buildingNameToType(const char* name) {
    if (!name) return BLDG_EMPTY;
    if (strCaseEq(name, "HOUSE") || strCaseEq(name, "RESIDENTIAL")) return BLDG_HOUSE;
    if (strCaseEq(name, "FACTORY") || strCaseEq(name, "INDUSTRIAL")) return BLDG_FACTORY;
    if (strCaseEq(name, "PARK") || strCaseEq(name, "GREEN_PARK")) return BLDG_PARK;
    if (strCaseEq(name, "SOLAR") || strCaseEq(name, "SOLAR_FARM")) return BLDG_SOLAR;
    return BLDG_EMPTY;
}

inline const char* buildingTypeToName(BuildingType t) {
    switch (t) {
        case BLDG_HOUSE:   return "HOUSE";
        case BLDG_FACTORY: return "FACTORY";
        case BLDG_PARK:    return "PARK";
        case BLDG_SOLAR:   return "SOLAR";
        default:           return "EMPTY";
    }
}

inline const char* slotStatusToName(SlotStatus st) {
    switch (st) {
        case SLOT_WAITING_PLACEMENT: return "WAIT_PLACE";
        case SLOT_ACTIVE:            return "ACTIVE";
        case SLOT_ILLEGAL:           return "ILLEGAL";
        case SLOT_WAITING_REMOVAL:   return "WAIT_REMOVE";
        default:                     return "EMPTY";
    }
}

inline int32_t getBuildingCost(BuildingType t) {
    switch (t) {
        case BLDG_HOUSE:   return COST_HOUSE;
        case BLDG_FACTORY: return COST_FACTORY;
        case BLDG_PARK:    return COST_PARK;
        case BLDG_SOLAR:   return COST_SOLAR;
        default:           return 0;
    }
}

inline int32_t getBuildingIncome(BuildingType t) {
    switch (t) {
        case BLDG_HOUSE:   return INCOME_HOUSE;
        case BLDG_FACTORY: return INCOME_FACTORY;
        case BLDG_PARK:    return INCOME_PARK;
        case BLDG_SOLAR:   return INCOME_SOLAR_MAX;
        default:           return 0;
    }
}

inline float getBuildingAqiDelta(BuildingType t) {
    switch (t) {
        case BLDG_HOUSE:   return AQI_DELTA_HOUSE;
        case BLDG_FACTORY: return AQI_DELTA_FACTORY;
        case BLDG_PARK:    return AQI_DELTA_PARK;
        case BLDG_SOLAR:   return AQI_DELTA_SOLAR;
        default:           return 0.0f;
    }
}

inline int requestBuildingPermit(CityState& s, BuildingType type, int preferredSlot) {
    if (type == BLDG_EMPTY) {
        setCityAlert(s, "Invalid building type requested.");
        return -1;
    }
    int32_t cost = getBuildingCost(type);
    if (s.money < cost) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Insufficient funds for %s ($%ld needed).",
                 buildingTypeToName(type), static_cast<long>(cost));
        setCityAlert(s, buf);
        return -1;
    }

    int targetSlot = preferredSlot;
    int maxAllowedSlots = s.isBlockBUnlocked ? NUM_SLOTS : LAND_A_SLOTS;

    if (targetSlot < 0) {
        for (int i = 0; i < maxAllowedSlots; ++i) {
            if (s.slots[i].status == SLOT_EMPTY) {
                targetSlot = i;
                break;
            }
        }
    }

    if (targetSlot < 0 || targetSlot >= NUM_SLOTS) {
        setCityAlert(s, "No empty grid slot available for permit.");
        return -1;
    }
    if (targetSlot >= LAND_A_SLOTS && !s.isBlockBUnlocked) {
        setCityAlert(s, "Land B is locked! Buy Land B ($1000) first.");
        return -1;
    }
    if (s.slots[targetSlot].status != SLOT_EMPTY) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Slot %s is occupied or pending (%s).",
                 indexToSlotName(targetSlot), slotStatusToName(s.slots[targetSlot].status));
        setCityAlert(s, buf);
        return -1;
    }

    s.money -= cost;
    s.slots[targetSlot].permittedType = type;
    s.slots[targetSlot].status = SLOT_WAITING_PLACEMENT;

    char buf[128];
    snprintf(buf, sizeof(buf), "Permit approved! Place %s LEGO on %s.",
             buildingTypeToName(type), indexToSlotName(targetSlot));
    setCityAlert(s, buf);
    return targetSlot;
}

inline bool processVisionDetection(CityState& s, int slotIdx, BuildingType seen) {
    if (slotIdx < 0 || slotIdx >= NUM_SLOTS) return false;
    if (slotIdx >= LAND_A_SLOTS && !s.isBlockBUnlocked) {
        // Mask out Land B detections until unlocked
        return false;
    }

    GridSlot& slot = s.slots[slotIdx];
    slot.detectedType = seen;

    if (seen == BLDG_EMPTY) {
        if (slot.status == SLOT_WAITING_REMOVAL || slot.status == SLOT_ILLEGAL) {
            slot.permittedType = BLDG_EMPTY;
            slot.status = SLOT_EMPTY;
            char buf[128];
            snprintf(buf, sizeof(buf), "Slot %s cleared.", indexToSlotName(slotIdx));
            setCityAlert(s, buf);
            return true;
        }
        if (slot.status == SLOT_ACTIVE) {
            // Physical brick removed without selling
            slot.permittedType = BLDG_EMPTY;
            slot.status = SLOT_EMPTY;
            char buf[128];
            snprintf(buf, sizeof(buf), "Building removed from %s.", indexToSlotName(slotIdx));
            setCityAlert(s, buf);
            return true;
        }
        return false;
    }

    // Non-empty LEGO brick detected
    if (slot.status == SLOT_WAITING_PLACEMENT && seen == slot.permittedType) {
        slot.status = SLOT_ACTIVE;
        char buf[128];
        snprintf(buf, sizeof(buf), "%s verified on %s and activated!",
                 buildingTypeToName(seen), indexToSlotName(slotIdx));
        setCityAlert(s, buf);
        return true;
    }

    if (slot.status == SLOT_ACTIVE && seen == slot.permittedType) {
        return false; // Unchanged steady-state
    }

    if (slot.status == SLOT_EMPTY ||
        (slot.status == SLOT_WAITING_PLACEMENT && seen != slot.permittedType)) {
        slot.status = SLOT_ILLEGAL;
        char buf[128];
        snprintf(buf, sizeof(buf), "ILLEGAL BUILD on %s (%s)! Remove or buy permit.",
                 indexToSlotName(slotIdx), buildingTypeToName(seen));
        setCityAlert(s, buf);
        return true;
    }

    return false;
}

inline bool sellBuildingSlot(CityState& s, int slotIdx) {
    if (slotIdx < 0 || slotIdx >= NUM_SLOTS) return false;
    GridSlot& slot = s.slots[slotIdx];
    if (slot.status != SLOT_ACTIVE && slot.status != SLOT_WAITING_PLACEMENT) {
        setCityAlert(s, "Nothing active to sell on that slot.");
        return false;
    }

    int32_t refund = getBuildingCost(slot.permittedType) / 2;
    s.money += refund;
    slot.permittedType = BLDG_EMPTY;
    slot.status = (slot.detectedType == BLDG_EMPTY) ? SLOT_EMPTY : SLOT_WAITING_REMOVAL;

    char buf[128];
    snprintf(buf, sizeof(buf), "Sold %s (+$%ld). Remove LEGO brick.",
             indexToSlotName(slotIdx), static_cast<long>(refund));
    setCityAlert(s, buf);
    return true;
}

inline bool purchaseLandB(CityState& s) {
    if (s.isBlockBUnlocked) {
        setCityAlert(s, "Land B is already unlocked.");
        return false;
    }
    if (s.money < COST_LAND_B) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Need $%ld to unlock Land B (have $%ld).",
                 static_cast<long>(COST_LAND_B), static_cast<long>(s.money));
        setCityAlert(s, buf);
        return false;
    }
    s.money -= COST_LAND_B;
    s.isBlockBUnlocked = true;
    setCityAlert(s, "Land B unlocked ($1000)! Sending UNLOCK_BLOCK_B over ESP-MESH.");
    return true;
}

inline void computeSunPwm(uint8_t sunPhase, uint8_t& whiteDuty, uint8_t& yellowDuty, bool& isNight) {
    uint8_t p = (sunPhase > 100) ? 100 : sunPhase;
    if (p <= 20) {
        // Dawn: warm yellow ramps up, white begins softly
        isNight = false;
        yellowDuty = static_cast<uint8_t>(60 + (p * 195) / 20);
        whiteDuty  = static_cast<uint8_t>((p * 120) / 20);
    } else if (p <= 55) {
        // Noon: bright white peak, yellow low
        isNight = false;
        whiteDuty  = 255;
        yellowDuty = 60;
    } else if (p <= 75) {
        // Dusk: white dims, warm yellow peaks
        isNight = false;
        uint8_t k = p - 55; // 0..20
        whiteDuty  = static_cast<uint8_t>(255 - (k * 255) / 20);
        yellowDuty = static_cast<uint8_t>(60 + (k * 195) / 20);
    } else {
        // Night: Sun off, warm yellow streetlamps stay softly illuminated
        isNight = true;
        whiteDuty  = 0;
        yellowDuty = 50;
    }
}

inline float computeHybridAQI(const CityState& s) {
    float baseMq = s.landA.mqAqi;
    if (s.isBlockBUnlocked && s.landB.valid) {
        baseMq = (s.landA.mqAqi + s.landB.mqAqi) * 0.5f;
    }
    float virtualDelta = 0.0f;
    for (int i = 0; i < NUM_SLOTS; ++i) {
        if (s.slots[i].status == SLOT_ACTIVE) {
            virtualDelta += getBuildingAqiDelta(s.slots[i].permittedType);
        }
    }
    float total = baseMq + virtualDelta;
    return (total < 0.0f) ? 0.0f : total;
}

inline float computeUrbanHeatTemp(float rawTemp, int activeNonParkBuildings, int activeParks) {
    int unbuffered = activeNonParkBuildings - activeParks;
    if (unbuffered < 0) unbuffered = 0;
    return rawTemp + static_cast<float>(unbuffered) * UHI_TEMP_PER_BLDG;
}

inline void runEconomyTick(CityState& s) {
    // 1. Update Hybrid AQI & Urban Heat Island
    s.cityAqi = computeHybridAQI(s);

    int nonParksA = 0, parksA = 0;
    int nonParksB = 0, parksB = 0;
    for (int i = 0; i < LAND_A_SLOTS; ++i) {
        if (s.slots[i].status == SLOT_ACTIVE) {
            if (s.slots[i].permittedType == BLDG_PARK) parksA++;
            else nonParksA++;
        }
    }
    for (int i = LAND_A_SLOTS; i < NUM_SLOTS; ++i) {
        if (s.slots[i].status == SLOT_ACTIVE) {
            if (s.slots[i].permittedType == BLDG_PARK) parksB++;
            else nonParksB++;
        }
    }
    s.effectiveTempA = computeUrbanHeatTemp(s.landA.tempC, nonParksA, parksA);
    s.effectiveTempB = computeUrbanHeatTemp(s.landB.tempC, nonParksB, parksB);

    // 2. Determine Night status & LDR daylight factor
    uint8_t dummyW = 0, dummyY = 0;
    bool phaseNight = false;
    computeSunPwm(s.sunPhase, dummyW, dummyY, phaseNight);

    float avgLdr = s.landA.ldrPct;
    if (s.isBlockBUnlocked && s.landB.valid) {
        avgLdr = (s.landA.ldrPct + s.landB.ldrPct) * 0.5f;
    }
    if (avgLdr < 0.0f) avgLdr = 0.0f;
    if (avgLdr > 100.0f) avgLdr = 100.0f;
    s.isNight = phaseNight || (avgLdr < 20.0f);

    // 3. Calculate per-tick economy
    bool smogPenalty = (s.cityAqi > SMOG_AQI_THRESHOLD);
    int32_t grossIncome = 0;
    int32_t upkeep = 0;

    for (int i = 0; i < NUM_SLOTS; ++i) {
        if (s.slots[i].status != SLOT_ACTIVE) continue;
        BuildingType bt = s.slots[i].permittedType;
        if (bt == BLDG_HOUSE) {
            int32_t inc = INCOME_HOUSE;
            if (smogPenalty) inc /= 2; // -50% when AQI > 150
            grossIncome += inc;
        } else if (bt == BLDG_FACTORY) {
            grossIncome += INCOME_FACTORY;
        } else if (bt == BLDG_PARK) {
            upkeep += (-INCOME_PARK);
        } else if (bt == BLDG_SOLAR) {
            if (!s.isNight) {
                int32_t solarInc = static_cast<int32_t>(roundf(INCOME_SOLAR_MAX * (avgLdr / 100.0f)));
                grossIncome += solarInc;
            }
        }
    }

    if (s.mayorOnSite && grossIncome > 0) {
        grossIncome = static_cast<int32_t>(roundf(grossIncome * 1.10f));
    }

    if (s.isNight) {
        upkeep += NIGHT_STREETLIGHT_COST;
    }

    int32_t net = grossIncome - upkeep;
    s.lastTickIncome = net;
    s.money += net;
    if (s.money < 0) s.money = 0;
    s.tickCount++;
}
