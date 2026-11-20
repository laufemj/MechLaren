#include <Dynamixel2Arduino.h>

#if defined(ARDUINO_OpenRB)
  #define DXL_SERIAL Serial1
  #define DEBUG_SERIAL Serial
  #define BLT_SERIAL Serial2
  const int DXL_DIR_PIN = -1;
#endif

const int MAX_GOAL_PWM = 700;
const int POS_DEADZONE = 50;

constexpr uint8_t              GRIPPER_NUM =  1;      // Number of Grippers

///////////////////////////////* ONLY CHANGE HERE *///////////////////////////////
constexpr uint8_t             DXL_ID_COUNT = 6;       // Number of total joints
constexpr uint8_t GRIPPER_IDS[GRIPPER_NUM] = {14};     // Enter Gripper IDs
//////////////////////////////////////////////////////////////////////////////////
int                  DXL_IDS[DXL_ID_COUNT] = {7,8,10,12,13,14};
int         initialPositions[DXL_ID_COUNT] = {};
int32_t                 GRIP_INIT_POSITION =  0;

const float     DYNAMIXEL_PROTOCOL_VERSION = 2.0;
const uint16_t          GOAL_POSITION_ADDR = 116;      // Goal Position address
const uint16_t          GOAL_PWM_ADDR = 100;           // Goal PWM address
// const uint16_t       PROFILE_VELOCITY_ADDR = 112;      // Profile Velocity address


Dynamixel2Arduino dxl(DXL_SERIAL, DXL_DIR_PIN);

typedef struct sw_data{
  int32_t goal_position;
} __attribute__((packed)) sw_data_t;
sw_data_t sw_data[DXL_ID_COUNT];

DYNAMIXEL::InfoSyncWriteInst_t sw_infos;
DYNAMIXEL::XELInfoSyncWrite_t info_xels_sw[DXL_ID_COUNT];

// This namespace is required to use DYNAMIXEL Control table item name definitions
using namespace ControlTableItem;

bool BLT_CONNECTION = false;

void setup() {
  for (uint8_t i = 0; i < DXL_ID_COUNT; i++) { initialPositions[i] = 2048; }

  pinMode(LED_BUILTIN, OUTPUT);
  DEBUG_SERIAL.begin(57600);       // OpenRB Serial communication speed
  BLT_SERIAL.begin(57600);         // BT-210 communication speed
  dxl.begin(1000000);               // Dynamixel baudrate
  dxl.setPortProtocolVersion(DYNAMIXEL_PROTOCOL_VERSION);

  DEBUG_SERIAL.println("SETUP INIT");

  // Fill the members of the structure to syncWrite using the internal packet buffer
  sw_infos.packet.p_buf = nullptr;
  sw_infos.packet.is_completed = false;
  sw_infos.addr = GOAL_POSITION_ADDR;
  sw_infos.addr_length = 4;
  sw_infos.p_xels = info_xels_sw;
  sw_infos.xel_count = 0;
  
  for(uint8_t i = 0; i < DXL_ID_COUNT; i++){
    info_xels_sw[i].id = DXL_IDS[i];
    info_xels_sw[i].p_data = (uint8_t*)&sw_data[i].goal_position;
    sw_infos.xel_count++;
  }
  sw_infos.is_info_changed = true;  


  for (uint8_t i = 0; i < DXL_ID_COUNT; i++) {
    bool isGripperMotor = false;
    for (uint8_t j = 0; j < GRIPPER_NUM; j++) { // check if i reaches gripper IDs
      if (GRIPPER_IDS[j] == DXL_IDS[i]) {isGripperMotor = true; break;}
    }

    dxl.torqueOff(DXL_IDS[i]);

    if (isGripperMotor){
      // Set to current-based position control mode for Gripper Motor
      dxl.setOperatingMode(DXL_IDS[i], OP_PWM_CONTROL);

      // Current Control Parameters for Gripper Motor
      // dxl.setGoalCurrent(1, 700); // Max current for xc330-t181 : 910
      // dxl.setGoalPWM(DXL_IDS[i], 90.0, UNIT_PERCENT); // Set goal PWM as 90%

      int currentPosition = dxl.getPresentPosition(DXL_IDS[i]);
      GRIP_INIT_POSITION = currentPosition;
      initialPositions[i] = GRIP_INIT_POSITION;
      // int targetPosition = initialPositions[i];
      // int distance = abs(targetPosition - currentPosition);
      // dxl.writeControlTableItem(PROFILE_VELOCITY, DXL_IDS[i], 20);
    }

    else{
      int currentPosition = dxl.getPresentPosition(DXL_IDS[i]);
      int targetPosition = initialPositions[i];
      int distance = abs(targetPosition - currentPosition);
      dxl.writeControlTableItem(PROFILE_VELOCITY, DXL_IDS[i], 20);
    }

    dxl.torqueOn(DXL_IDS[i]);
    delay(10);
  }

  // Set the initial position for all motors.
  for (uint8_t i = 0; i < DXL_ID_COUNT-1; i++) {
    // Turn on torque for all motors.
    dxl.torqueOff(DXL_IDS[i]);
    dxl.torqueOn(DXL_IDS[i]);
    delay(50);

    dxl.writeControlTableItem(GOAL_POSITION, DXL_IDS[i], initialPositions[i]);

    // if return false : failure -> print error
    if (dxl.writeControlTableItem(GOAL_POSITION, DXL_IDS[i], initialPositions[i]) == 0){
      DEBUG_SERIAL.println("[ERROR] ERROR WHILE WRITING GOAL POSITION TO MOTOR ID");
    }

    DEBUG_SERIAL.print("Set motor ");
    DEBUG_SERIAL.print(DXL_IDS[i]);
    DEBUG_SERIAL.print(" initial position to: ");
    DEBUG_SERIAL.println(initialPositions[i]);
  }
  DEBUG_SERIAL.println("Setup completed.");
}

