#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_INA219.h>
#include <BH1750.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//WIFI------------
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// Your Wi-Fi
const char* ssid = "CMF_Phone2";
const char* password = "NcmfPhone2";

// Telegram
#define BOT_TOKEN "8785585155:AAFZU9P7jul1-zfpX-q_W_zCXK24AQDqNiI"
#define CHAT_ID "5123901768"

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

void setup() {
  Serial.begin(115200);

  Serial.println("Connecting to WiFi...");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected!");

  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  // Allows ESP32 to connect to Telegram
  client.setInsecure();

  // Send test message
  bot.sendMessage(
    CHAT_ID,
    "Hello! ESP32 Telegram test is working.",
    ""
  );

  Serial.println("Telegram message sent!");
}

void loop() {
}
//WIFI---------------

// =====================================================
// OLED
// =====================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

// =====================================================
// INA219
// =====================================================

Adafruit_INA219 ina219;

// =====================================================
// BH1750
// =====================================================

BH1750 lightMeter;

// =====================================================
// DS18B20
// =====================================================

#define ONE_WIRE_BUS 4

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);

// =====================================================
// VIRTUAL BATTERY
// =====================================================

// Battery specification
const float BATTERY_VOLTAGE = 3.5;       // V
const float BATTERY_CAPACITY_MAH = 2000; // mAh

// Convert mAh to Ah
const float BATTERY_CAPACITY_AH =
  BATTERY_CAPACITY_MAH / 1000.0;

// Total battery energy
// 3.5 V × 2 Ah = 7 Wh
const float BATTERY_ENERGY_WH =
  BATTERY_VOLTAGE * BATTERY_CAPACITY_AH;

// =====================================================
// CHARGING MODEL
// =====================================================

// Assumed charging efficiency
const float BASE_EFFICIENCY = 0.80;

// Minimum solar power considered as charging
const float MIN_CHARGING_POWER = 0.10;

// =====================================================
// POWER AVERAGING
// =====================================================

#define POWER_SAMPLES 10

float powerSamples[POWER_SAMPLES];

int powerIndex = 0;

bool bufferFull = false;

// =====================================================
// SENSOR VARIABLES
// =====================================================

float solarVoltage = 0.0;
float solarCurrent = 0.0;
float solarPower = 0.0;

float averagePower = 0.0;

float lightLux = 0.0;
float temperature = 0.0;

// =====================================================
// FACTORS
// =====================================================

float lightFactor = 1.0;
float temperatureFactor = 1.0;

float effectivePower = 0.0;

// =====================================================
// BATTERY VARIABLES
// =====================================================

// Starts at 0 Wh
float storedEnergyWh = 0.0;

float remainingEnergyWh = 0.0;

float chargePercent = 0.0;

float fullTimeHours = -1.0;

// =====================================================
// TIME
// =====================================================

