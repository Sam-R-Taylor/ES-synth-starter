#include <Arduino.h>
#include <U8g2lib.h>
#include <STM32FreeRTOS.h>

//Constants
  const uint32_t interval = 100; //Display update interval

//Pin definitions
  //Row select and enable
  const int RA0_PIN = D3;
  const int RA1_PIN = D6;
  const int RA2_PIN = D12;
  const int REN_PIN = A5;

  //Matrix input and output
  const int C0_PIN = A2;
  const int C1_PIN = D9;
  const int C2_PIN = A6;
  const int C3_PIN = D1;
  const int OUT_PIN = D11;

  //Audio analogue out
  const int OUTL_PIN = A4;
  const int OUTR_PIN = A3;

  //Joystick analogue in
  const int JOYY_PIN = A0;
  const int JOYX_PIN = A1;

  //Output multiplexer bits
  const int DEN_BIT = 3;
  const int DRST_BIT = 4;
  const int HKOW_BIT = 5;
  const int HKOE_BIT = 6;

//Display driver object
U8G2_SSD1305_128X32_NONAME_F_HW_I2C u8g2(U8G2_R0);

//Key Constant
uint8_t keyArray[7];

//Generating Sound  vars/consts
const static int32_t STEPSIZES [] = {51076056, 54113197, 57330935, 60740010, 64351798, 68178356, 72232452, 76527617, 81078186, 85899345, 91007186, 96418755};
const static char* NOTES[13] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", " "};
volatile int32_t currentStepSize;

//Function to set outputs using key matrix
void setOutMuxBit(const uint8_t bitIdx, const bool value) {
      digitalWrite(REN_PIN,LOW);
      digitalWrite(RA0_PIN, bitIdx & 0x01);
      digitalWrite(RA1_PIN, bitIdx & 0x02);
      digitalWrite(RA2_PIN, bitIdx & 0x04);
      digitalWrite(OUT_PIN,value);
      digitalWrite(REN_PIN,HIGH);
      delayMicroseconds(2);
      digitalWrite(REN_PIN,LOW);
}

void sampleISR(){
  static int32_t phaseAcc = 0;
  phaseAcc += currentStepSize;
  int32_t Vout = phaseAcc >> 24;
  analogWrite(OUTR_PIN, Vout + 128);
}

void scanKeysTask(void * pvParameters);

void displayUpdateTask(void * pvParameters);

