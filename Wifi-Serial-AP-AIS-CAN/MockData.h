#ifndef _MockData_H_
#define _MockData_H_

// ── Mock mode ─────────────────────────────────────────────────────────────────
// 0 = full real    — all data from hardware; mock values never used
// 1 = partial mock — mock fills in for each sensor group until it initialises;
//                    once a sensor provides a valid reading it takes over permanently
// 2 = full mock    — all values from constants below (useful for UI development
//                    and demos away from the boat)
#define MOCK_MODE 1

// ── Single source of truth for all simulation values ─────────────────────────
// Edit here to change the simulated vessel state.
//
// Scenario: Arisaig Harbour, NW Highlands — running due west at 5.5 kts,
//           10 kts of wind from due east (dead downwind run).

#define MOCK_LATITUDE          (56.0 + 54.50 / 60.0)   // 56°54.50'N
#define MOCK_LONGITUDE        -(5.0  + 51.00 / 60.0)   // 005°51.00'W
#define MOCK_HEADING           270.0   // due west
#define MOCK_COG               270.0
#define MOCK_SOG               5.5     // kn
#define MOCK_STW               5.3     // kn
#define MOCK_TWD               90.0    // wind FROM due east
#define MOCK_TWS               10.0    // kn
#define MOCK_TWA               180.0   // dead downwind
#define MOCK_AWS               4.5     // kn  (TWS - SOG, tailwind reduces apparent)
#define MOCK_AWA               180.0
#define MOCK_WATER_DEPTH       4.5     // m
#define MOCK_WATER_TEMP        11.0    // °C  typical Scottish sea surface
#define MOCK_RUDDER_POSITION  -5.0     // degrees, slight port on a downwind run
#define MOCK_TRIP_LOG          8.5     // NM
#define MOCK_TOTAL_LOG         2341.8  // NM
#define MOCK_GPS_TIME          43200.0 // s  (12:00:00 UTC)
#define MOCK_VARIATION        -4.5     // degrees, westerly declination — NW Scotland

// Local sensor mocks — used in MOCK_MODE 2 (full mock) and in MOCK_MODE 1 until
// the real sensor initialises (DS18B20 returns a valid reading / ADC reads > 5 V).
#define MOCK_FRIDGE_TEMP       4.2     // °C
#define MOCK_BATTERY_VOLTAGE   12.8    // V

#endif // _MockData_H_
