# Train_Documentation.md — Edge Impulse FOMO Training Guide for BlockCity RTOS

This guide walks you through setting up the physical LEGO baseplate rig, collecting real LEGO brick images using the ESP32-CAM, training a **FOMO (Faster Objects, More Objects)** model in Edge Impulse, and deploying the quantized `.zip` library back onto the ESP32-CAM.

---

## 1. Physical Baseplate & LEGO Building Setup

### 1.1 Baseplate Grid Geometry (8 Slots)
Place two **16×16** LEGO baseplates side by side (forming a **32×16** city grid) directly underneath the overhead ESP32-CAM mount:

```text
+-----------------------------------+-----------------------------------+
|         LAND A (ESP32-1)          |         LAND B (ESP32-2)          |
|  +-------------+ +-------------+  |  +-------------+ +-------------+  |
|  |     A1      | |     A2      |  |  |     B1      | |     B2      |  |
|  |  (0.05..26, | |  (0.27..48, |  |  |  (0.52..73, | |  (0.74..95, |  |
|  |   0.08..48) | |   0.08..48) |  |  |   0.08..48) | |   0.08..48) |  |
|  +-------------+ +-------------+  |  +-------------+ +-------------+  |
|  +-------------+ +-------------+  |  +-------------+ +-------------+  |
|  |     A3      | |     A4      |  |  |     B3      | |     B4      |  |
|  |  (0.05..26, | |  (0.27..48, |  |  |  (0.52..73, | |  (0.74..95, |  |
|  |   0.52..92) | |   0.52..92) |  |  |   0.52..92) | |   0.52..92) |  |
|  +-------------+ +-------------+  |  +-------------+ +-------------+  |
+-----------------------------------+-----------------------------------+
```

### 1.2 Camera Rig & Lighting
- **Camera Height:** Mount the AI Thinker **ESP32-CAM** on a rigid PVC, acrylic, or LEGO Technic overhead bridge **35–45 cm** directly above the seam between Land A and Land B.
- **Anti-Glare Fill Light:** Place a thin piece of white paper/diffuser tape over the onboard GPIO 4 flash LED. The firmware automatically runs GPIO 4 at ~5% PWM duty (`CAM_SOFT_FILL_DUTY = 13`) so shadows from the dynamic White/Yellow Sun LEDs do not confuse FOMO.
- **Baseplate Color:** Use a neutral light-gray or white baseplate so colored LEGO bricks stand out clearly.

### 1.3 Recommended LEGO Brick Classes (4 Labels)
Build distinct, consistent LEGO roofs/models for each class so FOMO achieves >90% F1-score:

| Label in Edge Impulse | City Role | Recommended LEGO Visual Design |
| :--- | :--- | :--- |
| `house` | Residential House ($100) | Yellow or Blue 4×4 brick stack with sloped roof |
| `factory` | Industrial Factory ($250) | Red 4×4 or 4×6 brick block with gray smokestack cylinder |
| `park` | Green Park ($150) | Bright Green flat tile + tree/bush studs |
| `solar` | Solar Farm ($300) | Dark Blue/Black slanted tile panel on gray base |

> **Tip:** Avoid mixing identical brick colors between classes (for example, do not make both House and Factory red).

---

## 2. Step 1 — Flash ESP32-CAM in Capture & Alignment Mode

1. Copy `include/secrets.h.example` to `include/secrets.h` (already created locally) and fill in your 2.4 GHz `WIFI_SSID` and `WIFI_PASSWORD`.
2. Open `src/esp32_cam/fomo_config.h` and set:
   ```cpp
   #define CAM_MODE CAM_OPERATING_MODE_CAPTURE
   ```
3. Attach the **ESP32-CAM-MB** USB programmer and flash:
   ```powershell
   pio run -e esp32_cam -t upload
   pio device monitor -b 115200
   ```
4. Note the printed IP address (e.g., `http://192.168.1.120`) and open it in your browser.
5. Align your two LEGO baseplates until slots `A1–A4` (green dashed boxes) and `B1–B4` (amber dashed boxes) line up with your 8 physical lots. Adjust `DEFAULT_SLOT_ROIS` in `src/esp32_cam/grid_mapper.h` if needed.

---

## 3. Step 2 — Collect & Label the Dataset

