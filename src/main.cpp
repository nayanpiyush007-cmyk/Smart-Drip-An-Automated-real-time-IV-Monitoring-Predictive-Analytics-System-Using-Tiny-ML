#define BLYNK_TEMPLATE_ID   "TMPL3pP73jjgq"
#define BLYNK_TEMPLATE_NAME "SmartDripVein"
#define BLYNK_AUTH_TOKEN    "cEVTGSb_ZeujispK-KbPaxOykdJlFVmt"

#include <Arduino.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HX711.h>
#include "model_code.h"

// ─── TINYML MODEL PLACEHOLDER ──────────────────────────────────

// ───────────────────────────────────────────────────────────────

#define DOUT      4
#define CLK       5
#define BUZZER    15
#define IR_SENSOR 2

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

HX711 scale;

char auth[] = "cEVTGSb_ZeujispK-KbPaxOykdJlFVmt";
char ssid[] = "byebye";
char pass[] = "Bholu@007";

float weight         = 0;
float startWeight    = 0;
unsigned long startTime = 0;
int   mlRemaining    = 0;
int   percentLeft    = 0;
float calibration = 442084;

volatile int dropCount              = 0;
volatile unsigned long lastDropTime = 0;
volatile unsigned long currentDropInterval = 0;
float dropsPerMinute                = 0;

bool alert50sent       = false;
bool alert20sent       = false;
bool blockageAlertSent = false;
bool fastAlertSent     = false;
bool slowAlertSent     = false;

unsigned long previousMillis = 0;
const long interval          = 1000;

void beep(int times) {
    for (int i = 0; i < times; i++) {
        digitalWrite(BUZZER, HIGH);
        delay(100);
        digitalWrite(BUZZER, LOW);
        delay(100);
    }
}

void IRAM_ATTR countDrop() {
    unsigned long now      = millis();
    unsigned long timeDiff = now - lastDropTime;
    if (timeDiff > 50) {
        dropCount++;
        currentDropInterval = timeDiff;
        lastDropTime        = now;
    }
}

void updateDisplay(float mins) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(10, 0);
    display.print("--- Smart-Drip ---");

    display.setCursor(0, 16);
    display.print("Vol: ");
    display.print(mlRemaining);
    display.print(" mL  ");
    display.print(percentLeft);
    display.print("%");

    display.setCursor(0, 30);
    display.print("Rate: ");
    display.print(dropsPerMinute, 1);
    display.print(" dpm");

    display.setCursor(0, 44);
    if (mins >= 999) {
        display.print("Calculating...");
    } else {
        display.print("Empty in: ");
        display.print((int)mins);
        display.print(" min");
    }

    display.setCursor(0, 56);
    display.print("Drops: ");
    display.print(dropCount);

    display.display();
}

