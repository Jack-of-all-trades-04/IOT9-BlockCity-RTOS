#pragma once

#include <stdint.h>

// ============================================================================
// BlockCity RTOS Shared Protocol, Economy Constants & Virtual Pins
// ============================================================================

enum BuildingType : uint8_t {
    BLDG_EMPTY = 0,
    BLDG_HOUSE = 1,
    BLDG_FACTORY = 2,
    BLDG_PARK = 3,
    BLDG_SOLAR = 4
};

enum SlotStatus : uint8_t {
    SLOT_EMPTY = 0,
    SLOT_WAITING_PLACEMENT = 1,
    SLOT_ACTIVE = 2,
    SLOT_ILLEGAL = 3,
    SLOT_WAITING_REMOVAL = 4
};

static constexpr int NUM_SLOTS = 8;
static constexpr int LAND_A_SLOTS = 4; // Indices 0..3 -> A1..A4
static constexpr int LAND_B_SLOTS = 4; // Indices 4..7 -> B1..B4

// Economy Ruleset (per 10-second simulation tick)
static constexpr int32_t STARTING_MONEY         = 500;
static constexpr int32_t COST_HOUSE             = 100;
static constexpr int32_t COST_FACTORY           = 250;
static constexpr int32_t COST_PARK              = 150;
static constexpr int32_t COST_SOLAR             = 300;
static constexpr int32_t COST_LAND_B            = 1000;

static constexpr int32_t INCOME_HOUSE           = 20;
static constexpr int32_t INCOME_FACTORY         = 65;
static constexpr int32_t INCOME_PARK            = -5;   // Municipal upkeep
static constexpr int32_t INCOME_SOLAR_MAX       = 30;   // Scaled by LDR daylight %
static constexpr int32_t NIGHT_STREETLIGHT_COST = 10;   // Deducted per tick at night

static constexpr float   AQI_DELTA_HOUSE        = 2.0f;
static constexpr float   AQI_DELTA_FACTORY      = 25.0f;
static constexpr float   AQI_DELTA_PARK         = -18.0f;
static constexpr float   AQI_DELTA_SOLAR        = 0.0f;
static constexpr float   SMOG_AQI_THRESHOLD     = 150.0f;
static constexpr float   UHI_TEMP_PER_BLDG      = 0.5f;

// Timing Constants (ms)
static constexpr uint32_t ECONOMY_TICK_MS       = 10000;
static constexpr uint32_t SUN_STEP_MS           = 2000;
static constexpr uint32_t SENSOR_POLL_MS        = 2500;
static constexpr uint32_t MESH_TELEMETRY_MS     = 3000;

// ESP-MESH Command Keys
#define CMD_HELLO_EXPANSION "HELLO_EXPANSION"
#define CMD_UNLOCK_BLOCK_B  "UNLOCK_BLOCK_B"
#define CMD_LOCK_BLOCK_B    "LOCK_BLOCK_B"
#define CMD_SYNC_SUN        "SYNC_SUN"
#define CMD_TELEMETRY_B     "TELEMETRY_B"
#define CMD_STATUS_ACK      "STATUS_ACK"

// Blynk Virtual Pin Mapping
#define VPIN_COMMAND        V0   // String input: BUY:HOUSE:A1, SELL:A2, BUY:LAND_B, RESET
#define VPIN_JSON_STATE     V1   // String output: Compact JSON state snapshot
#define VPIN_MONEY          V2   // Integer output: City Treasury ($)
#define VPIN_CITY_AQI       V3   // Float output: Hybrid City AQI
#define VPIN_LAND_A_TEMP    V4   // Float output: Land A Effective Temp (C)
#define VPIN_LAND_A_HUM     V5   // Float output: Land A Humidity (%)
#define VPIN_LAND_B_TEMP    V6   // Float output: Land B Effective Temp (C)
#define VPIN_LAND_B_HUM     V7   // Float output: Land B Humidity (%)
#define VPIN_LAND_A_LDR     V8   // Float output: Land A Sunlight (%)
#define VPIN_LAND_B_LDR     V9   // Float output: Land B Sunlight (%)
#define VPIN_SUN_SLIDER     V10  // Integer input (0..100, or -1 for auto cycle)
#define VPIN_ALERTS         V11  // String output: Status & Illegal Build / Smog alerts
#define VPIN_BUY_LAND_B     V12  // Button input: 1 = Trigger Buy Land B ($1000)
