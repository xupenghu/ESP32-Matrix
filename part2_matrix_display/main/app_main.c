/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "hub75_display.h"
#include "anim.h"
#include "image.h"

void app_main() {
    hub75_display_init();
    int apos = 0;
    while (1) {
        const uint8_t *pix = &anim[apos * DISPLAY_ONE_FRAME];  // pixel data for this animation frame  gImage_image;  //
        hub75_display_draw_frame(pix);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        // show next frame of Nyancat animation
        apos++;
        if (apos >= 12)
            apos = 0;
    }
}
