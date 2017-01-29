
/*
 * Inductance Meter Shield for Arduino Uno
 * Lukas Fässler, 2014-12-14
 * See www.soldernerd.com for description, schematics, Eagle files, photos and more
 */

#include <LiquidCrystal.h>
#define LCD_RS 12
#define LCD_RW 11
#define LCD_EN 10
#define LCD_D7 2
#define LCD_D6 3
#define LCD_D5 4
#define LCD_D4 5
LiquidCrystal lcd(LCD_RS, LCD_RW, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

#define BUTTON 9
#define COUNTER 8
#define CAPTURE_INT 7
#define OVERFLOW_INT 6
#define CPU_FREQUENCY 15992430.43 /* must be a float. use 16000000.0 as default */
#define CAPACITY 0.0000000005 /* in Farad */
#define ZERO_INDUCTANCE 1.86 /* in microHenry (uH) */

#define OVERFLOW_COUNT_MINIMUM 64
#define OVERFLOW_COUNT_MAXIMUM 128

volatile uint8_t overflow_count;
volatile uint16_t capture_count;
volatile uint16_t capture_count_sav;
volatile int32_t total_count;
volatile int32_t total_count_sav;
volatile uint8_t done;

float measured_frequency;
float measured_inductance;
float zero_calibration;
float adjusted_inductance;

void setup()
{
  pinMode(BUTTON, INPUT);
  pinMode(COUNTER, INPUT);
  pinMode(CAPTURE_INT, OUTPUT);
  pinMode(OVERFLOW_INT, OUTPUT);
  
  overflow_count = 0;
  capture_count = 0;
  capture_count_sav = 0;
  total_count = 0;
  total_count_sav = 0;
  done = 0;
  zero_calibration = ZERO_INDUCTANCE;
  
  /*
   * TCCR1A – Timer/Counter1 Control Register A, page 132
   * Bit 7:6 – COM1A1:0: Compare Output Mode for Channel A (Physical Pin 15, Arduino Pin 9)
   * Bit 5:4 – COM1B1:0: Compare Output Mode for Channel B (Physical Pin 16, Arduino Pin 10)
   * Bit 3:2 - Reserved
   * Bit 1:0 – WGM11:0: Waveform Generation Mode
   */
   
  TCCR1A = 0b00000000; //No output
  
  /*
   * TCCR1B – Timer/Counter1 Control Register B, page 134
   * Bit 7 – ICNC1: Input Capture Noise Canceler
   * Bit 6 – ICES1: Input Capture Edge Select
   * Bit 5 – Reserved
   * Bit 4:3 – WGM13:2: Waveform Generation Mode
   * Bit 2:0 – CS12:0: Clock Select
   */
  
  TCCR1B = 0b00000001; //No Noise Canceler, Falling Edge, Prescaler=1
  
  /*
   * TIMSK1 – Timer/Counter1 Interrupt Mask Register, page 136
   * Bit 7, 6 – Reserved
   * Bit 5 – ICIE1: Timer/Counter1, Input Capture Interrupt Enable
   * Bit 4, 3 – Reserved
   * Bit 2 – OCIE1B: Timer/Counter1, Output Compare B Match Interrupt Enable
   * Bit 1 – OCIE1A: Timer/Counter1, Output Compare A Match Interrupt Enable
   * Bit 0 – TOIE1: Timer/Counter1, Overflow Interrupt Enable
   */
   
  TIMSK1 = 0b00100001; //Input Capture and Overflow Interrupts
  
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Inductance Meter");
  lcd.setCursor(0, 1);
  lcd.print("Lukas Faessler  ");
  delay(1000);
  
  //Turn off timer0
  TCCR0B = 0x00;
  TIMSK0 = 0x00;
}

ISR(TIMER1_OVF_vect)
{
  digitalWrite(OVERFLOW_INT, HIGH);
  overflow_count++;
  total_count += 65536;
  if(overflow_count>OVERFLOW_COUNT_MAXIMUM)
  {
    done = 2;
  }
  digitalWrite(OVERFLOW_INT, LOW);
}

ISR(TIMER1_CAPT_vect)
{
  digitalWrite(CAPTURE_INT, HIGH);
  ++capture_count;
  if(overflow_count>=OVERFLOW_COUNT_MINIMUM)
  {
    uint16_t timer_value;
    uint8_t timer_low;
    timer_low = ICR1L;
    timer_value = ICR1H;
    timer_value <<= 8;
    timer_value |= timer_low;
    total_count_sav = total_count + timer_value;
    total_count = 0;
    total_count -= timer_value;
    capture_count_sav = capture_count;
    capture_count = 0;
    overflow_count = 0;
    done = 1;
  }
  digitalWrite(CAPTURE_INT, LOW);
}

void loop()
{
  if(done==1)
  {
    measured_frequency = (256.0 * CPU_FREQUENCY * capture_count_sav) / total_count_sav;
    lcd.setCursor(0, 0);
    lcd.print("Freq:        kHz");
    lcd.setCursor(6, 0);
    lcd.print(measured_frequency/1000);
    measured_inductance = 2 * PI * measured_frequency * sqrt(CAPACITY);
    measured_inductance = 1000000 / (measured_inductance * measured_inductance);
    if(digitalRead(BUTTON)==0) //Button pressed
    {
      zero_calibration = measured_inductance;
    }
    adjusted_inductance = measured_inductance - zero_calibration;
    if(abs(adjusted_inductance)<1)
    {
      lcd.setCursor(0, 1);
      lcd.print("Induc:        nH");
      lcd.setCursor(7, 1);
      lcd.print(1000*adjusted_inductance);      
    }
    else
    {
      lcd.setCursor(0, 1);
      lcd.print("Induc:        uH");
      lcd.setCursor(7, 1);
      lcd.print(adjusted_inductance);
    }
    done = 0;
  }
  else if(done==2)
  {
 
    lcd.setCursor(0, 0);
    lcd.print("Not resonating  ");
    if(abs(zero_calibration)<1)
    {
      lcd.setCursor(0, 1);
      lcd.print("Calib:        nH");
      lcd.setCursor(7, 1);
      lcd.print(1000*zero_calibration);      
    }
    else
    {
      lcd.setCursor(0, 1);
      lcd.print("Calib:        uH");
      lcd.setCursor(7, 1);
      lcd.print(zero_calibration);
    }
    done = 0;
  }
}


