// --- Pin Definitions ---
const int IRPins[8] = {22, 24, 26, 28, 30, 32, 34, 36};
const int Dir1Pin_A = 23; // Left Motor Direction 1
const int Dir2Pin_A = 25; // Left Motor Direction 2
const int SpeedPin_A = 2;   // Left Motor Speed (PWM)
const int Dir1Pin_B = 27; // Right Motor Direction 1
const int Dir2Pin_B = 29; // Right Motor Direction 2
const int SpeedPin_B = 3;   // Right Motor Speed (PWM)
const int EchoPin = 8;
const int TrigPin = 9;

// --- PID Constants (튜닝이 필요한 값들) ---
float Kp = 45.0;
float Ki = 0.0;
float Kd = 80.0;

// --- Global Variables ---
float error = 0;
float lastError = 0; 
float integral = 0;
const float midPsn = 3.5; 
int baseSpeed = 130;     // Base speed of the robot


float filtered_psn = 3.5; // 필터링된 위치 값
float alpha = 0.87; // LPF 상수 (0.0 ~ 1.0). 1에 가까울수록 원본, 0에 가까울수록 부드러움.
                   // 튜닝 대상


void setup() {
  Serial.begin(9600);
  for (int i = 0; i < 8; i++) {
    pinMode(IRPins[i], INPUT);
  }
  pinMode(Dir1Pin_A, OUTPUT);
  pinMode(Dir2Pin_A, OUTPUT);
  pinMode(SpeedPin_A, OUTPUT);
  pinMode(Dir1Pin_B, OUTPUT);
  pinMode(Dir2Pin_B, OUTPUT);
  pinMode(SpeedPin_B, OUTPUT);

  pinMode(TrigPin, OUTPUT);
  pinMode(EchoPin, INPUT);
}

void stopMotors() {
  analogWrite(SpeedPin_A, 0);
  analogWrite(SpeedPin_B, 0);
  digitalWrite(Dir1Pin_A, LOW);
  digitalWrite(Dir2Pin_A, LOW);
  digitalWrite(Dir1Pin_B, LOW);
  digitalWrite(Dir2Pin_B, LOW);
}

void loop() {
  long duration;
  float distance;
  digitalWrite(TrigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(TrigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(TrigPin, LOW);
  duration = pulseIn(EchoPin, HIGH);
  distance = duration * 0.034 / 2;

  if (distance < 24 && distance > 0) {
    stopMotors(); 
    Serial.println("Obstacle Detected! Stopping.");
  } 
  else {
    // Declare variables at the top of the loop's scope
    int vleft = 0;
    int vright = 0;
    
    // --- 1. Read Sensor Values ---
    int IRValues[8];
    int min = 8, max = -1;
    int count = 0;
    for (int i = 0; i < 8; i++) {
      IRValues[i] = digitalRead(IRPins[i]);
      if (IRValues[i] == LOW) { // Assuming LOW means the sensor is over the black line
        if (min == 8) min = i;
        max = i;
        count++;
      }
    }

    // --- 2. Process Sensor Data and Control Motors ---
    // If the line is detected
    if (count > 0) {
      float psn = (min + max) / 2.0; 
      

      filtered_psn = (alpha * psn) + ((1.0 - alpha) * filtered_psn);
      // ------------------------------------------
      
      error = midPsn - filtered_psn; 
      
      integral += error;
      integral = constrain(integral, -400, 400); // Prevent integral windup

      float derivative = error - lastError;
    
      float correction = Kp * error + Ki * integral + Kd * derivative;

      lastError = error; 
      // ----------------------------------------------------


      vleft = baseSpeed - correction;
      vright = baseSpeed + correction;
      
      // Constrain speeds
      vleft = constrain(vleft, -255, 255);
      vright = constrain(vright, -255, 255);

      // --- Control Left Motor ---
      if (vleft >= 0) {
        digitalWrite(Dir1Pin_A, LOW); digitalWrite(Dir2Pin_A, HIGH);
        analogWrite(SpeedPin_A, vleft);
      } else {
        digitalWrite(Dir1Pin_A, HIGH); digitalWrite(Dir2Pin_A, LOW);
        analogWrite(SpeedPin_A, -vleft);
      }

      // --- Control Right Motor ---
      if (vright >= 0) {
        digitalWrite(Dir1Pin_B, LOW); digitalWrite(Dir2Pin_B, HIGH);
        analogWrite(SpeedPin_B, vright);
      } else {
        digitalWrite(Dir1Pin_B, HIGH); digitalWrite(Dir2Pin_B, LOW);
        analogWrite(SpeedPin_B, -vright);
      }
    } 
    // If the line is lost
    else {

      if (lastError > 0) { // Line was on the left, so turn right
        digitalWrite(Dir1Pin_A, LOW);  digitalWrite(Dir2Pin_A, HIGH); 
        digitalWrite(Dir1Pin_B, LOW); digitalWrite(Dir2Pin_B, HIGH);  
        analogWrite(SpeedPin_A, 70);
        analogWrite(SpeedPin_B, 120);
      } 
      else { // Line was on the right, so turn left
        digitalWrite(Dir1Pin_A, LOW); digitalWrite(Dir2Pin_A, HIGH); 
        digitalWrite(Dir1Pin_B, LOW);  digitalWrite(Dir2Pin_B, HIGH); 
        analogWrite(SpeedPin_A, 120);
        analogWrite(SpeedPin_B, 70);
      }

      // if (lastError > 0) { // Line was on the left, so turn right
      //   digitalWrite(Dir1Pin_A, HIGH);  digitalWrite(Dir2Pin_A, LOW); 
      //   digitalWrite(Dir1Pin_B, HIGH); digitalWrite(Dir2Pin_B, LOW);  
      //   analogWrite(SpeedPin_A, 200);
      //   analogWrite(SpeedPin_B, 100);
      // } 
      // else { // Line was on the right, so turn left
      //   digitalWrite(Dir1Pin_A, HIGH); digitalWrite(Dir2Pin_A, LOW); 
      //   digitalWrite(Dir1Pin_B, HIGH);  digitalWrite(Dir2Pin_B, LOW); 
      //   analogWrite(SpeedPin_A, 100);
      //   analogWrite(SpeedPin_B, 200);
      // }
    }

    //--- 3. Debugging Output ---
    Serial.print("Err: ");
    Serial.print(error);
    Serial.print(" | L: ");
    Serial.print(vleft);
    Serial.print(" | R: ");
    Serial.println(vright);
  }

  delay(10); // A small delay to stabilize the loop
}