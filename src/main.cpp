#include <Arduino_GFX_Library.h>
#include <SPI.h>
#include <Adafruit_MAX31865.h>
#include <Adafruit_MAX31855.h>

// TFT Display Pins
#define TFT_SCK    18
#define TFT_MOSI   23
#define TFT_MISO   19
#define TFT_CS     22
#define TFT_DC     21
#define TFT_RESET  17

// HSPI Pins for ESP32
#define HSPI_MISO 12
#define HSPI_MOSI 13
#define HSPI_SCK  14
#define MAX31865_CS 27  // Chip Select pin for the MAX31865
#define MAX31855_CS 26  // Chip Select pin for the MAX31855

// Reference Resistor and Nominal Resistance for MAX31865
#define RREF      430.0
#define RNOMINAL  100.0

// SSR Pin
#define SSR_PIN 15


struct TemperatureData {
  float liquidTemp = 0.0;
  float plateTemp = 0.0;
};

// Declare a global instance of TemperatureData
volatile TemperatureData tempData;


#define DATA_POINTS 100 // Number of data points for 10 minutes
volatile float actualTemps[DATA_POINTS]; // Store actual temperatures
volatile float targetTemps[DATA_POINTS]; // Store target temperatures
volatile int dataIndex = 0; // Index for the next data point to update
volatile unsigned long lastDataUpdate = 0; // Last data update timestamp

// Target Temperature (°C)
float targetTemperature = 30.0; // Example target temperature
float tolerance = 0.05; // Tolerance for target temperature

// PID Control Parameters (Placeholder for simplicity)
unsigned long lastMillis = 0;
bool ssrState = false;
unsigned long ssrOnTime = 10000; // SSR on time in milliseconds
unsigned long ssrOffTime = 1000; // SSR off time in milliseconds
unsigned long maxOnTime = 10000; // Maximum SSR on time
unsigned long minOnTime = 100; // Minimum SSR on time as we approach target

