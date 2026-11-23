#pragma once

// ============================================================================
// BlockCity RTOS Hardware Pin Configuration
// Symmetric sensor/actuator layout for ESP32-1 (Land A) and ESP32-2 (Land B)
// ============================================================================

// --- Land A (ESP32-1) & Land B (ESP32-2) Per-Land Pins ---
// IMPORTANT: Analog inputs MUST use ADC1 (GPIO 32-39).
// ADC2 pins are unusable while Wi-Fi / ESP-MESH is active.
#define PIN_DHT         21   // DHT11 Data (use 4.7k-10k pull-up if bare sensor)
#define PIN_MQ_AO       34   // ADC1_CH6: MQ-135/MQ-2 Analog Out via 1k+2k voltage divider (<=3.3V)
#define PIN_LDR_AO      35   // ADC1_CH7: LDR + 10k pull-down divider powered from 3.3V

#define PIN_LED_WHITE   25   // PWM -> NPN Transistor (2N2222/BC547) -> White Sun LED Bank
#define PIN_LED_YELLOW  26   // PWM -> NPN Transistor (2N2222/BC547) -> Yellow Sun/Streetlamp Bank
#define PIN_STATUS_LED  27   // Zone Status LED (Land A heartbeat / Land B lock/unlock indicator)
#define PIN_FAN_ALERT   32   // Smog Exhaust Fan / Buzzer Alert Trigger (Active HIGH when AQI > 150)
#define PIN_WAKE_BTN    33   // Land B External Wake / Hot-Plug Sense Input (RTC GPIO)

// PWM Settings (Arduino Core 3.x ledcAttach)
#define PWM_FREQ_HZ     5000
#define PWM_RES_BITS    8

// --- AI Thinker ESP32-CAM Pinout ---
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22
#define CAM_FLASH_LED    4
