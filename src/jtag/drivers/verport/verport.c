/***************************************************************************
 *   Copyright (C) 2005 by Dominic Rath                                    *
 *   Dominic.Rath@gmx.de                                                   *
 *                                                                         *
 *   Copyright (C) 2008 by Spencer Oliver                                  *
 *   spen@spen-soft.co.uk                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <jtag/interface.h>
#include "bitbang.h"
#include "Vtap_ext.h"

/* parallel port cable description
 */
struct cable {
	const char *name;
	uint8_t TDO_MASK;	/* status port bit containing current TDO value */
	uint8_t TRST_MASK;	/* data port bit for TRST */
	uint8_t TMS_MASK;	/* data port bit for TMS */
	uint8_t TCK_MASK;	/* data port bit for TCK */
	uint8_t TDI_MASK;	/* data port bit for TDI */
	uint8_t SRST_MASK;	/* data port bit for SRST */
	uint8_t OUTPUT_INVERT;	/* data port bits that should be inverted */
	uint8_t INPUT_INVERT;	/* status port that should be inverted */
	uint8_t PORT_INIT;	/* initialize data port with this value */
	uint8_t PORT_EXIT;	/* de-initialize data port with this value */
	uint8_t LED_MASK;	/* data port bit for LED */
};

/* configuration */
static uint16_t verport_port;
static bool verport_exit;
static uint32_t verport_toggling_time_ns = 1000;
static int wait_states;

/* interface variables
 */
static int otms, otck, otrstn, otdi;

static int verport_read(void)
{
	return Vtap_time_step(otms, otck, otrstn, otdi);
}

static void verport_write(int tck, int tms, int tdi)
{
  int i;
  i = 5;
  while (i-- > 0)
    Vtap_time_step(tms, tck, 1, tdi);
  otms = tms;
  otck = tck;
  otdi = tdi;
}

/* (1) assert or (0) deassert reset lines */
static void verport_reset(int trst, int srst)
{
	LOG_DEBUG("trst: %i, srst: %i", trst, srst);
        otrstn = !trst;
        Vtap_time_step(otms, otck, otrstn, otdi);
}

static int verport_speed(int speed)
{
	wait_states = speed;
	return ERROR_OK;
}

static int verport_khz(int khz, int *jtag_speed)
{
	if (khz == 0) {
		LOG_DEBUG("RCLK not supported");
		return ERROR_FAIL;
	}

	*jtag_speed = 499999 / (khz * verport_toggling_time_ns);
	return ERROR_OK;
}

static int verport_speed_div(int speed, int *khz)
{
	uint32_t denominator = (speed + 1) * verport_toggling_time_ns;

	*khz = (499999 + denominator) / denominator;
	return ERROR_OK;
}

static struct bitbang_interface verport_bitbang = {
		.read = &verport_read,
		.write = &verport_write,
		.reset = &verport_reset,
	};

static int verport_init(void)
{
        LOG_WARNING("Simulating JTAG with Verilator");
        Vtap_start();
        atexit(Vtap_finish);

	verport_reset(0, 0);
	verport_write(0, 0, 0);

	bitbang_interface = &verport_bitbang;

	return ERROR_OK;
}

static int verport_quit(void)
{
	return ERROR_OK;
}

COMMAND_HANDLER(verport_handle_verport_port_command)
{
	if (CMD_ARGC == 1) {
		/* only if the port wasn't overwritten by cmdline */
		if (verport_port == 0)
			COMMAND_PARSE_NUMBER(u16, CMD_ARGV[0], verport_port);
		else {
			LOG_ERROR("The verport port was already configured!");
			return ERROR_FAIL;
		}
	}

	command_print(CMD_CTX, "verport port = 0x%" PRIx16 "", verport_port);

	return ERROR_OK;
}

COMMAND_HANDLER(verport_handle_verport_cable_command)
{
	if (CMD_ARGC == 0)
		return ERROR_OK;

	return ERROR_OK;
}

COMMAND_HANDLER(verport_handle_write_on_exit_command)
{
	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	COMMAND_PARSE_ON_OFF(CMD_ARGV[0], verport_exit);

	return ERROR_OK;
}

COMMAND_HANDLER(verport_handle_verport_toggling_time_command)
{
	if (CMD_ARGC == 1) {
		uint32_t ns;
		int retval = parse_u32(CMD_ARGV[0], &ns);

		if (ERROR_OK != retval)
			return retval;

		if (ns == 0) {
			LOG_ERROR("0 ns is not a valid verport toggling time");
			return ERROR_FAIL;
		}

		verport_toggling_time_ns = ns;
		retval = jtag_get_speed(&wait_states);
		if (retval != ERROR_OK) {
			/* if jtag_get_speed fails then the clock_mode
			 * has not been configured, this happens if verport_toggling_time is
			 * called before the adapter speed is set */
			LOG_INFO("no verport speed set - defaulting to zero wait states");
			wait_states = 0;
		}
	}

	command_print(CMD_CTX, "verport toggling time = %" PRIu32 " ns",
			verport_toggling_time_ns);

	return ERROR_OK;
}

static const struct command_registration verport_command_handlers[] = {
	{
		.name = "verport_port",
		.handler = verport_handle_verport_port_command,
		.mode = COMMAND_CONFIG,
		.help = "Display the address of the I/O port (e.g. 0x378) "
			"or the number of the '/dev/verport' device used.  "
			"If a parameter is provided, first change that port.",
		.usage = "[port_number]",
	},
	{
		.name = "verport_cable",
		.handler = verport_handle_verport_cable_command,
		.mode = COMMAND_CONFIG,
		.help = "Set the layout of the parallel port cable "
			"used to connect to the target.",
		/* REVISIT there's no way to list layouts we know ... */
		.usage = "[layout]",
	},
	{
		.name = "verport_write_on_exit",
		.handler = verport_handle_write_on_exit_command,
		.mode = COMMAND_CONFIG,
		.help = "Configure the parallel driver to write "
			"a known value to the parallel interface on exit.",
		.usage = "('on'|'off')",
	},
	{
		.name = "verport_toggling_time",
		.handler = verport_handle_verport_toggling_time_command,
		.mode = COMMAND_CONFIG,
		.help = "Displays or assigns how many nanoseconds it "
			"takes for the hardware to toggle TCK.",
		.usage = "[nanoseconds]",
	},
	COMMAND_REGISTRATION_DONE
};

struct jtag_interface verport_interface = {
	.name = "verport",
	.supported = DEBUG_CAP_TMS_SEQ,
	.commands = verport_command_handlers,

	.init = verport_init,
	.quit = verport_quit,
	.khz = verport_khz,
	.speed_div = verport_speed_div,
	.speed = verport_speed,
	.execute_queue = bitbang_execute_queue,
};
