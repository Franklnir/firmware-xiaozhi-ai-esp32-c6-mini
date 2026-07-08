# ESP32-C6 Super Mini

Pin mapping for ESP32-C6FH4 Super Mini with INMP441 microphone and MAX98357 I2S amplifier.

| Signal | ESP32-C6 GPIO | INMP441 | MAX98357 |
| --- | ---: | --- | --- |
| 3V3 | 3V3 | VDD | VIN |
| GND | GND | GND | GND |
| I2S BCLK/SCK | GPIO6 | SCK | BCLK |
| I2S LRCK/WS | GPIO7 | WS | LRC |
| I2S mic data | GPIO20 | SD | - |
| I2S speaker data | GPIO21 | - | DIN |

Tie INMP441 `L/R` to `GND` so the microphone uses the left I2S slot.

The firmware app status LED uses the onboard WS2812 RGB LED on GPIO8. GPIO8 is
also a strapping pin, so do not add external pull-up or pull-down circuitry to
it. The plain onboard LED on GPIO15 is used as a power/charge status LED by this
board profile.

## Battery, charging LED, and soft power-off

`BAT+` / `BAT-` only connects the Li-ion/LiPo cell to the board power and
charger circuit. The green onboard battery LED is controlled directly by the
charger IC, not by ESP32 firmware:

- USB charging: green battery LED on.
- Battery connected and not charging: green battery LED off.
- No battery connected: green battery LED may blink.

Firmware reads charging status from GPIO2 by default. Solder the charger
`CHG` / `STAT` signal to GPIO2:

```c
#define BATTERY_CHARGING_GPIO GPIO_NUM_2
#define BATTERY_CHARGING_ACTIVE_LEVEL 0
```

With that wiring, GPIO15 mirrors charging state: on while charging, off when not
charging. The firmware also exposes `self.battery.get_status`, so voice commands
such as "status baterai" or "apakah sedang charging" can report `charging` or
`not_charging`. If GPIO2 is not wired to `CHG` / `STAT`, the input pull-up reads
as `not_charging`.

Real battery on/off needs a hardware switch in series with `BAT+`, or a load
switch/latching power circuit. The firmware also supports a soft power-off
through deep sleep, but ESP32-C6 cannot use the BOOT button on GPIO9 as a
deep-sleep wake pin. Wire a separate button from an RTC-capable GPIO to GND, for
example GPIO0 to GND, then set:

```c
#define DEEP_SLEEP_WAKE_GPIO GPIO_NUM_0
#define DEEP_SLEEP_WAKE_ACTIVE_LEVEL 0
```

After that, hold BOOT for 3 seconds to enter soft power-off, then press the wake
button to turn it back on. If `DEEP_SLEEP_WAKE_GPIO` is left as `GPIO_NUM_NC`,
long-press BOOT logs a warning and does not power off, so the board cannot get
stuck asleep without a wake path.
