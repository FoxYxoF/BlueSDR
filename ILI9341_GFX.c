

#include "ILI9341_GFX.h"
//#include "w25qxx.h"
//#include "stdlib.h"

#define swap(a, b) { int16_t t = a; a = b; b = t; }

volatile uint16_t LCD_HEIGHT = ILI9341_SCREEN_HEIGHT;
volatile uint16_t LCD_WIDTH	 = ILI9341_SCREEN_WIDTH;

#define START_WRITE 1024

volatile uint8_t spi1_tx_full_flag = 0;
volatile uint8_t spi1_tx_half_flag = 0;
volatile uint8_t spi2_rx_full_flag = 0;
volatile uint8_t spi2_rx_half_flag = 0;

uint8_t dma_spi_fl;             // Флаг окончания передачи через spi
uint8_t buf_disp[832];          // буфер для дисплея
char txt_buf[32];               // Буфер символов
uint8_t inc_wf = 0;             // инкримент водопада
#define K_SMOOTH 10000          // Коэффициент плавности водоавда (0.8 в формате Q15: 0.8 * 32768)
static q15_t smoothed_mag[256]; // массив для усреднения водопада
#define S_SMOOTH 30000          // коэффициент сглаживания сметра
static q15_t smoothed_smetr;    // буфер для усреднения показометра
char old_txt_freq[12] = {0};     // Для ILI9341_Draw_MainFrec храним то, что уже на экране
// ================= калибровка s-meter =================
//#define SMETER_MIN_DB   60   // Текущая отсечка (остается 163 уед. диапазона)
//#define SMETER_SCALE    377  // (240 * 256) / (223 - 60) = 376.9
// Глобальные переменные калибровки S-метра
uint16_t smeter_min_db = 60;  // Порог шума (подбирается руками в меню или коде) "S-Meter Noise Floor"
uint16_t smeter_scale  = 359; // Будет рассчитан автоматически (Q8)

static const uint16_t waterfall_palette_rainbow[256] = {
    0x0000, 0x0002, 0x0004, 0x0006, 0x0009, 0x000B, 0x000D, 0x000F, 0x0011, 0x0014, 0x0016, 0x0018, 0x001A, 0x001D, 0x001F, 0x00BF,
    0x015F, 0x01FF, 0x029F, 0x033F, 0x03DF, 0x047F, 0x051F, 0x05BF, 0x065F, 0x06FF, 0x079F, 0x083F, 0x08DF, 0x097F, 0x0A1F, 0x0ABF,
    0x0B5F, 0x0BFF, 0x0C9F, 0x0D3F, 0x0DDF, 0x0E7F, 0x0F1F, 0x0FBF, 0x105F, 0x10FF, 0x119F, 0x123F, 0x12DF, 0x137F, 0x141F, 0x14BF,
    0x153F, 0x15DF, 0x167F, 0x171F, 0x17BF, 0x17FE, 0x17FC, 0x17F9, 0x17F7, 0x17F5, 0x17F2, 0x17F0, 0x17EE, 0x17EB, 0x17E9, 0x17E7,
    0x17E4, 0x17E2, 0x17E0, 0x1FE0, 0x27E0, 0x2FE0, 0x37E0, 0x3FE0, 0x47E0, 0x4FE0, 0x57E0, 0x5FE0, 0x67E0, 0x6FE0, 0x77E0, 0x7FE0,
    0x87E0, 0x8FE0, 0x97E0, 0x9FE0, 0xA7E0, 0xAFE0, 0xB7E0, 0xBFE0, 0xC7E0, 0xCFE0, 0xD7E0, 0xDFE0, 0xE7E0, 0xEFE0, 0xF7E0, 0xFFE0,
    0xFFC0, 0xFFA0, 0xFF80, 0xFF50, 0xFF30, 0xFF10, 0xFEE0, 0xFEC0, 0xFEA0, 0xFE70, 0xFE50, 0xFE30, 0xFE00, 0xFDE0, 0xFDC0, 0xFD90,
    0xFD70, 0xFD50, 0xFD20, 0xFD00, 0xFCE0, 0xFCB0, 0xFC90, 0xFC70, 0xFC40, 0xFC20, 0xFC00, 0xFBE0, 0xFBC0, 0xFB90, 0xFB70, 0xFB50,
    0xFB20, 0xFB00, 0xFAE0, 0xFAB0, 0xFA90, 0xFA70, 0xFA40, 0xFA20, 0xFA00, 0xF9E0, 0xF9C0, 0xF990, 0xF970, 0xF950, 0xF920, 0xF900,
    0xF8E0, 0xF8B0, 0xF890, 0xF870, 0xF840, 0xF820, 0xF800, 0xF802, 0xF804, 0xF806, 0xF809, 0xF80B, 0xF80D, 0xF80F, 0xF811, 0xF814,
    0xF816, 0xF818, 0xF81A, 0xF81D, 0xF81F, 0xF83F, 0xF85F, 0xF87F, 0xF89F, 0xF8BF, 0xF8DF, 0xF8FF, 0xF91F, 0xF93F, 0xF95F, 0xF97F,
    0xF99F, 0xF9BF, 0xF9DF, 0xF9FF, 0xFA1F, 0xFA3F, 0xFA5F, 0xFA7F, 0xFA9F, 0xFABF, 0xFADF, 0xFAFF, 0xFB1F, 0xFB3F, 0xFB5F, 0xFB7F,
    0xFB9F, 0xFBBF, 0xFBDF, 0xFBFF, 0xFC1F, 0xFC3F, 0xFC5F, 0xFC7F, 0xFC9F, 0xFCBF, 0xFCDF, 0xFCFF, 0xFD1F, 0xFD3F, 0xFD5F, 0xFD7F,
    0xFD9F, 0xFDBF, 0xFDDF, 0xFDFF, 0xFE1F, 0xFE3F, 0xFE5F, 0xFE7F, 0xFE9F, 0xFEBF, 0xFEDF, 0xFEFF, 0xFF1F, 0xFF3F, 0xFF5F, 0xFF7F,
    0xFF9F, 0xFFBF, 0xFFDF, 0xFFFF, 0xFFFE, 0xFFFC, 0xFFF9, 0xFFF7, 0xFFF5, 0xFFF2, 0xFFF0, 0xFFEE, 0xFFEB, 0xFFE9, 0xFFE7, 0xFFE4,
    0xFFE2, 0xFFE0, 0xFFDE, 0xFFDB, 0xFFD9, 0xFFD7, 0xFFD4, 0xFFD2, 0xFFD0, 0xFFCE, 0xFFCB, 0xFFC9, 0xFFC7, 0xFFC4, 0xFFC2, 0xFFC0
};

