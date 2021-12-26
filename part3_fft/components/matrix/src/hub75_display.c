#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "string.h"

#include "hub75_display.h"
#include "esp_log.h"

static const char       *TAG                        = "hub75-display";
static TaskHandle_t      xHandle_hub75_display_task = NULL;
static SemaphoreHandle_t xSem_display_one_frame     = NULL;
static int sg_backbuf_id = 0;  // which buffer is the backbuffer, as in, which one is not active so we can write to it
static unsigned char sg_display_buffer[2][DISPLAY_HEIGHT * DISPLAY_WIDTH * 3];

// Get a pixel from the image at pix, assuming the image is a 64x32 8R8G8B image
// Returns it as an uint32 with the lower 24 bits containing the RGB values.
static uint32_t _get_pixel(const unsigned char *pix, int x, int y) {
    const unsigned char *p = pix + ((x + y * DISPLAY_WIDTH) * 3);
    return (p[0] << 16) | (p[1] << 8) | (p[2]);
}

static void _display_task_entry(void *param) {
    ESP_LOGI(TAG, "_display_task_entry....");
    int brightness = 16;  // Change to set the global brightness of the display, range 2-63
                          // Warning when set too high: Do not look into LEDs with remaining eye.

    i2s_parallel_buffer_desc_t bufdesc[2][1 << BITPLANE_CNT];
    i2s_parallel_config_t      cfg = {
             .gpio_bus = {HUB75_R1_GPIO, HUB75_G1_GPIO, HUB75_B1_GPIO, HUB75_R2_GPIO, HUB75_G2_GPIO, HUB75_B2_GPIO, -1, -1,
                     HUB75_A_GPIO, HUB75_B_GPIO, HUB75_C_GPIO, HUB75_D_GPIO, HUB75_STB_GPIO, HUB75_CE_GPIO, -1, -1},
             .gpio_clk    = HUB75_CLK_GPIO,
             .bits        = I2S_PARALLEL_BITS_16,
             .clkspeed_hz = 20 * 1000 * 1000,
             .bufa        = bufdesc[0],
             .bufb        = bufdesc[1],
    };

    uint16_t *bitplane[2][BITPLANE_CNT];

    for (int i = 0; i < BITPLANE_CNT; i++) {
        for (int j = 0; j < 2; j++) {
            bitplane[j][i] = heap_caps_malloc(BITPLANE_SZ * 2, MALLOC_CAP_DMA);
            assert(bitplane[j][i] && "Can't allocate bitplane memory");
        }
    }

    // Do binary time division setup. Essentially, we need n of plane 0, 2n of plane 1, 4n of plane 2 etc, but that
    // needs to be divided evenly over time to stop flicker from happening. This little bit of code tries to do that
    // more-or-less elegantly.
    int times[BITPLANE_CNT] = {0};

    for (int i = 0; i < ((1 << BITPLANE_CNT) - 1); i++) {
        int ch = 0;
        // Find plane that needs insertion the most
        for (int j = 0; j < BITPLANE_CNT; j++) {
            if (times[j] <= times[ch])
                ch = j;
        }
        // Insert the plane
        for (int j = 0; j < 2; j++) {
            bufdesc[j][i].memory = bitplane[j][ch];
            bufdesc[j][i].size   = BITPLANE_SZ * 2;
        }
        // Magic to make sure we choose this bitplane an appropriate time later next time
        times[ch] += (1 << (BITPLANE_CNT - ch));
    }

    // End markers
    bufdesc[0][((1 << BITPLANE_CNT) - 1)].memory = NULL;
    bufdesc[1][((1 << BITPLANE_CNT) - 1)].memory = NULL;

    // Setup I2S
    i2s_parallel_setup(&I2S1, &cfg);

    while (1) {
        // Fill bitplanes with the data for the current image
        // const uint8_t *pix = &anim[apos * DISPLAY_WIDTH * DISPLAY_HEIGHT * 3];  // pixel data for this animation
        // frame
        xSemaphoreTake(xSem_display_one_frame, portMAX_DELAY);
        uint8_t *pix = sg_display_buffer[sg_backbuf_id];
        for (int pl = 0; pl < BITPLANE_CNT; pl++) {
            int       mask = (1 << (8 - BITPLANE_CNT + pl));  // bitmask for pixel data in input for this bitplane
            uint16_t *p    = bitplane[sg_backbuf_id][pl];     // bitplane location to write to
            for (unsigned int y = 0; y < 16; y++) {
                int lbits = 0;  // 当前要显示的行
                if ((y - 1) & 1)
                    lbits |= BIT_A;
                if ((y - 1) & 2)
                    lbits |= BIT_B;
                if ((y - 1) & 4)
                    lbits |= BIT_C;
                if ((y - 1) & 8)
                    lbits |= BIT_D;
                for (int fx = 0; fx < DISPLAY_WIDTH; fx++) {
#if DISPLAY_ROWS_SWAPPED
                    int x =
                        fx ^ 1;  // to correct for the fact that the stupid LED screen I have has each row swapped...
#else
                    int x = fx;
#endif

                    int v = lbits;
                    // Do not show image while the line bits are changing
                    if (fx < 1 || fx >= brightness)
                        v |= BIT_OE;
                    if (fx == 62)
                        v |= BIT_LAT;  // latch on second-to-last bit... why not last bit? Dunno, probably a timing
                                       // thing.

                    int c1, c2;
                    c1 = _get_pixel(pix, x, y);
                    c2 = _get_pixel(pix, x, y + 16);
                    if (c1 & (mask << 16))
                        v |= BIT_R1;
                    if (c1 & (mask << 8))
                        v |= BIT_G1;
                    if (c1 & (mask << 0))
                        v |= BIT_B1;
                    if (c2 & (mask << 16))
                        v |= BIT_R2;
                    if (c2 & (mask << 8))
                        v |= BIT_G2;
                    if (c2 & (mask << 0))
                        v |= BIT_B2;
                    // Save the calculated value to the bitplane memory
                    *p++ = v;
                }
            }
        }

        // Show our work!
        i2s_parallel_flip_to_buffer(&I2S1, sg_backbuf_id);
        sg_backbuf_id ^= 1;  // change to free buf id
    }
}

int hub75_display_draw_pixel(uint8_t x, uint8_t y, uint32_t color) {
    return 0;
}

int hub75_display_draw_frame(uint8_t *frame) {
    if (frame == NULL) {
        ESP_LOGE(TAG, "frame == null");
        return -1;
    } else {
        memcpy(sg_display_buffer[sg_backbuf_id], frame, DISPLAY_ONE_FRAME);
        xSemaphoreGive(xSem_display_one_frame);
        return 0;
    }
}

int hub75_display_get_free_buffer_id(void) {
    return sg_backbuf_id;
}

int hub75_display_init(void) {
    xTaskCreatePinnedToCore(_display_task_entry,          //任务函数
                            "hub75_display",              //任务名称
                            4096,                         //堆栈大小
                            NULL,                         //传递参数
                            12,                           //任务优先级
                            &xHandle_hub75_display_task,  //任务句柄
                            tskNO_AFFINITY);              //无关联，不绑定在任何一个核上
    xSem_display_one_frame = xSemaphoreCreateCounting(10, 0);
    if (xHandle_hub75_display_task && xSem_display_one_frame) {
        ESP_LOGI(TAG, "hub75 display init success.");
        return 0;
    }
    return -1;
}
