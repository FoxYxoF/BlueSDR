/* Векторы прерываний */
#include "it.h"
#include "ILI9341_GFX.h"


//////////////// мусор для проверки
//bool trg = false;
////////////////////// Основная логика и для сохранения /////////////////////////////////////
volatile bool rx_tx_fl = false;                  // Прием(0) передача(1)

int16_t    cal_si = 5213;                // Калибровка si
int16_t    cal_fase = 40;                 // Калибровка фазы
int16_t    cal_balance = 100;              // Калибровка баланса фаз

uint8_t    waterful_gain = 2;            // Усиление водопада биты или 6дб
uint32_t   bandpass_ranges[5] =          // Диапазоны полосового фильтра и фнч
 {2000000, 4000000, 8000000, 16000000, 30000000};

uint16_t   bandwidth[5] =                 // Полосы фильтра зч под индексы модуляции
   {500, 2800, 2800, 5000, 5000};         // 0:cw, 1:lsb, 2:usb, 3:am  4:fm    /*  */  
	 

extern trx_state_t     trx_state;         // Состояние трансивера
extern trx_state_f     trx_state_flag;    // Флаги состояния трансивера
extern bool            menu_fl;           // Флаг входа в меню настроек

///////////////////////////// Буферы  //////////////////////////////////////
uint16_t   adcData[64];                   // Буфер сырых данных с АЦП1 и АЦП2
uint16_t   adc1 = 0;                      // Апскейл на 16/2 с АЦП1 (15бит)
uint16_t   adc2 = 0;                      // Апскейл на 16/2 с АЦП2 (15бит)
q15_t      in_I_ch = 0;                   // Входной сигнал In-phase   (Re) 15бит без постоянной составляющей adc1-16384
q15_t      in_Q_ch = 0;                   // Входной сигнал Quadrature (Im) 15бит без постоянной составляющей adc2-16384
q15_t      out_I_ch = 0;                  // Выдной сигнал 
q15_t      out_Q_ch = 1;                  // Выдной сигнал 
q15_t      in_I_up = 0;                   // Входной сигнал апскей на два
q15_t      in_Q_up = 0;                   // Входной сигнал апскей на два
///////////////// CIC фильтр ////////////////////////////////////////////////////////////
static int32_t i_i1 = 0, i_i2 = 0, i_i3 = 0; // Секция интеграторов для I
static int32_t i_c1_z1 = 0, i_c2_z1 = 0, i_c3_z1 = 0; // Секция гребенки для I

static int32_t q_i1 = 0, q_i2 = 0, q_i3 = 0; // Секция интеграторов для Q
static int32_t q_c1_z1 = 0, q_c2_z1 = 0, q_c3_z1 = 0; // Секция гребенки для Q

// Для перехода 42-21кгц
static int32_t i_int1 = 0, i_int2 = 0, i_int3 = 0;
static int32_t q_int1 = 0, q_int2 = 0, q_int3 = 0;
// Comb
static int32_t i_d1 = 0, i_d2 = 0, i_d3 = 0;
static int32_t q_d1 = 0, q_d2 = 0, q_d3 = 0;
////////// буферы I и Q и LPF каналов ////////////////////////////////////////////////////////
#define buff_size 256                     // размер буферов
#define buff_size_half 128                // размер половины буферов
q15_t in_raw_i [buff_size];               // входной массив I
q15_t in_raw_q [buff_size];               // входной массив Q
q15_t out_ht_q [buff_size];               // выходной массив Q после Гильберта
q31_t in_lpf   [buff_size];               // входной массив ФНЧ
q31_t out_lpf  [buff_size];               // выходной массив ФНЧ
///////// Коеффициенты филтра Гилбер ///////////////////////////////////////////////////
#define   delay_hil    319                // задержка преобразования Гильберта = размер буфера + (длинна фильтра/2) - 1,  256 + (128/2) - 1 = 319;
q15_t     coeff_hil_q15[n_coeff_hil+1];   // коеффициенты фильтра
q15_t     pstate_hil[n_coeff_hil + block_size_h + 1];// массив состояний
q15_t     delay_arr[delay_hil];           // массив задержки Гилберта
arm_fir_instance_q15 f_hil;
///////////// Модуляция ////////////////////////	
uint8_t	  mode = 3;                       // Модуляция  (0:SW 1:LSB 2:USB 3:AM 4:FM)



