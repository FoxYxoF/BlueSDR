

#ifndef ILI9341_GFX_H
#define  ILI9341_GFX_H

#include "main.h"
#include "fonts.h"

////////// DISPL ////////////
//extern SPI_HandleTypeDef hspi1;
//#define DISP_SPI_PTR     &hspi1
#define DISP_SPI         SPI2
#define SPI1_DR_8bit  (*(__IO uint8_t *)((uint32_t)&(SPI2->DR))) 

////////////////////////////////////// РЅР°СЃС‚СЂРѕР№РєР° РїРёРЅРѕРІ /////////////////////////////////////
// С‡РёРї СЃРµР»РµРєС‚ CS Р°РєС‚РёРІРЅС‹Р№ СѓСЂРѕРІРµРЅСЊ РЅРёР·РєРёР№
#define DISP_CS_SELECT      GPIOA->BRR = GPIO_BSRR_BS12 
#define DISP_CS_UNSELECT    GPIOA->BSRR = GPIO_BSRR_BS12  

#define DISP_DC_DATA        GPIOA->BSRR = GPIO_BSRR_BS11  
#define DISP_DC_CMD         GPIOA->BRR = GPIO_BSRR_BS11  

#define DISP_RST_RESET      GPIOA->BRR = GPIO_BSRR_BS10  
#define DISP_RST_WORK       GPIOA->BSRR = GPIO_BSRR_BS10  

///////////////// С€РёСЂРёРЅР° РІС‹СЃРѕС‚Р° ///////////////////
#define ILI9341_SCREEN_WIDTH 	320
#define ILI9341_SCREEN_HEIGHT   240

// РєРѕРјР°РЅРґС‹
#define ILI9341_VSCRSADD 0x37 ///< Vertical Scrolling Start Address
#define ILI9341_VSCRDEF 0x33  ///< Vertical Scrolling Definition

// СЂР°Р·Р»РёС‡РЅС‹Рµ С†РІРµС‚Р°, СЃРѕР·РґР°С‚СЊ РЅСѓР¶РЅС‹Р№ РјРѕР¶РЅРѕ Р·РґРµСЃСЊ https://trolsoft.ru/ru/articles/rgb565-color-picker
//                     -----______-----
#define MYFON        0b0000000000000000  // #------
#define TEXTCL       0b0000011111100000  // #------
#define BORDERCL     0b0000010000000000  // #------
#define BLACK       0x0000
#define NAVY        0x000F
#define DARKGREEN   0x03E0
#define DARKCYAN    0x03EF
#define MAROON      0x7800
#define PURPLE      0x780F
#define OLIVE       0x7BE0
#define LIGHTGREY   0xC618
#define DARKGREY    0x7BEF
#define BLUE        0x001F
#define GREEN       0x07E0
#define CYAN        0x07FF
#define RED         0xF800
#define MAGENTA     0xF81F
#define YELLOW      0xFFE0
#define WHITE       0xFFFF
#define ORANGE      0xFD20
#define GREENYELLOW 0xAFE5
#define PINK        0xF81F

/////////////////////////////////////////////////////////
#define SCREEN_VERTICAL_1		0
#define SCREEN_HORIZONTAL_1		1
#define SCREEN_VERTICAL_2		2
#define SCREEN_HORIZONTAL_2		3

/////////////////////////////////////////////////////////
//void ILI9341_SPI_Init(void);
//void ILI9341_SPI_Send(unsigned char SPI_Data);
void delay_us(uint32_t us);
void ILI9341_Write_Command(uint8_t Command);
void ILI9341_Write_Data(uint8_t Data);
void ILI9341_Set_Address(uint16_t X1, uint16_t Y1, uint16_t X2, uint16_t Y2);
void ILI9341_Reset(void);
void ILI9341_Set_Rotation(uint8_t Rotation);
void ILI9341_Init(void);
void ILI9341_Fill_Screen(uint16_t Colour);
void ILI9341_Draw_Colour(uint16_t Colour);
void ILI9341_Draw_Pixel(uint16_t X, uint16_t Y, uint16_t Colour);
void ILI9341_Draw_Colour_Burst(uint16_t Colour, uint32_t Size);
void ILI9341_Scroll_To(uint16_t Y);
void ILI9341_Set_Scroll_Margins(uint16_t Top, uint16_t Bottom);

