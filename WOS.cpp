#include <Arduino.h>
#include <EEPROM.h>
#define WOS_CPP_FILE
#include "WOS_Config.h"
#include "WOS.h"

// Machine State
#define WOS_NUM_LAMP_BANKS  8

volatile byte LampStates[WOS_NUM_LAMP_BANKS], LampDim1[WOS_NUM_LAMP_BANKS], LampDim2[WOS_NUM_LAMP_BANKS];
volatile byte LampFlashPeriod[WOS_MAX_LAMPS];
byte DimDivisor1 = 2;
byte DimDivisor2 = 3;

#ifndef WOS_NUM_DIGITS
#define WOS_NUM_DIGITS  6
#endif

#ifndef INTERRUPT_OCR1A_COUNTER
#define INTERRUPT_OCR1A_COUNTER 16574
#endif

volatile byte DisplayDigits[4][WOS_NUM_DIGITS];
volatile byte DisplayCreditDigits[2];
volatile byte DisplayCreditDigitEnable;
volatile byte DisplayBIPDigits[2];
volatile byte DisplayBIPDigitEnable;
volatile byte DisplayDigitEnable[4];
volatile byte BoardLEDs = 0;

#define SWITCH_COLUMNS      8
#define SWITCH_STACK_SIZE   64
#define SWITCH_STACK_EMPTY  0xFF
volatile byte SwitchStackFirst;
volatile byte SwitchStackLast;
volatile byte SwitchStack[SWITCH_STACK_SIZE];
volatile byte SwitchesMinus2[SWITCH_COLUMNS];
volatile byte SwitchesMinus1[SWITCH_COLUMNS];
volatile byte SwitchesNow[SWITCH_COLUMNS];
volatile boolean UpDownSwitch = false;

#define SOUND_STACK_SIZE  32
#define SOUND_STACK_EMPTY 0x0000
volatile byte SoundStackFirst;
volatile byte SoundStackLast;
volatile unsigned short SoundStack[SOUND_STACK_SIZE];

#define TIMED_SOUND_STACK_SIZE  64
struct TimedSoundEntry {
  byte inUse;
  unsigned long pushTime;
  unsigned short soundNumber;
  byte numPushes;
};
TimedSoundEntry TimedSoundStack[TIMED_SOUND_STACK_SIZE];

#define SOLENOID_STACK_SIZE 250
#define SOLENOID_STACK_EMPTY 0xFF
unsigned short ContinuousSolenoidBits = 0;
volatile byte SolenoidStackFirst;
volatile byte SolenoidStackLast;
volatile byte SolenoidStack[SOLENOID_STACK_SIZE];
boolean SolenoidStackEnabled = true;
volatile byte CurrentSolenoidByte = 0xFF;

#define TIMED_SOLENOID_STACK_SIZE 48
struct TimedSolenoidEntry {
  byte inUse;
  unsigned long pushTime;
  byte solenoidNumber;
  byte numPushes;
  byte disableOverride;
};
TimedSolenoidEntry TimedSolenoidStack[TIMED_SOLENOID_STACK_SIZE];


#ifdef WILLIAMS_11_MPU

const uint16_t FourteenSegmentASCII[96] = {
  0x0000, /* (space) */
  0x4006, /* ! */
  0x0202, /* " */
  0x12CE, /* # */
  0x12ED, /* $ */
  0x3FE4, /* % */
  0x2359, /* & */
  0x0200, /* ' */
  0x2400, /* ( */
  0x0900, /* ) */
  0x3FC0, /* * */
  0x12C0, /* + */
  0x0800, /* , */
  0x00C0, /* - */
  0x4000, /* . */
  0x0C00, /* / */
  0x0C3F, /* 0 */
  0x0406, /* 1 */
  0x00DB, /* 2 */
  0x008F, /* 3 */
  0x00E6, /* 4 */
  0x2069, /* 5 */
  0x00FD, /* 6 */
  0x0007, /* 7 */
  0x00FF, /* 8 */
  0x00EF, /* 9 */
  0x1200, /* : */
  0x0A00, /* ; */
  0x2440, /* < */
  0x00C8, /* = */
  0x0980, /* > */
  0x5083, /* ? */
  0x02BB, /* @ */
  0x00F7, /* A */
  0x128F, /* B */
  0x0039, /* C */
  0x120F, /* D */
  0x0079, /* E */
  0x0071, /* F */
  0x00BD, /* G */
  0x00F6, /* H */
  0x1209, /* I */
  0x001E, /* J */
  0x2470, /* K */
  0x0038, /* L */
  0x0536, /* M */
  0x2136, /* N */
  0x003F, /* O */
  0x00F3, /* P */
  0x203F, /* Q */
  0x20F3, /* R */
  0x00ED, /* S */
  0x1201, /* T */
  0x003E, /* U */
  0x0C30, /* V */
  0x2836, /* W */
  0x2D00, /* X */
  0x00EE, /* Y */
  0x0C09, /* Z */
  0x0039, /* [ */
  0x2100, /* \ */
  0x000F, /* ] */
  0x2800, /* ^ */
  0x0008, /* _ */
  0x0100, /* ` */
  0x1058, /* a */
  0x2078, /* b */
  0x00D8, /* c */
  0x088E, /* d */
  0x0858, /* e */
  0x14C0, /* f */
  0x048E, /* g */
  0x1070, /* h */
  0x1000, /* i */
  0x0A10, /* j */
  0x3600, /* k */
  0x0030, /* l */
  0x10D4, /* m */
  0x1050, /* n */
  0x00DC, /* o */
  0x0170, /* p */
  0x0486, /* q */
  0x0050, /* r */
  0x2088, /* s */
  0x0078, /* t */
  0x001C, /* u */
  0x0810, /* v */
  0x2814, /* w */
  0x2D00, /* x */
  0x028E, /* y */
  0x0848, /* z */
  0x0949, /* { */
  0x1200, /* | */
  0x2489, /* } */
  0x0CC0, /* ~ */
  0x0000, /* (del) */
};

#endif

/*
 * PIA I - 0x2800
 *    Displays
 * 
 *    PA0-PA3 are decoded into 16 display output strobes
 *    PA4-PA7 are inputs for dip switches
 *    CA1 is diagnostic switch input
 *    CA2 is R/E for DIP switches & LEDs
 *    
 *    PB0-PB3 are BCD2 outputs
 *    PB4-BP7 are BCD1 outputs
 *    CB1 is diagnostic switch input
 *    CB2 is ST6 (special trigger 6) for solenoid board
 * 
 * PIA II - 0x3000
 *    Switches
 *    
 *    PA0-PA7 are switch inputs
 *    CA1 is nothing
 *    CA2 is ST4
 *    PB0-PB7 are switch outputs
 *    CB1 is nothing
 *    CB2 is ST3
 *    
 * PIA III - 0x2400
 *    Lamps
 *    
 *    PA0-PA7 are lamp rows (grounds)
 *    CA1 is nothing
 *    CA2 is ST2
 *    PB0-PB7 are lamp strobes (power)
 *    CB1 is nothing
 *    CB2 is ST1
 *    
 * PIA IV - 0x2200
 *    Solenoids
 *    
 *    PA0-PA7 are solenoid drives 1-8 (1=on)
 *    CA1 is nothing
 *    CA2 is ST5
 *    PB0-PB7 are solenoid drives 8-16 (1=on)
 *    CB1 is nothing
 *    CB2 turns on mometary solenoids and flippers
 *     
 */



