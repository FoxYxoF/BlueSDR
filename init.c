/*инициализация железа*/
#include "init.h"
#include "it.h"

#define M_PI 3.14159265358979323846f

extern uint16_t                     adcData[];           // Массив для хранения данных: [ADC2_Data | ADC1_Data]
extern arm_fir_instance_q15         f_lpf;               // Структура состояний для КИХ ФНЧ
extern arm_fir_instance_q15         f_hil;               // Инициализация КИХ Гилберта
extern arm_cfft_instance_q15        rfft_lpf;            // Структура состояний для ОБПФ для коэффицентов КИХ ФНЧ
extern q15_t                        pstate_lpf[];        // массив состояний
extern q15_t                        coeff_lpf_q15[];     // коеффициенты KИХ ФНЧ
extern q15_t                        out_rfft_lpf_q15[];  // Массив выхода ОБПФ для КИХ ФНЧ
extern q15_t                        coeff_hil_q15[];     // коеффициенты фильтра
extern q15_t     pstate_hil[n_coeff_hil + block_size_h]; // массив состояний

extern bool       a_b_frec;                              // частота A(0), B(1)
extern uint8_t    band_idx;                              // Текущий диапазон
extern uint32_t   bands_frec_a[];                        // Диапазоны частота
extern uint32_t   bands_frec_b[];                        // Диапазоны частота
extern uint32_t   bandpass_ranges[];                     // Диапазоны полосового фильтра и фнч

extern uint32_t                     main_frec;           // Основная частоа
extern uint16_t                     step;                // Шаг перестройки
extern bool                         rx_tx_fl;            // RX/TX
// Счетчики
extern uint16_t   c_fft;             // Счетчик заполнения массива FFT
extern uint16_t   c_buff;            // Счетчик основных буферов
extern uint16_t   c_buff_delay;      // Счетчик основных буферов задержка Гилберта
// Флаги
extern bool       fft_arr_fl;        // Флаг заполнения массива для водопада FFT
extern bool       hil_half_fl;       // Первая/вторая половина основных буферов
extern bool       upscale_flag;      // Флаг апскейла, понижаем частоту дискритизации до 14.42кГц	
////////////////////////////////// ФНЧ на БИХ /////////////////////////////////////
// Структура и буферы для ФНЧ
#define LPF_STAGES 8  // 16-й порядок = 8 секций
arm_biquad_casd_df1_inst_q31 S_LPF;
q31_t lpf_coeffs[LPF_STAGES*5];    // [b0, b1, b2, a1, a2]
q31_t lpf_state[LPF_STAGES*4];     // Состояние фильтра (нужно 4 на одну секцию)
/////////////////////////////////////////////////////////////////
uint32_t stateIndex = 0; // Для самописного Гильберта
/////////////////// Буферы /////////////////////
extern char old_txt_freq[];  // Текстовый буфер для частоты

//////////////////////////    si5351  ////////////////////////////////////////////
		si5351PLLConfig_t pll_conf;
		si5351OutputConfig_t out_conf;
//////////////////////////////////////////////////////


void SysTick_Setup(void) {  // Настройка системного таймера
	FLASH->ACR |= 0x02; 		  // Flash latency = 2
	RCC->CR |= RCC_CR_HSEON;	// Запустить HSE
	while (!(RCC->CR & RCC_CR_HSERDY)) __NOP(); // Дождаться запуска
	RCC->CFGR |= (7 << 18)		// PLL x9
	          | RCC_CFGR_PLLSRC  // ВЫБРАТЬ HSE (это и есть 16-й бит!)
						|  RCC_CFGR_ADCPRE_DIV6		  // ADC Prescaler /6
						|  (4 << 8);		  // APB1 Prescaler /2
	Clock_112MHz();
	RCC->CR |= RCC_CR_PLLON;	// Включить PLL
	while (!(RCC->CR & RCC_CR_PLLRDY)) __NOP(); // Ждать готовности PLL
	RCC->CFGR |= 0x02;	// Переключиться на тактирование от PLL
	SysTick->LOAD = 72000;
	// Счетчик включить, прерывания будут, источник тактирования SYSTEM OSC
	SysTick->CTRL |= SysTick_CTRL_ENABLE | SysTick_CTRL_TICKINT | SysTick_CTRL_CLKSOURCE; 	
	
	NVIC_SetPriority(SysTick_IRQn, 15);
	NVIC_EnableIRQ(SysTick_IRQn);
}

