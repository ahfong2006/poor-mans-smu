

class CalibrationClass {

  private:
  unsigned long timeSinceLastChange;
  
 // float meas_adc[100] = {0.00, 099.90, 499.60, 899.34, 0999.26, 1099.25, 1498.90, 1998.63, 2098.61, 2498.41, 2997.94, 3097.94, 3197.20, 3299.20, 3501.65, 3603.22, 3705.30, 3805.94, 4006.41, 4205.52, 4504.10, 5002.26, 5501.45, 5801.24, 6001.18, 6201.00, 6500.93, 7000.84, 7500.81, 8000.78};
 // float set_adc[100]  = {0.04, 100.01, 499.91, 900.03, 0999.98, 1100.02, 1499.94, 1999.95, 2099.98, 2500.02, 2999.99, 3099.96, 3198.95, 3300.00, 3500.02, 3599.98, 3700.01, 3799.97, 4000.03, 4200.01, 4500.04, 4999.81, 5499.80, 5799.81, 5999.98, 6199.78, 6499.80, 6999.78, 7499.78, 7999.75};

 float meas_adc[100] = {-5999.46, -4999.58, -3999.63, -2999.71, -1999.75, -0999.70, -499.85, -099.87, 0.00, 100.09, 500.07, 899.73, 999.66, 1199.78, 1499.94, 1999.90, 2499.85, 3000.60, 4002.33, 4502.80, 4801.93, 4900.34, 4999.55, 5494.60, 5694.50, 5998.30};
  float set_adc[100] = {-6000.00, -5000.00, -4000.00, -3000.00, -2000.00, -1000.00, -500.00, -100.00, 0.00, 100.00, 500.00, 900.00, 1000.00, 1200.00, 1500.00, 2000.00, 2500.00, 3000.00, 4000.00, 4500.00, 4800.00, 4900.00, 5000.00, 5500.00, 5700.00, 6000.00};
  int adc_cal_points = 8 + 2 + 16;
public:
  float nullValue;
  bool useCalibratedValues = true;

  bool toggleCalibratedValues();
  float adjust(float v);
  void toggleNullValue(float v);
  void renderCal(int x, int y, float valM, float setM, bool cur);
  void init();
};
