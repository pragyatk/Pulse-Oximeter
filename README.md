# Pulse Oximeter

## Background
Pulse oximetry is used to assess blood oxygenation levels without taking a blood sample and is the most common method to measure peripheral oxygen saturation, termed $\mathrm{SpO}_2$, in the bloodstream. This vital sign is an important metric for healthcare providers in administering further treatments, such as supplemental oxygen. Several studies have shown that for darker skin pigmentation, the pulse oximeter may give inaccurate or inconsistent readings. This is because melanin, the molecule that gives our skin its pigment, both absorbs and scatters light, leading to attenuation of the light intensity at the photodetector of the pulse oximeter. As pulse oximetry is an optical method, any alterations to the path taken by the oximeter light-emitting diodes (LEDs), such as an increase in melanin concentration in the skin, can lead to measurement inaccuracies.

## Solution
A pulse oximeter was constructed from scratch, with a 3D-printed enclosure and probe as well as a printed circuit board. The circuit includes hardware filtering and gain via operational amplifiers, skin sensing via a QTI sensor, red and IR LEDs, a photodetector, an OLED screen, and an Arduino Nano ESP32 with Wi-Fi connectivity. 

Below are the major steps in the operation of the device:
1. Readings from the QTI sensor are used to determine the skin tone of the subject.
2. Based on the skin tone type identified, the red LED brightness is modulated.
3. Using the selected LED brightness, red and IR PPG signals are obtained. A multiplexing approach is used to rapidly switch between two duplicate analog filtering circuits to minimize crosstalk.
4. The red and IR PPG data are sent over to a server running on a virtual machine via an HTTPS POST request and Wi-Fi communication.
5. The server receives the data to filter and analyze the AC and DC components of the signals, resulting in the calculation of an abitrary R-value.
6. The R-value is then mapped to an $\mathrm{SpO}_2$ value according to a calibration curve empirically determined.
7. The $\mathrm{SpO}_2$ value and heart rate are received by the Arduino Nano ESP32 via an HTTPS GET request and are displayed on the OLED screen.

## Code
Two code files are provided; an `.ino` Arduino script and a `.py` Python server script. The Arduino script is uploaded to the Arudino Nano ESP32, whereas the Python script is run on a virtual machine hosted externally. 