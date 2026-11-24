#include "secrets.h" // Must be included BEFORE BlynkSimpleEsp32.h

#include <Arduino.h>
#include <AlteriomPainlessMesh.h>
#include <ArduinoJson.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#if ENABLE_BLE_MAYOR_BOOST
#include <BLEAdvertisedDevice.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#endif

#include "city_logic.h"
#include "city_protocol.h"
#include "pin_config.h"

// ============================================================================
// BlockCity RTOS — ESP32-1 (Land A Master, Mesh Root, Blynk & Supabase GW)
// Syllabus Coverage: Modules 1-5 (FreeRTOS), 6 (BLE), 7 (WiFi/HTTP),
//                    8 (Power Mgmt Sync), 9 (Blynk/Node-RED), 10 (ESP-MESH)
// ============================================================================

struct VisionEvent {
    uint8_t slotIdx;
    uint8_t buildingType;
    float confidence;
};

struct MeshMessage {
    uint32_t fromNode;
    char payload[256];
};

struct SupabaseJob {
    bool isEvent; // false = city_telemetry, true = building_events
    char body[512];
};

// FreeRTOS Primitives (Modules 1-5)
static SemaphoreHandle_t cityMutex = nullptr;            // [MODUL 4] Mutex
static SemaphoreHandle_t economyTickSemaphore = nullptr; // [MODUL 5] Timer Semaphore
static QueueHandle_t visionEventQueue = nullptr;         // [MODUL 3] Queues
static QueueHandle_t meshRxQueue = nullptr;
static QueueHandle_t meshTxQueue = nullptr;
static QueueHandle_t supabaseQueue = nullptr;
static TimerHandle_t economyTimer = nullptr;             // [MODUL 5] Software Timers
static TimerHandle_t sunCycleTimer = nullptr;

// Core Objects
static Scheduler userScheduler;
static painlessMesh mesh;                                // [MODUL 10] ESP-MESH Root
static WebServer masterHttpServer(MASTER_HTTP_PORT);     // [MODUL 7] HTTP Ingestion
static DHT dhtLandA(PIN_DHT, DHT11);
static Preferences prefs;
static CityState cityState;

static volatile bool blynkReady = false;
static uint32_t lastSunSyncPhase = 255;

// ============================================================================
// NVS Flash Persistence (Preserves City State across resets)
// ============================================================================
static void saveStateToNvs(const CityState& s) {
    prefs.begin("blockcity", false);
    prefs.putInt("money", s.money);
    prefs.putBool("unlockedB", s.isBlockBUnlocked);
    prefs.putUChar("sunPhase", s.sunPhase);
    prefs.putUInt("ticks", s.tickCount);
    for (int i = 0; i < NUM_SLOTS; ++i) {
        char k1[12], k2[12];
        snprintf(k1, sizeof(k1), "sp%d", i);
        snprintf(k2, sizeof(k2), "ss%d", i);
        prefs.putUChar(k1, static_cast<uint8_t>(s.slots[i].permittedType));
        prefs.putUChar(k2, static_cast<uint8_t>(s.slots[i].status));
    }
    prefs.end();
}

static void loadStateFromNvs(CityState& s) {
    prefs.begin("blockcity", true);
    if (prefs.isKey("money")) {
        s.money = prefs.getInt("money", STARTING_MONEY);
        s.isBlockBUnlocked = prefs.getBool("unlockedB", false);
        s.sunPhase = prefs.getUChar("sunPhase", 35);
        s.tickCount = prefs.getUInt("ticks", 0);
        for (int i = 0; i < NUM_SLOTS; ++i) {
            char k1[12], k2[12];
            snprintf(k1, sizeof(k1), "sp%d", i);
            snprintf(k2, sizeof(k2), "ss%d", i);
            s.slots[i].permittedType = static_cast<BuildingType>(prefs.getUChar(k1, BLDG_EMPTY));
            s.slots[i].status = static_cast<SlotStatus>(prefs.getUChar(k2, SLOT_EMPTY));
        }
    }
    prefs.end();
}