void ILI9341_Draw_Rectangle(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height, uint16_t Colour);
void ILI9341_Draw_Horizontal_Line(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Colour);
// РѕС‚СЂРёСЃРѕРІС‹РІР°РµРј РёРЅС‚РµСЂС„РµР№СЃ
void apply_smoothing(q15_t* new_mag); // СѓСЃСЂРµРґРЅРµРЅРёРµ РІРѕРґРѕРїР°РґР°
void ILI9341_Draw_Waterfall(uint16_t* data); // РІРѕРґРѕРїР°Рґ FFT
void ILI9341_num18x34(uint16_t x, uint16_t y, uint8_t ch, uint16_t color, uint16_t bgcolor);
void ILI9341_Draw_MainFrec(uint16_t x, uint16_t y, uint32_t freq); // Р’С‹РІРѕРґРёРј РѕСЃРЅРѕРІРЅСѓСЋ С‡Р°СЃС‚РѕС‚Сѓ
void ILI9341_Draw_Frec11x18(uint16_t x, uint16_t y, uint32_t freq);  // СЂРёСЃСѓРµРј С‡Р°СЃС‚РѕС‚Сѓ РјР°Р»РµРЅСЊРєРёРј С€СЂРёС„С‚РѕРј
void format_freq(uint32_t f, char *out); // Р§РёСЃР»Рѕ РІ С‡Р°СЃС‚РѕС‚Сѓ
void format_var(int32_t f, char *out); // Р§РёСЃР»Рѕ РІ СЃС‚СЂРѕРєСѓ
void ILI9341_Draw_Menu_Var(uint16_t x, uint16_t y, uint8_t leng, int32_t var); // Р РёСЃСѓРµРј РїРµСЂРµРјРµРЅРЅС‹Рµ РІ РјРµРЅСЋ
void ILI9341_Draw_Scale();// СЂРёСЃСѓРµРј С€РєР°Р»Сѓ СЂСЏРґРѕРј СЃ РІРѕРґРѕРїР°РґРѕРј
void Draw_SMeter_Labels(int16_t analog_gain_db, uint16_t bar_right_x); // Р РёСЃСѓРµРј С€РєР°Р»Сѓ s-РјРµС‚СЂР°
void ILI9341_Draw_Smetr(q15_t value); // РІРѕРґРѕРїР°Рґ РїРѕР»РѕСЃСѓ s-РјРµС‚СЂР°

void ILI9341_Draw_Vertical_Line(uint16_t X, uint16_t Y, uint16_t Height, uint16_t Colour);
void ILI9341_Draw_Hollow_Circle(uint16_t X, uint16_t Y, uint16_t Radius, uint16_t Colour);
void ILI9341_Draw_Filled_Circle(uint16_t X, uint16_t Y, uint16_t Radius, uint16_t Colour);
void ILI9341_Draw_Hollow_Rectangle_Coord(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1, uint16_t Colour);
void ILI9341_Draw_Filled_Rectangle_Coord(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1, uint16_t Colour);
//void ILI9341_Draw_Char(char character, uint16_t X, uint16_t Y, uint16_t colour, uint16_t size, uint16_t background_colour);
void ILI9341_WriteChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor);
void ILI9341_Draw_Text(const char* text, uint16_t X, uint16_t Y, uint16_t colour, uint16_t size, uint16_t background_Colour);
void ILI9341_Draw_Filled_Rectangle_Size_Text(uint16_t X0, uint16_t Y0, uint16_t Size_X, uint16_t Size_Y, uint16_t Colour);

//USING CONVERTER: http://www.digole.com/tools/PicturetoC_Hex_converter.php
//65K colour (2Bytes / Pixel)
void ILI9341_Draw_Image(const char *image_array, uint16_t x_coordinat, uint16_t y_coordinat, uint16_t img_width, uint16_t img_height, uint32_t s_img);

void ILI9341_WriteString(uint16_t x, uint16_t y, const char* str, FontDef font, uint16_t color, uint16_t bgcolor);

void ILI9341_Random_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);

#endif