void Clock_112MHz(void) { // Разгон
    // 1. Включаем HSE (внешний кварц)
    RCC->CR |= RCC_CR_HSEON;
    while(!(RCC->CR & RCC_CR_HSERDY));

    // 2. Настройка Flash Latency! Это критично.
    // Для 72-96 МГц нужно 2 цикла ожидания (Two wait states)
    // Если гнать выше 100 МГц, может потребоваться удача, так как официально 2 — предел.
    FLASH->ACR |= FLASH_ACR_LATENCY_2;

    // 3. Настройка делителей шин (чтобы периферия не сошла с ума)
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1;  // AHB = 96 MHz
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV1; // APB2 = 96 MHz (Макс по паспорту 72)
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2; // APB1 = 48 MHz (Макс по паспорту 36)

    // 4. Множитель PLL = 12 (8 * 12 = 96) 14-112мГц 16-128мГц
    RCC->CFGR &= ~RCC_CFGR_PLLMULL; // Очистка
    RCC->CFGR |= RCC_CFGR_PLLSRC_HSE | RCC_CFGR_PLLMULL14;

    // 5. Включаем PLL
    RCC->CR |= RCC_CR_PLLON;
    while(!(RCC->CR & RCC_CR_PLLRDY));

    // 6. Переключаемся на PLL
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

void GPIO_Init(void){	// инициализация портов ввода вывода
	/* GPIO Ports Clock Enable */
	/* Включаем тактирование портов GPIO */
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;	// Включить тактирование GPIOB
	RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;	// Включить тактирование GPIOB
	RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;	// Включить тактирование GPIOB
	// Отключаем JTAG, оставляем только SWD (PA13/PA14)
  AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;	
 
	// Для SPI2
	// Настройка пинов (PB13 - SCK, PB15 - MOSI)
	// Режим: Alternate Function Push-Pull, 50MHz (CNF=10, MODE=11 -> 0xB)
	GPIOB->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13 | GPIO_CRH_MODE15 | GPIO_CRH_CNF15);
	GPIOB->CRH |= (GPIO_CRH_MODE13 | GPIO_CRH_CNF13_1); // PB13
	GPIOB->CRH |= (GPIO_CRH_MODE15 | GPIO_CRH_CNF15_1); // PB15
	// PB14 (MISO) обычно настраивается как Input Floating или Pull-up
	GPIOB->CRH &= ~(GPIO_CRH_MODE14 | GPIO_CRH_CNF14);
	GPIOB->CRH |= GPIO_CRH_CNF14_0; // Input Floating
	
	
	// PA10 TFT_DC_Pin
	GPIOA->CRH &= ~(GPIO_CRH_MODE10 | GPIO_CRH_CNF10); // Полностью очищаем MODE и CNF для PA10
	GPIOA->CRH |= (GPIO_CRH_MODE10_0 | GPIO_CRH_MODE10_1); // MODE = 11 (Max Speed 50MHz), CNF = 00 (Push-Pull)
	// PA11 TFT_RST_Pin
	GPIOA->CRH &= ~(GPIO_CRH_MODE11 | GPIO_CRH_CNF11); // Полностью очищаем MODE и CNF для PA11
	GPIOA->CRH |= (GPIO_CRH_MODE11_0 | GPIO_CRH_MODE11_1); // MODE = 11 (Max Speed 50MHz), CNF = 00 (Push-Pull)
	// PA12 TFT_CS_Pin 
	GPIOA->CRH &= ~(GPIO_CRH_MODE12 | GPIO_CRH_CNF12); // Полностью очищаем MODE и CNF для PA12
	GPIOA->CRH |= (GPIO_CRH_MODE12_0 | GPIO_CRH_MODE12_1); // MODE = 11 (Max Speed 50MHz), CNF = 00 (Push-Pull

	// PA2 ДПФ1
	GPIOA->CRL   &= ~GPIO_CRL_CNF2;     // Сброс CNF (00: General purpose output push-pull)
	GPIOA->CRL   &= ~GPIO_CRL_MODE2;    // Очистка режима
	GPIOA->CRL   |=  GPIO_CRL_MODE2_0;   // Установка MODE (01: Output mode, max speed 10 MHz)
	// PA1 ДПФ2
	GPIOA->CRL   &= ~GPIO_CRL_CNF1;     // Сброс CNF (00: General purpose output push-pull)
	GPIOA->CRL   &= ~GPIO_CRL_MODE1;    // Очистка режима
	GPIOA->CRL   |=  GPIO_CRL_MODE1_0;   // Установка MODE (01: Output mode, max speed 10 MHz)
	// PA0 ДПФ3
	GPIOA->CRL   &= ~GPIO_CRL_CNF0;     // Сброс CNF (00: General purpose output push-pull)
	GPIOA->CRL   &= ~GPIO_CRL_MODE0;    // Очистка режима
	GPIOA->CRL   |=  GPIO_CRL_MODE0_0;   // Установка MODE (01: Output mode, max speed 10 MHz)
	// PC15 ДПФ4
	GPIOC->CRH   &= ~GPIO_CRH_CNF15;    // Сброс CNF (00: General purpose output push-pull)
	GPIOC->CRH   &= ~GPIO_CRH_MODE15;   // Очистка режима
	GPIOC->CRH   |=  GPIO_CRH_MODE15_0;  // Установка MODE (01: Output mode, max speed 10 MHz)
	// PC14 ДПФ5
	GPIOC->CRH   &= ~GPIO_CRH_CNF14;    // Сброс CNF (00: General purpose output push-pull)
	GPIOC->CRH   &= ~GPIO_CRH_MODE14;   // Очистка режима
	GPIOC->CRH   |=  GPIO_CRH_MODE14_0;  // Установка MODE (01: Output mode, max speed 10 MHz)
	// Исходное состояние: выключаем все выходы (логический 0)
	GPIOA->BRR  = (GPIO_BSRR_BS0 | GPIO_BSRR_BS1 | GPIO_BSRR_BS2);
	GPIOC->BRR  = (GPIO_BSRR_BS14 | GPIO_BSRR_BS15);
		
	// PB10 TX
	GPIOB->CRH    |=  GPIO_CRH_MODE10_0;  
	GPIOB->CRH    &= ~GPIO_CRH_CNF10;    
	// PB11 RX
	GPIOB->CRH    |=  GPIO_CRH_MODE11_0;  
	GPIOB->CRH    &= ~GPIO_CRH_CNF11;  
	
	// аналоговые входы АЦП
	// I
	GPIOA->CRL    &= ~GPIO_CRL_MODE4;    //  Сбрасываем в 0b00. Режим = Аналоговый Вход
	GPIOA->CRL    &= ~GPIO_CRL_CNF4;     //  режим-00: 0b00 Вход без подтяжки 
	// Q
	GPIOA->CRL    &= ~GPIO_CRL_MODE5;    //  Сбрасываем в 0b00. Режим = Вход
	GPIOA->CRL    &= ~GPIO_CRL_CNF5;     //  режим-00: 0b00 Вход без подтяжки	
	// Mic
	GPIOA->CRL    &= ~GPIO_CRL_MODE7;    //  Сбрасываем в 0b00. Режим = Вход
	GPIOA->CRL    &= ~GPIO_CRL_CNF7;     //  режим-00: 0b00 Вход без подтяжки
  // Выходы ШИМ
	// I
	GPIOA->CRL &= ~(GPIO_CRL_MODE3 | GPIO_CRL_CNF3); // Очистка PA3
  GPIOA->CRL |=  (GPIO_CRL_MODE3_1 | GPIO_CRL_CNF3_1); // Mode: 2MHz, CNF: AF-PP
	// Q
	GPIOA->CRL &= ~(GPIO_CRL_MODE6 | GPIO_CRL_CNF6); // Очистка PA6
	GPIOA->CRL |=  (GPIO_CRL_MODE6_1 | GPIO_CRL_CNF6_1); // Mode: 2MHz, CNF: AF-PP
	// Динамик
	//GPIOB->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0); // Очистка PB0
	//GPIOB->CRL |=  (GPIO_CRL_MODE0_1 | GPIO_CRL_CNF0_1); // Mode: 2MHz, CNF: AF-PP
	GPIOB->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1); // Очистка PB1
	GPIOB->CRL |=  (GPIO_CRL_MODE1_1 | GPIO_CRL_CNF1_1); // Mode: 2MHz, CNF: AF-PP
	
	//GPIOC->BSRR = (1<<13);              //  Потушили диод.(подтянули к питанию)
	//delay_us(3000);
	GPIOC->BSRR |= GPIO_BSRR_BS13;        //  Потушили диод. Установили бит.
	
	//GPIOC->BSRR = (1<<13);              //  Зажгли диод.(подтянули к общему)
	//delay_us(3000);
	GPIOC->BSRR |= GPIO_BSRR_BR13;       //  Зажгли диод. Сбросили бит. 

  GPIOB->BRR |= GPIO_BSRR_BS11;       //   Убрали бит прием.
  GPIOB->BSRR |= GPIO_BSRR_BS10;      //   Установили бит передачи.

  GPIOB->BRR |= GPIO_BSRR_BS10;       //   Убрали бит передачи.
  GPIOB->BSRR |= GPIO_BSRR_BS11;      //   Установили бит прием.
}


