#ifndef _LocalSensors_H_
#define _LocalSensors_H_

// Hardware-measured data local to this device.
// NMEA2000-derived data lives in BoatData.h.
// To add a sensor: add a field here, populate it in the relevant read function,
// and it will automatically appear in the /data JSON endpoint.
struct tLocalSensors {
  float ExhaustTemp    = 0;   // DS18B20 on GPIO 13 (1-Wire)
  float FridgeTemp     = 0;   // PT1000 via MAX31865 (not yet wired — stays 0 until hardware added)
  float BatteryVoltage = 0;   // ADC on GPIO 34 via voltage divider
};

#endif // _LocalSensors_H_
