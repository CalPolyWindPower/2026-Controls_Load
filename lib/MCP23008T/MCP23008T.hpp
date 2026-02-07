#pragma once

#include <Adafruit_BusIO_Register.h>
#include <Adafruit_I2CDevice.h>
#include <cstdint>

/**
 * @brief Class to interface with the MCP23008T I2C 8-Bit I/O Expander
 * @see
 * https://www.digikey.com/en/products/detail/microchip-technology/MCP23008T-E-SS/736037
 */
class MCP23008T {
  public:
    static constexpr uint16_t REG_IODIR_ADDR = 0x00;
    static constexpr uint16_t REG_IPOL_ADDR = 0x01;
    static constexpr uint16_t REG_GPINTEN_ADDR = 0x02;
    static constexpr uint16_t REG_DEFVAL_ADDR = 0x03;
    static constexpr uint16_t REG_INTCON_ADDR = 0x04;
    static constexpr uint16_t REG_IOCON_ADDR = 0x05;
    static constexpr uint16_t REG_GPPU_ADDR = 0x06;
    static constexpr uint16_t REG_INTF_ADDR = 0x07;
    static constexpr uint16_t REG_INTCAP_ADDR = 0x08;
    static constexpr uint16_t REG_GPIO_ADDR = 0x09;
    static constexpr uint16_t REG_OLAT_ADDR = 0x0A;

    static constexpr uint8_t ONE_BYTE = 1;

    /**
     * Valid addresses are 0x40, 0x42, 0x44, 0x46, 0x48, 0x4A, 0x4C, & 0x4E
     */
    enum I2C_ADDRESS: uint8_t {
        I2C_ADDR_0x40 = 0x40,
        I2C_ADDR_0x42 = 0x42,
        I2C_ADDR_0x44 = 0x44,
        I2C_ADDR_0x46 = 0x46,
        I2C_ADDR_0x48 = 0x48,
        I2C_ADDR_0x4A = 0x4A,
        I2C_ADDR_0x4C = 0x4C,
        I2C_ADDR_0x4E = 0x4E
    };

    MCP23008T(I2C_ADDRESS address = I2C_ADDR_0x40, TwoWire *wire = &Wire) : device(address, wire) {}

    bool begin() {
        if (!device.begin()) {
            return false;
        }

        return true;
    }

    /**
     * @details When a bit is set, the corresponding pin becomes an input (1).
     * When a bit is clear (0), the corresponding pin becomes an output.
     *
     * @returns Returns true on success, false otherwise
     */
    inline bool setIODir(uint32_t value) { return regIODIR.write(value); }

    /**
     * @details If a bit is set, the corresponding GPIO register bit will
     * reflect the inverted value on the pin.
     *
     * @returns Returns true on success, false otherwise
     */
    inline bool setInputPolarity(uint32_t value) {
        return regIPOL.write(value);
    }

    /**
     * @details If a bit is set, the corresponding pin is enabled for
     * interrupt-on-change. The DEFVAL and INTCON registers must also be
     * configured if any pins are enabled for interrupt-on-change.
     *
     * @returns Returns true on success, false otherwise
     */
    inline bool setIntOnChange(uint32_t value) {
        return regGPINTEN.write(value);
    }

    /**
     * @details The default comparison value is configured in the DEFVAL
     * register. If enabled (via GPINTEN and INTCON) to compare against the
     * DEFVAL register, an opposite value on the associated pin will cause an
     * interrupt to occur.
     *
     * @returns Returns true on success, false otherwise
     */
    inline bool setDefaultCompVal(uint32_t value) {
        return regDEFVAL.write(value);
    }

    /**
     * @details The INTCON register controls how the associated pin value is
     * compared for the interrupt-on-change feature. If a bit is set, the
     * corresponding I/O pin is compared against the associated bit in the
     * DEFVAL register. If a bit value is clear, the corresponding I/O pin is
     * compared against the previous value.
     *
     * @returns Returns true on success, false otherwise
     */
    inline bool setIntOnChange(uint32_t value) {
        return regINTCON.write(value);
    }

    /**
     * @details The IOCON register contains several bits for configuring the
     * device:
     * - The Sequential Operation (SEQOP) controls the incrementing function of
     *   the Address Pointer. If the Address Pointer is disabled, the Address
     *   Pointer does not automatically increment after each byte is clocked
     *   during a serial transfer. This feature is useful when it is desired to
     *   continuously poll (read) or modify (write) a register. • The Slew Rate
     *   (DISSLW) bit controls the slew rate function on the SDA pin. If
     *   enabled, the SDA slew rate will be controlled when driving from a high
     *   to a low.
     * - The Hardware Address Enable (HAEN) control bit enables/disables the
     *   hardware address pins (A1, A0) on the MCP23S08. This bit is not used on
     *   the MCP23008. The address pins are always enabled on the MCP23008.
     * - The Open-Drain (ODR) control bit enables/disables the INT pin for
     *   open-drain configuration.
     * - The Interrupt Polarity (INTPOL) control bit sets the polarity of the
     *   INT pin. This bit is functional only when the ODR bit is cleared,
     *   configuring the INT pin as active push-pull.
     * bit 7-6 Unimplemented: Read as ‘0’
     * bit 5 SEQOP: Sequential Operation Mode
     *  1 = Sequential operation disabled, Address Pointer does not increment
     *  0 = Sequential operation enabled, Address Pointer increments
     * bit 4 DISSLW: Slew Rate Control Bit for SDA Output
     *  1 = Slew rate disabled
     *  0 = Slew rate enabled
     * bit 3 HAEN: Hardware Address Enable (MCP23S08 only)
     *  Address pins are always enabled on MCP23008.
     *  1 = Enables the MCP23S08 address pins
     *  0 = Disables the MCP23S08 address pins
     * bit 2 ODR: This bit configures the INT pin as an open-drain output
     *  1 = Open-drain output (overrides the INTPOL bit)
     *  0 = Active driver output (INTPOL bit sets the polarity)
     * bit 1 INTPOL: This bit sets the polarity of the INT output pin
     *  1 = Active-high
     *  0 = Active-low
     * bit 0 Unimplemented: Read as ‘0’
     *
     * @returns Returns true on success, false otherwise
     */
    inline bool setConfig(uint32_t value) { return regIOCON.write(value); }

