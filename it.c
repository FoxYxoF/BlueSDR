/* Векторы прерываний */
#include "it.h"
#include "ILI9341_GFX.h"


//////////////// мусор для проверки
//bool trg = false;
////////////////////// Основная логика и для сохранения /////////////////////////////////////
volatile bool rx_tx_fl = false;                  // Прием(0) передача(1)
uint32_t   main_frec =  7100000;              // Основная частота
volatile bool a_b_frec = 1;                   // частота A(0), B(1)
uint8_t    att_pre = 0;                       // Аттеньюатор/напрямую/предусилитель
uint8_t    mod = 0;                           // Модуляция sw lsb usb am fm
uint8_t    band_idx = 2;                      // Текущий диапазон
uint16_t   step = 100;                        // Шаг перестройки

int16_t    cal_si = 5213;                     // Калибровка si
int32_t    cal_fase = 0;                      // Калибровка фазы
int32_t    cal_balance = 0;                   // Калибровка баланса фаз

uint32_t   bandpass_ranges[5] =               // Диапазоны полосового фильтра и фнч
 {2000000, 4000000, 8000000, 16000000, 30000000};
uint32_t   bands_frec_a[9] =                  // Диапазоны частота A
   {1875000, 3630000, 7090000, 10125000, 14200000, 18150000, 21250000, 24950000, 28500000};
uint32_t   bands_frec_b[9] =                  // Диапазоны частота B
   {1875100, 3630100, 7090100, 10125100, 14200100, 18150100, 21250100, 24950100, 28500100};
uint8_t    bands_att_pre[9] =                 // Диапазоны аттеньюатор(0)/напрямую(1)/предусилитель(2)
   {0, 0, 1, 1, 1, 2, 2, 2, 2};                           
uint8_t    bands_mod_a[9] =                   // Диапазоны модуляция A cw(0), ssb(1), am(2), fm(3)
   {1, 1, 1, 0, 1, 1, 1, 1, 1};               // 30м: CW(0), остальные: SSB(1)
uint8_t    bands_mod_b[9] =                   // Диапазоны модуляция B cw(0), ssb(1), am(2), fm(3)
   {1, 1, 1, 0, 1, 1, 1, 1, 1};               // 30м: CW(0), остальные: SSB(1)
uint16_t   bandwidth[4] =                     // Полосы фильтра зч под индексы модуляции
   {500, 2700, 5000, 5000};                   // 0:cw, 1:ssb, 2:am, 3:fm     /*  */  
///////////////////////////////// меню настроек ///////////////////////////////////////////
bool       menu_fl = 0;                       // Флаг фхода в меню настроек
bool       sub_menu_fl = 0;                   // Флаг фхода в подменю
const char* MAIN_MENU[] = {
	  "Bandwidth",         // 0
    "Calibration",       // 1
    "Waterfall",         // 2
    "AGC",               // 3
    "Mic Limiter",       // 4
	  "Band-Pass Filter",  // 5
	  "Reserved",          // 6
    "Exit"               // 7
};

const uint8_t MAIN_MENU_SIZE = sizeof(MAIN_MENU) / sizeof(MAIN_MENU[0]);

// 0. Подменю "Bandwidth"
const char* SUB_0[] = {
    "CW",
    "SSB",
	  "AM",
	  "FM",
    "Back"
};
const uint8_t SUB_0_SIZE = sizeof(SUB_0) / sizeof(SUB_0[0]);

// 1. Подменю "Calibration"
const char* SUB_1[] = {
    "si5351",
    "Phase",
	  "Balance",
    "Back"
};
const uint8_t SUB_1_SIZE = sizeof(SUB_1) / sizeof(SUB_1[0]);

// 2. Подменю "Waterfall"
const char* SUB_2[] = {
    "Range",
    "Pallet",
    "Back"
};
const uint8_t SUB_2_SIZE = sizeof(SUB_2) / sizeof(SUB_2[0]);

// 3. Подменю "AGC"
const char* SUB_3[] = {
    "Attack",
    "Release",
    "Back"
};
const uint8_t SUB_3_SIZE = sizeof(SUB_3) / sizeof(SUB_3[0]);

// 4. Подменю "Mic Limiter"
const char* SUB_4[] = {
    "Attack",
    "Release",
	  "Gain",
    "Back"
};
const uint8_t SUB_4_SIZE = sizeof(SUB_4) / sizeof(SUB_4[0]);

// 5. Подменю "Band-Pass Filter"
const char* SUB_5[] = {
    "Filter 1",
    "Filter 2",
		"Filter 3",
		"Filter 4",
		"Filter 5",
    "Back"
};
const uint8_t SUB_5_SIZE = sizeof(SUB_5) / sizeof(SUB_5[0]);

// 6. Подменю "Reserved"
const char* SUB_6[] = {
    "Reserved 1",
    "Reserved 2",
    "Back"
};
const uint8_t SUB_6_SIZE = sizeof(SUB_6) / sizeof(SUB_6[0]);
// --- ТАБЛИЦА СВЯЗЕЙ (КАРТА МЕНЮ) ---
// Индекс в этой таблице = Номер пункта главного меню
const char** ALL_SUB_MENUS[] = {
    SUB_0,    // Индекс 0 (Привязано к "Bandwidth")
    SUB_1,    // Индекс 1 (Привязано к "Calibration")
    SUB_2,    // Индекс 2 (Привязано к "Waterfall")
    SUB_3,    // Индекс 3 (Привязано к "AGC")
    SUB_4,    // Индекс 4 (Привязано к "Mic Limiter")
    SUB_5,    // Индекс 5 (Привязано к "Band-Pass Filter")
    SUB_6,    // Индекс 6 (Привязано к "Reserved")
    0         // Индекс 7 (Пусто для   "Exit", подменю нет)
};