#define PIA_DISPLAY_PORT_A      0x2800
#define PIA_DISPLAY_CONTROL_A   0x2801
#define PIA_DISPLAY_PORT_B      0x2802
#define PIA_DISPLAY_CONTROL_B   0x2803
#define PIA_SWITCH_PORT_A       0x3000
#define PIA_SWITCH_CONTROL_A    0x3001
#define PIA_SWITCH_PORT_B       0x3002
#define PIA_SWITCH_CONTROL_B    0x3003
#define PIA_LAMPS_PORT_A        0x2400
#define PIA_LAMPS_CONTROL_A     0x2401
#define PIA_LAMPS_PORT_B        0x2402
#define PIA_LAMPS_CONTROL_B     0x2403
#define PIA_SOLENOID_PORT_A     0x2200
#define PIA_SOLENOID_CONTROL_A  0x2201
#define PIA_SOLENOID_PORT_B     0x2202
#define PIA_SOLENOID_CONTROL_B  0x2203
#ifdef WILLIAMS_11_MPU
#define PIA_SOUND_11_PORT_A             0x2100
#define PIA_SOUND_11_CONTROL_A          0x2101
#define PIA_SOLENOID_11_PORT_B          0x2102
#define PIA_SOLENOID_11_CONTROL_B       0x2103
#endif

#define WOS_PINS_OUTPUT true
#define WOS_PINS_INPUT false



#if (RPU_VERSION==1)
#define RPU_VMA_PIN         4
#define RPU_RW_PIN          5
#define RPU_PHI2_PIN        3
#define RPU_SWITCH_PIN      13
#define RPU_BUFFER_DISABLE  2
#define RPU_HALT_PIN        14
#define RPU_RESET_PIN       14

/*
 * Helper Functions
 */
void WOS_SetAddressPinsDirection(boolean pinsOutput) {  
  for (int count=0; count<16; count++) {
    pinMode(16+count, pinsOutput?OUTPUT:INPUT);
  }
}

void WOS_SetDataPinsDirection(boolean pinsOutput) {
  for (int count=0; count<7; count++) {
    pinMode(6+count, pinsOutput?OUTPUT:INPUT);
  }
  pinMode(15, pinsOutput?OUTPUT:INPUT);
}


// REV 1 HARDWARE
void WOS_DataWrite(int address, byte data) {
  
  // Set data pins to output
  DDRH = DDRH | 0x78;
  DDRB = DDRB | 0x70;
  DDRJ = DDRJ | 0x01;

  // Set R/W to LOW
  PORTE = (PORTE & 0xF7);

  // Put data on pins
  // Lower Nibble goes on PortH3 through H6
  PORTH = (PORTH&0x87) | ((data&0x0F)<<3);
  // Bits 4-6 go on PortB4 through B6
  PORTB = (PORTB&0x8F) | ((data&0x70));
  // Bit 7 goes on PortJ0
  PORTJ = (PORTJ&0xFE) | (data>>7);  

  // Set up address lines
  PORTH = (PORTH & 0xFC) | ((address & 0x0001)<<1) | ((address & 0x0002)>>1); // A0-A1
  PORTD = (PORTD & 0xF0) | ((address & 0x0004)<<1) | ((address & 0x0008)>>1) | ((address & 0x0010)>>3) | ((address & 0x0020)>>5); // A2-A5
  PORTA = ((address & 0x3FC0)>>6); // A6-A13
  PORTC = (PORTC & 0x3F) | ((address & 0x4000)>>7) | ((address & 0x8000)>>9); // A14-A15

  // Set clock low
  PORTE &= ~0x20;
  //delayMicroseconds(3);

  // Pulse VMA over one clock cycle
  // Set VMA ON
  PORTG = PORTG | 0x20;

  // Set clock high
  PORTE |= 0x20;
  //delayMicroseconds(3);

  // Set clock low
  PORTE &= ~0x20;
  //delayMicroseconds(3);

  // Set clock high
  PORTE |= 0x20;

  // Set VMA OFF
  PORTG = PORTG & 0xDF;

  // Unset address lines
  PORTH = (PORTH & 0xFC);
  PORTD = (PORTD & 0xF0);
  PORTA = 0;
  PORTC = (PORTC & 0x3F);
  
  // Set R/W back to HIGH
  PORTE = (PORTE | 0x08);

  // Set data pins to input
  DDRH = DDRH & 0x87;
  DDRB = DDRB & 0x8F;
  DDRJ = DDRJ & 0xFE;
  
}



byte WOS_DataRead(int address) {
  
  // Set data pins to input
  DDRH = DDRH & 0x87;
  DDRB = DDRB & 0x8F;
  DDRJ = DDRJ & 0xFE;

  // Set R/W to HIGH
  DDRE = DDRE | 0x08;
  PORTE = (PORTE | 0x08);

  // Set up address lines
  PORTH = (PORTH & 0xFC) | ((address & 0x0001)<<1) | ((address & 0x0002)>>1); // A0-A1
  PORTD = (PORTD & 0xF0) | ((address & 0x0004)<<1) | ((address & 0x0008)>>1) | ((address & 0x0010)>>3) | ((address & 0x0020)>>5); // A2-A5
  PORTA = ((address & 0x3FC0)>>6); // A6-A13
  PORTC = (PORTC & 0x3F) | ((address & 0x4000)>>7) | ((address & 0x8000)>>9); // A14-A15

  // Set clock low
  PORTE &= ~0x20;
  //delayMicroseconds(3);

  // Pulse VMA over one clock cycle
  // Set VMA ON
  PORTG = PORTG | 0x20;

  // Set clock high
  PORTE |= 0x20;
  //delayMicroseconds(3);

  // Set clock low
  PORTE &= ~0x20;
  //delayMicroseconds(3);
  
  // Set clock high
  PORTE |= 0x20;

  byte inputData;
  inputData = (PINH & 0x78)>>3;
  inputData |= (PINB & 0x70);
  inputData |= PINJ << 7;

  // Set VMA OFF
  PORTG = PORTG & 0xDF;

  // Set R/W to LOW
  PORTE = (PORTE & 0xF7);

  // Unset address lines
  PORTH = (PORTH & 0xFC);
  PORTD = (PORTD & 0xF0);
  PORTA = 0;
  PORTC = (PORTC & 0x3F);

  return inputData;
}


