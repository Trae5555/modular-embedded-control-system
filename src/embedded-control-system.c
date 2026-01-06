#include <stdint.h>
#include "I2C0.h"
#include "CortexM.h"
#include "tm4c123gh6pm.h"
#include "my_procs_at_ece_uccs.h"
#include "my_defines_at_ece_uccs.h"

#define DS_CLK_PIN	2					//PA2 is the DS1620 clock line
#define DS_DQ_PIN 	3					//PA3 is the DS1620 data line
#define DS_RST_PIN	4					//PA4 is the DS1620 reset/transaction line

#define DS_CLK_HIGH() (GPIO_PORTA_DATA_R |=  (1<<DS_CLK_PIN))			//set DS clock high
#define DS_CLK_LOW()	(GPIO_PORTA_DATA_R &= ~(1<<DS_CLK_PIN))			//set DS clock low
#define DS_RST_HIGH() (GPIO_PORTA_DATA_R |=  (1<<DS_RST_PIN))			//set DS reset high
#define DS_RST_LOW()	(GPIO_PORTA_DATA_R &= ~(1<<DS_RST_PIN))			//set DS reset low
#define DS_DQ_HIGH()	(GPIO_PORTA_DATA_R |=  (1<<DS_DQ_PIN))			//set DS data high
#define DS_DQ_LOW()		(GPIO_PORTA_DATA_R &= ~(1<<DS_DQ_PIN))			//set DS data low

volatile uint8_t DIPstate;		//global variable for I2C Switch position

const uint8_t SevenSegmentTable[16] = 
{
	0xC0, // 0
	0xF9,	// 1
	0xA4,	// 2
	0XB0,	// 3
	0x99,	// 4
	0x92,	// 5
	0x82,	// 6
	0xF8,	// 7
	0x80,	// 8
	0x90,	// 9
	0x88,	// A
	0x83,	// B
	0xC6,	// C
	0xA1,	// D
	0x86,	// E
	0x8E	// F
};

void GPIO_PORTF_INIT(void)											//initialization for gpio port f
{
	SYSCTL_RCGCGPIO_R |= 0x20;							//enable clock for port f
	while((SYSCTL_PRGPIO_R & 0x20) == 0);		//wait for clock to be ready
	GPIO_PORTF_LOCK_R = 0x4C4F434B;					//unlock GPIO port F
	GPIO_PORTF_CR_R |= 0x01;								//enable changes to PF0
	GPIO_PORTF_AMSEL_R  &= ~0x0E;						//bitclear to disable analog select
	GPIO_PORTF_PCTL_R  &= ~0x0000FFF0;			//bitclear to disable port control
	GPIO_PORTF_DIR_R |= 0x0E;								//set direction of to configure as outputs
	//GPIO_PORTF_DIR_R &= ~0x11;							//set PF0 and PF4 as inputs
	GPIO_PORTF_AFSEL_R &= ~0x1F;						//disable analog function select
	GPIO_PORTF_DEN_R |= 0x1F;								//enable digital enable functions
	GPIO_PORTF_PUR_R |= 0x11;
}

void WATCHDOG_INIT(void)									//initialization watchdog timer
{
	SYSCTL_RCGCWD_R |= 0x01;								//enable clock for watchdog timer
	while((SYSCTL_PRWD_R & 0x01) == 0);			//wait for clock to be ready
	WATCHDOG0_LOCK_R = 0x1ACCE551;					//unlock watchdog timer
	WATCHDOG0_LOAD_R = 0xFFFFFF;						//set load value
	WATCHDOG0_ICR_R = 0x01;									//clear interupts for watchdog timer
	WATCHDOG0_CTL_R |= 0x03;								//enable watchdog timer
}

void TIMER3A_INIT(void)										//initialization for timer 3
{
	SYSCTL_RCGCTIMER_R |= 0x08;							//enable clock for timer 3
	while((SYSCTL_PRTIMER_R & 0x08) ==0);		//wait for clock to be ready
	TIMER3_CTL_R = 0x00;										//disbale timer 3 for set up(control register)
	TIMER3_CFG_R = 0x00000000;							//configure as a 32 bit timer(configuration register)
	TIMER3_TAMR_R = 0x02;										//set as periodic mode(mode register)
	TIMER3_TAILR_R = 16000000 - 1;					//set interval count value(interval load register)
	TIMER3_ICR_R = 0x01;										//clear timeout flag(interrupt clear register)
	TIMER3_IMR_R = 0x01;										//enable interrupt timeout flag(interrupt mask register)
	NVIC_EN1_R |= 0x08;											//enable interupts for timer 3(nested vector interrupt controller)
	TIMER3_CTL_R |= 0x01;										//re enable timer 3
}

