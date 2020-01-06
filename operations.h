

//#define SOURCE_CURRENT 100
//#define SOURCE_VOLTAGE 101

#define SOURCE_DC 150
#define SOURCE_PULSE 151
#define SOURCE_SWEEP 152
#define SETTINGS 153


#define SET  200
#define LIMIT  201

#ifndef OPERATIONS_H
#define OPERATIONS_H
enum OPERATION_TYPE {
  SOURCE_VOLTAGE,
  SOURCE_CURRENT
};
enum CURRENT_RANGE {
  AMP1,
  MILLIAMP10
};


#endif
