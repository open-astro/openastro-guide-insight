# Imaging gear in the guide log

OpenAstro PHD2 and OpenAstro Guider read the imaging gear from AlpacaBridge and write it into the guide log. Guide Insight reads these lines for its assessment and the Session details section. Stock PHD2 logs don't have them, and everything still works without them.

This is the format all three apps agree on. The guiders' side is tracked in [openastro-phd2#33](https://github.com/open-astro/openastro-phd2/issues/33) and [openastro-guider#82](https://github.com/open-astro/openastro-guider/issues/82).

## Header lines

The guider writes these lines in the header of each guiding and calibration section, next to `Equipment Profile =`:

```
Equipment source = AlpacaBridge 192.168.1.20:11111
Imaging telescope = Sky-Watcher EQ6-R Pro, Focal length = 250 mm, Aperture = 51 mm, Focal ratio = f/4.9
Imaging camera = ZWO ASI2600MM Pro, Sensor = IMX571, Sensor type = Monochrome, Pixel size = 3.76 x 3.76 um, Sensor size = 6248 x 4176 px, Binning = 1x1, Pixel scale = 3.10 arc-sec/px, Gain = 100, Offset = 50, Readout mode = High Gain, Cooler = On, Sensor temp = -10.0 C
Filter wheel = ZWO EFW 7x36, Filter = Ha, Position = 3 of 7, Focus offset = 0
Rotator = ZWO CAA, Position = 92.3 deg, Mechanical position = 90.0 deg, Reverse = false
Focuser = ZWO EAF, Position = 12345 steps, Temperature = 8.4 C
Observing conditions = WeeWX, Seeing = 2.1 arc-sec, Star FWHM = unknown, Wind speed = 3.2 m/s, Wind gust = 5.0 m/s, Temperature = 8.9 C, Humidity = 71 %, Dew point = 3.9 C, Pressure = 1012 hPa, Cloud cover = 0 %, Sky quality = 21.2 mag/arcsec^2
Guide train = Separate guide scope, Guide/imaging focal length ratio = 0.48
```

- Each line is `Name, Key = value, Key = value, ...`. Values never contain `, `.
- A value the driver doesn't report is written as `unknown`. The key is still written.
- A device type the rig doesn't have gets no line.
- When the guider can't tell which camera takes the images, it writes `Imaging camera = ambiguous (n cameras)` with no values.

## Events during guiding

```
INFO: EQUIPMENT Filter = OIII, Position = 4 of 7, Focus offset = 12
INFO: EQUIPMENT Rotator position = 180.0 deg
INFO: EQUIPMENT Focuser position = 12410 steps, Temperature = 6.1 C
INFO: IMAGING EXPOSURE start, Duration = 300.0 s, Filter = Ha
INFO: IMAGING EXPOSURE end, Duration = 300.0 s
INFO: CONDITIONS Seeing = 2.4 arc-sec, Wind speed = 5.1 m/s, Wind gust = 8.0 m/s, Temperature = 7.2 C, Humidity = 78 %
```

The guider writes `CONDITIONS` once a minute while guiding, and only when there is an ObservingConditions device.

## What Guide Insight uses today

| Line | Used for |
|---|---|
| `Imaging telescope` | Session details; its focal length gives the imaging pixel scale when the camera line doesn't |
| `Imaging camera` | The assessment's verdict, which compares the total RMS with this pixel scale (or one worked out from the pixel size, binning and focal length); Session details |
| `Filter wheel` | Session details |
| `INFO: CONDITIONS` | The assessment names the wind during the roughest 5 minutes |
| `INFO: GA Result` (stock PHD2) | The Guiding Assistant's Total HPF-RMS is the sky's floor the assessment compares guiding with |

Guide Insight doesn't use the other lines yet. They're specified now so that logs written today can be used later, for example to grade each imaging exposure.
