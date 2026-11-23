# IOT9-BlockCity-RTOS

**BlockCity RTOS: A Tangible Smart City Simulation Using ESP-MESH, Edge Vision, and Blynk**

A FreeRTOS & IoT Physical–Digital Twin smart city built with **3× ESP32 nodes** (`ESP32-1` Land A Master, `ESP32-2` Land B Expansion, `ESP32-CAM` Overhead FOMO Vision), dual per-land environmental sensors (**DHT11**, **MQ Gas**, **LDR**, **White + Yellow Sun LEDs**), **Blynk IoT**, and **Supabase PostgreSQL**.

- **Deep Technical Documentation:** [Full_Explanation.md](Full_Explanation.md)
- **FOMO LEGO Dataset & Training Guide:** [Train_Documentation.md](Train_Documentation.md)
- **Blynk & Supabase Setup Guide:** [Blnyk_Supabase_Setup.md](Blnyk_Supabase_Setup.md)
- **Final Report (`.docx`):** [docs/IOT9-BlockCity-RTOS_Report.docx](docs/IOT9-BlockCity-RTOS_Report.docx)

---

## Introduction

Modern smart city monitoring relies on distributed sensor networks, deterministic real-time scheduling, and synchronized edge-to-cloud pipelines. **BlockCity RTOS** models these mechanics as an interactive physical–digital twin: players buy building permits in **Blynk IoT**, physically snap **LEGO buildings** onto modular **Land A (`A1–A4`)** and **Land B (`B1–B4`)** baseplates, and have every placement verified in real time by an overhead **ESP32-CAM** running **Edge Impulse FOMO**.

Real-world environmental sensors on **both** Land A and Land B—**DHT11** (temperature/humidity), **MQ Gas** (air quality/smoke), and **LDR** (ambient daylight)—directly drive the virtual economy, urban heat island offset, solar farm output, automatic dusk/night streetlamps, and smog alerts. Saving **$1,000** unlocks **Land B** over **ESP-MESH**, waking **ESP32-2** from power-save standby into full synchronized operation.

This design satisfies **10 out of 10** course practicum modules (FreeRTOS SMP, Task Management, Queues, Mutex Synchronization, Software Timers, BLE Proximity Boost, Wi-Fi/HTTP, Power Management, Blynk IoT, and ESP-MESH).

---

## Implementation

### Repository Structure
```text
├── include/
│   ├── city_logic.h          # Portable C++17 simulation engine & environmental math
│   ├── city_protocol.h       # Economy constants, enums, Blynk V-pins, Mesh commands
│   ├── pin_config.h          # Symmetric Land A/B GPIO pinout + AI Thinker ESP32-CAM
│   └── secrets.h.example     # Template for Wi-Fi, Blynk, Supabase & BLE credentials
├── src/
│   ├── block_a/main_block_a.cpp  # ESP32-1: FreeRTOS SMP, Mesh Root, Blynk, HTTP, Supabase, BLE
│   ├── block_b/main_block_b.cpp  # ESP32-2: Mesh Leaf, Land B Sensors/LEDs, Power Management
│   └── esp32_cam/                # ESP32-CAM: OV2640 Capture/FOMO UI + 8-slot Grid Mapper
├── supabase/schema.sql       # PostgreSQL tables (city_telemetry, building_events) & RLS
├── test/test_city_logic.cpp  # Host C++17 unit tests for economy, AQI, UHI & ROI mapper
├── scripts/fill_report.py    # Populates the UI Final Report template (.docx)
├── Train_Documentation.md    # Step-by-step LEGO FOMO dataset & Edge Impulse guide
├── Blnyk_Supabase_Setup.md   # Step-by-step Blynk & Supabase configuration guide
└── Full_Explanation.md       # Complete system, RTOS, hardware & math reference
```

### Quick Build & Setup
1. Copy `include/secrets.h.example` to `include/secrets.h` and configure Wi-Fi, Blynk, and Supabase credentials (see [Blnyk_Supabase_Setup.md](Blnyk_Supabase_Setup.md)).
2. Train and deploy the LEGO FOMO model onto `esp32_cam` following [Train_Documentation.md](Train_Documentation.md).
3. Build and flash each environment using PlatformIO:
   ```powershell
   pio run -e block_a -t upload
   pio run -e block_b -t upload
   pio run -e esp32_cam -t upload
   ```

