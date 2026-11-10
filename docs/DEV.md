# Dev Instructions

To compile:

```pio run -e thingpulse_wrover_modular```

To upload:

```pio run -e thingpulse_wrover_modular -t upload --upload-port /dev/ttyACM1```

To Monitor

```pio device monitor -p /dev/ttyACM1 -b 115200```

To see device list

```pio device list```