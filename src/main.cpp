#include <Arduino_GFX_Library.h>
#include <Adafruit_MAX31865.h>

// TFT Display Pins
#define TFT_SCK    18
#define TFT_MOSI   23
#define TFT_MISO   19
#define TFT_CS     22
#define TFT_DC     21
#define TFT_RESET  17

// SSR Pin
#define SSR_PIN 15

// HSPI Pins for ESP32
#define HSPI_MISO 12
#define HSPI_MOSI 13
#define HSPI_SCK  14
#define HSPI_CS   27  // Example CS pin for MAX31865

// Global variables for display update
volatile float globalCurrentTemperature = 0.0;
volatile bool globalSSRState = false;

// Create an SPIClass object for HSPI
SPIClass hspi(HSPI);

// Initialize Adafruit_MAX31865 with the HSPI and CS pin
Adafruit_MAX31865 rtd(HSPI_CS, &hspi);

// Reference Resistor and Nominal Resistance
#define RREF      430.0
#define RNOMINAL  100.0


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

// Forward declaration of helper functions
void drawSquiggle(Arduino_ILI9341 &display, int x, int startY, int endY, uint16_t color);
void drawMixerIcon(Arduino_ILI9341 &display, int centerX, int centerY);
void displayText(Arduino_ILI9341 &display, const char* text, int x, int y, int textSize, uint16_t textColor, uint16_t valueColor, const char* value);
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
 
void updateDisplayTask(void *parameter) {
  for (;;) { // Task loop
    char tempStr[16];
    sprintf(tempStr, "%.2f°C", globalCurrentTemperature);
    display.fillRect(20, 80, 200, 30, WHITE); // Clear the previous value
    displayText(display, "Liquid Temp:", 20, 80, 2, BLACK, BLUE, tempStr);

    // Add more display updates here based on global variables
   drawGraph(display); // Draw the temperature graph
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
  }
}
void setup(void) {
  pinMode(SSR_PIN, OUTPUT);
  digitalWrite(SSR_PIN, LOW); // Ensure SSR is off at start

  // SPI and Display Initialization as before
  hspi.begin(HSPI_SCK, HSPI_MISO, HSPI_MOSI, HSPI_CS);
  rtd.begin(MAX31865_2WIRE); // Adjust as per your RTD type

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
  // Display Setup as before
}



void loop() {
  // Read the current temperature from the sensor
  float currentTemperature = rtd.temperature(RNOMINAL, RREF);
  globalCurrentTemperature = currentTemperature; // Update the global variable for display

  unsigned long currentMillis = millis();

  // Simple control logic to turn SSR on/off based on the temperature
  if (currentTemperature < targetTemperature - tolerance && !globalSSRState && (currentMillis - lastMillis >= ssrOffTime)) {
    digitalWrite(SSR_PIN, HIGH); // Turn on the SSR
    globalSSRState = true; // Update the global state
    lastMillis = currentMillis;
  } else if (globalSSRState && (currentMillis - lastMillis >= ssrOnTime)) {
    digitalWrite(SSR_PIN, LOW); // Turn off the SSR
    globalSSRState = false; // Update the global state
    lastMillis = currentMillis;

    // Adjust the SSR on-time dynamically based on how close we are to the target temperature
    if (targetTemperature - currentTemperature <= 2) { // If getting closer to the target
      ssrOnTime = max(minOnTime, ssrOnTime / 2); // Reduce on-time, but not less than minOnTime
    }
  }
 
  if (currentMillis - lastDataUpdate >= 6000) { // 6 seconds have passed
    actualTemps[dataIndex] = globalCurrentTemperature; // Assume this is updated elsewhere in your loop
    targetTemps[dataIndex] = targetTemperature; // Assuming targetTemperature is a global or constant

    dataIndex = (dataIndex + 1) % DATA_POINTS; // Move to the next index, wrap around if necessary
    lastDataUpdate = currentMillis; // Update the last data update timestamp
  }
  // Note: The actual PID control logic or more sophisticated control should replace the above if needed

  // No need to delay here as vTaskDelay is used in the display task for timing
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
