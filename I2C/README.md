**I2C Communication for Servo Control**


**Introduction**

The Inter-Integrated Circuit (I2C) communication protocol is widely used in today's technology for inter-device communication. It facilitates communication between microcontrollers, sensors, actuators, and other peripheral devices, allowing them to exchange data seamlessly. In this project, we explore the use of I2C for controlling a servo motor based on analog input readings.

**Purpose**

The purpose of this project is to demonstrate how I2C can be utilized for real-time communication between microcontrollers. We implement a master-slave architecture where the master device reads analog sensor values and sends them to the slave device, which in turn controls a servo motor based on the received data.

**Functionality**

**I2C Master Task**

The I2C master task is responsible for:

Reading analog sensor values (ADC readings).
Formatting the data.
Transmitting the data over the I2C bus to the slave device.

**I2C Slave Task**

The I2C slave task handles:

Receiving data from the master device over the I2C bus.
Interpreting the received data.
Controlling the servo motor based on the interpreted data.

**Servo Control Logic**

The servo motor's position is controlled by varying the pulse width of the PWM (Pulse Width Modulation) signal. The ADC readings from the sensor are converted into PWM duty cycle values, which are then applied to the servo motor. By adjusting the duty cycle, we can precisely control the servo motor's position.

**Conclusion**

This project showcases the practical implementation of I2C communication for servo control. Understanding the master-slave architecture and the logic behind servo control via PWM signals provides valuable insights into embedded systems development.
<img width="1076" alt="Uppgift3 koppling" src="https://github.com/LegionTronX/ESP32-Deep-Dive-Series/assets/144809804/8b5822f1-685a-453f-9c28-64ea96a35c4a">


https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/peripherals/i2c.html
