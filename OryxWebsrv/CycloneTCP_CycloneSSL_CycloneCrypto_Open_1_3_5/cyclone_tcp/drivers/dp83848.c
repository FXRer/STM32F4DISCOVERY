/**
 * @file dp83848.c
 * @brief DP83848 Ethernet PHY transceiver
 *
 * @section License
 *
 * Copyright (C) 2010-2013 Oryx Embedded. All rights reserved.
 *
 * This file is part of CycloneTCP Open.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * @author Oryx Embedded (www.oryx-embedded.com)
 * @version 1.3.5
 **/

//Switch to the appropriate trace level
#define TRACE_LEVEL NIC_TRACE_LEVEL

//Dependencies
#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "tcp_ip_stack.h"
#include "dp83848.h"
#include "debug.h"

/**
 * @brief DP83848 Ethernet PHY driver
 **/

const PhyDriver dp83848PhyDriver =
{
   dp83848Init,
   dp83848Tick,
   dp83848EnableIrq,
   dp83848DisableIrq,
   dp83848EventHandler,
};


/**
 * @brief DP83848 PHY transceiver initialization
 * @param[in] interface Underlying network interface
 * @return Error code
 **/

error_t dp83848Init(NetInterface *interface)
{
   //Debug message
   TRACE_INFO("Initializing DP83848...\r\n");

   //Reset PHY transceiver
   dp83848WritePhyReg(interface, DP83848_PHY_REG_BMCR, BMCR_RESET);
   //Wait for the reset to complete
   while(dp83848ReadPhyReg(interface, DP83848_PHY_REG_BMCR) & BMCR_RESET);

   //Dump PHY registers for debugging purpose
   dp83848DumpPhyReg(interface);

   //Configure PWR_DOWN/INT pin as an interrupt output
   dp83848WritePhyReg(interface, DP83848_PHY_REG_MICR, MICR_INTEN | MICR_INT_OE);
   //The PHY will generate interrupts when link status changes are detected
   dp83848WritePhyReg(interface, DP83848_PHY_REG_MISR, MISR_LINK_INT_EN);

   //Successful initialization
   return NO_ERROR;
}

/**
 * @brief DP83848 timer handler
 * @param[in] interface Underlying network interface
 **/

void dp83848Tick(NetInterface *interface)
{
	uint16_t value;
	bool_t linkState;

	//Read basic status register
	value = dp83848ReadPhyReg(interface, DP83848_PHY_REG_BMSR);
	//Retrieve current link state
	linkState = (value & BMSR_LINK_STATUS) ? TRUE : FALSE;

	//Link up event?
	if(linkState && !interface->linkState)
	{
		//A PHY event is pending...
		interface->phyEvent = TRUE;
		//Notify the user that the link state has changed
		osEventSet(interface->nicRxEvent);
	}
	//Link down event?
	else if(!linkState && interface->linkState)
	{
		//A PHY event is pending...
		interface->phyEvent = TRUE;
		//Notify the user that the link state has changed
		osEventSet(interface->nicRxEvent);
	}
}

/**
 * @brief Enable interrupts
 * @param[in] interface Underlying network interface
 **/

void dp83848EnableIrq(NetInterface *interface)
{
}

/**
 * @brief Disable interrupts
 * @param[in] interface Underlying network interface
 **/

void dp83848DisableIrq(NetInterface *interface)
{
}

/**
 * @brief DP83848 event handler
 * @param[in] interface Underlying network interface
 * @return TRUE if a link state change notification is received
 **/

bool_t dp83848EventHandler(NetInterface *interface)
{
	uint16_t status;

	//Read status register to acknowledge the interrupt
	status = dp83848ReadPhyReg(interface, DP83848_PHY_REG_MISR);

	//Link status change?
	if(status & MISR_LINK_INT)
	{
		//Read PHY status register
		status = dp83848ReadPhyReg(interface, DP83848_PHY_REG_PHYSTS);

		//Link is up?
		if(status & PHYSTS_LINK_STATUS)
		{
			//Check current speed
			interface->speed100 = (status & PHYSTS_SPEED_STATUS) ? FALSE : TRUE;
			//Check duplex mode
			interface->fullDuplex = (status & PHYSTS_DUPLEX_STATUS) ? TRUE : FALSE;
			//Update link state
			interface->linkState = TRUE;

			//Display link state
			TRACE_INFO("Link is up (%s)...\r\n", interface->name);

			//Display actual speed and duplex mode
			TRACE_INFO("%s %s\r\n",
			interface->speed100 ? "100BASE-TX" : "10BASE-T",
			interface->fullDuplex ? "Full-Duplex" : "Half-Duplex");
		}
		else
		{
			//Update link state
			interface->linkState = FALSE;

			//Display link state
			TRACE_INFO("Link is down (%s)...\r\n", interface->name);
		}

		//Notify the user that the link state has changed
		return TRUE;
	}
	else
	{
		//No link state change...
		return FALSE;
	}
}

/**
 * @brief Write PHY register
 * @param[in] interface Underlying network interface
 * @param[in] address Register address
 * @param[in] data Register value
 **/

void dp83848WritePhyReg(NetInterface *interface, uint8_t address, uint16_t data)
{
   //Write the specified PHY register
   interface->nicDriver->writePhyReg(DP83848_PHY_ADDR, address, data);
}


/**
 * @brief Read PHY register
 * @param[in] interface Underlying network interface
 * @param[in] address PHY register address
 * @return Register value
 **/

uint16_t dp83848ReadPhyReg(NetInterface *interface, uint8_t address)
{
   //Read the specified PHY register
   return interface->nicDriver->readPhyReg(DP83848_PHY_ADDR, address);
}


/**
 * @brief Dump PHY registers for debugging purpose
 * @param[in] interface Underlying network interface
 **/

void dp83848DumpPhyReg(NetInterface *interface)
{
   uint_t i;

   //Loop through PHY registers
   for(i = 0; i < 32; i++)
   {
      //Display current PHY register
	  TRACE_DEBUG("%02X: 0x%04X\r\n", i, dp83848ReadPhyReg(interface, i));
   }

   //Terminate with a line feed
   TRACE_DEBUG("\r\n");
}
