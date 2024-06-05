#include <Servo.h>
#include <Wire.h>

#define MPU9250_ADDRESS 0x68
#define GYRO_FULL_SCALE_2000_DPS 0x18
Servo servoDer;
Servo servoIzq;
int currentPosDer = 0;
int currentPosIzq = 0;
bool newData = false;
char receivedChars[32]; // Array to store the received string
int index = 0;

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
  servoDer.attach(9); // Change the pin according to your setup for the right servo
  servoIzq.attach(10); // Change the pin according to your setup for the left servo
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
  //Serial.print(" GZ:");
  //Serial.println(gz);

  delay(50);
  //fin seccion giroscopio
  if (Serial.available() > 0) {
    // Lee el byte que llega desde la serial
    char data = Serial.read();
    Serial.println(data);
    delay(1000);
  }
   delay(50);
  /*
  if (Serial.available() > 0) {
    char receivedChar = Serial.read(); // Read the incoming byte

    if (receivedChar == '\n') { // Check if the received character is newline
      receivedChars[index] = '\0'; // Null-terminate the string
      newData = true; // Set flag to indicate new data is available
      index = 0; // Reset index for the next input
    } else {
      receivedChars[index] = receivedChar;
      index++;
    }
  }

  if (newData) {
    Serial.println("Listo");

    // Process the received string
    char *token = strtok(receivedChars, ",");
    if (token != NULL) {
      currentPosIzq = atoi(token); // Get the left servo position

      token = strtok(NULL, ",");
      if (token != NULL) {
        currentPosDer = atoi(token); // Get the right servo position

        // Validate and update the left servo position
        if (currentPosIzq >= 0 && currentPosIzq <= 180) {
          servoIzq.write(currentPosIzq);
        }

        // Validate and update the right servo position
        if (currentPosDer >= 0 && currentPosDer <= 180) {
          servoDer.write(currentPosDer);
        }
      }
    }

    newData = false; // Reset flag for the next data
  }*/
}
