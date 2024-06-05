#include <Servo.h>
#include <Wire.h>

#define MPU9250_ADDRESS 0x68
#define GYRO_FULL_SCALE_2000_DPS 0x18
Servo servoDer;
Servo servoIzq;
int currentPosDer = 0;
int currentPosIzq = 0;
int dx_value = 7;
int dy_value = 7;
//bool newData = false;
char receivedChars[32]; // Array to store the received string
int index = 0;
bool newData = false;
const byte numChars = 32;
//char receivedChars[numChars];   // an array to store the received data



void I2Cread(uint8_t Address, uint8_t Register, uint8_t Nbytes, uint8_t* Data)
{
  Wire.beginTransmission(Address);
  Wire.write(Register);
  Wire.endTransmission();

  Wire.requestFrom(Address, Nbytes);
  uint8_t index = 0;
  while (Wire.available())
    Data[index++] = Wire.read();
}

// Función auxiliar de escritura
void I2CwriteByte(uint8_t Address, uint8_t Register, uint8_t Data)
{
  Wire.beginTransmission(Address);
  Wire.write(Register);
  Wire.write(Data);
  Wire.endTransmission();
}

void setup() {
  Serial.begin(9600);
  servoDer.attach(3); // Change the pin according to your setup for the right servo
  servoIzq.attach(4); // Change the pin according to your setup for the left servo
  Wire.begin();
  I2CwriteByte(MPU9250_ADDRESS, 27, GYRO_FULL_SCALE_2000_DPS);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {

  // Lectura acelerómetro y giroscopio
  uint8_t Buf[14];
  I2Cread(MPU9250_ADDRESS, 0x3B, 14, Buf);

  // Convertir registros giroscopio
  int16_t gx = -(Buf[8] << 8 | Buf[9]);
  int16_t gy = -(Buf[10] << 8 | Buf[11]);
  int16_t gz = -(Buf[12] << 8 | Buf[13]);

  // Giroscopio
  Serial.print("GX:");
  Serial.print(gx);
  Serial.print(" GY:");
  Serial.println(gy);
  Recdata();
  showNewData();
 
  
  delay(25);
  parseInput(receivedChars);
  moverServo();
  delay(25);
  //fin seccion giroscopio

  
}

void Recdata(){

  static byte ndx = 0;
  char endMarker = '\n';
  char rc;

  while (Serial.available() > 0 && newData == false) {
      rc = Serial.read();

      if (rc != endMarker) {
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      }
      else {
        receivedChars[ndx] = '\0'; // terminate the string
        ndx = 0;
        newData = true;
      }
  }
}

void showNewData() {
  if (newData == true) {
    Serial.print("This just in ... ");
    Serial.println(receivedChars);
    newData = false;
  }
}
void parseInput(String data) {
  int dxIndex = data.indexOf("dx");
  int dyIndex = data.indexOf("dy");
  
  if (dxIndex != -1 && dyIndex != -1) {
    // Extraer los valores después de "dx" y "dy"
    dx_value = data.substring(dxIndex + 3, data.indexOf(' ', dxIndex + 3)).toInt();
    dy_value = data.substring(dyIndex + 3).toInt();
  }
}

void moverServo(){
  
  if(dx_value!=1){ servoIzq.write(90);}
  if(dy_value!=1){ servoDer.write(90);}
  else{
    servoDer.write(0);
    servoIzq.write(0);}
  delay(100);

}