To prevent accuracy drops when the White/Yellow Sun LEDs dim from Noon to Dusk/Night, collect samples under **all lighting conditions**:

1. Capture **60–100 JPEG images per class** (`house`, `factory`, `park`, `solar`), plus **40+ multi-building scenes** with 2 to 8 buildings placed simultaneously across `A1–B4`, and **20 empty baseplate images**.
2. Use the **"Download Sample JPEG"** button on the ESP32-CAM web page (or script `curl http://<CAM_IP>/capture.jpg -o sample_001.jpg`).
3. Vary brick orientation (0°, 90°, 180°, 270°) and Sun LED brightness (Noon White, Dawn/Dusk Yellow, Night).
4. In **Edge Impulse Studio** (`https://studio.edgeimpulse.com`):
   - Create a new project (e.g., `BlockCity_FOMO`).
   - Go to **Data acquisition → Upload data** and upload your `.jpg` files with an 80/20 Train/Test split.
   - Open **Labeling queue** and draw tight bounding boxes around every LEGO building in each image, tagging each box as `house`, `factory`, `park`, or `solar`. Leave empty baseplate studs unlabeled (FOMO learns them as background).

---

## 4. Step 3 — Train FOMO in Edge Impulse Studio

1. **Create Impulse**:
   - **Image data**: `96 × 96` px, Resize mode = **Squash** (or Fit Shortest Axis — must match your camera square `240×240` crop).
   - **Processing block**: **Image** (Color depth = **RGB**).
   - **Learning block**: **Object Detection (Images)** → Choose **FOMO (Faster Objects, More Objects) MobileNetV2 0.35**.
2. **Generate Features**:
   - Click **Image → Save parameters → Generate features**. Verify clusters separate cleanly in Feature Explorer.
3. **Train Model**:
   - Epochs: `60`–`80`, Learning rate: `0.001`, Batch size: `32`, enable **Data augmentation**.
   - Click **Start training**.
   - Target **F1-score ≥ 85%** (ideally >90%) on validation data before deploying.
4. **Model Testing**:
   - Run **Classify all** on your 20% Test set and inspect the Confusion Matrix for any misclassifications between classes.

---

## 5. Step 4 — Export & Deploy to ESP32-CAM

1. Go to **Deployment** in Edge Impulse Studio.
2. Select **Arduino library** and choose **Quantized (int8)** optimization (EON Compiler enabled).
3. Check the estimated resource footprint (must stay under ~250 KB RAM and fit in `huge_app.csv` flash).
4. Click **Build** to download the `.zip` library (e.g., `ei-blockcity_fomo-arduino-1.0.x.zip`).
5. Extract the `.zip` into `lib/` so the header path is `lib/<ProjectName>_inferencing/src/<ProjectName>_inferencing.h`.
6. Update `src/esp32_cam/fomo_config.h`:
   ```cpp
   #define CAM_MODE CAM_OPERATING_MODE_INFERENCE
   #define USE_EDGE_IMPULSE_FOMO 1
   ```
7. Update the `#include <BlockCity_FOMO_inferencing.h>` line in `src/esp32_cam/main_cam.cpp` to match your exact downloaded Edge Impulse header filename.
8. Flash the ESP32-CAM:
   ```powershell
   pio run -e esp32_cam -t upload
   ```

---

## 6. Verification & Troubleshooting

| Symptom | Root Cause | Fix |
| :--- | :--- | :--- |
| `Camera init failed: 0x105` or brownout reboot | Insufficient 5V current from USB port | Power ESP32-CAM from a dedicated 5V 2A–3A adapter with common GND |
| `run_classifier` returns `EI_IMPULSE_OUT_OF_MEMORY` | PSRAM not enabled or buffer allocation failed | Ensure `-DBOARD_HAS_PSRAM` is present in `platformio.ini` `[env:esp32_cam]` |
| Wrong slot triggered (e.g., `A1` triggers `A2`) | Camera tilted or shifted on mount | Open `http://<CAM_IP>/` and realign baseplate or tune `DEFAULT_SLOT_ROIS` in `grid_mapper.h` |
| Houses misread as Factories at Dusk | Yellow/Orange Sun LED shifts hue | Keep soft-fill LED (`duty ~ 13`) active and add Dusk/Night photos to Edge Impulse dataset |
