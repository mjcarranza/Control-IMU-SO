#include <Servo.h>

Servo servoDer;
Servo servoIzq;
int currentPosDer = 0;
int currentPosIzq = 0;
bool newData = false;
char receivedChars[32]; // Array to store the received string
int index = 0;

void setup() {
  Serial.begin(9600);
  servoDer.attach(9); // Change the pin according to your setup for the right servo
  servoIzq.attach(10); // Change the pin according to your setup for the left servo
}

void loop() {
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
  }
}
