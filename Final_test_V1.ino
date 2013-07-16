/*
Test program for 2 station 

*/

#include <SPI.h>
#include <LiquidCrystal.h>
#include "nRF24L01.h"
#include "RF24.h"


//=================================================================================
//=============================Global variables and pins===========================
//=================================================================================

//-------------------radioA----------------------- 
// Set up nRF24L01 radioA on SPI bus plus pins 42 & 40
RF24 radio(42,40);
union radpack
{
  byte package[32];
  int myints[16];
  float myfloats[8];
};
radpack radioA_in;
radpack radioA;
//
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };
typedef enum { role_ping_out = 1, role_pong_back } role_e;
const char* role_friendly_name[] = { "invalid", "Ping out", "Pong back"};
role_e role;

//
LiquidCrystal lcd(12, 11, 8, 7, 6, 5);


//----------------Pin Definations-------------------

const int role_pin = 2;
const int button_pin = 9;
const int spawnButt = 30; 
const int buzzer = 46;//assigned to timer 5, compare A
const int LEDG = 36;
const int LEDO = 35;
const int LEDR = 34;


//-------------Global variables--------------------

boolean lck = false;
boolean tempbool = false;
int transFail = 0;
int i = 0;
int resendID = 0;
unsigned long timerOld = 0;
unsigned long timerTemp = 0;


//-----------Adjustment variables------------------

int ticketStart = 80; //The number of tickets to start with
int duty = 250; // Dutycycle for teh buzzer (0-255)
unsigned long timerMax = 30; //in min
unsigned long timerStep = 30; //in Sec
unsigned long buttTimer = 2; //in Sec


//=================================================================================
//=====================================Setup=======================================
//=================================================================================

void setup(void)
{
  pinMode(buzzer, OUTPUT);
  pinMode(role_pin, INPUT);
  pinMode(13,OUTPUT);
  pinMode(spawnButt, INPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDO, OUTPUT);
  pinMode(LEDR, OUTPUT);
  digitalWrite(buzzer,LOW);
  digitalWrite(role_pin,HIGH);
  digitalWrite(13,LOW);
  digitalWrite(spawnButt, HIGH);
  digitalWrite(LEDG, LOW);
  digitalWrite(LEDO, LOW);
  digitalWrite(LEDR, LOW);
  delay(200); 
  
  // read the address pin, establish our role
  if (digitalRead(role_pin))
    role = role_ping_out;
  else
    role = role_pong_back;
  
  //Initilise data array  
  for(i = 0 ; i < 15 ; i++)
  {
    radioA_in.myints[i] = 0;
    radioA.myints[i] = 0;
  }
  
  //Set starting tickets
  radioA.myints[2] = ticketStart;
  radioA.myints[3] = ticketStart;
  
  //Set up variables for use with timer
  timerMax = timerMax * 60 * 1000;
  timerStep = timerStep * 1000;
  buttTimer = buttTimer * 1000;

  //Set up serial comunication for debugging
  Serial.begin(57600);
  Serial.write("\n\rRF24/examples/pingpair/\n\r");
  Serial.write("ROLE:");
  Serial.write(role_friendly_name[role]);
  Serial.write("\n\r");
  
  //Set up the radio and role of the station
  radio.begin();
  radio.setRetries(20,15);
  radio.setPayloadSize(16);
  //Set radio up for high power long distance transmition 
  radio.setPALevel(RF24_PA_HIGH);
  radio.setDataRate(RF24_250KBPS);
  
  if ( role == role_ping_out )
  {
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1,pipes[1]);
    digitalWrite(LEDG, HIGH);
  }
  else
  {
    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(1,pipes[0]);
    digitalWrite(LEDO, HIGH);
  }
  
  radio.startListening();
  radio.printDetails();
  
  //Initilise the LCD
  lcd.begin(16, 2);
  
  //Display inital start up screen and tests
  lcd.clear();
  lcd.print("BF3 station test");
  delay(2000);
  
  digitalWrite(13, HIGH);
//  digitalWrite(buzzer, HIGH);
//  delay(20);
  timer5(duty);
  delay(2000);
  digitalWrite(13, LOW);
  timer5(0);
  
  lcd.clear();
  updateTic();
}


//=================================================================================
//=====================================Loop========================================
//=================================================================================

