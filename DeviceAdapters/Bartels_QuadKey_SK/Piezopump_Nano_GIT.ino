/////////////////////////////////////////////////////////////////////////////////////////
//
//  Bartels QuadKey Piezo Pump driver Arduino firmware for Micro-Manager 2 integration
//  Designed to use the micro-manager UI for all heavy lifting, still using RS-232 though
//
//  For more information, contact:
//  Sunil Kumar - sunil.kumar@imperial.ac.uk
//
//  Overview:
//  ### !X - sets value of ### for X, FOR CURRENT PUMP ONLY
//  ?X - asks for current value of X, FOR CURRENT PUMP ONLY
//  P - which pump (Get all by binary adding e.g. "3" for "1 and 2")
//  V - voltage
//  T - runtime (ms)
//  R - ramp-peak setting - check the Bartels manual. Default = square wave
//  F - boost converter frequency - probably only useful if you have electrical noise issues?
//  S - TBD
//
//  For the stuff where it's easier to think of in binary, will assume 0 = 0, anything else = 1
//
//  \ - start/stop selected pump going
//  * - stop all pumps
//
//  PUMPS SHOULD BE STOPPED BEFORE CHANGING THE VOLTAGE TO PROLONG THE WORKING LIFE


#include <Wire.h> //I2C library
//See p.13 of manual for first thing to do - adding
//Need to standardise the returned line endings

// Addresses of registers to be used - values should all be 0x00 at init, apart from ID, which is 0xB2
#define DEVID 0x00// Device ID
//DevID bits 3,2,1,0 followed by Revision 3,2,1,0

#define Power_mode 0x01// Power mode
//Overtemp X X X X X X Enable
//Overtemp = 1 => thermal shutdown temp exceeded, switches off outputs. Power cycle to reset

#define Output_freq 0x02// Output frequency
//FO7 FO6 FO5 FO4 FO3 FO2 FO1 FO0

//FO 7 and 6:
//00 = 50-100
//01 = 100-200
//10 = 200-400
//11 = 400-800

#define Slope_shape 0x03// Slope / Shape
// X ENDAMP X X SHAPE1 SHAPE0 SL1 SL0
// ENDAMP = damping enable - 1 = enable active damping on LX node. 0 = disable
// If damping enabled, could reduce EMI

// SHAPE 1,0:
// 0X = Full wave
// 10 = Half wave
// 11 = Half wave

// SL 1,0:
// 00 = Sine
// 01 = Fast slope
// 10 = Faster slope
// 11 = Fastest slope (square wave)

#define Boost_conv_freq 0x04// Boost converter frequency
// SS1 SS0 X FSW4 FSW3 FSW2 FSW1 FSW0
// SS 1,0 - set spread-spectrum modulation freq as a fraction of boost converter freq
// 00 = disabled
// 01 = 1/8
// 10 = 1/32
// 11 = 1/128
// FSW 4 - set range of boost conv
// FSW 3,2,1,0 set switching freq of boost converter within range set by FSW4
// FSW4: 0 = 800-1600kHz, 1 = 400-800kHz

// FSW 3,2,1 from 0000 to 1111 (@FSW4 = 0): 800,853,907,960,1013,1067,1120,1173,1227,1280,1333,1387,1440,1493,1547,1600 - halve these for other range

//#define Audio = 0x05// Audio effects?! NOT SUPPORTED

#define Ramp_peak_1 0x06// Ramping time and peak output for Channel 1
#define Ramp_peak_2 0x07// Ramping time and peak output for Channel 2
#define Ramp_peak_3 0x08// Ramping time and peak output for Channel 3
#define Ramp_peak_4 0x09// Ramping time and peak output for Channel 4

// RT2, RT1, RT0?, VO4, VO3, VO2, VO1, VO0
// Ramp time 2,1,0:
// 000 = ASAP
// 001 = 62.5ms
// 010 = 125ms
// 011 = 250ms
// 100 = 500ms
// 101 = 750ms
// 110 = 1000ms
// 111 = 2000ms

//Voltage output: linear from 00000 = follow COM to 11111 = 150V relative to COM

