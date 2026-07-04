/*инициализация железа*/
#include "main.h"

void SysTick_Setup(void);     // Настройка системного таймера
void Clock_112MHz(void);       // Разгон
void GPIO_Init(void);   	    // инициализация портов ввода вывода
void SPI2_init(void);         // инициализация SPI
void PWM2_Init(void);         // Инициализация шим
void PWM3_Init(void);         // Инициализация шим
void ADC_DMA_Init(void);
void TIM4_Init(void);         // Инициализация таймера 4 для массивов КИХ(преобразования Гилберта и ФНЧ)
void I2C1_Recover(void);      // перезапускаем зависший I2C
void I2C1_Init(void);         // Инициализация I2C1
void TIM1_Encoder_Init(void); // Инициализация энкодера
void GPIO_Init_Buttons(void); // Инициализация кнопок
void DSP_init(void);          // Инициализация функций библиотеки DSP
void RX_Device_Inint(void);  // Инициализация ЦАП и АЦП на прием
void TX_Device_Inint(void);  // Инициализация ЦАП и АЦП на передачу
void Calculate_lpf_Q31(float cutOffFreq, float sampleRate); // ситаем коэфициенты ФНЧ для БИХ
void calculate_lpf_coeffs_q15(q15_t *pCoeffs, float cutoff_freq); // Коеффициенты для КИХ ФНЧ
void coeff_hilbert_init(void); // Считаем коэффициенты преобразования Гильберта h(n)
void fast_hilbert_q15_custom(const arm_fir_instance_q15 *S, q15_t *pSrc, q15_t *pDst, uint32_t blockSize); // Своя реализация Гильберта
//void process_allpass_q31(q31_t *src, q31_t *dst, uint16_t len, const q31_t *coeffs, q31_t *states);  // БИХ All-pass фазовращмтель
void init_filters(void); // БИХ биквад для фазовращателя
uint8_t Process_Encoder(uint32_t *frec, uint32_t step); // Проверяем частоту
void si5351_SetFrec(uint32_t frec); // Установка частоты si5351
void Main_Scren_Init(void);    // Инициализация главного экрана
void Refreash_A_B(void);       // Обновляем A/B VFO
void Refreash_Band(void);      // Обновляем диапазон