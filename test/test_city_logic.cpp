#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "city_logic.h"
#include "grid_mapper.h"

static int testsPassed = 0;

#define RUN_TEST(fn)                                                    \
    do {                                                                \
        fn();                                                           \
        testsPassed++;                                                  \
        printf("[PASS] %s\n", #fn);                                     \
    } while (0)

static void testInitialState() {
    CityState s;
    initCityState(s);
    assert(s.money == 500);
    assert(s.isBlockBUnlocked == false);
    assert(s.isBlockBOnline == false);
    for (int i = 0; i < NUM_SLOTS; ++i) {
        assert(s.slots[i].status == SLOT_EMPTY);
        assert(s.slots[i].permittedType == BLDG_EMPTY);
    }
}

static void testPermitAndPlaceWorkflow() {
    CityState s;
    initCityState(s);

    // Reject Land B while locked
    int failB = requestBuildingPermit(s, BLDG_HOUSE, slotNameToIndex("B1"));
    assert(failB == -1);
    assert(s.money == 500);

    // Approve House on A1 ($100)
    int idx = requestBuildingPermit(s, BLDG_HOUSE, slotNameToIndex("A1"));
    assert(idx == 0);
    assert(s.money == 400);
    assert(s.slots[0].status == SLOT_WAITING_PLACEMENT);

    // Vision sees HOUSE on A1 -> activates
    bool changed = processVisionDetection(s, 0, BLDG_HOUSE);
    assert(changed == true);
    assert(s.slots[0].status == SLOT_ACTIVE);

    // Approve Factory on A2 ($250)
    int idx2 = requestBuildingPermit(s, BLDG_FACTORY, slotNameToIndex("A2"));
    assert(idx2 == 1);
    assert(s.money == 150);
    assert(processVisionDetection(s, 1, BLDG_FACTORY) == true);
    assert(s.slots[1].status == SLOT_ACTIVE);
}

static void testIllegalConstructionAndSell() {
    CityState s;
    initCityState(s);

    // Drop a Factory on A3 without buying a permit
    bool changed = processVisionDetection(s, slotNameToIndex("A3"), BLDG_FACTORY);
    assert(changed == true);
    assert(s.slots[2].status == SLOT_ILLEGAL);

    // Illegal building must NOT generate income
    s.landA = {27.0f, 60.0f, 300, 20.0f, 90.0f, true};
    runEconomyTick(s);
    assert(s.lastTickIncome == 0);
    assert(s.money == 500);

    // Remove illegal brick -> returns to EMPTY
    assert(processVisionDetection(s, 2, BLDG_EMPTY) == true);
    assert(s.slots[2].status == SLOT_EMPTY);

    // Buy House on A1 ($100), verify, then sell for 50% ($50)
    assert(requestBuildingPermit(s, BLDG_HOUSE, 0) == 0);
    assert(s.money == 400);
    processVisionDetection(s, 0, BLDG_HOUSE);
    assert(sellBuildingSlot(s, 0) == true);
    assert(s.money == 450);
    assert(s.slots[0].status == SLOT_WAITING_REMOVAL);
    processVisionDetection(s, 0, BLDG_EMPTY);
    assert(s.slots[0].status == SLOT_EMPTY);
}

static void testEconomySmogSolarNightAndBleBoost() {
    CityState s;
    initCityState(s);
    s.sunPhase = 35; // Noon (daytime)
    s.landA = {28.0f, 55.0f, 350, 20.0f, 100.0f, true};

    // Place House on A1 ($100) and Solar on A4 ($300) -> $100 left
    assert(requestBuildingPermit(s, BLDG_HOUSE, 0) == 0);
    processVisionDetection(s, 0, BLDG_HOUSE);
    assert(requestBuildingPermit(s, BLDG_SOLAR, 3) == 3);
    processVisionDetection(s, 3, BLDG_SOLAR);
    assert(s.money == 100);

    // Tick at bright Noon: House (+20) + Solar (+30 * 100%) = +50
    runEconomyTick(s);
    assert(s.lastTickIncome == 50);
    assert(s.money == 150);
    // UHI: 2 active non-park buildings, 0 parks => +1.0C
    assert(fabsf(s.effectiveTempA - 29.0f) < 0.01f);

    // Enable Mayor BLE boost (+10%): 50 * 1.10 = 55
    s.mayorOnSite = true;
    runEconomyTick(s);
    assert(s.lastTickIncome == 55);

    // Switch to Night (phase 85, LDR 10%): Solar = 0, House = 20 * 1.1 = 22, Streetlights = -10 => net +12
    s.sunPhase = 85;
    s.landA.ldrPct = 10.0f;
    runEconomyTick(s);
    assert(s.isNight == true);
    assert(s.lastTickIncome == 12);

    // Trigger Smog (MQ AQI = 160 > 150) at Noon without Mayor boost
    s.mayorOnSite = false;
    s.sunPhase = 35;
    s.landA.ldrPct = 100.0f;
    s.landA.mqAqi = 160.0f;
    runEconomyTick(s);
    // House halved (20/2 = 10) + Solar (30) = +40
    assert(s.cityAqi > 150.0f);
    assert(s.lastTickIncome == 40);
}

static void testLandBExpansionAndHybridAqi() {
    CityState s;
    initCityState(s);
    s.money = 1200;
    s.landA = {26.0f, 60.0f, 300, 30.0f, 80.0f, true};
    s.landB = {29.0f, 58.0f, 400, 50.0f, 80.0f, true};

    // Masked B1 detection ignored before unlock
    assert(processVisionDetection(s, slotNameToIndex("B1"), BLDG_HOUSE) == false);
    assert(s.slots[4].status == SLOT_EMPTY);

    // Unlock Land B ($1000)
    assert(purchaseLandB(s) == true);
    assert(s.money == 200);
    assert(s.isBlockBUnlocked == true);

    // Now B1 accepts permit & detection
    assert(requestBuildingPermit(s, BLDG_HOUSE, slotNameToIndex("B1")) == 4);
    assert(processVisionDetection(s, 4, BLDG_HOUSE) == true);
    assert(s.slots[4].status == SLOT_ACTIVE);

    // Hybrid AQI averages Land A (30) + Land B (50) = 40 + 2 (1 House) = 42
    float aqi = computeHybridAQI(s);
    assert(fabsf(aqi - 42.0f) < 0.01f);
}

static void testSunPwmCrossfade() {
    uint8_t w = 0, y = 0;
    bool night = false;

    computeSunPwm(10, w, y, night); // Dawn
    assert(!night && y > w);

    computeSunPwm(35, w, y, night); // Noon
    assert(!night && w == 255 && y == 60);

    computeSunPwm(70, w, y, night); // Dusk
    assert(!night && y > w);

    computeSunPwm(90, w, y, night); // Night
    assert(night && w == 0 && y == 50);
}

static void testGridMapperAndDebouncer() {
    assert(mapCentroidToSlot(0.15f, 0.25f) == 0); // A1
    assert(mapCentroidToSlot(0.35f, 0.25f) == 1); // A2
    assert(mapCentroidToSlot(0.15f, 0.75f) == 2); // A3
    assert(mapCentroidToSlot(0.35f, 0.75f) == 3); // A4
    assert(mapCentroidToSlot(0.60f, 0.25f) == 4); // B1
    assert(mapCentroidToSlot(0.85f, 0.25f) == 5); // B2
    assert(mapCentroidToSlot(0.60f, 0.75f) == 6); // B3
    assert(mapCentroidToSlot(0.85f, 0.75f) == 7); // B4
    assert(mapCentroidToSlot(0.50f, 0.50f) == -1); // Seam gap

    SlotDebouncer db(3);
    assert(db.update(0, BLDG_FACTORY) == false); // frame 1
    assert(db.update(0, BLDG_FACTORY) == false); // frame 2
    assert(db.update(0, BLDG_FACTORY) == true);  // frame 3 -> stable!
    assert(db.stableType(0) == BLDG_FACTORY);
    assert(db.update(0, BLDG_FACTORY) == false); // steady-state
}

int main() {
    RUN_TEST(testInitialState);
    RUN_TEST(testPermitAndPlaceWorkflow);
    RUN_TEST(testIllegalConstructionAndSell);
    RUN_TEST(testEconomySmogSolarNightAndBleBoost);
    RUN_TEST(testLandBExpansionAndHybridAqi);
    RUN_TEST(testSunPwmCrossfade);
    RUN_TEST(testGridMapperAndDebouncer);

    printf("\nAll %d unit tests passed!\n", testsPassed);
    return 0;
}
