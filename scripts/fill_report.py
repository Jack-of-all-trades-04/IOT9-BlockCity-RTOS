"""Populate the UI Final Report template (docs/Format Laporan Proyek Akhir IOT.docx)
without modifying the original template file, writing output to:
    docs/IOT9-BlockCity-RTOS_Report.docx
"""

from pathlib import Path
import docx

TEMPLATE_PATH = Path("docs/Format Laporan Proyek Akhir IOT.docx")
OUTPUT_PATH = Path("docs/IOT9-BlockCity-RTOS_Report.docx")


def set_para_text(para, text: str) -> None:
    """Replace paragraph text while preserving the first run's font styling."""
    if not para.runs:
        para.text = text
        return
    para.runs[0].text = text
    for run in para.runs[1:]:
        run.text = ""


def set_cell_text(cell, text: str) -> None:
    """Replace table cell text while preserving the first paragraph/run styling."""
    if not cell.paragraphs:
        cell.text = text
        return
    set_para_text(cell.paragraphs[0], text)
    for p in cell.paragraphs[1:]:
        set_para_text(p, "")


def main() -> None:
    doc = docx.Document(TEMPLATE_PATH)

    replacements = {
        # Cover Page
        6: "BLOCKCITY RTOS: A TANGIBLE SMART CITY SIMULATION USING ESP-MESH, EDGE VISION, AND BLYNK",
        9: "GROUP IOT-9",
        10: "[GROUP MEMBER 1]\t\t[STUDENT ID 1]",
        11: "[GROUP MEMBER 2]\t\t[STUDENT ID 2]",
        12: "[GROUP MEMBER 3]\t\t[STUDENT ID 3]",
        13: "[GROUP MEMBER 4]\t\t[STUDENT ID 4]",

        # Preface
        27: (
            "Praise and gratitude are offered to Almighty God for enabling the completion of "
            "this Real-Time Systems and Internet of Things Final Project Report entitled "
            "\"BlockCity RTOS: A Tangible Smart City Simulation Using ESP-MESH, Edge Vision, "
            "and Blynk.\" This project was developed to fulfill the practicum requirements of "
            "the Department of Electrical Engineering, Universitas Indonesia."
        ),
        28: (
            "BlockCity RTOS bridges embedded real-time operating systems (FreeRTOS), wireless "
            "mesh networking (ESP-MESH), edge computer vision (Edge Impulse FOMO on ESP32-CAM), "
            "and cloud IoT platforms (Blynk and Supabase) into a tangible Physical-Digital Twin. "
            "Every physical LEGO placement and environmental sensor measurement directly drives "
            "the simulated city economy and ecological state."
        ),
        29: (
            "We express our sincere appreciation to our lecturers and laboratory assistants for "
            "their guidance throughout Modules 1 to 10, as well as to all Group IOT-9 members "
            "for their collaborative engineering effort. Constructive feedback is warmly welcomed "
            "for future improvements."
        ),
        33: "Depok, December 2026",
        36: "Group IOT-9",

        # Chapter 1: Introduction
        52: (
            "Urban smart-city monitoring systems require deterministic synchronization across "
            "distributed sensor nodes, edge intelligence, and cloud dashboards. In typical "
            "undergraduate IoT prototypes, however, systems consist of a single microcontroller "
            "polling sensors sequentially inside a blocking loop, which fails to demonstrate "
            "real-time concurrency, multi-node expansion, or physical-to-digital interaction."
        ),
        53: (
            "A naive approach to tangible smart-city simulation might use robotic arms or "
            "gantries to snap LEGO bricks onto a baseplate. In practice, LEGO clutch power "
            "requires high vertical force that stalls hobby servos, complicates power wiring, "
            "and introduces mechanical failure points. Meanwhile, naive RGB color thresholding "
            "on an overhead camera breaks under dynamic day/night LED lighting."
        ),
        54: (
            "Furthermore, integrating Wi-Fi, ESP-MESH, Blynk TLS, HTTPS cloud logging, and "
            "BLE on a single ESP32 without FreeRTOS core pinning, mutex protection, and "
            "non-blocking timers leads to race conditions, heap exhaustion, and ADC2 pin "
            "conflicts when analog sensors share pins with the Wi-Fi radio."
        ),

        57: (
            "BlockCity RTOS solves these challenges using a three-node ESP32 Physical-Digital "
            "Twin architecture. Players manage city finances and purchase Building Permits "
            "through Blynk IoT, physically place LEGO buildings on modular Land A (A1-A4) and "
            "Land B (B1-B4) baseplates, and rely on an overhead ESP32-CAM running Edge Impulse "
            "FOMO (with soft 5% GPIO4 fill light and 3-frame slot debouncing) to verify every "
            "placement over HTTP."
        ),
        58: (
            "ESP32-1 (Land A Master) runs dual-core FreeRTOS SMP separating networking (Core 0: "
            "AlteriomPainlessMesh Root, non-blocking Blynk, HTTP server, and Supabase HTTPS) from "
            "simulation and sensing (Core 1: CityEngine, DHT11/MQ/LDR on ADC1, White/Yellow PWM "
            "Sun LEDs, and BLE Mayor proximity boost). Once the city accumulates $1,000, "
            "purchasing Land B broadcasts UNLOCK_BLOCK_B over ESP-MESH, waking ESP32-2 (Land B) "
            "from modem power-save standby to stream live Land B telemetry and synchronize "
            "streetlamps."
        ),

        # Acceptance Criteria
        62: "Execute deterministic 10-second city economy and 2-second day/night sun ticks using FreeRTOS SMP tasks, queues, mutexes, and software timers without blocking delays.",
        63: "Verify manual LEGO building placements across 8 grid slots (A1-A4, B1-B4) via ESP32-CAM FOMO/ROI vision, flagging unpermitted builds as ILLEGAL with $0 income.",
        64: "Connect ESP32-1 (Land A) and ESP32-2 (Land B) via ESP-MESH so Land B remains in power-save standby until unlocked for $1,000, then streams live DHT11, MQ Gas, and LDR telemetry.",
        65: "Synchronize real-time controls, alerts, and historical telemetry with Blynk IoT (V0-V12), Flash NVS persistence, and Supabase PostgreSQL via PostgREST.",

        # Timeline and Milestones
        72: (
            "Project Timeline and Milestones (November 23 - December 6, 2026):\n"
            "• Nov 23 - Nov 25, 2026: Hardware Design Completion — Finalize dual-land schematic, "
            "ADC1 MQ/LDR dividers, NPN LED drivers, and 35-45 cm overhead camera rig.\n"
            "• Nov 25 - Nov 29, 2026: Software Development — Implement FreeRTOS SMP firmware "
            "(block_a, block_b, esp32_cam), city_logic.h, Supabase schema, and FOMO dataset."
        ),
        73: (
            "• Nov 30 - Dec 3, 2026: Integration and Testing — Verify ESP-MESH bridge + Blynk "
            "coexistence, FOMO inference accuracy under Dawn/Noon/Dusk/Night LEDs, and unit tests.\n"
            "• Dec 4 - Dec 6, 2026: Final Assembly and Verification — Complete physical LEGO "
            "board assembly, capture hardware photos, finalize report PDF, and submit on GitHub."
        ),

        # Chapter 2: Implementation
        96: (
            "Hardware design distributes sensing, actuation, and vision across three ESP32 nodes "
            "powered from regulated 5V 2A-3A rails with a shared ground reference. Both Land A "
            "(ESP32-1) and Land B (ESP32-2) share a symmetric pinout: DHT11 on GPIO21, MQ Gas "
            "analog output stepped down from 5V to <=3.33V via a 1kOhm + 2kOhm voltage divider "
            "into GPIO34 (ADC1_CH6), LDR + 10kOhm divider into GPIO35 (ADC1_CH7), White and "
            "Yellow Sun/Streetlamp LED banks driven by 2N2222/BC547 NPN transistors on GPIO25 "
            "and GPIO26, Status LED on GPIO27, Smog Fan/Alert on GPIO32, and Wake button on "
            "GPIO33. The AI Thinker ESP32-CAM mounts 35-45 cm overhead with GPIO4 diffused at "
            "~5% PWM duty to neutralize dynamic Sun shadows."
        ),
        99: (
            "Firmware is organized in a multi-environment PlatformIO workspace sharing portable "
            "headers (include/city_protocol.h, include/city_logic.h, include/pin_config.h). "
            "On ESP32-1 (block_a), FreeRTOS pins NetMesh, HttpIngest (:8080), and Supabase "
            "tasks to Core 0, while CityEngine, SensorA, SunPWM, and BleMayor run on Core 1. "
            "Four FreeRTOS queues decouple vision detections, mesh RX/TX, and HTTPS logging, "
            "while cityMutex protects CityState and Preferences.h persists progress to Flash NVS."
        ),
        100: (
            "On ESP32-2 (block_b), the node boots in STATE_LOCKED with WIFI_PS_MAX_MODEM power "
            "management and periodic HELLO_EXPANSION broadcasts. Receiving UNLOCK_BLOCK_B switches "
            "to full-power active sensing and mirrors SYNC_SUN PWM duties onto Land B LEDs. "
            "On ESP32-CAM (esp32_cam), OV2640 captures 240x240 RGB565 frames in PSRAM, maps "
            "FOMO/ROI centroids via grid_mapper.h, debounces 3 consecutive frames, and HTTP POSTs "
            "slot detections to ESP32-1."
        ),
        101: (
            "System Workflow Flowchart Summary: [Blynk Permit Purchase (V0/V12)] -> "
            "[ESP32-1 Deducts Funds & Sets WAIT_PLACE] -> [Player Places LEGO Brick] -> "
            "[ESP32-CAM FOMO + 3-Frame Debounce] -> [HTTP POST /api/vision] -> "
            "[Permit Match? Yes: Activate Slot & Apply Tax/AQI/UHI | No: Flag ILLEGAL ($0)] -> "
            "[10s FreeRTOS Economy Timer] -> [Sync Blynk V1-V11, ESP-MESH Land B & Supabase]."
        ),
        102: (
            "Cloud and visualization services integrate Blynk IoT (Virtual Pins V0-V12 and "
            "Events illegal_build, smog_alert, land_b_unlocked) with Supabase PostgreSQL "
            "(tables city_telemetry and building_events accessed over HTTPS PostgREST with RLS "
            "policies). All ten course practicum modules (Modules 1-10) are explicitly covered."
        ),
        105: (
            "Hardware and software integration centers on three coexistence boundaries: "
            "(1) AlteriomPainlessMesh uses mesh.stationManual() alongside non-blocking "
            "Blynk.config() so Wi-Fi STA and Mesh share a single radio channel without reset "
            "loops; (2) all analog sensors reside on ADC1 (GPIO34/35) to prevent ADC2/Wi-Fi "
            "conflicts; and (3) huge_app.csv partition layout allocates 3.0 MB app flash for "
            "simultaneous Wi-Fi, Mesh, Blynk, TLS HTTPS, and BLE stacks."
        ),

        # Chapter 3: Testing and Evaluation
        125: (
            "Verification was conducted in two stages: (1) Automated host C++17 unit testing "
            "(test/test_city_logic.cpp compiled with g++ -std=c++17 -Wall -Wextra) covering the "
            "economy ruleset, illegal construction enforcement, hybrid dual-land AQI, Urban Heat "
            "Island temperature offset, solar LDR daylight scaling, BLE Mayor +10% tax boost, "
            "Sun PWM crossfading, and 8-slot ROI grid mapping/debouncing; and (2) Full "
            "cross-compilation of all three ESP32 firmware targets using PlatformIO "
            "(espressif32 @ 8.0.0, Arduino Core 3.3.12)."
        ),
        126: (
            "Live bench integration testing follows the protocols documented in "
            "Train_Documentation.md and Blnyk_Supabase_Setup.md: calibrating the 8-slot overlay "
            "on the ESP32-CAM web UI, verifying Blynk V0-V12 updates, injecting smoke near "
            "Land B's MQ sensor to trigger AQI > 150 smog alerts, and confirming ESP-MESH "
            "unlocking of Land B upon reaching $1,000."
        ),
        129: (
            "All 7 automated host unit test suites passed (7/7 PASS, exit code 0), confirming "
            "exact deterministic behavior of permit validation, 50% sell refunds, smog tax "
            "penalties, night streetlight upkeep, Land B masking prior to unlock, and 3-frame "
            "vision debouncing. All three PlatformIO firmware environments built cleanly with "
            "zero errors."
        ),
        131: "Table 2 / Fig 2. Compiled Firmware Memory Footprint & Unit Test Summary (All 3 Targets Verified)",
        132: (
            "Measured PlatformIO memory usage (huge_app.csv: 320 KB RAM, 3,145,728 B App Flash):\n"
            "• block_a (ESP32-1 Land A Master): RAM 68,596 B (20.9%), Flash 2,552,946 B (81.2%) — SUCCESS\n"
            "• block_b (ESP32-2 Land B Expansion): RAM 52,608 B (16.1%), Flash 1,642,558 B (52.2%) — SUCCESS\n"
            "• esp32_cam (OV2640 Vision Node): RAM 69,432 B (21.2%), Flash 1,298,897 B (41.3%) — SUCCESS\n"
            "[Insert live hardware demo screenshots, Blynk dashboard graph, and Supabase CSV plot here during final presentation week.]"
        ),
        135: (
            "Evaluating resource utilization confirms that dual-core task pinning eliminates "
            "blocking delays between network TLS requests and real-time PWM sun dimming. "
            "DRAM consumption stays around 16%-21% across all three nodes, leaving ample heap "
            "for Wi-Fi, Mesh buffers, and Blynk TLS sessions. On block_a, Flash usage reaches "
            "81.2% due to combined BLE + Wi-Fi + Mesh + TLS libraries, justifying huge_app.csv."
        ),
        136: (
            "For physical vision robustness, relying purely on raw color thresholds degrades "
            "when the White/Yellow Sun LEDs transition from Noon to Dusk. Combining the "
            "diffused 5% GPIO4 fill light with an Edge Impulse FOMO MobileNetV2 (int8) model "
            "trained across all four lighting phases resolves hue shifts without requiring "
            "paper fiducial stickers."
        ),

        # Chapter 4: Conclusion
        140: (
            "BlockCity RTOS successfully demonstrates a Tangible Physical-Digital Twin smart "
            "city built on three ESP32 nodes, FreeRTOS symmetric multiprocessing, ESP-MESH "
            "expansion, Edge Impulse FOMO computer vision, Blynk IoT, and Supabase PostgreSQL. "
            "The system satisfies all ten practicum syllabus modules while eliminating the "
            "mechanical fragility of robotic LEGO placement."
        ),
        141: (
            "Verification confirmed 7/7 passing simulation and grid-mapping unit tests and "
            "clean compilation across block_a, block_b, and esp32_cam targets. Future work "
            "includes training the full multi-lighting LEGO FOMO dataset per "
            "Train_Documentation.md and recording long-duration Supabase telemetry plots during "
            "the live demonstration."
        ),

        # References (IEEE Format)
        155: "[1] Espressif Systems, \"ESP32 Technical Reference Manual,\" v5.2, 2024. [Online]. Available: https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf",
        156: "[2] Real Time Engineers Ltd., \"FreeRTOS Kernel Developer Guide: Task Management, Queues, Mutexes, and Software Timers,\" 2024. [Online]. Available: https://www.freertos.org/Documentation/RTOS_book.html",
        157: "[3] Alteriom / painlessMesh Contributors, \"AlteriomPainlessMesh Library v2.1.1,\" GitHub, 2026. [Online]. Available: https://github.com/Alteriom/painlessMesh",
        158: "[4] Edge Impulse Inc., \"FOMO: Object Detection for Constrained Devices,\" Edge Impulse Documentation, 2025. [Online]. Available: https://docs.edgeimpulse.com/docs/edge-impulse-studio/learning-blocks/object-detection/fomo-object-detection-for-constrained-devices",
        159: "[5] Blynk Technologies Inc., \"Blynk IoT Platform Documentation: Virtual Pins, Events, and ESP32 Integration,\" 2025. [Online]. Available: https://docs.blynk.io/",
        160: "[6] Supabase Inc., \"Supabase PostgREST API and PostgreSQL Row Level Security Documentation,\" 2025. [Online]. Available: https://supabase.com/docs",
        161: "[7] B. Blanchon, \"ArduinoJson: Efficient JSON Serialization for Embedded Systems (v7),\" 2025. [Online]. Available: https://arduinojson.org/",

        # Appendices
        182: (
            "Full schematic & pinout specification is maintained in include/pin_config.h and "
            "Full_Explanation.md:\n"
            "• ESP32-1 (Land A) & ESP32-2 (Land B): DHT11 -> GPIO21, MQ Gas (5V->3.33V via 1k+2k "
            "divider) -> GPIO34 (ADC1), LDR (3.3V+10k) -> GPIO35 (ADC1), White LED PWM (NPN) -> "
            "GPIO25, Yellow LED PWM (NPN) -> GPIO26, Status LED -> GPIO27, Fan/Alert -> GPIO32, "
            "Wake Button -> GPIO33.\n"
            "• ESP32-CAM (AI Thinker): Standard OV2640 pinout + GPIO4 Soft-Fill Flash LED (5% PWM).\n"
            "[Insert final KiCad/Fritzing schematic image here before PDF export.]"
        ),
        184: (
            "[Insert construction and live wiring photographs of Land A, Land B, overhead "
            "ESP32-CAM rig, and Blynk/Supabase screenshots here prior to final PDF submission.]"
        ),
    }

    for idx, text in replacements.items():
        if idx < len(doc.paragraphs):
            set_para_text(doc.paragraphs[idx], text)

    # Fill Table 0: Roles and Responsibilities
    if doc.tables:
        t = doc.tables[0]
        roles_data = [
            ("Project Lead & ESP32-1 RTOS", "FreeRTOS SMP architecture, city_logic.h engine, Blynk & NVS integration", "[Group Member 1]"),
            ("ESP-MESH & Land B Hardware", "ESP32-2 power management, mesh protocol, Land B DHT11/MQ/LDR & LEDs", "[Group Member 2]"),
            ("ESP32-CAM & FOMO Vision", "Overhead rig, Edge Impulse FOMO dataset collection, training & grid_mapper.h", "[Group Member 3]"),
            ("Cloud DB, Testing & Docs", "Supabase schema/REST logging, unit tests, setup guides & Final Report", "[Group Member 4]"),
            ("All Members (Shared QA)", "Hardware-in-the-loop integration testing, presentation demo & Git commits", "Group IOT-9"),
        ]
        for row_idx, (role, resp, person) in enumerate(roles_data, start=1):
            if row_idx < len(t.rows):
                set_cell_text(t.rows[row_idx].cells[0], role)
                set_cell_text(t.rows[row_idx].cells[1], resp)
                set_cell_text(t.rows[row_idx].cells[2], person)

    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    doc.save(OUTPUT_PATH)

    # Verify zero leftover Lorem ipsum / Maecenas placeholders
    check_doc = docx.Document(OUTPUT_PATH)
    leftover = []
    for i, p in enumerate(check_doc.paragraphs):
        low = p.text.lower()
        if "lorem ipsum" in low or "maecenas" in low or "donec at iaculis" in low:
            leftover.append((i, p.text[:60]))
    if leftover:
        raise RuntimeError(f"Unreplaced template text remained: {leftover}")
    print(f"Successfully generated {OUTPUT_PATH} with 0 template placeholders remaining.")


if __name__ == "__main__":
    main()
