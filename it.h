#ifndef IT_H_
#define IT_H_
#include "main.h"

#define n_coeff_hil 127                 // длинна фильтра преобразования Гильберта
#define block_size_h 128                // размер входного блока для фильтра Гилберта

typedef struct {
    int16_t threshold;  // Порог срабатывания (например, 1024 или 2048)
    int16_t attack;     // Скорость атаки (сдвиг >> n, меньше n = быстрее)
    int16_t release;    // Скорость восстановления (шаг прибавления к Gain)
} AGC_Config;
int16_t process_dynamic_gain(int32_t sample_in, const AGC_Config *cfg, int32_t *pEnv, int32_t *pGain); //

void SysTick_Handler(void);	// Прерывание от системного таймера (счетчика миллисекунд)
void HardFault_Handler(void);	// Прерывание от критической ошибки
void DMA1_Channel1_IRQHandler(void);  // Прерывания DMA1
void Fill_buff(void);           // Заполнение буфферных массивов
//void hilbert_q_only_q15(q15_t *pSrc, q15_t *pDstQ, uint32_t blockSize); // преобразование Гильберта
uint8_t Get_Buttons_Code(void); // Так как пины разбросаны, их нужно собрать в одну тетраду (0..15) программно
void Start_Debounce(void); // Таймер антидребезга кнопок
void Button_Process(uint8_t code); // Обработчик нажатия кнопок
void Button_LongPress_Process(uint8_t code);  // Обработчик нажатия кнопок при длительном удержании
void DrawVarMenu(uint8_t menu_idx, uint8_t sub_idx); // Отрисовка переменных в меню

#endif
