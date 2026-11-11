# esp32-ina219-battery-monitor-mqtt — publish battery voltage, current, power and mAh over MQTT

IoT example by [Fluidwire](https://fluidwire.com/).

An ESP32 sketch that reads an INA219 high-side current sensor in series with a battery, solar panel or load. It samples once a second, integrates current into mAh and mWh (coulomb counting), estimates a rough state of charge from voltage, and publishes a JSON reading to MQTT every 10 seconds. Handy for off-grid and solar thesis prototypes in the Philippines where you need to know how a pack really behaves.

## Required hardware

- 1x ESP32 dev board
- 1x INA219 breakout (Adafruit or generic GY-219)
- A single-cell Li-ion/LiPo pack (or solar + charger setup) and a load
- WiFi and an MQTT broker (public `broker.hivemq.com` works for testing)

## Required libraries

- **Adafruit INA219** by Adafruit (Library Manager, installs Adafruit BusIO)
- **PubSubClient** by Nick O'Leary (Library Manager)
- `WiFi` and `Wire` are built into the ESP32 Arduino core

## Wiring

| INA219 | ESP32 |
|--------|-------|
| VCC | 3V3 |
| GND | GND (shared with battery -) |
| SDA | GPIO21 |
| SCL | GPIO22 |

Measurement is high-side: battery `+` to `Vin+`, `Vin-` to the load/charger `+`, battery `-` straight to the load/charger `-`. With this orientation, positive current means discharging and negative means charging.

## Setup

1. Copy `config.h.example` to `config.h` and fill in WiFi, broker and your pack's rated capacity.
2. Choose the calibration call in `setup()` that matches your current range.
3. Flash and open Serial Monitor at **115200 baud**.
4. Subscribe from a PC:

```bash
mosquitto_sub -h broker.hivemq.com -t 'fluidwire/demo/battery/ina219' -v
```

## Expected serial output

```
[ina219] ready
[wifi] connecting to your-wifi-ssid
.....
[wifi] IP: 192.168.1.50
[mqtt] connecting... ok
[mqtt] published {"bus_v":3.91,"current_ma":212.4,"power_mw":830.5,"soc_pct":71,"used_mah":12.35,"net_mwh":47.2,"uptime_s":10}
```

## Troubleshooting

- **`ina219 not found`:** check SDA/SCL, power, and the address (0x40 by default; A0/A1 solder pads change it).
- **Current reads 0:** the load must pass through `Vin+` to `Vin-`; bridging them bypasses the shunt.
- **Sign is flipped:** swap the `Vin+` and `Vin-` connections.
- **SoC looks off under load:** voltage sags under current, so the linear estimate is only indicative. Use resting voltage or a dedicated fuel gauge for accuracy.
- **mAh resets on reboot:** counters live in RAM; store them in NVS if you need persistence.

## Security note

Never commit `config.h`; it is git-ignored. Public brokers are readable by anyone, so use a private broker with TLS and credentials for real deployments. The INA219 is for low-voltage DC only (max 26V bus on the breakout), never mains.

## License

MIT — see [LICENSE](LICENSE).

## Got stuck?

Drop by [fluidwire.com](https://fluidwire.com/) — happy to help.
