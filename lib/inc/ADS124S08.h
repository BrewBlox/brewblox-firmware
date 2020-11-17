/*
 * Copyright 2020 BrewPi B.V.
 *
 * This file is part of Brewblox.
 *
 * Brewblox is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Brewblox is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Brewblox.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <array>
//#include <cstdint>
//#include <cstring>
#include "hal/hal_delay.h"
#include "hal/hal_gpio.h"
#include "hal/hal_spi.h"

// #define INT_VREF 2.5

// #define SPI_WORD_SIZE 8
// #define SPI_SPEED 5000000

// #define DELAY_4TCLK (uint32_t)(1) // 1usec ~= (4.0 /ADS124S08_FCLK)
// #define DELAY_4096TCLK (uint32_t)(4096.0 * 1000000 / ADS124S08_FCLK)
// #define DELAY_2p2MS (uint32_t)(0.0022 * 1000000 / ADS124S08_FCLK)
// #define TIMEOUT_COUNTER 10000

class ADS124S08 {
public:
    // constants
    static constexpr uint8_t NUM_REGISTERS = 18;
    static constexpr uint32_t ADS124S08_FCLK = 4096000;
    static constexpr uint8_t ADS124S08_BITRES = 24;
    // Lengths of conversion data components
    static constexpr uint8_t COMMAND_LENGTH = 2;
    static constexpr uint8_t DATA_LENGTH = 3;
    static constexpr uint8_t STATUS_LENGTH = 1;
    static constexpr uint8_t CRC_LENGTH = 1;

    ADS124S08(uint8_t spi_host, int ss_pin);
    ~ADS124S08();

private:
    SpiConfig spiConfig;

    bool converting = false;
    enum DATA_MODE : uint8_t {
        NORMAL = 0x00,
        STATUS = 0x01,
        CRC = 0x02,
    };

    enum OPCODE : uint8_t {
        // SPI Contro; Commands
        NOP = 0x00,
        WAKEUP = 0x02,
        POWERDOWN = 0x04,
        RESET = 0x06,
        START = 0x08,
        STOP = 0x0A,

        //SPI Calibration Commands
        SYOCAL = 0x16,
        SYGCAL = 0x17,
        SFOCAL = 0x19,

        // SPI Data Read Command
        RDATA = 0x12,

        // SPI Register Read and Write Commands
        RREG = 0x20,
        WREG = 0x40,
        RWREG_MASK = 0x1F,
    };

    /* Read mode enum */
    enum ReadMode : uint8_t {
        DIRECT,
        COMMAND
    };

    /* ADS124S08 Register 0x0 (ID) Definition
 *|  Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0  |
 *-----------------------------------------------------------------------------------------------
 *|       			RESERVED[4:0]      			 			 |  	      DEV_ID[2:0]           |
 *-----------------------------------------------------------------------------------------------
 *
 */
    // ID register address

    class Register {
    public:
        Register()
            : value(0)
        {
        }

        Register(uint8_t init)
            : value(std::move(init))
        {
        }

        uint8_t value;
    };

    class RegId : public Register {
    public:
        RegId()
            : Register(0x00)
        {
        }
        static constexpr uint8_t address = 0x00;

        // possible values
        static constexpr uint8_t ADS_124S08 = 0x00;
        static constexpr uint8_t ADS_124S06 = 0x01;
        static constexpr uint8_t ADS_114S08 = 0x04;
        static constexpr uint8_t ADS_114S06 = 0x05;
    };

    /* ADS124S08 Register 0x1 (STATUS) Definition
 *|  Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0   |
 *------------------------------------------------------------------------------------------------
 *|  FL_POR  |    nRDY   | FL_P_RAILP| FL_P_RAILN| FL_N_RAILP| FL_N_RAILN| FL_REF_L1 | FL_REF_L0 |
 *------------------------------------------------------------------------------------------------
 */

    class RegStatus : public Register {
    public:
        RegStatus()
            : Register(0x80)
        {
        }

        static constexpr uint8_t address = 0x01;

        static constexpr uint8_t ADS_FL_POR_MASK = 0x80;
        static constexpr uint8_t ADS_nRDY_MASK = 0x40;
        static constexpr uint8_t ADS_FL_P_RAILP_MASK = 0x20;
        static constexpr uint8_t ADS_FL_P_RAILN_MASK = 0x10;
        static constexpr uint8_t ADS_FL_N_RAILP_MASK = 0x08;
        static constexpr uint8_t ADS_FL_N_RAILN_MASK = 0x04;
        static constexpr uint8_t ADS_FL_REF_L1_MASK = 0x02;
        static constexpr uint8_t ADS_FL_REF_L0_MASK = 0x10;
    };

    /* ADS124S08 Register 0x2 (INPMUX) Definition
 *|   Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0  |
 *------------------------------------------------------------------------------------------------
 *|        			MUXP[3:0]   				 |       			MUXN[3:0]                    |
 *------------------------------------------------------------------------------------------------
 */

    class RegInpMux : public Register {
    public:
        RegInpMux()
            : Register(0x01)
        {
        }

        static constexpr uint8_t address = 0x02;

        // Define the ADC positive input channels (MUXP)
        static constexpr uint8_t ADS_P_AIN0 = 0x00;
        static constexpr uint8_t ADS_P_AIN1 = 0x10;
        static constexpr uint8_t ADS_P_AIN2 = 0x20;
        static constexpr uint8_t ADS_P_AIN3 = 0x30;
        static constexpr uint8_t ADS_P_AIN4 = 0x40;
        static constexpr uint8_t ADS_P_AIN5 = 0x50;
        static constexpr uint8_t ADS_P_AIN6 = 0x60;
        static constexpr uint8_t ADS_P_AIN7 = 0x70;
        static constexpr uint8_t ADS_P_AIN8 = 0x80;
        static constexpr uint8_t ADS_P_AIN9 = 0x90;
        static constexpr uint8_t ADS_P_AIN10 = 0xA0;
        static constexpr uint8_t ADS_P_AIN11 = 0xB0;
        static constexpr uint8_t ADS_P_AINCOM = 0xC0;

        // Define the ADC negative input channels (MUXN)
        static constexpr uint8_t ADS_N_AIN0 = 0x00;
        static constexpr uint8_t ADS_N_AIN1 = 0x01;
        static constexpr uint8_t ADS_N_AIN2 = 0x02;
        static constexpr uint8_t ADS_N_AIN3 = 0x03;
        static constexpr uint8_t ADS_N_AIN4 = 0x04;
        static constexpr uint8_t ADS_N_AIN5 = 0x05;
        static constexpr uint8_t ADS_N_AIN6 = 0x06;
        static constexpr uint8_t ADS_N_AIN7 = 0x07;
        static constexpr uint8_t ADS_N_AIN8 = 0x08;
        static constexpr uint8_t ADS_N_AIN9 = 0x09;
        static constexpr uint8_t ADS_N_AIN10 = 0x0A;
        static constexpr uint8_t ADS_N_AIN11 = 0x0B;
        static constexpr uint8_t ADS_N_AINCOM = 0x0C;
    };

    /* ADS124S08 Register 0x3 (PGA) Definition
 *|  Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0  |
 *-----------------------------------------------------------------------------------------------
 *|  		DELAY[2:0]		     	 |      PGA_EN[1:0]      |              GAIN[2:0]           |
 *-----------------------------------------------------------------------------------------------
 */

    class RegPga : public Register {
    public:
        RegPga()
            : Register(0x00)
        {
        }

        static constexpr uint8_t address = 0x03;

        // Define conversion delay in tmod clock periods
        static constexpr uint8_t ADS_DELAY_14 = 0x00;
        static constexpr uint8_t ADS_DELAY_25 = 0x20;
        static constexpr uint8_t ADS_DELAY_64 = 0x40;
        static constexpr uint8_t ADS_DELAY_256 = 0x60;
        static constexpr uint8_t ADS_DELAY_1024 = 0x80;
        static constexpr uint8_t ADS_DELAY_2048 = 0xA0;
        static constexpr uint8_t ADS_DELAY_4096 = 0xC0;
        static constexpr uint8_t ADS_DELAY_1 = 0xE0;

        // Define PGA control
        static constexpr uint8_t ADS_PGA_BYPASS = 0x00;
        static constexpr uint8_t ADS_PGA_ENABLED = 0x08;

        // Define Gain
        static constexpr uint8_t ADS_GAIN_1 = 0x00;
        static constexpr uint8_t ADS_GAIN_2 = 0x01;
        static constexpr uint8_t ADS_GAIN_4 = 0x02;
        static constexpr uint8_t ADS_GAIN_8 = 0x03;
        static constexpr uint8_t ADS_GAIN_16 = 0x04;
        static constexpr uint8_t ADS_GAIN_32 = 0x05;
        static constexpr uint8_t ADS_GAIN_64 = 0x06;
        static constexpr uint8_t ADS_GAIN_128 = 0x07;
        static constexpr uint8_t ADS_GAIN_MASK = 0x07;
    };

    /* ADS124S08 Register 0x4 (DATARATE) Definition
 *|  Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0  |
 *-----------------------------------------------------------------------------------------------
 *|   G_CHOP  |    CLK    |    MODE   |   FILTER  | 				  DR[3:0]                   |
 *-----------------------------------------------------------------------------------------------
 */
    class RegDataRate : public Register {
    public:
        RegDataRate()
            : Register(0x14)
        {
        }

        static constexpr uint8_t address = 0x04;

        static constexpr uint8_t ADS_GLOBALCHOP = 0x80;
        static constexpr uint8_t ADS_CLKSEL_EXT = 0x40;
        static constexpr uint8_t ADS_CONVMODE_SS = 0x20;
        static constexpr uint8_t ADS_CONVMODE_CONT = 0x00;
        static constexpr uint8_t ADS_FILTERTYPE_LL = 0x10;

        // Define the data rate */
        static constexpr uint8_t ADS_DR_2_5 = 0x00;
        static constexpr uint8_t ADS_DR_5 = 0x01;
        static constexpr uint8_t ADS_DR_10 = 0x02;
        static constexpr uint8_t ADS_DR_16 = 0x03;
        static constexpr uint8_t ADS_DR_20 = 0x04;
        static constexpr uint8_t ADS_DR_50 = 0x05;
        static constexpr uint8_t ADS_DR_60 = 0x06;
        static constexpr uint8_t ADS_DR_100 = 0x07;
        static constexpr uint8_t ADS_DR_200 = 0x08;
        static constexpr uint8_t ADS_DR_400 = 0x09;
        static constexpr uint8_t ADS_DR_800 = 0x0A;
        static constexpr uint8_t ADS_DR_1000 = 0x0B;
        static constexpr uint8_t ADS_DR_2000 = 0x0C;
        static constexpr uint8_t ADS_DR_4000 = 0x0D;
    };

    /* ADS124S08 Register 0x5 (REF) Definition
 *|  Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0  |
 *-----------------------------------------------------------------------------------------------
 *|	  FL_REF_EN[1:0]	 | nREFP_BUF | nREFN_BUF | 		REFSEL[1:0]		 | 		REFCON[1:0]     |
 *-----------------------------------------------------------------------------------------------
 */

    class RegRef : public Register {
    public:
        RegRef()
            : Register(0x10)
        {
        }
        static constexpr uint8_t address = 0x05;

        static constexpr uint8_t ADS_FLAG_REF_DISABLE = 0x00;
        static constexpr uint8_t ADS_FLAG_REF_EN_L0 = 0x40;
        static constexpr uint8_t ADS_FLAG_REF_EN_BOTH = 0x80;
        static constexpr uint8_t ADS_FLAG_REF_EN_10M = 0xC0;
        static constexpr uint8_t ADS_REFP_BYP_DISABLE = 0x20;
        static constexpr uint8_t ADS_REFP_BYP_ENABLE = 0x00;
        static constexpr uint8_t ADS_REFN_BYP_DISABLE = 0x10;
        static constexpr uint8_t ADS_REFN_BYP_ENABLE = 0x00;
        static constexpr uint8_t ADS_REFSEL_P0 = 0x00;
        static constexpr uint8_t ADS_REFSEL_P1 = 0x04;
        static constexpr uint8_t ADS_REFSEL_INT = 0x08;
        static constexpr uint8_t ADS_REFINT_OFF = 0x00;
        static constexpr uint8_t ADS_REFINT_ON_PDWN = 0x01;
        static constexpr uint8_t ADS_REFINT_ON_ALWAYS = 0x02;
    };

    /* ADS124S08 Register 0x6 (IDACMAG) Definition
 *|  Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0  |
 *-----------------------------------------------------------------------------------------------
 *|FL_RAIL_EN|	  PSW	 |     0     | 		0	 | 			    	IMAG[3:0]                   |
 *-----------------------------------------------------------------------------------------------
 */

    class RegIdacMag : public Register {
    public:
        RegIdacMag()
            : Register(0x00)
        {
        }
        static constexpr uint8_t address = 0x06;

        static constexpr uint8_t ADS_FLAG_RAIL_ENABLE = 0x80;
        static constexpr uint8_t ADS_FLAG_RAIL_DISABLE = 0x00;
        static constexpr uint8_t ADS_PSW_OPEN = 0x00;
        static constexpr uint8_t ADS_PSW_CLOSED = 0x40;
        static constexpr uint8_t ADS_IDACMAG_OFF = 0x00;
        static constexpr uint8_t ADS_IDACMAG_10 = 0x01;
        static constexpr uint8_t ADS_IDACMAG_50 = 0x02;
        static constexpr uint8_t ADS_IDACMAG_100 = 0x03;
        static constexpr uint8_t ADS_IDACMAG_250 = 0x04;
        static constexpr uint8_t ADS_IDACMAG_500 = 0x05;
        static constexpr uint8_t ADS_IDACMAG_750 = 0x06;
        static constexpr uint8_t ADS_IDACMAG_1000 = 0x07;
        static constexpr uint8_t ADS_IDACMAG_1500 = 0x08;
        static constexpr uint8_t ADS_IDACMAG_2000 = 0x09;
    };

    /* ADS124S08 Register 0x7 (IDACMUX) Definition
 *|  Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0  |
 *-----------------------------------------------------------------------------------------------
 *|                   I2MUX[3:0]                 |                   I1MUX[3:0]                 |
 *-----------------------------------------------------------------------------------------------
 */

    class RegIdacMux : public Register {
    public:
        RegIdacMux()
            : Register(0xFF)
        {
        }
        static constexpr uint8_t address = 0x07;

        // Define IDAC2 Output
        static constexpr uint8_t ADS_IDAC2_A0 = 0x00;
        static constexpr uint8_t ADS_IDAC2_A1 = 0x10;
        static constexpr uint8_t ADS_IDAC2_A2 = 0x20;
        static constexpr uint8_t ADS_IDAC2_A3 = 0x30;
        static constexpr uint8_t ADS_IDAC2_A4 = 0x40;
        static constexpr uint8_t ADS_IDAC2_A5 = 0x50;
        static constexpr uint8_t ADS_IDAC2_A6 = 0x60;
        static constexpr uint8_t ADS_IDAC2_A7 = 0x70;
        static constexpr uint8_t ADS_IDAC2_A8 = 0x80;
        static constexpr uint8_t ADS_IDAC2_A9 = 0x90;
        static constexpr uint8_t ADS_IDAC2_A10 = 0xA0;
        static constexpr uint8_t ADS_IDAC2_A11 = 0xB0;
        static constexpr uint8_t ADS_IDAC2_AINCOM = 0xC0;
        static constexpr uint8_t ADS_IDAC2_OFF = 0xF0;

        // Define IDAC1 Output
        static constexpr uint8_t ADS_IDAC1_A0 = 0x00;
        static constexpr uint8_t ADS_IDAC1_A1 = 0x01;
        static constexpr uint8_t ADS_IDAC1_A2 = 0x02;
        static constexpr uint8_t ADS_IDAC1_A3 = 0x03;
        static constexpr uint8_t ADS_IDAC1_A4 = 0x04;
        static constexpr uint8_t ADS_IDAC1_A5 = 0x05;
        static constexpr uint8_t ADS_IDAC1_A6 = 0x06;
        static constexpr uint8_t ADS_IDAC1_A7 = 0x07;
        static constexpr uint8_t ADS_IDAC1_A8 = 0x08;
        static constexpr uint8_t ADS_IDAC1_A9 = 0x09;
        static constexpr uint8_t ADS_IDAC1_A10 = 0x0A;
        static constexpr uint8_t ADS_IDAC1_A11 = 0x0B;
        static constexpr uint8_t ADS_IDAC1_AINCOM = 0x0C;
        static constexpr uint8_t ADS_IDAC1_OFF = 0x0F;
    };

    /* ADS124S08 Register 0x8 (VBIAS) Definition
 *|  Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0  |
 *-----------------------------------------------------------------------------------------------
 *| VB_LEVEL | 	VB_AINC  |  VB_AIN5  |  VB_AIN4  |  VB_AIN3  |  VB_AIN2  |  VB_AIN1  |  VB_AIN0 |
 *-----------------------------------------------------------------------------------------------
 */

    class RegVbias : public Register {
    public:
        RegVbias()
            : Register(0x00)
        {
        }
        static constexpr uint8_t address = 0x08;

        static constexpr uint8_t ADS_VBIAS_LVL_DIV2 = 0x00;
        static constexpr uint8_t ADS_VBIAS_LVL_DIV12 = 0x80;

        // Define VBIAS here
        static constexpr uint8_t ADS_VB_AINC = 0x40;
        static constexpr uint8_t ADS_VB_AIN5 = 0x20;
        static constexpr uint8_t ADS_VB_AIN4 = 0x10;
        static constexpr uint8_t ADS_VB_AIN3 = 0x08;
        static constexpr uint8_t ADS_VB_AIN2 = 0x04;
        static constexpr uint8_t ADS_VB_AIN1 = 0x02;
        static constexpr uint8_t ADS_VB_AIN0 = 0x01;
    };

    /* ADS124S08 Register 0x9 (SYS) Definition
 *|  Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0  |
 *-----------------------------------------------------------------------------------------------
 *|			   SYS_MON[2:0]			 |	   CAL_SAMP[1:0]     |  TIMEOUT  | 	  CRC	 | SENDSTAT |
 *-----------------------------------------------------------------------------------------------
 */

    class RegSys : public Register {
    public:
        RegSys()
            : Register(0x10)
        {
        }
        static constexpr uint8_t address = 0x09;

        static constexpr uint8_t ADS_SYS_MON_OFF = 0x00;
        static constexpr uint8_t ADS_SYS_MON_SHORT = 0x20;
        static constexpr uint8_t ADS_SYS_MON_TEMP = 0x40;
        static constexpr uint8_t ADS_SYS_MON_ADIV4 = 0x60;
        static constexpr uint8_t ADS_SYS_MON_DDIV4 = 0x80;
        static constexpr uint8_t ADS_SYS_MON_BCS_2 = 0xA0;
        static constexpr uint8_t ADS_SYS_MON_BCS_1 = 0xC0;
        static constexpr uint8_t ADS_SYS_MON_BCS_10 = 0xE0;
        static constexpr uint8_t ADS_CALSAMPLE_1 = 0x00;
        static constexpr uint8_t ADS_CALSAMPLE_4 = 0x08;
        static constexpr uint8_t ADS_CALSAMPLE_8 = 0x10;
        static constexpr uint8_t ADS_CALSAMPLE_16 = 0x18;
        static constexpr uint8_t ADS_TIMEOUT_DISABLE = 0x00;
        static constexpr uint8_t ADS_TIMEOUT_ENABLE = 0x04;
        static constexpr uint8_t ADS_CRC_DISABLE = 0x00;
        static constexpr uint8_t ADS_CRC_ENABLE = 0x02;
        static constexpr uint8_t ADS_CRC_MASK = 0x02;
        static constexpr uint8_t ADS_SENDSTATUS_DISABLE = 0x00;
        static constexpr uint8_t ADS_SENDSTATUS_ENABLE = 0x01;
        static constexpr uint8_t ADS_SENDSTATUS_MASK = 0x01;
    };

    /* ADS124S08 Register 0xA (OFCAL0) Definition 
 *|  Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0  |
 *-----------------------------------------------------------------------------------------------
 *|                                        OFC[7:0]                                             |
 *-----------------------------------------------------------------------------------------------
 */

    class RegOfcal0 : public Register {
    public:
        RegOfcal0()
            : Register(0x00)
        {
        }
        static constexpr uint8_t address = 0x0A;
    };

    /* ADS124S08 Register 0xB (OFCAL1) Definition
 *|  Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0  |
 *-----------------------------------------------------------------------------------------------
 *|                                        OFC[15:8]                                            |
 *-----------------------------------------------------------------------------------------------
 */
    class RegOfcal1 : public Register {
    public:
        RegOfcal1()
            : Register(0x00)
        {
        }
        static constexpr uint8_t address = 0x0B;
    };

    /* ADS124S08 Register 0xC (OFCAL2) Definition
 *|  Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0  |
 *-----------------------------------------------------------------------------------------------
 *|                                        OFC[23:16]                                           |
 *-----------------------------------------------------------------------------------------------
 */

    class RegOfcal2 : public Register {
    public:
        RegOfcal2()
            : Register(0x00)
        {
        }
        static constexpr uint8_t address = 0x0C;
    };

    /* ADS124S08 Register 0xD (FSCAL0) Definition
 *|  Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0  |
 *-----------------------------------------------------------------------------------------------
 *|                                        FSC[7:0]                                             |
 *-----------------------------------------------------------------------------------------------
 */

    class RegFscal0 : public Register {
    public:
        RegFscal0()
            : Register(0x00)
        {
        }
        static constexpr uint8_t address = 0x0D;
    };

    /* ADS124S08 Register 0xE (FSCAL1) Definition 
 *|  Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0  |
 *-----------------------------------------------------------------------------------------------
 *|                                        FSC[15:8]                                            |
 *-----------------------------------------------------------------------------------------------
 */

    class RegFscal1 : public Register {
    public:
        RegFscal1()
            : Register(0x00)
        {
        }
        static constexpr uint8_t address = 0x0E;
    };

    /* ADS124S08 Register 0xF (FSCAL2) Definition 
 *|  Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0  |
 *-----------------------------------------------------------------------------------------------
 *|                                        FSC[23:16]                                           |
 *-----------------------------------------------------------------------------------------------
 */

    class RegFscal2 : public Register {
    public:
        RegFscal2()
            : Register(0x40)
        {
        }
        static constexpr uint8_t address = 0x0F;
    };

    /* ADS124S08 Register 0x10 (GPIODAT) Definition
 *|  Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0  |
 *-----------------------------------------------------------------------------------------------
 *|                     DIR[3:0]    			 | 					DAT[3:0]                    |
 *-----------------------------------------------------------------------------------------------
 */
    // GPIODAT register address
    class RegGpiodat : public Register {
    public:
        RegGpiodat()
            : Register(0x00)
        {
        }
        static constexpr uint8_t address = 0x10;

        // Define GPIO direction (0-Output; 1-Input) here
        static constexpr uint8_t ADS_GPIO0_DIR_INPUT = 0x10;
        static constexpr uint8_t ADS_GPIO1_DIR_INPUT = 0x20;
        static constexpr uint8_t ADS_GPIO2_DIR_INPUT = 0x40;
        static constexpr uint8_t ADS_GPIO3_DIR_INPUT = 0x80;
    };

    /* ADS124S08 Register 0x11 (GPIOCON) Definition
 *|  Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0  |
 *-----------------------------------------------------------------------------------------------
 *|    0	 |	   0	 |	   0	 |	   0     |                    CON[3:0]                  |
 *-----------------------------------------------------------------------------------------------
 */

    // GPIODAT register address
    class RegGpiocon : public Register {
    public:
        RegGpiocon()
            : Register(0x00)
        {
        }
        static constexpr uint8_t address = 0x11;

        // Define GPIO configuration (0-Analog Input; 1-GPIO) here
        static constexpr uint8_t ADS_GPIO0_DIR_INPUT = 0x10;
        static constexpr uint8_t ADS_GPIO1_DIR_INPUT = 0x20;
        static constexpr uint8_t ADS_GPIO2_DIR_INPUT = 0x40;
        static constexpr uint8_t ADS_GPIO3_DIR_INPUT = 0x80;
    };

    using RegisterMap = std::array<Register, 18>;
    RegisterMap defaultRegisters()
    {
        return {
            RegId{},
            RegStatus{},
            RegInpMux{},
            RegPga{},
            RegDataRate{},
            RegRef{},
            RegIdacMag{},
            RegIdacMux{},
            RegVbias{},
            RegSys{},
            RegOfcal0{},
            RegOfcal1{},
            RegOfcal2{},
            RegFscal0{},
            RegFscal1{},
            RegFscal2{},
            RegGpiodat{},
            RegGpiocon{}};
    }

    RegisterMap registers = {0};

    //*****************************************************************************
    //
    // Function Prototypes
    //
    //*****************************************************************************

    bool startup();

    // bool adcStartupRoutine(ADCchar_Set* adcChars, SPI_Handle spiHdl);
    // bool changeADCParameters(ADCchar_Set* adcChars, SPI_Handle spiHdl);
    // int32_t readConvertedData(SPI_Handle spiHdl, uint8_t status[], readMode mode);
    // uint8_t readSingleRegister(SPI_Handle spiHdl, uint8_t address);
    // void readMultipleRegisters(SPI_Handle spiHdl, uint8_t startAddress, uint8_t count);
    // void sendCommand(SPI_Handle spiHdl, uint8_t op_code);
    // void startConversions(SPI_Handle spiHdl);
    // void stopConversions(SPI_Handle spiHdl);
    // void writeSingleRegister(SPI_Handle spiHdl, uint8_t address, uint8_t data);
    // void writeMultipleRegisters(SPI_Handle spiHdl, uint8_t startAddress, uint8_t count, uint8_t regData[]);

    // Internal variable getters
    // uint8_t getRegisterValue(uint8_t address);

    // Internal variable setters void restoreRegisterDefaults(void);
};

//*****************************************************************************
//
// Macros
//
//*****************************************************************************

/** Register bit checking macros...
 *  Return true if register bit is set (since last read or write).
 */
#define IS_SENDSTAT_SET ((bool)(getRegisterValue(REG_ADDR_SYS) & ADS_SENDSTATUS_MASK))
#define IS_CRC_SET ((bool)(getRegisterValue(REG_ADDR_SYS) & ADS_CRC_MASK))