void WaitClockCycle(int numCycles=1) {
  for (int count=0; count<numCycles; count++) {
    // Wait while clock is low
    while(!(PINE & 0x20));
  
    // Wait for a falling edge of the clock
    while((PINE & 0x20));
  }
}


#elif (RPU_VERSION==2)
#define RPU_VMA_PIN           40
#define RPU_RW_PIN            3
#define RPU_PHI2_PIN          39
#define RPU_SWITCH_PIN        38
#define RPU_BUFFER_DISABLE    5
#define RPU_HALT_PIN          41
#define RPU_RESET_PIN         42
#define RPU_DIAGNOSTIC_PIN    44

void WOS_SetAddressPinsDirection(boolean pinsOutput) {  
  for (int count=0; count<16; count++) {
    pinMode(A0+count, pinsOutput?OUTPUT:INPUT);
  }
}

void WOS_SetDataPinsDirection(boolean pinsOutput) {
  for (int count=0; count<8; count++) {
    pinMode(22, pinsOutput?OUTPUT:INPUT);
  }
}


// REVISION 2 HARDWARE
void WOS_DataWrite(int address, byte data) {
  
  // Set data pins to output
  DDRA = 0xFF;

  // Set R/W to LOW
  PORTE = (PORTE & 0xDF);

  // Put data on pins
  PORTA = data;

  // Set up address lines
  PORTF = (byte)(address & 0x00FF);
  PORTK = (byte)(address/256);

  // Set clock low (PG2) (if 6802/8)
  PORTG &= ~0x04;
  //delayMicroseconds(3);

  // Pulse VMA over one clock cycle
  // Set VMA ON
  PORTG = PORTG | 0x02;

  // Set clock high
  PORTG |= 0x04;
  //delayMicroseconds(3);

  // Set clock low
  PORTG &= ~0x04;
  //delayMicroseconds(3);

  // Set clock high
  PORTG |= 0x04;

  // Set VMA OFF
  PORTG = PORTG & 0xFD;

  // Unset address lines
  PORTF = 0x00;
  PORTK = 0x00;
  
  // Set R/W back to HIGH
  PORTE = (PORTE | 0x20);

  // Set data pins to input
  DDRA = 0x00;
  
}



byte WOS_DataRead(int address) {
  
  // Set data pins to input
  DDRA = 0x00;

  // Set R/W to HIGH
  DDRE = DDRE | 0x20;
  PORTE = (PORTE | 0x20);

  // Set up address lines
  PORTF = (byte)(address & 0x00FF);
  PORTK = (byte)(address/256);

  // Set clock low
  PORTG &= ~0x04;
  //delayMicroseconds(3);

  // Pulse VMA over one clock cycle
  // Set VMA ON
  PORTG = PORTG | 0x02;

  // Set clock high
  PORTG |= 0x04;
  //delayMicroseconds(3);

  // Set clock low
  PORTG &= ~0x04;
  //delayMicroseconds(3);
  
  // Set clock high
  PORTG |= 0x04;

  byte inputData;
  inputData = PINA;

  // Set VMA OFF
  PORTG = PORTG & 0xFD;

  // Set R/W to LOW
  PORTE = (PORTE & 0xDF);

  // Unset address lines
  PORTF = 0x00;
  PORTK = 0x00;

  return inputData;
}


#else
#error "RPU Hardware Definition Not Recognized"
#endif

void WOS_InitializePIAs() {
  WOS_DataWrite(PIA_DISPLAY_CONTROL_A, 0x31);
  WOS_DataWrite(PIA_DISPLAY_PORT_A, 0xFF);
  WOS_DataWrite(PIA_DISPLAY_CONTROL_A, 0x3D);
  WOS_DataWrite(PIA_DISPLAY_PORT_A, 0xC0);

  WOS_DataWrite(PIA_DISPLAY_CONTROL_B, 0x31);
  WOS_DataWrite(PIA_DISPLAY_PORT_B, 0xFF);
  WOS_DataWrite(PIA_DISPLAY_CONTROL_B, 0x3D);
  WOS_DataWrite(PIA_DISPLAY_PORT_B, 0x00);

  WOS_DataWrite(PIA_SWITCH_CONTROL_A, 0x38);
  WOS_DataWrite(PIA_SWITCH_PORT_A, 0x00);
  WOS_DataWrite(PIA_SWITCH_CONTROL_A, 0x3C);

  WOS_DataWrite(PIA_SWITCH_CONTROL_B, 0x38);
  WOS_DataWrite(PIA_SWITCH_PORT_B, 0xFF);
  WOS_DataWrite(PIA_SWITCH_CONTROL_B, 0x3C);
  WOS_DataWrite(PIA_SWITCH_PORT_B, 0x00);

  WOS_DataWrite(PIA_LAMPS_CONTROL_A, 0x38);
  WOS_DataWrite(PIA_LAMPS_PORT_A, 0xFF);
  WOS_DataWrite(PIA_LAMPS_CONTROL_A, 0x3C);
  WOS_DataWrite(PIA_LAMPS_PORT_A, 0xFF);  

  WOS_DataWrite(PIA_LAMPS_CONTROL_B, 0x38);
  WOS_DataWrite(PIA_LAMPS_PORT_B, 0xFF);
  WOS_DataWrite(PIA_LAMPS_CONTROL_B, 0x3C);
  WOS_DataWrite(PIA_LAMPS_PORT_B, 0x00);

#ifndef WILLIAMS_11_MPU
  WOS_DataWrite(PIA_SOLENOID_CONTROL_A, 0x38);
  WOS_DataWrite(PIA_SOLENOID_PORT_A, 0xFF);
  WOS_DataWrite(PIA_SOLENOID_CONTROL_A, 0x3C);
#endif
  WOS_DataWrite(PIA_SOLENOID_PORT_A, 0x00);

#ifndef WILLIAMS_11_MPU
  WOS_DataWrite(PIA_SOLENOID_CONTROL_B, 0x30);
  WOS_DataWrite(PIA_SOLENOID_PORT_B, 0xFF);
  WOS_DataWrite(PIA_SOLENOID_CONTROL_B, 0x34);
  WOS_DataWrite(PIA_SOLENOID_PORT_B, 0x00);
#endif

#ifdef WILLIAMS_11_MPU
  WOS_DataWrite(PIA_SOLENOID_11_CONTROL_B, 0x38);
  WOS_DataWrite(PIA_SOLENOID_11_PORT_B, 0xFF);
  WOS_DataWrite(PIA_SOLENOID_11_CONTROL_B, 0x3C);
  WOS_DataWrite(PIA_SOLENOID_11_PORT_B, 0x00);
#endif

}

