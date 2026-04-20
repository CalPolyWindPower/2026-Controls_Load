/**
 * @file LoadDemo.ino
 * @brief Manually control the actuator
 * @version 0.3.1 (1.0.1)
 * @since Winter 2026
 * @author Noah (@BobSaidHi <https://github.com/bobsaidhi>) for
 * @CalPolyWindPower <https://github.com/calpolywindpower>
 * @author Inspired by Trevor (@rover-t <https://github.com/rover-t>) at
 * @CalPolyWindPower <https://github.com/calpolywindpower>
 *
 * The Unexpected Maker ProS3[D] has one USB C port.  The baud rate is set
 * to to 115,000 below and uses the internal USB peripheral, not a separate
 * chip. Some compatible serial monitors include the Arduino IDE
 * <https://www.arduino.cc/en/software/>, VSCode "Serial" extension from
 * Microsoft
 * <https://marketplace.visualstudio.com/items?itemName=ms-vscode.vscode-serial-monitor>,or
 * PuTTY.
 *
 * To re-flash:
 * 1. Install Arduino IDE (I recommend >v2.0) from
 * https://www.arduino.cc/en/software/ if you don't have it.
 * 2. Go to the boards manager in the left sidebar and install
 * "esp32" by Espressif Systems.
 *   A. If it's missing, go to the following board URL under
 *      File > Preferences > Settings >
 *      Additional Board Manager URLS:
 *      `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
 *      (See also:
 *       https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
 * 3. Go to the library manager in the left sidebar and isntall "Adafruit BusIO" by Adafruit.
 * 4. Load the attached sketch or the latest version from GitHub:
 *    https://github.com/CalPolyWindPower/2026-Controls_Load/blob/main/demos/LoadDemo/LoadDemo.ino
 * 5. Set the board to "UM PROS3". It may show up as "ESP32 Family Device at first" or even multiple devices (Select the first one).
 * 6. Under Tools, set "USB CDC On Boot" to "Enabled"
 * 7. [Recommended] Under Tools, set "Core Debug Level" to "Info"
 * 8. Select Upload
 * 9. Open a serial terminal
 *   A. In Arduino IDE, got Tools > Serial Monitor and switch to 115200 baud
 *
 * @author Noah (@BobSaidHi <https://github.com/bobsaidhi>) for Cal Poly Wind
 * Power (@calpolywindpower <https://github.com/calpolywindpower>)
 * @see Inspired by
 * https://github.com/rover-t/CPWP2024-Controls/blob/main/ControlCode/OpenLoopController/OpenLoopController.ino
 * @see Inspired by
 * https://github.com/rover-t/CPWP2024-Controls/blob/main/ControlCode/OpenLoopController_alt/OpenLoopController_alt.ino
 * @see Based on https://github.com/CalPolyWindPower/2026-Controls_Nacelle/blob/main/demos/ActuatorDemo/ActuatorDemo.ino
 */

// MARK: Imports
#include <Arduino.h>
#include <cstdint>  // Fixed size integers
#include <Wire.h>
#include "./src/MCP23008T/MCP23008T.hpp"

// MARK: Globals

/**
 * @brief Configuration for serial (UART) communication
 * @details A namespace can be used to organize related constants, variables,
 * and objects together.
 * @see https://www.geeksforgeeks.org/cpp/namespace-in-c/
 * @details `constexpr` is similar to `#define` macros but type checked.  Macros
 * and  `constexpr` variables are guaranteed and to be evaluated at compile
 * time, and may even be embedded in the immediate field of an instruction,
 * which means they do not take up additional storage space, memory, or
 * registers at turn time.
 * @see https://en.cppreference.com/w/cpp/language/constexpr.html
 * @see https://www.sciencedirect.com/topics/computer-science/immediate-operand
 */
namespace SERIAL_CONFIG {
    constexpr long BAUD = 115200;  // BAUD rate (bits per second)
}  // namespace SERIAL_CONFIG

/**
 * @brief Configuration for the load and related pins
 * @details The standard integer types, such as int, only specify a minimum
 * size.  FEEDBACK_PIN is of the fixed width unsigned integer type `uint8_t`,
 * which is always 8 bits.  This was chosen because the `pinmode()` function
 * takes an `uint8_t` as an argument.
 */
namespace LOAD {
    /**
     * Valid addresses are 0x40, 0x42, 0x44, 0x46, 0x48, 0x4A, 0x4C, & 0x4E
     */
    constexpr MCP23008T::I2C_ADDRESS I2C_ADDRESS = MCP23008T::I2C_ADDRESS::I2C_ADDR_0x27; // A0-A2 all high

    /**
     * @brief Bitmask for the load control pins.
     * @details Only the first 6 pins (0-5) are used for load control.
     */
    constexpr uint8_t PINS_Msk = 0b0011'1111;

    MCP23008T loadDevice(I2C_ADDRESS,
                     &Wire);

