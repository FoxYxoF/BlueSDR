#ifndef IT_H
#define IT_H
#include "main.h"

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


void SysTick_Handler(void);	          // Прерывание от системного таймера (счетчика миллисекунд)
void HardFault_Handler(void);	        // Прерывание от критической ошибки
void DMA1_Channel1_IRQHandler(void);  // Прерывания DMA1
__STATIC_FORCEINLINE void Fill_buff(void);                 // Заполнение буфферных массивов
uint8_t Get_Buttons_Code(void); // Так как пины разбросаны, их нужно собрать в одну тетраду (0..15) программно
void Start_Debounce(void); // Таймер антидребезга кнопок
void Button_Process(uint8_t code); // Обработчик нажатия кнопок
void Button_LongPress_Process(uint8_t code);  // Обработчик нажатия кнопок при длительном удержании


#endif