void SPI2_init(void){	 
	RCC->APB1ENR |=RCC_APB1ENR_SPI2EN;


	
	SPI2->CR1 |= 
	           //  SPI_CR1_DFF  |   //  8 bit
							 //SPI_CR1_BR_0 |   
							 //SPI_CR1_BR_1 |
							 SPI_CR1_MSTR |
							 SPI_CR1_SSI  |            
							 SPI_CR1_SSM
								 ;   
   SPI2->CR2  = 0x700; // 8 bit
   //SPI2->CR2 |= SPI_CR2_FRXTH;
   SPI2->CR1 |= SPI_CR1_SPE;   
}

void PWM2_Init(void) { // Инициализация шим
    // 1. Включить тактирование GPIOA и TIM2
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    // 3. Настройка таймера TIM2
    TIM2->PSC = 1 - 1;       // Прескалер: 72 МГц / 1 = 72 МГц 
    TIM2->ARR = 1024 - 1;     // Период: 72M/1024

    // 4. Настройка PWM Mode 1 для канала 4 (TIM2_CH4)
    TIM2->CCMR2 |= TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4M_1; // PWM mode 1
    TIM2->CCMR2 |= TIM_CCMR2_OC4PE; // Включить предзагрузку (preload)
    
    // 5. Включить выход для канала 4
    TIM2->CCER |= TIM_CCER_CC4E; 

    // 6. Включить таймер
    TIM2->CR1 |= TIM_CR1_ARPE; // Auto-reload preload enable
    TIM2->CR1 |= TIM_CR1_CEN;  // Counter enable
	
    // duty: 0 - 1000 (соответствует ARR)
    //TIM2->CCR4 = duty;
   	TIM2->CCR4 = 100;
}

void PWM3_Init(void) { // Инициализация шим
    // 1. Включить тактирование GPIOA и TIM2
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    // 3. Настройка таймера TIM2
    TIM3->PSC = 1 - 1;       // Прескалер: 72 МГц / 1 = 72 МГц 
    TIM3->ARR = 1024 - 1;     // Период: 72M/1024

		// PWM mode 1 для CH4 + включение буфера (Preload)
		TIM3->CCMR2 |= (TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4PE);

		// Разрешаем выход канала 4 в регистре CCER
		TIM3->CCER |= TIM_CCER_CC4E;

		// Запуск таймера
		TIM3->CR1 |= TIM_CR1_CEN;
}

void TIM4_Init(void) { // инициализация таймера 4 для массивов преобразования Гилберта и ФНЧ
    // 1. Включить тактирование TIM4
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;

    // 2. Настройка делителя (Prescaler)
    // Таймер частотой 112 МГц / 8(предделитель ацп)/41(28.5 + 12.5 циклов на обработку одного семла)/16(предискретизация)
    TIM4->PSC = 8*41*8*2 - 1; 

    // 3. Настройка периода (Auto-reload)
    // / 256 (размер буферного массива)
    TIM4->ARR = 128 - 1; 

    // 4. Включить прерывание по переполнению (Update Interrupt)
    TIM4->DIER |= TIM_DIER_UIE;

    // 5. Разрешить прерывание в NVIC
	  NVIC_SetPriority(TIM4_IRQn, 1); // приоритет прерывания 
    NVIC_EnableIRQ(TIM4_IRQn);

    // 6. Включить таймер
    //TIM4->CR1 |= TIM_CR1_CEN;
}

void ADC_DMA_Init(void) {  // Инициализация АЦП и ПДП
  // 1. Тактирование
  RCC->APB2ENR |=  RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN;
  RCC->AHBENR  |=  RCC_AHBENR_DMA1EN;
  RCC->CFGR    |=  RCC_CFGR_ADCPRE_DIV8; // ADC Clock = 112MHz/8 = 14MHz

	DMA1_Channel1->CPAR = (uint32_t) &ADC1->DR; 
	DMA1_Channel1->CMAR = (uint32_t) &adcData;
	DMA1_Channel1->CNDTR = (uint32_t)16;

	DMA1_Channel1->CCR |=     DMA_CCR1_MINC     // Инкримент памяти
	                        | DMA_CCR1_MSIZE_1  // 32бит память
													| DMA_CCR1_PSIZE_1  // 32бит перефирия
													| DMA_CCR1_CIRC     // Кольцевой режим
													| DMA_CCR1_TCIE     // Прерывание по полному выполнению
													| DMA_CCR1_HTIE;    // Прерывание по половине
	DMA1_Channel1->CCR |= DMA_CCR1_EN;          // Включаем DMA1 канал1
	NVIC_SetPriority(DMA1_Channel1_IRQn, 0);    // Приоритет прерывания DMA1 максимальный(0)
	NVIC_EnableIRQ (DMA1_Channel1_IRQn);        // Разрешаем прерывания DMA1

	ADC1->CR2 |= ADC_CR2_ADON;
  delay_us(10);
	ADC2->CR2 |= ADC_CR2_ADON;
  delay_us(10);	
		
	ADC1->SQR3 = 4;
	ADC1->CR2 |= ADC_CR2_ADON | ADC_CR2_DMA | ADC_CR2_CONT |ADC_CR2_EXTTRIG | ADC_CR2_EXTSEL;  // 
	ADC1->CR1 |= ADC_CR1_DUALMOD_1;
	ADC1->CR1 |= ADC_CR1_DUALMOD_2;
	ADC1->SMPR2 &= ~ADC_SMPR2_SMP4; 
  ADC1->SMPR2 |= ADC_SMPR2_SMP4_1; //ADC sample time register 010 13.5 циклов ( 011 - 28.5)
  ADC1->SMPR2 |= ADC_SMPR2_SMP4_0; //ADC sample time register 001 7.5 циклов
	
	ADC2->SQR3 = 7;
	ADC2->CR2 |=   ADC_CR2_ADON | ADC_CR2_CONT | ADC_CR2_EXTTRIG | ADC_CR2_EXTSEL; //
  //ADC2->SMPR2 |= ADC_SMPR2_SMP9_1; //ADC sample time register 13.5 циклов
	//ADC1->SMPR2 |= ADC_SMPR2_SMP7_0; //ADC sample time register 001 7.5 циклов

	ADC1->CR2 |= ADC_CR2_CAL;
	while (ADC1->CR2 & ADC_CR2_CAL){};

	ADC2->CR2 |= ADC_CR2_CAL;
	while (ADC2->CR2 & ADC_CR2_CAL){};	
	
	//ADC2->CR2 |= ADC_CR2_SWSTART;	
	//ADC1->CR2 |= ADC_CR2_SWSTART;
}

