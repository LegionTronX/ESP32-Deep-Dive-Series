**Watchdog Implementation for System Safety**

**Introduction**

The Watchdog Timer (WDT) serves as a critical component in ensuring system reliability and safety by monitoring the execution of firmware code. It acts as a failsafe mechanism, resetting the system in case of software failures or hangs. In this project, we explore the implementation of the Watchdog Timer on an ESP32 microcontroller to enhance system robustness.

**Purpose**

The purpose of this project is to showcase the practical utilization of the Watchdog Timer for ensuring system integrity and preventing software lockups. By incorporating the Watchdog functionality into our firmware, we aim to demonstrate its effectiveness in maintaining system stability and mitigating potential software faults.

**Functionality**

Watchdog Initialization
The Watchdog Timer is configured with a specific timeout period, after which it triggers a system reset if not reset by the firmware. This initialization process ensures that the Watchdog is operational and ready to monitor the system's execution.

**Interrupt Handling**

The firmware sets up interrupt handlers to detect certain events or conditions, such as button presses or sensor readings. In the event of an unexpected behavior or software hang, the Watchdog Timer can be reset within the interrupt handler to prevent a system reset.

**System Monitoring**

The Watchdog continuously monitors the execution of the firmware code. If the firmware fails to reset the Watchdog within the specified timeout period, it triggers a system reset, restoring the system to a known state and preventing prolonged software hangs.

**Conclusion**

This project highlights the importance of incorporating Watchdog functionality into embedded systems to enhance reliability and safety. By implementing Watchdog monitoring and reset mechanisms, developers can ensure that their systems remain resilient to software faults and maintain operational integrity under various conditions.

https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/wdts.html

//interrupts

https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gpio.html