void WOS_SetBoardLEDs(boolean LED1, boolean LED2) {
  BoardLEDs = 0;
  if (LED1) BoardLEDs |= 0x20;
  if (LED2) BoardLEDs |= 0x10;
}



void WOS_SetupInterrupt() {
  cli();
  //set timer1 interrupt at 1Hz
  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register for selected increment
//  OCR1A = 16574;
  OCR1A = INTERRUPT_OCR1A_COUNTER;
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS10 and CS12 bits for 1024 prescaler
  TCCR1B |= (0 << CS12) | (0 << CS11) | (1 << CS10);  
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  sei();
}


void WOS_ClearVariables() {
  for (byte count=0; count<WOS_NUM_LAMP_BANKS; count++) {
    LampStates[WOS_NUM_LAMP_BANKS] = 0xFF;
    LampDim1[WOS_NUM_LAMP_BANKS] = 0x00;
    LampDim2[WOS_NUM_LAMP_BANKS] = 0x00;
  }

  for (byte count=0; count<WOS_MAX_LAMPS; count++) {
    LampFlashPeriod[count] = 0;
  }

  for (byte count=0; count<4; count++) {
    for (byte digit=0; digit<WOS_NUM_DIGITS; digit++) {
      DisplayDigits[count][digit] = 0;
    }
    DisplayDigitEnable[count] = 0x3F;
  }

  for (byte count=0; count<2; count++) {
    DisplayCreditDigits[count] = 0;
    DisplayBIPDigits[count] = 0;
  }
  DisplayCreditDigitEnable = 0x03;
  DisplayBIPDigitEnable = 0x03;


  for (byte count=0; count<SWITCH_COLUMNS; count++) {
    SwitchesMinus2[count] = 0xFF;
    SwitchesMinus1[count] = 0xFF;
    SwitchesNow[count] = 0xFF;
  }

  // Reset solenoid stack
  SolenoidStackFirst = 0;
  SolenoidStackLast = 0;

  // Reset switch stack
  SwitchStackFirst = 0;
  SwitchStackLast = 0;

  // Reset sound stack
  SoundStackFirst = 0;
  SoundStackLast = 0;

}

/*
 * Lamp handling functions
 */

void WOS_SetDimDivisor(byte level, byte divisor) {
  if (level==1) DimDivisor1 = divisor;
  if (level==2) DimDivisor2 = divisor;
}

void WOS_ApplyFlashToLamps(unsigned long curTime) {

  for (int count=0; count<WOS_MAX_LAMPS; count++) {
    if ( LampFlashPeriod[count]!=0 ) {
      unsigned long adjustedLampFlash = (unsigned long)LampFlashPeriod[count] * (unsigned long)50;
      if ((curTime/adjustedLampFlash)%2) {
        LampStates[count/8] &= ~(0x01<<(count%8));
      } else {
        LampStates[count/8] |= (0x01<<(count%8));
      }
    } // end if this light should flash
  } // end loop on lights
}


void WOS_FlashAllLamps(unsigned long curTime) {
  for (int count=0; count<WOS_MAX_LAMPS; count++) {
    WOS_SetLampState(count, 1, 0, 500);  
  }

  WOS_ApplyFlashToLamps(curTime);
}

void WOS_TurnOffAllLamps() {
  for (int count=0; count<WOS_MAX_LAMPS; count++) {
    WOS_SetLampState(count, 0, 0, 0);  
  }
}

void WOS_SetLampState(int lampNum, byte s_lampState, byte s_lampDim, int s_lampFlashPeriod) {
  if (lampNum>=WOS_MAX_LAMPS || lampNum<0) return;
  
  if (s_lampState) {
    int adjustedLampFlash = s_lampFlashPeriod/50;
    
    if (s_lampFlashPeriod!=0 && adjustedLampFlash==0) adjustedLampFlash = 1;
    if (adjustedLampFlash>250) adjustedLampFlash = 250;
    
    // Only turn on the lamp if there's no flash, because if there's a flash
    // then the lamp will be turned on by the ApplyFlashToLamps function
    if (s_lampFlashPeriod==0) LampStates[lampNum/8] &= ~(0x01<<(lampNum%8));
    LampFlashPeriod[lampNum] = adjustedLampFlash;
  } else {
    LampStates[lampNum/8] |= (0x01<<(lampNum%8));
    LampFlashPeriod[lampNum] = 0;
  }

  if (s_lampDim & 0x01) {    
    LampDim1[lampNum/8] |= (0x01<<(lampNum%8));
  } else {
    LampDim1[lampNum/8] &= ~(0x01<<(lampNum%8));
  }

  if (s_lampDim & 0x02) {    
    LampDim2[lampNum/8] |= (0x01<<(lampNum%8));
  } else {
    LampDim2[lampNum/8] &= ~(0x01<<(lampNum%8));
  }

}


/*
 * Sound handling functions
 */
unsigned short SoundLowerLimit = 0x0100;
unsigned short SoundUpperLimit = 0x1F00;

void WOS_SetSoundValueLimits(unsigned short lowerLimit, unsigned short upperLimit) {
  SoundLowerLimit = lowerLimit;
  SoundUpperLimit = upperLimit;
}
 
int SpaceLeftOnSoundStack() {
  if (SoundStackFirst>=SOUND_STACK_SIZE || SoundStackLast>=SOUND_STACK_SIZE) return 0;
  if (SoundStackLast>=SoundStackFirst) return ((SOUND_STACK_SIZE-1) - (SoundStackLast-SoundStackFirst));
  return (SoundStackFirst - SoundStackLast) - 1;
}


void WOS_PushToSoundStack(unsigned short soundNumber, byte numPushes) {  
  // If the solenoid stack last index is out of range, then it's an error - return
  if (SpaceLeftOnSoundStack()==0) return;
  if (soundNumber<SoundLowerLimit || soundNumber>SoundUpperLimit) return;

  for (int count=0; count<numPushes; count++) {
    SoundStack[SoundStackLast] = soundNumber;
    
    SoundStackLast += 1;
    if (SoundStackLast==SOUND_STACK_SIZE) {
      // If the end index is off the end, then wrap
      SoundStackLast = 0;
    }
    // If the stack is now full, return
    if (SpaceLeftOnSoundStack()==0) return;
  }

}


unsigned short PullFirstFromSoundStack() {
  // If first and last are equal, there's nothing on the stack
  if (SoundStackFirst==SoundStackLast) {
    return SOUND_STACK_EMPTY;
  }
  
  unsigned short retVal = SoundStack[SoundStackFirst];

  SoundStackFirst += 1;
  if (SoundStackFirst>=SOUND_STACK_SIZE) SoundStackFirst = 0;

  return retVal;
}