// Таблица размеров подменю для контроля прокрутки
const uint8_t SUB_MENU_SIZES[] = {
    SUB_0_SIZE,  //
    SUB_1_SIZE,  //
    SUB_2_SIZE,  //
    SUB_3_SIZE,  //
    SUB_4_SIZE,  //
    SUB_5_SIZE,  //
    SUB_6_SIZE,  //
    0            // Для Exit подменю нет
};

// Инициализация переменных навигации (начинаем с Главного меню, 0-й строки)
volatile uint8_t current_menu = 0;
volatile uint8_t current_submenu = 0;
///////////////////////////// Буферы  //////////////////////////////////////
uint16_t   adcData[64];                 // Буфер сырых данных с АЦП1 и АЦП2
uint16_t   adc1 = 0;                    // Апскейл на 16/2 с АЦП1 (15бит)
uint16_t   adc2 = 0;                    // Апскейл на 16/2 с АЦП2 (15бит)
q15_t      in_I_ch = 0;                 // Входной сигнал In-phase   (Re) 15бит без постоянной составляющей adc1-16384
q15_t      in_Q_ch = 0;                 // Входной сигнал Quadrature (Im) 15бит без постоянной составляющей adc2-16384
q15_t      out_I_ch = 0;                // Выдной сигнал 
q15_t      out_Q_ch = 1;                // Выдной сигнал 
q15_t      in_I_up = 0;                 // Входной сигнал апскей на два
q15_t      in_Q_up = 0;                 // Входной сигнал апскей на два
///////////////// CIC фильтр ////////////////////////////////////////////////////////////
static int32_t i_i1 = 0, i_i2 = 0, i_i3 = 0; // Секция интеграторов для I
static int32_t i_c1_z1 = 0, i_c2_z1 = 0, i_c3_z1 = 0; // Секция гребенки для I

static int32_t q_i1 = 0, q_i2 = 0, q_i3 = 0; // Секция интеграторов для Q
static int32_t q_c1_z1 = 0, q_c2_z1 = 0, q_c3_z1 = 0; // Секция гребенки для Q
////////// буферы I и Q и LPF каналов ////////////////////////////////////////////////////////
#define buff_size 256                   // размер буферов
#define buff_size_half 128              // размер половины буферов
q15_t in_raw_i [buff_size];             // входной массив I
q15_t in_raw_q [buff_size];             // входной массив Q
q15_t out_ht_q [buff_size];             // выходной массив Q после Гильберта
q31_t in_lpf   [buff_size];             // входной массив ФНЧ
q31_t out_lpf  [buff_size];             // выходной массив ФНЧ
///////// Коеффициенты филтра Гилбер ///////////////////////////////////////////////////
#define   delay_hil    256+63               // задержка преобразования Гильберта = (длинна фильтра/2) -1
q15_t     coeff_hil_q15[n_coeff_hil+1];   // коеффициенты фильтра
q15_t     pstate_hil[n_coeff_hil + block_size_h + 1];// массив состояний
q15_t     delay_arr[delay_hil];         // массив задержки Гилберта
arm_fir_instance_q15 f_hil;
//////////// ФНЧ на БИХ /////////////////
// Структура и буферы для ФНЧ
extern arm_biquad_casd_df1_inst_q31 S_LPF;
extern q31_t lpf_coeffs[];    // [b0, b1, b2, a1, a2]
extern q31_t lpf_state[];     // Состояние фильтра (нужно 4 на одну секцию)
extern arm_biquad_casd_df1_inst_q31 S_Phase_I, S_Phase_Q;
/////////////////////   ФВЧ БИХ 2 порядок ////////////////////////////////
 /* Примеры для Fs = 21341 Гц:
 *   SHIFT = 4  =>  2^4 = 16  =>  Fc = 21341 / (6.28 * 16)  ? 212 Гц
 *   SHIFT = 5  =>  2^5 = 32  =>  Fc = 21341 / (6.28 * 32)  ? 106 Гц  
 *   SHIFT = 6  =>  2^6 = 64  =>  Fc = 21341 / (6.28 * 64)  ? 53 Гц
 *   SHIFT = 7  =>  2^7 = 128 =>  Fc = 21341 / (6.28 * 128) ? 26 Гц
 * 
 * Примечание для каскадного фильтра (2-й порядок):
 * Так как фильтр состоит из двух одинаковых каскадов, реальная точка затухания
 * в -3 dB сместится чуть выше (примерно в 1.5 раза выше, чем Fc одного каскада).
 * То есть при SHIFT = 5 общая точка -3 dB будет в районе 150-160 Гц, что 
 * идеально завалит помеху 70 Гц (она уйдет в зону глубокого подавления). */
#define HPF_SHIFT  4  // Жесткая константа для компилятора 

// Глобальные переменные состояния фильтра (обнулить при старте процессора)
static int64_t audio_lpf1 = 0;
static int64_t audio_lpf2 = 0;
static int64_t audio_lpf3 = 0;
// Счетчик для мягкого старта при включении
static uint16_t init_counter = 0;
/////////////////// All-passфазовращатель ////////////////////////////////
extern q31_t statesA[]; // 7 для y[n-1], 7 для x[n-1]
extern q31_t statesB[];
// Коэффициенты ветки А и B
extern q31_t coeffsA[];
extern q31_t coeffsB[];
extern arm_biquad_casd_df1_inst_q31 S_AP_A, S_AP_B;
///////////////////// FFT для водопада ///////////////////////////////////
arm_cfft_instance_q15 cfft;             // Структура состояний для arm_cfft_q15
q15_t complex_in_fft[512];              // Входной массив для БПФ{re, im, re, im...}
uint16_t fft_size = 256;                // Размер массива FFT
/////////////////////	 Динамика(компрессор АРУ) ///////////////////////////
// AGC_Config
//    int16_t threshold;  // Порог срабатывания (например, 1024 или 2048)
//    int16_t attack;     // Скорость атаки (сдвиг >> n, меньше n = быстрее)
//    int16_t release;    // Скорость восстановления (шаг прибавления к Gain)
// * Для частоты дискретизации Fs = 21000 Гц упрощенная формула:
// * T_мс = 2^N / 21
// Настройки для приема (Медленное АРУ)
const AGC_Config rx_agc = {500, 3, 8}; // Быстрая атака, быстрое отпускание(10 бит ограничение)
// Настройки для передачи (Быстрый компрессор)
const AGC_Config tx_comp = {500, 3, 12}; // Быстрая атака, плавное отпускание(10 бит ограничение)

