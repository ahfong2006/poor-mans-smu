/****************************************************************
 *  
 *  Initial GUI prototype for
 *  P O O R  M A N ' s  S M U
 *  
 *  by Helge Langehaug (2018, 2019, 2020)
 * 
 *****************************************************************/

//Uncomment the line below if you don't have the analog hardware, only processor and screen...
//The analog circuitry will then be simulated.
//Don't exect everything to work... It's not tested very often...
//#define USE_SIMULATOR


#include <SPI.h>
#include "GD2.h"
#include "colors.h"
#include "volt_display.h"
#include "current_display.h"
#include "Stats.h"
#include "Calibration.h"
#include "Filters.h"
#include "digit_util.h"
#include "tags.h"
#include "operations.h"
#include "Arduino.h"
#include "Wire.h"
#include "Mainmenu.h"
#include "SMU_HAL_dummy.h"
#include "SMU_HAL_717x.h"
#include "dial.h"
#include "FunctionPulse.h"
#include "FunctionSweep.h"
#include "Fan.h"
#include "RamClass.h"
#include "RotaryEncoder.h"
#include "PushButtons.h"
#include "TrendGraph.h"
#include "Logger.h"

#include "Settings.h"
#include "SimpleStats.h"

#include "utils.h"
#include "analogGauge.h"

//#define _SOURCE_AND_SINK 111

#define _PRINT_ERROR_VOLTAGE_SOURCE_SETTING 0
#define _PRINT_ERROR_CURRENT_SOURCE_SETTING 1
#define _PRINT_ERROR_SERIAL_COMMAND 2
#define _PRINT_ERROR_NO_CALIBRATION_DATA_FOUND 3
#define _PRINT_ERROR_FACTORY_CALIBRATION_RESTORE_FAILED_CATASTROPHICALLY

#define GEST_NONE 0
#define GEST_MOVE_LEFT 1
#define GEST_MOVE_RIGHT 2
#define GEST_MOVE_DOWN 3
#define LOWER_WIDGET_Y_POS 250

SimpleStatsClass SIMPLE_STATS;
LoggerClass LOGGER;

PushbuttonsClass PUSHBUTTONS;
PushbuttonsClass PUSHBUTTON_ENC;


bool anyDialogOpen();
void openMainMenu();
void renderAnalogGauge(int x, int y, int size, float degrees, float value, const char *title);
void showLoadResistance(int x, int y);
void loopMain();
void loopDigitize();
void handleSampling();
void closeMainMenuCallback(FUNCTION_TYPE functionType_);
void showFanSpeed(int x, int y);
void fltCommitCurrentSourceAutoRange(float mv, bool autoRange);
void closeSourceDCCallback(int set_or_limit, bool cancel);


#ifndef USE_SIMULATOR
ADCClass SMU[1] = {
  ADCClass()
};
#else
SMU_HAL_dummy SMU[1] = {
  SMU_HAL_dummy()
};
#endif

int scroll = 0;
int scrollDir = 0;


bool autoNullStarted = false;

int timeAtStartup;
bool nullCalibrationDone0 = false;
bool nullCalibrationDone1 = false;
bool nullCalibrationDone2 = false;

int noOfWidgets = 7;
int activeWidget = 0;

CURRENT_RANGE current_range = AMP1; // TODO: get rid of global

bool voltage_range = true;


uint32_t timeSinceLastChange = 0;  // TODO: get rid of global



OPERATION_TYPE operationType = SOURCE_VOLTAGE;


FUNCTION_TYPE functionType = SOURCE_DC_VOLTAGE;


FUNCTION_TYPE prevFuncType = SOURCE_DC_VOLTAGE;

void printError(int16_t  errorNum)
{
  Serial.print(F("Error("));
  Serial.print(errorNum);
  Serial.println(")");
}

OPERATION_TYPE getOperationType() {
  OPERATION_TYPE ot;
  //TODO:  This is messy! Based on older concept where function and operation were different stuff... this has slightly changed....

  if (functionType == SOURCE_DC_CURRENT) {
    ot = SOURCE_CURRENT;
  } else {
    ot = SOURCE_VOLTAGE;
  }

  // Only update the GPIO bit when operation has changed!  Originally put this in the callback from menu selection, but it failed... not sure why. Investigate.
  if (functionType != prevFuncType) {
    SMU[0].setGPIO(0, ot != SOURCE_VOLTAGE);
Serial.println("Changing mode !");

  }
    prevFuncType = functionType;

  return ot;

}
IntervalTimer myTimer;

#define SAMPLING_BY_INTERRUPT




