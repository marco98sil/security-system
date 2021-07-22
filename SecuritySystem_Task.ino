#include <cmsis_os.h>
#include <croutine.h>
#include <event_groups.h>
#include <FreeRTOS.h>
#include <FreeRTOSConfig.h>
#include <FreeRTOSConfig_Default.h>
#include <list.h>
#include <message_buffer.h>
#include <mpu_prototypes.h>
#include <portmacro.h>
#include <queue.h>
#include <semphr.h>
#include <stack_macros.h>
#include <STM32FreeRTOS.h>
#include <stream_buffer.h>
#include <task.h>
#include <timers.h>
#include <string.h>


#include <Keypad.h>
#include <LiquidCrystal.h>

const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns

//define the cymbols on the buttons of the keypads
char hexaKeys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
#define a1 PB8
#define a2 PB9
#define a3 PA5
#define a4 PA6
#define a5 PA7
#define a6 PA2
#define a7 PA15
#define a8 PB2
#define a9 PA4
byte rowPins[ROWS] = {a1, a2, a3, a4}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {a5, a6, a7, a8}; //connect to the column pinouts of the keypad

SemaphoreHandle_t xBinSemON;
SemaphoreHandle_t xBinSemPIR;
SemaphoreHandle_t xBinSemBUZZER;
SemaphoreHandle_t xBinSemKEYBOARD;
SemaphoreHandle_t xBinSemWINDOWS;
SemaphoreHandle_t xBinSemBALCONY;
SemaphoreHandle_t xMutex;
SemaphoreHandle_t xBinSemDISPLAY;

#define PinPIR  PA0  
#define speakerPin PC0 // define LED Interface
#define buttonPinON PC13 
#define buttonWindowsPin PA1
#define speakerPin PC0

//lcdPin
#define rs PD14
#define en PB0
#define d4 PA3
#define d5 PB4
#define d6 PB1
#define d7 PA4

//ultrasonicPin
#define echo PC4
#define trig PC5

Keypad customKeypad = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS); 
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);


#define ledBlueYellow PC9 

int varBT=LOW;//variable to block thread buzzer

int state=LOW;
int count = 0; //variable to write the password in the correct way
int varD=0; //shared varDiable for display task
char str;

static void Thread_ON(void* arg) {
  UNUSED(arg);
  
  while (1) {
    xSemaphoreTake(xBinSemON, portMAX_DELAY);
    xSemaphoreTake(xMutex,portMAX_DELAY);
    
    digitalWrite(ledBlueYellow, LOW);   // turn the LED on (HIGH is the voltage level)
    
    int valButtonON = digitalRead (buttonPinON);
    
    if(valButtonON==LOW){
      xSemaphoreGive(xBinSemPIR);
      xSemaphoreGive(xBinSemKEYBOARD);
      xSemaphoreGive(xBinSemWINDOWS);
      digitalWrite(ledBlueYellow, HIGH); 
      varD=1;
      xSemaphoreGive(xBinSemDISPLAY);
    }
    else
      xSemaphoreGive(xBinSemON);

      xSemaphoreGive(xMutex);
  }
    
}


static void Thread_WINDOWS(void* arg){
  UNUSED(arg);
  while(1){
    xSemaphoreTake(xBinSemWINDOWS, portMAX_DELAY);
    int valButtonWindows= digitalRead(buttonWindowsPin);
    if(valButtonWindows==LOW){
      xSemaphoreGive(xBinSemBUZZER);
    }
    else{
      xSemaphoreGive(xBinSemBALCONY);
    }  
  }
}

static void Thread_PIR(void* arg) {
  UNUSED(arg);
  int pirState = LOW;
  
  while (1) {
    xSemaphoreTake(xBinSemPIR, portMAX_DELAY);
    int tmp = digitalRead(PinPIR);  // read input value
    if (tmp == HIGH) {            // check if the input is HIGH
        if (pirState == LOW) {
          state=HIGH;
          xSemaphoreGive(xBinSemBUZZER);
          // We only want to print on the output change, not state
          pirState = HIGH;
        }
     } 
     else {
        if (pirState == HIGH){
          pirState = LOW;
        }
        xSemaphoreGive(xBinSemWINDOWS);
    } 
  }
}


static void Thread_BALCONY(void* arg) {
  UNUSED(arg);
  
  while (1) {
    xSemaphoreTake(xBinSemBALCONY, portMAX_DELAY);
    long distance,duration;
    digitalWrite(trig,HIGH);
    delayMicroseconds(10);
    digitalWrite(trig,LOW);
    duration=pulseIn(echo,HIGH);
    distance=(duration/2)/7.6;
    if(distance<200){
      xSemaphoreGive(xBinSemBUZZER);
    }
    else{
      xSemaphoreGive(xBinSemPIR);
    }
  } 
}


