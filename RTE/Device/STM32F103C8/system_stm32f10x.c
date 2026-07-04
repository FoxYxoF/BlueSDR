#include "stm32f10x.h"

/* Uncomment the following line if you need to relocate your vector Table in Internal SRAM. */ 
/* #define VECT_TAB_SRAM */
#define VECT_TAB_OFFSET  0x0 /*!< Vector Table base offset field. This value must be a multiple of 0x200. */

void SystemInit (void) {
  /* Reset the RCC clock configuration to the default reset state(for debug purpose) */
  RCC->CR |= (uint32_t)0x00000001; 	// Set HSION bit 
  RCC->CFGR &= (uint32_t)0xF8FF0000;	// Reset SW, HPRE, PPRE1, PPRE2, ADCPRE and MCO bits 
  RCC->CR &= (uint32_t)0xFEF6FFFF; 	// Reset HSEON, CSSON and PLLON bits 
  RCC->CR &= (uint32_t)0xFFFBFFFF; 	// Reset HSEBYP bit 
  RCC->CFGR &= (uint32_t)0xFF80FFFF; 	// Reset PLLSRC, PLLXTPRE, PLLMUL and USBPRE/OTGFSPRE bits 
  RCC->CIR = 0x009F0000; 			// Disable all interrupts and clear pending bits 
  SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET; // Vector Table Relocation in Internal FLASH. 
}
