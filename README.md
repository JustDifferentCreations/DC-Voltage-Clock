# DC Voltage Clock

A clock that tells time with three vintage Emico analog voltmeters in a dark wood case. A custom circuit card drives each needle to its place on the dial: one meter for the hour, one for the tens of minutes and one for the minutes.

Designed by [Just Different Creations](https://justdif.com) in Lufkin, Texas. See the [finished clock](https://justdif.com/#builds) and its [circuit card](https://justdif.com/pcb-projects/) on JustDif.com.

## How it works

- An **ATtiny84A** reads the time from a **DS3231** real-time clock over I2C (bit-banged on the USI peripheral). A coin cell keeps the DS3231 running when power is off.
- Each meter is driven by a PWM output, so the needle position is set by the duty cycle:
  - **Hours:** OC0A (PB2), 8-bit PWM
  - **Tens of minutes:** OC0B (PA7), 8-bit PWM
  - **Minutes:** OC1B (PA5), 10-bit PWM
- Two buttons set the time: **PB0** adds an hour and **PB1** adds a minute (internal pull-ups).

## Files

| File | What it is |
| --- | --- |
| `main.c` | Clock and meter driver code |
| `DS3231.c/.h`, `USI_I2C_Master.c/.h` | RTC and I2C helpers (see Credits) |
| `DC_VC_R1.1.pdf` | Schematic for circuit card Rev 1.1 |

## Building and flashing

- MPLAB X IDE with the XC8 compiler
- Device: ATtiny84A at 1 MHz (`F_CPU 1000000UL`)
- Program through the 6-pin ISP header on the card (+5V, GND, MISO, SCK, RST, MOSI)

## Credits

`DS3231.c`, `DS3231.h`, `USI_I2C_Master.c` and `USI_I2C_Master.h` come from [Pesc0/Clock](https://github.com/Pesc0/Clock), which is licensed under GPL-3.0.

## License

GPL-3.0. See [LICENSE](LICENSE).
