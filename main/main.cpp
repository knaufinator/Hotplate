#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h" // Include ESP-IDF logging library
#include "lvgl.h"
 
#include "lvgl_helpers.h"
#include "ui.h"


#define SSR_PIN GPIO_NUM_19 // SSR control pin
#define MAX_ON_TIME 10000 // Maximum on-time in milliseconds (e.g., 10 seconds)
#define WAIT_TIME 1000    // Wait time between on-cycles (e.g., 1 second)
// Simulated PID Parameters (replace with real PID logic)
double setpoint = 75.0, input = 70.0, output = 0.0;
double kp = 2.0, ki = 5.0, kd = 1.0;
double integral = 0.0, derivative = 0.0, prev_error = 0.0;
TickType_t lastTime;


// Function Prototypes
void gpio_init(void);
void pid_control_task(void *pvParameter);
void lvgl_init(void);
double readTemperatureSensor(void);
void update_pid(void); 



extern "C" {
    void app_main();
}


static const char *TAG = "LED Example"; // Define a tag for logging

void configure_led() {

}


void lvgl_init(void) {
    lv_init();
    lvgl_driver_init();

    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf_1[LV_HOR_RES_MAX * 10];
    lv_disp_draw_buf_init(&draw_buf, buf_1, NULL, LV_HOR_RES_MAX * 10);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = ili9341_flush;
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 320;
    lv_disp_drv_register(&disp_drv);
}

void gpio_init(void) {
   
    gpio_set_direction(SSR_PIN, GPIO_MODE_OUTPUT);
    // Initialize more GPIOs as needed
}

void pid_control_task(void *pvParameter) {
    lastTime = xTaskGetTickCount();

    while (1) {
        update_pid(); // Update PID calculations

        // Simulate PID action
        if (output > 0.5) { // Dummy condition for SSR activation
            gpio_set_level(SSR_PIN, 1); // SSR ON
            vTaskDelay(pdMS_TO_TICKS(MAX_ON_TIME));
            gpio_set_level(SSR_PIN, 0); // SSR OFF
        }

        vTaskDelay(pdMS_TO_TICKS(WAIT_TIME));
    }
}

void app_main() {
   
 // Initialize LVGL
    lvgl_init();

   //  ui_init();

    // Initialize GPIO
    gpio_init();

    // Create PID control task
    xTaskCreate(pid_control_task, "pid_control_task", 4096, NULL, 5, NULL);
}

double readTemperatureSensor(void) {
    // Placeholder for actual temperature reading logic
    return 75.0; // Example: fixed temperature value, replace with sensor reading
}

void update_pid(void) {
    // Simple PID logic (for illustration purposes)
    input = readTemperatureSensor();
    double error = setpoint - input;
    integral += (error * WAIT_TIME);
    derivative = (error - prev_error) / WAIT_TIME;

    output = kp * error + ki * integral + kd * derivative;
    prev_error = error;

    // Constrain output to 0-1 range
    if (output > 1.0) output = 1.0;
    if (output < 0.0) output = 0.0;
}