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
float Kp = 15.0;
float Ki = 0.0;
float Kd = 35.0;

// --- Global Variables ---
float error = 0;
float lastError = 0;
float integral = 0;
const float midPsn = 3.5; // IR Sensor array's center position
int baseSpeed = 90;      // Base speed of the robot
int avglast = 0;

// --- (추가된 부분) 과거 1000개의 에러 평균을 내기 위한 변수들 ---
const int HISTORY_SIZE =50;
float errorHistory[HISTORY_SIZE];
int historyIndex = 0;
float errorSum = 0.0;
int historyCount = 0;

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

// (추가된 부분) 에러 기록 배열을 0으로 초기화합니다.
  for (int i = 0; i < HISTORY_SIZE; i++) {
    errorHistory[i] = 0.0;
  }
}

void stopMotors() {
  analogWrite(SpeedPin_A, 0);
  analogWrite(SpeedPin_B, 0);
  // 방향 핀도 LOW로 설정하여 모터 드라이버의 부하를 줄여줄 수 있습니다.
  digitalWrite(Dir1Pin_A, LOW);
  digitalWrite(Dir2Pin_A, LOW);
  digitalWrite(Dir1Pin_B, LOW);
  digitalWrite(Dir2Pin_B, LOW);
}

void loop() {
// --- (추가) 0. 초음파 센서로 거리 측정 ---
  long duration;
  float distance;
  // Trig 핀을 잠시 LOW로 만들어 깨끗한 신호를 보낼 준비를 합니다.
  digitalWrite(TrigPin, LOW);
  delayMicroseconds(2);
  // Trig 핀을 10 마이크로초 동안 HIGH로 만들어 초음파를 발사합니다.
  digitalWrite(TrigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(TrigPin, LOW);
  // Echo 핀이 HIGH가 되는 시간을 측정합니다. (초음파가 돌아온 시간)
  duration = pulseIn(EchoPin, HIGH);
  // 측정된 시간(duration)을 cm 단위의 거리로 변환합니다.
  distance = duration * 0.034 / 2;

  // 디버깅을 위해 측정된 거리를 출력합니다.
  // Serial.print("Distance: ");
  // Serial.print(distance);
  // Serial.print("cm | ");

  // --- (수정) 1. 장애물 감지 및 분기 처리 ---
  // 측정된 거리가 0보다 크고 10cm보다 작으면 장애물로 인식합니다.
  // (distance > 0 조건은 센서의 잘못된 측정값을 걸러내기 위함입니다)
  if (distance < 20 && distance > 0) {
    stopMotors(); // 모터 정지
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
      
      // --- PID Calculation ---
      error = midPsn - psn;
      integral += error;
      integral = constrain(integral, -400, 400); // Prevent integral windup

      // --- (수정된 부분) Error History 업데이트 및 평균 계산 ---
      // 1. 합계에서 가장 오래된 에러 값을 뺍니다.
      errorSum -= errorHistory[historyIndex];
      // 2. 현재 에러 값을 히스토리 배열과 합계에 추가합니다.
      errorHistory[historyIndex] = error;
      errorSum += error;
      // 3. 다음 저장을 위해 인덱스를 증가시킵니다. (배열 끝에 도달하면 0으로 리셋)
      historyIndex++;
      if (historyIndex >= HISTORY_SIZE) {
          historyIndex = 0;
      }
      // 4. 버퍼가 찰 때까지 저장된 값의 개수를 셉니다.
      if (historyCount < HISTORY_SIZE) {
          historyCount++;
      }
      
      // 5. 과거 에러 값들의 평균을 계산하여 'lastError'로 사용합니다.
      //    (프로그램 시작 시 0으로 나누는 것을 방지)
      if (historyCount > 0) {
          lastError = errorSum / historyCount;
      } else {
          lastError = 0;
      }
      
      // 'lastError'에 이전 에러가 아닌 에러 평균값을 사용하여 미분항(Derivative)을 계산합니다.
      float derivative = error - lastError;
    
      float correction = Kp * error + Ki * integral + Kd * derivative;

      // --- Calculate Motor Speeds ---
      
      vleft = baseSpeed - correction;
      vright = baseSpeed + correction;
      

      // Constrain speeds to be within motor driver's capabilities (-255 to 255 for direction control)
      vleft = constrain(vleft, -255, 255);
      vright = constrain(vright, -255, 255);

      // --- Control Left Motor ---
      if (vleft >= 0) {
        digitalWrite(Dir1Pin_A, LOW); // Forward (Swapped)
        digitalWrite(Dir2Pin_A, HIGH);
        analogWrite(SpeedPin_A, vleft);
      } else {
        digitalWrite(Dir1Pin_A, HIGH); // Backward (Swapped)
        digitalWrite(Dir2Pin_A, LOW);
        analogWrite(SpeedPin_A, -vleft); // Speed must be positive
      }

      // --- Control Right Motor ---
      if (vright >= 0) {
        digitalWrite(Dir1Pin_B, LOW); // Forward (Swapped)
        digitalWrite(Dir2Pin_B, HIGH);
        analogWrite(SpeedPin_B, vright);
      } else {
        digitalWrite(Dir1Pin_B, HIGH); // Backward (Swapped)
        digitalWrite(Dir2Pin_B, LOW);
        analogWrite(SpeedPin_B, -vright); // Speed must be positive
      }
    } 
    // If the line is lost
    else {
      // Search for the line based on the last known position
      if (lastError > 0) { // Line was on the left, so turn right to find it
        digitalWrite(Dir1Pin_A, HIGH);  digitalWrite(Dir2Pin_A, LOW); // Left motor forward (Swapped)
        digitalWrite(Dir1Pin_B, HIGH); digitalWrite(Dir2Pin_B, LOW);  // Right motor backward (Swapped)
        analogWrite(SpeedPin_A, 70);
        analogWrite(SpeedPin_B, 40);
      } 
      else { // Line was on the right, so turn left to find it
        vleft = -100; vright = 100;
        digitalWrite(Dir1Pin_A, HIGH); digitalWrite(Dir2Pin_A, LOW);   // Left motor backward (Swapped)
        digitalWrite(Dir1Pin_B, HIGH);  digitalWrite(Dir2Pin_B, LOW);  // Right motor forward (Swapped)
        analogWrite(SpeedPin_A, 40);
        analogWrite(SpeedPin_B, 70);
      }
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