---

## Testing and Evaluation

### Automated Host Unit Tests (`test/test_city_logic.cpp`)
Compiled with `g++ -std=c++17 -Wall -Wextra` and executed after all firmware edits:
- **7 / 7 test suites passed**:
  1. `testInitialState` — Verifies `$500` starting funds, Land B locked, all 8 slots empty.
  2. `testPermitAndPlaceWorkflow` — Verifies Land B rejection before unlock, permit deduction, and vision activation.
  3. `testIllegalConstructionAndSell` — Verifies unpermitted LEGO brick flagging (`$0` income) and 50% sell refund.
  4. `testEconomySmogSolarNightAndBleBoost` — Verifies `+0.5°C` UHI, `AQI > 150` `-50%` house tax penalty, night streetlight `-$10` upkeep, solar LDR scaling, and BLE `+10%` boost.
  5. `testLandBExpansionAndHybridAqi` — Verifies `$1,000` Land B unlock and dual-land MQ AQI averaging.
  6. `testSunPwmCrossfade` — Verifies Dawn/Noon/Dusk/Night White & Yellow PWM transitions.
  7. `testGridMapperAndDebouncer` — Verifies normalized `(cx, cy)` ROI mapping (`A1–B4`) and 3-frame debouncing.

### PlatformIO Firmware Compilation Results (`espressif32 @ 8.0.0`, `huge_app.csv`)

| Target Environment | Board | RAM Usage | Flash Usage | Build Result |
| :--- | :--- | :--- | :--- | :--- |
| `block_a` | `nodemcu-32s` | `68,596 B` (20.9%) | `2,552,946 B` (81.2%) | **SUCCESS** |
| `block_b` | `nodemcu-32s` | `52,608 B` (16.1%) | `1,642,558 B` (52.2%) | **SUCCESS** |
| `esp32_cam` | `esp32cam` | `69,432 B` (21.2%) | `1,298,897 B` (41.3%) | **SUCCESS** |

### Evaluation Highlights
- Separating networking (`Core 0`) from deterministic simulation and sensor polling (`Core 1`) prevents Wi-Fi/TLS latency from stalling PWM sun transitions or economy ticks.
- Using `huge_app.csv` accommodates the combined Wi-Fi + `AlteriomPainlessMesh` + Blynk + HTTPS + BLE stack on `block_a` (81.2% Flash, 20.9% RAM) with comfortable DRAM headroom.

---

## Conclusion

**BlockCity RTOS** successfully integrates FreeRTOS symmetric multiprocessing, inter-task queues, mutexes, software timers, BLE proximity detection, HTTP REST, ESP-MESH expansion, Edge Impulse FOMO vision, Blynk IoT, and Supabase persistence into a cohesive Physical–Digital Twin smart city simulation. All three ESP32 firmware targets compile cleanly and all seven core simulation and vision-mapping unit tests pass.

---

## References

1. Espressif Systems, *ESP32 Technical Reference Manual*, v5.2, 2024. [Online]. Available: https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf
2. FreeRTOS, *FreeRTOS Kernel Developer Guide — Task Management, Queues, Semaphores, and Software Timers*. [Online]. Available: https://www.freertos.org/Documentation/RTOS_book.html
3. Alteriom / painlessMesh Contributors, *AlteriomPainlessMesh Library v2.1.1*. [Online]. Available: https://github.com/Alteriom/painlessMesh
4. Edge Impulse, *FOMO: Object Detection for Constrained Devices*. [Online]. Available: https://docs.edgeimpulse.com/docs/edge-impulse-studio/learning-blocks/object-detection/fomo-object-detection-for-constrained-devices
5. Blynk Technologies Inc., *Blynk IoT Documentation — Virtual Pins & Events*. [Online]. Available: https://docs.blynk.io/
6. Supabase Inc., *Supabase PostgREST API & Row Level Security Documentation*. [Online]. Available: https://supabase.com/docs
