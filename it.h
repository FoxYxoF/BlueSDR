#ifndef IT_H
#define IT_H
//#include "main.h"
#include <stdbool.h>
#include <stdint.h>
#include "stm32f10x.h"
#include "arm_math.h"

#define n_coeff_hil 127                 // длинна фильтра преобразования Гильберта
#define block_size_h 128                // размер входного блока для фильтра Гилберта

typedef struct {
    int8_t threshold_bits;  // Порог срабатывания (в битах) / Номер бита: 7=128, 8=256, 9=512, 10=1024 ... 15=32768
    int8_t attack;          // Скорость атаки (сдвиг >> n, меньше n = быстрее)
    int8_t release;         // Скорость восстановления (шаг прибавления к Gain)
    // Внутреннее состояние (переменные)
    int32_t env;   // Было *pEnv
    int32_t gain;  // Было *pGain
} AGC_Config;

__STATIC_FORCEINLINE int16_t process_dynamic_gain(int32_t sample_in, AGC_Config *cfg); //

// Структура для одного канала БИХ-фильтра 4-го порядка (2 последовательных каскада)
typedef struct {
    // История сигналов 1-го каскада
    int32_t x1, x2, y1, y2;
    // Коэффициенты 1-го каскада
    int32_t b0, b1, b2, a1, a2;

    // История сигналов 2-го каскада
    int32_t k2_x1, k2_x2, k2_y1, k2_y2;
    // Коэффициенты 2-го каскада
    int32_t k2_b0, k2_b1, k2_b2, k2_a1, k2_a2;
} biquad4_state_t;


// Объявляем функцию фильтрации, чтобы она была видна в других модулях
void Filter_Biquad_4th(int16_t *sample, biquad4_state_t *state);

void SysTick_Handler(void);	          // Прерывание от системного таймера (счетчика миллисекунд)
void HardFault_Handler(void);	        // Прерывание от критической ошибки
void DMA1_Channel1_IRQHandler(void);  // Прерывания DMA1
void Fill_buff(void);                 // Заполнение буфферных массивов
uint8_t Get_Buttons_Code(void); // Так как пины разбросаны, их нужно собрать в одну тетраду (0..15) программно
void Start_Debounce(void); // Таймер антидребезга кнопок
void Button_Process(uint8_t code); // Обработчик нажатия кнопок
void Button_LongPress_Process(uint8_t code);  // Обработчик нажатия кнопок при длительном удержании


#endif