boolean WOS_PushToTimedSoundStack(unsigned short soundNumber, byte numPushes, unsigned long whenToPlay) {
  for (int count=0; count<TIMED_SOUND_STACK_SIZE; count++) {
    if (!TimedSoundStack[count].inUse) {
      TimedSoundStack[count].inUse = true;
      TimedSoundStack[count].pushTime = whenToPlay;
      TimedSoundStack[count].soundNumber = soundNumber;
      TimedSoundStack[count].numPushes = numPushes;
      return true;
    }
  }
  return false;
}


void WOS_UpdateTimedSoundStack(unsigned long curTime) {
  for (int count=0; count<TIMED_SOUND_STACK_SIZE; count++) {
    if (TimedSoundStack[count].inUse && TimedSoundStack[count].pushTime<curTime) {
      WOS_PushToSoundStack(TimedSoundStack[count].soundNumber, TimedSoundStack[count].numPushes);
      TimedSoundStack[count].inUse = false;
    }
  }
}



/*
 * Solenoid handling functions
 */
void WOS_DisableSolenoidStack() {
  SolenoidStackEnabled = false;
  WOS_DataWrite(PIA_SOLENOID_CONTROL_B, 0x34);
}


void WOS_EnableSolenoidStack() {
  SolenoidStackEnabled = true;
  WOS_DataWrite(PIA_SOLENOID_CONTROL_B, 0x3C);
}


void WOS_SetDisableFlippers(boolean disableFlippers) {
  if (disableFlippers) WOS_DataWrite(PIA_SOLENOID_CONTROL_B, 0x34);
  else WOS_DataWrite(PIA_SOLENOID_CONTROL_B, 0x3C);
}


void WOS_SetContinuousSolenoid(boolean solOn, byte solNum) {
  unsigned short oldCont = ContinuousSolenoidBits;
  if (solOn) ContinuousSolenoidBits |= (1<<solNum);
  else ContinuousSolenoidBits &= ~(1<<solNum);

  if (oldCont!=ContinuousSolenoidBits) {
    byte origPortA = WOS_DataRead(PIA_SOLENOID_PORT_A);
    byte origPortB = WOS_DataRead(PIA_SOLENOID_PORT_B);
    if (origPortA!=(ContinuousSolenoidBits&0xFF)) WOS_DataWrite(PIA_SOLENOID_PORT_A, (ContinuousSolenoidBits&0xFF));
    if (origPortB!=(ContinuousSolenoidBits/256)) WOS_DataWrite(PIA_SOLENOID_PORT_B, (ContinuousSolenoidBits/256));
  }
}

void WOS_SetCoinLockout(boolean lockoutOn, byte solNum) {
  WOS_SetContinuousSolenoid(lockoutOn, solNum);  
}


int SpaceLeftOnSolenoidStack() {
  if (SolenoidStackFirst>=SOLENOID_STACK_SIZE || SolenoidStackLast>=SOLENOID_STACK_SIZE) return 0;
  if (SolenoidStackLast>=SolenoidStackFirst) return ((SOLENOID_STACK_SIZE-1) - (SolenoidStackLast-SolenoidStackFirst));
  return (SolenoidStackFirst - SolenoidStackLast) - 1;
}


void WOS_PushToSolenoidStack(byte solenoidNumber, byte numPushes, boolean disableOverride) {
  if (solenoidNumber>21) return;

  // if the solenoid stack is disabled and this isn't an override push, then return
  if (!disableOverride && !SolenoidStackEnabled) return;

  // If the solenoid stack last index is out of range, then it's an error - return
  if (SpaceLeftOnSolenoidStack()==0) return;

  for (int count=0; count<numPushes; count++) {
    SolenoidStack[SolenoidStackLast] = solenoidNumber;
    
    SolenoidStackLast += 1;
    if (SolenoidStackLast==SOLENOID_STACK_SIZE) {
      // If the end index is off the end, then wrap
      SolenoidStackLast = 0;
    }
    // If the stack is now full, return
    if (SpaceLeftOnSolenoidStack()==0) return;
  }
}

void PushToFrontOfSolenoidStack(byte solenoidNumber, byte numPushes) {
  if (solenoidNumber>21) return;
  
  // If the stack is full, return
  if (SpaceLeftOnSolenoidStack()==0 || !SolenoidStackEnabled) return;

  for (int count=0; count<numPushes; count++) {
    if (SolenoidStackFirst==0) SolenoidStackFirst = SOLENOID_STACK_SIZE-1;
    else SolenoidStackFirst -= 1;
    SolenoidStack[SolenoidStackFirst] = solenoidNumber;
    if (SpaceLeftOnSolenoidStack()==0) return;
  }
  
}

byte PullFirstFromSolenoidStack() {
  // If first and last are equal, there's nothing on the stack
  if (SolenoidStackFirst==SolenoidStackLast) return SOLENOID_STACK_EMPTY;
  
  byte retVal = SolenoidStack[SolenoidStackFirst];

  SolenoidStackFirst += 1;
  if (SolenoidStackFirst>=SOLENOID_STACK_SIZE) SolenoidStackFirst = 0;

  return retVal;
}


boolean WOS_PushToTimedSolenoidStack(byte solenoidNumber, byte numPushes, unsigned long whenToFire, boolean disableOverride) {
  for (int count=0; count<TIMED_SOLENOID_STACK_SIZE; count++) {
    if (!TimedSolenoidStack[count].inUse) {
      TimedSolenoidStack[count].inUse = true;
      TimedSolenoidStack[count].pushTime = whenToFire;
      TimedSolenoidStack[count].disableOverride = disableOverride;
      TimedSolenoidStack[count].solenoidNumber = solenoidNumber;
      TimedSolenoidStack[count].numPushes = numPushes;
      return true;
    }
  }
  return false;
}


void WOS_UpdateTimedSolenoidStack(unsigned long curTime) {
  for (int count=0; count<TIMED_SOLENOID_STACK_SIZE; count++) {
    if (TimedSolenoidStack[count].inUse && TimedSolenoidStack[count].pushTime<curTime) {
      WOS_PushToSolenoidStack(TimedSolenoidStack[count].solenoidNumber, TimedSolenoidStack[count].numPushes, TimedSolenoidStack[count].disableOverride);
      TimedSolenoidStack[count].inUse = false;
    }
  }
}







/*
 * Display handling functions
 * 
 */

byte WOS_SetDisplay(int displayNumber, unsigned long value, boolean blankByMagnitude, byte minDigits) {
  if (displayNumber<0 || displayNumber>3) return 0;

  byte blank = 0x00;

  for (int count=0; count<WOS_NUM_DIGITS; count++) {
    blank = blank * 2;
    if (value!=0 || count<minDigits) blank |= 1;
    DisplayDigits[displayNumber][(WOS_NUM_DIGITS-1)-count] = value%10;
    value /= 10;    
  }
  
  if (blankByMagnitude) DisplayDigitEnable[displayNumber] = blank;
  
  return blank;
}

