#include "config.h"


//  текущее состояние трансивера, оперативное
trx_state_t trx_state =  
{
    .version         = TRX_STATE_VERSION,

    .vfo_a_freq =
    {
        1875000,
        3630000,
        7090000,
        10125000,
        14200000,
        18150000,
        21250000,
        24950000,
        28500000
    },

    .vfo_b_freq =
    {
        1875100,
        3630100,
        7090100,
        10125100,
        14200100,
        18150100,
        21250100,
        24950100,
        28500100
    },

    .band_att_pre =
    {
        1, 1, 0, 0, 0, 0, 0, 0, 0
    },

    .band_mode_a =
    {
        1, 1, 1, 0, 1, 1, 1, 1, 1
    },

    .band_mode_b =
    {
        1, 1, 1, 0, 1, 1, 1, 1, 1
    },

    .current_band  = 2,
    .active_vfo    = 1,

    .tuning_step   = 100,

    .rit_offset    = 0,
    .xit_offset    = 0,

    .rit_enabled   = 0,
    .xit_enabled   = 0,
		
		.volume        = 20           
};

trx_state_f trx_state_flag =  
{
		.volume_enabled = 0,   // Флаг регулировки громкости 
    .bandwidth_enabled = 0   // Флаг регулировки полосы
};

// Указатель на место во Flash, где была найдена последняя рабочая запись
static const trx_state_t* last_valid_flash_state = NULL;

/**
  * @brief  Разблокировка контроллера Flash и сброс ошибок
  */
void Flash_Unlock(void) {
    if ((FLASH->CR & FLASH_CR_LOCK) != 0) {
        FLASH->KEYR = 0x45670123UL; 
        FLASH->KEYR = 0xCDEF89ABUL;
    }
    FLASH->SR |= FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
}

/**
  * @brief  Блокировка контроллера Flash
  */
void Flash_Lock(void) {
    FLASH->CR |= FLASH_CR_LOCK;
}

/**
  * @brief  Стирание одной страницы Flash
  */
void Flash_ErasePage(uint32_t page_addr) {
    while ((FLASH->SR & FLASH_SR_BSY) != 0); 
    FLASH->CR |= FLASH_CR_PER;               
    FLASH->AR = page_addr;                   
    FLASH->CR |= FLASH_CR_STRT;              
    while ((FLASH->SR & FLASH_SR_BSY) != 0); 
    FLASH->CR &= ~FLASH_CR_PER;              
}

/**
  * @brief  Быстрая запись во Flash с контролем успешности "на лету"
  */
bool Flash_WriteStruct(uint32_t dest_addr, const trx_state_t* src) {
    FLASH->CR |= FLASH_CR_PG; 
    
    uint16_t *src_ptr = (uint16_t*)src;
    volatile uint16_t *dest_ptr = (volatile uint16_t*)dest_addr;
    uint32_t halfwords = (sizeof(trx_state_t) + 1) / 2;
    bool success = true;
    
    for (uint32_t i = 0; i < halfwords; i++) {
        while ((FLASH->SR & FLASH_SR_BSY) != 0);
        dest_ptr[i] = src_ptr[i]; 
		  	while (FLASH->SR & FLASH_SR_BSY);
        
        // Мгновенная верификация записанного полуслова
        if (dest_ptr[i] != src_ptr[i]) {
            success = false; 
            break; 
        }
    }
    
    while ((FLASH->SR & FLASH_SR_BSY) != 0);
    FLASH->CR &= ~FLASH_CR_PG;
    
    return success;
}

/**
  * @brief  Сохранение состояния. Вызывается ИСКЛЮЧИТЕЛЬНО из PVD_IRQHandler.
  */
void TRX_State_Save(void) {
    uint32_t target_addr = 0;
    
    trx_state.magic = TRX_STATE_MAGIC;
    trx_state.version = TRX_STATE_VERSION;
    
    if (last_valid_flash_state != NULL) {
        trx_state.counter = last_valid_flash_state->counter + 1;
        
        uint32_t current_page = (uint32_t)last_valid_flash_state & ~(FLASH_PAGE_SIZE - 1);
        uint32_t next_record_addr = (uint32_t)last_valid_flash_state + sizeof(trx_state_t);
        
        if (next_record_addr + sizeof(trx_state_t) <= current_page + FLASH_PAGE_SIZE) {
            target_addr = next_record_addr;
        } else {
            target_addr = (current_page == TRX_STATE_PAGE0) ? TRX_STATE_PAGE1 : TRX_STATE_PAGE0;
        }
    } else {
        trx_state.counter = 1;
        target_addr = TRX_STATE_PAGE0;
    }
    
    if (target_addr != 0) {
        // Разблокируем Flash, пишем и НЕ тратим время на Flash_Lock(), так как МК сейчас отключится
        Flash_Unlock();
        if (Flash_WriteStruct(target_addr, &trx_state)) {
            last_valid_flash_state = (const trx_state_t*)target_addr;
        }
    }
}

/**
  * @brief  Загрузка состояния при старте МК + безопасный перенос страниц.
  */
void TRX_State_Load(void) {
    const trx_state_t* best_record = NULL;
    uint32_t max_counter = 0;
    uint32_t pages[2] = {TRX_STATE_PAGE0, TRX_STATE_PAGE1};
    
    FLASH->SR |= FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
    
    // 1. Линейный обход страниц
    for (int p = 0; p < 2; p++) {
        uint32_t page_end = pages[p] + FLASH_PAGE_SIZE;
        
        for (uint32_t addr = pages[p]; addr + sizeof(trx_state_t) <= page_end; addr += sizeof(trx_state_t)) {
            const trx_state_t* record = (const trx_state_t*)addr;
            
            if (record->magic == TRX_STATE_MAGIC && record->version == TRX_STATE_VERSION) {
                if (record->counter > max_counter) {
                    max_counter = record->counter;
                    best_record = record;
                }
            }
        }
    }
    
    // 2. Копируем в RAM, если нашли
    if (best_record != NULL) {
        trx_state = *best_record;
        last_valid_flash_state = best_record;
    } else {
        // Дефолтные настройки для чистой памяти
        // уже есть ничего не делаем выходим
        return; 
    }
    
    // 3. Обслуживание кольцевого буфера при старте
    uint32_t current_page = (uint32_t)last_valid_flash_state & ~(FLASH_PAGE_SIZE - 1);
    uint32_t next_record_addr = (uint32_t)last_valid_flash_state + sizeof(trx_state_t);
    
    if (next_record_addr + sizeof(trx_state_t) > current_page + FLASH_PAGE_SIZE) {
        uint32_t other_page = (current_page == TRX_STATE_PAGE0) ? TRX_STATE_PAGE1 : TRX_STATE_PAGE0;
        
        Flash_Unlock();
        Flash_ErasePage(other_page);
        
        // ПУНКТ 2: Двойная проверка — сначала проверяем успешность выполнения функции записи
        if (Flash_WriteStruct(other_page, &trx_state)) {
            const trx_state_t *rec = (const trx_state_t *)other_page;
            
            // Затем физически верифицируем критические заголовки в самой памяти Flash
            if (rec->magic == TRX_STATE_MAGIC && 
                rec->version == TRX_STATE_VERSION && 
                rec->counter == trx_state.counter) 
            {
                Flash_ErasePage(current_page);
                last_valid_flash_state = rec;
            }
        }
        
        // ПУНКТ 1: Обязательно блокируем Flash. Программа переходит к основному циклу
        Flash_Lock();
    } else {
        // Если перенос страниц не требовался, всё равно принудительно закрываем Flash
        Flash_Lock();
    }
}