/*
 * Copyright (c) 2018, Sensirion AG
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Sensirion AG nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "sensirion_arch_config.h"
#include "sensirion_common.h"
#include "sensirion_i2c.h"

#include "app_i2c.h"

#define DELAY_USEC (SENSIRION_I2C_CLOCK_PERIOD_USEC / 2)

static int8_t sensirion_wait_while_clock_stretching(
		uint8_t scl )
{
	/* Maximal timeout of 150ms (SCD30) in sleep polling cycles */
	uint32_t timeout_cycles = 150000 / SENSIRION_I2C_CLOCK_PERIOD_USEC;

	while (--timeout_cycles)
	{
		if (i2c_SCL_read( (gpio_num_t) scl ))
			return NO_ERROR;
		i2c_sleep_usec(SENSIRION_I2C_CLOCK_PERIOD_USEC);
	}

	return STATUS_FAIL;
}

static int8_t sensirion_i2c_write_byte(
		uint8_t    scl  ,
		uint8_t    sda  ,
		uint8_t    data )
{
	int8_t nack, i;

	for (i = 7; i >= 0; i--)
	{
		i2c_SCL_out( (gpio_num_t) scl );
		if ((data >> i) & 0x01)
			i2c_SDA_in( (gpio_num_t) sda );
		else
			i2c_SDA_out( (gpio_num_t) sda );

		i2c_sleep_usec(DELAY_USEC);
		i2c_SCL_in( (gpio_num_t) scl );

		i2c_sleep_usec(DELAY_USEC);
		if (sensirion_wait_while_clock_stretching(scl))
			return STATUS_FAIL;
	}

	i2c_SCL_out( (gpio_num_t) scl );
	i2c_SDA_in( (gpio_num_t) sda );
	
	i2c_sleep_usec(DELAY_USEC);
	i2c_SCL_in( (gpio_num_t) scl );
	if (sensirion_wait_while_clock_stretching(scl))
		return STATUS_FAIL;

	nack = (i2c_SDA_read( (gpio_num_t) sda ) != 0);
	i2c_SCL_out( (gpio_num_t) scl );

	return nack;
}

static uint8_t sensirion_i2c_read_byte(
		uint8_t    scl ,
		uint8_t    sda ,
		uint8_t    ack )
{
	int8_t i;
	uint8_t data = 0;

	i2c_SDA_in( (gpio_num_t) sda );
	for (i = 7; i >= 0; i--)
	{
		i2c_sleep_usec(DELAY_USEC);
		i2c_SCL_in( (gpio_num_t) scl );
		if (sensirion_wait_while_clock_stretching(scl))
		    return 0xFF;  // return 0xFF on error

		data |= (i2c_SDA_read( (gpio_num_t) sda ) != 0) << i;
		i2c_SCL_out( (gpio_num_t) scl );
	}

	if (ack)
		i2c_SDA_out( (gpio_num_t) sda );
	else
		i2c_SDA_in( (gpio_num_t) sda );

	i2c_sleep_usec(DELAY_USEC);
	i2c_SCL_in( (gpio_num_t) scl );

	i2c_sleep_usec(DELAY_USEC);
	if (sensirion_wait_while_clock_stretching(scl))
		return 0xFF;  // return 0xFF on error

	i2c_SCL_out( (gpio_num_t) scl );
	i2c_SDA_in( (gpio_num_t) sda );

	return data;
}

static int8_t sensirion_i2c_start(
		uint8_t    scl ,
		uint8_t    sda )
{
	i2c_SCL_in( (gpio_num_t) scl );
	if (sensirion_wait_while_clock_stretching(scl))
		return STATUS_FAIL;

	i2c_SDA_out( (gpio_num_t) sda );
	i2c_sleep_usec(DELAY_USEC);
	i2c_SCL_out( (gpio_num_t) scl );
	i2c_sleep_usec(DELAY_USEC);
	return NO_ERROR;
}

static void sensirion_i2c_stop(
		uint8_t    scl , 
		uint8_t    sda )
{
	i2c_SDA_out( (gpio_num_t) sda );
	i2c_sleep_usec(DELAY_USEC);
	i2c_SCL_in( (gpio_num_t) scl );
	i2c_sleep_usec(DELAY_USEC);
	i2c_SDA_in( (gpio_num_t) sda );
	i2c_sleep_usec(DELAY_USEC);
}

int8_t sensirion_i2c_write(
		uint8_t        scl     ,
		uint8_t        sda     ,
		uint8_t        address ,
		uint8_t const *data    ,
		uint16_t       count   )
{
	int8_t ret;
	uint16_t i;

	ret = sensirion_i2c_start(scl, sda);
	if (ret != NO_ERROR)
		return ret;

	ret = sensirion_i2c_write_byte(scl, sda, address << 1);
	if (ret != NO_ERROR)
	{
		sensirion_i2c_stop(scl, sda);
		return ret;
	}
	for (i = 0; i < count; i++)
	{
		ret = sensirion_i2c_write_byte(scl, sda, data[i]);
		if (ret != NO_ERROR)
		{
			sensirion_i2c_stop(scl, sda);
			break;
		}
	}
	sensirion_i2c_stop(scl, sda);
	return ret;
}

int8_t sensirion_i2c_read(
		uint8_t    scl     ,
		uint8_t    sda     ,
		uint8_t    address ,
		uint8_t   *data    ,
		uint16_t   count   )
{
	int8_t ret;
	uint8_t send_ack;
	uint16_t i;

	ret = sensirion_i2c_start(scl, sda);
	if (ret != NO_ERROR)
		return ret;

	ret = sensirion_i2c_write_byte(scl, sda, (address << 1) | 1);
	if (ret != NO_ERROR)
	{
		sensirion_i2c_stop(scl, sda);
		return ret;
	}
	for (i = 0; i < count; i++)
	{
		send_ack = i < (count - 1); /* last byte must be NACK'ed */
		data[i] = sensirion_i2c_read_byte(scl, sda, send_ack);
	}

	sensirion_i2c_stop(scl, sda);
	return NO_ERROR;
}

void sensirion_i2c_init(
		uint8_t    scl ,
		uint8_t    sda )
{
	i2c_init_pins( (gpio_num_t) scl, (gpio_num_t) sda );
	i2c_SCL_in( (gpio_num_t) scl );
	i2c_SDA_in( (gpio_num_t) sda );
}

void sensirion_i2c_release(
		uint8_t    scl ,
		uint8_t    sda )
{
	i2c_SCL_in( (gpio_num_t) scl );
	i2c_SDA_in( (gpio_num_t) sda );
	i2c_release_pins( (gpio_num_t) scl, (gpio_num_t) sda );
}

void sensirion_sleep_usec(uint32_t useconds)
{
	i2c_sleep_usec(useconds);
}