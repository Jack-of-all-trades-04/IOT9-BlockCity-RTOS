#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include "esp_camera.h"
#include "img_converters.h"

#include "city_logic.h"
#include "city_protocol.h"
#include "fomo_config.h"
#include "grid_mapper.h"
#include "pin_config.h"
#include "secrets.h"

#if USE_EDGE_IMPULSE_FOMO
// After exporting your Edge Impulse Arduino Library into lib/, include its header here:
#include <BlockCity_FOMO_inferencing.h>
#endif

// ============================================================================
// BlockCity RTOS — ESP32-CAM Overhead Edge Vision & Dataset Capture Node
// ============================================================================

static WebServer camServer(80);
static SemaphoreHandle_t camMutex = nullptr;
static SlotDebouncer debouncer(VISION_DEBOUNCE_FRAMES);
static uint8_t softFillDuty = CAM_SOFT_FILL_DUTY;
static bool cameraReady = false;

#if USE_EDGE_IMPULSE_FOMO
static uint8_t* eiRgbBuf = nullptr;
static size_t eiRgbBufLen = 0;

static int eiGetFeatureData(size_t offset, size_t length, float* outPtr) {
    size_t pixelIx = offset * 3;
    for (size_t i = 0; i < length; i++) {
        uint8_t r = eiRgbBuf[pixelIx + 0];
        uint8_t g = eiRgbBuf[pixelIx + 1];
        uint8_t b = eiRgbBuf[pixelIx + 2];
        outPtr[i] = static_cast<float>((r << 16) | (g << 8) | b);
        pixelIx += 3;
    }
    return 0;
}
#endif

static bool initCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = (CAM_MODE == CAM_OPERATING_MODE_CAPTURE) ? PIXFORMAT_JPEG : PIXFORMAT_RGB565;
    config.frame_size   = FRAMESIZE_240X240;
    config.jpeg_quality = 12;
    config.fb_count     = psramFound() ? 2 : 1;
    config.fb_location  = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
    config.grab_mode    = CAMERA_GRAB_LATEST;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[CAM] Camera init failed: 0x%x\n", err);
        return false;
    }
    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        s->set_brightness(s, 0);
        s->set_contrast(s, 1);
        s->set_saturation(s, 1);
    }
    Serial.printf("[CAM] OV2640 ready (PSRAM=%s)\n", psramFound() ? "YES" : "NO");
    return true;
}

static void postVisionEvent(int slotIdx, BuildingType bldg, float conf) {
    if (WiFi.status() != WL_CONNECTED) return;

    JsonDocument doc;
    doc["slot"] = indexToSlotName(slotIdx);
    doc["building"] = buildingTypeToName(bldg);
    doc["confidence"] = conf;

    String payload;
    serializeJson(doc, payload);

    HTTPClient http;
    String url = String("http://") + MASTER_HTTP_HOST + ":" + String(MASTER_HTTP_PORT) + "/api/vision";
    http.begin(url);
    http.setTimeout(1500);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(payload);
    Serial.printf("[CAM] POST %s -> HTTP %d\n", payload.c_str(), code);
    http.end();
}

// Lightweight fallback RGB signature classifier when USE_EDGE_IMPULSE_FOMO == 0
static BuildingType classifyRoiColorFallback(const uint16_t* rgb565, int w, int h, const SlotRoi& roi) {
    int x0 = static_cast<int>(roi.minX * w);
    int x1 = static_cast<int>(roi.maxX * w);
    int y0 = static_cast<int>(roi.minY * h);
    int y1 = static_cast<int>(roi.maxY * h);

    uint32_t count = 0;
    uint32_t sumR = 0, sumG = 0, sumB = 0;
    for (int y = y0; y < y1; y += 2) {
        for (int x = x0; x < x1; x += 2) {
            uint16_t px = rgb565[y * w + x];
            uint8_t r = ((px >> 11) & 0x1F) << 3;
            uint8_t g = ((px >> 5) & 0x3F) << 2;
            uint8_t b = (px & 0x1F) << 3;
            sumR += r;
            sumG += g;
            sumB += b;
            count++;
        }
    }
    if (count == 0) return BLDG_EMPTY;

    float avgR = static_cast<float>(sumR) / count;
    float avgG = static_cast<float>(sumG) / count;
    float avgB = static_cast<float>(sumB) / count;
    float maxC = fmaxf(avgR, fmaxf(avgG, avgB));
    float minC = fminf(avgR, fminf(avgG, avgB));

    // Low saturation neutral gray/white baseplate studs -> empty slot
    if ((maxC - minC) < 22.0f && avgR > 80.0f) return BLDG_EMPTY;
    if (avgR > avgG + 35.0f && avgR > avgB + 35.0f) return BLDG_FACTORY; // Red brick
    if (avgG > avgR + 25.0f && avgG > avgB + 20.0f) return BLDG_PARK;    // Green brick
    if (avgB > avgR + 25.0f && avgB > avgG + 10.0f) return BLDG_HOUSE;   // Blue/Yellow house
    if (avgR < 65.0f && avgG < 65.0f && avgB < 85.0f) return BLDG_SOLAR; // Dark solar panel
    return BLDG_EMPTY;
}

