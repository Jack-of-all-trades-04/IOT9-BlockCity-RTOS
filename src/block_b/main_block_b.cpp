#include <Arduino.h>
#include <AlteriomPainlessMesh.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Preferences.h>
#include <esp_wifi.h>

#include "city_protocol.h"
#include "pin_config.h"
#include "secrets.h"

// ============================================================================
// BlockCity RTOS — ESP32-2 (Land B Expansion Node)
// Syllabus Coverage: Modules 1-5 (FreeRTOS), 8 (Power Management), 10 (ESP-MESH)
// Per-Land Hardware: DHT11, MQ Gas (ADC1), LDR (ADC1), White+Yellow LEDs
// ============================================================================

static SemaphoreHandle_t stateMutex = nullptr;
static Scheduler userScheduler;
static painlessMesh mesh;
static DHT dhtLandB(PIN_DHT, DHT11);
static Preferences prefs;

static volatile bool isLandBUnlocked = false;
static uint8_t targetWhiteDuty = 0;
static uint8_t targetYellowDuty = 0;
static float latestCityAqi = 25.0f;
static float localMqAqi = 25.0f;

static void saveUnlockState(bool unlocked) {
    prefs.begin("blockcity_b", false);
    prefs.putBool("unlocked", unlocked);
    prefs.end();
}

static bool loadUnlockState() {
    prefs.begin("blockcity_b", true);
    bool val = prefs.getBool("unlocked", false);
    prefs.end();
    return val;
}

static uint16_t readAdcAveraged(uint8_t pin) {
    uint32_t sum = 0;
    for (int i = 0; i < 16; ++i) {
        sum += analogRead(pin);
        delayMicroseconds(150);
    }
    return static_cast<uint16_t>(sum / 16);
}

static void sendHelloExpansion() {
    JsonDocument doc;
    doc["cmd"] = CMD_HELLO_EXPANSION;
    doc["nodeId"] = mesh.getNodeId();
    doc["unlocked"] = static_cast<bool>(isLandBUnlocked);
    String out;
    serializeJson(doc, out);
    mesh.sendBroadcast(out);
}

static void applyPowerProfile(bool unlocked) {
    if (unlocked) {
        // Full performance when Land B is purchased & active
        esp_wifi_set_ps(WIFI_PS_NONE);
        digitalWrite(PIN_STATUS_LED, HIGH);
        Serial.println("[LAND_B] UNLOCKED: Full active sensing & mesh telemetry.");
    } else {
        // [MODUL 8] Power Management: Modem Power Save while Land B is locked
        esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
        ledcWrite(PIN_LED_WHITE, 0);
        ledcWrite(PIN_LED_YELLOW, 0);
        digitalWrite(PIN_FAN_ALERT, LOW);
        Serial.println("[LAND_B] LOCKED: Power-save standby mode active.");
    }
}

