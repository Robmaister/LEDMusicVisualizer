/*
 * Copyright (c) 2013 Robert Rouhani
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "fix_fft.h"
#include "HSBColor.h"
#include "LPD8806.h"
#include "SPI.h"

// Chose 2 pins for output; can be any valid output pins:
#define DATA_PIN 2
#define CLOCK_PIN 3

// Defines the number and arrangement of LEDs in the visualizer
#define NUM_BARS 8
#define BAR_LENGTH 8

// The difference in hue for each bar after the first.
#define BAR_HUE_DIFF 8

// Create a LPD8806 instance to control the strip
LPD8806 strip = LPD8806(NUM_BARS * BAR_LENGTH, DATA_PIN, CLOCK_PIN);

// The current hue of the first strip of the bar
int curHue = 0;

// FFT data storage
char im[NUM_BARS * 2], data[NUM_BARS * 2];
int prev[NUM_BARS];

// HSB/RGB data buffer
int rColor[3];

// Converts a 2d visualizer point to it's location on the strip
int getStripLocation(int col, int row)
{
  // The strips are in alternating directions. This adjusts the result for that.
  if (col % 2 == 0)
    row = BAR_LENGTH - row - 1;
    
  return col * BAR_LENGTH + row;
}

void setup()
{
  analogReference(DEFAULT);
  strip.begin();
  strip.show();
}

void loop()
{
  uint16_t i, j, k;
  uint32_t color;
  
  // Read analog input
  for (i = 0; i < NUM_BARS * 2; i++)
  {
    int val = (analogRead(3) + analogRead(2)) / 2;
    data[i] = val * 2;
    im[i] = 0;
    
    delay(1);
  }
  
  // Perform FFT on data
  // HACK 4 represents 2^4, or 16 (NUM_BARS * 2). Change this to adjust with NUM_BARS!
  int shift = fix_fft(data, im, 4, 0);

  // Clear pixels
  for (i = 0; i < NUM_BARS * BAR_LENGTH; i++)
    strip.setPixelColor(i, 0);
  
  // Set the proper pixels in each bar
  for (i = 0; i < NUM_BARS; i++)
  {
    // Each LED bar has 2 FFT frequencies that are summed together
    int fft_start = i * 2;
    int fft_count = 2;
    
    // Get a positive data point from the FFT
    int curData = 0;
    for (k = 0; k < fft_count; k++)
      curData += sqrt(data[fft_start + k] * data[fft_start + k] + im[fft_start + k] * im[fft_start + k]);
      
    // Account for the ShiftyVU's filtering
    if (i == 0 || i == 7)
      curData /= 2;
    
    // Smoothly drop from peaks by only allowing data points to be one LED lower than the previous iteration.
    // This prevents seizure-inducing flashes which might be caused by the ShiftyVU's filtering (?)
    if (prev[i] > BAR_LENGTH && curData < prev[i] - BAR_LENGTH)
      curData = prev[i] - BAR_LENGTH;
    
    // Base color for each bar
    H2R_HSBtoRGB((curHue + i * 8) % 360, 99, 99, rColor);
    color = strip.Color(rColor[0] / 2, rColor[1] / 2, rColor[2] / 2);
    
    // If only the first LED is lit, but not fully. This is outside the for loop because the subtraction of
    // BAR_LENGTH causes the value to wrap around to a very high number.
    if (curData < BAR_LENGTH)
    {
      int brightness = curData * 99 / BAR_LENGTH;
      H2R_HSBtoRGB((curHue + i * BAR_HUE_DIFF) % 360, 99, brightness, rColor);
      strip.setPixelColor(getStripLocation(i, 0), strip.Color(rColor[0] / 2, rColor[1] / 2, rColor[2] / 2));
    }
    else
    {
      for (j = 0; j < BAR_LENGTH; j++)
      {
        // Light up each fully lit LED the same way.
        if (curData - BAR_LENGTH > j * BAR_LENGTH)
          strip.setPixelColor(getStripLocation(i, j), color);
        else if (curData > j * BAR_LENGTH)
        {
          // Dims the last LED in the bar based on how close the data point is to the next LED.
          int brightness = (j * BAR_LENGTH - curData) * 99 / BAR_LENGTH;
          H2R_HSBtoRGB((curHue + i * BAR_HUE_DIFF) % 360, 99, brightness, rColor);
          strip.setPixelColor(getStripLocation(i, j), strip.Color(rColor[0] / 2, rColor[1] / 2, rColor[2] / 2));
        }
      }
    }
    
    // Store all of the data points for filtering of the next iteration.
    prev[i] = curData;
  }
  
  // Cycle through all the colors.
  if (curHue == 359)
    curHue = 0;
  else
    curHue++;
  
  // Display the strip.
  strip.show();
}
