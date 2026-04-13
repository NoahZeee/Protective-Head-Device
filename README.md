# Protective Head Device

## Overview
This device is designed to assist first responders in event of natural disaster and/or fires. It is a wearable device that can attach to a hat or helmet.

It works by providing the user with feedback on nearby objects in potential low-visbility environments (eg. filled with smoke), giving live ambient temperature readings, and communication with the team (situated outside) or centralized unit

# Features

## Nearby Object Warning
The ultrasonic sensor scans for nearby objects, and when one is detected, haptic motors vibrate lightly on the device to warn the user before they hit their head on something, the frequency of the vibration change base on the proximity to an object.

## Live Temperature Reading
OLED screen will display temperature readings to alert user of any potenitally dangerous temperature levels, it would also display any messages being sent from the central unit (webserver).

## Wireless Communcation
Using the esp32's wifi capability and a hotspot host, the user can communicate with other first reponsders if they are isolated, need help, or found a survivor that needs immediate medical attention. The esp (helmet) would have buttons signaling their state (such as backup request and emergency) which would be sent to the central webserver (a hotspot host such as a laptop) that will display the state. The central unit would also be able to send any text messages to the screen of the esp.