unsigned long lastEnergyUpdate = 0;

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  // I2C
  Wire.begin(21, 22);

  // ===================================================
  // OLED INITIALIZATION
  // ===================================================

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS
      )) {

    Serial.println("OLED FAILED");

    while (true);
  }

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);

  display.setCursor(25, 25);

  display.println("Starting...");

  display.display();

  delay(1500);

  // ===================================================
  // INA219
  // ===================================================

  if (!ina219.begin()) {

    Serial.println("INA219 FAILED");

    display.clearDisplay();

    display.setCursor(20, 25);

    display.println("INA219 ERROR");

    display.display();

    while (true);
  }

  // ===================================================
  // BH1750
  // ===================================================

  if (!lightMeter.begin(
        BH1750::CONTINUOUS_HIGH_RES_MODE
      )) {

    Serial.println("BH1750 FAILED");

    display.clearDisplay();

    display.setCursor(20, 25);

    display.println("BH1750 ERROR");

    display.display();

    while (true);
  }

  // ===================================================
  // DS18B20
  // ===================================================

  tempSensor.begin();

  // ===================================================
  // POWER BUFFER
  // ===================================================

  for (int i = 0; i < POWER_SAMPLES; i++) {

    powerSamples[i] = 0.0;
  }

  // ===================================================
  // INITIAL TIME
  // ===================================================

  lastEnergyUpdate = millis();

  Serial.println();
  Serial.println("==============================");
  Serial.println(" SOLAR CHARGE MONITOR");
  Serial.println("==============================");

  Serial.print("Battery Voltage : ");
  Serial.print(BATTERY_VOLTAGE);
  Serial.println(" V");

  Serial.print("Battery Capacity: ");
  Serial.print(BATTERY_CAPACITY_MAH);
  Serial.println(" mAh");

  Serial.print("Battery Energy  : ");
  Serial.print(BATTERY_ENERGY_WH);
  Serial.println(" Wh");

  Serial.println("Initial Charge  : 0%");
  Serial.println("==============================");
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  // ===================================================
  // 1. READ INA219
  // ===================================================

  solarVoltage = ina219.getBusVoltage_V();

  solarCurrent = ina219.getCurrent_mA();

  // Convert mA → A

  float currentAmp = solarCurrent / 1000.0;

  // Power = Voltage × Current

  solarPower = solarVoltage * currentAmp;

  // Prevent negative power

  if (solarPower < 0) {

    solarPower = 0;
  }

  // ===================================================
  // 2. READ BH1750
  // ===================================================

  lightLux = lightMeter.readLightLevel();

  if (lightLux < 0) {

    lightLux = 0;
  }

  // ===================================================
  // 3. READ DS18B20
  // ===================================================

  tempSensor.requestTemperatures();

  temperature = tempSensor.getTempCByIndex(0);

  if (temperature == DEVICE_DISCONNECTED_C) {

    temperature = 25.0;
  }

  // ===================================================
  // 4. STORE POWER SAMPLE
  // ===================================================

  powerSamples[powerIndex] = solarPower;

  powerIndex++;

  if (powerIndex >= POWER_SAMPLES) {

    powerIndex = 0;

    bufferFull = true;
  }

  // ===================================================
  // 5. CALCULATE AVERAGE POWER
  // ===================================================

  int numberOfSamples;

  if (bufferFull) {

    numberOfSamples = POWER_SAMPLES;

  } else {

    numberOfSamples = powerIndex;
  }

  float totalPower = 0.0;

  for (int i = 0; i < numberOfSamples; i++) {

    totalPower += powerSamples[i];
  }

  if (numberOfSamples > 0) {

    averagePower =
      totalPower / numberOfSamples;

  } else {

    averagePower = solarPower;
  }

  // ===================================================
  // 6. LIGHT FACTOR
  // ===================================================

  if (lightLux >= 10000) {

    lightFactor = 1.00;

  } else if (lightLux >= 5000) {

    lightFactor = 0.90;

  } else if (lightLux >= 2000) {

    lightFactor = 0.75;

  } else if (lightLux >= 500) {

    lightFactor = 0.50;

  } else {

    lightFactor = 0.20;
  }

  // ===================================================
  // 7. TEMPERATURE FACTOR
  // ===================================================

  if (temperature <= 35) {

    temperatureFactor = 1.00;

  } else if (temperature <= 40) {

    temperatureFactor = 0.95;

  } else if (temperature <= 45) {

    temperatureFactor = 0.90;

  } else {

    temperatureFactor = 0.80;
  }

  // ===================================================
  // 8. EFFECTIVE POWER
  // =====================================================

  effectivePower =
    averagePower *
    BASE_EFFICIENCY *
    lightFactor *
    temperatureFactor;

  // ===================================================
  // 9. CALCULATE ELAPSED TIME
  // ===================================================

  unsigned long currentMillis = millis();

  float deltaHours =
    (currentMillis - lastEnergyUpdate)
    / 3600000.0;

  lastEnergyUpdate = currentMillis;

  // ===================================================
  // 10. ADD ENERGY TO VIRTUAL BATTERY
  // ===================================================

  if (
    solarPower >= MIN_CHARGING_POWER &&
    storedEnergyWh < BATTERY_ENERGY_WH
  ) {

    float energyAdded =
      effectivePower * deltaHours;

    storedEnergyWh += energyAdded;

    // Prevent overcharging
    if (storedEnergyWh > BATTERY_ENERGY_WH) {

      storedEnergyWh = BATTERY_ENERGY_WH;
    }
  }

  // ===================================================
  // 11. CALCULATE CHARGE %
  // ===================================================

  chargePercent =
    (storedEnergyWh / BATTERY_ENERGY_WH)
    * 100.0;

  // Limit 0–100

  if (chargePercent > 100) {

    chargePercent = 100;
  }

  if (chargePercent < 0) {

    chargePercent = 0;
  }

  // ===================================================
  // 12. REMAINING ENERGY
  // ===================================================

  remainingEnergyWh =
    BATTERY_ENERGY_WH -
    storedEnergyWh;

  // ===================================================
  // 13. FULL IN CALCULATION
  // ===================================================

  if (chargePercent >= 100) {

    fullTimeHours = 0;

  } else if (
    effectivePower > 0.01 &&
    solarPower >= MIN_CHARGING_POWER
  ) {

    fullTimeHours =
      remainingEnergyWh /
      effectivePower;

  } else {

    fullTimeHours = -1;
  }

  // ===================================================
  // 14. SERIAL MONITOR
  // ===================================================

  Serial.println();
  Serial.println("----------- SENSOR DATA -----------");

  Serial.print("Solar Voltage : ");
  Serial.print(solarVoltage, 2);
  Serial.println(" V");

  Serial.print("Solar Current : ");
  Serial.print(solarCurrent, 1);
  Serial.println(" mA");

  Serial.print("Solar Power   : ");
  Serial.print(solarPower, 2);
  Serial.println(" W");

  Serial.print("Average Power : ");
  Serial.print(averagePower, 2);
  Serial.println(" W");

  Serial.print("Light         : ");
  Serial.print(lightLux, 0);
  Serial.println(" lux");

  Serial.print("Light Factor  : ");
  Serial.println(lightFactor, 2);

  Serial.print("Temperature   : ");
  Serial.print(temperature, 1);
  Serial.println(" C");

  Serial.print("Temp Factor   : ");
  Serial.println(temperatureFactor, 2);

  Serial.println();

  Serial.print("Effective Power: ");
  Serial.print(effectivePower, 3);
  Serial.println(" W");

  Serial.print("Stored Energy  : ");
  Serial.print(storedEnergyWh, 3);
  Serial.println(" Wh");

  Serial.print("Remaining Energy: ");
  Serial.print(remainingEnergyWh, 3);
  Serial.println(" Wh");

  Serial.print("Charge         : ");
  Serial.print(chargePercent, 1);
  Serial.println(" %");

  // ===================================================
  // STATUS
  // ===================================================

  if (chargePercent >= 100) {

    Serial.println("Status         : FULL");

  } else if (solarPower >= MIN_CHARGING_POWER) {

    Serial.println("Status         : CHARGING");

  } else {

    Serial.println("Status         : NOT CHARGING");
  }

  // ===================================================
  // FULL IN
  // ===================================================

  if (chargePercent >= 100) {

    Serial.println("Full In        : COMPLETE");

  } else if (fullTimeHours >= 0) {

    int totalMinutes =
      (int)(fullTimeHours * 60);

    int hours =
      totalMinutes / 60;

    int minutes =
      totalMinutes % 60;

    Serial.print("Full In        : ");

    Serial.print(hours);

    Serial.print("h ");

    Serial.print(minutes);

    Serial.println("m");

  } else {

    Serial.println("Full In        : --");
  }

  // ===================================================
  // OLED
  // ===================================================

  updateDisplay();

  // Update every 1 second

  delay(1000);
}


