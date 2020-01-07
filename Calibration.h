#include "operations.h"

class CalibrationClass {

  union cvt {
    float val;
    unsigned char b[4];
  };

  private:
  OPERATION_TYPE operationType;
  unsigned long timeSinceLastChange;

  float set_adc[100]; 
  float meas_adc[100];
  int adc_cal_points=0;

  float set_dac[100];
  float meas_dac[100];
  int dac_cal_points=0;
  
  float dacGainCompPos;
  float dacGainCompNeg;
  float adcGainCompPos;
  float adcGainCompNeg;

  int ea_dac_gain_comp_pos;
  int ea_dac_gain_comp_neg;
  int ea_adc_gain_comp_pos;
  int ea_adc_gain_comp_neg;
public:
  void floatToEeprom(int address, float f);
  float floatFromEeprom(int address);
  
  float getDacGainCompPos();
  void adjDacGainCompPos(float val);
  float getDacGainCompNeg();
  void adjDacGainCompNeg(float val);

  float getAdcGainCompPos();
  void adjAdcGainCompPos(float val);
  float getAdcGainCompNeg();
  void adjAdcGainCompNeg(float val);
  
  float nullValue[2];
  bool useCalibratedValues = true;
  
  bool toggleCalibratedValues();
  float dac_nonlinear_compensation(float v);
  float adc_nonlinear_compensation(float milliVolt);
  void toggleNullValue(float v, CURRENT_RANGE current_range);
  bool nullValueIsSet(CURRENT_RANGE current_range);
  void renderCal(int x, int y, float valM, float setM, bool cur, bool reduceDetails);
  void init(OPERATION_TYPE operation_type);
};

extern CalibrationClass V_CALIBRATION;
extern CalibrationClass C_CALIBRATION;