static const uint16_t waterfall_palette_lava[256] = {
    0x0000, 0x0001, 0x0001, 0x0002, 0x0002, 0x0003, 0x0003, 0x0004,
    0x0004, 0x0005, 0x0005, 0x0006, 0x0006, 0x0007, 0x0007, 0x0008,
    0x0008, 0x0009, 0x0009, 0x000A, 0x000A, 0x000B, 0x000B, 0x000C,
    0x000C, 0x000D, 0x000D, 0x000E, 0x000E, 0x000F, 0x000F, 0x000F,
    0x080F, 0x100E, 0x180E, 0x200E, 0x280D, 0x300D, 0x380D, 0x400C,
    0x480C, 0x500C, 0x580B, 0x600B, 0x680B, 0x700A, 0x780A, 0x800A,
    0x8809, 0x9009, 0x9809, 0xA008, 0xA808, 0xB008, 0xB807, 0xC007,
    0xC807, 0xD006, 0xD806, 0xE006, 0xE805, 0xF005, 0xF805, 0xF804,
    0xF804, 0xF804, 0xF804, 0xF803, 0xF803, 0xF803, 0xF803, 0xF802,
    0xF802, 0xF802, 0xF802, 0xF801, 0xF801, 0xF801, 0xF801, 0xF800,
    0xF800, 0xF800, 0xF800, 0xF800, 0xF820, 0xF840, 0xF840, 0xF860,
    0xF880, 0xF880, 0xF8A0, 0xF8C0, 0xF8C0, 0xF8E0, 0xF900, 0xF900,
    0xF920, 0xF940, 0xF940, 0xF960, 0xF980, 0xF980, 0xF9A0, 0xF9C0,
    0xF9C0, 0xF9E0, 0xFA00, 0xFA00, 0xFA20, 0xFA40, 0xFA40, 0xFA60,
    0xFA80, 0xFA80, 0xFAA0, 0xFAC0, 0xFAC0, 0xFAE0, 0xFB00, 0xFB00,
    0xFB20, 0xFB40, 0xFB40, 0xFB60, 0xFB80, 0xFB80, 0xFBA0, 0xFBC0,
    0xFBC0, 0xFBE0, 0xFC00, 0xFC00, 0xFC20, 0xFC40, 0xFC40, 0xFC60,
    0xFC80, 0xFC80, 0xFCA0, 0xFCC0, 0xFCC0, 0xFCE0, 0xFD00, 0xFD00,
    0xFD20, 0xFD40, 0xFD40, 0xFD60, 0xFD80, 0xFD80, 0xFDA0, 0xFDC0,
    0xFDC0, 0xFDE0, 0xFE00, 0xFE00, 0xFE20, 0xFE40, 0xFE40, 0xFE60,
    0xFE80, 0xFE80, 0xFEA0, 0xFEC0, 0xFEC0, 0xFEE0, 0xFF00, 0xFF00,
    0xFF02, 0xFF04, 0xFF06, 0xFF09, 0xFF0B, 0xFF0D, 0xFF0F, 0xFF11,
    0xFF14, 0xFF16, 0xFF18, 0xFF1A, 0xFF1D, 0xFF1F, 0xFF21, 0xFF23,
    0xFF26, 0xFF28, 0xFF2A, 0xFF2C, 0xFF2F, 0xFF31, 0xFF33, 0xFF36,
    0xFF38, 0xFF3A, 0xFF3C, 0xFF3F, 0xFF41, 0xFF43, 0xFF45, 0xFF48,
    0xFF4A, 0xFF4C, 0xFF4E, 0xFF51, 0xFF53, 0xFF55, 0xFF57, 0xFF5A,
    0xFF5C, 0xFF5D, 0xFF5F, 0xFF61, 0xFF63, 0xFF65, 0xFF67, 0xFF69,
    0xFF6B, 0xFF6D, 0xFF6F, 0xFF71, 0xFF73, 0xFF75, 0xFF77, 0xFF79,
    0xFF7B, 0xFF7D, 0xFF7F, 0xFF81, 0xFF83, 0xFF85, 0xFF87, 0xFF89,
    0xFF8B, 0xFF8D, 0xFF8F, 0xFF91, 0xFF93, 0xFF95, 0xFF97, 0xFF99,
    0xFF9B, 0xFF9D, 0xFF9F, 0xFFA1, 0xFFA3, 0xFFA5, 0xFFA7, 0xFFA9,
    0xFFAB, 0xFFAD, 0xFFAF, 0xFFB1, 0xFFB3, 0xFFB5, 0xFFB7, 0xFFFF
};

void delay_us(uint32_t us)
{
volatile uint32_t delay = (us * (72000000 / 1000000) / 4);
while (delay--);
} 

/* Send command (char) to LCD */
void ILI9341_Write_Command(uint8_t Command)
{
	DISP_DC_CMD;
	DISP_CS_SELECT;
	SPI1_DR_8bit = Command;
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	while(DISP_SPI->SR & SPI_SR_BSY);
	//SPI1_send(Command);
	DISP_CS_UNSELECT;
}

/* Send Data (char) to LCD */
void ILI9341_Write_Data(uint8_t Data)
{
	DISP_DC_DATA;
	DISP_CS_SELECT;
	SPI1_DR_8bit = Data;
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	while(DISP_SPI->SR & SPI_SR_BSY);
	//SPI1_send(Data);
	DISP_CS_UNSELECT;
}

/* Set Address - Location block - to draw into */
void ILI9341_Set_Address(uint16_t X1, uint16_t Y1, uint16_t X2, uint16_t Y2)
{
	DISP_DC_CMD;
	DISP_CS_SELECT;

	SPI1_DR_8bit = 0x2A;
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	while(DISP_SPI->SR & SPI_SR_BSY);
	//SPI1_send(0x2A);

	DISP_DC_DATA;
	SPI1_DR_8bit = (uint8_t)(X1 >> 8);
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	//SPI1_send((uint8_t)(X1 >> 8));
	SPI1_DR_8bit = (uint8_t)X1;
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	//SPI1_send((uint8_t)X1);
	SPI1_DR_8bit = (uint8_t)(X2 >> 8);
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	//SPI1_send((uint8_t)(X2 >> 8));
	SPI1_DR_8bit = (uint8_t)X2;
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	while(DISP_SPI->SR & SPI_SR_BSY);
	//SPI1_send((uint8_t)X2);

	DISP_DC_CMD;
	SPI1_DR_8bit = 0x2B;
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	while(DISP_SPI->SR & SPI_SR_BSY);
	//SPI1_send(0x2B);

	DISP_DC_DATA;
	SPI1_DR_8bit = (uint8_t)(Y1 >> 8);
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	//SPI1_send((uint8_t)(Y1 >> 8));
	SPI1_DR_8bit = (uint8_t)Y1;
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	//SPI1_send((uint8_t)Y1);
	SPI1_DR_8bit = (uint8_t)(Y2 >> 8);
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	//SPI1_send((uint8_t)(Y2 >> 8));
	SPI1_DR_8bit = (uint8_t)Y2;
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	while(DISP_SPI->SR & SPI_SR_BSY);
	//SPI1_send((uint8_t)Y2);

	DISP_DC_CMD;
	SPI1_DR_8bit = 0x2C;
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	while(DISP_SPI->SR & SPI_SR_BSY);
	//SPI1_send(0x2C);
	DISP_CS_UNSELECT;
}

/*HARDWARE RESET*/
void ILI9341_Reset(void)
{
	DISP_RST_RESET;
	delay_us(2000);
	DISP_CS_SELECT;
	delay_us(2000);
	DISP_RST_WORK;
  delay_us(2000);
}

