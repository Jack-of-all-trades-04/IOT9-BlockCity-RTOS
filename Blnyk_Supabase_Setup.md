# Blnyk_Supabase_Setup.md — Blynk IoT & Supabase Cloud Setup Guide

This guide explains how to configure **Blynk IoT** (the player control & real-time telemetry dashboard) and **Supabase** (PostgreSQL historical telemetry & event logging) for **BlockCity RTOS**.

---

## 1. Credential Security (`include/secrets.h`)

Never commit real Wi-Fi passwords, Blynk tokens, or Supabase keys to GitHub.
1. `include/secrets.h.example` is tracked in Git as a safe template.
2. `include/secrets.h` is listed in `.gitignore` and stays strictly local on your computer.
3. Fill in the constants inside `include/secrets.h` as you complete Sections 2 and 3 below.

---

## 2. Blynk IoT Setup (Player Interface & Telemetry)

### 2.1 Create the Template
1. Log in to [Blynk Cloud](https://blynk.cloud/) and go to **Developer Zone → Templates → + New Template**.
2. Set:
   - **Name:** `BlockCity RTOS`
   - **Hardware:** `ESP32`
   - **Connection Type:** `WiFi`
3. Copy the generated **Template ID** (`BLYNK_TEMPLATE_ID`) and **Template Name** (`BLYNK_TEMPLATE_NAME`) into `include/secrets.h`.

### 2.2 Configure Datastreams (Virtual Pins `V0`–`V12`)
Open the **Datastreams** tab and create all 13 Virtual Pins exactly as listed:

| Pin | Name | Data Type | Units | Min / Max | Default | Purpose |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `V0` | `City Command` | String | — | — | `""` | Command parser (`BUY:HOUSE:A1`, `SELL:A2`, `BUY:LAND_B`, `RESET`) |
| `V1` | `City JSON` | String | — | — | `"{}"` | Compact JSON snapshot of slots, night state & income |
| `V2` | `Treasury` | Integer | `$` | `0` / `100000` | `500` | Current player money (`$`) |
| `V3` | `City AQI` | Double | `AQI` | `0` / `500` | `25` | Hybrid MQ-135 + Factory/Park AQI |
| `V4` | `Land A Temp` | Double | `°C` | `0` / `60` | `27` | Land A DHT11 + Urban Heat Island temperature |
| `V5` | `Land A Hum` | Double | `%` | `0` / `100` | `60` | Land A DHT11 relative humidity |
| `V6` | `Land B Temp` | Double | `°C` | `0` / `60` | `27` | Land B DHT11 + UHI temp (via ESP-MESH) |
| `V7` | `Land B Hum` | Double | `%` | `0` / `100` | `60` | Land B DHT11 humidity (via ESP-MESH) |
| `V8` | `Land A LDR` | Double | `%` | `0` / `100` | `80` | Land A measured ambient light (%) |
| `V9` | `Land B LDR` | Double | `%` | `0` / `100` | `80` | Land B measured ambient light (%) |
| `V10` | `Sun Override` | Integer | `%` | `-1` / `100` | `-1` | `-1` = Auto Day/Night cycle; `0..100` = Manual Sun slider |
| `V11` | `Alert Feed` | String | — | — | `""` | Latest permit, illegal build, or smog alert |
| `V12` | `Buy Land B` | Integer | — | `0` / `1` | `0` | Push button (1 = Unlock Land B for `$1,000`) |

### 2.3 Configure Events & Notifications
Under **Events & Notifications → + Add New Event**:
1. **`illegal_build`** (Type: *Warning*, Enable Notifications) — Triggered when ESP32-CAM detects a LEGO brick placed without a paid permit.
2. **`smog_alert`** (Type: *Critical*, Enable Notifications) — Triggered when `City AQI > 150`.
3. **`land_b_unlocked`** (Type: *Info*) — Triggered when `$1,000` is spent to unlock ESP32-2 over ESP-MESH.

### 2.4 Build the Web & Mobile Dashboard
Arrange widgets on the Blynk Web Dashboard and Mobile App:
- **Economy Header:** Value Display (`V2` Treasury `$`), Gauge (`V3` City AQI, green `<100`, yellow `100–150`, red `>150`).
- **Build Menu (Styled Buttons / Segmented Switch bound to `V0`):**
  - *Buy House ($100)* → sends `"BUY:HOUSE"`
  - *Buy Factory ($250)* → sends `"BUY:FACTORY"`
  - *Buy Park ($150)* → sends `"BUY:PARK"`
  - *Buy Solar ($300)* → sends `"BUY:SOLAR"`
  - *Sell Slot A1..B4* → Text Input widget on `V0` (e.g., `SELL:A2`) or dedicated buttons.
  - *Unlock Land B ($1,000)* → Push Button bound to `V12` (Press=`1`, Release=`0`).
- **Sun Control:** Slider (`V10`, `-1` to `100`) to manually scrub Dawn/Noon/Dusk/Night during demos.
- **Land A & Land B Telemetry Panels:** Value Displays / Charts for `V4–V9`.
- **City Event Feed:** Label / Terminal widget bound to `V11`.

### 2.5 Provision the Device
1. Go to **Search → Devices → + New Device → From Template → BlockCity RTOS**.
2. Copy the **Auth Token** (`BLYNK_AUTH_TOKEN`) into `include/secrets.h`.

---

## 3. Supabase Cloud Database Setup (PostgreSQL + PostgREST)

### 3.1 Create Project & Apply Schema
1. Sign in to [Supabase](https://supabase.com/) and click **New Project** (e.g., `blockcity-rtos`).
2. Open **SQL Editor → New Query**.
3. Copy the entire contents of `supabase/schema.sql` from this repository, paste it into the editor, and click **Run**.
4. Verify that two tables (`city_telemetry`, `building_events`) and one view (`latest_city_status`) appear under **Table Editor**.

### 3.2 Configure ESP32-1 Credentials
1. In Supabase, navigate to **Project Settings → API**.
2. Copy:
   - **Project URL** (e.g., `https://abcdefg.supabase.co`) → `SUPABASE_URL` in `include/secrets.h`
   - **`anon` `public` key** → `SUPABASE_ANON_KEY` in `include/secrets.h`

### 3.3 Test REST Ingestion from PowerShell
Before flashing ESP32-1, confirm your Supabase table and RLS policy accept `POST` writes:

```powershell
$headers = @{
    "apikey"        = "YOUR_SUPABASE_ANON_KEY"
    "Authorization" = "Bearer YOUR_SUPABASE_ANON_KEY"
    "Content-Type"  = "application/json"
    "Prefer"        = "return=representation"
}
$body = @{
    tick_count = 1
    money = 500
    city_aqi = 32.5
    sun_phase = 35
    land_a_temp = 27.4
    land_a_hum = 61.0
} | ConvertTo-Json

Invoke-RestMethod -Uri "https://YOUR-PROJECT.supabase.co/rest/v1/city_telemetry" -Method Post -Headers $headers -Body $body
```

### 3.4 Exporting Data for Final Report Graphs
In Supabase **Table Editor → `city_telemetry`**, click **Export → Export to CSV** to plot City AQI, Treasury Growth, and Land A vs. Land B temperatures for Chapter 3 of the Final Report.

---

## 4. Optional Node-RED Local Bridge

If you want to visualize live JSON packets locally or route ESP32-CAM HTTP events through Node-RED (Syllabus Module 9):
1. Point ESP32-CAM's `MASTER_HTTP_PORT` to a Node-RED `http in` node (`POST /api/vision`), log/display the payload on a Node-RED Dashboard, and forward it via `http request` to `http://<ESP32_1_IP>:8080/api/vision`.
2. Poll `http://<ESP32_1_IP>:8080/api/state` every 5 seconds in Node-RED to drive local charts alongside Blynk and Supabase.