void WOS_SetDisplayBlank(int displayNumber, byte bitMask) {
  if (displayNumber<0 || displayNumber>3) return;
  DisplayDigitEnable[displayNumber] = bitMask;
}

void WOS_SetDisplayCredits(int value, boolean displayOn, boolean showBothDigits) {
  byte blank = 0x02;
  value = value % 100;
  if (value>=10) {
    DisplayCreditDigits[0] = value/10;
    blank |= 1;
  } else {
    DisplayCreditDigits[0] = 0;
    if (showBothDigits) blank |= 1;
  }
  DisplayCreditDigits[1] = value%10;
  if (displayOn) DisplayCreditDigitEnable = blank;
  else DisplayCreditDigitEnable = 0;
}

void WOS_SetDisplayBallInPlay(int value, boolean displayOn, boolean showBothDigits) {
  byte blank = 0x02;
  value = value % 100;
  if (value>=10) {
    DisplayBIPDigits[0] = value/10;
    blank |= 1;
  } else {
    DisplayBIPDigits[0] = 0;
    if (showBothDigits) blank |= 1;
  }
  DisplayBIPDigits[1] = value%10;
  if (displayOn) DisplayBIPDigitEnable = blank;
  else DisplayBIPDigitEnable = 0;  
}

void WOS_SetDisplayMatch(int value, boolean displayOn, boolean showBothDigits) {
  WOS_SetDisplayBallInPlay(value, displayOn, showBothDigits);  
}

void WOS_CycleAllDisplays(unsigned long curTime, byte digitNum) {
  int displayDigit = (curTime/250)%10;
  unsigned long value;
  value = displayDigit*111111;

  byte displayNumToShow = 0;
  byte displayBlank = 0x3F;

  if (digitNum!=0) {
    displayNumToShow = (digitNum-1)/6;
    displayBlank = (0x20)>>((digitNum-1)%6);
  }

  for (int count=0; count<4; count++) {
    if (digitNum) {
      WOS_SetDisplay(count, value);
      if (count==displayNumToShow) WOS_SetDisplayBlank(count, displayBlank);
      else WOS_SetDisplayBlank(count, 0);
    } else {
      WOS_SetDisplay(count, value, true);
    }
  }
  if (digitNum) {
    if (displayNumToShow==4) {
      WOS_SetDisplayCredits(11*displayDigit);
      WOS_SetDisplayBallInPlay(11*displayDigit);
    } else {
      WOS_SetDisplayCredits(0);
      WOS_SetDisplayBallInPlay(0);
    }
  } else {
    WOS_SetDisplayCredits(11*displayDigit);
    WOS_SetDisplayBallInPlay(11*displayDigit);
  }
}

byte WOS_GetDisplayBlank(int displayNumber) {
  if (displayNumber<0 || displayNumber>3) return 0;
  return DisplayDigitEnable[displayNumber];
}

/*
 * Switch handling functions
 */

int SpaceLeftOnSwitchStack() {
  if (SwitchStackFirst>=SWITCH_STACK_SIZE || SwitchStackLast>=SWITCH_STACK_SIZE) return 0;
  if (SwitchStackLast>=SwitchStackFirst) return ((SWITCH_STACK_SIZE-1) - (SwitchStackLast-SwitchStackFirst));
  return (SwitchStackFirst - SwitchStackLast) - 1;
}

void PushToSwitchStack(byte switchNumber) {
  if ((switchNumber>=(SWITCH_COLUMNS*8) && switchNumber!=SW_SELF_TEST_SWITCH)) return;

  // If the switch stack last index is out of range, then it's an error - return
  if (SpaceLeftOnSwitchStack()==0) return;

  // Self test is a special case - there's no good way to debounce it
  // so if it's already first on the stack, ignore it
  if (switchNumber==SW_SELF_TEST_SWITCH) {
    if (SwitchStackLast!=SwitchStackFirst && SwitchStack[SwitchStackFirst]==SW_SELF_TEST_SWITCH) return;
  }

  SwitchStack[SwitchStackLast] = switchNumber;
  
  SwitchStackLast += 1;
  if (SwitchStackLast==SWITCH_STACK_SIZE) {
    // If the end index is off the end, then wrap
    SwitchStackLast = 0;
  }
}


byte WOS_PullFirstFromSwitchStack() {
  // If first and last are equal, there's nothing on the stack
  if (SwitchStackFirst==SwitchStackLast) {
    return SWITCH_STACK_EMPTY;
  }

  byte retVal = SwitchStack[SwitchStackFirst];

  SwitchStackFirst += 1;
  if (SwitchStackFirst>=SWITCH_STACK_SIZE) SwitchStackFirst = 0;

  return retVal;
}


boolean CheckSwitchStack(byte switchNum) {
  for (byte stackIndex=SwitchStackFirst; stackIndex!=SwitchStackLast; stackIndex++) {
    if (stackIndex>=SWITCH_STACK_SIZE) stackIndex = 0; 
    if (SwitchStack[stackIndex]==switchNum) return true;
  }
  return false;
}


boolean WOS_ReadSingleSwitchState(byte switchNum) {
  if (switchNum>=(SWITCH_COLUMNS*8)) return false;

  int switchCol = switchNum/8;
  int switchBit = switchNum%8;
  if ( ((SwitchesNow[switchCol])>>switchBit) & 0x01 ) return true;
  else return false;
}


void WOS_ClearUpDownSwitchState() {
  UpDownSwitch = false;
}

boolean WOS_GetUpDownSwitchState() {
  return UpDownSwitch;
}



volatile unsigned long LampPass = 0;
volatile byte LampStrobe = 0;
volatile byte DisplayStrobe = 0;
volatile byte InterruptPass = 0;
boolean NeedToTurnOffTriggeredSolenoids = true;
byte BlankingBit[16] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x01, 0x02, 0x01, 0x02, 0x04, 0x08, 0x010, 0x20, 0x01, 0x02};
volatile byte UpDownPassCounter = 0;

