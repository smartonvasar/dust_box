# Dust box prototype kit


_Fork and BME280 GYBMEP version of the original dust_box._


Cloud support: https://iotguru.cloud

Support forum: https://forum.iotguru.live/viewtopic.php?f=17&t=6

![PM10 measurement](https://github.com/IoTGuruLive/dust_box/blob/master/images/sds011-pm10.png)

![Box](https://github.com/IoTGuruLive/dust_box/blob/master/images/box.jpg)

- based on SDS-011 dust sensor and BME280/BME680 environmental sensor
- measure PM10, PM2.5, temperature, humidity and pressure and send it to our cloud

Live example: https://iotguru.live/device/u_m5VpzbejH2jIowKWYR6g

## Gerber ZIP to print the PCB

![Gerber PBC](https://github.com/IoTGuruLive/dust_box/blob/master/images/pcb_top.png)

* gerber/gerber.zip

## 3D printable model

![Box bottom](https://github.com/IoTGuruLive/dust_box/blob/master/images/3d_model_bottom.png)

* The box bottom: model/Dust-SDS011-bottom.FCStd and model/Dust-SDS011-bottom.stl

![Box top](https://github.com/IoTGuruLive/dust_box/blob/master/images/3d_model_top.png)

* The box top: model/Dust-SDS011-top.FCStd and model/Dust-SDS011-top.stl

## Arduino IDE

First of all, in the preferences add the https://arduino.esp8266.com/stable/package_esp8266com_index.json to the list of Additional board managers.

Second, install the `The IoT Guru integration` library in the library manager.

## Wiring

| WEMOS | BME280 | SDS011 |
|-------|--------|--------|
| D1    | SCL    |        |
| D2    | SDA    |        |
| 5V    | VIN    | 5V     |
| GND   | GND    | GND    |
| D3    |        | RXD    |
| D4    |        | TXD    |