void measureAndPredict() {
    if (millis() - lastDropTime > 3000) {
        dropsPerMinute = 0;
    } else if (currentDropInterval > 0) {
        float newRate = 60000.0 / currentDropInterval;
        dropsPerMinute = (dropsPerMinute == 0) ? newRate
                       : (dropsPerMinute * 0.8) + (newRate * 0.2);
    }

    scale.set_scale(calibration);
    weight = scale.get_units(5);
    if (weight < 0) weight = 0;

    mlRemaining = (int)(weight * 1000);
    percentLeft = map(mlRemaining, 0, 505, 0, 100);
    if (percentLeft > 100) percentLeft = 100;
    if (percentLeft < 0)   percentLeft = 0;

    // ─── TINYML FEATURE ENGINEERING ──────────────────────────────
    float elapsedMin = (millis() - startTime) / 60000.0;
    float weightLost = startWeight - weight;
    float weightLossRate = (elapsedMin > 0) ? (weightLost / elapsedMin) : 0;
    float linearETA = (weightLossRate > 0) ? (weight / weightLossRate) : 999;
    
    // Smooth the drip rate specifically for the AI model
    static float smoothedDripRate = 0;
    smoothedDripRate = (smoothedDripRate * 0.8) + (dropsPerMinute * 0.2);

    // Package the 6 features exactly as the research document specified
    double model_features[6] = {
        (double)smoothedDripRate,
        (double)weight,
        (double)weightLost,
        (double)elapsedMin,
        (double)weightLossRate,
        (double)linearETA
    };

    // ─── TINYML INFERENCE EXECUTION ──────────────────────────────
    float minsLeft;
    if (elapsedMin < 0.5 || weightLost <= 0.02) {
        minsLeft = 999; // Wait for enough data to gather before predicting
    } else {
        // Run the AI model!
        minsLeft = (float)score(model_features); 
    }
    // ─────────────────────────────────────────────────────────────

    // Serial debug
    Serial.print("Weight: "); Serial.print(weight); Serial.println(" kg");
    Serial.print("Vol: ");    Serial.print(mlRemaining); Serial.println(" mL");
    Serial.print("Level: ");  Serial.print(percentLeft); Serial.println("%");
    Serial.print("Rate: ");   Serial.print(dropsPerMinute); Serial.println(" dpm");
    Serial.print("ETA: ");    Serial.print(minsLeft); Serial.println(" min");
    Serial.println("---");

    updateDisplay(minsLeft);

    Blynk.virtualWrite(V0, mlRemaining);
    Blynk.virtualWrite(V1, percentLeft);
    Blynk.virtualWrite(V2, dropsPerMinute);
    Blynk.virtualWrite(V3, (minsLeft >= 999) ? 0 : (int)minsLeft);

    // Anomaly alerts
    if (dropsPerMinute < 5 && percentLeft > 10 && !blockageAlertSent) {
        Blynk.logEvent("iv_alert", "Possible blockage detected!");
        beep(3); blockageAlertSent = true;
    } else if (dropsPerMinute >= 5) blockageAlertSent = false;

    if (dropsPerMinute > 80 && !fastAlertSent) {
        Blynk.logEvent("iv_alert", "Drip rate too fast!");
        beep(3); fastAlertSent = true;
    } else if (dropsPerMinute <= 80) fastAlertSent = false;

    if (dropsPerMinute < 10 && dropsPerMinute >= 5
        && percentLeft > 20 && !slowAlertSent) {
        Blynk.logEvent("iv_alert", "Drip rate too slow!");
        beep(2); slowAlertSent = true;
    } else if (dropsPerMinute >= 10) slowAlertSent = false;

    // Volume alerts
    if (percentLeft <= 50 && percentLeft >= 40 && !alert50sent) {
        Blynk.logEvent("iv_alert", "IV Bag is at 50%");
        beep(1); alert50sent = true;
    }
    if (percentLeft <= 20 && !alert20sent) {
        Blynk.logEvent("iv_alert", "IV Bag critically LOW");
        digitalWrite(BUZZER, HIGH);
        delay(3000);                 // Wait for 3 seconds
        digitalWrite(BUZZER, LOW);   // Turn it off!
        alert20sent = true;
    }
    if (percentLeft > 50) {
        alert50sent = false;
        alert20sent = false;
        digitalWrite(BUZZER, LOW);
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n--- BOOTING SMART-DRIP ---");

    // OLED
    Wire.begin(8, 9);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("ERROR: OLED not found!");
        while (true);
    }
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 28);
    display.print("Smart-Drip Init...");
    display.display();
    Serial.println("OLED OK");

    // HX711
    scale.begin(DOUT, CLK);
    scale.set_scale();
    scale.tare();
    Serial.println("Scale OK");

    // IR
    pinMode(IR_SENSOR, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(IR_SENSOR), countDrop, FALLING);
    Serial.println("IR OK");

    // Buzzer
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, LOW);

    // Blynk
    Serial.println("Connecting to Blynk...");
    Blynk.begin(auth, ssid, pass);
    Serial.println("Blynk OK");

    // Start weight
    startTime = millis();
    delay(1000);
    scale.set_scale(calibration);
    startWeight = scale.get_units(5);
    if (startWeight < 0) startWeight = 0;
    Serial.print("Start weight: ");
    Serial.println(startWeight);
    Serial.println("--- SETUP COMPLETE ---");
}

void loop() {
    Blynk.run();
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        measureAndPredict();
    }
}