// INTERRUPT HANDLER
ISR(TIMER1_COMPA_vect) {    //This is the interrupt request (running at 965.3 Hz)

  byte displayControlPortB = WOS_DataRead(PIA_DISPLAY_CONTROL_B);
  if (displayControlPortB & 0x80) {
    UpDownSwitch = true;
    UpDownPassCounter = 0;
    // Clear the interrupt
    WOS_DataRead(PIA_DISPLAY_PORT_B);
  } else {
    UpDownPassCounter += 1;
    if (UpDownPassCounter==50) {
      UpDownSwitch = false;
      UpDownPassCounter = 0;
    }
  }

#ifdef WILLIAMS_11_MPU
#else
  // Create display data
  byte digit1 = 0x0F, digit2 = 0x0F;
  byte blankingBit = BlankingBit[DisplayStrobe];
  if (DisplayStrobe<6) {
    if (DisplayDigitEnable[0]&blankingBit) digit1 = DisplayDigits[0][DisplayStrobe];
    if (DisplayDigitEnable[2]&blankingBit) digit2 = DisplayDigits[2][DisplayStrobe];
  } else if (DisplayStrobe<8) {
    if (DisplayBIPDigitEnable&blankingBit) digit1 = DisplayBIPDigits[DisplayStrobe-6];
  } else if (DisplayStrobe<14) {
    if (DisplayDigitEnable[1]&blankingBit) digit1 = DisplayDigits[1][DisplayStrobe-8];
    if (DisplayDigitEnable[3]&blankingBit) digit2 = DisplayDigits[3][DisplayStrobe-8];
  } else {
    if (DisplayCreditDigitEnable&blankingBit) digit1 = DisplayCreditDigits[DisplayStrobe-14];
  }  
  // Show current display digit
//  if (WOS_DataRead(PIA_DISPLAY_CONTROL_B) & 0x80) SawInterruptOnDisplayPortB1 = true;
  WOS_DataWrite(PIA_DISPLAY_PORT_A, BoardLEDs|DisplayStrobe);
  WOS_DataWrite(PIA_DISPLAY_PORT_B, digit1*16 | (digit2&0x0F));
#endif

  DisplayStrobe += 1; 
  if (DisplayStrobe>=16) DisplayStrobe = 0;

  if (InterruptPass==0) {
  
    // Show lamps
    byte curLampByte = LampStates[LampStrobe];
    if (LampPass%DimDivisor1) curLampByte |= LampDim1[LampStrobe];
    if (LampPass%DimDivisor2) curLampByte |= LampDim2[LampStrobe];
    WOS_DataWrite(PIA_LAMPS_PORT_B, 0x01<<(LampStrobe));
    WOS_DataWrite(PIA_LAMPS_PORT_A, curLampByte);
    
    LampStrobe += 1;
    if ((LampStrobe)>=WOS_NUM_LAMP_BANKS) {
      LampStrobe = 0;
      LampPass += 1;
    }
    
    // Check coin door switches
    byte displayControlPortA = WOS_DataRead(PIA_DISPLAY_CONTROL_A);
    if (displayControlPortA & 0x80) {
      // If the diagnostic switch isn't on the stack already, put it there
      if (!CheckSwitchStack(SW_SELF_TEST_SWITCH)) PushToSwitchStack(SW_SELF_TEST_SWITCH);
      // Clear the interrupt
      WOS_DataRead(PIA_DISPLAY_PORT_A);
    }

    // Check switches
    byte switchColStrobe = 1;
    for (byte switchCol=0; switchCol<8; switchCol++) {
      // Cycle the debouncing variables
      SwitchesMinus2[switchCol] = SwitchesMinus1[switchCol];
      SwitchesMinus1[switchCol] = SwitchesNow[switchCol];
      // Turn on the strobe
      WOS_DataWrite(PIA_SWITCH_PORT_B, switchColStrobe);
      // Hold it up for 30 us
      delayMicroseconds(12);
      // Read switch input
      SwitchesNow[switchCol] = WOS_DataRead(PIA_SWITCH_PORT_A);
      switchColStrobe *= 2;
    }
    WOS_DataWrite(PIA_SWITCH_PORT_B, 0);
    
    // If there are any closures, add them to the switch stack
    for (byte switchCol=0; switchCol<SWITCH_COLUMNS; switchCol++) {
      byte validClosures = (SwitchesNow[switchCol] & SwitchesMinus1[switchCol]) & ~SwitchesMinus2[switchCol];
      // If there is a valid switch closure (off, on, on)
      if (validClosures) {
        // Loop on bits of switch byte
        for (byte bitCount=0; bitCount<8; bitCount++) {
          // If this switch bit is closed
          if (validClosures&0x01) {
            byte validSwitchNum = switchCol*8 + bitCount;
            PushToSwitchStack(validSwitchNum);
          }
          validClosures = validClosures>>1;
        }        
      }
    }
  
  } else {
    // See if any solenoids need to be switched
    byte solenoidOn = PullFirstFromSolenoidStack();
    byte portA = ContinuousSolenoidBits&0xFF;
    byte portB = ContinuousSolenoidBits/256;
    if (solenoidOn!=SOLENOID_STACK_EMPTY) {
      if (solenoidOn<16) {
        unsigned short newSolenoidBytes = (1<<solenoidOn);
        portA |= (newSolenoidBytes&0xFF);
        portB |= (newSolenoidBytes/256);
        if (NeedToTurnOffTriggeredSolenoids) {
          WOS_DataWrite(PIA_LAMPS_CONTROL_B, 0x3C);
          WOS_DataWrite(PIA_LAMPS_CONTROL_A, 0x3C);
          WOS_DataWrite(PIA_SWITCH_CONTROL_B, 0x3C);
          WOS_DataWrite(PIA_SWITCH_CONTROL_A, 0x3C);
          WOS_DataWrite(PIA_SOLENOID_CONTROL_A, 0x3C);
          WOS_DataWrite(PIA_DISPLAY_CONTROL_B, 0x3D);
          NeedToTurnOffTriggeredSolenoids = false;
        }
      } else {
        if (solenoidOn==16) WOS_DataWrite(PIA_LAMPS_CONTROL_B, 0x34);
        if (solenoidOn==17) WOS_DataWrite(PIA_LAMPS_CONTROL_A, 0x34);
        if (solenoidOn==18) WOS_DataWrite(PIA_SWITCH_CONTROL_B, 0x34);
        if (solenoidOn==19) WOS_DataWrite(PIA_SWITCH_CONTROL_A, 0x34);
        if (solenoidOn==20) WOS_DataWrite(PIA_SOLENOID_CONTROL_A, 0x34);
        if (solenoidOn==21) WOS_DataWrite(PIA_DISPLAY_CONTROL_B, 0x35);
        NeedToTurnOffTriggeredSolenoids = true;
      }
    } else if (NeedToTurnOffTriggeredSolenoids) {
      NeedToTurnOffTriggeredSolenoids = false;
      WOS_DataWrite(PIA_LAMPS_CONTROL_B, 0x3C);
      WOS_DataWrite(PIA_LAMPS_CONTROL_A, 0x3C);
      WOS_DataWrite(PIA_SWITCH_CONTROL_B, 0x3C);
      WOS_DataWrite(PIA_SWITCH_CONTROL_A, 0x3C);
      WOS_DataWrite(PIA_SOLENOID_CONTROL_A, 0x3C);
      WOS_DataWrite(PIA_DISPLAY_CONTROL_B, 0x3D);
    }

    // See if any sounds need to be added
    unsigned short soundOn = PullFirstFromSoundStack();
    if (soundOn!=SOUND_STACK_EMPTY) {
      portA |= (soundOn&0xFF);
      portB |= (soundOn/256);
    }

    WOS_DataWrite(PIA_SOLENOID_PORT_A, portA);
#ifdef WILLIAMS_11_MPU
    WOS_DataWrite(PIA_SOLENOID_11_PORT_B, portB);
#else 
    WOS_DataWrite(PIA_SOLENOID_PORT_B, portB);
#endif    
  }

//  WOS_DataWrite(PIA_SOLENOID_11_PORT_B, InterruptPass);
  InterruptPass ^= 1;
}