static void Thread_BUZZER(void* arg) {
  UNUSED(arg);
  
  while (1) {
    xSemaphoreTake(xBinSemBUZZER, portMAX_DELAY);
    if(state==HIGH){
      vTaskDelay(10000);
      state=LOW;
    }
    xSemaphoreTake(xMutex,portMAX_DELAY);
    analogWrite (speakerPin, 255);
    delay(100);
    analogWrite (speakerPin, 0);
    //delay(400);
    
    if(varBT!=HIGH){
      xSemaphoreGive(xBinSemBUZZER);
    }
    else
      varBT=LOW;
    xSemaphoreGive(xMutex);
    vTaskDelay(100);
  }
}

static void Thread_DISPLAY(void* arg){
  lcd.begin(16, 2);
  lcd.print("ALLARME SPENTO");
  while(1){
      xSemaphoreTake(xBinSemDISPLAY,portMAX_DELAY);
      xSemaphoreTake(xMutex,portMAX_DELAY);
      if(varD==1){
        lcd.clear();
        lcd.print("ALLARME ATTIVO");
        lcd.setCursor(0,1);
        lcd.print("PASSWORD:");
      }
      else if (varD==2){
        lcd.setCursor(10+count,1);
        lcd.print(str);
        count++;
     }
     else if (varD==3){
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("PASSWORD OK");
        delay(5000);
        lcd.clear();
        lcd.print("ALLARME SPENTO");
     }
     else if (varD==4){
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("PASSWORD WRONG");
        lcd.setCursor(0,1);
        lcd.print("PASSWORD:");
     }


     xSemaphoreGive(xMutex);
  }
}


static void Thread_KEYBOARD(void* arg) {
  UNUSED(arg);
  char password[5]="1234";
  char test[6];
  
  while(1){
    xSemaphoreTake(xBinSemKEYBOARD,portMAX_DELAY);
    xSemaphoreTake(xMutex,portMAX_DELAY);

    
    char customKey = customKeypad.getKey();
  
    if (customKey){
      varD=2;
      str = customKey;      
      xSemaphoreGive(xBinSemDISPLAY);
      test[count]=customKey;
    }
    
    if(customKey=='#'){
       if(count==4 && strncmp(test,password,4)==0){//password ok
          varD = 3;
          xSemaphoreGive(xBinSemDISPLAY);
          
          varBT=HIGH;
          xSemaphoreGive(xBinSemON);
          count=0;
       }
       else{
          //password wrong
          for(int i=0;i<count;i++){
              test[i]='*';
          }
          count=0;
          varD = 4;
          xSemaphoreGive(xBinSemDISPLAY);
          //realises the semaphore because the passowrd is wrong and we want another one
          xSemaphoreGive(xBinSemKEYBOARD);
          }
     }
      
    else{
      xSemaphoreGive(xBinSemKEYBOARD);
    }
    
    xSemaphoreGive(xMutex);
    vTaskDelay(100);
  }
}


void setup() {
  // put your setup code here, to run once:
  xBinSemON = xSemaphoreCreateBinary();
  xBinSemPIR = xSemaphoreCreateBinary();
  xBinSemBUZZER = xSemaphoreCreateBinary();
  xBinSemKEYBOARD = xSemaphoreCreateBinary();
  xBinSemWINDOWS = xSemaphoreCreateBinary();
  xBinSemBALCONY=xSemaphoreCreateBinary();
  xMutex = xSemaphoreCreateBinary();
  xBinSemDISPLAY=xSemaphoreCreateBinary();

  xTaskCreate(Thread_ON, "Thread_ON", 100, NULL, 1, NULL);
  xTaskCreate(Thread_PIR, "Thread_PIR", 100, NULL, 1, NULL);
  xTaskCreate(Thread_BUZZER, "Thread_BUZZER", 100, NULL, 1, NULL);
  xTaskCreate(Thread_KEYBOARD, "Thread_KEYBOARD", 100, NULL, 1, NULL);
  xTaskCreate(Thread_WINDOWS, "Thread_WINDOWS", 100, NULL, 1, NULL);
  xTaskCreate(Thread_BALCONY, "Thread_BALCONY", 100, NULL, 1, NULL);
  xTaskCreate(Thread_DISPLAY, "Thread_DISPLAY", 100, NULL, 1, NULL);


  xSemaphoreGive(xBinSemON);
  xSemaphoreGive(xMutex);
  pinMode(PinPIR, INPUT);
  pinMode (buttonPinON, INPUT); 
  pinMode (buttonWindowsPin, INPUT); 
  pinMode(ledBlueYellow, OUTPUT);
  pinMode(speakerPin,OUTPUT);
  pinMode(echo,INPUT);
  pinMode(trig,OUTPUT);
  digitalWrite(trig,LOW);

  lcd.begin(16, 2);
  lcd.print("ALLARME SPENTO");

  vTaskStartScheduler();

}

void loop() {
  // put your main code here, to run repeatedly:

}