static void enqueueMeshBroadcast(const char* jsonStr) {
    if (!meshTxQueue || !jsonStr) return;
    MeshMessage msg{};
    snprintf(msg.payload, sizeof(msg.payload), "%s", jsonStr);
    xQueueSend(meshTxQueue, &msg, 0);
}

static void enqueueSupabaseTelemetry(const CityState& s) {
    if (!supabaseQueue) return;
    if (strstr(SUPABASE_URL, "YOUR-PROJECT") != nullptr) return;

    int h = 0, f = 0, p = 0, sol = 0;
    for (int i = 0; i < NUM_SLOTS; ++i) {
        if (s.slots[i].status == SLOT_ACTIVE) {
            if (s.slots[i].permittedType == BLDG_HOUSE) h++;
            else if (s.slots[i].permittedType == BLDG_FACTORY) f++;
            else if (s.slots[i].permittedType == BLDG_PARK) p++;
            else if (s.slots[i].permittedType == BLDG_SOLAR) sol++;
        }
    }

    JsonDocument doc;
    doc["tick_count"] = s.tickCount;
    doc["money"] = s.money;
    doc["last_tick_net"] = s.lastTickIncome;
    doc["city_aqi"] = s.cityAqi;
    doc["sun_phase"] = s.sunPhase;
    doc["is_night"] = s.isNight;
    doc["block_b_unlocked"] = s.isBlockBUnlocked;
    doc["block_b_online"] = s.isBlockBOnline;
    doc["mayor_on_site"] = s.mayorOnSite;
    doc["land_a_temp"] = s.effectiveTempA;
    doc["land_a_hum"] = s.landA.humPct;
    doc["land_a_mq_raw"] = s.landA.mqRaw;
    doc["land_a_ldr_pct"] = s.landA.ldrPct;
    doc["land_b_temp"] = s.effectiveTempB;
    doc["land_b_hum"] = s.landB.humPct;
    doc["land_b_mq_raw"] = s.landB.mqRaw;
    doc["land_b_ldr_pct"] = s.landB.ldrPct;
    doc["active_houses"] = h;
    doc["active_factories"] = f;
    doc["active_parks"] = p;
    doc["active_solars"] = sol;

    SupabaseJob job{};
    job.isEvent = false;
    serializeJson(doc, job.body, sizeof(job.body));
    xQueueSend(supabaseQueue, &job, 0);
}

static void enqueueSupabaseEvent(const char* slotName, const char* bldg, const char* evtType,
                                 int32_t moneyAfter, const char* details) {
    if (!supabaseQueue) return;
    if (strstr(SUPABASE_URL, "YOUR-PROJECT") != nullptr) return;

    JsonDocument doc;
    doc["slot_name"] = slotName;
    doc["building_type"] = bldg;
    doc["event_type"] = evtType;
    doc["money_after"] = moneyAfter;
    doc["details"] = details ? details : "";

    SupabaseJob job{};
    job.isEvent = true;
    serializeJson(doc, job.body, sizeof(job.body));
    xQueueSend(supabaseQueue, &job, 0);
}