// EEProm Helper functions

void WOS_WriteByteToEEProm(unsigned short startByte, byte value) {
  EEPROM.write(startByte, value);
}

byte WOS_ReadByteFromEEProm(unsigned short startByte) {
  byte value = EEPROM.read(startByte);

  // If this value is unset, set it
  if (value==0xFF) {
    value = 0;
    WOS_WriteByteToEEProm(startByte, value);
  }
  return value;
}



unsigned long WOS_ReadULFromEEProm(unsigned short startByte, unsigned long defaultValue) {
  unsigned long value;

  value = (((unsigned long)EEPROM.read(startByte+3))<<24) | 
          ((unsigned long)(EEPROM.read(startByte+2))<<16) | 
          ((unsigned long)(EEPROM.read(startByte+1))<<8) | 
          ((unsigned long)(EEPROM.read(startByte)));

  if (value==0xFFFFFFFF) {
    value = defaultValue; 
    WOS_WriteULToEEProm(startByte, value);
  }
  return value;
}


void WOS_WriteULToEEProm(unsigned short startByte, unsigned long value) {
  EEPROM.write(startByte+3, (byte)(value>>24));
  EEPROM.write(startByte+2, (byte)((value>>16) & 0x000000FF));
  EEPROM.write(startByte+1, (byte)((value>>8) & 0x000000FF));
  EEPROM.write(startByte, (byte)(value & 0x000000FF));
}


boolean WOS_DiagnosticModeRequested() {
  boolean bootToDiagnostics = false;
#if (RPU_VERSION==2)
  pinMode(RPU_DIAGNOSTIC_PIN, INPUT);
  if (digitalRead(RPU_DIAGNOSTIC_PIN)==1) bootToDiagnostics = true;
#endif  

  return bootToDiagnostics;
}


void WOS_InitializeMPU(byte creditResetButton) {
  // put the 6800 buffers into tri-state
  pinMode(RPU_BUFFER_DISABLE, OUTPUT);
  digitalWrite(RPU_BUFFER_DISABLE, 1);

  // Set /HALT low so the processor doesn't come online
  // (on some hardware, HALT & RESET are combined)
  pinMode(RPU_HALT_PIN, OUTPUT); 
  digitalWrite(RPU_HALT_PIN, 0);  
  pinMode(RPU_RESET_PIN, OUTPUT); 
  digitalWrite(RPU_RESET_PIN, 0);  

  // Set VMA, R/W, and PHI2 to OUTPUT
  pinMode(RPU_VMA_PIN, OUTPUT);
  pinMode(RPU_RW_PIN, OUTPUT);
  pinMode(RPU_PHI2_PIN, OUTPUT);
  // Make sure PIA IV (solenoid) CB2 is off so that solenoids are off
  WOS_SetAddressPinsDirection(WOS_PINS_OUTPUT);  
  WOS_DataWrite(PIA_SOLENOID_CONTROL_B, 0x30);
  
  delay(1000);
  boolean bootToOldCode = false;

  //Serial.write("Checking for credit button\n");
  // see if the credit button is being pressed
  if (0 && creditResetButton!=0xFF) {
    byte strobeLine = 0x01 << (creditResetButton/8);
    byte returnLine = 0x01 << (creditResetButton%8);

    WOS_DataWrite(PIA_SWITCH_CONTROL_A, 0x38);
    WOS_DataWrite(PIA_SWITCH_PORT_A, 0x00);
    WOS_DataWrite(PIA_SWITCH_CONTROL_A, 0x3C);
  
    WOS_DataWrite(PIA_SWITCH_CONTROL_B, 0x38);
    WOS_DataWrite(PIA_SWITCH_PORT_B, 0xFF);
    WOS_DataWrite(PIA_SWITCH_CONTROL_B, 0x3C);
    WOS_DataWrite(PIA_SWITCH_PORT_B, 0x00);
  
    WOS_DataWrite(PIA_SWITCH_PORT_B, strobeLine);
    // Hold it up for 30 us
    delayMicroseconds(12);

    // Read switch input
    byte switchValues = WOS_DataRead(PIA_SWITCH_PORT_A);
    WOS_DataWrite(PIA_SWITCH_PORT_B, 0);

    if (switchValues & returnLine) bootToOldCode = true;
  }

  boolean bootToDiagnostics = false;
#if (RPU_VERSION==2)
  pinMode(RPU_DIAGNOSTIC_PIN, INPUT);
  if (digitalRead(RPU_DIAGNOSTIC_PIN)==1) bootToDiagnostics = true;
#endif  
  
  pinMode(RPU_SWITCH_PIN, INPUT);
  // See if the switch is off
  if ((digitalRead(RPU_SWITCH_PIN)==0 || bootToOldCode) && !bootToDiagnostics) {

    //if (bootToOldCode) Serial.write("Allowing 680X to boot because of credit button\n");
    //else if (digitalRead(RPU_SWITCH_PIN)==0) Serial.write("Allowing 680X to boot because of hardware switch\n");
    // If the switch is off, allow 6808 to boot
    pinMode(RPU_BUFFER_DISABLE, OUTPUT); // IRQ
    // Turn on the tri-state buffers
    digitalWrite(RPU_BUFFER_DISABLE, 0);
    
    pinMode(RPU_PHI2_PIN, INPUT); // CLOCK
    pinMode(RPU_VMA_PIN, INPUT); // VMA
    pinMode(RPU_RW_PIN, INPUT); // R/W

    // Set all the pins to input so they'll stay out of the way
    WOS_SetDataPinsDirection(WOS_PINS_INPUT);
    WOS_SetAddressPinsDirection(WOS_PINS_INPUT);

    // Set /HALT high
    pinMode(RPU_HALT_PIN, OUTPUT);
    digitalWrite(RPU_HALT_PIN, 1);
    pinMode(RPU_RESET_PIN, OUTPUT);
    digitalWrite(RPU_RESET_PIN, 1);

    while (1);    
  }
  

  WOS_ClearVariables();
  WOS_SetAddressPinsDirection(WOS_PINS_OUTPUT);
  WOS_InitializePIAs();
  /*if (!bootToDiagnostics)*/ WOS_SetupInterrupt();
  
}


byte WOS_GetSwitchesNow(byte switchCol) {
  return SwitchesNow[switchCol];
}
