/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2010 Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/scb.h>

#define APP_ADDRESS	0x08002000

int main(void)
{
	//rcc_periph_clock_enable(RCC_GPIOA);

	//Boot the application if it's valid.
	if ((*(volatile uint32_t *)APP_ADDRESS & 0x2FFE0000) == 0x20000000) {
		//Set vector table base address.
		SCB_VTOR = APP_ADDRESS & 0xFFFF;
		//Initialise master stack pointer.
		asm volatile("msr msp, %0"::"g"
			     (*(volatile uint32_t *)APP_ADDRESS));
		//Jump to application.
		(*(void (**)())(APP_ADDRESS + 4))();
	}
}