static void handleRoot() {
    static const char HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8" />
  <title>BlockCity RTOS — ESP32-CAM FOMO Grid Alignment</title>
  <style>
    body { font-family: Arial, sans-serif; background: #111827; color: #f3f4f6; text-align: center; margin: 20px; }
    .stage { position: relative; display: inline-block; border: 2px solid #38bdf8; }
    img { display: block; width: 480px; height: 480px; }
    .roi { position: absolute; border: 1px dashed #22c55e; color: #22c55e; font-size: 12px; font-weight: bold; box-sizing: border-box; pointer-events: none; }
    .controls { margin-top: 14px; display: flex; gap: 10px; justify-content: center; flex-wrap: wrap; }
    button, a.btn { background: #0284c7; color: white; border: none; padding: 8px 14px; border-radius: 6px; cursor: pointer; text-decoration: none; }
  </style>
</head>
<body>
  <h2>BlockCity RTOS — Overhead Baseplate Alignment & Capture UI</h2>
  <div class="stage">
    <img id="preview" src="/capture.jpg" alt="Camera Frame" />
    <div class="roi" style="left:5%;top:8%;width:21%;height:40%;">A1</div>
    <div class="roi" style="left:27%;top:8%;width:21%;height:40%;">A2</div>
    <div class="roi" style="left:5%;top:52%;width:21%;height:40%;">A3</div>
    <div class="roi" style="left:27%;top:52%;width:21%;height:40%;">A4</div>
    <div class="roi" style="left:52%;top:8%;width:21%;height:40%;border-color:#f59e0b;color:#f59e0b;">B1</div>
    <div class="roi" style="left:74%;top:8%;width:21%;height:40%;border-color:#f59e0b;color:#f59e0b;">B2</div>
    <div class="roi" style="left:52%;top:52%;width:21%;height:40%;border-color:#f59e0b;color:#f59e0b;">B3</div>
    <div class="roi" style="left:74%;top:52%;width:21%;height:40%;border-color:#f59e0b;color:#f59e0b;">B4</div>
  </div>
  <div class="controls">
    <button onclick="refreshImg()">Refresh Frame</button>
    <a class="btn" id="dl" href="/capture.jpg" download="lego_sample.jpg">Download Sample JPEG</a>
    <label>Soft Fill LED: <input type="range" min="0" max="64" value="13" onchange="fetch('/flash?duty='+this.value)"></label>
  </div>
  <script>
    function refreshImg() {
      const ts = Date.now();
      document.getElementById('preview').src = '/capture.jpg?t=' + ts;
      document.getElementById('dl').href = '/capture.jpg?t=' + ts;
      document.getElementById('dl').download = 'blockcity_' + ts + '.jpg';
    }
  </script>
</body>
</html>
)rawliteral";
    camServer.send_P(200, "text/html", HTML);
}

static void handleCaptureJpg() {
    if (!cameraReady) {
        camServer.send(503, "text/plain", "Camera not ready");
        return;
    }
    if (xSemaphoreTake(camMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        camServer.send(503, "text/plain", "Camera busy");
        return;
    }
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        xSemaphoreGive(camMutex);
        camServer.send(500, "text/plain", "Capture failed");
        return;
    }

    uint8_t* jpgBuf = nullptr;
    size_t jpgLen = 0;
    bool ok = true;
    if (fb->format == PIXFORMAT_JPEG) {
        jpgBuf = fb->buf;
        jpgLen = fb->len;
    } else {
        ok = frame2jpg(fb, 80, &jpgBuf, &jpgLen);
    }

    if (ok && jpgBuf && jpgLen > 0) {
        camServer.sendHeader("Content-Disposition", "inline; filename=capture.jpg");
        camServer.send_P(200, "image/jpeg", reinterpret_cast<const char*>(jpgBuf), jpgLen);
    } else {
        camServer.send(500, "text/plain", "JPEG encode failed");
    }

    if (fb->format != PIXFORMAT_JPEG && jpgBuf) {
        free(jpgBuf);
    }
    esp_camera_fb_return(fb);
    xSemaphoreGive(camMutex);
}

static void handleFlashDuty() {
    if (camServer.hasArg("duty")) {
        int val = camServer.arg("duty").toInt();
        softFillDuty = static_cast<uint8_t>(constrain(val, 0, 255));
        ledcWrite(CAM_FLASH_LED, softFillDuty);
    }
    camServer.send(200, "text/plain", String(softFillDuty));
}

static void handleInject() {
    if (!camServer.hasArg("slot") || !camServer.hasArg("type")) {
        camServer.send(400, "text/plain", "Usage: POST /inject?slot=A1&type=HOUSE");
        return;
    }
    int slotIdx = slotNameToIndex(camServer.arg("slot").c_str());
    BuildingType bldg = buildingNameToType(camServer.arg("type").c_str());
    if (slotIdx < 0) {
        camServer.send(400, "text/plain", "Invalid slot");
        return;
    }
    postVisionEvent(slotIdx, bldg, 1.0f);
    camServer.send(200, "application/json", "{\"ok\":true}");
}

static void taskWebServer(void* pv) {
    (void)pv;
    for (;;) {
        camServer.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void taskVisionInference(void* pv) {
    (void)pv;
    for (;;) {
#if CAM_MODE == CAM_OPERATING_MODE_CAPTURE
        vTaskDelay(pdMS_TO_TICKS(500));
#else
        if (cameraReady && WiFi.status() == WL_CONNECTED &&
            xSemaphoreTake(camMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            camera_fb_t* fb = esp_camera_fb_get();
            if (fb && fb->format == PIXFORMAT_RGB565) {
                BuildingType frameSlots[NUM_SLOTS];
                float frameConf[NUM_SLOTS];
                for (int i = 0; i < NUM_SLOTS; ++i) {
                    frameSlots[i] = BLDG_EMPTY;
                    frameConf[i] = 0.0f;
                }

#if USE_EDGE_IMPULSE_FOMO
                size_t needed = static_cast<size_t>(EI_CLASSIFIER_INPUT_WIDTH) *
                                static_cast<size_t>(EI_CLASSIFIER_INPUT_HEIGHT) * 3;
                if (!eiRgbBuf || eiRgbBufLen < needed) {
                    eiRgbBuf = static_cast<uint8_t*>(ps_malloc(needed));
                    eiRgbBufLen = eiRgbBuf ? needed : 0;
                }
                if (eiRgbBuf && fmt2rgb888(fb->buf, fb->len, fb->format, eiRgbBuf)) {
                    ei::signal_t signal;
                    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
                    signal.get_data = &eiGetFeatureData;

                    ei_impulse_result_t result = {0};
                    if (run_classifier(&signal, &result, false) == EI_IMPULSE_OK) {
                        for (size_t ix = 0; ix < result.bounding_boxes_count; ix++) {
                            auto bb = result.bounding_boxes[ix];
                            if (bb.value < FOMO_CONFIDENCE_THRESHOLD) continue;
                            float cx = (bb.x + bb.width * 0.5f) / EI_CLASSIFIER_INPUT_WIDTH;
                            float cy = (bb.y + bb.height * 0.5f) / EI_CLASSIFIER_INPUT_HEIGHT;
                            int slotIdx = mapCentroidToSlot(cx, cy);
                            BuildingType bldg = buildingNameToType(bb.label);
                            if (slotIdx >= 0 && bldg != BLDG_EMPTY && bb.value > frameConf[slotIdx]) {
                                frameSlots[slotIdx] = bldg;
                                frameConf[slotIdx] = bb.value;
                            }
                        }
                    }
                }
#else
                const uint16_t* px = reinterpret_cast<const uint16_t*>(fb->buf);
                for (int i = 0; i < NUM_SLOTS; ++i) {
                    frameSlots[i] = classifyRoiColorFallback(px, fb->width, fb->height, DEFAULT_SLOT_ROIS[i]);
                    frameConf[i] = (frameSlots[i] == BLDG_EMPTY) ? 0.0f : 0.75f;
                }
#endif
                esp_camera_fb_return(fb);
                xSemaphoreGive(camMutex);

                for (int i = 0; i < NUM_SLOTS; ++i) {
                    if (debouncer.update(i, frameSlots[i])) {
                        postVisionEvent(i, debouncer.stableType(i), frameConf[i]);
                    }
                }
            } else {
                if (fb) esp_camera_fb_return(fb);
                xSemaphoreGive(camMutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(VISION_SCAN_INTERVAL_MS));
#endif
    }
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n[CAM] Booting BlockCity RTOS ESP32-CAM...");

    ledcAttach(CAM_FLASH_LED, PWM_FREQ_HZ, PWM_RES_BITS);
    ledcWrite(CAM_FLASH_LED, softFillDuty);

    camMutex = xSemaphoreCreateMutex();
    cameraReady = initCamera();

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("[CAM] Connecting to Wi-Fi %s", WIFI_SSID);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[CAM] IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("[CAM] Wi-Fi timeout — check secrets.h");
    }

    camServer.on("/", HTTP_GET, handleRoot);
    camServer.on("/capture.jpg", HTTP_GET, handleCaptureJpg);
    camServer.on("/flash", HTTP_GET, handleFlashDuty);
    camServer.on("/inject", HTTP_POST, handleInject);
    camServer.begin();

    xTaskCreatePinnedToCore(taskWebServer, "CamWeb", 4096, nullptr, 2, nullptr, 0);
    xTaskCreatePinnedToCore(taskVisionInference, "CamFOMO", 8192, nullptr, 2, nullptr, 1);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