/*Ser rotation of the screen - changes x0 and y0*/
void ILI9341_Set_Rotation(uint8_t Rotation)
{
	DISP_DC_CMD;
	DISP_CS_SELECT;
	SPI1_DR_8bit = 0x36;
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	while(DISP_SPI->SR & SPI_SR_BSY);
	DISP_CS_UNSELECT;

	
	/*//ILI9341
	switch(Rotation)
	{
		case SCREEN_VERTICAL_1:
			ILI9341_Write_Data(0x40|0x08);
			LCD_WIDTH = 240;
			LCD_HEIGHT = 320;
			break;
		case SCREEN_HORIZONTAL_1:
			ILI9341_Write_Data(0x20|0x08);
			LCD_WIDTH  = 320;
			LCD_HEIGHT = 240;
			break;
		case SCREEN_VERTICAL_2:
			ILI9341_Write_Data(0x80|0x08);
			LCD_WIDTH  = 240;
			LCD_HEIGHT = 320;
			break;
		case SCREEN_HORIZONTAL_2:
			ILI9341_Write_Data(0x40|0x80|0x20|0x08);
			LCD_WIDTH  = 320;
			LCD_HEIGHT = 240;
			break;
		default:
			break;
	} */
	// ST7789
	switch(Rotation)
	{
		case SCREEN_VERTICAL_1:
			ILI9341_Write_Data(0x20 | 0x00); // Честный вертикальный режим (обмен осей)
			LCD_WIDTH = 240;
			LCD_HEIGHT = 320;
			break;
		case SCREEN_HORIZONTAL_1:
			ILI9341_Write_Data(0x00 | 0x80); // Честный альбомный режим (без обмена осей, зеркало Y)
			LCD_WIDTH  = 320;
			LCD_HEIGHT = 240;
			break;
		case SCREEN_VERTICAL_2:
			ILI9341_Write_Data(0x20 | 0x40 | 0x80); // Честный перевернутый вертикальный
			LCD_WIDTH  = 240;
			LCD_HEIGHT = 320;
			break;
		case SCREEN_HORIZONTAL_2:
			ILI9341_Write_Data(0x00 | 0x40); // Честный альбомный 2 (рабочий для вашего водопада)
			LCD_WIDTH  = 320;
			LCD_HEIGHT = 240;
			break;
		default:
			break;
	}
	
}


/*Initialize LCD display*/
void ILI9341_Init(void)
{
	DISP_RST_WORK; //Enable LCD display
	//DISP_CS_SELECT; // Initialize SPI 
	ILI9341_Reset();
	
	//SOFTWARE RESET
	ILI9341_Write_Command(0x01);
	//HAL_Delay(1000);
	delay_us(10000);

	//POWER CONTROL A
	ILI9341_Write_Command(0xCB);
	ILI9341_Write_Data(0x39);
	ILI9341_Write_Data(0x2C);
	ILI9341_Write_Data(0x00);
	ILI9341_Write_Data(0x34);
	ILI9341_Write_Data(0x02);

	//POWER CONTROL B
	ILI9341_Write_Command(0xCF);
	ILI9341_Write_Data(0x00);
	ILI9341_Write_Data(0xC1);
	ILI9341_Write_Data(0x30);

	//DRIVER TIMING CONTROL A
	ILI9341_Write_Command(0xE8);
	ILI9341_Write_Data(0x85);
	ILI9341_Write_Data(0x00);
	ILI9341_Write_Data(0x78);

	//DRIVER TIMING CONTROL B
	ILI9341_Write_Command(0xEA);
	ILI9341_Write_Data(0x00);
	ILI9341_Write_Data(0x00);

	//POWER ON SEQUENCE CONTROL
	ILI9341_Write_Command(0xED);
	ILI9341_Write_Data(0x64);
	ILI9341_Write_Data(0x03);
	ILI9341_Write_Data(0x12);
	ILI9341_Write_Data(0x81);

	//PUMP RATIO CONTROL
	ILI9341_Write_Command(0xF7);
	ILI9341_Write_Data(0x20);

	//POWER CONTROL,VRH[5:0]
	ILI9341_Write_Command(0xC0);
	ILI9341_Write_Data(0x23);

	//POWER CONTROL,SAP[2:0];BT[3:0]
	ILI9341_Write_Command(0xC1);
	ILI9341_Write_Data(0x10);

	//VCM CONTROL
	ILI9341_Write_Command(0xC5);
	ILI9341_Write_Data(0x3E); 
	ILI9341_Write_Data(0x28); 

	//VCM CONTROL 2
	ILI9341_Write_Command(0xC7);
  ILI9341_Write_Data(0x86); 

	//MEMORY ACCESS CONTROL
	ILI9341_Write_Command(0x36);
	ILI9341_Write_Data(0x48);

	//PIXEL FORMAT
	ILI9341_Write_Command(0x3A);
	//ILI9341_Write_Data(0x00);
	//ILI9341_Write_Data(0x11); // ? bit rgb?
	//ILI9341_Write_Data(0x22); // ? bit rgb?
	//ILI9341_Write_Data(0x33); // x bit gray?

	//ILI9341_Write_Data(0x44); // ? bit rgb?
	ILI9341_Write_Data(0x55); // 16 bit rgb565

	//FRAME RATIO CONTROL, STANDARD RGB COLOR
	ILI9341_Write_Command(0xB1);
	ILI9341_Write_Data(0x00);
  ILI9341_Write_Data(0x1B); 

	//DISPLAY FUNCTION CONTROL
	ILI9341_Write_Command(0xB6);
	ILI9341_Write_Data(0x08);
	ILI9341_Write_Data(0x82);
	ILI9341_Write_Data(0x27);

	//3GAMMA FUNCTION DISABLE
	ILI9341_Write_Command(0xF2);
	ILI9341_Write_Data(0x00);

	//GAMMA CURVE SELECTED
	ILI9341_Write_Command(0x26);
	ILI9341_Write_Data(0x01);

	//POSITIVE GAMMA CORRECTION
	ILI9341_Write_Command(0xE0);
	ILI9341_Write_Data(0x0F);
	ILI9341_Write_Data(0x31);
	ILI9341_Write_Data(0x2B);
	ILI9341_Write_Data(0x0C);
	ILI9341_Write_Data(0x0E);
	ILI9341_Write_Data(0x08);
	ILI9341_Write_Data(0x4E);
	ILI9341_Write_Data(0xF1);
	ILI9341_Write_Data(0x37);
	ILI9341_Write_Data(0x07);
	ILI9341_Write_Data(0x10);
	ILI9341_Write_Data(0x03);
	ILI9341_Write_Data(0x0E);
	ILI9341_Write_Data(0x09);
	ILI9341_Write_Data(0x00);

	//NEGATIVE GAMMA CORRECTION
	ILI9341_Write_Command(0xE1);
	ILI9341_Write_Data(0x00);
	ILI9341_Write_Data(0x0E);
	ILI9341_Write_Data(0x14);
	ILI9341_Write_Data(0x03);
	ILI9341_Write_Data(0x11);
	ILI9341_Write_Data(0x07);
	ILI9341_Write_Data(0x31);
	ILI9341_Write_Data(0xC1);
	ILI9341_Write_Data(0x48);
	ILI9341_Write_Data(0x08);
	ILI9341_Write_Data(0x0F);
	ILI9341_Write_Data(0x0C);
	ILI9341_Write_Data(0x31);
	ILI9341_Write_Data(0x36);
	ILI9341_Write_Data(0x0F);

  // для ST7789 
	// 1. Настройка инверсии (0xB4). Заставляем чип использовать покадровую инверсию (Frame Inversion)
	ILI9341_Write_Command(0xB4);
	ILI9341_Write_Data(0x0B); // Этот байт фиксирует полярность при скроллинге
	// 2. Оптимизация питания подложки (0xBB). Убирает мерцание и цветовой сдвиг
	ILI9341_Write_Command(0xBB);
	ILI9341_Write_Data(0x28); // Оптимальный уровень VCOM для стабильного цвета
	// 3. Управление затворами (0xB7). Фиксирует стабильное напряжение строк матрицы
	ILI9341_Write_Command(0xB7);
	ILI9341_Write_Data(0x35);/**/
	
	//EXIT SLEEP
	ILI9341_Write_Command(0x11);
	//HAL_Delay(120);
	delay_us(120);
	

	//TURN ON DISPLAY
	ILI9341_Write_Command(0x29);

	//STARTING ROTATION
	ILI9341_Set_Rotation(SCREEN_HORIZONTAL_2);
	
}