//send OxAh to update all signal ramping time and peak voltage registers

//====================

boolean initialised = false;
int cont = 0;
int i = 0;
int which = 0;
int on_time = 2000;
int buttons_active = 0;
boolean readable = false;

int readout_byte = 0;
const int num_pump_buffers = 4;
byte Pump_V_read[num_pump_buffers];

unsigned long start_times[] = {0, 0, 0, 0};
//Need to update these at start from controller device
unsigned long durations[] = {990, 1090, 1122, 1224};
int button_states[] = {0, 0, 0, 0};
int pump_states[] = {0, 0, 0, 0};
int pump_ramp_volt[] = {0, 0, 0, 0};
int selected_pump = 1;
int current_voltage = 0;

int Pump_Address = 0b1111011; //For bartels controller if A0 and A1 tied high

byte rx_serial;
const int buflen = 20;
char serial_string[buflen];
int serial_str_pointer = 0;
String ser_value_str = "";
int ser_value_int = 0;
byte id_x = 0;

int pumps_max = 15;
int volts_max = 250;
int boost_freq_max = 1600;
int spread_max = 3;
int slope_max = 3;
int shape_max = 3;
int ramp_max = 7;
int duration_max = 10000;

int volts_to_set = 0b00011111;//Max volts
int ramp_to_set = 0b000; //Square wave
int duration_to_set = 200;


//WARNING! LOTS OF FUNCTIONS RETURNING (MAYBE UNCAUGHT) STRINGS SEEMS TO LEAD TO POSSIBLE HEAP FRAGMENTATION?

int byte_in = 0;
String inputstr = "";
String recvcmd = "";
String precmd = "";
String response_msg = "";
int setcmd = 999;
int getcmd = 999;
int precmdnum = 0;
boolean valid = false;

//Boost converter frequency
const char* F_set = "!F";
const char* F_get = "?F";

//Which pump
const char* P_set = "!P";
const char* P_get = "?P";

//Ramp-peak setting
const char* R_set = "!R";
const char* R_get = "?R";

//Run time [ms]
const char* T_set = "!T";
const char* T_get = "?T";

//Voltage
const char* V_set = "!V";
const char* V_get = "?V";

//Slope
const char* S_set = "!S";
const char* S_get = "?S";

//Continuous mode/Human-readable output on/Pump-priming (continuous?)
const char* Cont_toggle = "!C";
const char* Human_toggle = "!H";
const char* Prime_mode = "!A";

//GO!
const char* Run_mode = "/";

//Software version check
const char* REV_get = "?REV";

const char* set_cmds[] = {F_set, P_set, R_set, T_set, V_set, S_set, Cont_toggle, Human_toggle, Prime_mode, Run_mode};
const char* get_cmds[] = {F_get, P_get, R_get, T_get, V_get, S_get, REV_get};

void setup() {
  // 2-5 indicator LEDs, 6-8 input buttons:
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, INPUT_PULLUP);
  pinMode(7, INPUT_PULLUP);
  pinMode(8, INPUT_PULLUP);
  pinMode(9, INPUT_PULLUP);
  Serial.begin(9600, SERIAL_8N1);     // opens serial port, sets data rate to 57600 bps
  //Serial.write("\'Piezo pump controller\' - Sunil\r\n");
  //Serial.write("Go to https://wiki.imperial.ac.uk/display/Photonics/Bartels for help.\r\n");

  Wire.begin(); // begin I2C

  // Set fixed pump amplitude and freq for now
  Wire.beginTransmission(Pump_Address); //starts talking to pump
  Wire.write(Power_mode); //System register
  Wire.write(0b00000000); //writing 1 to EN bit (last one) = shutdown mode - no pumping, but settings saved
  Wire.endTransmission(); //stops talking to device

  Wire.beginTransmission(Pump_Address);
  Wire.write(Output_freq); //start at register 2
  Wire.write(0b00001111); //set at 50Hz (best for water?)
  Wire.endTransmission();

  Wire.beginTransmission(Pump_Address);
  Wire.write(Slope_shape); //start at register 3
  Wire.write(0b00001100); //BRUTALITY! (Square wave)
  Wire.endTransmission();

  Wire.beginTransmission(Pump_Address);
  Wire.write(Boost_conv_freq); //start at register 4
  Wire.write(0b110011111); //1/128 spread, 1600kHz boost conv freq
  Wire.endTransmission();

  for (int np=1;np<=num_pump_buffers;np++){
     set_P(np);
     set_V(250);
  }
  set_P(1);
}