static int32_t rx_env, rx_gain = 32768;
static int32_t tx_env, tx_gain = 32768;
/**/
///////////////////////// S - metr  ///////////////////////////////////////
q15_t s_abs = 0;
q15_t s_peak = 0;
//////////////////////// Кнопки меню //////////////////////////////////////
//uint8_t code_button = 0;
char txt_butt[32];
volatile uint8_t code_button = 0;
volatile uint8_t is_debouncing = 0;
volatile uint8_t last_button_state = 0; // Предыдущее состояние (для проверки отпускания)
volatile uint16_t press_timer = 0; // Счетчик времени удержания
#define LONG_PRESS_TIME 50 // 50 циклов по 20мс = 1000мс
uint8_t  long_press_executed = 0;
////////////////////////////////////////////////////////////////////////////
// Счетчики
uint16_t   c_fft  = 0;            // Счетчик заполнения массива FFT
uint16_t   c_buff  = 0;           // Счетчик основных буферов
uint16_t   c_buff_delay  = 0;     // Счетчик основных буферов задержка Гилберта
// Флаги
bool       fft_arr_fl = 0;        // Флаг заполнения массива для водопада FFT
bool       hil_half_fl = 0;       // Первая/вторая половина основных буферов
bool       upscale_flag = 0;      // Флаг апскейла, понижаем частоту дискритизации до 14.42кГц

void SysTick_Handler(void) {
	/*	if (trg){
		TIM2->CCR4 = 1024;
		trg = false;
	}
	else{
		TIM2->CCR4 = 0;
		trg = true;
	} */
    uint8_t current_code = Get_Buttons_Code();
    
    if (current_code != 0) {
        // Кнопка зажата
        if (last_button_state == 0) {
            // Только зафиксировали нажатие, начинаем отсчет
            press_timer = 0;
            long_press_executed = 0; // Сброс флага выполнения длинного нажатия
        } else {
            // Кнопка удерживается
            if (press_timer < LONG_PRESS_TIME) {
                press_timer++;
            } else if (!long_press_executed) {
                // Сработало длинное нажатие
                Button_LongPress_Process(current_code);
                long_press_executed = 1; // Помечаем, что длинное нажатие выполнено
            }
        }
        last_button_state = current_code;
    } 
    else {
        // Кнопку отпустили
        if (last_button_state != 0) {
            // Если кнопку отпустили ДО того, как сработало длинное нажатие
            if (!long_press_executed) {
                Button_Process(last_button_state); // Короткое нажатие
            }
        }
        
        SysTick->CTRL = 0; // Стоп
        last_button_state = 0;
        is_debouncing = 0;
        press_timer = 0;
        long_press_executed = 0;
    }
}

void HardFault_Handler(void) {
	for (;;) {
	}
}


void TIM4_IRQHandler(void) {
    if (TIM4->SR & TIM_SR_UIF) {
        TIM4->SR &= ~TIM_SR_UIF;

        // Если обработали первую половину буфера
        if ((c_buff >= buff_size_half) && (hil_half_fl)/**/) {
            // 1. Сначала считаем ФНЧ для ПЕРВОЙ половины
            arm_biquad_cascade_df1_q31(&S_LPF, &in_lpf[0], &out_lpf[0], buff_size_half);
					  //arm_fir_fast_q15(&f_hil, &in_raw_q[0], &out_ht_q[0], buff_size_half);
					  fast_hilbert_q15_custom(&f_hil, &in_raw_q[0], &out_ht_q[0], buff_size_half); // Своя реализация Гильберта
            // 2. Затем считаем фазовращатель на основе свежего out_lpf[0]
            
            hil_half_fl = 0;
        }
        // Если обработали вторую половину буфера
        else if ((c_buff < buff_size_half) && (!hil_half_fl)/**/) {
            // 1. Сначала ФНЧ для ВТОРОЙ половины
            arm_biquad_cascade_df1_q31(&S_LPF, &in_lpf[buff_size_half], &out_lpf[buff_size_half], buff_size_half);
				  	//arm_fir_fast_q15(&f_hil, &in_raw_q[buff_size_half], &out_ht_q[buff_size_half], buff_size_half); 
				  	fast_hilbert_q15_custom(&f_hil, &in_raw_q[buff_size_half], &out_ht_q[buff_size_half], buff_size_half); // Своя реализация Гильберта
            // 2. Затем фазовращатель для ВТОРОЙ половины
            
            hil_half_fl = 1;
        }
    }
}