// =====================================================
// OLED DISPLAY FUNCTION
// =====================================================

void updateDisplay() {

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);

  // ===================================================
  // TITLE
  // ===================================================

  display.setCursor(15, 0);

  display.println("SOLAR CHARGE MONITOR");

  // ===================================================
  // POWER
  // ===================================================

  display.setCursor(0, 8);

  display.print("Power : ");

  display.print(solarPower, 2);

  display.println(" W");

  // ===================================================
  // LIGHT
  // ===================================================

  display.setCursor(0, 16);

  display.print("Light : ");

  display.print(lightLux, 0);

  display.println(" lux");

  // ===================================================
  // TEMPERATURE
  // ===================================================

  display.setCursor(0, 24);

  display.print("Temp  : ");

  display.print(temperature, 1);

  display.println(" C");

  // ===================================================
  // CHARGE %
  // ===================================================

  display.setCursor(0, 32);

  display.print("Charge: ");

  display.print(chargePercent, 0);

  display.println("%");

  // ===================================================
  // BATTERY BAR
  // ===================================================

  display.drawRect(
    0,
    40,
    124,
    7,
    SSD1306_WHITE
  );

  int barWidth =
    (int)(120.0 *
          chargePercent /
          100.0);

  if (barWidth > 0) {

    display.fillRect(
      2,
      42,
      barWidth,
      3,
      SSD1306_WHITE
    );
  }

  // ===================================================
  // FULL IN
  // ===================================================

  display.setCursor(0, 49);

  display.print("Full in: ");

  if (chargePercent >= 100) {

    display.println("COMPLETE");

  } else if (fullTimeHours >= 0) {

    int totalMinutes =
      (int)(fullTimeHours * 60);

    int hours =
      totalMinutes / 60;

    int minutes =
      totalMinutes % 60;

    if (hours < 10) {

      display.print("0");
    }

    display.print(hours);

    display.print("h ");

    if (minutes < 10) {

      display.print("0");
    }

    display.print(minutes);

    display.println("m");

  } else {

    display.println("--");
  }

  // ===================================================
  // STATUS
  // ===================================================

  display.setCursor(0, 57);

  display.print("Status: ");

  if (chargePercent >= 100) {

    display.println("FULL");

  } else if (solarPower >= MIN_CHARGING_POWER) {

    display.println("CHARGING");

  } else {

    display.println("NOT CHARGING");
  }

  display.display();
}