static void pushBlynkTelemetry(const CityState& s) {
    if (!blynkReady || !Blynk.connected()) return;

    JsonDocument doc;
    doc["money"] = s.money;
    doc["net"] = s.lastTickIncome;
    doc["aqi"] = s.cityAqi;
    doc["night"] = s.isNight;
    doc["b_unlocked"] = s.isBlockBUnlocked;
    doc["b_online"] = s.isBlockBOnline;
    doc["mayor"] = s.mayorOnSite;

    JsonObject slotsObj = doc["slots"].to<JsonObject>();
    for (int i = 0; i < NUM_SLOTS; ++i) {
        char val[24];
        snprintf(val, sizeof(val), "%s:%s",
                 buildingTypeToName(s.slots[i].permittedType),
                 slotStatusToName(s.slots[i].status));
        slotsObj[indexToSlotName(i)] = val;
    }

    String jsonOut;
    serializeJson(doc, jsonOut);

    Blynk.virtualWrite(VPIN_JSON_STATE, jsonOut);
    Blynk.virtualWrite(VPIN_MONEY, s.money);
    Blynk.virtualWrite(VPIN_CITY_AQI, s.cityAqi);
    Blynk.virtualWrite(VPIN_LAND_A_TEMP, s.effectiveTempA);
    Blynk.virtualWrite(VPIN_LAND_A_HUM, s.landA.humPct);
    Blynk.virtualWrite(VPIN_LAND_B_TEMP, s.effectiveTempB);
    Blynk.virtualWrite(VPIN_LAND_B_HUM, s.landB.humPct);
    Blynk.virtualWrite(VPIN_LAND_A_LDR, s.landA.ldrPct);
    Blynk.virtualWrite(VPIN_LAND_B_LDR, s.landB.ldrPct);
    Blynk.virtualWrite(VPIN_ALERTS, s.lastAlert);
}

