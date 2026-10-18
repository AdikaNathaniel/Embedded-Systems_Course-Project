#include <LiquidCrystal.h>
#include <avr/wdt.h>         // For Watchdog
#include <avr/interrupt.h>   // For Timer1 ISR
#include <avr/sleep.h>       // For Low-Power Sleep
#include <EEPROM.h>          // For Data Logging

/*****************************************************/
/*                 LCD Setup                         */
/*****************************************************/
LiquidCrystal lcd(13, 12, 11, 10, 9, 8);

/*****************************************************/
/*         Biquad Filter Coefficients (SOS)          */
/*         Single-section Direct-Form II             */
/*****************************************************/
static const float b[3] = { 1.0, 2.0, 1.0 };
static const float a[3] = { 1.0, -1.7432602813357174, 0.7726599739785316 };
static const float G = 0.007349923160703567;  // Overall gain

/*****************************************************/
/*             Global States and Variables           */
/*****************************************************/
volatile float lastFilteredValue = 0.0f; // Latest filtered sample
volatile bool sampleReady = false;       // New sample flag

// Direct Form II internal state variables
static float w1 = 0.0f; // w[n-1]
static float w2 = 0.0f; // w[n-2]

// Hysteresis thresholds to avoid erratic switching
float thresholdHigh = 0.50;
float thresholdLow  = 0.45;

// Detection state variables
bool detection = false;           // Current detection state
bool previousDetection = false;   // For logging transitions

// EEPROM logging
int eventCount = 0;      // Count of detection events
int eepromAddress = 0;   // EEPROM address for eventCount

// Pin definitions
const int sensorPin   = A0; // LDR sensor (analog)
const int buzzerPin   = 6;  // Buzzer (tone output)
const int redLedPin   = 3;  // Red LED (PWM-capable, used here with digitalWrite)
const int greenLedPin = 5;  // Green LED (PWM-capable)

// Creative alarm mode variables
bool alarmActive = false;
unsigned long alarmStartTime = 0;
const unsigned long alarmDuration = 5000; // Alarm mode lasts 5 seconds

/*****************************************************/
/*        Single-Sample Direct Form II Filter        */
/*****************************************************/
float directForm2SingleSample(float x)
{
  float input = G * x;
  float a1 = a[1];
  float a2 = a[2];
  
  // Compute filter output
  float w0 = input - a1 * w1 - a2 * w2;
  float y0 = b[0] * w0 + b[1] * w1 + b[2] * w2;
  
  // Shift state variables
  w2 = w1;
  w1 = w0;
  
  return y0;
}

/*****************************************************/
/*                  Timer1 ISR                       */
/*****************************************************/
ISR(TIMER1_COMPA_vect)
{
  // Read sensor (0-1023) and normalize to 0.0-1.0
  float x_in = analogRead(sensorPin) / 1023.0;
  
  // Filter new sample
  float y_out = directForm2SingleSample(x_in);
  lastFilteredValue = y_out;
  
  // Signal that a new sample is ready
  sampleReady = true;
}

/*****************************************************/
/*                Setup Timer1                       */
/*****************************************************/
void setupTimer1()
{
  cli(); // Disable global interrupts
  
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0; // Reset counter
  
  // For a 16MHz clock with prescaler 1024: 16,000,000/1024 ≈ 15625 Hz.
  // For ~100Hz interrupts: OCR1A = 156.
  OCR1A = 156;
  
  TCCR1B |= (1 << WGM12);            // CTC mode
  TCCR1B |= (1 << CS12) | (1 << CS10); // Prescaler 1024
  TIMSK1 |= (1 << OCIE1A);            // Enable Timer1 compare interrupt
  
  sei(); // Re-enable interrupts
}

/*****************************************************/
/*             Setup Watchdog Timer                  */
/*****************************************************/
void setupWatchdog()
{
  wdt_enable(WDTO_2S); // 2-second watchdog timeout
}

/*****************************************************/
/*                   Low Power                       */
/*****************************************************/
void goToSleep()
{
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  sleep_cpu();
  sleep_disable();  // Resume after wake-up
}

/*****************************************************/
/*                EEPROM Logging                     */
/*****************************************************/
void loadEventCountFromEEPROM()
{
  EEPROM.get(eepromAddress, eventCount);
}

void saveEventCountToEEPROM()
{
  EEPROM.put(eepromAddress, eventCount);
}

/*****************************************************/
/*                       Setup                       */
/*****************************************************/
void setup()
{
  Serial.begin(9600);
  
  // Configure pin modes
  pinMode(sensorPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  
  // Initialize LCD
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Light Intensity");
  
  // Setup Timer1 and Watchdog Timer
  setupTimer1();
  setupWatchdog();
  
  // Load previous event count from EEPROM
  loadEventCountFromEEPROM();
  Serial.println("System Initialized");
  Serial.print("Loaded eventCount from EEPROM = ");
  Serial.println(eventCount);
}

/*****************************************************/
/*                       Loop                        */
/*****************************************************/
void loop()
{
  wdt_reset(); // Reset watchdog timer
  
  // Process new sensor sample and update detection state.
  if (sampleReady)
  {
    sampleReady = false;
    
    // Hysteresis: update detection state based on filtered value.
    if (!detection && lastFilteredValue > thresholdHigh)
      detection = true;
    else if (detection && lastFilteredValue < thresholdLow)
      detection = false;
    
    // Trigger alarm mode on rising edge.
    if (detection && !alarmActive)
    {
      alarmActive = true;
      alarmStartTime = millis();
      eventCount++;
      saveEventCountToEEPROM();
      Serial.print("Detection event #");
      Serial.println(eventCount);
    }
  }
  
  // If alarm mode is active, run creative outputs.
  if (alarmActive)
  {
    unsigned long t = millis() - alarmStartTime;
    
    // Buzzer modulation (1-second period)
    const unsigned long modPeriodBuzzer = 1000;
    float angleBuzzer = 2.0 * PI * (float)(t % modPeriodBuzzer) / (float)modPeriodBuzzer;
    unsigned int freq = 400 + (unsigned int)(200 * sin(angleBuzzer));
    tone(buzzerPin, freq);
    
    // Red LED blinking: blink with a period of 1000 ms (500 ms on, 500 ms off)
    const unsigned long blinkPeriod = 1000;
    if ((t % blinkPeriod) < (blinkPeriod / 2))
      digitalWrite(redLedPin, HIGH);
    else
      digitalWrite(redLedPin, LOW);
    
    // Ensure green LED is off during alarm.
    digitalWrite(greenLedPin, LOW);
    
    // Update LCD with alarm status.
    lcd.setCursor(0, 1);
    lcd.print("ALARM: DETECTED    ");
    
    // Print alarm info to Serial.
    Serial.print("ALARM Mode: Filtered Value = ");
    Serial.print(lastFilteredValue, 3);
    Serial.print(" (Freq: ");
    Serial.print(freq);
    Serial.println(")");
    
    // End alarm mode after the specified duration.
    if (t >= alarmDuration)
    {
      alarmActive = false;
      noTone(buzzerPin);
      digitalWrite(redLedPin, LOW);
    }
  }
  else // Normal mode.
  {
    noTone(buzzerPin);
    digitalWrite(redLedPin, LOW);
    digitalWrite(greenLedPin, HIGH);
    
    lcd.setCursor(0, 1);
    lcd.print("Status: Normal     ");
    
    Serial.print("Filtered Value = ");
    Serial.print(lastFilteredValue, 3);
    Serial.print(" => Detection = ");
    Serial.println(detection);
  }
  
  goToSleep();
}