void Process_IQ_Buffer(const uint16_t *p_data, int16_t *out_I, int16_t *out_Q) {
    // Копируем состояния в локальные переменные для размещения в регистрах процессора
    int32_t loc_i1 = i_i1; int32_t loc_i2 = i_i2; int32_t loc_i3 = i_i3;
    int32_t loc_q1 = q_i1; int32_t loc_q2 = q_i2; int32_t loc_q3 = q_i3;

    // --- СЕКЦИЯ ИНТЕГРАТОРОВ (Развернутый цикл с автоинкрементом указателя) ---
    // На каждой паре строк считывается сначала I, затем Q, а указатель p_data сдвигается вперед
    
    // Пара 0
    loc_i1 += *p_data++; loc_i2 += loc_i1; loc_i3 += loc_i2;
    loc_q1 += *p_data++; loc_q2 += loc_q1; loc_q3 += loc_q2;
    
    // Пара 1
    loc_i1 += *p_data++; loc_i2 += loc_i1; loc_i3 += loc_i2;
    loc_q1 += *p_data++; loc_q2 += loc_q1; loc_q3 += loc_q2;
    
    // Пара 2
    loc_i1 += *p_data++; loc_i2 += loc_i1; loc_i3 += loc_i2;
    loc_q1 += *p_data++; loc_q2 += loc_q1; loc_q3 += loc_q2;
    
    // Пара 3
    loc_i1 += *p_data++; loc_i2 += loc_i1; loc_i3 += loc_i2;
    loc_q1 += *p_data++; loc_q2 += loc_q1; loc_q3 += loc_q2;
    
    // Пара 4
    loc_i1 += *p_data++; loc_i2 += loc_i1; loc_i3 += loc_i2;
    loc_q1 += *p_data++; loc_q2 += loc_q1; loc_q3 += loc_q2;
    
    // Пара 5
    loc_i1 += *p_data++; loc_i2 += loc_i1; loc_i3 += loc_i2;
    loc_q1 += *p_data++; loc_q2 += loc_q1; loc_q3 += loc_q2;
    
    // Пара 6
    loc_i1 += *p_data++; loc_i2 += loc_i1; loc_i3 += loc_i2;
    loc_q1 += *p_data++; loc_q2 += loc_q1; loc_q3 += loc_q2;
    
    // Пара 7
    loc_i1 += *p_data++; loc_i2 += loc_i1; loc_i3 += loc_i2;
    loc_q1 += *p_data++; loc_q2 += loc_q1; loc_q3 += loc_q2;

    // Возвращаем измененные состояния интеграторов в глобальную память
    i_i1 = loc_i1; i_i2 = loc_i2; i_i3 = loc_i3;
    q_i1 = loc_q1; q_i2 = loc_q2; q_i3 = loc_q3;

    // --- СЕКЦИЯ ДЕЦИМАЦИИ И ГРЕБЕНЧАТЫХ ФИЛЬТРОВ ---
    // Выполняется один раз в самом конце, реализуя децимацию R=8
    
    // Канал I
    int32_t i_diff1 = loc_i3 - i_c1_z1;  i_c1_z1 = loc_i3;
    int32_t i_diff2 = i_diff1 - i_c2_z1; i_c2_z1 = i_diff1;
    int32_t i_diff3 = i_diff2 - i_c3_z1; i_c3_z1 = i_diff2;

    // Канал Q
    int32_t q_diff1 = loc_q3 - q_c1_z1;  q_c1_z1 = loc_q3;
    int32_t q_diff2 = q_diff1 - q_c2_z1; q_c2_z1 = q_diff1;
    int32_t q_diff3 = q_diff2 - q_c3_z1; q_c3_z1 = q_diff2;

    // --- МАСШТАБИРОВАНИЕ, УДАЛЕНИЕ ПОСТОЯННОЙ СОСТАВЛЯЮЩЕЙ И ВЫРАВНИВАНИЕ ПО 0 ---
    // Вычитаем смещение средней точки (2048 * 512 = 1048576) и сдвигаем на 6 бит для вывода в 15-бит
    *out_I = (int16_t)((i_diff3 - 1048576) >> 6);
    *out_Q = (int16_t)((q_diff3 - 1048576) >> 6);
}

void DMA1_Channel1_IRQHandler(void) {
	// Проверяем, что прерывание вызвано половиной передачи (Half Transfer)
	if (DMA1->ISR & DMA_ISR_HTIF1) { // Сбрасываем флаг прерывания (запись 1 в CHTIF1 очищает флаг HTIF1)
		DMA1->IFCR |= DMA_IFCR_CHTIF1;  
		// -------------------------------
		// делаем передискритизацию с CIC фильтром
		Process_IQ_Buffer(&adcData[0], &in_I_ch, &in_Q_ch);

		Fill_buff(); //Заполняем буферы
	}
	// Проверяем, что прерывание вызвано полной передачей (Transfer Complete)
	if (DMA1->ISR & DMA_ISR_TCIF1) { // Сбрасываем флаг прерывания (запись 1 в CTCIF1 очищает флаг TCIF1)
		DMA1->IFCR |= DMA_IFCR_CTCIF1;  
		// -------------------------------
    // делаем передискритизацию с CIC фильтром
    Process_IQ_Buffer(&adcData[16], &in_I_ch, &in_Q_ch);
		
		Fill_buff(); //Заполняем буферы
	}

}
//DC Blocker
//IIR 
//3-stage
// Блокировка постоянной. ФВЧ БИХ 3 порядка (18 dB/окт) 
static __inline void Process_Audio_HPF(q31_t *sample) {
    // Читаем 32-битное значение из памяти в регистр процессора
    int32_t audio_buf = *sample;

    // Мягкий старт для плавной зарядки всех каскадов при включении АЦП
    if (init_counter < 1024) {
        init_counter++;
        audio_lpf1 = audio_buf; 
        audio_lpf2 = 0;
        audio_lpf3 = 0;
        *sample = 0; 
        return;
    }

    // --- КАСКАД 1 ---
    audio_lpf1 += (audio_buf - audio_lpf1) >> HPF_SHIFT;
    audio_buf -= audio_lpf1; 

    // --- КАСКАД 2 ---
    audio_lpf2 += (audio_buf - audio_lpf2) >> HPF_SHIFT;
    audio_buf -= audio_lpf2; 

    // --- КАСКАД 3 ---
    audio_lpf3 += (audio_buf - audio_lpf3) >> HPF_SHIFT;
    audio_buf -= audio_lpf3; 

    // --- ОГРАНИЧЕНИЕ И ВЫВОД ---
    // Безопасное насыщение в рамках 16-битного динамического диапазона без сдвига масштаба
    int32_t audio_out_16 = __SSAT(audio_buf, 16);

    // Запись обратно в переменную q31_t с сохранением Ку = 1
    *sample = (q31_t)(audio_out_16);
}