void GPIO_PORTE_INIT(void)
{
	SYSCTL_RCGCGPIO_R |= 0x10;							//enable clock for port E
	while((SYSCTL_PRGPIO_R & 0x10) == 0);		//wait for clock to be enabled
	GPIO_PORTE_DIR_R &= ~0x02;							//enable port E as input
	GPIO_PORTE_AFSEL_R &= ~0x02;						//disable alternate functions for port E
	GPIO_PORTE_DEN_R |= 0x02;								//enable digital i/o for port E
	GPIO_PORTE_IS_R &= ~0x02;								//set interrupt sense to edge for port E
	GPIO_PORTE_IBE_R &= ~0x02;							//set interrupt edge to single for port E
	GPIO_PORTE_IEV_R &= ~0x02;							//set interrupt event to falling edge for port E
	GPIO_PORTE_ICR_R |= 0x02;								//clear interrupt for port E
	GPIO_PORTE_IM_R |= 0x02;								//enable interrupt for port E
	NVIC_EN0_R |= 0x10;											//enable interrrupt for port E
	GPIO_PORTE_PUR_R |= 0x02;								//enable pull up resistor for port E
}

void DS_INIT(void)
{
	SYSCTL_RCGCGPIO_R |= 0x01;							//enable clock for port A
	while((SYSCTL_PRGPIO_R & 0x01) == 0);		//Wait for clock to be ready. 
	GPIO_PORTA_DIR_R |= (1<<DS_CLK_PIN)|(1<<DS_DQ_PIN)|(1<<DS_RST_PIN);			//set clk pin, dq pin, reset pin as direction output
	GPIO_PORTA_DEN_R |= (1<<DS_CLK_PIN)|(1<<DS_DQ_PIN)|(1<<DS_RST_PIN);			//set clk pin, dq pin, reset pin as digital output
	GPIO_PORTA_AFSEL_R &= ~((1<<DS_CLK_PIN)|(1<<DS_DQ_PIN)|(1<<DS_RST_PIN));		//disable alternate funtions for clk, data, reset
	GPIO_PORTA_PCTL_R &= ~((0xF<<(4*DS_CLK_PIN))|(0xF<<(4*DS_DQ_PIN))|(0xF<<(4*DS_RST_PIN)));			//Disable peripherial controls for clk, data, reset
}

static inline void DS_PulseClock(void)				//begin user function for clock pulse
{
	DS_CLK_HIGH();															//drive clock high
	
	DS_CLK_LOW();																//drive clock low
}

static void DS_SendByte(uint8_t b)						//begin user function for sending LSB of b 
{
	for(int i = 0; i < 8; i++)									//loop over 8 LSB of byte
	{			
		if(b & 0x01)															//if bit is high, drive high
			DS_DQ_HIGH();
		else
			DS_DQ_LOW();														//else, drive bit low
			DS_PulseClock();												//clock bit into DS1620
			b >>= 1;																//shift bit, next bit becomes LSB
	}
}
	
void Watchdog_Service(void)								//watchdoge service routine to reset watchdog timer
{
	WATCHDOG0_ICR_R = 0x01;									//clear watchdog timer
}
void OutLCD(int8_t data, int8_t isCommand);
void InitLCD(void);
void LCDLDel(void);
void LCDSDel(void);

void WrCfg(uint8_t config);
uint8_t RdCfg(void);
void StartCnv(void);
uint16_t RdTemp(void);

void OutputString(char *str);
void LCD_Clear(void);
void delay(uint32_t count);
void OutputNumTo7Seg( unsigned int number_, unsigned char mode_, unsigned char base_);

void RotateLongString(void);
char scrollShift[64] = "Electrical and Computer Engineering    ";
uint32_t len = 39;