//////////// ФНЧ на БИХ /////////////////
// Структура и буферы для ФНЧ
extern arm_biquad_casd_df1_inst_q31 S_LPF;
extern q31_t lpf_coeffs[8 * 5];       // рабочий
extern q31_t lpf_coeffs_new[8 * 5];   // расчетный
extern q31_t lpf_state[8 * 4];        // Состояние фильтра (нужно 4 на одну секцию)
extern volatile uint8_t lpf_new;
extern uint8_t lpf_stages;            // Порядок БИХ ФНЧ биквада

/////////////////////   ФВЧ БИХ 3 порядок ////////////////////////////////
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
#define HPF_SHIFT  3  // ФВЧ
// Глобальные переменные состояния фильтра (обнулить при старте процессора)
static int64_t audio_lpf1 = 0;
static int64_t audio_lpf2 = 0;
static int64_t audio_lpf3 = 0;
// Счетчик для мягкого старта при включении
static uint16_t init_counter = 0;

///////////////////// FFT для водопада ///////////////////////////////////
arm_cfft_instance_q15 cfft;             // Структура состояний для arm_cfft_q15
q15_t complex_in_fft[512];              // Входной массив для БПФ{re, im, re, im...}
uint16_t fft_size = 256;                // Размер массива FFT
/////////////////////	 Динамика(компрессор АРУ) ///////////////////////////
// (масштабирование 9бит)
AGC_Config rx_agc  = {// Настройки компрессора на прием
  .threshold_bits = 9, 
	.attack = 3, 
	.release = 8, 
  .env = 0, 
  .gain = 32768}; 
AGC_Config tx_comp = { // Настройки компрессора на передачу
  .threshold_bits = 9, 
	.attack = 1, 
	.release = 12, 
  .env = 0, 
  .gain = 32768}; 
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
				if (lpf_new) // Если обновились коэфициенты ФНЧ
				{
						lpf_new = 0;
						// Переинициализируем фильтр на новое количество каскадов
						arm_biquad_cascade_df1_init_q31(&S_LPF, lpf_stages, lpf_coeffs_new, lpf_state, 1);
				}
        // Если обработали первую половину буфера
        if ((c_buff >= buff_size_half) && (hil_half_fl)/**/) {
            // 1. Сначала считаем ФНЧ для ПЕРВОЙ половины
            arm_biquad_cascade_df1_q31(&S_LPF, &in_lpf[0], &out_lpf[0], buff_size_half);
					  //arm_fir_fast_q15(&f_hil, &in_raw_q[0], &out_ht_q[0], buff_size_half);
					  fast_hilbert_q15_custom(&f_hil, &in_raw_q[0], &out_ht_q[0], buff_size_half); // Своя реализация Гильберта

            
            hil_half_fl = 0;
        }
        // Если обработали вторую половину буфера
        else if ((c_buff < buff_size_half) && (!hil_half_fl)/**/) {
            // 1. Сначала ФНЧ для ВТОРОЙ половины
            arm_biquad_cascade_df1_q31(&S_LPF, &in_lpf[buff_size_half], &out_lpf[buff_size_half], buff_size_half);
				  	//arm_fir_fast_q15(&f_hil, &in_raw_q[buff_size_half], &out_ht_q[buff_size_half], buff_size_half); 
				  	fast_hilbert_q15_custom(&f_hil, &in_raw_q[buff_size_half], &out_ht_q[buff_size_half], buff_size_half); // Своя реализация Гильберта

            
            hil_half_fl = 1;
        }
    }
}


