#ifndef HUB75_DISPLAY_H_
#define HUB75_DISPLAY_H_

#include "esp_heap_caps.h"

#include "val2pwm.h"
#include "i2s_parallel.h"

/*
This is example code to driver a p3(2121)64*32 -style RGB LED display. These types of displays do not have memory and
need to be refreshed continuously. The display has 2 RGB inputs, 4 inputs to select the active line, a pixel clock
input, a latch enable input and an output-enable input. The display can be seen as 2 64x16 displays consisting of the
upper half and the lower half of the display. Each half has a separate RGB pixel input, the rest of the inputs are
shared.

Each display half can only show one line of RGB pixels at a time: to do this, the RGB data for the line is input by
setting the RGB input pins to the desired value for the first pixel, giving the display a clock pulse, setting the RGB
input pins to the desired value for the second pixel, giving a clock pulse, etc. Do this 64 times to clock in an entire
row. The pixels will not be displayed yet: until the latch input is made high, the display will still send out the
previously clocked in line. Pulsing the latch input high will replace the displayed data with the data just clocked in.

The 4 line select inputs select where the currently active line is displayed: when provided with a binary number (0-15),
the latched pixel data will immediately appear on this line. Note: While clocking in data for a line, the *previous*
line is still displayed, and these lines should be set to the value to reflect the position the *previous* line is
supposed to be on.

Finally, the screen has an OE input, which is used to disable the LEDs when latching new data and changing the state of
the line select inputs: doing so hides any artifacts that appear at this time. The OE line is also used to dim the
display by only turning it on for a limited time every line.

All in all, an image can be displayed by 'scanning' the display, say, 100 times per second. The slowness of the human
eye hides the fact that only one line is showed at a time, and the display looks like every pixel is driven at the same
time.

Now, the RGB inputs for these types of displays are digital, meaning each red, green and blue subpixel can only be on or
off. This leads to a color palette of 8 pixels, not enough to display nice pictures. To get around this, we use binary
code modulation.

Binary code modulation is somewhat like PWM, but easier to implement in our case. First, we define the time we would
refresh the display without binary code modulation as the 'frame time'. For, say, a four-bit binary code modulation, the
frame time is divided into 15 ticks of equal length.

We also define 4 subframes (0 to 3), defining which LEDs are on and which LEDs are off during that subframe. (Subframes
are the same as a normal frame in non-binary-coded-modulation mode, but are showed faster.)  From our (non-monochrome)
input image, we take the (8-bit: bit 7 to bit 0) RGB pixel values. If the pixel values have bit 7 set, we turn the
corresponding LED on in subframe 3. If they have bit 6 set, we turn on the corresponding LED in subframe 2, if bit 5 is
set subframe 1, if bit 4 is set in subframe 0.

Now, in order to (on average within a frame) turn a LED on for the time specified in the pixel value in the input data,
we need to weigh the subframes. We have 15 pixels: if we show subframe 3 for 8 of them, subframe 2 for 4 of them,
subframe 1 for 2 of them and subframe 1 for 1 of them, this 'automatically' happens. (We also distribute the subframes
evenly over the ticks, which reduces flicker.)


In this code, we use the I2S peripheral in parallel mode to achieve this. Essentially, first we allocate memory for all
subframes. This memory contains a sequence of all the signals (2xRGB, line select, latch enable, output enable) that
need to be sent to the display for that subframe. Then we ask the I2S-parallel driver to set up a DMA chain so the
subframes are sent out in a sequence that satisfies the requirement that subframe x has to be sent out for (2^x) ticks.
Finally, we fill the subframes with image data.

We use a frontbuffer/backbuffer technique here to make sure the display is refreshed in one go and drawing artifacts do
not reach the display. In practice, for small displays this is not really necessarily.

Finally, the binary code modulated intensity of a LED does not correspond to the intensity as seen by human eyes. To
correct for that, a luminance correction is used. See val2pwm.c for more info.

Note: Because every subframe contains one bit of grayscale information, they are also referred to as 'bitplanes' by the
code below.
*/

// My display has each row swapped with its neighbour (so the rows are 2-1-4-3-6-5-8-7-...). If your display
// is more sane, uncomment this to get a good image.
#define DISPLAY_ROWS_SWAPPED 1

// This is the bit depth, per RGB subpixel, of the data that is sent to the display.
// The effective bit depth (in computer pixel terms) is less because of the PWM correction. With
// a bitplane count of 7, you should be able to reproduce an 16-bit image more or less faithfully, though.
#define BITPLANE_CNT 7

#define DISPLAY_HEIGHT    (32)
#define DISPLAY_WIDTH     (64)
#define DISPLAY_ONE_FRAME (DISPLAY_HEIGHT * DISPLAY_WIDTH * 3)
// 64*32 RGB leds, 2 pixels per 16-bit value...
#define BITPLANE_SZ (64 * 32 / 2)

// Upper half RGB
#define BIT_R1 (1 << 0)  // connected to GPIO5 here
#define BIT_G1 (1 << 1)  // connected to GPIO18 here
#define BIT_B1 (1 << 2)  // connected to GPIO23 here
// Lower half RGB
#define BIT_R2 (1 << 3)  // connected to GPIO22 here
#define BIT_G2 (1 << 4)  // connected to GPIO19 here
#define BIT_B2 (1 << 5)  // connected to GPIO21 here

#define BIT_A   (1 << 8)   // connected to GPIO0 here
#define BIT_B   (1 << 9)   // connected to GPIO14 here
#define BIT_C   (1 << 10)  // connected to GPIO12 here
#define BIT_D   (1 << 11)  // connected to GPIO13 here
#define BIT_LAT (1 << 12)  // connected to GPIO2 here
#define BIT_OE  (1 << 13)  // connected to GPIO4 here

// 根据自己的开发板来调整IO
#define HUB75_R1_GPIO (23)
#define HUB75_G1_GPIO (18)
#define HUB75_B1_GPIO (5)
#define HUB75_R2_GPIO (21)
#define HUB75_G2_GPIO (19)
#define HUB75_B2_GPIO (22)

#define HUB75_A_GPIO (0)   // GPIO0
#define HUB75_B_GPIO (14)  // GPIO14
#define HUB75_C_GPIO (12)  // GPIO12
#define HUB75_D_GPIO (13)  // GPIO13

#define HUB75_CLK_GPIO (15)
#define HUB75_STB_GPIO (2)
#define HUB75_CE_GPIO  (4)

typedef enum {
    eDISPLAY_BUFFER_ID_A = 0,
    eDISPLAY_BUFFER_ID_B = 1,
    eDISPLAY_BUFFER_ID_NONE,
} eDISPLAY_BUFFER_ID;

int hub75_display_init(void);
int hub75_display_draw_frame(uint8_t *frame);

#endif