    /**
     * @details The GPPU register controls the pull-up resistors for the PORT
     * pins. If a bit is set and the corresponding pin is configured as an
     * input, the corresponding PORT pin is internally pulled up with a 100 k
     * resistor
     *
     * @returns Returns true on success, false otherwise
     */
    inline bool setPullUp(uint32_t value) { return regGPPU.write(value); }

    /**
     * @details The INTF register reflects the interrupt condition on the
     * PORT pins of any pin that is enabled for interrupts via
     * the GPINTEN register. A ‘set’ bit indicates that the
     * associated pin caused the interrupt.
     *
     * This register is ‘read-only’. Writes to this register will be
     * ignored.
     *
     * @returns Returns 0xFFFFFFFF on failure, value otherwise
     */
    inline uint32_t readIntFlag(void) { return regINTF.read(); }

    /**
     * @details The INTCAP register captures the GPIO port value at the time the
     * interrupt occurred. The register is ‘read-only’ and is updated only when
     * an interrupt occurs. The register will remain unchanged until the
     * interrupt is cleared via a read of INTCAP or GPIO.
     *
     * @returns Returns 0xFFFFFFFF on failure, value otherwise
     */
    inline uint32_t readIntCap(void) { return regIntCap.read(); }

    /**
     * @details The GPIO register reflects the value on the port. Reading from
     * this register reads the port. Writing to this register modifies the
     * Output Latch (OLAT) register.
     *
     * @returns Returns true on success, false otherwise
     */
    inline bool setGPIO(uint32_t value) { return regGPIO.write(value); }

    /**
     * @details  The OLAT register provides access to the output latches. A read
     * from this register results in a read of the OLAT and not the port itself.
     * A write to this register modifies the output latches that modify the pins
     * configured as outputs.
     *
     * @returns Returns true on success, false otherwise
     */
    inline bool setOLAT(uint32_t value) { return regOLAT.write(value); }

  private:
    Adafruit_I2CDevice device;

    Adafruit_BusIO_Register regIODIR = Adafruit_BusIO_Register(
        &device, REG_IODIR_ADDR, ONE_BYTE, LSBFIRST, ONE_BYTE);
    Adafruit_BusIO_Register regIPOL = Adafruit_BusIO_Register(
        &device, REG_IPOL_ADDR, ONE_BYTE, LSBFIRST, ONE_BYTE);
    Adafruit_BusIO_Register regGPINTEN = Adafruit_BusIO_Register(
        &device, REG_GPINTEN_ADDR, ONE_BYTE, LSBFIRST, ONE_BYTE);
    Adafruit_BusIO_Register regDEFVAL = Adafruit_BusIO_Register(
        &device, REG_DEFVAL_ADDR, ONE_BYTE, LSBFIRST, ONE_BYTE);

    Adafruit_BusIO_Register regINTCON = Adafruit_BusIO_Register(
        &device, REG_INTCON_ADDR, ONE_BYTE, LSBFIRST, ONE_BYTE);
    Adafruit_BusIO_Register regIOCON = Adafruit_BusIO_Register(
        &device, REG_IOCON_ADDR, ONE_BYTE, LSBFIRST, ONE_BYTE);
    Adafruit_BusIO_Register regGPPU = Adafruit_BusIO_Register(
        &device, REG_GPPU_ADDR, ONE_BYTE, LSBFIRST, ONE_BYTE);
    Adafruit_BusIO_Register regINTF = Adafruit_BusIO_Register(
        &device, REG_INTF_ADDR, ONE_BYTE, LSBFIRST, ONE_BYTE);

    Adafruit_BusIO_Register regIntCap = Adafruit_BusIO_Register(
        &device, REG_INTCAP_ADDR, ONE_BYTE, LSBFIRST, ONE_BYTE);
    Adafruit_BusIO_Register regGPIO = Adafruit_BusIO_Register(
        &device, REG_GPIO_ADDR, ONE_BYTE, LSBFIRST, ONE_BYTE);
    Adafruit_BusIO_Register regOLAT = Adafruit_BusIO_Register(
        &device, REG_OLAT_ADDR, ONE_BYTE, LSBFIRST, ONE_BYTE);
};
