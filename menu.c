#include "menu.h"


extern volatile bool rx_tx_fl;                           // Прием(0) передача(1)

extern int16_t      cal_si;                              // Калибровка si
extern int16_t      cal_fase;                            // Калибровка фазы
extern int16_t      cal_balance;                         // Калибровка баланса фаз

extern uint32_t     bandpass_ranges[5];                  // Диапазоны полосового фильтра и фнч
extern uint16_t     bandwidth_tx[5];                        // Полосы фильтра зч под индексы модуляции 0:cw, 1:ssb, 2:am, 3:fm 
extern trx_state_t  trx_state;                           // Состояние трансивера
extern bool         menu_fl;                             // Флаг входа в основное меню
extern AGC_Config   rx_agc;                              // Настройки компрессора на прием
extern AGC_Config   tx_comp;                             // Настройки компрессора на передачу
//extern biquad4_state_t lpf_filter_I;                      // ФНЧ перед АЦП
//extern biquad4_state_t lpf_filter_Q;                      // ФНЧ перед АЦП

//extern uint8_t      lpf_new;                             // Флаг готовности коэфициентов ФНЧ
//extern uint8_t      lpf_stages;                          // Порядок БИХ ФНЧ биквада
///////////////////////////////// меню настроек ///////////////////////////////////////////
bool       menu_fl = 0;                                  // Флаг фхода в меню настроек
bool       sub_menu_fl = 0;                              // Флаг фхода в подменю
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
    "LSB",
	  "USB",
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
	  "Reserved",
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
	  "Treshold",
    "Back"
};
const uint8_t SUB_3_SIZE = sizeof(SUB_3) / sizeof(SUB_3[0]);