void I2C1_Recover(void) { // перезапускаем зависший I2C
	  volatile int d; // Объявляем один раз здесь
    // 1. Выключаем модуль I2C
    I2C1->CR1 &= ~I2C_CR1_PE;
    // 2. Настраиваем PB8 (SCL) и PB9 (SDA) как обычный выход Open-Drain
    // Очистка CRH (пины 8 и 9)
    GPIOB->CRH &= ~(GPIO_CRH_MODE8 | GPIO_CRH_CNF8 | GPIO_CRH_MODE9 | GPIO_CRH_CNF9);
    // Конфигурация: Mode=11 (50MHz), CNF=01 (General purpose Open-drain)
    GPIOB->CRH |= (GPIO_CRH_MODE8 | GPIO_CRH_CNF8_0 | GPIO_CRH_MODE9 | GPIO_CRH_CNF9_0);

    // 3. Генерируем 9 тактов на SCL
    // Это заставит Si5351 отпустить SDA, если она застряла в ожидании такта
    for (int i = 0; i < 9; i++) {
        GPIOB->BSRR = GPIO_BSRR_BR8;  // SCL = 0
        for (d = 0; d < 50; d++);
        GPIOB->BSRR = GPIO_BSRR_BS8;  // SCL = 1
        for (d = 0; d < 50; d++);
        
        // Если SDA поднялся в 1, значит устройство отпустило шину
        if (GPIOB->IDR & GPIO_IDR_IDR9) break;
    }

    // 4. Генерируем STOP условие вручную (SDA: 0 -> 1 при SCL = 1)
    GPIOB->BSRR = GPIO_BSRR_BR9;  // SDA = 0
    for (d = 0; d < 50; d++);
    GPIOB->BSRR = GPIO_BSRR_BS8;  // SCL = 1
    for (d = 0; d < 50; d++);
    GPIOB->BSRR = GPIO_BSRR_BS9;  // SDA = 1
    for (d = 0; d < 50; d++);

    // 5. Возвращаем пины в режим Alternate Function Open-Drain
    GPIOB->CRH &= ~(GPIO_CRH_MODE8 | GPIO_CRH_CNF8 | GPIO_CRH_MODE9 | GPIO_CRH_CNF9);
    GPIOB->CRH |= (GPIO_CRH_MODE8 | GPIO_CRH_CNF8 | GPIO_CRH_MODE9 | GPIO_CRH_CNF9);

    // 6. Программный сброс самого модуля I2C
    I2C1->CR1 |= I2C_CR1_SWRST;
    for (d = 0; d < 100; d++);
    I2C1->CR1 &= ~I2C_CR1_SWRST;

    // 7. Повторная инициализация I2C (вызовите вашу функцию инициализации)
    // I2C1_Init(); 
}
void I2C1_Init(void) { // Инициализация I2C1
	// 1. Включение тактирования портов, I2C1 и альтернативных функций (AFIO)
	RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;
	RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

	// 2. Ремаппинг I2C1 на PB8 и PB9
	AFIO->MAPR |= AFIO_MAPR_I2C1_REMAP;

	// 3. Настройка PB8 (SCL) и PB9 (SDA) как Alternate Function Open-Drain (50MHz)
	// Очистка битов конфигурации для PB8 и PB9 (регистр CRH, так как пины > 7)
	GPIOB->CRH &= ~(GPIO_CRH_MODE8 | GPIO_CRH_CNF8 | GPIO_CRH_MODE9 | GPIO_CRH_CNF9);
	// Установка: MODE=11 (50MHz), CNF=11 (AF Open-drain)
	GPIOB->CRH |= (GPIO_CRH_MODE8 | GPIO_CRH_CNF8 | GPIO_CRH_MODE9 | GPIO_CRH_CNF9);

	// 4. Сброс настроек I2C (опционально, для чистоты)
	I2C1->CR1 |= I2C_CR1_SWRST;
	I2C1->CR1 &= ~I2C_CR1_SWRST;

	// 5. Установка частоты периферии (PCLK1). Для 36 МГц:
	I2C1->CR2 |= 36; 

	// 6. Настройка скорости (Standard Mode, 100 кГц)
	// CCR = PCLK1 / (2 * Speed) = 36,000,000 / (2 * 100,000) = 180
	I2C1->CCR = 180;

	// 7. Максимальное время нарастания (TRISE)
	// Для 100 кГц: TRISE = PCLK1_MHz + 1 = 36 + 1 = 37
	I2C1->TRISE = 37;

	// 8. Включение модуля I2C1
	I2C1->CR1 |= I2C_CR1_PE;
}

void TIM1_Encoder_Init(void) {  // Инициализация энкодера
    // 1. Включаем тактирование GPIOA, альтернативных функций и TIM1
    RCC->APB2ENR |= (RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN | RCC_APB2ENR_TIM1EN);

    //2. Настройка PA8
    GPIOA->CRH &= ~GPIO_CRH_MODE8; // Сброс всех бит пина 8 MODE = 00 (Input)
    GPIOA->CRH |= GPIO_CRH_CNF8_1;   // CNF = 10 (Input with Pull), 
    GPIOA->CRH &= ~GPIO_CRH_CNF8_0;
	  GPIOA->ODR |= GPIO_ODR_ODR8;
	
    // 3. Настройка PA9
    GPIOA->CRH &= ~GPIO_CRH_MODE9; // Сброс всех бит пина 9 MODE = 00
    GPIOA->CRH |= GPIO_CRH_CNF9_1;    // CNF = 10, 
	  GPIOA->CRH &= ~GPIO_CRH_CNF9_0;
    GPIOA->ODR |= GPIO_ODR_ODR9;
    

    // 5. Настройка TIM1 в режим энкодера
    // CCMR1: Канал 1 на TI1 (CC1S=01), Канал 2 на TI2 (CC2S=01)
    TIM1->CCMR1 &= ~(TIM_CCMR1_CC1S | TIM_CCMR1_CC2S);
    TIM1->CCMR1 |= (TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0);
    
    // Добавляем фильтрацию (IC1F и IC2F = 1111), чтобы не было ложных срабатываний
    TIM1->CCMR1 |= (TIM_CCMR1_IC1F | TIM_CCMR1_IC2F);

    // SMCR: SMS = 011 (Encoder mode 3: счет по обоим входам)
    TIM1->SMCR &= ~TIM_SMCR_SMS;
    TIM1->SMCR |= (TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1);

    // 6. Установка периода и запуск
    TIM1->ARR = 0xFFFF;
    TIM1->CR1 |= TIM_CR1_CEN; //*/
}

