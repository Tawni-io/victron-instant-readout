#pragma once

#include <stdint.h>

#include "display.h"

// Cleaner 8x12 UI font (scale 1..3). Less blocky than the old 5x7.
void text_draw(int x, int y, const char* s, uint16_t fg, uint16_t bg, int scale);
int text_width(const char* s, int scale);
int text_height(int scale);