//INTERNAL FUNCTION OF LIBRARY, USAGE NOT RECOMENDED, USE Draw_Pixel INSTEAD
/*Sends single pixel colour information to LCD*/
void ILI9341_Draw_Colour(uint16_t Colour)
{
	DISP_DC_DATA;
	DISP_CS_SELECT;
	SPI1_DR_8bit = (Colour >> 8);
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	SPI1_DR_8bit = Colour;
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	while(DISP_SPI->SR & SPI_SR_BSY);
	DISP_CS_UNSELECT;
}

//INTERNAL FUNCTION OF LIBRARY
/*Sends block colour information to LCD*/
void ILI9341_Draw_Colour_Burst(uint16_t Colour, uint32_t Size)
{
	DISP_DC_DATA;
	DISP_CS_SELECT;

	while(Size > 0)
	{
		SPI1_DR_8bit = (Colour >> 8);
		while(!(DISP_SPI->SR & SPI_SR_TXE));
		SPI1_DR_8bit = Colour;
		while(!(DISP_SPI->SR & SPI_SR_TXE));
		Size--;
	}

	while(!(DISP_SPI->SR & SPI_SR_TXE));
	while(DISP_SPI->SR & SPI_SR_BSY);
	DISP_CS_UNSELECT;
}

// Scroll display memory
// Y - How many pixels to scroll display by
// Прокрутка памяти дисплея
// У - на сколько строк прокрутить дисплей
void ILI9341_Scroll_To(uint16_t Y)
{
	ILI9341_Write_Command(ILI9341_VSCRSADD);
	ILI9341_Write_Data(Y >> 8);
	ILI9341_Write_Data(Y & 0xFF);
}
//Set the height of the Top and Bottom Scroll Margins
//Top The height of the Top scroll margin
//Bottom The height of the Bottom scroll margin
// Устанавливает высоту верхнего и нижнего поля прокрутки
void ILI9341_Set_Scroll_Margins(uint16_t Top, uint16_t Bottom)
{

	// TFA+VSA+BFA must equal 320
    if (Top + Bottom <= 320) {
        uint16_t middle = 320 - (Top + Bottom);
        ILI9341_Write_Command(ILI9341_VSCRDEF);
        ILI9341_Write_Data(Top >> 8);
        ILI9341_Write_Data(Top & 0xFF);
        ILI9341_Write_Data(middle >> 8);
        ILI9341_Write_Data(middle & 0xFF);
        ILI9341_Write_Data(Bottom >> 8);
        ILI9341_Write_Data(Bottom & 0xFF);
    }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////// USER FUNCTION //////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//FILL THE ENTIRE SCREEN WITH SELECTED COLOUR (either #define-d ones or custom 16bit)
void ILI9341_Fill_Screen(uint16_t Colour)
{
	ILI9341_Set_Address(0, 0, LCD_WIDTH, LCD_HEIGHT);
	ILI9341_Draw_Colour_Burst(Colour, LCD_WIDTH * LCD_HEIGHT);
}

//DRAW PIXEL AT XY POSITION WITH SELECTED COLOUR
void ILI9341_Draw_Pixel(uint16_t X, uint16_t Y, uint16_t Colour)
{
	if((X >= LCD_WIDTH) || (Y >= LCD_HEIGHT)) return;

	//ADDRESS
	DISP_DC_CMD;
	DISP_CS_SELECT;
	SPI1_DR_8bit = 0x2A;
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	while(DISP_SPI->SR & SPI_SR_BSY);////////////

	DISP_DC_DATA;
	SPI1_DR_8bit = (X >> 8);
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	SPI1_DR_8bit = X;
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	SPI1_DR_8bit = ((X + 1) >> 8);
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	SPI1_DR_8bit = (X + 1);
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	while(DISP_SPI->SR & SPI_SR_BSY);

	//ADDRESS
	DISP_DC_CMD;
	SPI1_DR_8bit = 0x2B;
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	while(DISP_SPI->SR & SPI_SR_BSY);

	DISP_DC_DATA;
	SPI1_DR_8bit = (Y >> 8);
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	SPI1_DR_8bit = Y;
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	SPI1_DR_8bit = ((Y + 1) >> 8);
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	SPI1_DR_8bit = (Y + 1);
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	while(DISP_SPI->SR & SPI_SR_BSY);

	//ADDRESS
	DISP_DC_CMD;
	SPI1_DR_8bit = 0x2C;
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	while(DISP_SPI->SR & SPI_SR_BSY);

	DISP_DC_DATA;
	SPI1_DR_8bit = (Colour >> 8);
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	SPI1_DR_8bit = Colour;
	while(!(DISP_SPI->SR & SPI_SR_TXE));
	while(DISP_SPI->SR & SPI_SR_BSY);

	DISP_CS_UNSELECT;
}

//DRAW RECTANGLE OF SET SIZE AND HEIGTH AT X and Y POSITION WITH CUSTOM COLOUR
void ILI9341_Draw_Rectangle(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height, uint16_t Colour)
{
	if((X >= LCD_WIDTH) || (Y >= LCD_HEIGHT)) return;

	if((X + Width - 1) >= LCD_WIDTH)
	{
		Width = LCD_WIDTH - X;
	}

	if((Y + Height - 1) >= LCD_HEIGHT)
	{
		Height = LCD_HEIGHT - Y;
	}

	ILI9341_Set_Address(X, Y, X + Width - 1, Y + Height - 1);
	ILI9341_Draw_Colour_Burst(Colour, Height * Width);
}

//DRAW LINE FROM X,Y LOCATION to X+Width,Y LOCATION
void ILI9341_Draw_Horizontal_Line(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Colour)
{
	if((X >= LCD_WIDTH) || (Y >= LCD_HEIGHT)) return;

	if((X + Width - 1) >= LCD_WIDTH)
	{
		Width = LCD_WIDTH - X;
	}

	ILI9341_Set_Address(X, Y, X + Width - 1, Y);
	ILI9341_Draw_Colour_Burst(Colour, Width);
}
//////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// ИНТЕРФЕЙС ///////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// усреднение водопада
void apply_smoothing(q15_t* new_mag) { 

    
    uint16_t invK = 32767 - K_SMOOTH;

    for (int i = 0; i < 256; i++) {
        // Формула фильтра: (Старое * K + Новое * (1-K)) / 32768
        q31_t acc = (q31_t)smoothed_mag[i] * K_SMOOTH;
        acc += (q31_t)new_mag[i] * invK;
        
        // Сохраняем результат для следующего кадра
        smoothed_mag[i] = (q15_t)(acc >> 15);
        
        // Возвращаем усредненное значение обратно в массив для водопада
        new_mag[i] = smoothed_mag[i];
    }
}
// водопад FFT
void ILI9341_Draw_Waterfall(uint16_t* data) {
	uint16_t wf;
  uint16_t intensity;
  int16_t target_x;
	
	apply_smoothing(data); // усредняем водопад
	for(uint16_t i=0; i<256; i++){
	  intensity = data[i];//magnitude_to_db_pixel(data[i]/*<<2*/);
		// 2. ЖЕСТКО ограничиваем диапазон 0-255
    if (intensity > 255) intensity = 255; 
		// 2. Берем готовый цвет из палитры
    wf = waterfall_palette_lava[intensity];
		// 3. FFT Shift: частота 0 должна быть в центре (индекс 128)
		// Входной массив CFFT: [0..127] - полож., [128..255] - отриц.
		// Чтобы 0 был в центре:
		if (i < 128) target_x = i + 128; // Положительные уходят вправо
		else target_x = i - 128;         // Отрицательные уходят влево
		// 4. Заполнение буфера (big-endian для ILI9341)
		buf_disp[target_x * 2]     = (uint8_t)(wf >> 8);
		buf_disp[target_x * 2 + 1] = (uint8_t)(wf & 0xFF);
    }
	buf_disp[254] = 0b00000100;
	buf_disp[255] = 0b00000000;
	buf_disp[256] = 0b11111000;
	buf_disp[257] = 0b00000000;
	buf_disp[258] = 0b00000100;
	buf_disp[259] = 0b00000000;
	ILI9341_Set_Address(320-inc_wf, 0, 320-inc_wf, 240);
	DISP_DC_DATA;
	DISP_CS_SELECT;
		for(i=16; i<496; i++){ 
			
			while(!(DISP_SPI->SR & SPI_SR_TXE)); 
			SPI1_DR_8bit = buf_disp[i];
		}
	while(!(DISP_SPI->SR & SPI_SR_TXE)); 
  while(DISP_SPI->SR & SPI_SR_BSY);	
  DISP_CS_UNSELECT;//*/
		
	ILI9341_Set_Scroll_Margins(0, 260); // устанавливаем область прокрутки
	ILI9341_Scroll_To(inc_wf); // прокручиваем
	inc_wf++;
	if (inc_wf>60){
		inc_wf = 0;
	}
}

void ILI9341_Draw_MainFrec(uint16_t x, uint16_t y, uint32_t freq){

    char new_txt_buf[12];
    format_freq(freq, new_txt_buf); // Формируем новую строку

    for (int i = 0; i < 10; i++) {
        // Если символ в этой позиции изменился
        if (new_txt_buf[i] != old_txt_freq[i]) {
            // Вычисляем X координату для конкретного символа
            // Ширина Font_16x26 равна 16 пикселям
            uint16_t char_x = x + (i * 16); 
            if ((i==0)&&(new_txt_buf[i] =='0')){
            ILI9341_WriteChar(char_x, y, new_txt_buf[i], Font_16x26, BORDERCL, MYFON);}
						else{
						ILI9341_WriteChar(char_x, y, new_txt_buf[i], Font_16x26, GREEN, MYFON);}
            
            // Запоминаем изменение
            old_txt_freq[i] = new_txt_buf[i];
        }
    }
}

void ILI9341_Draw_Frec11x18(uint16_t x, uint16_t y, uint32_t freq){  // рисуем частоту маленьким шрифтом

//	format_freq(freq, txt_buf); 
//  ILI9341_WriteString(   70, 108, txt_buf, Font_16x26, GREEN, MYFON);
    char new_txt_buf[12];
    format_freq(freq, new_txt_buf); // Формируем новую строку
		ILI9341_WriteString(x, y, new_txt_buf, Font_11x18, GREEN, MYFON);
		if (new_txt_buf[0] =='0'){
            ILI9341_WriteChar(x, y, new_txt_buf[0], Font_11x18, BORDERCL, MYFON);}
}

void format_freq(uint32_t f, char *out) {
    // Заполняем её нулями вместо пробелов
    for(int n = 0; n < 10; n++) out[n] = '0'; 
    
    // Сразу расставляем точки на фиксированные позиции
    out[2] = '.'; // Позиция после десятков МГц (ХХ.ххх.ххх)
    out[6] = '.'; // Позиция после сотен кГц  (хх.ХХХ.ххх)
    out[10] = '\0'; // Терминатор строки

    int pos = 9; // Начинаем заполнение с самого конца (Герцы)

    if (f == 0) return; // Строка уже заполнена "00.000.000"

    while (f > 0 && pos >= 0) {
        // Пропускаем позиции, где уже стоят точки
        if (pos == 6 || pos == 2) {
            pos--;
        }
        
        out[pos--] = (f % 10) + '0';
        f /= 10;
    }
}

void format_var(uint32_t f, char *out){ // Переменную в строку
    // Заполняем пробелами
    for(int n = 0; n < 8; n++) out[n] = ' ';   
    out[8] = '\0'; // Терминатор строки
    int pos = 7; // Начинаем заполнение с самого конца 

    if (f == 0) {
			out[7] = '0';
			return; 
		}
    while (f > 0 && pos >= 0) {
        out[pos--] = (f % 10) + '0';
        f /= 10;
    }
}

void ILI9341_Draw_Menu_Var(uint16_t x, uint16_t y, uint32_t var){ // Рисуем переменные в меню
    char new_txt_buf[8];
    format_var(var, new_txt_buf); // Формируем новую строку
		ILI9341_WriteString(x, y, new_txt_buf, Font_11x18, GREEN, MYFON);
}

// рисуем шкалу рядом с водопадом
void ILI9341_Draw_Scale(){
	uint16_t i = 0;
	//for (i = 0; i < 5; i++){
	//	ILI9341_Draw_Horizontal_Line(250, i*45+30, 10, BORDERCL);
	//}//*/
	for (i = 0; i < 26; i++){ // рисуем половинку от середины вверх и вниз
		ILI9341_Draw_Horizontal_Line(255, 120+(i*256/44), 5, BORDERCL);
		ILI9341_Draw_Horizontal_Line(255, 119-(i*256/44), 5, BORDERCL);
		if (i==5){
			ILI9341_WriteString(243, 116+(i*256/44), "5", Font_7x10, BORDERCL, MYFON);
			ILI9341_WriteString(243, 116-(i*256/44), "5", Font_7x10, BORDERCL, MYFON);
			ILI9341_Draw_Horizontal_Line(250, 120+(i*256/44), 10, BORDERCL);
			ILI9341_Draw_Horizontal_Line(250, 119-(i*256/44), 10, BORDERCL);
		}
		if (i==10){
			ILI9341_WriteString(236, 116+(i*256/44), "10", Font_7x10, BORDERCL, MYFON);
			ILI9341_WriteString(236, 116-(i*256/44), "10", Font_7x10, BORDERCL, MYFON);
			ILI9341_Draw_Horizontal_Line(250, 120+(i*256/44), 10, BORDERCL);
			ILI9341_Draw_Horizontal_Line(250, 119-(i*256/44), 10, BORDERCL);			
		}
		if (i==15){
			ILI9341_WriteString(236, 116+(i*256/44), "15", Font_7x10, BORDERCL, MYFON);
			ILI9341_WriteString(236, 116-(i*256/44), "15", Font_7x10, BORDERCL, MYFON);
			ILI9341_Draw_Horizontal_Line(250, 120+(i*256/44), 10, BORDERCL);
			ILI9341_Draw_Horizontal_Line(250, 119-(i*256/44), 10, BORDERCL);
		}
		if (i==20){
			ILI9341_WriteString(236, 114+(i*256/44), "20", Font_7x10, BORDERCL, MYFON);
			ILI9341_WriteString(236, 117-(i*256/44), "20", Font_7x10, BORDERCL, MYFON);
			ILI9341_Draw_Horizontal_Line(250, 120+(i*256/44), 10, BORDERCL);
			ILI9341_Draw_Horizontal_Line(250, 119-(i*256/44), 10, BORDERCL);
		}
	}
	ILI9341_Draw_Horizontal_Line(250, 118, 10, BORDERCL);
	ILI9341_Draw_Horizontal_Line(250, 119, 10, RED);
	ILI9341_Draw_Horizontal_Line(250, 120, 10, BORDERCL);

}


/**
 * @brief Отрисовка шкалы S-метра с динамическим учётом усиления тракта
 * @param total_gain_db  Полное усиление всего аналогового тракта в дБ (УВЧ + УПЧ - АТТ)
 * @param bar_right_x    Координата X правой границы столбика (11)
 */
void Draw_SMeter_Labels(int16_t total_gain_db, uint16_t bar_right_x) {
  
    // Защита от деления на ноль или некорректных настроек шума
    if (smeter_min_db >= 231) {
        smeter_scale = 0;
        return;
    }
    // Вычисляем масштаб в формате Q8: (Высота * 256) / Диапазон
    smeter_scale = (uint16_t)((240 * 256) / (231 - smeter_min_db));
		
    // 1. Очистка зоны разметки (ширина 36 пикселей справа от столбика)
    for (uint16_t y = 10; y < 235; y++) {
        ILI9341_Draw_Horizontal_Line(bar_right_x + 1, y, 36, MYFON);
    }

    // 1. Очистка зоны разметки (ширина 36 пикселей справа от столбика)
    for (y = 10; y < 235; y++) {
        ILI9341_Draw_Horizontal_Line(bar_right_x + 1, y, 36, MYFON);
    }

    // Расширенная структура метки
    typedef struct {
        uint16_t raw_at_zero_gain; // Положительный сырой уровень относительно S1
        const char* text;          // Текст метки (NULL для просто линий)
        uint8_t is_text;           // 1 — выводить текст и длинную засечку, 0 — только короткую линию
    } SMeterLabel;

    // Полная карта: основные баллы, промежуточные S4/S6/S8 и линии +5/+15/+25/+35 дБ
    static const SMeterLabel labels[] = {
        {32,  "-S3",  1}, // S3
        {48,  "",    0}, // Промежуточная линия (S4)
        {64,  "-S5",  1}, // S5
        {80,  "",    0}, // Промежуточная линия (S6)
        {96,  "-S7",  1}, // S7
        {112, "",    0}, // Промежуточная линия (S8)
        {128, "-S9",  1}, // S9
        
        {141, "",    0}, // Линия +5 дБ (128 + 13.3)
        {155, "+10", 1}, // +10 дБ
        {168, "",    0}, // Линия +15 дБ (155 + 13.3)
        {181, "+20", 1}, // +20 дБ
        {194, "",    0}, // Линия +25 дБ (181 + 13.3)
        {208, "+30", 1}, // +30 дБ
        {221, "",    0}, // Линия +35 дБ (208 + 13.3)
        {234, "+40", 1}  // +40 дБ
    };

    // Общее количество элементов в новой карте
    #define LABELS_COUNT (sizeof(labels) / sizeof(labels[0]))

    // Переводим дБ усиления в единицы шкалы raw_height
    int32_t gain_offset = (int32_t)(total_gain_db * 681) >> 8;

    // 2. Цикл отрисовки меток и линий
    for (uint8_t i = 0; i < LABELS_COUNT; i++) {
        // Компенсируем сдвиг карты (+128), возвращаясь к вашей исходной математике
        int32_t raw_target = ((int32_t)labels[i].raw_at_zero_gain - 128) + gain_offset;

        // Применяем вашу программную отсечку нуля (SMETER_MIN_DB = 60)
        int32_t target_calibrated = raw_target - smeter_min_db;
        
        // Если точка ниже порога шума экрана — скрываем
        if (target_calibrated < 0) continue; 

        // Переводим в пиксели высоты экрана по вашей формуле (SMETER_SCALE = 377)
        int32_t pixel_height = (target_calibrated * smeter_scale) >> 8;
        
        // Если метка улетела выше экрана (больше 240 пкс) — скрываем
        if (pixel_height > 240) continue;

        // Вычисляем итоговый Y на экране (0 вверху, 240 внизу)
        uint16_t y_pos = 240 - pixel_height;

        // Жесткая защита координат, чтобы графика не вылезала за экран
        if (y_pos < 10 || y_pos > 230) continue;

        // 3. Логика отрисовки в зависимости от типа метки
        if (labels[i].is_text) {
            // Основной балл: длинная засечка (4 пикселя) и текст
            //ILI9341_Draw_Horizontal_Line(bar_right_x + 1, y_pos, 4, BORDERCL);

            ILI9341_WriteString(bar_right_x + 1, y_pos - 4, labels[i].text, Font_7x10, BORDERCL, MYFON);
        } else {
            // Промежуточная линия: короткая засечка (2 пикселя), текст не выводим
            ILI9341_Draw_Horizontal_Line(bar_right_x + 1, y_pos, 2, BORDERCL);
        }
    }
}

// выводим полосу s-метра
void ILI9341_Draw_Smetr(q15_t value){
    // 1. Модуль знака с защитой от переполнения q15_t (-32768)
    int32_t s_value = (int32_t)value;
    if (s_value < 0) s_value = -s_value;
    if (s_value == 0) s_value = 1; 

    // 2. ЛОГАРИФМИЧЕСКИЙ перевод
    uint32_t u_value = (uint32_t)s_value; 
    uint32_t lz = __CLZ(u_value); 
    uint32_t bit_pos = 31 - lz; // Позиция старшего бита (0...14 для q15)

    // Извлекаем 3 бита мантиссы, идущие ЗА старшим битом
    uint32_t mantissa = 0;
    if (bit_pos >= 3) {
        mantissa = (u_value >> (bit_pos - 3)) & 0x07;
    } else {
        // Безопасное извлечение для младших битов
        mantissa = (u_value & ((1 << bit_pos) - 1)) << (3 - bit_pos);
    }

    // Сырая высота: bit_pos умножаем на 16 (сдвиг << 4), а не на 15.
    // Это дает идеальную сетку: 16 уровней внутри каждого бита.
    // Максимальное значение при bit_pos=14: (14 * 16) + 7 = 231 (отлично укладывается в 240)
    int32_t raw_height = (bit_pos << 4) + mantissa;

    // 3. Калибровка и масштабирование
    int32_t target_calibrated = raw_height - smeter_min_db;
    if (target_calibrated < 0) target_calibrated = 0;

    // Применяем масштаб (умножаем в Q8 и сдвигаем >> 8)
    uint16_t target_bar_height = (uint16_t)((target_calibrated * smeter_scale) >> 8);

    // Жесткое ограничение физических размеров экрана
    if (target_bar_height > 240) target_bar_height = 240;

    // Статическая переменная хранит текущую высоту столбика в пикселях (0...240)
    static int16_t bar_height = 0;

    // 3. Логика движения столбика 
    if (target_bar_height > bar_height) {
        // Мгновенный или быстрый взлет до целевого значения
        bar_height = target_bar_height; 
    } else if (target_bar_height < bar_height) {
        // Линейное и плавное падение в пикселях.
        // ПОДБОР СКОРОСТИ: 1 - очень медленно, 2 - среднее, 5 - быстро, 10 - мгновенно.
        #define DROP_SPEED_PIXELS 3 
        
        bar_height -= DROP_SPEED_PIXELS;
        
        // Защита: если проскочили целевое значение вниз
        if (bar_height < target_bar_height) {
            bar_height = target_bar_height;
        }
    }

    // 4. Отрисовка стык-в-стык (используем уже рассчитанный bar_height)
    uint16_t bg_height = 240 - bar_height;
    
    // Рисуем ФОН (верхняя пустая часть шкалы)
    if (bg_height > 0) {
        ILI9341_Draw_Rectangle(1, 0, 10, bg_height, MYFON);
    }
    
    // Рисуем СТОЛБИК (активная зеленая часть)
    if (bar_height > 0) {
        ILI9341_Draw_Rectangle(1, bg_height, 10, bar_height, GREEN);
    } 
 
}

void Draw_Step(uint16_t step){ // Рисуем плашки под основной частотой
	ILI9341_Draw_Rectangle(70+16*6, 108+26, 16*5, 2, MYFON);
	switch (step) {
			case 1: 
					// 
					ILI9341_Draw_Rectangle(63+16*10, 108+26, 16, 2, GREEN);
					break;
			case 10: 
					// 
					ILI9341_Draw_Rectangle(63+16*9, 108+26, 16, 2, GREEN);
					break;
			case 100: 
					//
					ILI9341_Draw_Rectangle(63+16*8, 108+26, 16, 2, GREEN);
					break;
			case 1000: 
					// 
					ILI9341_Draw_Rectangle(63+16*6, 108+26, 16, 2, GREEN);
					break;
	}
}

//DRAW LINE FROM X,Y LOCATION to X,Y+Height LOCATION
void ILI9341_Draw_Vertical_Line(uint16_t X, uint16_t Y, uint16_t Height, uint16_t Colour)
{
	if((X >= LCD_WIDTH) || (Y >= LCD_HEIGHT)) return;

	if((Y + Height - 1) >= LCD_HEIGHT)
	{
		Height = LCD_HEIGHT - Y;
	}

	ILI9341_Set_Address(X, Y, X, Y + Height - 1);
	ILI9341_Draw_Colour_Burst(Colour, Height);
}

/*Draw hollow circle at X,Y location with specified radius and colour. X and Y represent circles center */
void ILI9341_Draw_Hollow_Circle(uint16_t X, uint16_t Y, uint16_t Radius, uint16_t Colour)
{
	int x = Radius - 1;
    int y = 0;
    int dx = 1;
    int dy = 1;
    int err = dx - (Radius << 1);

    while (x >= y)
    {
        ILI9341_Draw_Pixel(X + x, Y + y, Colour);
        ILI9341_Draw_Pixel(X + y, Y + x, Colour);
        ILI9341_Draw_Pixel(X - y, Y + x, Colour);
        ILI9341_Draw_Pixel(X - x, Y + y, Colour);
        ILI9341_Draw_Pixel(X - x, Y - y, Colour);
        ILI9341_Draw_Pixel(X - y, Y - x, Colour);
        ILI9341_Draw_Pixel(X + y, Y - x, Colour);
        ILI9341_Draw_Pixel(X + x, Y - y, Colour);

        if (err <= 0)
        {
            y++;
            err += dy;
            dy += 2;
        }

        if (err > 0)
        {
            x--;
            dx += 2;
            err += (-Radius << 1) + dx;
        }
    }
}

/*Draw filled circle at X,Y location with specified radius and colour. X and Y represent circles center */
void ILI9341_Draw_Filled_Circle(uint16_t X, uint16_t Y, uint16_t Radius, uint16_t Colour)
{
	int x = Radius;
	int y = 0;
	int xChange = 1 - (Radius << 1);
	int yChange = 0;
	int radiusError = 0;
	int i = 0;
	
	while (x >= y)
	{
		for (i = X - x; i <= X + x; i++)
		{
			ILI9341_Draw_Pixel(i, Y + y,Colour);
			ILI9341_Draw_Pixel(i, Y - y,Colour);
		}

		for (i = X - y; i <= X + y; i++)
		{
			ILI9341_Draw_Pixel(i, Y + x,Colour);
			ILI9341_Draw_Pixel(i, Y - x,Colour);
		}

		y++;
		radiusError += yChange;
		yChange += 2;

		if(((radiusError << 1) + xChange) > 0)
		{
			x--;
			radiusError += xChange;
			xChange += 2;
		}
	}
}

/*Draw a hollow rectangle between positions X0,Y0 and X1,Y1 with specified colour*/
void ILI9341_Draw_Hollow_Rectangle_Coord(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1, uint16_t Colour)
{
	uint16_t 	X_length = 0;
	uint16_t 	Y_length = 0;
	uint8_t		Negative_X = 0;
	uint8_t 	Negative_Y = 0;
	int32_t 	Calc_Negative = 0;
	
	Calc_Negative = X1 - X0;
	if(Calc_Negative < 0) Negative_X = 1;
	Calc_Negative = 0;
	
	Calc_Negative = Y1 - Y0;
	if(Calc_Negative < 0) Negative_Y = 1;
	
	//DRAW HORIZONTAL!
	if(!Negative_X)
	{
		X_length = X1 - X0;		
	}
	else
	{
		X_length = X0 - X1;		
	}

	ILI9341_Draw_Horizontal_Line(X0, Y0, X_length, Colour);
	ILI9341_Draw_Horizontal_Line(X0, Y1, X_length, Colour);
	
	//DRAW VERTICAL!
	if(!Negative_Y)
	{
		Y_length = Y1 - Y0;		
	}
	else
	{
		Y_length = Y0 - Y1;		
	}

	ILI9341_Draw_Vertical_Line(X0, Y0, Y_length, Colour);
	ILI9341_Draw_Vertical_Line(X1, Y0, Y_length, Colour);
	
	if((X_length > 0)||(Y_length > 0)) 
	{
		ILI9341_Draw_Pixel(X1, Y1, Colour);
	}
}

/*Draw a filled rectangle between positions X0,Y0 and X1,Y1 with specified colour*/
void ILI9341_Draw_Filled_Rectangle_Coord(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1, uint16_t Colour)
{
	uint16_t 	X_length = 0;
	uint16_t 	Y_length = 0;
	uint8_t		Negative_X = 0;
	uint8_t 	Negative_Y = 0;
	int32_t 	Calc_Negative = 0;
	
	uint16_t X0_true = 0;
	uint16_t Y0_true = 0;
	
	Calc_Negative = X1 - X0;
	if(Calc_Negative < 0) Negative_X = 1;
	Calc_Negative = 0;
	
	Calc_Negative = Y1 - Y0;
	if(Calc_Negative < 0) Negative_Y = 1;
	
	//DRAW HORIZONTAL!
	if(!Negative_X)
	{
		X_length = X1 - X0;
		X0_true = X0;
	}
	else
	{
		X_length = X0 - X1;
		X0_true = X1;
	}
	
	//DRAW VERTICAL!
	if(!Negative_Y)
	{
		Y_length = Y1 - Y0;
		Y0_true = Y0;		
	}
	else
	{
		Y_length = Y0 - Y1;
		Y0_true = Y1;	
	}
	
	ILI9341_Draw_Rectangle(X0_true, Y0_true, X_length, Y_length, Colour);	
}

//////////////////////////////////////////////////////////////////////////////////////////////
void ILI9341_Random_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
	// Bresenham's algorithm - thx wikpedia

	int16_t steep = abs(y2 - y1) > abs(x2 - x1);

	if(steep)
	{
		swap(x1, y1);
		swap(x2, y2);
	}

	if(x1 > x2)
	{
		swap(x1, x2);
		swap(y1, y2);
	}

	int16_t dx, dy;

	dx = x2 - x1;
	dy = abs(y2 - y1);

	int16_t err = dx / 2;
	int16_t ystep;

	if(y1 < y2)
	{
		ystep = 1;
	}
	else
	{
		ystep = -1;
	}

	for(; x1 <= x2; x1++)
	{
		if(steep)
		{
			ILI9341_Draw_Pixel(y1, x1, color);
		}
		else
		{
			ILI9341_Draw_Pixel(x1, y1, color);
		}

		err -= dy;

		if(err < 0)
		{
			y1 += ystep;
			err += dx;
		}
	}
}