// 4. Подменю "Mic Limiter"
const char* SUB_4[] = {
    "Attack",
    "Release",
	  "Treshold",
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

// Компрессор
// Единый массив для перевода сдвига (shift) в строку
const char* const agc_time_strs[] = {
    " 0.05 ms",  // shift = 0  (Минимум для Атаки)
    "  0.1 ms",   // shift = 1  (tx_comp attack)
    "  0.2 ms",   // shift = 2
    "  0.4 ms",   // shift = 3  (rx_agc attack)
    "  0.8 ms",   // shift = 4  (Минимум для Релиза)
    "  1.5 ms",   // shift = 5  (Максимум для Атаки)
    "  3.0 ms",   // shift = 6
    "  6.0 ms",   // shift = 7
    " 12.0 ms",  // shift = 8  (rx_agc release)
    " 24.0 ms",  // shift = 9
    " 48.0 ms",  // shift = 10
    " 96.0 ms",  // shift = 11
    "192.0 ms", // shift = 12 (tx_comp release)
    "384.0 ms", // shift = 13
    "768.0 ms", // shift = 14
    " 1.5 sec"   // shift = 15 (Максимум для Релиза)
};

// Инициализация переменных навигации (начинаем с Главного меню, 0-й строки)
volatile uint8_t current_menu = 0;
volatile uint8_t current_submenu = 0;


void Menu_Up(void){   // Меню шаг вверх  
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

void Menu_Down(void){    // Меню шаг вниз
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

void Menu_Change(void){  // Выбор подменю по кнопке
	if(!sub_menu_fl){      // Если в основном меню
		
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
}
	
void Menu_Draw(void){
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
			Menu_DrawVar(current_menu, i); // Отрисовываем значения переменных
		}
	}
	Redraw_Step(trx_state.tuning_step, menu_fl);		// перерисовали шаг
}

void Menu_DrawVar(uint8_t menu_idx, uint8_t sub_idx){ // Отрисовка переменных в меню
		switch (menu_idx) {
		case 0x00: // Bandwidth
			/*"CW",
			"LSB",
		  "USB",
			"AM",
			"FM",
			"Back"*/
			switch (sub_idx) {
			case 0x00: // CW
          ILI9341_Draw_Menu_Var(152, 0*18, 4, bandwidth_tx[0]);
				break;
			case 0x01: // LSB
          ILI9341_Draw_Menu_Var(152, 1*18, 4, bandwidth_tx[1]);
				break;
			case 0x02: // USB
          ILI9341_Draw_Menu_Var(152, 2*18, 4, bandwidth_tx[2]);
				break;
			case 0x03: // AM
          ILI9341_Draw_Menu_Var(152, 3*18, 4, bandwidth_tx[3]);
				break;
			case 0x04: // FM
          ILI9341_Draw_Menu_Var(152, 4*18, 4, bandwidth_tx[4]);
				break;
			}
			break;
				
		case 0x01: // Calibration
      /*"si5351",
      "Phase",
		  "Balance",
		  "Reserved",
      "Back"*/
			switch (sub_idx) {
			case 0x00: // si5351
          ILI9341_Draw_Menu_Var(152, 0*18, 6, cal_si);
				break;
			case 0x01: // Phase
          ILI9341_Draw_Menu_Var(152, 1*18, 6, cal_fase);
				break;
			case 0x02: // Balance
          ILI9341_Draw_Menu_Var(152, 2*18, 6, cal_balance); 
				break;
			case 0x03: // Reserved
//          ILI9341_Draw_Menu_Var(152, 3*18, cal_auto_fl); 
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
		  "Treshold",
      "Back"*/
			switch (sub_idx) {
			case 0x00: // Attack
          ILI9341_WriteString( 152, 0*18, agc_time_strs[rx_agc.attack], Font_11x18, GREEN, MYFON); 
				break;
			case 0x01: // Release
          ILI9341_WriteString( 152, 1*18, agc_time_strs[rx_agc.release], Font_11x18, GREEN, MYFON); 
				break;
			case 0x02: // Treshold
          ILI9341_Draw_Menu_Var(152, 2*18, 2, rx_agc.threshold_bits);
				break;
			}
			break;
				
		case 0x04: // Mic Limiter
			/*"Attack",
      "Release",
	    "Treshold",
      "Back"*/
			switch (sub_idx) {
			case 0x00: // Attack
          ILI9341_WriteString( 152, 0*18, agc_time_strs[tx_comp.attack], Font_11x18, GREEN, MYFON); 
				break;
			case 0x01: // Release
          ILI9341_WriteString( 152, 1*18, agc_time_strs[tx_comp.release], Font_11x18, GREEN, MYFON); 
				break;
			case 0x02: // Treshold
          ILI9341_Draw_Menu_Var(152, 2*18, 2, tx_comp.threshold_bits);
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
          ILI9341_Draw_Menu_Var(152, 0*18, 8, bandpass_ranges[0]);
				break;
			case 0x01: // Filter 2
          ILI9341_Draw_Menu_Var(152, 1*18, 8, bandpass_ranges[1]);
				break;
			case 0x02: // Filter 3
          ILI9341_Draw_Menu_Var(152, 2*18, 8, bandpass_ranges[2]);
				break;
			case 0x03: // Filter 4
          ILI9341_Draw_Menu_Var(152, 3*18, 8, bandpass_ranges[3]);
				break;
			case 0x04: // Filter 5
          ILI9341_Draw_Menu_Var(152, 4*18, 8, bandpass_ranges[4]);
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

void Menu_Enc(uint8_t menu_idx, uint8_t sub_idx){ // Изменение по энкодеру
	//ILI9341_Draw_Menu_Var(152, 0, bands_frec_b[trx_state.current_band]);
	int32_t tmp;
		switch (menu_idx) {
		case 0x00: // Bandwidth
			/*"CW",
			"LSB",
		  "USB",
			"AM",
			"FM",
			"Back"*/
			switch (sub_idx) {
			case 0x00: // CW
				tmp = bandwidth_tx[0];
				if (Process_Encoder(&tmp, 10, 0, 5000)){ //
          bandwidth_tx[0] = tmp;
					//lpf_new = Calculate_lpf_Q31(bandwidth_rx[0], 21875.0f, &lpf_stages); // Установка полосы пропускания
					//Calculate_Biquad4_Butterworth(bandwidth_rx[0], 42682.0f, &lpf_filter_I);// Коэфициенты для самописного биквада ФНЧ Баттерворта
          //Calculate_Biquad4_Butterworth(bandwidth_rx[0], 42682.0f, &lpf_filter_Q);
          ILI9341_Draw_Menu_Var(152, 0*18, 4, bandwidth_tx[0]);
				}
				break;
			case 0x01: // LSB
				tmp = bandwidth_tx[1];
				if (Process_Encoder(&tmp, 10, 0, 5000)){ //
          bandwidth_tx[1] = tmp;
          //lpf_new = Calculate_lpf_Q31(bandwidth_rx[1], 10671.0f, &lpf_stages); // Установка полосы пропускания
					//Calculate_Biquad4_Butterworth(bandwidth_rx[1], 42682.0f, &lpf_filter_I);// Коэфициенты для самописного биквада ФНЧ Баттерворта
          //Calculate_Biquad4_Butterworth(bandwidth_rx[1], 42682.0f, &lpf_filter_Q);
          ILI9341_Draw_Menu_Var(152, 1*18, 4, bandwidth_tx[1]);
				}
				break;
			case 0x02: // USB
				tmp = bandwidth_tx[2];
				if (Process_Encoder(&tmp, 10, 0, 5000)){ //
          bandwidth_tx[2] = tmp;	
					//lpf_new = Calculate_lpf_Q31(bandwidth_rx[2], 10671.0f, &lpf_stages); // Установка полосы пропускания
					//Calculate_Biquad4_Butterworth(bandwidth_rx[2], 42682.0f, &lpf_filter_I);// Коэфициенты для самописного биквада ФНЧ Баттерворта
          //Calculate_Biquad4_Butterworth(bandwidth_rx[2], 42682.0f, &lpf_filter_Q);
          ILI9341_Draw_Menu_Var(152, 2*18, 4, bandwidth_tx[2]);
				}
				break;
			case 0x03: // AM
				tmp = bandwidth_tx[3];
				if (Process_Encoder(&tmp, 10, 0, 5000)){ //
          bandwidth_tx[3] = tmp;
					//lpf_new = Calculate_lpf_Q31(bandwidth_rx[3], 21875.0f, &lpf_stages); // Установка полосы пропускания
					//Calculate_Biquad4_Butterworth(bandwidth_rx[3], 42682.0f, &lpf_filter_I);// Коэфициенты для самописного биквада ФНЧ Баттерворта
          //Calculate_Biquad4_Butterworth(bandwidth_rx[3], 42682.0f, &lpf_filter_Q);
          ILI9341_Draw_Menu_Var(152, 3*18, 4, bandwidth_tx[3]);
				}
				break;
			case 0x04: // FM
				tmp = bandwidth_tx[4];
				if (Process_Encoder(&tmp, 10, 0, 5000)){ //
          bandwidth_tx[4] = tmp;
					//lpf_new = Calculate_lpf_Q31(bandwidth_rx[4], 21875.0f, &lpf_stages); // Установка полосы пропускания
					//Calculate_Biquad4_Butterworth(bandwidth_rx[4], 42682.0f, &lpf_filter_I);// Коэфициенты для самописного биквада ФНЧ Баттерворта
          //Calculate_Biquad4_Butterworth(bandwidth_rx[4], 42682.0f, &lpf_filter_Q);
          ILI9341_Draw_Menu_Var(152, 4*18, 4, bandwidth_tx[4]);
				}
				break;
			}
			break;
				
		case 0x01: // Calibration
      /*"si5351",
      "Phase",
		  "Balance",
	  	"Reserved",
      "Back"*/
			switch (sub_idx) {
			case 0x00: // si5351
				tmp = cal_si;
				if (Process_Encoder(&tmp, trx_state.tuning_step, -32000, 32000)){ //
          cal_si = tmp;
					ILI9341_Draw_Menu_Var(152, 0*18, 6, cal_si);
					si5351_Init(cal_si);
					if (!trx_state.active_vfo) { si5351_SetFrec(trx_state.vfo_a_freq[trx_state.current_band]<<2); }
					else { si5351_SetFrec(trx_state.vfo_b_freq[trx_state.current_band]<<2); }
				}
				break;
			case 0x01: // Phase
				tmp = cal_fase;
				if (Process_Encoder(&tmp, trx_state.tuning_step, -32000, 32000)){ //
          cal_fase = tmp;
					ILI9341_Draw_Menu_Var(152, 1*18, 6, cal_fase);		
				}
				break;
			case 0x02: // Balance
				tmp = cal_balance;
				if (Process_Encoder(&tmp, trx_state.tuning_step, -32000, 32000)){ //
					cal_balance = tmp;
          ILI9341_Draw_Menu_Var(152, 2*18, 6, cal_balance);
				}
			case 0x03: // Reserved
//				tmp = cal_auto_fl;
//				if (Process_Encoder(&tmp, 1, 0, 1)){ //
//					cal_auto_fl = tmp;
//          ILI9341_Draw_Menu_Var(152, 3*18, 1, cal_auto_fl);
//				}
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
		  "Treshold",
      "Back"*/
			switch (sub_idx) {
			case 0x00: // Attack
					tmp = rx_agc.attack;
					if (Process_Encoder(&tmp, 1, 0, 15)){ // Если энкодер крутили
						rx_agc.attack = tmp;
						ILI9341_WriteString( 152, 0*18, agc_time_strs[rx_agc.attack], Font_11x18, GREEN, MYFON); 
			  	}
				break;
			case 0x01: // Release
					tmp = rx_agc.release;
					if (Process_Encoder(&tmp, 1, 0, 15)){ // Если энкодер крутили
						rx_agc.release = tmp;
            ILI9341_WriteString( 152, 1*18, agc_time_strs[rx_agc.release], Font_11x18, GREEN, MYFON); 
					}
				break;
			case 0x02: // Treshold
					tmp = rx_agc.threshold_bits;
					if (Process_Encoder(&tmp, 1, 0, 15)){ // Если энкодер крутили
						rx_agc.threshold_bits = tmp;
            ILI9341_Draw_Menu_Var(152, 2*18, 2, rx_agc.threshold_bits);
					}
				break;
			}
			break;
				
		case 0x04: // Mic Limiter
			/*"Attack",
      "Release",
	    "Treshold",
      "Back"*/
			switch (sub_idx) {
			case 0x00: // Attack
					tmp = tx_comp.attack;
					if (Process_Encoder(&tmp, 1, 0, 15)){ // Если энкодер крутили
						tx_comp.attack = tmp;
            ILI9341_WriteString( 152, 0*18, agc_time_strs[tx_comp.attack], Font_11x18, GREEN, MYFON); 
					}
				break;
			case 0x01: // Release
					tmp = tx_comp.release;
					if (Process_Encoder(&tmp, 1, 0, 15)){ // Если энкодер крутили
						tx_comp.release = tmp;
            ILI9341_WriteString( 152, 1*18, agc_time_strs[tx_comp.release], Font_11x18, GREEN, MYFON); 
					}
				break;
			case 0x02: // Treshold
					tmp = tx_comp.threshold_bits;
					if (Process_Encoder(&tmp, 1, 0, 15)){ // Если энкодер крутили
						tx_comp.threshold_bits = tmp;
            ILI9341_Draw_Menu_Var(152, 2*18, 2, tx_comp.threshold_bits);
					}
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
          //ILI9341_Draw_Menu_Var(152, 0*18, 8, bandpass_ranges[0]);
				break;
			case 0x01: // Filter 2
          //ILI9341_Draw_Menu_Var(152, 1*18, 8, bandpass_ranges[1]);
				break;
			case 0x02: // Filter 3
          //ILI9341_Draw_Menu_Var(152, 2*18, 8, bandpass_ranges[2]);
				break;
			case 0x03: // Filter 4
          //ILI9341_Draw_Menu_Var(152, 3*18, 8, bandpass_ranges[3]);
				break;
			case 0x04: // Filter 5
          //ILI9341_Draw_Menu_Var(152, 4*18, 8, bandpass_ranges[4]);
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