void GPIO_Init_Buttons(void) { // Инициализация кнопок
    // 1. Тактирование портов и альтернативных функций
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;

    // 2. ОСВОБОЖДЕНИЕ ПИНОВ PA15, PB3, PB4 (Отключаем JTAG, оставляем SWD)
    // Важно: не используйте |=, так как это поле из 3-х бит.
    AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG) | AFIO_MAPR_SWJ_CFG_JTAGDISABLE;

    // 3. Настройка PA15 как Input Pull-up
    GPIOA->CRH &= ~(GPIO_CRH_MODE15 | GPIO_CRH_CNF15);
    GPIOA->CRH |= GPIO_CRH_CNF15_1; // Input with pull-up/down
    GPIOA->ODR |= GPIO_ODR_ODR15;   // Выбор именно Pull-up

    // 4. Настройка PB3, PB4, PB6 как Input Pull-up
    GPIOB->CRL &= ~(GPIO_CRL_MODE3 | GPIO_CRL_CNF3 | GPIO_CRL_MODE4 | GPIO_CRL_CNF4 | GPIO_CRL_MODE6 | GPIO_CRL_CNF6);
    GPIOB->CRL |= (GPIO_CRL_CNF3_1 | GPIO_CRL_CNF4_1 | GPIO_CRL_CNF6_1);
    GPIOB->ODR |= (GPIO_ODR_ODR3 | GPIO_ODR_ODR4 | GPIO_ODR_ODR6);

    // 5. Привязка линий EXTI к портам (с обязательной очисткой!)
    // EXTI3 (PB3) -> EXTICR[0] (пин 3)
    AFIO->EXTICR[0] = (AFIO->EXTICR[0] & ~AFIO_EXTICR1_EXTI3) | AFIO_EXTICR1_EXTI3_PB;
    
    // EXTI4 (PB4) -> EXTICR[1] (пин 4)
    AFIO->EXTICR[1] = (AFIO->EXTICR[1] & ~AFIO_EXTICR2_EXTI4) | AFIO_EXTICR2_EXTI4_PB;
    
    // EXTI6 (PB6) -> EXTICR[1] (пин 6)
    AFIO->EXTICR[1] = (AFIO->EXTICR[1] & ~AFIO_EXTICR2_EXTI6) | AFIO_EXTICR2_EXTI6_PB;
    
    // EXTI15 (PA15) -> EXTICR[3] (пин 15). PA15 - это 0x0000, поэтому просто очистка.
    AFIO->EXTICR[3] = (AFIO->EXTICR[3] & ~AFIO_EXTICR4_EXTI15);

    // 6. Настройка масок и условий EXTI
    EXTI->IMR  |= (EXTI_IMR_MR15 | EXTI_IMR_MR3 | EXTI_IMR_MR4 | EXTI_IMR_MR6);
    EXTI->FTSR |= (EXTI_FTSR_TR15 | EXTI_FTSR_TR3 | EXTI_FTSR_TR4 | EXTI_FTSR_TR6); // По спаду
    EXTI->RTSR |= (EXTI_RTSR_TR15 | EXTI_RTSR_TR3 | EXTI_RTSR_TR4 | EXTI_RTSR_TR6); // По фронту

    EXTI->PR = EXTI_PR_PR3;
		EXTI->PR = EXTI_PR_PR4;
		EXTI->PR = EXTI_PR_PR6;
		EXTI->PR = EXTI_PR_PR15;
    // 7. Разрешение прерываний в NVIC
    NVIC_EnableIRQ(EXTI3_IRQn);
    NVIC_EnableIRQ(EXTI4_IRQn);
    NVIC_EnableIRQ(EXTI9_5_IRQn);   // Обрабатывает PB6
    NVIC_EnableIRQ(EXTI15_10_IRQn); // Обрабатывает PA15
}

void DSP_init(void){  // Инициализация функций библиотеки DSP
	coeff_hilbert_init();
	arm_fir_init_q15(&f_hil, n_coeff_hil, coeff_hil_q15, pstate_hil, block_size_h); // Инициализация КИХ Гилберта
	
	// 3000.0f - частота среза в Гц
	// 21875.0f - частота дискретизации (Sample Rate)
	Calculate_lpf_Q31(3000.0f, 21875.0f); 
}