/////////////////////////////////////// Картинка из массива ///////////////////////////////////////////////////
void ILI9341_Draw_Image(const char *image_array, uint16_t x_coordinat, uint16_t y_coordinat, uint16_t img_width, uint16_t img_height, uint32_t s_img)
{
	ILI9341_Set_Address(x_coordinat, y_coordinat, img_width + x_coordinat - 1, img_height + y_coordinat - 1);

	DISP_DC_DATA;
	DISP_CS_SELECT;

	for(uint32_t i = 0; i < s_img; i++)
	{
		SPI1_DR_8bit = image_array[i];
		while(!(DISP_SPI->SR & SPI_SR_TXE));
	}

	while(!(DISP_SPI->SR & SPI_SR_TXE));
	while(DISP_SPI->SR & SPI_SR_BSY);
	DISP_CS_UNSELECT;
}


// Вывод символа
void ILI9341_WriteChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor)
{
    uint32_t i, b, j, h = 0;

    ILI9341_Set_Address(x, y, x + font.width - 1, y + font.height - 1);
	DISP_DC_DATA;
	DISP_CS_SELECT;
    for(i = 0; i < font.height; i++)
    {
        b = font.data[(ch - 32) * font.height + i];

        for(j = 0; j < font.width; j++)
        {
            if((b << j) & 0x8000)
            {
                ILI9341_Write_Data(color >> 8);
                ILI9341_Write_Data(color & 0xFF);
                //buf_disp[h] = color >> 8;
                //h++;
                //buf_disp[h] = color & 0xFF;
                //h++;
            }
            else
            {
                ILI9341_Write_Data(bgcolor >> 8);
                ILI9341_Write_Data(bgcolor & 0xFF);
                //buf_disp[h] = bgcolor >> 8;
                //h++;
                //buf_disp[h] = bgcolor & 0xFF;
                //h++;
            }

        }
    }

  //  HAL_SPI_Transmit_DMA(&hspi1, buf_disp, font.width*font.height*2);
  //  while(!dma_spi_fl);
    DISP_CS_UNSELECT;//*/
  //  dma_spi_fl=0;
}

