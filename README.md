# esp32-cam-daytime-timelapse
A ESP32-CAM simple, power efficient and functional program for making timelapses during day with automatic exposure.

I made this code based on other codes, the code works just fine, and i couldnt find anything that does exactly what i wanted, that is make a yearly timelapse.

How it works:
The ESP32-CAM boots, turns on the sensor and check the light, if the light is too low (its night) it will start deep sleep, else (its day) it will take a picture.
Everything is automatic done by the sensor itself, beacuse of that i put a 3 seconds delay, so it can ajust itself. When the picture is taken, its automaticly saves in the SD card and sleeps for the determinated period of time, then it reapeats the cicle.
In case of a error it reboots to try again.
All the photos are stored with automatic counting.

Im new in this, but the code just works.
If you have any suggestions / improvements let me know :)