void RX_Device_Inint(void){  // Инициализация ЦАП и АЦП на прием
	  GPIOB->BRR |= GPIO_BSRR_BS10;       //   Убрали бит передачи.
    GPIOB->BSRR |= GPIO_BSRR_BS11;      //   Установили бит прием.
    // 1. ПОЛНАЯ ОСТАНОВКА
    ADC1->CR2 &= ~(ADC_CR2_SWSTART | ADC_CR2_CONT); // Стоп запуск и цикл
    DMA1_Channel1->CCR &= ~DMA_CCR1_EN;             // Стоп ПДП
    TIM4->CR1 &= ~TIM_CR1_CEN;     // Останавливаем преобразование гилберта и фнч
	
    // 2. СМЕНА КАНАЛОВ (записываем напрямую, чтобы не осталось старых бит)
    ADC1->SQR3 = 4; 
	  //ADC1->SMPR2 |= ADC_SMPR2_SMP4_0; //ADC sample time register 001 7.5 циклов
    ADC2->SQR3 = 5; 

    // 3. СБРОС ФЛАГОВ И ПЕРЕЗАПУСК ПДП
    // Сначала сбросим регистр количества данных
    DMA1_Channel1->CNDTR = 16; 
    DMA1->IFCR = DMA_IFCR_CGIF1; // Сброс всех флагов 1-го канала (глобальный флаг)
	
		// переключаем шим
		// Настраиваем режим работы канала TIM3_CH4 в регистре CCMR2 (каналы 3 и 4 настраиваются тут)
		// Очищаем биты режима четвертого канала
		TIM3->CCMR2 &= ~TIM_CCMR2_OC4M;
		// Задаем PWM Mode 1 (биты 110) + включаем Preload (OC4PE) для плавного перехода
		TIM3->CCMR2 |= (TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4M_1) | TIM_CCMR2_OC4PE;
		// Копируем текущую скважность из первого канала в четвертый
		TIM3->CCR4 = TIM3->CCR1;
		// Коммутируем выходы в регистре CCER
		TIM3->CCER &= ~TIM_CCER_CC1E; // Выключаем Channel 1 (пин PA6)
		TIM3->CCER |= TIM_CCER_CC4E;  // Включаем Channel 4 (пин PB1)
	
    // Включаем всё обратно
    DMA1_Channel1->CCR |= DMA_CCR1_EN;
    ADC1->CR2 |= ADC_CR2_CONT;   
	
		// Счетчики
		c_fft  = 0;            // Счетчик заполнения массива FFT
		c_buff  = 0;           // Счетчик основных буферов
		c_buff_delay  = 0;     // Счетчик основных буферов задержка Гилберта
		// Флаги
		fft_arr_fl = 0;        // Флаг заполнения массива для водопада FFT
		hil_half_fl = 0;       // Первая/вторая половина основных буферов
		upscale_flag = 0;     // Флаг апскейла, понижаем частоту дискритизации до 14.42кГц	
    // 4. ЗАПУСК
    ADC1->CR2 |= ADC_CR2_SWSTART; 
	  TIM4->CR1 |= TIM_CR1_CEN;     // Стартуем преобразование гилберта и фнч
}
void TX_Device_Inint(void){  // Инициализация ЦАП и АЦП на передачу
	  GPIOB->BRR |= GPIO_BSRR_BS11;       //   Убрали бит прием.
    GPIOB->BSRR |= GPIO_BSRR_BS10;      //   Установили бит передачи.
    // 1. ПОЛНАЯ ОСТАНОВКА
    ADC1->CR2 &= ~(ADC_CR2_SWSTART | ADC_CR2_CONT); // Стоп запуск и цикл
    DMA1_Channel1->CCR &= ~DMA_CCR1_EN;             // Стоп ПДП
    //TIM4->CR1 &= ~TIM_CR1_CEN;     // Останавливаем преобразование гилберта и фнч
	
    // 2. СМЕНА КАНАЛОВ (записываем напрямую, чтобы не осталось старых бит)
    ADC1->SQR3 = 4; 
	  //ADC1->SMPR2 |= ADC_SMPR2_SMP4_0; //ADC sample time register 001 7.5 циклов
    ADC2->SQR3 = 7; 

    // 3. СБРОС ФЛАГОВ И ПЕРЕЗАПУСК ПДП
    // Сначала сбросим регистр количества данных
    DMA1_Channel1->CNDTR = 16; 
    DMA1->IFCR = DMA_IFCR_CGIF1; // Сброс всех флагов 1-го канала (глобальный флаг)
    
	  // переключаем шим
		// Настраиваем режим работы канала TIM3_CH1 в регистре CCMR1
		// Очищаем биты режима первого канала
		TIM3->CCMR1 &= ~TIM_CCMR1_OC1M;
		// Задаем PWM Mode 1 (биты 110) + включаем Preload (OC1PE) для плавной смены
		TIM3->CCMR1 |= (TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1) | TIM_CCMR1_OC1PE;
		// Копируем скважность из четвертого канала в первый
		TIM3->CCR1 = TIM3->CCR4;
		// Коммутируем выходы в регистре CCER
		TIM3->CCER &= ~TIM_CCER_CC4E; // Выключаем Channel 4 (пин PB1)
		TIM3->CCER |= TIM_CCER_CC1E;  // Включаем Channel 1 (пин PA6)

    // Включаем всё обратно
    DMA1_Channel1->CCR |= DMA_CCR1_EN;
    ADC1->CR2 |= ADC_CR2_CONT;    
    
		// Счетчики
		c_fft  = 0;            // Счетчик заполнения массива FFT
		c_buff  = 0;           // Счетчик основных буферов
		c_buff_delay  = 0;     // Счетчик основных буферов задержка Гилберта
		// Флаги
		fft_arr_fl = 0;        // Флаг заполнения массива для водопада FFT
		hil_half_fl = 0;       // Первая/вторая половина основных буферов
		upscale_flag = 0;     // Флаг апскейла, понижаем частоту дискритизации до 14.42кГц	
    // 4. ЗАПУСК
    ADC1->CR2 |= ADC_CR2_SWSTART; 
	  TIM4->CR1 |= TIM_CR1_CEN;     // Стартуем преобразование гилберта и фнч
}
void Calculate_lpf_Q31(float cutOffFreq, float sampleRate) { // ситаем коэфициенты ФНЧ для БИХ
    float pi = 3.1415926535f;
    float fs = sampleRate;
    float fc = cutOffFreq;
	
    float n = (float)LPF_STAGES * 2.0f; // Порядок фильтра (16)
    float omega = tanf(pi * fc / fs);
    float omega2 = omega * omega;

    for (int i = 0; i < LPF_STAGES; i++) {
        // Угол полюса для Баттерворта 16-го порядка
        float angle = pi * (2.0f * (float)i + 1.0f) / (2.0f * n);
        float q = 1.0f / (2.0f * cosf(angle)); // Добротность секции

        float delta = omega2 + omega / q + 1.0f;
        
        float b0 = omega2 / delta;
        float b1 = 2.0f * b0;
        float b2 = b0;
        float a1 = 2.0f * (omega2 - 1.0f) / delta;
        float a2 = (omega2 - omega / q + 1.0f) / delta;

        // Запись в массив CMSIS (b0, b1, b2, a1, a2)
        // Делим на 2 для postShift=1 (защита от переполнения)
        lpf_coeffs[i * 5 + 0] = (q31_t)(b0 * 1073741824.0f); 
        lpf_coeffs[i * 5 + 1] = (q31_t)(b1 * 1073741824.0f);
        lpf_coeffs[i * 5 + 2] = (q31_t)(b2 * 1073741824.0f);
        lpf_coeffs[i * 5 + 3] = (q31_t)(-a1 * 1073741824.0f); // Инверсия a1
        lpf_coeffs[i * 5 + 4] = (q31_t)(-a2 * 1073741824.0f); // Инверсия a2
    }

    // Очистка памяти фильтра
    memset(lpf_state, 0, sizeof(lpf_state));
		// LPF_STAGES - количество секций (биквадов)
		// lpf_coeffs - массив из 5 коэффициентов
		// lpf_state - массив из 4 состояний
		// 1 - postShift (компенсирует деление коэффициентов на 2 для защиты от переполнения)
    // Инициализация. postShift = 1 обязателен, так как коэффициенты делены на 2
    arm_biquad_cascade_df1_init_q31(&S_LPF, LPF_STAGES, lpf_coeffs, lpf_state, 1);
}