// Вывод строки
void ILI9341_WriteString(uint16_t x, uint16_t y, const char* str, FontDef font, uint16_t color, uint16_t bgcolor)
{
    while(*str)
    {
        if(x + font.width >= ILI9341_SCREEN_WIDTH)
        {
            x = 0;
            y += font.height;

            if(y + font.height >= ILI9341_SCREEN_HEIGHT)
            {
                break;
            }

            if(*str == ' ')
            {
                str++;
                continue;
            }
        }

        ILI9341_WriteChar(x, y, *str, font, color, bgcolor);
        x += font.width;
        str++;
    }
}

///////////////////////////////////////// FROM SD ///////////////////////////////////////////////////
/*void ILI9341_Draw_Image_From_SD(char *name_file, uint16_t X, uint16_t Y, uint16_t xsize, uint16_t ysize)
{
	uint16_t s_buf = 512;

	uint8_t read_buff[512] = {0,};
	UINT bytes_read = 0;

	if(f_open(&myfile, name_file, FA_READ) != FR_OK)
	{
		HAL_UART_Transmit(&huart1, (uint8_t*)"OpenF_ER\n", 9, 1000);
	}

	int32_t size_file = f_size(&myfile);
 
	ILI9341_Set_Address(X, Y, X + xsize - 1, Y + ysize - 1);
	DISP_DC_DATA;
	DISP_CS_SELECT;

	while(size_file > 0)
	{
		if(size_file < 512) s_buf = size_file;
		f_read(&myfile, read_buff, s_buf, &bytes_read);
		HAL_SPI_Transmit_DMA(DISP_SPI_PTR, (uint8_t*)read_buff, s_buf);
		size_file = size_file - 512;
	}
	

	if(f_close(&myfile) != FR_OK)
	{
		HAL_UART_Transmit(&huart1, (uint8_t*)"CloseF_ER\n", 10, 1000);
	}

	while((HAL_SPI_GetState(DISP_SPI_PTR) != HAL_SPI_STATE_READY));
	
	DISP_CS_UNSELECT;
}*/