/// Заполняем буферы в прерывании DMA1
void Fill_buff(void){	
	
	if (upscale_flag){// если передескретизация выполнена

		if (!rx_tx_fl){ // Прием/передача
			// Если прием
			//in_raw_i[c_buff] = in_I_up;
			in_raw_q[c_buff] = -in_Q_up;
			out_I_ch = delay_arr[c_buff_delay];
	  	delay_arr[c_buff_delay] = in_I_up;
			
			in_lpf[c_buff] = out_ht_q[c_buff] + out_I_ch;
			Process_Audio_HPF(&in_lpf[c_buff]);
			
			if (out_lpf[c_buff] > s_peak) s_peak = out_lpf[c_buff];
			
			out_Q_ch = process_dynamic_gain(out_lpf[c_buff], &rx_agc, &rx_env, &rx_gain);// Ограничение 10бит
			//if (out_Q_ch > s_peak) s_peak = out_Q_ch;
			TIM3->CCR4 = ((uint16_t)(out_Q_ch+512))>>2;
		}
		else{
			// Если передача
			//in_lpf[c_buff] = in_Q_up; 
			//p_data[1];
			s_abs = in_Q_up;
			//s_abs = out_lpf[c_buff];
			if (s_abs < 0) s_abs = -s_abs;
			if (s_abs > s_peak) s_peak = s_abs;
			//s_abs = process_dynamic_gain(in_Q_up, &tx_comp, &tx_env, &tx_gain)<<6; // Ограничение 10бит
			in_lpf[c_buff] = process_dynamic_gain(in_Q_up, &tx_comp, &tx_env, &tx_gain)<<6; // Ограничение 10бит
			in_raw_q[c_buff] = (int16_t)__SSAT(out_lpf[c_buff], 16);//
			out_I_ch = delay_arr[c_buff_delay];
	  	delay_arr[c_buff_delay] = (int16_t)__SSAT(out_lpf[c_buff], 16);//	
			out_Q_ch = out_ht_q[c_buff];
			TIM3->CCR1 = (uint16_t)((__SSAT(out_I_ch, 16)+32767)>>6);
	  	TIM2->CCR4 = (uint16_t)((__SSAT(out_Q_ch, 16)+32767)>>6);		
		}		
		

		// Инкримент основных буферов
		c_buff++; 
		if (c_buff >= buff_size){ // если буфер заполнился
			c_buff = 0; // обнуляем
		}	

		// Инкримент задержки Гилберта основных буферов
		c_buff_delay++; 
		if (c_buff_delay >= delay_hil){ // если буфер заполнился
			c_buff_delay = 0; // обнуляем
		}
		// обновляем значения для апскейла
		in_I_up = in_I_ch;
    in_Q_up = in_Q_ch;
		upscale_flag = false;
	}
	else{ // если нет апскейл на два
		in_I_up = (in_I_up + in_I_ch);
    in_Q_up = (in_Q_up + in_Q_ch);
    upscale_flag = true; // поднимаем флаг
	}

	if (!rx_tx_fl){ // Прием
			// Заполняем массив FFT
			if ((c_fft < fft_size)&&(!fft_arr_fl)){ // Если массив fft не заполнен
				complex_in_fft[(c_fft<<1)] = in_Q_up<<2;
				complex_in_fft[(c_fft<<1)+1] = -in_I_up<<2;     // заполняем re
				c_fft++;
			}
			else{
				c_fft = 0;
				fft_arr_fl = true;
			} 	
	} else {//передача	
			// Заполняем массив FFT
			if ((c_fft < fft_size)&&(!fft_arr_fl)){ // Если массив fft не заполнен
				complex_in_fft[(c_fft<<1)] = out_I_ch>>2;
				complex_in_fft[(c_fft<<1)+1] = -out_Q_ch>>2;     // заполняем re
				c_fft++;
			}
			else{
				c_fft = 0;
				fft_arr_fl = true;
			} 		

	}
}



uint8_t Get_Buttons_Code(void) { // Так как пины разбросаны, их нужно собрать в одну тетраду (0..15) программно:
    uint8_t code = 0;
    if (!(GPIOA->IDR & GPIO_IDR_IDR15)) code |= (1 << 0); // PA15 -> Bit 3
    if (!(GPIOB->IDR & GPIO_IDR_IDR3))  code |= (1 << 1); // PB6  -> Bit 2
    if (!(GPIOB->IDR & GPIO_IDR_IDR4))  code |= (1 << 2); // PB4  -> Bit 1
    if (!(GPIOB->IDR & GPIO_IDR_IDR6))  code |= (1 << 3); // PB3  -> Bit 0
    return code; // Вернет значение от 0 до 15
}