/*void calculate_lpf_coeffs_q15(q15_t *pCoeffs, float cutoff_freq) {
    const int N = 64;
    float taps[64];
    float sum = 0.0f;
    float M = (N - 1) / 2.0f; // Центр фильтра

    // 1. Считаем идеальный импульсный отклик (Sinc) + Окно Блэкмана
    for (int n = 0; n < N; n++) {
        if (n == M) {
            taps[n] = 2.0f * cutoff_freq;
        } else {
            float x = (float)n - M;
            taps[n] = sinf(2.0f * M_PI * cutoff_freq * x) / (M_PI * x);
        }

        // Накладываем окно Блэкмана для подавления вне полосы
        float window = 0.42f - 0.5f * cosf(2.0f * M_PI * n / (N - 1)) + 0.08f * cosf(4.0f * M_PI * n / (N - 1));
        taps[n] *= window;
        
        sum += taps[n];
    }
    // 2. Нормировка (чтобы коэффициент передачи в полосе пропускания был 1.0)
    // 3. Перевод в формат Q15
    for (n = 0; n < N; n++) {
        taps[n] /= sum; // Убираем усиление/затухание
        pCoeffs[n] = (q15_t)__SSAT((int32_t)(taps[n] * 32768.0f), 16);
    }
} */



void coeff_hilbert_init(void) {
    float32_t out_coe;
    float32_t window;
    int16_t half = n_coeff_hil / 2;

    for (int16_t i = 1; i <= half; i++) {
        // 1. Расчет идеального коэффициента (только для нечетных i)
        if (i % 2 != 0) {
            // Формула: 2 / (pi * i)*(уменьшение усиления)
            out_coe = 2.0f / (3.1415926535f * (float32_t)i)* 0.92f;
        } else {
            out_coe = 0.0f;
        }

        // 2. Расчет окна Хэмминга для текущего смещения
        // Индексы симметричны относительно центра
        int16_t idx_pos = half + i;
        int16_t idx_neg = half - i;
        
        // Окно для текущего индекса
        window = 0.53836f - 0.46164f * arm_cos_f32((2.0f * 3.1415926535f * (float32_t)idx_pos) / (n_coeff_hil - 1));

        // 3. Сохранение в Q15 с учетом антисимметрии h(i) = -h(-i)
        float32_t val_pos = out_coe * window;
        float32_t val_neg = -val_pos;

        arm_float_to_q15(&val_pos, &coeff_hil_q15[idx_pos], 1);
        arm_float_to_q15(&val_neg, &coeff_hil_q15[idx_neg], 1);
    }

    // Центр фильтра Гильберта всегда строго 0
    coeff_hil_q15[half] = 0;
}

/**
 * Сверхбыстрый фильтр Гильберта для q15.
 * Условия:
 * 1. numTaps — нечетное (стандарт для Гильберта).
 * 2. Каждый второй коэффициент в pCoeffs — строго НОЛЬ.
 * 3. Используем только ненулевые коэффициенты.
 */
/*void fast_hilbert_q15_custom(const arm_fir_instance_q15 *S, q15_t *pSrc, q15_t *pDst, uint32_t blockSize) { 
    q15_t *pState = S->pState;                 
    const q15_t *pCoeffs = S->pCoeffs;         
    uint32_t numTaps = S->numTaps;             
    uint32_t halfTaps = numTaps >> 1;          
    uint32_t bCnt, i;
    
    uint32_t curIdx = stateIndex; // Текущая позиция "головы"

    for (bCnt = 0; bCnt < blockSize; bCnt++) {
        // 1. Записываем новый отсчет в кольцевой буфер
        pState[curIdx] = *pSrc++;
        
        q31_t acc = 0;

        // 2. Расчет с учетом кольцевого смещения
        for (i = 0; i < halfTaps; i += 2) {
            // Индексы для антисимметрии в кольцевом буфере
            int32_t idx_new = (int32_t)curIdx - i;
            if (idx_new < 0) idx_new += numTaps;
            
            int32_t idx_old = (int32_t)curIdx - (numTaps - 1 - i);
            if (idx_old < 0) idx_old += numTaps;

            q31_t x_diff = (q31_t)pState[idx_new] - pState[idx_old];
            acc += x_diff * pCoeffs[i];
        }

        *pDst++ = (q15_t)__SSAT(acc >> 15, 16);

        // 3. Продвигаем голову буфера
        curIdx++;
        if (curIdx >= numTaps) curIdx = 0;
    }
    
    stateIndex = curIdx; // Сохраняем для следующего blockSize
}*/
void fast_hilbert_q15_custom(const arm_fir_instance_q15 *S, q15_t *pSrc, q15_t *pDst, uint32_t blockSize) { 
    q15_t *pState = S->pState;
    const q15_t *pCoeffs = S->pCoeffs;
    uint32_t numTaps = S->numTaps;
    uint32_t halfTaps = numTaps >> 1;
    
    // 1. Копируем blockSize новых семплов в конец буфера состояний
    // (Буфер должен быть размером numTaps + blockSize - 1)
    memcpy(&pState[numTaps - 1], pSrc, blockSize * sizeof(q15_t));

    q15_t *pStateCur = pState; // Указатель на начало текущего окна

    for (uint32_t bCnt = 0; bCnt < blockSize; bCnt++) {
        q31_t acc = 0;
        
        // Математика та же, но БЕЗ кольцевых проверок
        // Используем два указателя: от начала окна и от конца
        q15_t *pInNew = &pStateCur[numTaps - 1]; 
        q15_t *pInOld = &pStateCur[0];
        
        for (uint32_t i = 0; i < halfTaps; i += 2) {
            // pCoeffs[i] — это только ненулевые коэффициенты
            acc += (q31_t)(*pInNew - *pInOld) * pCoeffs[i];
            pInNew -= 2;
            pInOld += 2;
        }

        *pDst++ = (q15_t)__SSAT(acc >> 15, 16);
        pStateCur++; // Двигаем окно
    }

    // 2. Переносим остаток состояний в начало для следующего вызова
    memmove(pState, &pState[blockSize], (numTaps - 1) * sizeof(q15_t));
}
//////////// Проверяем энкодер. Делать в цикле.  1 если значение изменилось ////////////////////
uint8_t Process_Encoder(uint32_t *frec, uint32_t step) {
    static int16_t last_counter = 0;
    int16_t current_counter = (int16_t)TIM1->CNT;
    
    // Делитель 4 для стандартных энкодеров с трещоткой (EC11)
    int16_t delta = (current_counter - last_counter) / 4; 

    if (delta == 0) return 0; // Изменений нет

    int64_t temp_frec = (int64_t)*frec + (delta * (int64_t)step);

    // Ограничители
    if (temp_frec < 0) temp_frec = 0;
    if (temp_frec > 120000000) temp_frec = 120000000;

    *frec = (uint32_t)temp_frec;
    last_counter += (delta * 4); // Синхронизируем счетчик

    return 1; // Частота изменилась
}

