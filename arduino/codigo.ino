#include <Servo.h>

Servo servoDer;
int currentPos; // Variable to store the current servo position
bool newData = false; // Flag to indicate if new data is available

void setup() {
  Serial.begin(9600);
  servoDer.attach(9); // Change the pin according to your setup
}

void loop() {
  if (Serial.available() > 0) {
    char receivedChar = Serial.read(); // Read the incoming byte
    
    if (receivedChar == '\n') { // Check if the received character is newline
      newData = true; // Set flag to indicate new data is available
    } else { // If character is not newline, append it to current position string
      currentPos = currentPos * 10 + (receivedChar - '0');
    }
  }

  if (newData) { // Check if new data is available
    Serial.println("Listo");
    // Validate that the position is within the servo's range
    if (currentPos >= 0 && currentPos <= 180) {
      // Update the servo position to the stored value
      servoDer.write(currentPos);
    }

    // Reset variables for next data
    currentPos = 0;
    newData = false;
  }
}