void Start_Debounce(void) { // Таймер антидребезга кнопок
    if (!is_debouncing) {
        is_debouncing = 1;
        SysTick->LOAD = (72000000 / 1000) * 20 - 1; // 20мс
        SysTick->VAL = 0;
        SysTick->CTRL = SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_CLKSOURCE_Msk;
    }
}
// Обработчик прерываний нажатия кнопок
void EXTI3_IRQHandler(void)     { EXTI->PR = EXTI_PR_PR3;  Start_Debounce(); }
void EXTI4_IRQHandler(void)     { EXTI->PR = EXTI_PR_PR4;  Start_Debounce(); }
void EXTI9_5_IRQHandler(void)   { EXTI->PR = EXTI_PR_PR6;  Start_Debounce(); }
void EXTI15_10_IRQHandler(void) { EXTI->PR = EXTI_PR_PR15; Start_Debounce(); }

/**
 * @param sample_in - входной отсчет (q15)
 * @param cfg       - указатель на конфиг (rx_agc или tx_comp)
 * @param pEnv      - указатель на переменную огибающей (static)
 * @param pGain     - указатель на переменную усиления (static)
 */
// не забыть сделать инлайн
int16_t process_dynamic_gain(int32_t sample_in, const AGC_Config *cfg, int32_t *pEnv, int32_t *pGain) {
    int32_t abs_s = abs(sample_in);

    // 1. Быстрый пиковый детектор огибающей с защитой от "залипания"
    int32_t env_diff = abs_s - *pEnv;
    if (env_diff > 0) {
        *pEnv += (env_diff >> 3); // Быстрая атака
    } else {
        int32_t release_step = env_diff >> 8;
        // Защита от бесконечного падения: если сдвиг дал 0, но разница еще есть, принудительно вычитаем 1
        if (release_step == 0 && env_diff < 0) release_step = -1;
        *pEnv += release_step;
    }

    // 2. Расчет целевого усиления (оптимизированное деленияе)
    int32_t target_gain = 32768; // 1.0 в Q15
    if (*pEnv > cfg->threshold) {
        // Явно переводим в uint32_t. Беззнаковое деление в библиотеках ARM 
        // компилируется в существенно более короткий и быстрый цикл.
        uint32_t num = (uint32_t)cfg->threshold << 15;
        uint32_t den = (uint32_t)(*pEnv);
        
        target_gain = (int32_t)(num / den);
    }
		
		
    // 3. Плавное сглаживание Gain с гарантированным возвратом к 1.0 (32768)
    int32_t gain_diff = target_gain - *pGain;
    if (gain_diff < 0) {
        *pGain += (gain_diff >> cfg->attack);  // Быстро зажимаем
    } else {
        int32_t gain_step = gain_diff >> cfg->release;
        // Защита от "замерзания": если сдвиг дал 0, но гейн еще не дошел до target_gain, принудительно прибавляем 1
        if (gain_step == 0 && gain_diff > 0) gain_step = 1;
        *pGain += gain_step;
    }
		
    // 4. Применение усиления (Вход * Gain) >> 15
    int32_t out = (sample_in * (*pGain)) >> 15;

    // 5. Аппаратное насыщение (Ограничение) средствами Cortex-M3
    return (int16_t)__SSAT(out, 10);
}