void si5351_SetFrec(uint32_t frec){
	// Статическая переменная хранит индекс текущего включенного диапазона.
	// Инициализируем значением 255, чтобы при первом запуске гарантированно сработал переключатель.
  static uint8_t current_range = 255;
	// Определение необходимого диапазона
	uint8_t new_range = 4; // По умолчанию самый верхний диапазон (> 16 МГц)
	for (uint8_t i = 0; i < 5; i++) {
			if ((frec>>2) <= bandpass_ranges[i]) {
					new_range = i;
					break;
			}
	}
	// Если диапазон не изменился, пропускаем управление GPIO
	if (new_range != current_range) {   
			// Поочередно гасим старый и зажигаем новый пин через регистр BSRR.
			// BRx - сброс (0), BSx - установка (1).
			switch (new_range) {
					case 0: // PA2 (До 2 МГц)
							GPIOA->BSRR = GPIO_BSRR_BS2 | GPIO_BSRR_BR1 | GPIO_BSRR_BR0;
							GPIOC->BSRR = GPIO_BSRR_BR15 | GPIO_BSRR_BR14;
							break;
					case 1: // PA1 (От 2 до 4 МГц)
							GPIOA->BSRR = GPIO_BSRR_BR2 | GPIO_BSRR_BS1 | GPIO_BSRR_BR0;
							GPIOC->BSRR = GPIO_BSRR_BR15 | GPIO_BSRR_BR14;
							break;
					case 2: // PA0 (От 4 до 8 МГц)
							GPIOA->BSRR = GPIO_BSRR_BR2 | GPIO_BSRR_BR1 | GPIO_BSRR_BS0;
							GPIOC->BSRR = GPIO_BSRR_BR15 | GPIO_BSRR_BR14;
							break;
					case 3: // PC15 (От 8 до 16 МГц)
							GPIOA->BSRR = GPIO_BSRR_BR2 | GPIO_BSRR_BR1 | GPIO_BSRR_BR0;
							GPIOC->BSRR = GPIO_BSRR_BS15 | GPIO_BSRR_BR14;
							break;
					case 4: // PC14 (Выше 16 МГц)
							GPIOA->BSRR = GPIO_BSRR_BR2 | GPIO_BSRR_BR1 | GPIO_BSRR_BR0;
							GPIOC->BSRR = GPIO_BSRR_BR15 | GPIO_BSRR_BS14;
							break;
			}
			// Запоминаем новый активный диапазон
			current_range = new_range;
	} 
	// Установка частоты на CLK0
	// Параметры: частота в Гц, ток драйвера (2MA, 4MA, 6MA, 8MA)
	si5351_SetupCLK0(frec, SI5351_DRIVE_STRENGTH_6MA);
	si5351_EnableOutputs(1 << 0);
	//si5351_SetupCLK2(1876000, SI5351_DRIVE_STRENGTH_2MA);

	// 3. Включаем ОБА выхода одновременно через битовую маску
	//si5351_EnableOutputs((1 << 2) | (1 << 0));
}

void Main_Scren_Init(void){         // Инициализация основного экрана
	a_b_frec=0;
	ILI9341_Fill_Screen(MYFON);       // заливка всего экрана цветом (цвета в файле ILI9341_GFX.h)
	Draw_Step(step);                  // Рисуем плашки под основной частотой
	ILI9341_Draw_Scale();             // Шкала водопада
  Draw_SMeter_Labels(40, 11);
	if (!rx_tx_fl) {                  // RX/TX
	  ILI9341_WriteString( 46, 108, "R", Font_16x26, GREEN, MYFON); // RX/TX
	}
	else {
		ILI9341_WriteString( 46, 108, "T", Font_16x26, GREEN, MYFON); // RX/TX
	}
	Refreash_Band();  // Обновляем диапазон
  Refreash_A_B(); // Обновляем A/B VFO ------------
	ILI9341_WriteString( 46, 118+26,    "   SDR ON", Font_16x26, WHITE, MYFON); // RX/TX
	ILI9341_WriteString( 46+8, 118+26+26, " BLUE PILL", Font_16x26, WHITE, MYFON); // RX/TX
}

void Refreash_A_B(void){           // Перерисовываем A/B VFO
	memset(old_txt_freq, 0, 12);     // обнуляем текстовый буфер вывода частоты
	if (!a_b_frec) {                 // A/B
	  ILI9341_WriteString( 46, 108-26, "VFO B", Font_11x18, GREEN, MYFON); // vfo a/vfo b
		ILI9341_Draw_Frec11x18(46 + 84, 108-26, bands_frec_b[band_idx]);
		ILI9341_Draw_MainFrec(78, 108, bands_frec_a[band_idx]); // выводим основную частоу
		si5351_SetFrec(bands_frec_a[band_idx]<<2);
	}
	else {
		ILI9341_WriteString( 46, 108-26, "VFO A", Font_11x18, GREEN, MYFON); // vfo a/vfo b
		ILI9341_Draw_Frec11x18(46 + 84, 108-26, bands_frec_a[band_idx]);
		ILI9341_Draw_MainFrec(78, 108, bands_frec_b[band_idx]); // выводим основную частоу
		si5351_SetFrec(bands_frec_b[band_idx]<<2);
	}/**/
}

void Refreash_Band(void){     // Обновляем диапазон
	ILI9341_WriteString( 46, 0, "Band - ", Font_11x18, GREEN, MYFON); // Диапазон
	switch (band_idx) {
		case 0x00: // 160m
		  ILI9341_WriteString( 46+77, 0, "160m", Font_11x18, GREEN, MYFON); //
		  break;
		case 0x01: // 80m
		  ILI9341_WriteString( 46+77, 0, "80m", Font_11x18, GREEN, MYFON); // 
		  break;
		case 0x02: // 40m
		  ILI9341_WriteString( 46+77, 0, "40m", Font_11x18, GREEN, MYFON); // 	
		  break;
		case 0x03: // 30m
		  ILI9341_WriteString( 46+77, 0, "30m", Font_11x18, GREEN, MYFON); // 
		  break;
		case 0x04: // 20m
		  ILI9341_WriteString( 46+77, 0, "20m", Font_11x18, GREEN, MYFON); // 	
		  break;
		case 0x05: // 17m
		  ILI9341_WriteString( 46+77, 0, "17m", Font_11x18, GREEN, MYFON); // 	
		  break;
		case 0x06: // 15m
		  ILI9341_WriteString( 46+77, 0, "15m", Font_11x18, GREEN, MYFON); // 
		  break;
		case 0x07: // 12m
		  ILI9341_WriteString( 46+77, 0, "12m", Font_11x18, GREEN, MYFON); // 
		  break;
		case 0x08: // 10m
		  ILI9341_WriteString( 46+77, 0, "10m", Font_11x18, GREEN, MYFON); // 
		  break;
	}
}
