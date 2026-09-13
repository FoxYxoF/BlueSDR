#ifndef INIT_H
#define INIT_H

#include "it.h"
#include "main.h"

//void SysTick_Setup(void);     // Настройка системного таймера
void Clock_System_Init(void); // Инициализация системных таймеров и тактирования
//void Clock_112MHz(void);       // Разгон
void GPIO_Init(void);   	    // инициализация портов ввода вывода
void SPI2_init(void);         // инициализация SPI
void PWM2_Init(void);         // Инициализация шим
void PWM3_Init(void);          // Инициализация шим
void ADC_DMA_Init(void);
void TIM4_Init(void);         // Инициализация таймера 4 для массивов КИХ(преобразования Гилберта и ФНЧ)
void SPI2_DMA_Init(void);
void I2C1_Recover(void);      // перезапускаем зависший I2C
void I2C1_Init(void);         // Инициализация I2C1
void TIM1_Encoder_Init(void); // Инициализация энкодера
void PWR_Init(void);          // Инициализация контроля питания
void GPIO_Init_Buttons(void); // Инициализация кнопок
void Update_Biquad_LPF(arm_biquad_casd_df1_inst_q31 *S, uint8_t stages, q31_t *coeffs_dest, const q31_t *coeffs_src, q31_t *state_buf); // Обновляем коэфициенты и переинициализируем
void DSP_init(void);          // Инициализация функций библиотеки DSP
void RX_Device_Inint(void);   // Инициализация ЦАП и АЦП на прием
void TX_Device_Inint(void);   // Инициализация ЦАП и АЦП на передачу
uint8_t Calculate_lpf_Q31(float cutOffFreq, float sampleRate, uint8_t *out_stages); // считаем коэффициенты ФНЧ для БИХ

void Calculate_Biquad4_Butterworth(float cutOffFreq, float sampleRate, biquad4_state_t *state);// Расчет коэфициентов для самописного биквада ФНЧ Баттерворта

void calculate_lpf_coeffs_q15(q15_t *pCoeffs, float cutoff_freq); // Коеффициенты для КИХ ФНЧ
void coeff_hilbert_init(void); // Считаем коэффициенты преобразования Гильберта h(n)
void fast_hilbert_q15_custom(const arm_fir_instance_q15 *S, q15_t *pSrc, q15_t *pDst, uint32_t blockSize); // Своя реализация Гильберта
void init_filters(void); // БИХ биквад для фазовращателя
uint8_t Process_Encoder(int32_t *value, int32_t step, int32_t min_val, int32_t max_val); // Проверяем частоту
void si5351_SetFrec(uint32_t frec); // Установка частоты si5351
void Set_mode();               // Установка режима из конфига
void Main_Scren_Init(void);    // Инициализация главного экрана
void Redraw_A_B(void);         // Перерисовываем A/B VFO
void Redraw_Band(void);        // Перерисовываем диапазон
void Redraw_mode(void);        // Перерисовываем модуляцию
void Redraw_volume(void);      // Перерисовываем громкость
void Redraw_bandwidth(void);   // Перерисовываем полосу
void Redraw_ATT(void);         // Перерисовываем аттеньюатор
void Redraw_Main_Scr(void);    // Перерисовываем главный экран
void Redraw_Step(uint16_t step, uint8_t m_fl); // Перерисовываем шаг для нормального режима и настроек

#endif