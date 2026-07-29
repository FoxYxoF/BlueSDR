#ifndef MENU_H
#define MENU_H

#include "ILI9341_GFX.h"


void Menu_Up(void);                                        // Меню переход вверх  
void Menu_Down(void);                                      // Мюшаг вниз
void Menu_Change(void);                                    // Выбор подменю по кнопке
void Menu_Draw(void);                                      // Отрисовка самого меню
void Menu_DrawVar(uint8_t menu_idx, uint8_t sub_idx);      // Отрисовка переменных в меню
void Menu_Enc(uint8_t menu_idx, uint8_t sub_idx);          // Изменение по энкодеру

#endif