void rotaryChangedVoltCurrentFn(float changeVal) {

   if (operationType == SOURCE_VOLTAGE) {
       Serial.print("rotary changeval:");
       Serial.println(changeVal);
     if(SOURCE_DIAL.isDialogOpen()) {
       float mv = SOURCE_DIAL.getMv();
       int64_t change_uV =  changeVal*1000;
       int64_t new_uV = mv*1000 + change_uV;
       SOURCE_DIAL.setMv(new_uV /1000.0);
     } else {
       Serial.print("from smu setvalue mv=");
       DIGIT_UTIL.print_uint64_t(SMU[0].getSetValue_micro());
       Serial.println();

       int64_t change_uV =  changeVal*1000;
       Serial.print("change value in uV=");
       DIGIT_UTIL.print_uint64_t(change_uV);
       Serial.println();

       int64_t new_uV = SMU[0].getSetValue_micro() + change_uV;
       Serial.print("new uV=");
       DIGIT_UTIL.print_uint64_t(new_uV);
       Serial.println();

/*
       float newVoltage_mV = new_uV / 1000.0;
       Serial.print("new mv=");
       Serial.println(newVoltage_mV,5);
*/
       if (SMU[0].fltSetCommitVoltageSource(new_uV, true)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
     }

   } else {

     if (current_range == MILLIAMP10) {
       changeVal = changeVal / 100.0;
     }

 if(SOURCE_DIAL.isDialogOpen()) {
       float mv = SOURCE_DIAL.getMv();
       int64_t change_uV =  changeVal*1000;
       int64_t new_uV = mv*1000 + change_uV;
       SOURCE_DIAL.setMv(new_uV /1000.0);
     } else {
              Serial.print("from smu setvalue mv=");
       DIGIT_UTIL.print_uint64_t(SMU[0].getSetValue_micro());
       Serial.println();

       int64_t change_uV =  changeVal*1000;
        Serial.print("change value in uV=");
        DIGIT_UTIL.print_uint64_t(change_uV);
        Serial.println();

       int64_t new_uV = SMU[0].getSetValue_micro() + change_uV;
       Serial.print("new uV=");
       DIGIT_UTIL.print_uint64_t(new_uV);
       Serial.println();


       fltCommitCurrentSourceAutoRange(new_uV, false);
     }
   }

      
}

bool showSettings = false;

void pushButtonEncInterrupt(int key, bool quickPress, bool holdAfterLongPress, bool releaseAfterLongPress) {
Serial.print("XXXKey pressed:");
  Serial.print(key);
  Serial.print(" ");
  Serial.println(quickPress==true?"QUICK" : "");
  Serial.println(holdAfterLongPress==true?"HOLDING" : "");
  Serial.println(releaseAfterLongPress==true?"RELEASED AFTER HOLDING" : "");
}
void pushButtonInterrupt(int key, bool quickPress, bool holdAfterLongPress, bool releaseAfterLongPress) {
  Serial.print("Key pressed:");
  Serial.print(key);
  Serial.print(" ");
  Serial.println(quickPress==true?"QUICK" : "");
  Serial.println(holdAfterLongPress==true?"HOLDING" : "");
  Serial.println(releaseAfterLongPress==true?"RELEASED AFTER HOLDING" : "");

  if (quickPress && key ==1 && MAINMENU.active == false) {
    openMainMenu();
  }

  else if (quickPress && key ==4 && showSettings == false) {
    showSettings = true;
  } else if (quickPress && key ==4 && showSettings == true) {
    showSettings = false;
  }

}

bool reduceDetails() {
  return scrollDir != 0 || MAINMENU.active == true or anyDialogOpen();
}

void  disable_ADC_DAC_SPI_units() {
  SMU[0].disable_ADC_DAC_SPI_units();
}






void setup()
{
//	while (!Serial) ; // wait

  LOGGER.init();

  SIMPLE_STATS.init();
  
   disable_ADC_DAC_SPI_units();
   delay(50);
   //TODO: Organise pin numbers into definitions ?
   pinMode(6,OUTPUT); // LCD powerdown pin?
   //digitalWrite(6, LOW);
   pinMode(5,OUTPUT); // RAM

    pinMode(7,OUTPUT);
    pinMode(8,OUTPUT);
    pinMode(9,OUTPUT);
    pinMode(10,OUTPUT);

    pinMode(4,OUTPUT); // current range io pin for switching on/off 100ohm shunt
    digitalWrite(4, LOW);


    // Changed in later revisions to be inpur for limits
    // TODO: Fix this mess
    pinMode(3,INPUT); // (NO!) current range selector
    pinMode(2,INPUT); // (NO!) Fan speed feedback


    PUSHBUTTONS.init(3, 1000); // bushbuttons based on analog pin 3, and holding 1000ms
    PUSHBUTTONS.setCallback(pushButtonInterrupt);
    PUSHBUTTON_ENC.init(16, 1000); 
    PUSHBUTTON_ENC.setCallback(pushButtonEncInterrupt);
    
    FAN.init();
    
    //pinMode(11,OUTPUT);
    //pinMode(12,INPUT);
    //pinMode(13,OUTPUT);

    Serial.println("Initializing graphics controller FT81x...");
    Serial.flush();

    // bootup FT8xx
    // Drive the PD_N pin high
    // brief reset on the LCD clear pin
    //TODO: Does not work yet.... why ?  Need to reconnect USB (power cycle)...
    for (int i=0;i<2;i++) {
    digitalWrite(6, LOW);
    delay(200);
    digitalWrite(6, HIGH);
    delay(200);
    }


   GD.begin(0);
   delay(500);

   Serial.println("Initializing graphics...");
   Serial.flush();
   GD.cmd_romfont(1, 34); // put FT81x font 34 in slot 1
   GD.Clear();
   GD.ColorRGB(0xaaaaff);
   GD.ColorA(200);
   GD.cmd_text(250, 200 ,   31, 0, "Poor man's SMU");
   GD.ColorRGB(0xaaaaaa);
   GD.cmd_text(250, 240 ,   28, 0, "Designed    by    Helge Langehaug");
   GD.cmd_text(250, 270 ,   28, 1, "V0.160");

   GD.swap();
   delay(501);

   GD.__end();
   Serial.println("Graphics initialized.");
   Serial.flush();

   disable_ADC_DAC_SPI_units();
   delay(100);
   Serial.println("Initializing SMU...");
   SMU[0].init();
   SETTINGS.init();

   SMU[0].setSamplingRate(20);
   operationType = getOperationType();

   V_CALIBRATION.init(SOURCE_VOLTAGE);
   C_CALIBRATION.init(SOURCE_CURRENT);
   Serial.println("SMU initialized");
   Serial.flush();

   if (operationType == SOURCE_VOLTAGE) {
     SMU[0].fltSetCommitVoltageSource(SETTINGS.setMilliVoltage*1000, true);
     Serial.println("Source voltage");
     SMU[0].fltSetCommitCurrentLimit(SETTINGS.setCurrentLimit*1000, _SOURCE_AND_SINK); 
   } 
   Serial.print("Default source voltage ");
   Serial.println(SETTINGS.setMilliVoltage);
   Serial.println(" mV");
   Serial.print("Default current limit ");
   Serial.println(SETTINGS.setCurrentLimit);
   Serial.println(" mA");
   Serial.flush();

   V_STATS.init(DigitUtilClass::typeVoltage);
   C_STATS.init(DigitUtilClass::typeCurrent);
   
   V_FILTERS.init(1234);
   C_FILTERS.init(5678);
   
   SOURCE_DIAL.init();
   LIMIT_DIAL.init();

   FUNCTION_PULSE.init(/*SMU[0]*/);
   FUNCTION_SWEEP.init(/*SMU[0]*/);

   RAM.init();
  
   timeAtStartup = millis();

  // SPI.usingInterrupt(2);
  // pinMode(2,INPUT);
  // attachInterrupt(2, handleSampling, CHANGE);

#ifdef SAMPLING_BY_INTERRUPT
  myTimer.begin(handleSampling, 20); // in microseconds.Lower that 20 hangs the display... why ?
  SPI.usingInterrupt(myTimer);
#endif

  ROTARY_ENCODER.init(rotaryChangedVoltCurrentFn);
 
  //TC74 
  Wire.begin();

    
} 




void showStatusIndicator(int x,int y,const char* text, bool enable, bool warn) {
  GD.Begin(RECTS);
  GD.ColorRGB(warn ? 0xFF0000 : COLOR_VOLT);
  GD.ColorA(enable ? 150 : 25);
  
  GD.LineWidth(150);
  GD.Vertex2ii(x, 15 + y);
  GD.Vertex2ii(x + 55, y + 24);
  GD.ColorRGB(0xffffff);
  GD.ColorA(enable ? 200 : 25);
  GD.cmd_text(x+ 2, y + 10, 27, 0, text);
  GD.ColorA(255);
}

void sourcePulsePanel(int x, int y) {
  FUNCTION_PULSE.render(x,y);
}

void sourceSweepPanel(int x, int y) {
  FUNCTION_SWEEP.render(x,y);
}

void sourceCurrentPanel(int x, int y) {

  // heading
  GD.ColorRGB(COLOR_CURRENT);
  GD.ColorA(120);
  GD.cmd_text(x+20, y + 2 ,   29, 0, "SOURCE CURRENT");
  GD.cmd_text(x+20 + 1, y + 2 + 1 ,   29, 0, "SOURCE CURRENT");
  
  // primary
  bool shownA = current_range == MILLIAMP10;
  shownA = false; // Override. Dont show nA
  CURRENT_DISPLAY.renderMeasured(x /*+ 17*/,y + 26, C_FILTERS.mean, false, shownA, current_range);

  // secondary
  GD.ColorRGB(COLOR_CURRENT);
  GD.ColorA(180); // a bit lighter
  DIGIT_UTIL.renderValue(x + 290,  y-4 , C_STATS.rawValue, 4, DigitUtilClass::typeCurrent); 

  GD.ColorA(255);
  CURRENT_DISPLAY.renderSet(x + 120, y + 131, SMU[0].getSetValue_micro());

  GD.ColorRGB(0,0,0);
  GD.cmd_fgcolor(0xaaaa90);  
  
  GD.Tag(BUTTON_SOURCE_SET);
  GD.cmd_button(x + 20,y + 132,95,50,29,OPT_NOTEAR,"SET");
  
 
  GD.Tag(BUTTON_CUR_AUTO);
  GD.cmd_button(x+350,y+132,95,50,29,0,current_range==AMP1 ? "1A" : "10mA");
  GD.Tag(0); // Note: Prevents button in some cases to react also when touching other places in UI. Why ?
  
}




void sourceVoltagePanel(int x, int y) {

  // heading
  GD.ColorRGB(COLOR_VOLT);
  GD.ColorA(120);
  GD.cmd_text(x+20, y + 2 ,   29, 0, "SOURCE VOLTAGE");
  GD.cmd_text(x+20 + 1, y + 2 + 1 ,   29, 0, "SOURCE VOLTAGE");
  
  // primary
  VOLT_DISPLAY.renderMeasured(x /*+ 17*/,y + 26, V_FILTERS.mean, false);

  // secondary
  GD.ColorRGB(COLOR_VOLT);
  GD.ColorA(180); // a bit lighter
  DIGIT_UTIL.renderValue(x + 300,  y-4 , V_STATS.rawValue, 4, DigitUtilClass::typeVoltage); 

  GD.ColorA(255);
  VOLT_DISPLAY.renderSet(x + 120, y + 131, SMU[0].getSetValue_micro());


  float uVstep = 1000000.0/ powf(2.0,18.0); // for 1V 18bit DAC

  if (!reduceDetails()) {
    y=y+5;
    if (abs(SMU[0].DAC_RANGE_LOW)== 5.0 && abs(SMU[0].DAC_RANGE_HIGH) == 5.0) {
      uVstep = uVstep *20.0;
        GD.cmd_text(x+120,y+170, 27, 0, "+/-10V [step:");
        DIGIT_UTIL.renderValue(x + 205,  y+173 , uVstep, 0, -1 /*DigitUtilClass::typeVoltage*/); 
        GD.cmd_text(x+270,y+170, 27, 0, "uV]");
    }
    else if (abs(SMU[0].DAC_RANGE_LOW) == 2.5 && abs(SMU[0].DAC_RANGE_HIGH) == 2.5) {
        uVstep = uVstep *10.0;
        GD.cmd_text(x+120,y+170, 27, 0, "+/- 5V  [step:");
        DIGIT_UTIL.renderValue(x + 205,  y+173 , uVstep, 0, -1/*DigitUtilClass::typeVoltage*/);
        GD.cmd_text(x+270,y+170, 27, 0, "uV]");
    } else if (abs(SMU[0].DAC_RANGE_LOW) == 10.0&& abs(SMU[0].DAC_RANGE_HIGH) == 10.0){
        uVstep = uVstep *40.0;
        GD.cmd_text(x+120,y+170, 27, 0, "+/- 20V [step:");
        DIGIT_UTIL.renderValue(x + 205 + 15,  y+173 , uVstep, 0, -1/*DigitUtilClass::typeVoltage*/); 
        GD.cmd_text(x+270+ 15,y+170, 27, 0, "uV]");
    } else {
         GD.cmd_text(x+120,y+170, 27, 0, " ?");
    }
    y=y-5;
  }
  GD.ColorRGB(0,0,0);
  GD.cmd_fgcolor(0xaaaa90);  

  
/*
  GD.ColorRGB(COLOR_VOLT);
  GD.cmd_text(x+0,y+132+10+5, 28, 0, "SOURCE");
  GD.ColorRGB(0);
  GD.Tag(BUTTON_SOURCE_SET);
  GD.cmd_button(x + 20+100-30-5,y + 132,95+100,50,29,OPT_NOTEAR,""); // SET
    //VOLT_DISPLAY.renderSet(x + 120-100, y + 131, SMU[0].getSetValuemV());
  DIGIT_UTIL.renderValue(x + 120-50+5,  y+131+10-5+3 , SMU[0].getSetValuemV(), 3, DigitUtilClass::typeVoltage); 
  //GD.cmd_text(x+ + 120-50+100+50+30+5,  y+131+10-5+3, 30, 0, "V");
*/

  GD.Tag(BUTTON_SOURCE_SET);
  GD.cmd_button(x + 20, y + 132, 95, 50, 29, OPT_NOTEAR, "SET");

  GD.Tag(BUTTON_VOLT_AUTO);
  GD.cmd_button(x + 350 + 20, y + 132, 95 + 0, 50, 29, 0, "AUTO");

  GD.Tag(0);
/*  TODO: Fix. Preliminary removed because it causes exception often when clicking buttons...
*/
  if (!reduceDetails()) {
    // Add some minor statistics on screen. Nice while looking at long time stability...
    GD.ColorRGB(0xaaaaaa);

    x=x+ 115;
    y=y-5;
    GD.cmd_text(x + 350+20+150-10,y + 132+2, 27, 0, "Min");
    DIGIT_UTIL.renderValue(x + 350+20+170,y + 132, SIMPLE_STATS.minimum, 1, DigitUtilClass::typeVoltage); 

    GD.cmd_text(x + 350+20+150-10,y + 132+ 20+2, 27, 0, "Max");
    DIGIT_UTIL.renderValue(x + 350+20+170,y + 132+ 20, SIMPLE_STATS.maximum, 1, DigitUtilClass::typeVoltage); 

    GD.cmd_text(x + 350+20+150-10,y + 132+ 40+2, 27, 0, "Samples");
    //DIGIT_UTIL.renderValue(x + 350+40+170+20,y + 132+ 40, SIMPLE_STATS.samples / 1000.0, 1, -1); 
    GD.cmd_number(x + 350+40+170+35,y + 132+ 40, 28, 6, SIMPLE_STATS.samples);
  }



}

void renderStatusIndicators(int x, int y) {
  x=x+10;
  showStatusIndicator(x+630, y+5, "FILTER", V_FILTERS.filterSize>1, false);
  if (operationType == SOURCE_VOLTAGE) {
    showStatusIndicator(x+710, y+5, "NULLv", V_CALIBRATION.nullValueIsSet(current_range), false);
    showStatusIndicator(x+710, y+45, "RELv", V_CALIBRATION.relativeValueIsSet(current_range), false);

  } else {
    showStatusIndicator(x+710, y+5, "NULLc", C_CALIBRATION.nullValueIsSet(current_range), false);
    showStatusIndicator(x+710, y+45, "RELc", C_CALIBRATION.relativeValueIsSet(current_range), false);

  }
  showStatusIndicator(x+630, y+45, "50Hz", false, false);
  showStatusIndicator(x+630, y+85, "COMP", SMU[0].hasCompliance(), true);
    if (operationType == SOURCE_VOLTAGE) {
      showStatusIndicator(x+710, y+85, "UNCAL", !V_CALIBRATION.useCalibratedValues, true);
    } else {
      showStatusIndicator(x+710, y+85, "UNCALc", !C_CALIBRATION.useCalibratedValues, true);
    }
}


bool anyDialogOpen() {
  // make sure buttons below the dialog do not reach if finger is not removed from screen
  // when dialog disappears. Use a timer for now...
  return SOURCE_DIAL.isDialogOpen() or LIMIT_DIAL.isDialogOpen();
}

void closeAllOpenDialogs() {
  if ( SOURCE_DIAL.isDialogOpen() ) {
    SOURCE_DIAL.close();
  }
  if (LIMIT_DIAL.isDialogOpen() ) {
    LIMIT_DIAL.close();
  }
}

int maxFilterSliderValue = 50;
int maxSamplesSliderValue = 100;
int sliderTags[2] = {TAG_FILTER_SLIDER, TAG_FILTER_SLIDER_B};
int trackingOngoing = 0;

void handleSliders(int x, int y) { 
  
  y=y+40;

  GD.ColorRGB(trackingOngoing==TAG_FILTER_SLIDER?0x00ff00:0xaaaaaa);
  GD.Tag(TAG_FILTER_SLIDER);
  GD.cmd_slider(500+x, y+30, 280,15, OPT_FLAT, V_FILTERS.filterSize * (65535/maxFilterSliderValue), 65535);
  GD.cmd_track(500+x, y+30, 280, 20, TAG_FILTER_SLIDER);
  GD.Tag(0);
  
  GD.Tag(TAG_FILTER_SLIDER_B);
  GD.ColorRGB(trackingOngoing==TAG_FILTER_SLIDER_B?0x00ff00:0xaaaaaa);
  GD.cmd_slider(500+x, y+90, 280,15, OPT_FLAT, V_STATS.getNrOfSamplesBeforeStore()* (65535/maxSamplesSliderValue), 65535);
  GD.cmd_track(500+x, y+90, 280, 20, TAG_FILTER_SLIDER_B);
  GD.Tag(0);  

  GD.ColorRGB(trackingOngoing==TAG_FILTER_SLIDER?0x00ff00:0xaaaaaa);
  GD.cmd_text(500+x,y, 27, 0, "Filter size:");
  GD.cmd_number(580+x,y, 27, 0, V_FILTERS.filterSize);

  GD.ColorRGB(trackingOngoing==TAG_FILTER_SLIDER_B?0x00ff00:0xaaaaaa);
  GD.cmd_text(500+x,y+60, 27, 0, "Samples size:");
  GD.cmd_number(605+x,y+60, 27, 0, V_STATS.getNrOfSamplesBeforeStore());

  if (!anyDialogOpen()) {
    GD.Tag(BUTTON_CLEAR_BUFFER);
    GD.ColorRGB(DIGIT_UTIL.indicateColor(0x000000, 0x00ff00, 50, BUTTON_CLEAR_BUFFER));
    GD.cmd_button(x+500,y+130,95,50,29,0,"CLEAR");
  }
  
  if (!anyDialogOpen()) {
    if (V_CALIBRATION.relativeValueIsSet(current_range)) {
      GD.ColorRGB(0x00ff00);
    } else {
      GD.ColorRGB(0x000000);
    }
  } else {
    //GD.ColorA(100);
  }
  GD.Tag(BUTTON_REL);
  GD.cmd_button(x+700,y+130,95,50,29,0,"REL");

  if (!anyDialogOpen()) {
    if (V_CALIBRATION.useCalibratedValues == false) {
      GD.ColorRGB(0x00ff00);
    } else {
      GD.ColorRGB(0x000000);
    }
  }
  
  GD.Tag(BUTTON_UNCAL);
  GD.cmd_button(x+600,y+130,95,50,29,0,"UNCAL");
  GD.Tag(0); // hack to avoid UNCAL button to be called when you press SET on voltage. (when the experimental widget is shown)... 
             // Dont fully understand why this is needed...
  
  GD.ColorA(255);

  GD.get_inputs();
  switch (GD.inputs.track_tag & 0xff) {
    case TAG_FILTER_SLIDER: {
      Serial.print("Set filter value:");
      int slider_val = maxFilterSliderValue * GD.inputs.track_val / 65535.0;
      Serial.println(slider_val);
      V_FILTERS.setFilterSize(int(slider_val));
      // currently set same as for voltage
      C_FILTERS.setFilterSize(int(slider_val));

      break;
    }
    case TAG_FILTER_SLIDER_B:{
      Serial.print("Set samples value:");
      int slider_val = maxSamplesSliderValue * GD.inputs.track_val / 65535.0;
      Serial.println(slider_val);
      V_STATS.setNrOfSamplesBeforeStore(int(slider_val));
      // for now, just use same is current as for voltage
      C_STATS.setNrOfSamplesBeforeStore(int(slider_val));
      break;
    }
    default:
      break;
  }
}


void renderExperimental(int x, int y, float valM, float setM, bool cur, bool lessDetails) {

  handleSliders(x,y);

  y=y+65;
  x=x+100;
  
  if (cur) {
     //Special handling: set current must currently be positive even if sink/negative.
     //                  This give error when comparing negative measured and positive set.
     //                  Use absolute values to give "correct" comparision...
     setM = abs(setM);
     valM = abs(valM);
  }

  
  float deviationInPercent = 100.0 * ((setM - valM) / setM);
  if (setM == 0.0) {
    deviationInPercent = 100;
  }
  float degrees = -deviationInPercent * 700.0;
  if (!lessDetails) {
    ANALOG_GAUGE.renderAnalogGauge(x+90,y,240, degrees, deviationInPercent, "Deviation from SET");
  }


  GD.ColorRGB(0x000000);

  // Visible number on button is half, because the sampling is effectlively the halv because
  // current and voltage is not sampled simultanously in the AD converter !

  int sr = SMU[0].getSamplingRate();

  GD.ColorRGB(sr==5?0x00ff00:0x0000);
  GD.Tag(BUTTON_SAMPLE_RATE_5);
  GD.cmd_button(x-50,y-20,95,40,29,0,"2.5Hz");
  
  GD.ColorRGB(sr==20?0x00ff00:0x0000);
  GD.Tag(BUTTON_SAMPLE_RATE_20);
  GD.cmd_button(x-50,y+40-15,95,40,29,0,"10Hz");
  
  GD.ColorRGB(sr==50?0x00ff00:0x0000);
  GD.Tag(BUTTON_SAMPLE_RATE_50);
  GD.cmd_button(x-50,y+80-15+5,95,40,29,0,"25Hz");

  GD.ColorRGB(sr==100?0x00ff00:0x0000);
  GD.Tag(BUTTON_SAMPLE_RATE_100);
  GD.cmd_button(x-50,y+120-15+10,95,40,29,0,"50Hz");
  GD.Tag(0);
  
}

void renderVoltageGraph(int x,int y, bool scrolling) {
  V_STATS.renderTrend(x, y, scrolling);
}
void renderCurrentGraph(int x,int y, bool scrolling) {
  C_STATS.renderTrend(x, y, scrolling);
}

void renderHistogram(int x,int y, bool scrolling) {
  V_STATS.renderHistogram(x,y,scrolling);
}

void renderBar(int x, int y, float rawValue, uint64_t setValue_u) {
  float setValue = setValue_u/1000.0; // logic below based on milli
  if (reduceDetails()) {
    return;
  }
  GD.Begin(RECTS);
  GD.LineWidth(10);
  for (int i=0;i<40;i++) {
    int percent = 100 * (abs(rawValue) / abs(setValue));
    if (percent > i*2.5) {
       GD.ColorA(255);
    } else {
       GD.ColorA(60);
    }
    if (i>34) {
       GD.ColorRGB(0xff0000);
    } else if (i>29) {
       GD.ColorRGB(COLOR_ORANGE);
    }else {
       GD.ColorRGB(0x00cc00);
    }
    int position_x1 = x+20 + i*14;
    int position_x2 = position_x1 + 11;
    if (position_x1 > 0 && position_x2 < 800) {
      GD.Vertex2ii(position_x1 ,y);
      GD.Vertex2ii(x+20 + (i+1)*14 - 3, y+9);
    }
    
  }
}

void measureVoltagePanel(int x, int y, boolean compliance) {
  if (x >= 800) {
    return;
  }
  y=y+28;

  VOLT_DISPLAY.renderMeasured(x /*+ 17*/, y, V_FILTERS.mean, compliance);
  VOLT_DISPLAY.renderSet(x+120, y+105, SMU[0].getLimitValue_micro());

  y=y+105;
  
  GD.ColorRGB(0,0,0);
  GD.cmd_fgcolor(0xaaaa90);  
  GD.Tag(BUTTON_LIM_SET);
  GD.cmd_button(x+20,y,95,50,29,0,"LIM");
 
  GD.Tag(0); // Note: Prevents button in some cases to react also when touching other places in UI. Why ?
  
}
void measureCurrentPanel(int x, int y, boolean compliance, bool showBar) {
  if (x >= 800) {
    return;
  }
  
  y=y+28;
  if (current_range == AMP1 && abs(C_STATS.rawValue) > SETTINGS.max_current_1A_range()) {
    if (showBar) {
      y=y+12; // dont show bar when overflow... just add extra space so the panel gets same size as without overflow...
    }
    GD.ColorA(255);
    CURRENT_DISPLAY.renderOverflowSW(x + 17, y);
  } else 
   if ( (current_range == MILLIAMP10 && abs(C_STATS.rawValue) > SETTINGS.max_current_10mA_range())) {
    if (showBar) {
      y=y+12; // dont show bar when overflow... just add extra space so the panel gets same size as without overflow...
    }
    GD.ColorA(255);
    CURRENT_DISPLAY.renderOverflowSW(x + 17, y);
  }
  
  else {
    if (showBar) {
      renderBar(x,y, C_STATS.rawValue, SMU[0].getLimitValue_micro());
      y=y+12;
    }
    GD.ColorA(255);
    bool shownA = current_range == MILLIAMP10;
    shownA = false; // Override. Dont show nA
    CURRENT_DISPLAY.renderMeasured(x /*+ 17*/, y, C_FILTERS.mean, compliance, shownA, current_range); 
  }
  CURRENT_DISPLAY.renderSet(x+120, y+105, SMU[0].getLimitValue_micro());

  y=y+105;
  
  GD.ColorRGB(0,0,0);
  GD.cmd_fgcolor(0xaaaa90);  
  GD.Tag(BUTTON_LIM_SET);
  GD.cmd_button(x+20,y,95,50,29,0,"LIM");
  GD.Tag(BUTTON_CUR_AUTO);
  GD.cmd_button(x+350,y,95,50,29,0,current_range==AMP1 ? "1A" : "10mA");
  GD.Tag(0); // Note: Prevents button in some cases to react also when touching other places in UI. Why ?
}

void drawBall(int x, int y, bool set) {
  GD.Begin(POINTS);
  GD.PointSize(16 * 6);  
  if (set == true) {
    GD.ColorRGB(255,255,255); 
  } else {
    GD.ColorRGB(0,0,0); 
  }
  GD.Vertex2ii(x, y);
}

void widgetBodyHeaderTab(int y, int activeWidget) {
  y=y+10;
  GD.Begin(RECTS);

  // tab light shaddow
  GD.ColorA(255);
  GD.LineWidth(350);
  GD.ColorRGB(0x555555);
  GD.Vertex2ii(21,y-2);
  GD.Vertex2ii(778, 480);
  
  // tab 
  GD.ColorA(255);
  GD.LineWidth(350);
  GD.ColorRGB(0x2c2c2c);
  GD.Vertex2ii(22,y);
  GD.Vertex2ii(778, 480);

  // Why does this blend to dark gray instead of being just black ?
  // Is it because the rectangle above "shines" though ? It is not set to transparrent...
  // must learn more about the blending....
  GD.Begin(RECTS);
  GD.ColorA(180); // slightly transparrent to grey shine though a bit
  GD.LineWidth(10);
  GD.ColorRGB(0x000000);
  GD.Vertex2ii(0,y+15);
  GD.Vertex2ii(799, 480);

  // strip between tab heading and content
  GD.Begin(LINE_STRIP);
  GD.ColorA(200);
  GD.LineWidth(10);
  GD.ColorRGB(COLOR_CURRENT_TEXT);
  GD.Vertex2ii(0,y+15+2);
  GD.Vertex2ii(798, y+15+2);
  GD.ColorA(255);


  y=y-2;
  int x = 400 - 30 * noOfWidgets/2;
  for (int i = 0; i < noOfWidgets; i++) {
    drawBall(x+ i*30,y,activeWidget == i);
  }
}


void showWidget(int y, int widgetNo, int scroll) {
  int yPos = y-6;
  if (widgetNo ==0 && functionType == SOURCE_SWEEP) {
     if (scroll ==0){
       GD.ColorRGB(COLOR_VOLT);
       GD.cmd_text(20, yPos, 29, 0, "MEASURE VOLTAGE");
     }
     measureCurrentPanel(scroll, yPos + 20, SMU[0].hasCompliance(), true);

  } else if (widgetNo ==0 && functionType == SOURCE_PULSE) {
     if (scroll ==0){
       GD.ColorRGB(COLOR_CURRENT_TEXT);
       GD.cmd_text(20, yPos, 29, 0, "MEASURE CURRENT");
     }
     measureCurrentPanel(scroll, yPos + 20, SMU[0].hasCompliance(), true);
  } else if (widgetNo ==0 && operationType == SOURCE_VOLTAGE) {
     if (scroll ==0){
       GD.ColorRGB(COLOR_CURRENT_TEXT);
       GD.cmd_text(20, yPos, 29, 0, "MEASURE CURRENT");
     }
     measureCurrentPanel(scroll, yPos + 20, SMU[0].hasCompliance(), true);
  } else if (widgetNo ==0 && operationType == SOURCE_CURRENT) {
     if (scroll ==0){
       GD.ColorRGB(COLOR_VOLT);
       GD.cmd_text(20, yPos, 29, 0, "MEASURE VOLTAGE");
     }
     measureVoltagePanel(scroll, yPos + 20, SMU[0].hasCompliance());
  } else if (widgetNo == 1) {
    if (!anyDialogOpen()){
       if (scroll ==0){
         GD.ColorRGB(COLOR_CURRENT_TEXT);
         GD.cmd_text(20, yPos, 29, 0, "CURRENT TREND");
       }
       renderCurrentGraph(scroll, yPos, reduceDetails());
    }
  } else if (widgetNo == 2) {
      if (!anyDialogOpen()){
        if (scroll ==0){
          GD.ColorRGB(COLOR_VOLT);
          GD.cmd_text(20, yPos, 29, 0, "VOLTAGE TREND");
        }
        renderVoltageGraph(scroll, yPos, reduceDetails());
      }
  } else if (widgetNo == 3) {
      if (scroll ==0){
        GD.ColorRGB(COLOR_VOLT);
        GD.cmd_text(20, yPos, 29, 0, "VOLTAGE HISTOGRAM");
      }
      renderHistogram(scroll, yPos, reduceDetails());
  } else if (widgetNo == 4) {
      if (scroll ==0){
        GD.ColorRGB(COLOR_VOLT);
        GD.cmd_text(20, yPos, 29, 0, "EXPERIMENTAL");
      }

      if (operationType == SOURCE_VOLTAGE) {
        float rawM = V_FILTERS.mean;
        float setM = SMU[0].getSetValue_micro()/1000.0;
        renderExperimental(scroll,yPos, rawM, setM, false, reduceDetails());
      } else {
        float rawM = C_FILTERS.mean;
        float setM = SMU[0].getSetValue_micro()/1000.0;
        renderExperimental(scroll,yPos, rawM, setM, false, reduceDetails());
      }
  } 
  else if (widgetNo == 5) {
      if (scroll ==0){
        GD.ColorRGB(COLOR_VOLT);
        GD.cmd_text(20, yPos, 29, 0, "CAL");
      }

      if (operationType == SOURCE_VOLTAGE) {
         float rawM = V_FILTERS.mean;
         float setM = SMU[0].getSetValue_micro()/1000.0;
         if (abs(setM) > 2300) {   // TODO: Hack, using MILLIAMP10 to indicate high volt range (10V). Stupid. Fix !!!!
            V_CALIBRATION.renderCal(scroll,yPos, rawM, setM, MILLIAMP10, reduceDetails());
         } else {
           // TODO: Hack, using MILLIAMP10 to indicate high volt range (10V). Stupid. Fix !!!!
            V_CALIBRATION.renderCal(scroll,yPos, rawM, setM, AMP1, reduceDetails());

         }
       } else {
         float rawM = C_FILTERS.mean;
        float setM = SMU[0].getSetValue_micro()*1000.0;
        C_CALIBRATION.renderCal(scroll,yPos, rawM, setM, current_range,  reduceDetails());
      }
      
  }
  else if (widgetNo == 6) {
      if (scroll ==0){
        GD.ColorRGB(COLOR_VOLT);
        GD.cmd_text(20, yPos, 29, 0, "CAL 2");
      }

      if (operationType == SOURCE_VOLTAGE) {
         float rawM = V_FILTERS.mean;
         float setM = SMU[0].getSetValue_micro()/1000.0;
         V_CALIBRATION.renderCal2(scroll,yPos, rawM, setM, current_range, reduceDetails());
       } else {
         float rawM = C_FILTERS.mean;
         float setM = SMU[0].getSetValue_micro()/1000.0;
         C_CALIBRATION.renderCal2(scroll,yPos, rawM, setM, current_range, reduceDetails());
      }   
  }
  

}




int gestureDetected = GEST_NONE;
int scrollSpeed = 70;
void handleWidgetScrollPosition() {
  if (gestureDetected == GEST_MOVE_LEFT) {
//    if (activeWidget == noOfWidgets -1) {
//      Serial.println("reached right end");
//    } else {
//      scrollDir = -1;
//    }
    scrollDir = -1;
  } 
  else if (gestureDetected == GEST_MOVE_RIGHT) {
//    if (activeWidget == 0) {
//      Serial.println("reached left end");
//    } else {
//      scrollDir = 1;
//    }
    scrollDir = 1;
  } 
  
  scroll = scroll + scrollDir * scrollSpeed;
  if (scroll <= -800 && scrollDir != 0) {
    activeWidget ++;
    if (activeWidget > noOfWidgets -1) {
      // swap arund
      activeWidget = 0;
    }
    scrollDir = 0;
    scroll = 0;
  } else if (scroll >= 800 && scrollDir != 0) {
    activeWidget --;
    if (activeWidget < 0) {
      // swap around
      activeWidget = noOfWidgets - 1;
    }
    scrollDir = 0;
    scroll = 0;
  }
}

void displayWidget() {
   widgetBodyHeaderTab(LOWER_WIDGET_Y_POS, activeWidget);

  if (activeWidget >= 0) {
    if (scrollDir == 0) {
      showWidget(LOWER_WIDGET_Y_POS, activeWidget, 0);
    }
    else if (scrollDir == -1) {
      showWidget(LOWER_WIDGET_Y_POS,activeWidget, scroll);
      if (activeWidget == noOfWidgets - 1) {
        // swap from last to first
        showWidget(LOWER_WIDGET_Y_POS, 0, scroll + 800);
      } else {
        showWidget(LOWER_WIDGET_Y_POS,activeWidget + 1, scroll + 800);
      }
    } 
    else if (scrollDir == 1) {
      if (activeWidget == 0) { 
        // swap from first to last
        showWidget(LOWER_WIDGET_Y_POS,noOfWidgets -1 , scroll - 800);
      } else {
        showWidget(LOWER_WIDGET_Y_POS,activeWidget - 1, scroll - 800);
      }
      showWidget(LOWER_WIDGET_Y_POS,activeWidget, scroll + 0);
    }   
  }
  
}

void bluredBackground() {
    GD.Begin(RECTS);
    GD.ColorA(150);
    GD.ColorRGB(0x000000);
    GD.Vertex2ii(0,0);
    GD.Vertex2ii(800, 480);
}

void openMainMenu() {

   if (anyDialogOpen()) {
      closeAllOpenDialogs();
    }
    MAINMENU.open(closeMainMenuCallback);
}

void handleMenuScrolldown(){

  if (gestureDetected == GEST_MOVE_DOWN && MAINMENU.active == false) {
    openMainMenu();
    return; // start the animation etc. next time, so UI that needs to reduce details have time to reach.
  }  
  
  // main menu
  if (MAINMENU.active) {
    MAINMENU.render();
  }
 
}





void renderMainHeader() {
 
  // register screen for gestures on top half, required for pulling menu from top
  /*
  GD.Tag(GESTURE_AREA_HIGH);
  GD.Begin(RECTS);
  GD.ColorRGB(0x000000);
  GD.Vertex2ii(0,0);
  GD.Vertex2ii(800, LOWER_WIDGET_Y_POS);
  */

  // top header
  GD.Begin(RECTS);
  GD.ColorA(255);
  GD.ColorRGB(0x181818);
  GD.Vertex2ii(0,0);
  GD.Vertex2ii(800, 22);
  
  if (MAINMENU.active == true or anyDialogOpen()) {
    return;
  }
  
  GD.ColorA(255);
  GD.ColorRGB(0xdddddd);
  //GD.cmd_text(20, 0, 27, 0, "Input 25.4V / - 25.3V"); // NOTE: Currently only dummy info
  showLoadResistance(590,0);
  //showFanSpeed(220, 0);
  //GD.cmd_number(50,0,27,2,LOGGER.percentageFull);
  GD.cmd_number(520,0,27,2,UTILS.LM60_getTemperature(6));

    GD.cmd_number(50,0,27,3,analogRead(16));



  int temp = UTILS.TC74_getTemperature();
  GD.ColorRGB(0xdddddd);
  GD.cmd_text(410,0,27,0,"Temp:");
  if (temp > SETTINGS.getMaxTempAllowed()) {
    GD.ColorRGB(0xff0000);
    GD.Begin(RECTS);
    GD.Vertex2ii(465, 0);
    GD.Vertex2ii(520, 20);
    GD.ColorRGB(0xffffff);
  } else {
    GD.ColorRGB(0x00ff00);
  }
  GD.cmd_number(470,0,27,3, UTILS.TC74_getTemperature());
  GD.cmd_text(500,0,27,0,"C");

  GD.ColorRGB(0xdddddd);
  // Show log info
  //int logEntries = RAM.getCurrentLogAddress();
  //GD.cmd_text(300,0,27,0,"Log:");
  //GD.cmd_number(340,0,27,0,logEntries);
  float percentage = LOGGER.percentageFull;
  if (percentage>=100.0) {
    GD.cmd_text(300,0,27,0,"Log:      %");
  } else if (percentage>=10.0) {
    GD.cmd_text(300,0,27,0,"Log:     %");
  } else {
    GD.cmd_text(300,0,27,0,"Log:    %");
  }
  GD.cmd_number(335,1,27,0, LOGGER.percentageFull);

  // line below top header
  int y = 25;
  GD.Begin(LINE_STRIP);
  GD.ColorA(200);
  GD.LineWidth(15);
  GD.ColorRGB(COLOR_VOLT);
  GD.Vertex2ii(1,y);
  GD.Vertex2ii(799, y);
  GD.ColorA(255);

  // uptime
  DIGIT_UTIL.displayTime(millis(), 120, 0);
  
}

void showFanSpeed(int x, int y) {
//   GD.cmd_text(x,y,27,0, "Fan:");
//  GD.cmd_number(x+30,y,27,5, FAN.getRPMValueFiltered());
//  GD.cmd_text(x+80,y,27,0, "RPM");
//  Serial.print(FAN.getPWMFanRPM());
//  Serial.print("(");
//  Serial.print(FAN.getFanWidth());
//  Serial.flush();
}
void showLoadResistance(int x, int y) {
   // calculate resistance only if the voltage and current is above some limits. Else the calculation will just be noise...
    if(abs(V_FILTERS.mean) > 10.0 or abs(C_FILTERS.mean) > 0.010) {
      float resistance = abs(V_FILTERS.mean / C_FILTERS.mean);
      GD.ColorRGB(0xdddddd);
      int kOhm, ohm, mOhm;
      bool neg;
      //DIGIT_UTIL.separate(&a, &ma, &ua, &neg, rawMa);
      DIGIT_UTIL.separate(&kOhm, &ohm, &mOhm, &neg, resistance);

      if (kOhm > 0) {
        GD.cmd_number(x, y, 27, 3, kOhm);
        GD.cmd_text(x+30, y,  27, 0, ".");
        GD.cmd_number(x+35, y, 27, 3, ohm);
        GD.cmd_text(x+70, y,  27, 0, "kOhm");
      } else {
        GD.cmd_number(x, y, 27, 3, ohm);
        GD.cmd_text(x+30, y,  27, 0, ".");
        GD.cmd_number(x+35, y, 27, 3, mOhm);
        GD.cmd_text(x+70, y,  27, 0, "ohm");
      }

    }
}
void renderUpperDisplay(OPERATION_TYPE operationType, FUNCTION_TYPE functionType) {

  int x = 0;
  int y = 32;
 
  // show upper panel
  if (functionType == SOURCE_DC_VOLTAGE || functionType == SOURCE_DC_CURRENT) {
    if (operationType == SOURCE_VOLTAGE) {
        sourceVoltagePanel(x,y);
    } else {
        sourceCurrentPanel(x,y);
    }
    renderStatusIndicators(x,y);

  } else if (functionType == SOURCE_PULSE) {
    sourcePulsePanel(x,y);
  } else if (functionType == SOURCE_SWEEP) {
    sourceSweepPanel(x,y);
  }
  
}

int gestOldX = 0;
int gestOldY = 0;
int gestDurationX = 0;
int gestDurationY = 0;
uint32_t trackingDetectedTimer = millis();
bool ignoreGesture(int t){
  if (trackingDetectedTimer + 50 > millis()) {
    return true;
  }
  t = t & 0xff;
  // TODO: Make more generic.
  //       The idea is to ignore swipe especially when controlling sliders !
  for (int i=0;i<2;i++) {
    if (t == sliderTags[i]) {
      trackingDetectedTimer = millis();
      trackingOngoing = t;

      return true;
    }
  }
  trackingOngoing = 0;
  return false;
}

int detectGestures() {
  GD.get_inputs();
  if (ignoreGesture(GD.inputs.track_tag)){  
    return GEST_NONE;
  }
  
  //Serial.println(GD.inputs.tag);
  int touchX = GD.inputs.x;
  int touchY = GD.inputs.y;
  int gestDistanceX = touchX - gestOldX;
  int gestDistanceY = touchY - gestOldY;

  if (touchX == -32768 && touchX == -32768) { // TODO: Why negative values when no touch ? Use unsigned ?
    gestDurationX = 0;
    gestDurationY = 0;
    gestDistanceX = 0;
    gestDistanceY = 0;
    gestOldX = 0;
    gestOldY = 0;
  }

 // if ((GD.inputs.tag == GESTURE_AREA_LOW || GD.inputs.tag == GESTURE_AREA_HIGH) && gestureDetected ==EST_NONE) {
  if (gestureDetected == GEST_NONE) {
    if (touchX > 0 && touchY > LOWER_WIDGET_Y_POS && gestDistanceX < -20 && scrollDir == 0) {
      if (++gestDurationX >= 3) {
        Serial.println("gesture = move left");
        Serial.flush();
        gestureDetected = GEST_MOVE_LEFT;
        gestDurationX = 0;
      }
    }
    else if (touchX > 0 && touchY > LOWER_WIDGET_Y_POS && gestDistanceX > 20 && scrollDir == 0) {
      if (++gestDurationX >= 3) {
        Serial.println("gesture = move right");
        Serial.flush();
        gestureDetected = GEST_MOVE_RIGHT;
        gestDurationX = 0;
      }
    } 
    else if (touchY > 0 && touchY<150 && gestDistanceY > 10 && scrollDir == 0) {
       if (++gestDurationY >= 2) {
        Serial.println("gesture = move down from upper");
        Serial.flush();
        gestureDetected = GEST_MOVE_DOWN;
        gestDurationY = 0;
      }
    } 
    
  } else {
    gestureDetected = GEST_NONE;
    gestDurationX = 0;
    gestDurationY = 0;
  }
  gestOldX = touchX; //GD.inputs.y;  
  gestOldY = touchY; //GD.inputs.x;  
  return gestureDetected;
}

int timeBeforeAutoNull = millis() + 5000;

void startNullCalibration() {
  Serial.println("Force auto null...");
  timeBeforeAutoNull = millis();
  nullCalibrationDone0 = false;
  nullCalibrationDone1 = false;
  nullCalibrationDone2 = false;
  autoNullStarted = true;

  // set null to 0
  if (operationType == SOURCE_VOLTAGE) {
    V_CALIBRATION.setNullValueVol(0.0, MILLIAMP10);
    V_CALIBRATION.setNullValueVol(0.0, AMP1);
  } else {
    V_CALIBRATION.setNullValueCur(0.0, MILLIAMP10);
    V_CALIBRATION.setNullValueCur(0.0, AMP1);
  }
  //TODO: Do the same when calibration on constant current mode as well !
  


}

void handleAutoNull() {

//   Serial.print("x ");
//   Serial.print(nullCalibrationDone0);
//   Serial.print(" ");
//   Serial.println(timeBeforeAutoNull < millis());
  if (autoNullStarted && !nullCalibrationDone0 && timeBeforeAutoNull < (int)millis()) {
    Serial.println("Performing auto null...");
    C_CALIBRATION.useCalibratedValues = false;
    V_CALIBRATION.useCalibratedValues = false;
    current_range = MILLIAMP10;
    //Serial.println("Null calibration initiated...");
    SMU[0].setCurrentRange(current_range, operationType);
    if (operationType == SOURCE_VOLTAGE) {
      SMU[0].fltSetCommitVoltageSource(0, true);
    } else {
      SMU[0].fltSetCommitCurrentSource(0);
    }
    //V_FILTERS.init();
    V_FILTERS.setFilterSize(10);
    C_FILTERS.setFilterSize(10);

    SMU[0].setSamplingRate(20);
    nullCalibrationDone0 = true;
   
  }
  int msWaitPrCal = 5000;
  if (autoNullStarted && !nullCalibrationDone1 && timeBeforeAutoNull + msWaitPrCal < (int)millis()) {
    //float v = V_STATS.rawValue; 
    if (operationType == SOURCE_VOLTAGE) {
      float v = V_FILTERS.mean;   
      V_CALIBRATION.setNullValueVol(v, current_range);
      Serial.print("Removed voltage offset from 10mA range:");  
      Serial.println(v,3);  
    } else {
      float v = C_FILTERS.mean;   
      V_CALIBRATION.setNullValueCur(v, current_range);
      Serial.print("Removed current offset from 10mA range:");  
      Serial.println(v,3);  
    }

    nullCalibrationDone1 = true;
  } 
 
  if (autoNullStarted && !nullCalibrationDone2 && /*timeAtStartup + */timeBeforeAutoNull + msWaitPrCal + 100 < (int)millis()) {
    current_range = AMP1;
    //Serial.println("Null calibration initiated...");
    SMU[0].setCurrentRange(current_range, operationType);
    if (operationType == SOURCE_VOLTAGE) {
      SMU[0].fltSetCommitVoltageSource(0, true);
    } else {
      SMU[0].fltSetCommitCurrentSource(0);
    }
  }
  if (autoNullStarted && !nullCalibrationDone2 && /*timeAtStartup +*/ timeBeforeAutoNull + msWaitPrCal*2 < (int)millis()) {
   if (operationType == SOURCE_VOLTAGE) {
      float v = V_FILTERS.mean;   
      V_CALIBRATION.setNullValueVol(v, current_range);
      Serial.print("Removed voltage offset from 10mA range:");  
      Serial.println(v,3);  
    } else {
      float v = C_FILTERS.mean;   
      V_CALIBRATION.setNullValueCur(v, current_range);
      Serial.print("Removed current offset from 10mA range:");  
      Serial.println(v,3);  
    }
    
    
    nullCalibrationDone2 = true;
    //V_FILTERS.init(1234);
    V_FILTERS.setFilterSize(5);
    //C_FILTERS.init(2345);
    C_FILTERS.setFilterSize(5);
    V_CALIBRATION.useCalibratedValues = true;
        C_CALIBRATION.useCalibratedValues = true;

    autoNullStarted = false;

  } 

}

void notification(const char *text) {
   GD.Begin(RECTS);
    GD.ColorA(230);  // already opaque, why ?
    GD.ColorRGB(0x222222);
    GD.Vertex2ii(180, 160);
    GD.Vertex2ii(620, 280);
    GD.ColorRGB(0xffffff);
    GD.cmd_text(250, 200 , 29, 0, text);
}


bool digitize = false;

int ramAdrPtr = 0;

float ramEmulator[1000];

int maxFloats = 32000; // 32000 is max for the ram
int nrOfFloats = 400;

bool bufferOverflow = false;
float maxDigV = -100000.00, minDigV = 100000.00, curDigV;
float maxDigI = -100000.00, minDigI = 100000.00, curDigI;
int digitizeDuration = millis();
float lastVoltage = 0.0;
bool triggered = false;
int count = 1;
float sampleVolt = 10.0;
int logTimer = millis();
int digitizeCounter = 0;
int msDigit = 0;
float simulatedWaveform;


void handleSamplingForDigitizer(int dataR) {
  digitizeCounter ++;
    
//     if (count % 20== 0) {
//       SMU[0].fltSetCommitVoltageSource(sampleVolt, false);
//       if (sampleVolt == 2000.0) {
//        sampleVolt = -2000.0;
//       } else {
//        sampleVolt = 2000.0;
//       }
//     }
//     count ++;

  if (dataR == 1) {
    return;
  }
//     if (dataR == 1) {
//      float i = SMU[0].measureCurrent(AMP1);   
//      RAM.writeRAMfloat(ramAdrPtr, i);
//      ramAdrPtr += 4;
//      if (ramAdrPtr > maxFloats*4) {
//        bufferOverflow = true;
//      }
//      //Serial.println(i);
//      curDigI = i;
//      if (i > maxDigI) {
//        maxDigI= i;
//      } 
//      if (i < minDigI) {
//        minDigI = i;
//      }
//      
//      return;
//     }



  float v = SMU[0].measureMilliVoltage();  
   //v=100.0 + random(20)/100.0;
   //simulatedWaveform += 0.05;
   // v = v + sin(simulatedWaveform)*5.0;
    


      
  bool continuous = true;
  if (!triggered) {
    if (continuous or (lastVoltage < 100.0 && v > 100.0)) {
      triggered = true;
      //Serial.println("Triggered!");
      //Serial.println(lastVoltage);
      } else {
       lastVoltage = v;         
       return;
      }
    }
    lastVoltage = v;

    //RAM.writeRAMfloat(ramAdrPtr, v);
    ramEmulator[ramAdrPtr/4] = v;
    if (ramAdrPtr == 0) {
      digitizeDuration = millis();
    }
    ramAdrPtr += 4;
    if (ramAdrPtr > nrOfFloats*4) {
      bufferOverflow = true;
      triggered = false;
      //lastVoltage = 0.0;
      //Serial.println("Overflow!");
      ramAdrPtr = 0;
      //Serial.print("Digitize duration:");
      //Serial.println(millis() - digitizeDuration);
      //Serial.print("Adr:");
      //Serial.println(ramAdrPtr);  
    }
    curDigV = v;
    if (v > maxDigV) {
      maxDigV = v;
    } 
    if (v < minDigV) {
      minDigV = v;
    }      
    return;
  }
  
static void handleSampling() {

  int dataR = SMU[0].dataReady();
  if (digitize == true && bufferOverflow==false && (dataR == 0 or dataR == 1)) {
    handleSamplingForDigitizer(dataR);
  }
 
  if (dataR == -1) {
    return;
  }
  if (dataR == -99) {
    Serial.println("DONT USE SAMPLE!");  
  } 
  else if (dataR == -98) {
    Serial.println("OVERFLOW"); // haven't been able to get this to work...
  }
  
  else if (dataR == 1) {
    float Cout = SMU[0].measureCurrent(current_range);
    //TODO: DIffer between constant current and constant voltage nulling
    Cout = Cout - V_CALIBRATION.nullValueCur[current_range];
    Cout = Cout - C_CALIBRATION.relativeValue[current_range];
    C_STATS.addSample(Cout);
    C_FILTERS.updateMean(Cout, false);
  }
  else if(dataR == 0) {
    float Vout = SMU[0].measureMilliVoltage();
    Vout = Vout - V_CALIBRATION.relativeValue[current_range];
    V_STATS.addSample(Vout);    
    V_FILTERS.updateMean(Vout, true);
    SIMPLE_STATS.registerValue(V_FILTERS.mean);
    LOGGER.registerValue2(Vout);
    // store now and then
    if (logTimer + 1000 < (int)millis()) {
     logTimer = millis();
     //RAM.logData(V_FILTERS.mean);
     RAM.logDataCalculateMean(V_FILTERS.mean, 1);
    }
  }
}

void handleAutoCurrentRange() {
     return;
     if (!autoNullStarted && !V_CALIBRATION.autoCalInProgress && !C_CALIBRATION.autoCalInProgress) {
      float milliAmpere = C_STATS.rawValue;
//      Serial.print(milliAmpere,5);
//      Serial.print("mA, current range:");
//      Serial.println(current_range);

      // auto current range switch. TODO: Move to hardware ? Note that range switch also requires change in limit
      float hysteresis = 0.5;
      float switchAt = SETTINGS.max_current_10mA_range();
      
        if (current_range == AMP1 && abs(milliAmpere) < switchAt - hysteresis) {
          current_range = MILLIAMP10;
          SMU[0].setCurrentRange(current_range,operationType);
          Serial.println("switching to range 1");
           //TODO: Use getLimitValue from SMU instead of LIMIT_DIAL ?
          if (operationType == SOURCE_VOLTAGE){
            if (SMU[0].fltSetCommitCurrentLimit(SMU[0].getLimitValue_micro()*1000/*LIMIT_DIAL.getMv()*/, _SOURCE_AND_SINK)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
          } 

        }
        // TODO: Make separate function to calculate current based on shunt and voltage!
//        Serial.print("Check 10mA range and if it should switch to 1A... ma=");
//        Serial.print(milliAmpere);
//        Serial.print(" ");
//        Serial.println(switchAt);
        if (current_range == MILLIAMP10 && abs(milliAmpere) > switchAt) {
          current_range = AMP1;
          SMU[0].setCurrentRange(current_range,operationType);
          Serial.println("switching to range 0");
           //TODO: Use getLimitValue from SMU instead of LIMIT_DIAL ?
          if (operationType == SOURCE_VOLTAGE){
            if (SMU[0].fltSetCommitCurrentLimit(SMU[0].getLimitValue_micro()*1000/*LIMIT_DIAL.getMv()/1000.0*/, _SOURCE_AND_SINK)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
          } 

          
        }
    }
}

int displayUpdateTimer = millis();

int loopUpdateTimer = millis();

void loop() {

  if (V_CALIBRATION.autoCalInProgress) {
    V_CALIBRATION.autoCalADCfromDAC();
  } 
  if (C_CALIBRATION.autoCalInProgress) {
    C_CALIBRATION.autoCalADCfromDAC();
  } 
  

  // No need to update display more often that the eye can detect.
  // Too often will cause jitter in the sampling because display and DAC/ADC share same SPI port.
  // Will that be improved by Teensy 4 where there are more that one SPI port ?
  //
  // Note that the scrolling speed and gesture detection speed will be affected.
  // 
  ROTARY_ENCODER.handle(SMU[0].use100uVSetResolution());

  //TODO: Not sure if this should be here.... this is more that "display time"... it also handles sampling of not driven by timer interrupt...
  if (displayUpdateTimer + 20 > (int)millis()) {
    return; 
  }
 
  displayUpdateTimer = millis();
 
 if (functionType == DIGITIZE) {
    loopDigitize();

  } else if (functionType == GRAPH) {

//    if (loopUpdateTimer + 10 > millis() ) {
//      return;
//    }
//    loopUpdateTimer = millis();

    OPERATION_TYPE ot = getOperationType();
    disable_ADC_DAC_SPI_units();
    GD.resume();
    GD.Clear();
    detectGestures();
    renderMainHeader();
    TRENDGRAPH.loop(ot);

    handleMenuScrolldown();
    int tag = GD.inputs.tag;
    // TODO: don't need to check buttons for inactive menus or functions...
    MAINMENU.handleButtonAction(tag);
    PUSHBUTTONS.handle();
        PUSHBUTTON_ENC.handle();

    GD.swap();
    GD.__end();
    
  } else if (functionType == DATALOGGER) {

//    if (loopUpdateTimer + 10 > millis() ) {
//      return;
//    }
//    loopUpdateTimer = millis();

    OPERATION_TYPE ot = getOperationType();
    disable_ADC_DAC_SPI_units();
    GD.resume();
    GD.Clear();
    detectGestures();
    renderMainHeader();
    LOGGER.loop();

    handleMenuScrolldown();
    int tag = GD.inputs.tag;
    // TODO: don't need to check buttons for inactive menus or functions...
    MAINMENU.handleButtonAction(tag);
    PUSHBUTTONS.handle();
        PUSHBUTTON_ENC.handle();

    GD.swap();
    GD.__end();
    #ifndef SAMPLING_BY_INTERRUPT 
    handleSampling(); 
   #endif
    
  }
  else {
    loopMain();
  }
}





int samplingDelayTimer = millis();

void loopDigitize() {
  //if (loopUpdateTimer + 10 > (int)millis() ) {
  //  return;
  //}
  
  loopUpdateTimer = millis();
  operationType = getOperationType();
  disable_ADC_DAC_SPI_units();
  
  //int fullSamplePeriod;
   if (digitize == false && !MAINMENU.active /*&&  samplingDelayTimer + 50 < millis()*/){
     
    ramAdrPtr = 0;
     minDigV = 1000000;
     maxDigV = - 1000000;
     minDigI = 1000000;
     maxDigI = - 1000000;
     SMU[0].setSamplingRate(10000);
     nrOfFloats = 400;
          //bufferOverflowed=false;
     digitize = true;
         bufferOverflow = false;
    

  msDigit= millis();
     
   }

  disable_ADC_DAC_SPI_units();
  if (bufferOverflow == true) {
    digitize = false;
    Serial.println("Initiate new samping round...");
    samplingDelayTimer = millis();
    
  }
  //TODO: Move digitizine to a separate class
  if (bufferOverflow == false && digitize == true){
//    //GD.cmd_number(100, 100, 1, 6, ramAdrPtr);  
//    GD.resume();
//    GD.Clear();
//    VOLT_DISPLAY.renderMeasured(100,200, maxDigV);
//    VOLT_DISPLAY.renderMeasured(100,300, minDigV);
//    //VOLT_DISPLAY.renderMeasured(100,400, curDigV);
//    Serial.println(ramAdrPtr);
//    GD.swap();
//    GD.__end();
  } else {
    //fullSamplePeriod = millis()-msDigit;
//         SMU[0].setSamplingRate(10);

    //digitize = false; 
    bufferOverflow = false;

    
    GD.resume();
      GD.Clear();
        //SMU[0].setSamplingRate(100);

        

    detectGestures();
    renderMainHeader();
    VOLT_DISPLAY.renderMeasured(10,50, minDigV, false);
    VOLT_DISPLAY.renderMeasured(10,150, maxDigV, false);
    CURRENT_DISPLAY.renderMeasured(10,250, minDigI, false, false,current_range);
    CURRENT_DISPLAY.renderMeasured(10,350, maxDigI, false, false, current_range);

   // VOLT_DISPLAY.renderMeasured(10,50, digitizeCounter, false);
    //    VOLT_DISPLAY.renderMeasured(10,200, fullSamplePeriod, false);

digitizeCounter = 0;

  GD.Begin(LINE_STRIP);
  GD.ColorA(255);
  GD.ColorRGB(0x00ff00);

  float mva[400];
  float mia[400];

  GD.__end();
  for (int x = 0; x<nrOfFloats/2; x++) {
    int adr = x*8;
    mva[x] = ramEmulator[adr/4]; //RAM.readRAMfloat(adr);
    
    mia[x] = ramEmulator[adr/4 + 1]; //RAM.readRAMfloat(adr+4);
  }
  GD.resume();
  
  for (int x = 0; x<nrOfFloats/2; x++) {
    //int adr = x*8;
    //GD.__end();
    //float mv = RAM.readRAMfloat(adr);
    //float i = RAM.readRAMfloat(adr+4);
    //GD.resume();
   // Serial.println(mv,5);

   if (minDigV < 100000) {
      //float span = (maxDig - minDig);
      float mid = (maxDigV + minDigV) /2.0;
      float relative = mid - mva[x];
     
      float y = relative*20;
     // float y = mva[x] / 100.0;
      GD.Vertex2ii(x*4, 200 - y);
   }
    
  }

  GD.Begin(LINE_STRIP);
  GD.ColorA(255);
  GD.ColorRGB(0xff0000);

  for (int x = 0; x<nrOfFloats/2; x++) {
    //int adr = x*8;
    //GD.__end();
    //float mv = RAM.readRAMfloat(adr);
    //float i = RAM.readRAMfloat(adr+4);
    //GD.resume();
   // Serial.println(mv,5);

    if (minDigI < 100000) {
      //float relative = minDigI - mia[x];
      //float y = relative*50;
      float y = mia[x]/10.0;
      GD.Vertex2ii(x*4, 400 - y);
    }
    
  }
  
    //VOLT_DISPLAY.renderMeasured(100,50, V_FILTERS.mean);
    //CURRENT_DISPLAY.renderMeasured(100,150, C_FILTERS.mean, false, false);

      handleMenuScrolldown();

  int tag = GD.inputs.tag;
   // TODO: don't need to check buttons for inactive menus or functions...
  MAINMENU.handleButtonAction(tag);
        PUSHBUTTONS.handle();
                PUSHBUTTON_ENC.handle();


   GD.swap();
    GD.__end();
  
  }
  
}





int prevTag = 0;
int prevTag2 = 0;
int tagTimer = millis();


bool buttonPressed=false;
int buttonPressedPeriod = 0;
int prevButton = 0;
int x[10];
int y[10];
int nrOfChecks = 3;
int checkButtons() {

  int valueToReturnIfTooFast = 0;

    if (MAINMENU.active == true) {
      return 0;
    }
    
   int trackTag = GD.inputs.track_tag & 0xff;
   if (trackTag != 0 && trackingOngoing) {
     return 0;
   }

   int tag;

    tag = GD.inputs.tag;



    if (tag>0 && tagTimer + 20 > (int)millis()) {
       return 0;
    } 
        tagTimer = millis();

    if (tag>0) {
      int touchX = GD.inputs.x;
      int touchY = GD.inputs.y;
      buttonPressed = true;
      if (prevButton == buttonPressed) {
        x[buttonPressedPeriod] = touchX;
        y[buttonPressedPeriod] = touchY;
        buttonPressedPeriod ++;

        //Serial.print("Button pressed:");
        //Serial.println(buttonPressedPeriod);
      }
      prevButton = buttonPressed;
    } else {
      prevButton = 0;
      buttonPressed = false;
      buttonPressedPeriod = 0;
    }

    if (buttonPressedPeriod<nrOfChecks) {
      return 0;
    }
    bool accidental = false;
    // check that all registred touches was within a certain area. To make sure it's not a gesture accidentally touching a button
    for (int i = 0; i<nrOfChecks-1;i++) {
      if (x[i] > x[i+1] + 10 or x[i] < x[i+1] - 10 or  y[i] > y[i+1] + 10 or  y[i] < y[i+1] - 10) {
        // at least one touch outside acceptable area !
        accidental = true;
      }
    }

    if (accidental) {
      Serial.println("Gesture accidentally touched a button...");
      return 0;
    } else {
      //Serial.println("Button pressed :-)");
    }


    
    buttonPressedPeriod= 0;

 
      
    prevTag = tag;
    
  
    if (tag == BUTTON_SOURCE_SET) {
      Serial.println("open dial to set source, start with value ");
      Serial.println((float)SMU[0].getSetValue_micro()/1000.0);
      SOURCE_DIAL.open(operationType, SET,  closeSourceDCCallback, SMU[0].getSetValue_micro());
    } else if (tag == BUTTON_LIM_SET) {
      Serial.println("open dial to set limit, start with value ");
      DIGIT_UTIL.print_uint64_t(SMU[0].getLimitValue_micro());
      LIMIT_DIAL.open(operationType, LIMIT, closeSourceDCCallback, SMU[0].getLimitValue_micro());
    } else if (tag == BUTTON_REL) {
      Serial.println("Set relative");
      V_CALIBRATION.toggleRelativeValue(V_STATS.rawValue, current_range);
      C_CALIBRATION.toggleRelativeValue(C_STATS.rawValue, current_range);
    } else if (tag == BUTTON_UNCAL) {
      Serial.println("Uncal set");
      V_CALIBRATION.toggleCalibratedValues();
      C_CALIBRATION.toggleCalibratedValues();
    } else if (tag == BUTTON_CLEAR_BUFFER) {
      Serial.println("clearbuffer set");

      SIMPLE_STATS.clear(); // TODO: Separate clearing for this ?
      V_STATS.clearBuffer();
      C_STATS.clearBuffer();
      DIGIT_UTIL.startIndicator(tag); 
    } else if (tag == BUTTON_CUR_AUTO) { //TODO: Change name
      if (timeSinceLastChange + 500 < millis()){
        Serial.println("current range set");
       
        // swap current range
        if (current_range == AMP1) {
          current_range = MILLIAMP10;
        } else {
          current_range = AMP1;
        }
        timeSinceLastChange = millis();
        
        // update limit setting 
        //TODO: Update the limit value when changing current range.
        //      Messy to to it here because if requires to switch between UI and DAC/ADC.
        //      Move all operations to change DAC/ADC stuff to a common place when it needs to change? And only have that parth within the SPI settings ?
        GD.__end();
        disable_ADC_DAC_SPI_units(); 


        
        SMU[0].setCurrentRange(current_range,operationType);

        // NOTE: The current limit update is now automatically handled by the setCurrentRange function
        /*
        //TODO: Use getLimitValue from SMU instead of LIMIT_DIAL ?
        if (operationType == SOURCE_CURRENT){
   
          if (SMU[0].fltSetCommitVoltageLimit(LIMIT_DIAL.getMv()/1000.0, _SOURCE_AND_SINK)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);

        } else {
          if (SMU[0].fltSetCommitCurrentLimit(*LIMIT_DIAL.getMv()/1000.0, _SOURCE_AND_SINK)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);

         }
        */
        GD.resume();

        
        if (operationType == SOURCE_CURRENT){
          //Serial.println("SHOULD SET NEW OUTPUT WHEN SWITCHING CURRENT RANGE IN SOURCE CURRENT MODE ????");
          //if (SMU[0].fltSetCommitCurrentSource(SMU[0].getSetValuemV())) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
        }



        

      }
    } else if (tag == BUTTON_SAMPLE_RATE_5 or tag == BUTTON_SAMPLE_RATE_20 or tag == BUTTON_SAMPLE_RATE_50 or tag == BUTTON_SAMPLE_RATE_100) { //TODO: Change name
      if (timeSinceLastChange + 500 < millis()){
        timeSinceLastChange = millis();
      } 
     
      if (tag == BUTTON_SAMPLE_RATE_5) {
        SMU[0].setSamplingRate(5); 
      }
      if (tag == BUTTON_SAMPLE_RATE_20) {
        SMU[0].setSamplingRate(20);
      }
      if (tag == BUTTON_SAMPLE_RATE_50) {
        SMU[0].setSamplingRate(50);
      }
       if (tag == BUTTON_SAMPLE_RATE_100) {
        SMU[0].setSamplingRate(100);
      }

    } 




    #ifndef USE_SIMULATOR // dont want to mess up calibration while in simulation mode...
    else if (tag == BUTTON_DAC_GAIN_COMP_UP) {
       if (timeSinceLastChange + 200 > millis()){
        return valueToReturnIfTooFast;
       } 
       timeSinceLastChange = millis();
       DIGIT_UTIL.startIndicator(tag);
       float mv = SMU[0].getSetValue_micro()/1000.0;
       if (operationType == SOURCE_VOLTAGE) {
         if (mv < 0) {
            V_CALIBRATION.adjDacGainCompNeg(0.000005);
         } else {
            V_CALIBRATION.adjDacGainCompPos(0.000005);
         }
         GD.__end();
         if (SMU[0].fltSetCommitVoltageSource(mv*1000, true)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
         GD.resume();
       } else {
          if (mv < 0) {
            C_CALIBRATION.adjDacGainCompNeg(0.00001);
         } else {
            C_CALIBRATION.adjDacGainCompPos(0.00001);
         }
         GD.__end();
         if (SMU[0].fltSetCommitCurrentSource(mv*1000)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
         GD.resume();
       }
      
    } 
    
    else if (tag == BUTTON_DAC_GAIN_COMP_UP2) {
       if (timeSinceLastChange + 200 > millis()){
        return valueToReturnIfTooFast;
       } 
       timeSinceLastChange = millis();
       DIGIT_UTIL.startIndicator(tag);
       float mv = SMU[0].getSetValue_micro()/1000.0;
       if (operationType == SOURCE_VOLTAGE) {
         if (mv < 0) {
            V_CALIBRATION.adjDacGainCompNeg2(0.000005);
         } else {
            V_CALIBRATION.adjDacGainCompPos2(0.000005);
         }
         GD.__end();
         if (SMU[0].fltSetCommitVoltageSource(mv*1000, true)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
         GD.resume();
       } else {
          if (mv < 0) {
            C_CALIBRATION.adjDacGainCompNeg2(0.000005);
         } else {
            C_CALIBRATION.adjDacGainCompPos2(0.000005);
         }
         GD.__end();
         if (SMU[0].fltSetCommitCurrentSource(mv*1000)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
         GD.resume();
       }
      
    } 

    else if (tag == BUTTON_DAC_GAIN_COMP_DOWN) {
       if (timeSinceLastChange + 200 > millis()){
        return valueToReturnIfTooFast;
      } 
       timeSinceLastChange = millis();
       DIGIT_UTIL.startIndicator(tag);
       float mv = SMU[0].getSetValue_micro()/1000;
       if (operationType == SOURCE_VOLTAGE) {
         if (mv < 0) {
            V_CALIBRATION.adjDacGainCompNeg(-0.000005);
         } else {
            V_CALIBRATION.adjDacGainCompPos(-0.000005);
         }
         //if (operationType == SOURCE_VOLTAGE) {
         GD.__end();
         if (SMU[0].fltSetCommitVoltageSource(mv*1000, true)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
         GD.resume();
       } else {
          if (mv < 0) {
            C_CALIBRATION.adjDacGainCompNeg(-0.00001 );
         } else {
            C_CALIBRATION.adjDacGainCompPos(-0.00001 );
         }
         GD.__end();
         if (SMU[0].fltSetCommitCurrentSource(mv*1000)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
         GD.resume();
       }   
    } 
    
    else if (tag == BUTTON_DAC_GAIN_COMP_DOWN2) {
       if (timeSinceLastChange + 200 > millis()){
        return valueToReturnIfTooFast;
      } 
       timeSinceLastChange = millis();
       DIGIT_UTIL.startIndicator(tag);
       float mv = SMU[0].getSetValue_micro()/1000.0;
       if (operationType == SOURCE_VOLTAGE) {
         if (mv < 0) {
            V_CALIBRATION.adjDacGainCompNeg2(-0.000005);
         } else {
            V_CALIBRATION.adjDacGainCompPos2(-0.000005);
         }
         //if (operationType == SOURCE_VOLTAGE) {
         GD.__end();
         if (SMU[0].fltSetCommitVoltageSource(mv*1000, true)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
         GD.resume();
       } else {
          if (mv < 0) {
            C_CALIBRATION.adjDacGainCompNeg2(-0.000005);
         } else {
            C_CALIBRATION.adjDacGainCompPos2(-0.000005);
         }
         GD.__end();
         if (SMU[0].fltSetCommitCurrentSource(mv*1000)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
         GD.resume();
       }   
    } 
    
    



    else if (tag == BUTTON_DAC_GAIN_COMP_LIM_UP) {
       if (timeSinceLastChange + 200 > millis()){
        return valueToReturnIfTooFast;
      } 
       timeSinceLastChange = millis();
       DIGIT_UTIL.startIndicator(tag);
       float mv = SMU[0].getLimitValue_micro();
       if (operationType == SOURCE_VOLTAGE) {
         V_CALIBRATION.adjDacGainCompLim(+0.000005*10.0);
         
         //if (operationType == SOURCE_VOLTAGE) {
         GD.__end();
         if (SMU[0].fltSetCommitCurrentLimit(mv*1000, true)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
         GD.resume();
       }
      
       
    } 
    
    
    else if (tag == BUTTON_DAC_GAIN_COMP_LIM_DOWN) {
       if (timeSinceLastChange + 200 > millis()){
        return valueToReturnIfTooFast;
      } 
       timeSinceLastChange = millis();
       DIGIT_UTIL.startIndicator(tag);
       float mv = SMU[0].getLimitValue_micro();
       if (operationType == SOURCE_VOLTAGE) {
         V_CALIBRATION.adjDacGainCompLim(-0.000005*10.0);
         
         //if (operationType == SOURCE_VOLTAGE) {
         GD.__end();
         if (SMU[0].fltSetCommitCurrentLimit(mv*1000, true)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
         GD.resume();
       }
      
       
    } 
    
    
    
    
    else if (tag == BUTTON_ADC_GAIN_COMP_UP) {
       if (timeSinceLastChange + 200 > millis()){
        return valueToReturnIfTooFast;
        
       } 
       timeSinceLastChange = millis();
       DIGIT_UTIL.startIndicator(tag);
       float mv = SMU[0].getSetValue_micro()/1000.0;
       if (operationType == SOURCE_VOLTAGE) {
         mv = V_STATS.rawValue;
         if (mv < 0) {
            V_CALIBRATION.adjAdcGainCompNeg(0.000001 * 10.0);
         } else {
            V_CALIBRATION.adjAdcGainCompPos(0.000001 * 10.0);
         }
       } else {
         mv = C_STATS.rawValue;
         if (mv < 0) {
            C_CALIBRATION.adjAdcGainCompNeg(0.000005 *10.0);
         } else {
            C_CALIBRATION.adjAdcGainCompPos(0.000005 *10.0);
         }
       }
      
    } 
    else if (tag == BUTTON_ADC_GAIN_COMP_DOWN) {
       if (timeSinceLastChange + 200 > millis()){
        return valueToReturnIfTooFast;
       } 
       timeSinceLastChange = millis();
       DIGIT_UTIL.startIndicator(tag);
       float mv = SMU[0].getSetValue_micro()/1000.0;
       if (operationType == SOURCE_VOLTAGE) {
         mv = V_STATS.rawValue;
         if (mv < 0) {
            V_CALIBRATION.adjAdcGainCompNeg(-0.000001 * 10.0);
         } else {
            V_CALIBRATION.adjAdcGainCompPos(-0.000001 * 10.0);
         }
       } else {
        mv = C_STATS.rawValue;
        if (mv < 0) {
            C_CALIBRATION.adjAdcGainCompNeg(-0.000005 *2.0);
         } else {
            C_CALIBRATION.adjAdcGainCompPos(-0.000005 *2.0);
         }
       }
      
    } 




    else if (tag == BUTTON_ADC_GAIN_COMP_UP2) {
       if (timeSinceLastChange + 200 > millis()){
        return valueToReturnIfTooFast;
        
       } 
       timeSinceLastChange = millis();
       DIGIT_UTIL.startIndicator(tag);
       float mv = SMU[0].getSetValue_micro()/1000.0;
       if (operationType == SOURCE_VOLTAGE) {
         mv = V_STATS.rawValue;
         if (mv < 0) {
            V_CALIBRATION.adjAdcGainCompNeg2(0.000001);
         } else {
            V_CALIBRATION.adjAdcGainCompPos2(0.000001);
         }
       } else {
         mv = C_STATS.rawValue;
         if (mv < 0) {
            C_CALIBRATION.adjAdcGainCompNeg2(0.000001);
         } else {
            C_CALIBRATION.adjAdcGainCompPos2(0.000001);
         }
       }
      
    } 
    else if (tag == BUTTON_ADC_GAIN_COMP_DOWN2) {
       if (timeSinceLastChange + 200 > millis()){
        return valueToReturnIfTooFast;
       } 
       timeSinceLastChange = millis();
       DIGIT_UTIL.startIndicator(tag);
       float mv = SMU[0].getSetValue_micro()/1000.0;
       if (operationType == SOURCE_VOLTAGE) {
         mv = V_STATS.rawValue;
         if (mv < 0) {
            V_CALIBRATION.adjAdcGainCompNeg2(-0.000001);
         } else {
            V_CALIBRATION.adjAdcGainCompPos2(-0.000001);
         }
       } else {
        mv = C_STATS.rawValue;
        if (mv < 0) {
            C_CALIBRATION.adjAdcGainCompNeg2(-0.000001);
         } else {
            C_CALIBRATION.adjAdcGainCompPos2(-0.000001);
         }
       }
      
    } 
    
    
    
    else if (tag == BUTTON_DAC_ZERO_COMP_UP or tag == BUTTON_DAC_ZERO_COMP_UP2 ) {
      if (timeSinceLastChange + 200 > millis()){
        return valueToReturnIfTooFast;
      } 
      timeSinceLastChange = millis();
       DIGIT_UTIL.startIndicator(tag);
       float mv = SMU[0].getSetValue_micro()/1000.0;
       if (operationType == SOURCE_VOLTAGE) {
          V_CALIBRATION.adjDacZeroComp(+0.000002);
          GD.__end();
         if (SMU[0].fltSetCommitVoltageSource(mv*1000, true)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
         GD.resume();
         
       } else {
          if (tag == BUTTON_DAC_ZERO_COMP_UP) {
            C_CALIBRATION.adjDacZeroComp(+0.000001);
          } else {
            C_CALIBRATION.adjDacZeroComp2(+0.000002);
          }
          GD.__end();
         if (SMU[0].fltSetCommitCurrentSource(mv*1000)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
         GD.resume();    
       }
    }
    else if (tag == BUTTON_DAC_ZERO_COMP_DOWN or tag == BUTTON_DAC_ZERO_COMP_DOWN2) {
       if (timeSinceLastChange + 200 > millis()){
         return valueToReturnIfTooFast;
       } 
       timeSinceLastChange = millis();
       DIGIT_UTIL.startIndicator(tag);
       float mv = SMU[0].getSetValue_micro()/1000.0;
       if (operationType == SOURCE_VOLTAGE) {
         V_CALIBRATION.adjDacZeroComp(-0.000002);
         GD.__end();
         if (SMU[0].fltSetCommitVoltageSource(mv*1000, true)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
         GD.resume();
       } else {
         if (tag == BUTTON_DAC_ZERO_COMP_DOWN) {
           C_CALIBRATION.adjDacZeroComp(-0.000001);
         } else {
           C_CALIBRATION.adjDacZeroComp2(-0.000002);
         }
         GD.__end();
         if (SMU[0].fltSetCommitCurrentSource(mv*1000)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
         GD.resume(); 
       }
    }
    else if (tag == BUTTON_DAC_NONLINEAR_CALIBRATE) {
       if (timeSinceLastChange + 1000 > millis()){
        return valueToReturnIfTooFast;
       } 
       timeSinceLastChange = millis();
       DIGIT_UTIL.startIndicator(tag);

       if (operationType == SOURCE_VOLTAGE) {
         Serial.println("Start auto calibration of dac non linearity in voltage source mode");
         V_CALIBRATION.startAutoCal();
       } else {
         Serial.println("Start auto calibration of dac non linearity in current source mode");
         C_CALIBRATION.startAutoCal();
       }
      
    } else if (tag == BUTTON_DAC_ZERO_CALIBRATE) {
       if (timeSinceLastChange + 1000 > millis()){
        return valueToReturnIfTooFast;
       } 
       Serial.println("Start zero calibration of dac....");
       DIGIT_UTIL.startIndicator(tag);

       timeSinceLastChange = millis();
       startNullCalibration();
    }
    #endif  // USE_SIMULATOR
    
    return tag;
}



void loopMain()
{
 
  
  handleAutoNull();
  operationType = getOperationType();
  
  if (nullCalibrationDone2) { 
    //if (!V_CALIBRATION.autoCalDone) {
    //  V_CALIBRATION.startAutoCal();
    //}
    RAM.startLog();
  }

  GD.__end();

  // Auto range current measurement while sourcing voltage. 
  // Note that this gives small glitches in voltage.
  // TODO: Find out how large glitches and if it's a real problem...
  if (operationType == SOURCE_VOLTAGE) {
      handleAutoCurrentRange();
  }

  #ifndef SAMPLING_BY_INTERRUPT 
    handleSampling(); 
  #endif


  if (functionType == SOURCE_SWEEP) {
    FUNCTION_SWEEP.handle();
  }
  
  disable_ADC_DAC_SPI_units();
  GD.resume();
  GD.Clear();
  renderMainHeader();

  if (showSettings == false) {
    renderUpperDisplay(operationType, functionType);  
    detectGestures();
    if (!gestureDetected) {
      int tag = checkButtons();
      
      // TODO: don't need to check buttons for inactive menus or functions...
      MAINMENU.handleButtonAction(GD.inputs.tag);
      FUNCTION_PULSE.handleButtonAction(tag);

      // Use raw tags for sweep function. TODO: update checkBUttons to handle holding etc.
      FUNCTION_SWEEP.handleButtonAction(GD.inputs.tag); 
    }

    handleWidgetScrollPosition();
    displayWidget();  
    handleMenuScrolldown();
  }

  
  
  PUSHBUTTONS.handle();
    PUSHBUTTON_ENC.handle();

  
  if (V_CALIBRATION.autoCalInProgress or C_CALIBRATION.autoCalInProgress) {
    notification("Auto calibration in progress...");
  }
  if (autoNullStarted && !nullCalibrationDone2) {
    notification("Wait for null adjustment...");
  }

  if (anyDialogOpen()) {
    bluredBackground();
    if (SOURCE_DIAL.isDialogOpen()){
      SOURCE_DIAL.checkKeypress();
      SOURCE_DIAL.handleKeypadDialog();
    } else if (LIMIT_DIAL.isDialogOpen()) {
      LIMIT_DIAL.checkKeypress();
      LIMIT_DIAL.handleKeypadDialog();
    }

  }

  GD.swap(); 
  GD.__end();
}

void closedPulse(OPERATION_TYPE t) {
  
}

void closedSweep(OPERATION_TYPE t) {
  
}

void rotaryChangedDontCareFn(float changeVal) {
}

void pushButtonEncDontCareFn(int key, bool quickPress, bool holdAfterLongPress, bool releaseAfterLongPress) {
}


void closeMainMenuCallback(FUNCTION_TYPE newFunctionType) {
  
  Serial.println("Closed main menu callback");
  Serial.println("New Selected function:");
  Serial.println(newFunctionType);
  Serial.println("Old function:");
  Serial.println(functionType);

  
  Serial.flush();

  // "unregister" function to be called when rotaty encoder is detected.
  // Has to be reinitiated by the function that needs it.
  // This is to avoid unwanted stuff to happen when rotating the knob in
  // different function views.
  ROTARY_ENCODER.init(rotaryChangedDontCareFn); 
  PUSHBUTTON_ENC.setCallback(pushButtonEncDontCareFn); 

  // do a close on the existing function. It should do neccessary cleanup
  if (functionType == SOURCE_PULSE) {
    FUNCTION_PULSE.close(); // Hmmm... how to let something go in the background while showing logger ????
  } else if (functionType == SOURCE_SWEEP) {
    FUNCTION_SWEEP.close(); //Hmmm... how to let something go in the background while showing logger ????
  } else if (functionType == DIGITIZE) {
    //TODO: Use method when digitizer is moved out as separate class
    //      For now, just readjust sampling speed
    SMU[0].setSamplingRate(20); //TODO: Should get back to same as before, not a default one
  } 
 

  // The newly selected function...
  if (newFunctionType == SOURCE_PULSE) {
    FUNCTION_PULSE.open(operationType, closedPulse);
  } else if (newFunctionType == SOURCE_SWEEP) {
    FUNCTION_SWEEP.open(operationType, closedSweep);
  }
  else if (newFunctionType == SOURCE_DC_VOLTAGE) {   
    //disable_ADC_DAC_SPI_units();
    GD.__end();
    ROTARY_ENCODER.init(rotaryChangedVoltCurrentFn);
    PUSHBUTTON_ENC.setCallback(pushButtonEncInterrupt); 

    if (SMU[0].operationType == SOURCE_VOLTAGE) {
      // If previous SMU operation was sourcing voltage, use that voltage
      if (SMU[0].fltSetCommitVoltageSource(SMU[0].getSetValue_micro(), true)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
      if (SMU[0].fltSetCommitCurrentLimit(SMU[0].getLimitValue_micro(), true)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
    } else {
      // If previous SMU operation was sourcing current, use a predefined voltage
      if (SMU[0].fltSetCommitVoltageSource(SETTINGS.setMilliVoltage*1000, true)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
      if (SMU[0].fltSetCommitCurrentLimit(SETTINGS.setCurrentLimit*1000, true)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
    }
     GD.resume();
  }
  else if (newFunctionType == SOURCE_DC_CURRENT) {
      ROTARY_ENCODER.init(rotaryChangedVoltCurrentFn);
          PUSHBUTTON_ENC.setCallback(pushButtonEncInterrupt); 


    //disable_ADC_DAC_SPI_units();
    GD.__end();
    if (SMU[0].operationType == SOURCE_CURRENT) {
      // If previous SMU operation was sourcing current, use that current
      if (SMU[0].fltSetCommitCurrentSource(SMU[0].getSetValue_micro()/*SETTINGS.setMilliAmpere*/)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
      if (SMU[0].fltSetCommitVoltageLimit(SMU[0].getLimitValue_micro(), true)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
    } else {
      // If previous SMU operation was sourcing voltage, use a predefined current
      if (SMU[0].fltSetCommitCurrentSource(SETTINGS.setMilliAmpere*1000/*SETTINGS.setMilliAmpere*/)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
      if (SMU[0].fltSetCommitVoltageLimit(SETTINGS.setVoltageLimit*1000, true)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
    }

    GD.resume();
  }
  else if (newFunctionType == DATALOGGER) {
     //ROTARY_ENCODER.init(TRENDGRAPH.rotaryChangedFn);
     ROTARY_ENCODER.init(LOGGER.rotaryChangedFn);
         PUSHBUTTON_ENC.setCallback(LOGGER.rotarySwitchFn); 


  }
  functionType = newFunctionType;

  
}

void fltCommitCurrentSourceAutoRange(float uV, bool autoRange) {
   // auto current range when sourcing current
   if (autoRange) {
      if (abs(uV) < 8000 ) {  // TODO: should theoretically be 10mA full scale, but there seem to be some limitations... i.e. reference volt a bit less that 5V...
        current_range = MILLIAMP10;
        SMU[0].setCurrentRange(current_range, operationType);
      } else {
        current_range = AMP1;
        SMU[0].setCurrentRange(current_range,operationType);
      }
      

      if (SMU[0].fltSetCommitCurrentSource(uV)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
   }
   else {
          if (SMU[0].fltSetCommitCurrentSource(uV)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);

   }



    
}

void closeSourceDCCallback(int set_or_limit, bool cancel) {

  if (cancel) {
      Serial.println("Closed SET/LIMIT dialog by cancel");
    return;
  }
  GD.__end();
  disable_ADC_DAC_SPI_units();
  if (set_or_limit == SET) {
    float mv = SOURCE_DIAL.getMv(); // TODO: get current value from another place ?
    Serial.print("Closed SET dialog, value=");
    Serial.println(mv);
    if (operationType == SOURCE_VOLTAGE) {
       if (SMU[0].fltSetCommitVoltageSource(mv * 1000, true)) printError(_PRINT_ERROR_VOLTAGE_SOURCE_SETTING);
    } else {
      fltCommitCurrentSourceAutoRange(mv * 1000, false);
    }
  }
  if (set_or_limit == LIMIT) {
    float mv = LIMIT_DIAL.getMv(); // TODO: get current value from another place ?
    Serial.print("Closed LIMIT dialog, value=");
    Serial.println(mv);
    if (operationType == SOURCE_VOLTAGE) {
     if (SMU[0].fltSetCommitCurrentLimit(mv * 1000, _SOURCE_AND_SINK)) printError(_PRINT_ERROR_CURRENT_SOURCE_SETTING);
    } else {
     if (SMU[0].fltSetCommitVoltageLimit(mv * 1000, _SOURCE_AND_SINK)) printError(_PRINT_ERROR_CURRENT_SOURCE_SETTING);
    }
  }
  GD.resume();
}