__STATIC_FORCEINLINE void Process_IQ_Buffer(const uint16_t *p_data, int16_t *out_I, int16_t *out_Q) {
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

void PVD_IRQHandler(void) 
{
    if (EXTI->PR & EXTI_PR_PR16)
    {
        EXTI->PR = EXTI_PR_PR16; // Сбрасываем флаг EXTI

        // Проверяем: питание РЕАЛЬНО упало ниже 2.9V?
        if (PWR->CSR & PWR_CSR_PVDO) 
        {
            __disable_irq(); // Выключаем остальные прерывания, чтобы не мешали

            TRX_State_Save(); // Мгновенно шьем во Flash!

            while (1); // Засыпаем навечно, ждем полной разрядки конденсатора
        }
    }
}

//DC Blocker
//IIR 
//3-stage
// Блокировка постоянной. ФВЧ БИХ 3 порядка (18 dB/окт) 
__STATIC_FORCEINLINE void Process_Audio_HPF(q31_t *sample) {
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



/// ЦОС прерывание от DMA1
__STATIC_FORCEINLINE void Fill_buff(void){	
	// калибровка каналов			
	// 1. Коррекция амплитуды канала I
	// Умножение в формате Q15: (in_I_ch * cal_balance) >> 15. 
	// Затем прибавляем к исходному сигналу и аппаратно ограничиваем до 15 бит.
	int32_t I_cal = (int32_t)in_I_ch + (((int32_t)in_I_ch * cal_balance) >> 15);
	in_I_ch = (q15_t)__SSAT(I_cal, 15);

	// 2. Коррекция фазы канала Q
	// Подмешиваем скорректированный канал I в канал Q и снова жестко насыщаем в 15 бит.
	int32_t Q_cal = (int32_t)in_Q_ch + ((I_cal * cal_fase) >> 15);
	in_Q_ch = (q15_t)__SSAT(Q_cal, 15);
	// --------------------
	// CIC Integrator
	// --------------------
	i_int1 += in_I_ch;
	i_int2 += i_int1;
	i_int3 += i_int2;

	q_int1 += in_Q_ch;
	q_int2 += q_int1;
	q_int3 += q_int2;
	if (upscale_flag){// если передескретизация выполнена
		if (!rx_tx_fl){ // Прием/передача
			// Если прием
			in_raw_q[c_buff] = -in_Q_up; // Отдаем в буфер сырого для преобразования Гильберта
			// Делаем задержку I канала для выравнивания с преобразованием Гильберта
			// Коьцевой буфер размером в задержку
			// Задержка = размер буфера + (длинна фильтра/2) - 1,  256 + (128/2) - 1 = 319;
			out_I_ch = delay_arr[c_buff_delay];
	  	delay_arr[c_buff_delay] = in_I_up;

			// CW
			if(mode == 0){		
				in_lpf[c_buff] = in_I_up + in_Q_up;
			}				
			// LSB
			if(mode == 1){
			  in_lpf[c_buff] = out_ht_q[c_buff] + out_I_ch;
			}
			// USB
			if(mode == 2){
			  in_lpf[c_buff] = out_ht_q[c_buff] - out_I_ch;
			}
      // AM
			if (mode == 3) {
				// Берем модули I и Q каналов без ветвления
				int32_t abs_i = (in_I_up ^ (in_I_up >> 31)) - (in_I_up >> 31);
				int32_t abs_q = (in_Q_up ^ (in_Q_up >> 31)) - (in_Q_up >> 31);
				// Аппроксимация альфа-бета: Max + Min/4
				in_lpf[c_buff] = (abs_i > abs_q) ? (abs_i + (abs_q >> 2)) : (abs_q + (abs_i >> 2));
			}		
			
			Process_Audio_HPF(&in_lpf[c_buff]);		// ФВЧ	
			if (out_lpf[c_buff] > s_peak) s_peak = out_lpf[c_buff];		// Детектор пиков для s-метра	
			out_Q_ch = process_dynamic_gain(out_lpf[c_buff], &rx_agc);// Ограничение 9бит
      int16_t out_rx = apply_gain(out_Q_ch, trx_state.volume); // Регулируем громкость
      out_rx += 512; // переводим значение в положительную область для вывода ШИМ
			TIM3->CCR4 = (uint16_t)out_rx;
		}
		else{
			// Если передача
			//in_lpf[c_buff] = in_Q_up; 
			//p_data[1];
			s_abs = in_Q_up;
			//s_abs = out_lpf[c_buff];
			if (s_abs < 0) s_abs = -s_abs;
			if (s_abs > s_peak) s_peak = s_abs;
			//s_abs = process_dynamic_gain(in_Q_up, &tx_comp)<<6; // Ограничение 10бит
			in_lpf[c_buff] = process_dynamic_gain(in_Q_up, &tx_comp)<<6; // Ограничение 9бит
			in_raw_q[c_buff] = out_lpf[c_buff];   //
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
		//in_I_up = in_I_ch;
    //in_Q_up = in_Q_ch;
		upscale_flag = false;
	}
	else{
		//------------------------
		// CIC3 COMB
		//------------------------
		int32_t t;

		t = i_int3;
		in_I_up = t - i_d1;
		i_d1 = t;

		t = in_I_up;
		in_I_up = t - i_d2;
		i_d2 = t;

		t = in_I_up;
		in_I_up = t - i_d3;
		i_d3 = t;


		t = q_int3;
		in_Q_up = t - q_d1;
		q_d1 = t;

		t = in_Q_up;
		in_Q_up = t - q_d2;
		q_d2 = t;

		t = in_Q_up;
		in_Q_up = t - q_d3;
		q_d3 = t;


		in_I_up >>= 2;
		in_Q_up >>= 2;

		upscale_flag = true;
	}

	if (!rx_tx_fl){ // Прием
			// Заполняем массив FFT
			if ((c_fft < fft_size)&&(!fft_arr_fl)){ // Если массив fft не заполнен
				complex_in_fft[(c_fft<<1)] = in_Q_ch<<2;
				complex_in_fft[(c_fft<<1)+1] = -in_I_ch<<2;     // заполняем re
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
// Компрессор/ару
__STATIC_FORCEINLINE int16_t process_dynamic_gain(int32_t sample_in, AGC_Config *cfg) {
    int32_t abs_s = abs(sample_in);

    // 1. Быстрый пиковый детектор огибающей
    int32_t env_diff = abs_s - cfg->env;
    if (env_diff > 0) {
        uint32_t env_attack_shift = (cfg->attack > 0) ? (cfg->attack - 1) : 0;
        cfg->env += (env_diff >> env_attack_shift); 
    } else {
        uint32_t env_release_shift = (cfg->release > 4) ? (cfg->release - 4) : 1;
        int32_t release_step = env_diff >> env_release_shift;
        if (release_step == 0 && env_diff < 0) release_step = -1;
        cfg->env += release_step;
    }

    // Вычисляем пороги
    int32_t sat_level = 1 << cfg->threshold_bits; 
    int32_t thresh_val = sat_level - (sat_level >> 3);

    // 2. Расчет целевого усиления
    int32_t target_gain = 32768; 
    if (cfg->env > thresh_val) {
        uint32_t num = (uint32_t)thresh_val << 15;
        uint32_t den = (uint32_t)(cfg->env);
        target_gain = (int32_t)(num / den);
    }
		
    // 3. Плавное сглаживание Gain
    int32_t gain_diff = target_gain - cfg->gain;
    if (gain_diff < 0) {
        cfg->gain += (gain_diff >> cfg->attack);  
    } else {
        int32_t gain_step = gain_diff >> cfg->release;
        if (gain_step == 0 && gain_diff > 0) gain_step = 1;
        cfg->gain += gain_step;
    }
		
    // 4. Применение усиления
    int32_t out = (sample_in * cfg->gain) >> 15;

    // 5. Мягкое ограничение
    if (out > (sat_level - 1))  out = sat_level - 1;
    if (out < -sat_level)       out = -sat_level;

    // 6. Масштабирование
    if (cfg->threshold_bits < 9) {
        out <<= (9 - cfg->threshold_bits);
    } else if (cfg->threshold_bits > 9) {
        out >>= (cfg->threshold_bits - 9);
    }

    return (int16_t)out;
}

void Button_Process(uint8_t code) { // Обработчик нажатия кнопок
	switch (code) {
		case 0x01: // Кнопка 1 (0001)
			ILI9341_WriteString(   30, 30, "    Button 1", Font_11x18, GREEN, MYFON);
			break;
				
		case 0x02: // Кнопка 2 (0010)
			//Шаг перестройки
			switch (trx_state.tuning_step)				// Уменьшаем щаг, плашка под символами идет слева на право
			{
					case 1000: trx_state.tuning_step = 100;  break;
					case 100:  trx_state.tuning_step = 10;   break;
					case 10:   trx_state.tuning_step = 1;    break;
					default:   trx_state.tuning_step = 1000; break;
			}
			Redraw_Step(trx_state.tuning_step, menu_fl);   // Рисуем шаг
			break;
				
		case 0x03: // Кнопка 3 (0011) Mode
		  	//Переключем модуляцию
		    if (mode<4){ mode++; }else{ mode=0; }
		    if(trx_state.active_vfo==0){ // Если VFO A
					 trx_state.band_mode_a[trx_state.current_band] = mode;
				}
				if(trx_state.active_vfo==1){ // Если VFO B
					 trx_state.band_mode_b[trx_state.current_band] = mode;
				}
				lpf_new = Calculate_lpf_Q31(bandwidth[mode], 21875.0f, &lpf_stages); // Установка полосы пропускания
				Redraw_mode();        // Перерисовываем модуляцию
			break;
				
		case 0x04: // Кнопка 4 (0100) Band+
			if(!menu_fl){ // Если не в меню
				if (trx_state.current_band<8) { trx_state.current_band++; } else { trx_state.current_band=0; }
		    Redraw_Band();   // Обновляем диапазон
				Redraw_A_B();    // Обновляем A/B VFO
				Set_mode();      // Установка режима модуляции из trx_state
				Redraw_mode();   // Перерисовываем модуляцию
			} 
			else{
        Menu_Up();   // Меню шаг вверх  
			}
			break;
				
		case 0x05: // Кнопка 5 (0101) VFO A/B
			//ILI9341_WriteString(   30, 30, "    Button 5", Font_11x18, GREEN, MYFON);
		  if(!menu_fl){ // Если не в меню
				if (trx_state.active_vfo) {                 // A/B
					trx_state.active_vfo = false;
				}
				else {
					trx_state.active_vfo = true;
				}/**/
				Redraw_A_B(); // Обновляем A/B VFO
				Set_mode();      // Установка режима модуляции
				Redraw_mode();   // Перерисовываем модуляцию
			}
			break;
				
		case 0x06: // Кнопка 6 (0110)
			//ILI9341_WriteString(   30, 30, "    Button 6", Font_11x18, GREEN, MYFON);
		  if(!menu_fl){ // Если не в меню

			}
		  else{ // Если в меню кнопка выбора
        Menu_Change();  // Выбор подменю по кнопке
				Menu_Draw();     // Отрисовка меню
			}
			break;
				
		case 0x07: // Кнопка 7 (0111) Band-
			if(!menu_fl){ // Если не в меню
				if (trx_state.current_band>0) { trx_state.current_band--; } else { trx_state.current_band=8; }
		    Redraw_Band();  // Обновляем диапазон
				Redraw_A_B();   // Обновляем A/B VFO
				Set_mode();      // Установка режима модуляции из trx_state
				Redraw_mode();   // Перерисовываем модуляцию
			}
			else{
        Menu_Down();   // Меню шаг вниз
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
      if (trx_state_flag.volume_enabled){
				trx_state_flag.volume_enabled = false;
			}
			else{
				trx_state_flag.volume_enabled = true;
			}		
			Redraw_volume();
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
				if (trx_state.active_vfo) {                 // B=A
					trx_state.vfo_a_freq[trx_state.current_band] = trx_state.vfo_b_freq[trx_state.current_band];
				}
				else {                                      // A=B
          trx_state.vfo_b_freq[trx_state.current_band] = trx_state.vfo_a_freq[trx_state.current_band];
				}/**/
				Redraw_A_B();    // Обновляем A/B VFO
				Set_mode();      // Установка режима модуляции из trx_state
				Redraw_mode();   // Перерисовываем модуляцию
			}
			break;
				
		case 0x06: // Кнопка 6 (0110) Вход и выход в меню
			// Переходим в меню настроек
		  if(!menu_fl){
				menu_fl = true;
				
        Menu_Draw(); // Отрисовка меню
				//Redraw_Step(trx_state.tuning_step, menu_fl);		// перерисовали шаг
			}
			else{
				menu_fl = false;
        Redraw_Main_Scr();  // Перерисовываем основной экран
				Set_mode();         // Установка режима модуляции
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