void Button_Process(uint8_t code) { // Обработчик нажатия кнопок
	switch (code) {
		case 0x01: // Кнопка 1 (0001)
			ILI9341_WriteString(   30, 30, "    Button 1", Font_11x18, GREEN, MYFON);
			break;
				
		case 0x02: // Кнопка 2 (0010)
			//Шаг перестройки
		  if(!menu_fl){ // Если не в меню
				step *= 10;
				if (step>1000) step = 1;
				Draw_Step(step);   
			}
		  else{ // Если в меню
				step *= 10;
				if (step>1000) step = 1;
				//Draw_Step(step);   
			}
			break;
				
		case 0x03: // Кнопка 3 (0011)
			ILI9341_WriteString(   30, 30, "    Button 3", Font_11x18, GREEN, MYFON);
			break;
				
		case 0x04: // Кнопка 4 (0100) Band+
			if(!menu_fl){ // Если не в меню
				if (band_idx<8) { band_idx++; } else { band_idx=0; }
		    Refreash_Band();  // Обновляем диапазон
				Refreash_A_B();   // Обновляем A/B VFO
			} 
			else{
				if(!sub_menu_fl){ // Если не в подменю(основное меню)
					ILI9341_WriteString(   46, current_menu*18, MAIN_MENU[current_menu], Font_11x18, BORDERCL, MYFON);    // Неактивный пункт
					if (current_menu<MAIN_MENU_SIZE-1){ current_menu++; }
					else { current_menu=0; }
					ILI9341_WriteString(   46, current_menu*18, MAIN_MENU[current_menu], Font_11x18, GREEN, MYFON);    // Активный пункт
				}
				else{ // Если в подменю
					ILI9341_WriteString(   46, current_submenu*18, ALL_SUB_MENUS[current_menu][current_submenu], Font_11x18, BORDERCL, MYFON);    // Неактивный пункт
					if (current_submenu<SUB_MENU_SIZES[current_menu]-1){ current_submenu++; }
					else { current_submenu=0; }
					ILI9341_WriteString(   46, current_submenu*18, ALL_SUB_MENUS[current_menu][current_submenu], Font_11x18, GREEN, MYFON);    // Активный пункт
				}
			}
			break;
				
		case 0x05: // Кнопка 5 (0101) VFO A/B
			//ILI9341_WriteString(   30, 30, "    Button 5", Font_11x18, GREEN, MYFON);
		  if(!menu_fl){ // Если не в меню
				if (a_b_frec) {                 // A/B
					a_b_frec = false;
				}
				else {
					a_b_frec = true;
				}/**/
				Refreash_A_B(); // Обновляем A/B VFO
			}
			break;
				
		case 0x06: // Кнопка 6 (0110)
			//ILI9341_WriteString(   30, 30, "    Button 6", Font_11x18, GREEN, MYFON);
		  if(!menu_fl){ // Если не в меню

			}
		  else{ // Если в меню кнопка выбора
				if(!sub_menu_fl){ // Если в основном меню
					
					if(current_menu == MAIN_MENU_SIZE-1){ // Если EXIT
						current_menu=0;
					}
					else{
						sub_menu_fl = true; // Входим в подменю
					}
				}
				else{
					if(current_submenu == SUB_MENU_SIZES[current_menu]-1){ // Если Back
						current_submenu=0;
						sub_menu_fl = false; // Входим в меню
					}
				}

					// Отрисовка меню
					ILI9341_Draw_Rectangle(46, 0, 243-46, 240, MYFON);
					if (!sub_menu_fl){// Если в основном меню
						for(uint8_t i=0; i<MAIN_MENU_SIZE; i++){ 
							if (i == current_menu){
								ILI9341_WriteString(   46, i*18, MAIN_MENU[i], Font_11x18, GREEN, MYFON);    // Активный пункт
							} else{
								ILI9341_WriteString(   46, i*18, MAIN_MENU[i], Font_11x18, BORDERCL, MYFON); // Неактивный пункт
							}
						}
					}
					else{ // Если в подменю
						for(uint8_t i=0; i<SUB_MENU_SIZES[current_menu]; i++){ 
							if (i == current_submenu){
								ILI9341_WriteString(   46, i*18, ALL_SUB_MENUS[current_menu][i], Font_11x18, GREEN, MYFON);    // Активный пункт
							} else{
								ILI9341_WriteString(   46, i*18,ALL_SUB_MENUS[current_menu][i], Font_11x18, BORDERCL, MYFON); // Неактивный пункт
							}
							DrawVarMenu(current_menu, i);
						}
					}
			}
			break;
				
		case 0x07: // Кнопка 7 (0111)
			if(!menu_fl){ // Если не в меню
				if (band_idx>0) { band_idx--; } else { band_idx=8; }
		    Refreash_Band();  // Обновляем диапазон
				Refreash_A_B();   // Обновляем A/B VFO
			}
			else{
				if(!sub_menu_fl){ // Если не в подменю(основное меню)
					ILI9341_WriteString(   46, current_menu*18, MAIN_MENU[current_menu], Font_11x18, BORDERCL, MYFON);    // Неактивный пункт
					if (current_menu>0){ current_menu--; }
					else { current_menu=MAIN_MENU_SIZE-1; }
					ILI9341_WriteString(   46, current_menu*18, MAIN_MENU[current_menu], Font_11x18, GREEN, MYFON);    // Активный пункт
				}
				else{ // Если в подменю
					ILI9341_WriteString(   46, current_submenu*18, ALL_SUB_MENUS[current_menu][current_submenu], Font_11x18, BORDERCL, MYFON);    // Неактивный пункт
					if (current_submenu>0){ current_submenu--; }
					else { current_submenu=SUB_MENU_SIZES[current_menu]-1; }
					ILI9341_WriteString(   46, current_submenu*18, ALL_SUB_MENUS[current_menu][current_submenu], Font_11x18, GREEN, MYFON);    // Активный пункт
				}
			}
			break;
				
		case 0x08: // Кнопка 8 (1000)
			// RX/TX
		  if(!menu_fl){ // Если не в меню
				if (!rx_tx_fl) { rx_tx_fl = 1; } else { rx_tx_fl = 0; }
				if (!rx_tx_fl) {                   // RX/TX
					ILI9341_WriteString( 46, 108, "R", Font_16x26, GREEN, MYFON); // RX/TX
					RX_Device_Inint();
				}
				else {
					ILI9341_WriteString( 46, 108, "T", Font_16x26, GREEN, MYFON); // RX/TX
					TX_Device_Inint();
				}
	  	}
			break;

		default:
			// Код не распознан или нажато несколько кнопок сразу
			//char txt_buf[32];               // Буфер символов
	    //format_freq(code, txt_buf); 
	    //ILI9341_WriteString(   30, 30, txt_buf, Font_16x26, GREEN, MYFON);
			break;
	}
}

