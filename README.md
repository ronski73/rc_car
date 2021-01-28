# rc_car
Modded RC car (based on the Nikko VaporizR2 Nano) with a Wemos ESP8266 Pro mini and Wemos motorshield V2.0.
For control a remote app was created with Roboremo.

Things worth mentioning:
- Watertight concealment is preseved due to small size of the WEMOS pcbs
- Control via Roboremo app (only Android) https://play.google.com/store/apps/details?id=com.hardcodedjoy.roboremofree&hl=en_US&gl=US
- Car sets up its own AP (see source for info)
- OTA update!
- Remote debugging via putty!

To set this up the first time:
- Copy the roboremo file in the RoboRemoFile directory to your mobile device and import it in Roboremo
- Open the ino file in the RoboRemoOta directory with the Arduino IDE and compile and download it into the ESP8266
- Turn the ESP8266 on
- Wait until the LED start flashing
- Connect your mobile device to the WIFI 'rc_car'
- Enter credentials (see source)
- Open the Roboremo app and connect to the IP address 192.168.0.1
- After succesfull connection, you can control the rc car