    bool configureLoad() {
        // Check all addr.
        // for(MCP23008T::I2C_ADDRESS addr = MCP23008T::I2C_ADDRESS::I2C_ADDR_0x40; addr <= MCP23008T::I2C_ADDRESS::I2C_ADDR_0x4E; addr = (MCP23008T::I2C_ADDRESS)(addr + 2)) {
        //     MCP23008T tempLoad(addr, &Wire);
        // for(uint8_t addr = 0; addr <= 0b0111'1111; addr++) {
        //     MCP23008T tempLoad(addr, &Wire);
        //     if (!tempLoad.begin()) {
        //         ESP_LOGE("cL", "Failed to initialize MCP23008T device at 0x%02X",
        //             addr);
        //     } else {
        //         ESP_LOGI("cL", "Sucess init MCP23008T device at 0x%02X",
        //             addr);
        //     }
        // }


        if (!loadDevice.begin()) {
            ESP_LOGE("cL", "Failed to initialize MCP23008T device at 0x%02X",
                    loadDevice.getAddress());
        }

        // First six pins outputs, made last two inputs as that's the default
        if (!loadDevice.setIODir(0b1100'0000)) {
            ESP_LOGE("cL", "Failed to set IODIR on MCP23008T at 0x%02X",
                    loadDevice.getAddress());
            return false;
        }

        // Disable sequential operation, other settings default
        if (!loadDevice.setConfig(0b0010'0000)) {
            ESP_LOGW("cL", "Failed to set IOCON on MCP23008T at 0x%02X",
                    loadDevice.getAddress());
        }

        // Set all outputs low, note that pin 0 is inverted
        if (!loadDevice.setGPIO(0b0000'0000)) {
            ESP_LOGE("cL", "Failed to set GPIO on MCP23008T at 0x%02X",
                    loadDevice.getAddress());
            return false;
        }

        // Else: success
        return true;
    }
}  // namespace LOAD

/**
 * @brief Configuration for the LED and related pins
 * @details As we have more program storage space, and hopefully more RAM than
 * we need, the slight performance boost from the fast integer types, such ast
 * uint_fast32_t, are likely worth it for us.  These types specify a minium
 * size, but allow the compiler (actually the system/ standard libraries) to
 * substitute a type that a particular chip (micro-architecture) is more
 * optimized for.
 */
// namespace LED {
// constexpr uint8_t PIN = 15;  // LED pin
// constexpr uint_fast32_t TIME_ON_MS =
//   1000;  // Time LED stays on in milliseconds
// constexpr uint_fast32_t TIME_OFF_MS =
//   3000;  // Time LED stays off in milliseconds
// }  // namespace LED

/**
 * @details put your setup code here, to run once:
 */
void setup() {
  Serial.begin(SERIAL_CONFIG::BAUD);  // Start serial

  // The DFRobot FireBeetle 2 ESP32-C5 has one user controllable LED built in
  // #if defined(ESP32)
  //   /**
  //      * @see
  //      * https://docs.arduino.cc/language-reference/en/functions/digital-io/pinMode/
  //      */
  //   pinMode(LED::PIN, OUTPUT);     // Setup LED
  //   digitalWrite(LED::PIN, HIGH);  // Toggle LED

  //   // Debug actuator pin
  //   // pinMode(Actuator::CONTROL_PIN, OUTPUT);     // Setup LED
  //   // digitalWrite(Actuator::CONTROL_PIN, HIGH);  // Toggle LED
  //   // while(1) {}
  // #endif

  /**
     * @details The load object, is in the Actuator namespace
     */
  // Wire.begin(8, 9); // SDA, SCL
  Wire.begin(1, 2); // SDA, SCL
  LOAD::configureLoad();
  LOAD::loadDevice.setGPIO(0b0000'0000);
  Serial.print("Input load setPoint as a 7 bit unsigned integer: ");
}

/**
 * @details put your main code here, to run repeatedly:
 */
void loop() {
  // Check if Serial data is available
  if (Serial.available() > 0) {
    int setPoint = Serial.parseInt();

    // It would appear parseInt returns 0 on failure
    if (setPoint != 0) {
      // Check and write position
      if (setPoint < 0) {
        Serial.print("\nsetPoint too small: ");
        Serial.println(setPoint);
      } else if ((setPoint > LOAD::PINS_Msk)) {
        Serial.print("\nsetPoint too large: ");
        Serial.println(setPoint);
      } else {
        Serial.print("\nNew setPoint: ");
        Serial.println(setPoint);
        LOAD::loadDevice.setGPIO(setPoint);
      }
      // Serial.print("Reported GPIO Reg: ");
      // Serial.println(LOAD::loadDevice.readGPIO());

      Serial.print("Input load setPoint as a bitset: ");
    }
  }

// #if defined(ESP32)
//   // Toggle LED because why not, but only on the EPS32
//   static int lastLEDTime_ms = 0;  // Track time toggled
//   static bool LEDOn = false;      // Track LED state

//   // Check LED state and update
//   if (!LEDOn && millis() - lastLEDTime_ms > LED::TIME_OFF_MS) {
//     digitalWrite(LED::PIN, HIGH);  // Toggle LED
//     lastLEDTime_ms = millis();     // Save time
//     LEDOn = true;
//   } else if (LEDOn && millis() - lastLEDTime_ms > LED::TIME_ON_MS) {
//     digitalWrite(LED::PIN, LOW);  // Toggle LED
//     lastLEDTime_ms = millis();    // Save time
//     LEDOn = false;
//   }
// #endif
}

