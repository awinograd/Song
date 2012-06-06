#include <JsonHandler.h>
#include <HardwareSerial.h>
#include <SD.h>

#define END_CMD_CHAR '!'

HardwareSerial Uart = HardwareSerial();

#define UART_BUFFER_SIZE 50

char response[120];

JsonHandler::JsonHandler(){
}

void JsonHandler::setup(){
	Uart.begin(9600);       // start Uart communication at 9600bps
}

bool JsonHandler::inputAvailable(){
 return Uart.available() || Serial.available(); 
}

void JsonHandler::readChar(char &c){
  if(Uart.available()){
    c = Uart.read();
  }
  if(Serial.available()){
    c = Serial.read();
  }  
}

void JsonHandler::readCommand(char* buffer, char* data){
  bool dataInfo = false;
  int i = 0;
  
  //wait a some time to allow the input stream to buffer so we can read whole commands in.
  delay(UART_BUFFER_SIZE);

  while(inputAvailable()){
    char inChar;
    readChar(inChar);
    if (inChar == END_CMD_CHAR){
      return; 
    }
    if(!dataInfo){
      if (inChar == ','){
        i=0;
        dataInfo=true; 
        continue;
      }
      buffer[i] = inChar;  
      i++;
      if( i > UART_BUFFER_SIZE ){
        buffer[i-1] = '\0';
        Uart.print("Command too long.");
      }
      buffer[i] = '\0';
    }
    else{
      data[i] = inChar;
      i++;
      data[i] = '\0';
    }
  }
}

void JsonHandler::addKeyValuePair(const char* key, const char* val, bool firstPair){
  char* appendChars = ",\"";
  int offset = 1;
  if (firstPair){
    strcpy(response, "{}");
    offset = 0;
    appendChars = "\"";
  }

  int len = strlen(response);
  int lenKey = strlen(key);
  int lenVal = strlen(val);
  strcpy(response+len-1, appendChars);
  strcpy(response+len+offset, key);
  strcpy(response+len+offset+lenKey, "\":\"");
  strcpy(response+len+offset+lenKey+3, val);
  strcpy(response+len+offset+lenKey+3+lenVal, "\"}");
  response[strlen(response)+1] = '\0';
}

void JsonHandler::addKeyValuePair(const char* key, const char* val){
  addKeyValuePair(key, val, false); 
}

void JsonHandler::respond(){
    Serial.println(response);
	Uart.print(response);
	Uart.print(END_CMD_CHAR);
	response[0] = '\0';
}