#define DIPSwitchAddress  0x20			//PCF8574 for DIP switch
uint8_t SevenSegment1Address = 0x23;			//PCF8574 for first seven segment display
uint8_t SevenSegment2Address = 0x24;			//PCF8574 for second seven segment diplay
uint8_t PCF1 = 0x21;
uint8_t PCF2 = 0x22;
uint8_t flag =0;

		unsigned char SwitchState;
		volatile unsigned char countDirection=1;		//global variable for count direction
		volatile unsigned char displayMode=2;				//global variable for display mode state
		volatile unsigned char numberBase=10;			//global variable for base diplay mode
		volatile uint8_t swapNames = 0;
		volatile uint8_t clearLCD = 0;
		volatile uint8_t LongString = 0;
		volatile uint8_t tempMode = 0;

int main(void)										//begin main function
	{
		I2C_Init();
		GPIO_PORTF_INIT();						//call gpio port f initialization
		GPIO_PORTE_INIT();						//call gpio port E initialization
		WATCHDOG_INIT();							//call watchdog initialization
		TIMER3A_INIT();								//call timer initialization
		I2C_Recv(DIPSwitchAddress);		//call I2C to begin initial read
		DS_INIT();										//initialize port A
		
		{
			uint8_t cfg = RdCfg();			//start continuous conversation mode
			if((cfg & 0x01) == 0)				//if 0, then one send
			{
				WrCfg(cfg | 0x01);				//if 1, then continous
			}	
		}
		StartCnv();										//trigger first conversation
		
		delay(2000000);
		InitLCD();
		
		OutLCD (0x80, 0);
		LCDLDel();
		OutputString("     TRAE      ");
		
		OutLCD(0xC0, 1);
		LCDLDel();
		
		LCDLDel();
		OutputString("     ARANDA     ");
		
	volatile uint8_t count = 0;	//count variable for counter
		
  while (1) 											//begin while loop
	{
		Watchdog_Service();
		
		if (tempMode)												//if switch is flipped
		{
			int16_t raw = RdTemp();						//read byte from readtemp user function
			float celcius = (raw & 1)					//ternary to extract C
											? ((raw >> 1) + 0.5f)								//If it’s 1, the temperature was an odd half-degree/ take raw >> +0.5
											:	(raw >> 1);												//If it’s 0, it was a whole degree, else, whole number
			float fahrenheit = celcius * 9.0f/5.0f + 32.0f;			//conversion for fahrenheit
			
			char line1[17], line2[17];													//allocate 17 bytes to store 16 char plus terminating 
			sprintf(line1, "   %.1f\xDF" "C   ",celcius);				//prints the float with one digit after the decimal, print degree symbol, 
			sprintf(line2, "   %.1f\xDF" "F   ",fahrenheit);		//similar formating for fahrenheit, sprintf to combine numbers and text in one call
			
			OutLCD(0x01,1);
			LCDLDel();
			OutLCD(0x80,0);
			OutputString(line1);
			OutLCD(0xC0,1);
			OutputString(line2);
			
			delay(2000000);
		}
		else if(LongString)
{
	RotateLongString();
		OutLCD(0x80, 1);
		LCDLDel();
	OutputString(scrollShift);
	
		OutLCD(0xC0, 1);
		LCDLDel();
	OutputString(scrollShift + 16);
	
	LCDLDel();
}	
else
{
	if(countDirection) count++;
	else count--;
	
		if (flag == 1) 
			{
					flag=0;
		if(clearLCD)
			{ 
				OutLCD (0x01, 1);
				LCDLDel();
			}		
				if(swapNames)
				{
					OutLCD(0x80, 1);
					LCDLDel();
					OutputString("     ARANDA     ");
					
					OutLCD(0xC0, 1);
					LCDLDel();
					OutputString("      TRAE      ");
				}
				
				else
				{
					OutLCD(0x80, 1);
					LCDLDel();
					OutputString("      TRAE      ");
				
					OutLCD(0xC0, 1);
					LCDLDel();
					OutputString("     ARANDA     ");
				}
			}
			delay(500000);							//delay to see number change
			
	OutputNumTo7Seg( count, displayMode, numberBase);		//call function to determine state of seven segment
	}
}
}
	
volatile uint8_t led_counter = 0;	//define counter variable
			
