# Test Patterns

Simple solid colors, gradients and test line patterns, could be used to test matrices for proper operation, flickering and estimate fillrate timings.

It is also used in CI test builds to check different build flags scenarios.

Should be build and uploaded as a [platformio](https://platformio.org/) project


To build with Arduino's framework use
```
pio run -t upload
```

To build using ESP32 IDF with arduino's component use
```
pio run -t upload -e idfarduino
```
