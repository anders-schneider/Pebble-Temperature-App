# Pebble-Temperature-App

Pebble Watch App assignment for CIT 595, Spring 2015

This project involved coordinating messages between a Pebble watch, a server, and an Arduino microcontroller (serially connected to the server). The app allows a user to view the current temperature, view the average/high/low temperatures over the last hour, switch between Celsius and Fahrenheit, put the Arduino into standby mode, and initiate a "dance mode" display on the Arduino.

The watch communicates with the server via HTTP requests (this part was implemented in Javascript) and processes incoming messages and other events (button presses, e.g.). The server is a multi-threaded C++ program which listens for HTTP requests from the watch and incoming temperature data via a serial connection with the Arduino microcontroller. The Arduino communicates directly with the server, sending messages with the current temperature in Celsius and responding to requests to go into standby or dance mode.

More information is available in the Technical and User Documentation pdfs included in this directory.