// Display Object Initialization
Arduino_ESP32SPI bus(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
Arduino_ILI9341 display(&bus, TFT_RESET);


// Initialize SPIClass object for HSPI with custom pins
SPIClass hspi(HSPI);

// Initialize Adafruit_MAX31865 with HSPI and CS pin
Adafruit_MAX31865 max31865 = Adafruit_MAX31865(MAX31865_CS, &hspi);

// Initialize Adafruit_MAX31855 for the K-type thermocouple
Adafruit_MAX31855 max31855(MAX31855_CS, &hspi);



// Function Declarations
void readAndPrintMAX31865Temperature();
void handleMAX31865Faults(uint8_t fault);
void readAndPrintMAX31855Temperature();
void handleMAX31855Faults(uint8_t fault);

void drawSquiggle(Arduino_ILI9341 &display, int x, int startY, int endY, uint16_t color);
void drawMixerIcon(Arduino_ILI9341 &display, int centerX, int centerY);
void displayText(Arduino_ILI9341 &display, const char* text, int x, int y, int textSize, uint16_t textColor, uint16_t valueColor, const char* value);
void drawGraph(Arduino_ILI9341 &display);
void updateDisplayTask(void *parameter);

void setup() {
  Serial.begin(9600);
  
  // Initialize HSPI with custom pins
  hspi.begin(HSPI_SCK, HSPI_MISO, HSPI_MOSI); // No need for HSPI_CS here
  
  // Initialize the MAX31865 with the custom HSPI
  max31865.begin(MAX31865_2WIRE); // Adjust for 2WIRE or 4WIRE as needed
  

  pinMode(SSR_PIN, OUTPUT);
  digitalWrite(SSR_PIN, LOW); // Ensure SSR is off at start
 
  display.begin();
  display.setRotation(1); // Set display to landscape mode
  display.fillScreen(WHITE);
 
  // Create a task that will update the display
  xTaskCreate(
    updateDisplayTask,   /* Task function */
    "UpdateDisplay",     /* Name of the task */
    2048,                /* Stack size in words */
    NULL,                /* Task input parameter */
    1,                   /* Priority of the task */
    NULL);               /* Task handle */
 
  Serial.println("Setup complete.");
}
void updateDisplayTask(void *parameter) {
  for (;;) { // Task loop
    char liquidTempStr[16];
    char plateTempStr[16];
    
    // Assuming tempData is your global struct holding the latest temperature values
    sprintf(liquidTempStr, "%.2f°C", tempData.liquidTemp);
    sprintf(plateTempStr, "%.2f°C", tempData.plateTemp);
    
    // Display Liquid Temperature
    // Increase the width from 220 to 240 or more to ensure complete clearing
    display.fillRect(20, 80, 240, 30, WHITE); // Clear the previous value with more width
    displayText(display, "Liquid Temp:", 20, 80, 2, BLACK, BLUE, liquidTempStr);
    
    // Display Plate Temperature
    // Increase the width from 220 to 240 or more to ensure complete clearing
    display.fillRect(20, 50, 240, 30, WHITE); // Clear the previous value with more width
    displayText(display, "Plate Temp:", 20, 50, 2, BLACK, BLUE, plateTempStr);

    // Add more display updates here based on global variables
    drawGraph(display); // Draw the temperature graph
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
  }
}


 void drawGraph(Arduino_ILI9341 &display) {
  int graphHeight = 50; // Height of the graph in pixels
  int graphWidth = 300; // Width of the graph in pixels
  int graphX = 10; // X position of the graph
  int graphY = 190; // Y position of the graph (assuming a 240px height display)

  // Clear the graph area
  display.fillRect(graphX, graphY, graphWidth, graphHeight, WHITE);

  // Find min and max temperature for scaling
  float minTemp = targetTemperature - 5; // Example: target temp with some margin
  float maxTemp = targetTemperature + 5; // Adjust based on your expected temp range

  // Draw the actual temperature graph
  for (int i = 0; i < DATA_POINTS - 1; i++) {
    int x1 = graphX + (i * graphWidth / DATA_POINTS);
    int y1 = graphY + graphHeight - (int)((actualTemps[(dataIndex + i) % DATA_POINTS] - minTemp) * graphHeight / (maxTemp - minTemp));
    int x2 = graphX + ((i + 1) * graphWidth / DATA_POINTS);
    int y2 = graphY + graphHeight - (int)((actualTemps[(dataIndex + i + 1) % DATA_POINTS] - minTemp) * graphHeight / (maxTemp - minTemp));
    display.drawLine(x1, y1, x2, y2, BLUE);
  }

  // Draw the target temperature graph
  for (int i = 0; i < DATA_POINTS - 1; i++) {
    int x1 = graphX + (i * graphWidth / DATA_POINTS);
    int y1 = graphY + graphHeight - (int)((targetTemps[(dataIndex + i) % DATA_POINTS] - minTemp) * graphHeight / (maxTemp - minTemp));
    int x2 = graphX + ((i + 1) * graphWidth / DATA_POINTS);
    int y2 = graphY + graphHeight - (int)((targetTemps[(dataIndex + i + 1) % DATA_POINTS] - minTemp) * graphHeight / (maxTemp - minTemp));
    display.drawLine(x1, y1, x2, y2, RED);
  }
}
 
void loop() {
  readAndPrintMAX31865Temperature();
  readAndPrintMAX31855Temperature();
 
}
void readAndPrintMAX31865Temperature() {
  float temperature = max31865.temperature(RNOMINAL, RREF);
  tempData.liquidTemp = temperature; // Update global struct

  uint8_t fault = max31865.readFault();
  if (fault) {
    handleMAX31865Faults(fault);
    max31865.clearFault();
  }
}

void readAndPrintMAX31855Temperature() {
  double celsius = max31855.readCelsius();
  if (isnan(celsius)) {
    Serial.println("MAX31855 reading error. Checking faults...");
    handleMAX31855Faults(max31855.readError());
  } else {
    tempData.plateTemp = celsius; // Update global struct
  }
}


// Helper function to draw squiggles for the hotplate indicator
void drawSquiggle(Arduino_ILI9341 &display, int x, int startY, int endY, uint16_t color) {
  for (int y = startY; y < endY; y += 4) {
    display.drawLine(x - 2, y, x + 2, y + 2, color);
    display.drawLine(x + 2, y + 2, x - 2, y + 4, color);
  }
}

// Helper function to draw the mixer status icon
void drawMixerIcon(Arduino_ILI9341 &display, int centerX, int centerY) {
  // Draw a simple propeller-like icon
  display.fillTriangle(centerX - 10, centerY, centerX, centerY - 15, centerX + 10, centerY, GREEN);
  display.fillTriangle(centerX, centerY - 10, centerX + 15, centerY, centerX, centerY + 10, GREEN);
  display.fillTriangle(centerX + 10, centerY, centerX, centerY + 15, centerX - 10, centerY, GREEN);
  display.fillTriangle(centerX, centerY + 10, centerX - 15, centerY, centerX, centerY - 10, GREEN);
}

// Helper function to display text and value placeholders
void displayText(Arduino_ILI9341 &display, const char* text, int x, int y, int textSize, uint16_t textColor, uint16_t valueColor, const char* value) {
  display.setCursor(x, y);
  display.setTextSize(textSize);
  display.setTextColor(textColor);
  display.print(text);
  
  int valueX = x + 160; // Adjust based on the length of your text
  display.setCursor(valueX, y);
  display.setTextColor(valueColor);
  display.print(value);
}

void handleMAX31865Faults(uint8_t fault) {
  Serial.println("MAX31865 Fault Detected:");
  if (fault & MAX31865_FAULT_HIGHTHRESH) {
    Serial.println("- RTD High Threshold");
  }
  if (fault & MAX31865_FAULT_LOWTHRESH) {
    Serial.println("- RTD Low Threshold");
  }
  if (fault & MAX31865_FAULT_REFINLOW) {
    Serial.println("- REFIN- > 0.85 x Bias");
  }
  if (fault & MAX31865_FAULT_REFINHIGH) {
    Serial.println("- REFIN- < 0.85 x Bias - FORCE- open");
  }
  if (fault & MAX31865_FAULT_RTDINLOW) {
    Serial.println("- RTDIN- < 0.85 x Bias - FORCE- open");
  }
  if (fault & MAX31865_FAULT_OVUV) {
    Serial.println("- Under/Over voltage");
  }
}

 
void handleMAX31855Faults(uint8_t fault) {
  if (fault) {
    if (fault & MAX31855_FAULT_OPEN) {
      Serial.println("- Thermocouple is open (no connections)");
    }
    if (fault & MAX31855_FAULT_SHORT_GND) {
      Serial.println("- Thermocouple is short-circuited to GND");
    }
    if (fault & MAX31855_FAULT_SHORT_VCC) {
      Serial.println("- Thermocouple is short-circuited to VCC");
    }
  } else {
    Serial.println("- No specific faults detected.");
  }
}