// ============================================================================
// Command Parser (Shared by Blynk V0 and HTTP /api/simulate)
// ============================================================================
static void executeCityCommand(const String& rawCmd) {
    String cmd = rawCmd;
    cmd.trim();
    cmd.toUpperCase();
    if (cmd.isEmpty()) return;

    bool triggerUnlockMesh = false;
    bool triggerResetMesh = false;
    CityState snapshot{};

    if (xSemaphoreTake(cityMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        if (cmd == "BUY:LAND_B" || cmd == "UNLOCK_B") {
            if (purchaseLandB(cityState)) {
                triggerUnlockMesh = true;
                enqueueSupabaseEvent("B1-B4", "LAND_B", "LAND_B_UNLOCKED",
                                     cityState.money, cityState.lastAlert);
            }
        } else if (cmd == "RESET" || cmd == "RESET_CITY") {
            initCityState(cityState);
            triggerResetMesh = true;
            setCityAlert(cityState, "City reset to initial state ($500).");
        } else if (cmd.startsWith("BUY:")) {
            // Formats: BUY:HOUSE or BUY:HOUSE:A1
            int firstColon = cmd.indexOf(':');
            int secondColon = cmd.indexOf(':', firstColon + 1);
            String bldgStr = (secondColon > 0) ? cmd.substring(firstColon + 1, secondColon)
                                               : cmd.substring(firstColon + 1);
            int slotIdx = -1;
            if (secondColon > 0) {
                slotIdx = slotNameToIndex(cmd.substring(secondColon + 1).c_str());
            }
            BuildingType bt = buildingNameToType(bldgStr.c_str());
            int assigned = requestBuildingPermit(cityState, bt, slotIdx);
            if (assigned >= 0) {
                enqueueSupabaseEvent(indexToSlotName(assigned), buildingTypeToName(bt),
                                     "PERMIT_BOUGHT", cityState.money, cityState.lastAlert);
            }
        } else if (cmd.startsWith("SELL:")) {
            int slotIdx = slotNameToIndex(cmd.substring(5).c_str());
            if (sellBuildingSlot(cityState, slotIdx)) {
                enqueueSupabaseEvent(indexToSlotName(slotIdx), "EMPTY",
                                     "SOLD", cityState.money, cityState.lastAlert);
            }
        }
        saveStateToNvs(cityState);
        snapshot = cityState;
        xSemaphoreGive(cityMutex);
    }

    if (triggerUnlockMesh) {
        JsonDocument m;
        m["cmd"] = CMD_UNLOCK_BLOCK_B;
        char buf[128];
        serializeJson(m, buf, sizeof(buf));
        enqueueMeshBroadcast(buf);
        if (blynkReady && Blynk.connected()) {
            Blynk.logEvent("land_b_unlocked", "Land B unlocked! ESP32-2 Mesh active.");
        }
    }
    if (triggerResetMesh) {
        JsonDocument m;
        m["cmd"] = CMD_LOCK_BLOCK_B;
        char buf[128];
        serializeJson(m, buf, sizeof(buf));
        enqueueMeshBroadcast(buf);
    }
    pushBlynkTelemetry(snapshot);
}

// ============================================================================
// [MODUL 9] Blynk Virtual Pin Handlers
// ============================================================================
BLYNK_WRITE(VPIN_COMMAND) {
    executeCityCommand(param.asStr());
}

BLYNK_WRITE(VPIN_SUN_SLIDER) {
    int val = param.asInt();
    if (xSemaphoreTake(cityMutex, pdMS_TO_TICKS(150)) == pdTRUE) {
        if (val < 0) {
            cityState.manualSunOverride = false;
            setCityAlert(cityState, "Sun simulator returned to Auto Day/Night cycle.");
        } else {
            cityState.manualSunOverride = true;
            cityState.sunPhase = static_cast<uint8_t>(constrain(val, 0, 100));
        }
        xSemaphoreGive(cityMutex);
    }
}

BLYNK_WRITE(VPIN_BUY_LAND_B) {
    if (param.asInt() == 1) {
        executeCityCommand("BUY:LAND_B");
    }
}

// ============================================================================
// [MODUL 5] FreeRTOS Software Timer Callbacks (Non-Blocking)
// ============================================================================
static void onEconomyTimer(TimerHandle_t xTimer) {
    (void)xTimer;
    xSemaphoreGive(economyTickSemaphore);
}

static void onSunCycleTimer(TimerHandle_t xTimer) {
    (void)xTimer;
    if (xSemaphoreTake(cityMutex, 0) == pdTRUE) {
        if (!cityState.manualSunOverride) {
            cityState.sunPhase = (cityState.sunPhase + 1) % 101;
        }
        xSemaphoreGive(cityMutex);
    }
}

// ============================================================================
// [MODUL 1 & 2] Core 0 Tasks (Wi-Fi, Mesh, Blynk, HTTP Server, Supabase)
// ============================================================================
static void taskNetworkMesh(void* pv) {
    (void)pv;
    mesh.setDebugMsgTypes(ERROR | STARTUP);
    mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
    mesh.stationManual(WIFI_SSID, WIFI_PASSWORD);
    mesh.setRoot(true);
    mesh.setContainsRoot(true);

    mesh.onReceive([](uint32_t from, String& msg) {
        MeshMessage item{};
        item.fromNode = from;
        snprintf(item.payload, sizeof(item.payload), "%s", msg.c_str());
        xQueueSend(meshRxQueue, &item, 0);
    });

    mesh.onNewConnection([](uint32_t nodeId) {
        Serial.printf("[MESH] Node connected: %u\n", nodeId);
    });

    for (;;) {
        mesh.update();

        // Outbound Mesh TX Queue
        MeshMessage out{};
        while (xQueueReceive(meshTxQueue, &out, 0) == pdTRUE) {
            String s(out.payload);
            mesh.sendBroadcast(s);
        }

        // Non-blocking Blynk initialization once Wi-Fi STA connects
        if (WiFi.status() == WL_CONNECTED) {
            if (!blynkReady && strstr(BLYNK_AUTH_TOKEN, "YOUR_") == nullptr) {
                Blynk.config(BLYNK_AUTH_TOKEN);
                blynkReady = true;
                Serial.printf("[NET] Wi-Fi connected (%s), Blynk configured.\n",
                              WiFi.localIP().toString().c_str());
            }
            if (blynkReady) {
                Blynk.run();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void handleApiVision() {
    if (!masterHttpServer.hasArg("plain")) {
        masterHttpServer.send(400, "application/json", "{\"error\":\"Missing JSON body\"}");
        return;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, masterHttpServer.arg("plain"));
    if (err) {
        masterHttpServer.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    int slotIdx = slotNameToIndex(doc["slot"] | "");
    BuildingType bldg = buildingNameToType(doc["building"] | "");
    float conf = doc["confidence"] | 1.0f;

    if (slotIdx < 0) {
        masterHttpServer.send(400, "application/json", "{\"error\":\"Invalid slot\"}");
        return;
    }

    VisionEvent ev{static_cast<uint8_t>(slotIdx), static_cast<uint8_t>(bldg), conf};
    xQueueSend(visionEventQueue, &ev, pdMS_TO_TICKS(50));
    masterHttpServer.send(200, "application/json", "{\"ok\":true}");
}

static void handleApiState() {
    CityState snap{};
    if (xSemaphoreTake(cityMutex, pdMS_TO_TICKS(150)) == pdTRUE) {
        snap = cityState;
        xSemaphoreGive(cityMutex);
    }

    JsonDocument doc;
    doc["money"] = snap.money;
    doc["tick"] = snap.tickCount;
    doc["lastTickIncome"] = snap.lastTickIncome;
    doc["cityAqi"] = snap.cityAqi;
    doc["sunPhase"] = snap.sunPhase;
    doc["isNight"] = snap.isNight;
    doc["blockBUnlocked"] = snap.isBlockBUnlocked;
    doc["blockBOnline"] = snap.isBlockBOnline;
    doc["mayorOnSite"] = snap.mayorOnSite;
    doc["alert"] = snap.lastAlert;

    JsonArray arr = doc["slots"].to<JsonArray>();
    for (int i = 0; i < NUM_SLOTS; ++i) {
        JsonObject o = arr.add<JsonObject>();
        o["slot"] = indexToSlotName(i);
        o["permitted"] = buildingTypeToName(snap.slots[i].permittedType);
        o["detected"] = buildingTypeToName(snap.slots[i].detectedType);
        o["status"] = slotStatusToName(snap.slots[i].status);
    }

    String out;
    serializeJson(doc, out);
    masterHttpServer.send(200, "application/json", out);
}

static void handleApiSimulate() {
    if (masterHttpServer.hasArg("cmd")) {
        executeCityCommand(masterHttpServer.arg("cmd"));
        masterHttpServer.send(200, "application/json", "{\"ok\":true}");
        return;
    }
    masterHttpServer.send(400, "application/json", "{\"error\":\"Use ?cmd=BUY:HOUSE:A1\"}");
}

static void taskVisionHttp(void* pv) {
    (void)pv;
    masterHttpServer.on("/api/vision", HTTP_POST, handleApiVision);
    masterHttpServer.on("/api/state", HTTP_GET, handleApiState);
    masterHttpServer.on("/api/simulate", HTTP_POST, handleApiSimulate);
    masterHttpServer.begin();
    Serial.printf("[HTTP] Master API listening on port %d\n", MASTER_HTTP_PORT);

    for (;;) {
        masterHttpServer.handleClient();
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

static void taskSupabaseCloud(void* pv) {
    (void)pv;
    SupabaseJob job{};
    for (;;) {
        if (xQueueReceive(supabaseQueue, &job, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if (WiFi.status() == WL_CONNECTED && strstr(SUPABASE_URL, "YOUR-PROJECT") == nullptr) {
                WiFiClientSecure client;
                client.setInsecure();
                HTTPClient http;
                String endpoint = String(SUPABASE_URL) +
                                  (job.isEvent ? "/rest/v1/building_events" : "/rest/v1/city_telemetry");
                if (http.begin(client, endpoint)) {
                    http.setTimeout(3500);
                    http.addHeader("Content-Type", "application/json");
                    http.addHeader("apikey", SUPABASE_ANON_KEY);
                    // Only send Authorization: Bearer for legacy JWT keys (eyJ...).
                    // New Supabase keys (sb_publishable_... / sb_secret_...) must use apikey header only.
                    if (strncmp(SUPABASE_ANON_KEY, "eyJ", 3) == 0) {
                        http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
                    }
                    http.addHeader("Prefer", "return=minimal");
                    int code = http.POST(job.body);
                    Serial.printf("[SUPABASE] POST %s -> %d\n",
                                  job.isEvent ? "event" : "telemetry", code);
                    http.end();
                }
            }
        }
    }
}

// ============================================================================
// [MODUL 1 & 2] Core 1 Tasks (City Engine, Land A Sensors, Sun PWM, BLE Mayor)
// ============================================================================
static void taskCityEngine(void* pv) {
    (void)pv;
    VisionEvent vEv{};
    MeshMessage mRx{};

    for (;;) {
        bool stateChanged = false;
        bool illegalFlagged = false;
        bool unlockSyncNeeded = false;

        // 1. Process incoming ESP32-CAM vision detections [MODUL 3]
        while (xQueueReceive(visionEventQueue, &vEv, 0) == pdTRUE) {
            if (xSemaphoreTake(cityMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                bool changed = processVisionDetection(
                    cityState, vEv.slotIdx, static_cast<BuildingType>(vEv.buildingType));
                if (changed) {
                    stateChanged = true;
                    SlotStatus st = cityState.slots[vEv.slotIdx].status;
                    if (st == SLOT_ILLEGAL) {
                        illegalFlagged = true;
                        enqueueSupabaseEvent(indexToSlotName(vEv.slotIdx),
                                             buildingTypeToName(static_cast<BuildingType>(vEv.buildingType)),
                                             "ILLEGAL_BUILD", cityState.money, cityState.lastAlert);
                    } else if (st == SLOT_ACTIVE) {
                        enqueueSupabaseEvent(indexToSlotName(vEv.slotIdx),
                                             buildingTypeToName(cityState.slots[vEv.slotIdx].permittedType),
                                             "VERIFIED_ACTIVE", cityState.money, cityState.lastAlert);
                    }
                    saveStateToNvs(cityState);
                }
                xSemaphoreGive(cityMutex);
            }
        }

        // 2. Process incoming ESP-MESH packets from Land B [MODUL 10]
        while (xQueueReceive(meshRxQueue, &mRx, 0) == pdTRUE) {
            JsonDocument doc;
            if (deserializeJson(doc, mRx.payload) == DeserializationError::Ok) {
                const char* cmd = doc["cmd"] | "";
                if (strcmp(cmd, CMD_HELLO_EXPANSION) == 0) {
                    if (xSemaphoreTake(cityMutex, pdMS_TO_TICKS(150)) == pdTRUE) {
                        cityState.isBlockBOnline = true;
                        if (cityState.isBlockBUnlocked) {
                            unlockSyncNeeded = true;
                        }
                        xSemaphoreGive(cityMutex);
                    }
                } else if (strcmp(cmd, CMD_TELEMETRY_B) == 0) {
                    if (xSemaphoreTake(cityMutex, pdMS_TO_TICKS(150)) == pdTRUE) {
                        cityState.isBlockBOnline = true;
                        cityState.landB.tempC  = doc["temp"] | cityState.landB.tempC;
                        cityState.landB.humPct = doc["hum"] | cityState.landB.humPct;
                        cityState.landB.mqRaw  = doc["mqRaw"] | cityState.landB.mqRaw;
                        cityState.landB.mqAqi  = doc["mqAqi"] | cityState.landB.mqAqi;
                        cityState.landB.ldrPct = doc["ldrPct"] | cityState.landB.ldrPct;
                        cityState.landB.valid  = true;
                        xSemaphoreGive(cityMutex);
                    }
                }
            }
        }

        if (unlockSyncNeeded) {
            JsonDocument m;
            m["cmd"] = CMD_UNLOCK_BLOCK_B;
            char buf[96];
            serializeJson(m, buf, sizeof(buf));
            enqueueMeshBroadcast(buf);
        }

        // 3. Process 10-second Economy & Environment Tick [MODUL 5]
        if (xSemaphoreTake(economyTickSemaphore, 0) == pdTRUE) {
            CityState snap{};
            bool smogNow = false;

            if (xSemaphoreTake(cityMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                runEconomyTick(cityState);
                smogNow = (cityState.cityAqi > SMOG_AQI_THRESHOLD);
                saveStateToNvs(cityState);
                snap = cityState;
                xSemaphoreGive(cityMutex);
            }

            digitalWrite(PIN_FAN_ALERT, smogNow ? HIGH : LOW);
            digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED));
            pushBlynkTelemetry(snap);
            enqueueSupabaseTelemetry(snap);

            if (smogNow && blynkReady && Blynk.connected()) {
                Blynk.logEvent("smog_alert", "City AQI exceeded 150! Add Green Parks.");
            }
            Serial.printf("[TICK #%lu] Money=$%ld Net=%+ld AQI=%.1f Night=%d B_Unlocked=%d\n",
                          static_cast<unsigned long>(snap.tickCount),
                          static_cast<long>(snap.money),
                          static_cast<long>(snap.lastTickIncome),
                          snap.cityAqi, snap.isNight ? 1 : 0, snap.isBlockBUnlocked ? 1 : 0);
        } else if (stateChanged) {
            CityState snap{};
            if (xSemaphoreTake(cityMutex, pdMS_TO_TICKS(150)) == pdTRUE) {
                snap = cityState;
                xSemaphoreGive(cityMutex);
            }
            pushBlynkTelemetry(snap);
            if (illegalFlagged && blynkReady && Blynk.connected()) {
                Blynk.logEvent("illegal_build", snap.lastAlert);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static uint16_t readAdcAveraged(uint8_t pin) {
    uint32_t sum = 0;
    for (int i = 0; i < 16; ++i) {
        sum += analogRead(pin);
        delayMicroseconds(150);
    }
    return static_cast<uint16_t>(sum / 16);
}

static void taskSensorLandA(void* pv) {
    (void)pv;
    dhtLandA.begin();
    for (;;) {
        float t = dhtLandA.readTemperature();
        float h = dhtLandA.readHumidity();
        uint16_t mqRaw = readAdcAveraged(PIN_MQ_AO);
        uint16_t ldrRaw = readAdcAveraged(PIN_LDR_AO);

        float mqAqi = (mqRaw / 4095.0f) * 250.0f;
        float ldrPct = (ldrRaw / 4095.0f) * 100.0f;

        if (xSemaphoreTake(cityMutex, pdMS_TO_TICKS(150)) == pdTRUE) {
            if (!isnan(t)) cityState.landA.tempC = t;
            if (!isnan(h)) cityState.landA.humPct = h;
            cityState.landA.mqRaw = mqRaw;
            cityState.landA.mqAqi = mqAqi;
            cityState.landA.ldrPct = ldrPct;
            cityState.landA.valid = true;
            xSemaphoreGive(cityMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(SENSOR_POLL_MS));
    }
}

static void taskSunActuator(void* pv) {
    (void)pv;
    for (;;) {
        uint8_t phase = 35;
        float aqi = 25.0f;
        if (xSemaphoreTake(cityMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            phase = cityState.sunPhase;
            aqi = cityState.cityAqi;
            xSemaphoreGive(cityMutex);
        }

        uint8_t whiteDuty = 0, yellowDuty = 0;
        bool isNight = false;
        computeSunPwm(phase, whiteDuty, yellowDuty, isNight);

        ledcWrite(PIN_LED_WHITE, whiteDuty);
        ledcWrite(PIN_LED_YELLOW, yellowDuty);

        if (phase != lastSunSyncPhase) {
            lastSunSyncPhase = phase;
            JsonDocument m;
            m["cmd"] = CMD_SYNC_SUN;
            m["phase"] = phase;
            m["white"] = whiteDuty;
            m["yellow"] = yellowDuty;
            m["night"] = isNight;
            m["aqi"] = aqi;
            char buf[160];
            serializeJson(m, buf, sizeof(buf));
            enqueueMeshBroadcast(buf);
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

#if ENABLE_BLE_MAYOR_BOOST
// [MODUL 6] Lightweight BLE Proximity Scanner ("Mayor On-Site +10% Tax Boost")
static void taskBleMayor(void* pv) {
    (void)pv;
    BLEDevice::init("BlockCity-Master");
    BLEScan* pScan = BLEDevice::getScan();
    pScan->setActiveScan(false);
    pScan->setInterval(100);
    pScan->setWindow(50);

    for (;;) {
        BLEScanResults* results = pScan->start(2, false);
        bool mayorNear = false;
        if (results) {
            for (int i = 0; i < results->getCount(); ++i) {
                BLEAdvertisedDevice dev = results->getDevice(i);
                if (dev.haveName() && dev.getName() == MAYOR_BLE_NAME && dev.getRSSI() > -75) {
                    mayorNear = true;
                    break;
                }
            }
        }
        pScan->clearResults();

        if (xSemaphoreTake(cityMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            cityState.mayorOnSite = mayorNear;
            xSemaphoreGive(cityMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(18000));
    }
}
#endif

// ============================================================================
// Setup & Idle Loop
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== BlockCity RTOS: ESP32-1 (Land A Master) ===");

    pinMode(PIN_STATUS_LED, OUTPUT);
    pinMode(PIN_FAN_ALERT, OUTPUT);
    digitalWrite(PIN_STATUS_LED, HIGH);
    digitalWrite(PIN_FAN_ALERT, LOW);

    // Arduino Core 3.x LEDC PWM setup
    ledcAttach(PIN_LED_WHITE, PWM_FREQ_HZ, PWM_RES_BITS);
    ledcAttach(PIN_LED_YELLOW, PWM_FREQ_HZ, PWM_RES_BITS);

    initCityState(cityState);
    loadStateFromNvs(cityState);

    cityMutex = xSemaphoreCreateMutex();
    economyTickSemaphore = xSemaphoreCreateBinary();
    visionEventQueue = xQueueCreate(16, sizeof(VisionEvent));
    meshRxQueue = xQueueCreate(16, sizeof(MeshMessage));
    meshTxQueue = xQueueCreate(16, sizeof(MeshMessage));
    supabaseQueue = xQueueCreate(12, sizeof(SupabaseJob));

    economyTimer = xTimerCreate("EconTick", pdMS_TO_TICKS(ECONOMY_TICK_MS), pdTRUE, nullptr, onEconomyTimer);
    sunCycleTimer = xTimerCreate("SunCycle", pdMS_TO_TICKS(SUN_STEP_MS), pdTRUE, nullptr, onSunCycleTimer);
    xTimerStart(economyTimer, 0);
    xTimerStart(sunCycleTimer, 0);

    // [MODUL 1] Dual-Core SMP Task Pinning
    xTaskCreatePinnedToCore(taskNetworkMesh,   "NetMesh",   6144, nullptr, 3, nullptr, 0);
    xTaskCreatePinnedToCore(taskVisionHttp,    "HttpIngest",4096, nullptr, 3, nullptr, 0);
    xTaskCreatePinnedToCore(taskSupabaseCloud, "Supabase",  6144, nullptr, 1, nullptr, 0);

    xTaskCreatePinnedToCore(taskCityEngine,    "CityEngine",6144, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(taskSensorLandA,   "SensorA",   3072, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(taskSunActuator,   "SunPWM",    3072, nullptr, 1, nullptr, 1);

#if ENABLE_BLE_MAYOR_BOOST
    xTaskCreatePinnedToCore(taskBleMayor,      "BleMayor",  4096, nullptr, 1, nullptr, 1);
#endif
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