void setup() {
  // put your setup code here, to run once:

  //Set pin directions
  pinMode(RA0_PIN, OUTPUT);
  pinMode(RA1_PIN, OUTPUT);
  pinMode(RA2_PIN, OUTPUT);
  pinMode(REN_PIN, OUTPUT);
  pinMode(OUT_PIN, OUTPUT);
  pinMode(OUTL_PIN, OUTPUT);
  pinMode(OUTR_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(C0_PIN, INPUT);
  pinMode(C1_PIN, INPUT);
  pinMode(C2_PIN, INPUT);
  pinMode(C3_PIN, INPUT);
  pinMode(JOYX_PIN, INPUT);
  pinMode(JOYY_PIN, INPUT);

  //Initialise display
  setOutMuxBit(DRST_BIT, LOW);  //Assert display logic reset
  delayMicroseconds(2);
  setOutMuxBit(DRST_BIT, HIGH);  //Release display logic reset
  u8g2.begin();
  setOutMuxBit(DEN_BIT, HIGH);  //Enable display power supply

  //Initialise UART
  Serial.begin(9600);
  Serial.println("Hello World");

  //Timer Interrupt 
  TIM_TypeDef *Instance = TIM1;
  HardwareTimer *sampleTimer= new HardwareTimer(Instance);
  sampleTimer->setOverflow(22000, HERTZ_FORMAT);
  sampleTimer->attachInterrupt(sampleISR);
  sampleTimer->resume();

  TaskHandle_t scanKeysHandle = NULL;
  xTaskCreate(scanKeysTask, /* Function that implements the task */
                 "scanKeys", /* Text name for the task */
                 64,         /* Stack size in words, not bytes*/
                 NULL,       /* Parameter passed into the task */
                 1,          /* Task priority*/
                 &scanKeysHandle);
  
  vTaskStartScheduler();

}

uint8_t readCols(){
  uint8_t C0 = digitalRead(C0_PIN);
  uint8_t C1 = digitalRead(C1_PIN);
  uint8_t C2 = digitalRead(C2_PIN);
  uint8_t C3 = digitalRead(C3_PIN);
  uint8_t funcOut = (C0) | (C1 << 1) | (C2 << 2) | (C3 << 3 ) ; 
  return funcOut ; 
}

void setRow(uint8_t rowIdx){
  digitalWrite(REN_PIN, LOW);

  if ((rowIdx) == 0){
  digitalWrite(RA2_PIN, LOW);
  digitalWrite(RA1_PIN, LOW);
  digitalWrite(RA0_PIN, LOW);
  }
   if ((rowIdx) == 1){
  digitalWrite(RA2_PIN, LOW);
  digitalWrite(RA1_PIN, LOW);
  digitalWrite(RA0_PIN, HIGH);
  }
   if ((rowIdx) == 2){
  digitalWrite(RA2_PIN, LOW);
  digitalWrite(RA1_PIN, HIGH);
  digitalWrite(RA0_PIN, LOW);
  }

  digitalWrite(REN_PIN, HIGH);
}

int keyState(uint8_t * keyArray){
  switch(keyArray[0]){
    case 0xE:
      return 0 ; 
      break;

    case 0xD:
      return 1;
      break;

    case 0xB:
      return 2;
      break;   

    case 0x7:
      return 3;
      break;

    default:
      break;
         
  }

  switch(keyArray[1]){
    case 0xE:
      return 4;
      break;

    case 0xD:
      return 5;
      break;

    case 0xB:
      return 6;
      break;

    case 0x7:
      return 7;
      break;
    
    default:
      break;    
  }

  switch(keyArray[2]){
    case 0xE:
      return 8;
      break;

    case 0xD:
      return 9;
      break;

    case 0xB:
      return 10;
      break;

    case 0x7:
      return 11;
      
      break;

    default:
      return 12;
      break;    
  }
  
  
}

void scanKeysTask(void * pvParameters) {
    const TickType_t xFrequency = 50/portTICK_PERIOD_MS;
    TickType_t xLastWakeTime= xTaskGetTickCount();
    while (true){
    vTaskDelayUntil( &xLastWakeTime, xFrequency ); //Go Here or at the end of loop? Test
    for(int i ; i < 3 ; i++)
    {
      setRow(i);
      delayMicroseconds(3);
      uint8_t keys = readCols();
      keyArray[i] = keys;
      
    }

    uint16_t keyOut = (keyArray[2] << 8 ) | (keyArray[1] << 4 ) |  (keyArray[0]) ; 
    uint8_t Idx = keyState(keyArray);
    int32_t localCurrentStepSize = STEPSIZES[Idx] ; 

    __atomic_store_n(&currentStepSize,localCurrentStepSize,__ATOMIC_RELAXED);
    }
}

void loop() {
  // // put your main code here, to run repeatedly:
  // static uint32_t next = millis();
  // static uint32_t count = 0;

  // //Reading Array
  // uint8_t keys ;

  // if (millis() > next) 
  // {
  //   next += interval;

  //   //Update display
  //   u8g2.clearBuffer();         // clear the internal memory
  //   u8g2.setFont(u8g2_font_ncenB08_tr); // choose a suitable font
  //   u8g2.drawStr(2,10,"Helllo World!");  // write something to the internal memory
    
  //   scanKeysTask();

  //   u8g2.setCursor(2,20);
  //   u8g2.print(keyOut,HEX);

  //   //Note Played
  //   u8g2.setCursor(2,30);
  //   u8g2.print(NOTES[noteIdx]);

  //   u8g2.sendBuffer();          // transfer internal memory to the display

  //   //Toggle LED
  //   digitalToggle(LED_BUILTIN);
  // }
}