#ifndef _LocalSensors_H_
#define _LocalSensors_H_

// Hardware-measured data local to this device.
// NMEA2000-derived data lives in BoatData.h.
// To add a sensor: add a field here, populate it in the relevant read function,
// and it will automatically appear in the /data JSON endpoint.
struct tLocalSensors {
  float FridgeTemp     = 0;   // DS18B20 on GPIO 13
  float BatteryVoltage = 0;   // ADC on GPIO 34 via voltage divider
  // Add future sensors here, e.g.:
  // float CabinTemp        = 0;
  // float EngineCoolantTemp= 0;
  // float OilPressure      = 0;
  // float StarterVoltage   = 0;
};

#endif // _LocalSensors_H_
