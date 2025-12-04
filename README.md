# IoT Light Sensor and Switch Prototype

## Project Overview
This project is a prototype that simulates the behavior of a light sensor and a switch controlling an LED. The light sensor has a configurable threshold, and the push button cycles through three states: **Off**, **On**, and **Sensor-Controlled**. The system allows both local and remote control of the LED, providing a hands-on exploration of IoT concepts.

## Features
- Configurable light sensor threshold (LDR)  
- Push button with three operating modes: Off, On, Sensor-Controlled  
- LED visual feedback  
- Integration with **Ubidots** and **AWS IoT Core** via **MQTT**  
  - Remote device control  
  - Status monitoring  
  - Email notifications based on sensor or switch events  

## Hardware Components
- ESP32 microcontroller with OLED display  
- LDR (Light Dependent Resistor) sensor  
- Push button  
- Red LED  
- Resistors  
- Protoboard, jumper wires, and cables  

## Software Integration
The project demonstrates connectivity and integration with cloud IoT platforms:  
- **Ubidots**: Easy and fast integration, suitable for industrial applications  
- **AWS IoT Core**: Emphasizes security, reliability, and deep integration with the AWS ecosystem  

## Learning Outcomes
Through this project, practical insights were gained on:  
- Communication challenges between embedded devices and cloud platforms  
- Real-time monitoring and control of IoT devices  
- Potential applications for scalable and more complex IoT projects