void loop(void)
{
  //-------------------radio-----------------------
  if ( radio.available() )
  {
    bool done = false;
    while (!done)
    {
      done = radio.read( &radioA_in, 32);
    }
    delay(20);
    tempbool = checkData();
      
    if(tempbool == false)
    {
      transFail++;
      if(transFail > 50)
      {
        //To many transmition errors, do something here 
      }
    }
  }
  
  //------------------Inputs--------------------------
  if((digitalRead(spawnButt) == HIGH) && (lck == false))
  {
    timerTemp = millis();
    while(digitalRead(spawnButt) == HIGH){//Do nothing
    }
    
    if((millis() - timerTemp) > buttTimer)
    {
      if(role == role_ping_out)
        radioA.myints[2]--;
      else
        radioA.myints[3]--;
    }
      
    sendData();
    lck = true;
    digitalWrite(13,HIGH);
  }
  else if(digitalRead(spawnButt) == LOW){
    digitalWrite(13,LOW);
    lck = false;
  }
  
  //-------------------Timer-----------------------
  if(role == role_ping_out)
  {
     if((millis() - timerOld) >= timerStep)
     {
        radioA.myints[2]--;
        radioA.myints[3]--;
        timerOld = millis();
        sendData();
        updateTic();
     }
     updateTimer();
  }
  
  //------------Check win comditions----------------
  if(role == role_ping_out)//only for timer check
  {
    if(millis() >= timerMax)
    {
      lcd.clear();
      digitalWrite(buzzer, HIGH);
      delay(20);
      timer5(duty);
      while(1)
      {
       lcd.setCursor(0,0);
       lcd.print("Time over");
       lcd.setCursor(0,1);
       if(radioA.myints[3] <= radioA.myints[2])
         lcd.print("Team A Wins");
       else if(radioA.myints[2] <= radioA.myints[3])
         lcd.print("Team B wins");
       else
         lcd.print("Game Drawn");
       delay(1000);
       lcd.clear();
       delay(800);
      } 
    }
  }
  if(radioA.myints[2] <= 0)
  {
    lcd.clear();
    digitalWrite(buzzer, HIGH);
    delay(20);
    timer5(duty);
    while(1)
    {
       lcd.setCursor(0,0);
       lcd.print("Tickets expired");
       lcd.setCursor(0,1);
       lcd.print("Team B Wins");
       delay(1000);
       lcd.clear();
       delay(800);
    }
  }
  if(radioA.myints[3] <= 0)
  {
    lcd.clear();
    digitalWrite(buzzer, HIGH);
    delay(20);
    timer5(duty);
    while(1)
    {
       lcd.setCursor(0,0);
       lcd.print("Tickets expired");
       lcd.setCursor(0,1);
       lcd.print("Team A Wins");
       delay(1000);
       lcd.clear();
       delay(800);
    }
  }
}


//=================================================================================
//================================Background functions=============================
//=================================================================================


//Check the input data array is sensical 
boolean checkData(void)
{
  for(i = 0 ; i <= 15 ; i++)
  {
    if((radioA_in.myints[i] < -65535) || (radioA_in.myints[i] > 65535))
      return false;
  }  
  
  lcd.setCursor(0, 1);
  lcd.print(radioA_in.myints[2]);
    
  for(i = 0 ; i <= 15 ; i++)
  {
    radioA.myints[i] = radioA_in.myints[i];
  }
  
  updateTic();
  
  return true;
}


// Prepare and transmit the information in the data array
void sendData()
{
  boolean done = false;
  //Packet name
  int tempa = radioA.myints[0];
  tempa++;
  if(tempa > 800)
  {
    tempa = 0;
  }
  radioA.myints[0] = tempa;
  
  radioA.myints[14] = 0;
  
  radio.stopListening();
  boolean ok = radio.write( &radioA, 32);   
  radio.startListening();
  
  updateTic();
}

// Update the top row of the lcd to display the ticket count
void updateTic()
{
  //LCD code
  lcd.setCursor(0, 0);
  lcd.print("A:");
  lcd.print(radioA.myints[2]);
  lcd.setCursor(8, 0);
  lcd.print("B:");
  lcd.print(radioA.myints[3]);
}

//Update the timer display on the LCD
void updateTimer()
{
  lcd.setCursor(0, 1);
  lcd.print("Time");
  lcd.print((millis()/1000)/60);
  lcd.print(":");
  if(((millis()/1000)%60) <= 9)
  {
    lcd.print("0");
    lcd.print((millis()/1000)%60);
  }
  else
  {
    lcd.print((millis()/1000)%60);
    lcd.print(" ");
  }
}

//Start/Stop timer 5 at a set duty cycle
//
// 0 - Stop the timer
// 1 - 255 Duty cycle
//
void timer5(int j)
{
  if(j == 0)
  {
    TCCR5B &= (0 << CS51) | (0 << CS50);
    delay(1);
    digitalWrite(buzzer, LOW);
    return;
  }
  TCCR5A |= ((1 << COM5A1) | (1 << WGM50));
  TCCR5B |= (1 << WGM52);
  //TIMSK5 |= ((1 << OCIE5A) | (1 << TOIE5));
  OCR5A = j;
  TCCR5B |= ((1 << CS51) | (1 << CS50));
}


// vim:cin:ai:sts=2 sw=2 ft=cpp