void loop(){
  if (BLT_SERIAL.available()>0) {
    String data = BLT_SERIAL.readStringUntil('\n'); // Read data from HC-05.

    sw_infos.is_info_changed = false;
    sw_infos.xel_count = 0;
    int startIndex = 0;

    for (uint8_t i = 0; i < DXL_ID_COUNT; i++) {
      bool isGripperMotor = false;
      for (uint8_t j = 0; j < GRIPPER_NUM; j++) { // check if i reaches gripper IDs
        if (GRIPPER_IDS[j] == DXL_IDS[i]) {isGripperMotor = true; break;}
      }

      // Parse the recieved data & divide it by commas
      int endIndex = data.indexOf(',', startIndex);
      if (endIndex == -1) {
        if(DXL_ID_COUNT-1 == i) {
          endIndex = data.length();
          sw_infos.is_info_changed = true;
        }
        else break;
      }

      sw_data[i].goal_position = data.substring(startIndex, endIndex).toInt();
      sw_infos.xel_count++;

      if (isGripperMotor){ // Set gripping motor sw infos added to present position (Current based = Extended position control mode)
        int32_t orig_grip_goal = sw_data[i].goal_position;
        int16_t goal_pwm = 0;

        if (orig_grip_goal > POS_DEADZONE) {
          goal_pwm = map(orig_grip_goal, POS_DEADZONE, 1000, 0, MAX_GOAL_PWM);
        } else if (orig_grip_goal < -POS_DEADZONE) {
          goal_pwm = map(orig_grip_goal, -1000, -POS_DEADZONE, -MAX_GOAL_PWM, 0);
        }

        goal_pwm = constrain(goal_pwm, -MAX_GOAL_PWM, MAX_GOAL_PWM);

        dxl.writeControlTableItem(GOAL_PWM, DXL_IDS[i], goal_pwm);
        
        // (디버그)
        // DEBUG_SERIAL.print("Gripper "); DEBUG_SERIAL.print(DXL_IDS[i]);
        // DEBUG_SERIAL.print(" PWM: "); DEBUG_SERIAL.println(goal_pwm);

        // sw_data[i].goal_position = initialPositions[i] + orig_grip_goal;
      }
      else { // Check out of range position goals to other motors (Position control mode)
        if((sw_data[i].goal_position < 0) || (4095 < sw_data[i].goal_position)) {
          DEBUG_SERIAL.print("WRONG DATA INCLUDED... changing sw info changed to FALSE...\n");
          sw_infos.is_info_changed = false;
          break;
        }

        sw_data[sw_infos.xel_count].goal_position = sw_data[i].goal_position;
        info_xels_sw[sw_infos.xel_count].id = DXL_IDS[i];
        info_xels_sw[sw_infos.xel_count].p_data = (uint8_t*)&sw_data[sw_infos.xel_count].goal_position;
        sw_infos.xel_count++; // SyncWrite할 모터 개수 증가
      }
      startIndex = endIndex + 1;
    }
    // Set the goal position for all motors.
    if(sw_infos.is_info_changed) {
      for (uint8_t i = 0; i < sw_infos.xel_count; i++) {
        uint8_t motor_id = sw_infos.p_xels[i].id;
        int32_t goal_position = sw_data[i].goal_position;
        DEBUG_SERIAL.print("Motor ID: ");
        DEBUG_SERIAL.print(motor_id);
        DEBUG_SERIAL.print(" | Goal Position: ");
        DEBUG_SERIAL.println(goal_position);
      }
      dxl.syncWrite(&sw_infos);
    }
  }
}