void loop()
{
  if (initialised == false){
    stop_pumping();
    initialised = true;
  }
  //Strip the physical button handling???
  //Implement a pause feature? What would it be useful for, though?
  //readpins(); //check if a button has been pressed
  if (Serial.available() > 0)
  {
    rx_serial = Serial.read();//Anything on the serial port?
    serial_str_pointer += 1;
    if (rx_serial == 8)
    {
      //Backspace
      serial_str_pointer -= 1;
      // echo back:
      Serial.write(rx_serial);
    } else if (rx_serial == 42)
    { // 42 is * https://www.asciitable.com/
      stop_pumping();
      //Reset any partial commands
      serial_str_pointer = 0;
      inputstr = "";
      Respond("STOPPED ALL PUMPS ok\r\n","* ok\r\n",readable);
    } else if (rx_serial == 13 || serial_str_pointer >= (buflen - 2))
    {
      //Enter/CRLF means interpret now, as does buffer overflow
      serial_string[serial_str_pointer] = '\0';
      inputstr = String(serial_string);
      interpret_cmd(inputstr);
      inputstr = "";
      serial_str_pointer = 0;
      for (int k = 0; k < buflen; k++)
      {
        serial_string[k] = 0;
      }
    } else {
      //Non-special characters get appended to accumulating input string
      serial_string[serial_str_pointer - 1] = rx_serial;
      Serial.write(rx_serial);
    }
  }

//  if (buttons_active > 0)
//  {
//    for (int idx = 0; idx < num_pump_buffers; idx++)
//    {
//      //If off, turn on - REMEMBER - PULLUP HIGH, so 0 is active on button_states
//      //The "&&button_states" also acts as debouncing for shit buttons, assuming a long enough on-time
//      if (pump_states[idx] == 0 && button_states[idx] == 0)
//      {
//        set_pump_on(idx);
//      }
//    }
//  }

  for (int idx = 0; idx < num_pump_buffers; idx++)
  {
    //is end time in past and pump still on?
    if ((start_times[idx] + durations[idx]) < millis() && pump_states[idx] == 1)
    {
      //Then shut it down, unless e.g. in continuous mode...
      if (cont == 0)
      {
        set_pump_off(idx);
        Respond("Pumping finished - ok\r\n","",readable);
      }
    }
  }
}


//Button activity detection function
void readpins() {
  buttons_active = 0;
  for (int i = 0; i < num_pump_buffers; i++) {
    buttons_active = buttons_active + (1 - digitalRead(i + 6));
    button_states[i] = digitalRead(i + 6);
  }
}