void Timer3A_Handler(void)				//interrupt service routine
{
	TIMER3_ICR_R = 0x01;						//clear timer interrupt flag
	
	led_counter++;
	
	if(led_counter > 7)							//count to 8 differnt led states
	{
		led_counter = 0;							//reset led counter
	}
	switch(led_counter)							//switch between led states
	{
	case 0:
			GPIO_PORTF_DATA_R = 0x00;		//all leds off
		break;
	case 1:
			GPIO_PORTF_DATA_R = 0x02;		//red led on
		break;
	case 2:
			GPIO_PORTF_DATA_R = 0x08;		//green led on
		break;
	case 3:
			GPIO_PORTF_DATA_R = 0x04;		//blue led on
		break;
	case 4:
			GPIO_PORTF_DATA_R = 0x06;		//red and blue led on
		break;
	case 5: 
			GPIO_PORTF_DATA_R = 0x0A;		//green and red led on
		break;
	case 6: 
			GPIO_PORTF_DATA_R = 0x0C;		//green and blue led on
		break;
	case 7:
			GPIO_PORTF_DATA_R = 0x0E;		//all leds on
		break;
	}
}

void GPIOPortE_Handler(void)													//start ISR for DIP switch/seven segment display
{
	//uint8_t LowerBits, UpperBits;											//define variables for ISR
//	unsigned char SwitchState;													//create variable for switch positions
	
	GPIO_PORTE_ICR_R |= 0x02;														//clear interrupt for port E
	
	Watchdog_Service();																	//reset watchdog timer to prevent reset
	
	DIPstate = I2C_Recv(DIPSwitchAddress);							//read DIP state value through I2C
	DIPstate = I2C_Recv(DIPSwitchAddress);							//read DIP state value through I2C(delay for clock to adjust)

	
	//LowerBits = DIPstate & 0x0F;												//mask the lower bits being read from the DIP switch	
	//UpperBits = (DIPstate & 0xF0) >> 4;									//read the upper bits being read from the DIP switch and shift for seven segment display
		
	SwitchState = DIPstate;																//read from I2C variable and mask for only 4 switches

			countDirection = SwitchState & 0x01;							//determine countDirection switch position
			displayMode=(SwitchState & 0x6)>>1;								//determine the display mode by shifting bit and determinine values 0-3
	
	if (SwitchState & 0x08)
	{
		numberBase = 16;
	}
	else
	{
		numberBase = 10;
	}
	
	swapNames = (SwitchState & 0x10) ? 1 : 0;
	clearLCD = (SwitchState & 0x20) ? 1 : 0;
	LongString = (SwitchState & 0x40) ? 1 : 0;
	tempMode = (SwitchState & 0x80) ? 1 : 0;
	flag=1;

}

void OutputNumTo7Seg( unsigned int number_, unsigned char mode_, unsigned char base_)			//start output function
{
	unsigned char leftDigit;										//variable for left seven segment
	unsigned char rightDigit;										//varaible for right seven segment
	

		leftDigit = (number_ / base_)% base_;			//extract second least significant digit
		rightDigit = number_ % base_;							//extract least significant digit

	
	if (mode_ == 0)															//if statment to determine mode display position for seven segments
	{
		I2C_Send1(SevenSegment1Address, SevenSegmentTable[rightDigit]);
	}
	else if (mode_ == 1)
	{
		I2C_Send1(SevenSegment2Address, SevenSegmentTable[rightDigit]);
	}
	else if (mode_ == 2)
	{
		I2C_Send1(SevenSegment1Address, SevenSegmentTable[rightDigit]);
		I2C_Send1(SevenSegment2Address, SevenSegmentTable[leftDigit]);

	}


}
void delay(uint32_t count)			//start function for delay
	{
	volatile uint32_t i;					//variable for delay function
	
	for(i = 0; i < count; i++)		//run function to create delay
	{
		
	}
}

void LCDSDel (void)
{
	int i;
	
	for(i = 0; i < 57; i++)
	{
			Watchdog_Service();	
	}
}

void LCDLDel (void)
{
	int i;
	
	for(i = 0; i< 2800; i++)
	{
			Watchdog_Service();	
	}
}