// ============================================================================
// Core 0 Task: ESP-MESH Leaf Node
// ============================================================================
static void taskMeshLeaf(void* pv) {
    (void)pv;
    mesh.setDebugMsgTypes(ERROR | STARTUP);
    mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
    mesh.setContainsRoot(true);

    mesh.onReceive([](uint32_t from, String& msg) {
        (void)from;
        JsonDocument doc;
        if (deserializeJson(doc, msg) != DeserializationError::Ok) return;
        const char* cmd = doc["cmd"] | "";

        if (strcmp(cmd, CMD_UNLOCK_BLOCK_B) == 0) {
            if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                isLandBUnlocked = true;
                saveUnlockState(true);
                xSemaphoreGive(stateMutex);
            }
            applyPowerProfile(true);
        } else if (strcmp(cmd, CMD_LOCK_BLOCK_B) == 0) {
            if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                isLandBUnlocked = false;
                saveUnlockState(false);
                xSemaphoreGive(stateMutex);
            }
            applyPowerProfile(false);
        } else if (strcmp(cmd, CMD_SYNC_SUN) == 0) {
            if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                targetWhiteDuty = doc["white"] | 0;
                targetYellowDuty = doc["yellow"] | 0;
                latestCityAqi = doc["aqi"] | latestCityAqi;
                xSemaphoreGive(stateMutex);
            }
        }
    });

    mesh.onNewConnection([](uint32_t nodeId) {
        Serial.printf("[MESH_B] Connected to mesh peer %u\n", nodeId);
        sendHelloExpansion();
    });

    uint32_t lastHelloMs = 0;
    for (;;) {
        mesh.update();
        if (!isLandBUnlocked && millis() - lastHelloMs > 15000) {
            lastHelloMs = millis();
            sendHelloExpansion();
        }
        if (digitalRead(PIN_WAKE_BTN) == LOW) {
            sendHelloExpansion();
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

// ============================================================================
// Core 1 Task: Land B Sensors (DHT11, MQ Gas, LDR) -> Mesh Telemetry
// ============================================================================
static void taskSensorLandB(void* pv) {
    (void)pv;
    dhtLandB.begin();

    for (;;) {
        if (isLandBUnlocked) {
            float t = dhtLandB.readTemperature();
            float h = dhtLandB.readHumidity();
            uint16_t mqRaw = readAdcAveraged(PIN_MQ_AO);
            uint16_t ldrRaw = readAdcAveraged(PIN_LDR_AO);

            float mqAqi = (mqRaw / 4095.0f) * 250.0f;
            float ldrPct = (ldrRaw / 4095.0f) * 100.0f;

            if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                localMqAqi = mqAqi;
                xSemaphoreGive(stateMutex);
            }

            JsonDocument doc;
            doc["cmd"] = CMD_TELEMETRY_B;
            doc["temp"] = isnan(t) ? 27.0f : t;
            doc["hum"] = isnan(h) ? 60.0f : h;
            doc["mqRaw"] = mqRaw;
            doc["mqAqi"] = mqAqi;
            doc["ldrPct"] = ldrPct;

            String out;
            serializeJson(doc, out);
            mesh.sendBroadcast(out);

            vTaskDelay(pdMS_TO_TICKS(MESH_TELEMETRY_MS));
        } else {
            // Standby poll interval while locked
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

// ============================================================================
// Core 1 Task: Land B White/Yellow LEDs & Smog Exhaust Fan
// ============================================================================
static void taskLedLandB(void* pv) {
    (void)pv;
    for (;;) {
        if (isLandBUnlocked) {
            uint8_t w = 0, y = 0;
            float cityAqi = 0.0f, mq = 0.0f;
            if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                w = targetWhiteDuty;
                y = targetYellowDuty;
                cityAqi = latestCityAqi;
                mq = localMqAqi;
                xSemaphoreGive(stateMutex);
            }
            ledcWrite(PIN_LED_WHITE, w);
            ledcWrite(PIN_LED_YELLOW, y);
            digitalWrite(PIN_STATUS_LED, HIGH);
            digitalWrite(PIN_FAN_ALERT, (cityAqi > SMOG_AQI_THRESHOLD || mq > SMOG_AQI_THRESHOLD) ? HIGH : LOW);
            vTaskDelay(pdMS_TO_TICKS(200));
        } else {
            // Slow standby heartbeat blink on Status LED while locked
            digitalWrite(PIN_STATUS_LED, HIGH);
            vTaskDelay(pdMS_TO_TICKS(100));
            digitalWrite(PIN_STATUS_LED, LOW);
            vTaskDelay(pdMS_TO_TICKS(1900));
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== BlockCity RTOS: ESP32-2 (Land B Expansion) ===");

    pinMode(PIN_STATUS_LED, OUTPUT);
    pinMode(PIN_FAN_ALERT, OUTPUT);
    pinMode(PIN_WAKE_BTN, INPUT_PULLUP);

    ledcAttach(PIN_LED_WHITE, PWM_FREQ_HZ, PWM_RES_BITS);
    ledcAttach(PIN_LED_YELLOW, PWM_FREQ_HZ, PWM_RES_BITS);

    stateMutex = xSemaphoreCreateMutex();
    isLandBUnlocked = loadUnlockState();
    applyPowerProfile(isLandBUnlocked);

    xTaskCreatePinnedToCore(taskMeshLeaf,    "MeshLeaf", 6144, nullptr, 3, nullptr, 0);
    xTaskCreatePinnedToCore(taskSensorLandB, "SensorB",  4096, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(taskLedLandB,    "LedLandB", 2048, nullptr, 1, nullptr, 1);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