/////////////////////////////////////////// From_W25QFLASH ////////////////////////////////////////////////
/*void ILI9341_Draw_Image_From_W25QFLASH(uint32_t len, uint32_t adres, uint16_t X, uint16_t Y, uint16_t xsize, uint16_t ysize)
{
	uint8_t reciv[SIZE_RECIV] = {0,};
	int32_t len_img = len;
	int32_t offset = 0;

	spi1_tx_full_flag = 1;
	spi1_tx_half_flag = 1;
	spi2_rx_half_flag = 0;

	w25qxx.Lock = 1;

	W25QFLASH_CS_SELECT;

	W25qxx_Spi(0x0B);

	if(w25qxx.ID >= W25Q256)
		W25qxx_Spi((adres & 0xFF000000) >> 24);

	W25qxx_Spi((adres & 0xFF0000) >> 16);
	W25qxx_Spi((adres & 0xFF00) >> 8);
	W25qxx_Spi(adres & 0xFF);
	W25qxx_Spi(0);

	ILI9341_Set_Address(X, Y, X + xsize - 1, Y + ysize - 1);
	DISP_DC_DATA;
	DISP_CS_SELECT;

	while(len_img > 0)
	{
		if(len_img < SIZE_RECIV)
		{
			while(spi1_tx_full_flag == 0){}
			spi1_tx_full_flag = 0;

			HAL_SPI_Receive_DMA(W25QXX_SPI_PTR, (uint8_t*)reciv, len_img);

			while(spi2_rx_full_flag == 0){}
			spi2_rx_full_flag = 0;

			spi1_tx_full_flag = 0;
			HAL_SPI_Transmit_DMA(DISP_SPI_PTR, (uint8_t*)reciv, len_img);
			while(spi1_tx_full_flag == 0){}

			break;
		}

		while(spi1_tx_half_flag == 0){}/////////////////
		spi1_tx_half_flag = 0;

		HAL_SPI_Receive_DMA(W25QXX_SPI_PTR, (uint8_t*)reciv, SIZE_RECIV);


		while(spi2_rx_half_flag == 0){}
		spi2_rx_half_flag = 0;
		spi1_tx_full_flag = 0;

		HAL_SPI_Transmit_DMA(DISP_SPI_PTR, (uint8_t*)reciv, SIZE_RECIV);

		offset = (offset + SIZE_RECIV);
		len_img = (len_img - SIZE_RECIV);
	}

	W25QFLASH_CS_UNSELECT;
	w25qxx.Lock = 0;
	DISP_CS_UNSELECT;
}*/



