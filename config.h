#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include "main.h"
//#include "ILI9341_GFX.h"


#define FLASH_PAGE_SIZE         1024U          // Размер страницы Flash STM32F103

//// Адреса страниц для  Состояния (State) Мертвые
//#define TRX_STATE_PAGE0         0x0800F000UL
//#define TRX_STATE_PAGE1         0x0800F400UL
//// Адреса страниц для Состояния (State) — Конец 64 КБ
#define TRX_STATE_PAGE0         0x0800F800UL  // Страница 62
#define TRX_STATE_PAGE1         0x0800FC00UL   // Страница 63 (Последняя)

#define TRX_STATE_VERSION       3             // Менять, если структура изменится
#define TRX_STATE_MAGIC         0x54525853UL  // Сигнатура "TRXS"

		
typedef struct                        
{
    uint32_t magic;                   // Сигнатура записи
    uint32_t counter;                 // Инкрементный счетчик записей
    uint16_t version;                 // Версия структуры

    uint32_t vfo_a_freq[9];           // Массив частот VFO A для 9 диапазонов
    uint32_t vfo_b_freq[9];           // Массив частот VFO B для 9 диапазонов

    uint8_t  band_att_pre[9];         // Аттенюатор/УВЧ для 9 диапазонов
  	uint8_t  band_mode_a[9];          // Модуляция VFO A для 9 диапазонов (0:SW 1:LSB 2:USB 3:AM 4:FM)
    uint8_t  band_mode_b[9];          // Модуляция VFO B для 9 диапазонов

    uint8_t  current_band;            // Текущий диапазон
    uint8_t  active_vfo;              // Активный VFO (0=A, 1=B)

    uint16_t tuning_step;             // Шаг перестройки (Hz)
    int16_t  rit_offset;              // Расстройка приемника (Hz)
    int16_t  xit_offset;              // Расстройка передатчика (Hz)

    uint8_t  rit_enabled;             // Флаг включения RIT
    uint8_t  xit_enabled;             // Флаг включения XIT
	
	  uint8_t  volume;                  // Громкость
} trx_state_t;                        // Состояние трансивера

typedef struct                        // Состояние трансивера
{
    uint8_t  volume_enabled;          // Флаг регулировки громкости
	  uint8_t  bandwidth_enabled;       // Флаг регулировки полосы
} trx_state_f;                        // Флаги состояния трансивера


// S зависимось
static const q15_t volume_lut_100[101] = {
    0,     41,    102,   182,   283,   402,   539,   695,   867,   1057,
    1263,  1485,  1721,  1972,  2236,  2514,  2803,  3105,  3417,  3740,
    4072,  4414,  4764,  5122,  5488,  5860,  6239,  6623,  7013,  7407,
    7806,  8209,  8614,  9022,  9433,  9845,  10258, 10672, 11087, 11501,
    11915, 12328, 12740, 13150, 13559, 13965, 14369, 14770, 15167, 15562,
    15953, 16340, 16723, 17102, 17477, 17847, 18212, 18571, 18925, 19273,
    19616, 19952, 20282, 20606, 20923, 21234, 21538, 21835, 22126, 22409,
    22686, 22956, 23219, 23476, 23727, 23971, 24209, 24443, 24671, 24896,
    25117, 25336, 25555, 25774, 26000, 26237, 26494, 26778, 27099, 27467,
    27896, 28399, 28994, 29699, 30536, 31385, 32072, 32484, 32665, 32735,
    32767
};


//  Регулировка уровня
__STATIC_FORCEINLINE q15_t apply_gain(q15_t sample, uint8_t step) {
    return (q15_t)(((int32_t)sample * volume_lut_100[step]) >> 15);
}
void TRX_State_Save(void);  // сохранение текшего состояния трансивера
void TRX_State_Load(void);  // загрузка текшего состояния трансивера

#endif