void interpret_cmd(String command) {
  //Do comparison to command set for validity, then run appropriate function if ok
  valid = false;
  setcmd = 999;//Should do default nothing
  getcmd = 999;//Should do default nothing
  //Go through all 'set value' commands
  //Serial.print(sizeof(set_cmds)/sizeof(char*));
  //Stupid missing null terminators?
  for (int k = 0; k < (sizeof(set_cmds) / sizeof(char*)); k++) {
    recvcmd = command.substring(inputstr.length() - (strlen(set_cmds[k])));
    precmd = command.substring(0, (inputstr.length() - (strlen(set_cmds[k]) + 1)));
    if (recvcmd == String(set_cmds[k])) {
      setcmd = k;
      valid = true;
      precmdnum = precmd.toInt();
      //Serial.print(k);
      //Serial.print(precmdnum);
    }
  }

  //Go through all 'get value' commands if it wasn't a set command
  if (!valid) {
    for (int k = 0; k < (sizeof(get_cmds) / sizeof(char*)); k++) {
      recvcmd = command.substring((inputstr.length() - (strlen(get_cmds[k]))));
      if (String(get_cmds[k]).equals(recvcmd)) {
        valid = true;
        getcmd = k;
      }
    }
  }

  if (valid == true) {
    //Command is valid
    //Reminder: char* set_cmds[] = {F_set,P_set,R_set,T_set,V_set,S_set,Cont_toggle,Human_toggle,Prime_mode};
    //Now work out what command was to be executed...
    switch (setcmd) {
      case 0:
        set_F(precmdnum);
        break;
      case 1:
        set_P(precmdnum);
        break;
      case 2:
        set_R(precmdnum);
        break;
      case 3:
        set_T(precmdnum);
        break;
      case 4:
        stop_pumping(); //Manufacturer says it's bad to do voltage changes while running
        set_V(precmdnum);
        break;
      case 5:
        set_S(precmdnum);
        break;
      case 6:
        toggle_cont_mode();
        break;
      case 7:
        toggle_human_mode();
        break;
      case 8:
        set_prime_mode();
        break;
      case 9:
        set_pump_on(selected_pump);
        break;
      default:
        //Serial.print("SET COMMAND NOT RECOGNISED\r\n");
        break;
    }

    //Emulate extra space on set command returns if needed
    //if(setcmd < 999){
    //Serial.print(" ");
    //}

    //Reminder: char* get_cmds[] = {F_get,P_get,R_get,T_get,V_get,S_get,REV_get};
    //Get?
    switch (getcmd) {
      case 0:
        get_F();
        break;
      case 1:
        get_P();
        break;
      case 2:
        get_R();
        break;
      case 3:
        get_T();
        break;
      case 4:
        get_V();
        break;
      case 5:
        get_S();
        break;
      case 6:
        get_REV();
        break;
      default:
        //Serial.print("GET COMMAND NOT RECOGNISED\r\n");
        break;
    }
    Serial.print(" ok\r\n");
  } else {
    Serial.print(" ");
    Serial.print(inputstr);
    Serial.print(" was input - ok\r\n");
  }
}

void Respond(String hum_message, String PC_message, bool human_readable) {
  if (human_readable == true) {
    Serial.print("\n" + hum_message);
  } else {
    Serial.print("\n" + PC_message);
  }
  response_msg = "";
}

//==========================================
//  Command action handling
//==========================================
// Try to standardise on returning blah: ###
// where # is some related number
//==========================================
void oops() {
  Serial.print("NOT YET IMPLEMENTED!");
}
//==========================================
void get_F() {
  oops();
}
void set_F(int newF) {
  oops();
}
//==========================================
void get_S() {
  oops();
}
void set_S(int newS) {
  oops();
}
//==========================================
void get_P() {
  response_msg = ("Active pump is #: " + String(selected_pump+1));
  Respond(response_msg, String(selected_pump+1), readable);
}
void set_P(int newP) {
  if (newP > 0 && newP <= num_pump_buffers) {
    selected_pump = newP-1;
    response_msg = ("Active pump set to #: " + String(newP));
    Respond(response_msg,"", readable);
  }
}
//==========================================
void get_R() {
  response_msg = "fg";//"Pump "+String(selected_pump)+" ramp/peak setting is: "+"GET # HERE");
  Respond(response_msg, "", readable);
}
void set_R(int newR) {
  oops();
}
//==========================================
void get_T() {
  response_msg = "Pump " + String(selected_pump) + " runtime [ms] is: " + String(durations[selected_pump]);
  Respond(response_msg, "", readable);
}
void set_T(int newT) {
  durations[selected_pump] = newT;
  response_msg = "Pump " + String(selected_pump) + " runtime [ms] set to: " + String(durations[selected_pump]);
  Respond(response_msg, "", readable);
}
//==========================================
void get_V() {
  response_msg = "Pump " + String(selected_pump) + " voltage [V] is: " + String(((pump_ramp_volt[selected_pump] & 0b00011111) * volts_max) / 31);
  Respond(response_msg, "", readable);
}

