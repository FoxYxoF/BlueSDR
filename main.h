#include <stdbool.h>
#include <stdint.h>
#include "stm32f10x.h"
#include "arm_math.h"
#include "si5351.h"
#include "ILI9341_GFX.h"

// Функция перевода амплитуды Q15 в значение для пикселя водопада (0-255)
uint8_t magnitude_to_db_pixel(q15_t mag);