void OutLCD(int8_t data, int8_t isCommand)
{
	uint8_t controlByte = 0x00;
	
	if (isCommand)
	{
		controlByte &= ~(1 << 0);
	}
	else
	{
		controlByte |= (1 << 0);
	}
	
	controlByte &= ~(1 << 2);
	
	I2C_Send1(PCF1, controlByte);
	I2C_Send1(PCF2, controlByte);
	LCDSDel();
	
  I2C_Send1(PCF2, data);  // Send it to both PCF1 and PCF2
	
	controlByte |= (1 << 2);
	I2C_Send1(PCF1, controlByte);
	
	LCDSDel();
	
	controlByte &= ~(1 << 2);
	I2C_Send1(PCF1, controlByte);
	
	LCDLDel();
}

void InitLCD(void)
{
	OutLCD(0x3C, 1);
	LCDLDel();
	
	OutLCD (0x14, 1);
	LCDSDel ();
	
	OutLCD(0x0C, 1);
	LCDSDel ();
	
	OutLCD (0x06, 1);
	LCDSDel ();
	
	OutLCD (0x01, 1);
	LCDLDel ();
}

void OutputString(char* str)
{
	while (*str)
	{
		OutLCD (*str, 0);
		LCDSDel ();
		str++;
	}
}

void RotateLongString(void)
{
	uint8_t rotateLeft = (SwitchState & 0x10) ? 1 : 0;
	
	if(rotateLeft == 0)
	{
		char temp = scrollShift[0];
		for(uint32_t i = 0; i <len -1; i++)
		{
			scrollShift[i] = scrollShift[i +1];
		}
		scrollShift[len - 1] = temp;
	}
	else
	{
		char temp = scrollShift[len -1];
		for (int i = len - 1; i > 0; i--)
		{
			scrollShift[i] = scrollShift[i - 1];
		}
		scrollShift[0] = temp;
	}
}

void WrCfg(uint8_t config)								//start write user function
{
	GPIO_PORTA_DIR_R |= (1<<DS_DQ_PIN);			//ensure dq is output
	DS_RST_HIGH();													//set reset high to begin transaction
	DS_SendByte(0x0C);											//send write configuration command
	DS_SendByte(config);										//send 8-bit config value
	DS_RST_LOW();														//pull reset low to end transaction
}

uint8_t RdCfg(void)												//start read user function
{
	uint8_t cfg = 0;												
	GPIO_PORTA_DIR_R |= (1<<DS_DQ_PIN);			//dq must be output to send
	DS_RST_HIGH();													//start transaction
	DS_SendByte(0xAC);											//send read configuration
	GPIO_PORTA_DIR_R &= ~(1<<DS_DQ_PIN);		//set dq as input so ds1620 drives
	for(int i = 0; i < 8; i++)							//shift through each LSB to read
	{
		cfg |= ((GPIO_PORTA_DATA_R >> DS_DQ_PIN) & 0x01) << i;		//shift through 8 bits from data
		DS_PulseClock();																					//pulse to shift
	}
	DS_RST_LOW();																								//End transaction
	return cfg;																									//return configuration byte
}

void StartCnv(void)														//start continuous conversation mode user function
{
	GPIO_PORTA_DIR_R |= (1<<DS_DQ_PIN);					//set dq as output
	DS_RST_HIGH();															//start transaction
	DS_SendByte(0xEE);													//send convert temperature command
	DS_RST_LOW();																//End transaction
}

uint16_t RdTemp(void)													//start read temperature user function
{
	uint16_t raw = 0;														
	GPIO_PORTA_DIR_R |= (1<<DS_DQ_PIN);					//set dq as output
	DS_RST_HIGH();															//start transaction
	DS_SendByte(0xAA);													//send read temperature command
	GPIO_PORTA_DIR_R &= ~(1<<DS_DQ_PIN);				//set dq as input for reading
	for(int i = 0; i < 9; i++)									//start loop for 8 bit read
	{
		raw |= ((GPIO_PORTA_DATA_R >> DS_DQ_PIN) & 0x01) << i;			//shift through 8 bites
		DS_PulseClock();																						//pulse to shift
	}
	DS_RST_LOW();																									//End transaction
	
	if(raw & (1<<8)) raw |= 0xFE00;								//bit 8 = sign for negative temperatures.
	return (int16_t)raw;
}
