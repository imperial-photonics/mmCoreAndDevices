// MAKE SURE to input correct LED_PIN, LED_COUNT, and pixel type flags
// before uploading to arduino board

// Modified based on strand_test.ino   
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif
 
// Which pin on the Arduino is connected to the NeoPixels?
// On a Trinket or Gemma we suggest changing this to 1:
#define LED_PIN 5

// How many NeoPixels are attached to the Arduino?
#define LED_COUNT 1

// Declare our NeoPixel strip object:
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_RGBW + NEO_KHZ800);
// Argument 1 = Number of pixels in NeoPixel strip
// Argument 2 = Arduino pin number (most are valid)
// Argument 3 = Pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)

// declare other useful params
unsigned long BaudRate = 9600;
char Terminator = '\n';
String Get_Flag = "?";
String Err_sign = "Err:";

// setup() function -- runs once at startup --------------------------------
void setup() {
  // These lines are specifically to support the Adafruit Trinket 5V 16 MHz.
  // Any other board, you can remove this part (but no harm leaving it):
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
  clock_prescale_set(clock_div_1);
#endif
  // END of Trinket-specific code.
  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();            // Turn OFF all pixels ASAP  

  // open serial for comm
  Serial.begin(BaudRate);
}

// loop() function -- runs repeatedly as long as board is on ---------------
void loop(){
   if (Serial.available() > 0) {
    // read the incoming string:
    String str = Serial.readStringUntil(Terminator);   
    str.trim(); // remove whitespaces at ends

    // Incoming string: {pxInd}{Cmd}{SetParam/GetFlag}
    // pxInd -- uint16_t
    // Cmd -- char: N(Num), P(Pin), L(Brightness), W(White), R(Red), G(Green), B(Blue)
    // SetParam -- uint8_t for L, W, R, G, B   
    bool is_get = str.endsWith(Get_Flag);
    if (is_get){ 
      str.remove(str.length()-Get_Flag.length()); // remove Get_Flag    
    }  
    uint16_t pixelInd; 
    char cmd;
    uint8_t setParam; 
    int parse_num = is_get? 2 : 3;
    int result = is_get? sscanf(str.c_str(), "%hu%c", &pixelInd, &cmd) 
      : sscanf(str.c_str(), "%hu%c%hhu", &pixelInd, &cmd, &setParam);   

    // downstream
    if (result!=parse_num){
      Serial.print(Err_sign);
      Serial.print("InvalidMessage: ");
      Serial.println(str);
    } else {  
      switch (cmd) {
        case 'N':
        case 'P':
          if(is_get){
            Serial.println((cmd=='N')? strip.numPixels() : strip.getPin()); 
          } else {
            Serial.print(Err_sign);
            Serial.print(cmd);
            Serial.println(" is get-only.");
          }
          break;
        case 'L':
          if (!is_get){ 
            strip.setBrightness(setParam);  
            strip.show();
          }  
          Serial.println(strip.getBrightness()); 
          break;
        case 'W':
        case 'R':
        case 'G':
        case 'B':
          if (pixelInd >= strip.numPixels()){ 
            // check pixelInd
            Serial.print(Err_sign);
            Serial.print(pixelInd);
            Serial.print("MUST be within [0,");
            Serial.print(strip.numPixels());
            Serial.println(").");
          } else {
            int ret = 0;

            // set if needed
            if(!is_get){
              // Serial.print("trying to set");
              // Serial.print(pixelInd);
              // Serial.print(cmd);
              // Serial.println(setParam);
              ret = setColor(pixelInd, cmd, setParam);
            }

            if(ret!=0){
              // if set failed, report error
              Serial.print(Err_sign);
              Serial.print("Failed to set LED ");
              Serial.print(pixelInd);
              Serial.print(" to ");
              Serial.print(cmd);
              Serial.print("=");
              Serial.print(setParam);
              Serial.println(".");
            }else{
              // get 
              uint8_t value;
              ret = getColor(pixelInd, cmd, value);
              if(ret!=0){
                // if get failed, report error
                Serial.print(Err_sign);
                Serial.print("Failed to read LED ");
                Serial.print(pixelInd);
                Serial.print("(");
                Serial.print(cmd);
                Serial.print("=");
                Serial.println("?).");
              }else{   
                Serial.println(value);
              }
            } 
          } 
          break;
        default:
          Serial.print(Err_sign);
          Serial.print("UNSupportedCommand: ");
          Serial.println(cmd);
          break;
      }
    } 
  }
}

// utility function 
int setColor(uint16_t pixelInd, char which, uint8_t color){
  int ret = 0;

  uint8_t shift = 0; 
  switch (which){
    case 'W':
      shift = 8 * 3;
      break;
    case 'R':
      shift = 8 * 2;
      break;
    case 'G':
      shift = 8 * 1;
      break;
    case 'B':
      shift = 8 * 0;
      break;
    default: 
      ret = -1;
      break;
  } 

  if (ret != 0){
    return ret;
  }  

  uint32_t factor =  0xFF;
  factor = factor << shift;
  uint32_t single_color = color;
  single_color = single_color << shift;
  uint32_t set_color = (strip.getPixelColor(pixelInd) & (~factor)) | (single_color & factor); 
  strip.setPixelColor(pixelInd, set_color);
  strip.show();

  return ret;
}


int getColor(uint16_t pixelInd, char which, uint8_t& color){
  int ret = 0;

  uint8_t shift = 0; 
  switch (which){
    case 'W':
      shift = 8 * 3;
      break;
    case 'R':
      shift = 8 * 2;
      break;
    case 'G':
      shift = 8 * 1;
      break;
    case 'B':
      shift = 8 * 0;
      break;
    default: 
      ret = -1;
      break;
  } 

  if (ret != 0){
    return ret;
  } 

  color = (strip.getPixelColor(pixelInd) >> shift) & 0xFF;

  return ret;
}