void set_V(int newV) {
  set_pump_volt(selected_pump, newV);
  response_msg = "Pump " + String(selected_pump) + " voltage [V] set to: " + String(((pump_ramp_volt[selected_pump] & 0b00011111) * volts_max) / 31);
  Respond(response_msg, "", readable);
}
//==========================================
void get_W() {
  oops();
}
void set_W(int newW) {
  oops();
}
//===========================================
void get_REV() {
  Serial.print("0.0.1 - baseline working pump controller for MM2");
}
//==========================================
void toggle_cont_mode() {
  if (0 != cont) {
    cont = 0;
    stop_pumping();
  } else {
    cont = 1;
  }
  Respond("Cont mode @: "+String(response_msg), "", readable);
}
void toggle_human_mode() {
  readable = !readable;
}
void set_prime_mode() {
  oops();
}

//==========================================
void stop_pumping() {
  for (int i = 0; i < num_pump_buffers; i++) {
    set_pump_off(i);
  }
}

void set_pump_volt(int pump_num, int volts_wanted) {
  if (volts_wanted > volts_max) {
    volts_wanted = volts_max;
  }
  if (volts_wanted < 0) {
    volts_wanted = 0;
  }
  //Max setting is 11111 i.e. 31, so normalise to that...
  int volts_to_set = (31 * volts_wanted) / volts_max;
  pump_ramp_volt[pump_num] = ((ramp_to_set << 5) + volts_to_set);
  Wire.beginTransmission(Pump_Address); //starts talking to pump
  Wire.write(Ramp_peak_1 + (pump_num)); //Pump n register (Pump 0-3)
  Wire.write(pump_ramp_volt[pump_num]);
  Wire.endTransmission(); //stops talking to device
}


void set_pump_on(int idx3) {
  digitalWrite(idx3 + 2, HIGH); //LEDs on 2-5
  start_times[idx3] = millis();
  pump_states[idx3] = 1;

  Wire.beginTransmission(Pump_Address); //starts talking to pump
  Wire.write(Ramp_peak_1 + idx3); //Pump n register
  Wire.write(pump_ramp_volt[idx3]);
  Wire.endTransmission(); //stops talking to device

  Wire.beginTransmission(Pump_Address); //starts talking to pump
  Wire.write(0x0A); //Apply settings
  Wire.endTransmission(); //stops talking to device

  Wire.beginTransmission(Pump_Address); //starts talking to pump
  Wire.write(0x01); //System register
  Wire.write(0b00000001); //writing 1 to EN bit (last one) = shutdown mode - no pumping, but settings saved
  Wire.endTransmission(); //stops talking to device
}

void set_pump_off(int idx4) {
  digitalWrite(idx4 + 2, LOW);

  pump_states[idx4] = 0;

  Wire.beginTransmission(Pump_Address); //starts talking to pump
  Wire.write(Ramp_peak_1 + idx4); //Pump n register
  Wire.write(0b00000000); //ramptime = instant, Vpeak = 0
  Wire.endTransmission(); //stops talking to device

  Wire.beginTransmission(Pump_Address); //starts talking to pump
  Wire.write(0x0A); //Apply settings
  Wire.endTransmission(); //stops talking to device

  Wire.beginTransmission(Pump_Address); //starts talking to pump
  Wire.write(0x01); //System register
  Wire.write(0b00000001); //writing 1 to EN bit (last one) = shutdown mode - no pumping, but settings saved
  Wire.endTransmission(); //stops talking to device
}

int ReadRegister(int reg_number) {
  Wire.beginTransmission(Pump_Address);
  //HEX CONVERSION FOR REG_ADDR: HEX_ADDR = reg_number
  int HEX_ADDR = 0x00;
  Wire.write(HEX_ADDR); //start at register 0
  Wire.endTransmission();
  Wire.requestFrom(Pump_Address, 1); // requests 1 bytes of data - controller will auto-increment
  Serial.print(bitRead(readout_byte, 7));
  Serial.print(bitRead(readout_byte, 6));
  Serial.print(bitRead(readout_byte, 5));
  Serial.print(bitRead(readout_byte, 4));
  Serial.print(bitRead(readout_byte, 3));
  Serial.print(bitRead(readout_byte, 2));
  Serial.print(bitRead(readout_byte, 1));
  Serial.print(bitRead(readout_byte, 0));
  Serial.print("\r\n");
  return 0;
}