void Button_LongPress_Process(uint8_t code) { // Обработчик нажатия кнопок при длительном удержании
	switch (code) {
		case 0x01: // Кнопка 1 (0001)
			ILI9341_WriteString(   30, 30, "LongButton 1", Font_11x18, GREEN, MYFON);
			break;
				
		case 0x02: // Кнопка 2 (0010)

			break;
				
		case 0x03: // Кнопка 3 (0011)
			ILI9341_WriteString(   30, 30, "LongButton 3", Font_11x18, GREEN, MYFON);
			break;
				
		case 0x04: // Кнопка 4 (0100)
			//ILI9341_WriteString(   30, 30, "LongButton 4", Font_11x18, GREEN, MYFON);
			break;
				
		case 0x05: // Кнопка 5 (0101)
			//ILI9341_WriteString(   30, 30, "LongButton 5", Font_11x18, GREEN, MYFON);
		  if(!menu_fl){ // Если не в меню
				if (a_b_frec) {                 // A/B
					bands_frec_a[band_idx] = bands_frec_b[band_idx];
				}
				else {
          bands_frec_b[band_idx] = bands_frec_a[band_idx];
				}/**/
				Refreash_A_B(); // Обновляем A/B VFO
			}
			break;
				
		case 0x06: // Кнопка 6 (0110) Вход и выход в меню
			// Переходим в меню настроек
		  if(!menu_fl){
				menu_fl = true;
				// Отрисовка меню
				ILI9341_Draw_Rectangle(46, 0, 243-46, 240, MYFON);
				if (!sub_menu_fl){// Если в основном меню
					for(uint8_t i=0; i<MAIN_MENU_SIZE; i++){ 
						if (i == current_menu){
							ILI9341_WriteString(   46, i*18, MAIN_MENU[i], Font_11x18, GREEN, MYFON);    // Активный пункт
						} else{
							ILI9341_WriteString(   46, i*18, MAIN_MENU[i], Font_11x18, BORDERCL, MYFON); // Неактивный пункт
						}
					}
				}
				else{ // Если в подменю
					for(uint8_t i=0; i<SUB_MENU_SIZES[current_menu]; i++){ 
						if (i == current_submenu){
							ILI9341_WriteString(   46, i*18, ALL_SUB_MENUS[current_menu][i], Font_11x18, GREEN, MYFON);    // Активный пункт
						} else{
							ILI9341_WriteString(   46, i*18,ALL_SUB_MENUS[current_menu][i], Font_11x18, BORDERCL, MYFON); // Неактивный пункт
						}
						DrawVarMenu(current_menu, i);
					}
				}
			}
			else{
				menu_fl = false;
				// Отрисовка основного экрана
				ILI9341_Draw_Rectangle(46, 0, 243-46, 240, MYFON);
				if (!rx_tx_fl) {                   // RX/TX
					ILI9341_WriteString( 46, 108, "R", Font_16x26, GREEN, MYFON); // RX/TX
				}
				else {
					ILI9341_WriteString( 46, 108, "T", Font_16x26, GREEN, MYFON); // RX/TX
				}
				Refreash_Band();  // Обновляем диапазон
				Refreash_A_B(); // Обновляем A/B VFO
			}			
			break;
				
		case 0x07: // Кнопка 7 (0111)
			//ILI9341_WriteString(   30, 30, "LongButton 7", Font_11x18, GREEN, MYFON);
			break;
				
		case 0x08: // Кнопка 8 (1000)
			ILI9341_WriteString(   30, 30, "LongButton 8", Font_11x18, GREEN, MYFON);
			break;

		default:
			// Код не распознан или нажато несколько кнопок сразу
			//char txt_buf[32];               // Буфер символов
	    //format_freq(code, txt_buf); 
	    //ILI9341_WriteString(   30, 30, txt_buf, Font_16x26, GREEN, MYFON);
			break;
	}
}

void DrawVarMenu(uint8_t menu_idx, uint8_t sub_idx){ // Отрисовка переменных в меню
	//ILI9341_Draw_Menu_Var(152, 0, bands_frec_b[band_idx]);
		switch (menu_idx) {
		case 0x00: // Bandwidth
			/*"CW",
			"SSB",
			"AM",
			"FM",
			"Back"*/
			switch (sub_idx) {
			case 0x00: // CW
          ILI9341_Draw_Menu_Var(152, 0*18, bandwidth[0]);
				break;
			case 0x01: // SSB
          ILI9341_Draw_Menu_Var(152, 1*18, bandwidth[1]);
				break;
			case 0x02: // AM
          ILI9341_Draw_Menu_Var(152, 2*18, bandwidth[2]);
				break;
			case 0x03: // FM
          ILI9341_Draw_Menu_Var(152, 3*18, bandwidth[3]);
				break;
			}
			break;
				
		case 0x01: // Calibration
      /*"si5351",
      "Phase",
		  "Balance",
      "Back"*/
			switch (sub_idx) {
			case 0x00: // si5351
          ILI9341_Draw_Menu_Var(152, 0*18, cal_si);
				break;
			case 0x01: // Phase
          ILI9341_Draw_Menu_Var(152, 1*18, cal_fase);
				break;
			case 0x02: // Balance
          ILI9341_Draw_Menu_Var(152, 2*18, cal_balance);
				break;
			}
			break;
				
		case 0x02: // Waterfall
			/*"Range",
      "Pallet",
      "Back"*/
			switch (sub_idx) {
			case 0x00: // Range

				break;
			case 0x01: // Pallet

				break;
			}
			break;
		
		case 0x03: // AGC
			/*"Attack",
      "Release",
      "Back"*/
			switch (sub_idx) {
			case 0x00: // Attack

				break;
			case 0x01: // Release

				break;
			}
			break;
				
		case 0x04: // Mic Limiter
			/*"Attack",
      "Release",
	    "Gain",
      "Back"*/
			switch (sub_idx) {
			case 0x00: // Attack

				break;
			case 0x01: // Release

				break;
			case 0x02: // Gain

				break;
			}
			break;
				
		case 0x05: // Band-Pass Filter
			/*"Filter 1",
      "Filter 2",
	  	"Filter 3",
	  	"Filter 4",
	  	"Filter 5",
      "Back"*/
			switch (sub_idx) {
			case 0x00: // Filter 1
          ILI9341_Draw_Menu_Var(152, 0*18, bandpass_ranges[0]);
				break;
			case 0x01: // Filter 2
          ILI9341_Draw_Menu_Var(152, 1*18, bandpass_ranges[1]);
				break;
			case 0x02: // Filter 3
          ILI9341_Draw_Menu_Var(152, 2*18, bandpass_ranges[2]);
				break;
			case 0x03: // Filter 4
          ILI9341_Draw_Menu_Var(152, 3*18, bandpass_ranges[3]);
				break;
			case 0x04: // Filter 5
          ILI9341_Draw_Menu_Var(152, 4*18, bandpass_ranges[4]);
				break;	
			}
			break;
				
		case 0x06: // Reserved
			/*"Reserved 1",
      "Reserved 2",
      "Back"*/
			switch (sub_idx) {
			case 0x00: // Reserved 1

				break;
			case 0x01: // Reserved 2

				break;
			}
			break;
	}
}
