#pragma once

// ============================================================================
// ESP32-CAM FOMO Configuration & Operating Modes
// ============================================================================

#define CAM_OPERATING_MODE_CAPTURE   1
#define CAM_OPERATING_MODE_INFERENCE 2

// Switch to CAM_OPERATING_MODE_CAPTURE when collecting LEGO training photos.
// Switch to CAM_OPERATING_MODE_INFERENCE for live city operation.
#define CAM_MODE CAM_OPERATING_MODE_INFERENCE

// Set to 1 after exporting your Edge Impulse Arduino Library (.zip) into lib/
// and updating the include header in main_cam.cpp.
#define USE_EDGE_IMPULSE_FOMO 0

#define FOMO_CONFIDENCE_THRESHOLD    0.60f
#define VISION_DEBOUNCE_FRAMES       3
#define VISION_SCAN_INTERVAL_MS      1500
#define CAM_SOFT_FILL_DUTY           13    // ~5% of 255 to soften Sun LED shadows
