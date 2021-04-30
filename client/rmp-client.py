#!/usr/bin/env python3

import click
import pyxpudpserver as XPUDP
import serial


class Frequency:
	PREFIX_START = 118
	PREFIX_END = 136

	def __init__(self, prefix: int, suffix: int):
		self.prefix = prefix
		self.suffix = suffix

	def __str__(self):
		return f"{self.prefix}.{self.suffix}"

	def __float__(self):
		return float((self.prefix * 1000) + self.suffix)

	@property
	def is_testing(self):
		return self.prefix == 888 and self.suffix == 888

	def encode(self) -> bytes:
		"""
			Returns a two byte encoding of the frequency, where the first six bits represent
			the prefix, and the remaining ten bits represent the suffix in.
		"""
		if not self.is_testing:
			if self.prefix < self.PREFIX_START or self.prefix > self.PREFIX_END:
				raise RuntimeError("Invalid frequency")
			prefix = format(self.prefix - 100, '06b')
		else:
			prefix = "000000"
		suffix = format(self.suffix, '010b')
		encoded = prefix + suffix
		return bytes([int(encoded[:8], 2), int(encoded[8:], 2)])

	@classmethod
	def from_string(cls, value: str) -> "Frequency":
		if not len(value) == 6:
			raise ValueError(f"Expected frequency to have exactly 7 characters, got {len(value)}")
		if value[4] != ".":
			raise ValueError(f'Expected a dot at position 4, got "{value[4]}"')
		return cls(prefix=int(value[:3]), suffix=int(value[4:]))

	@classmethod
	def from_digits(cls, value: int) -> "Frequency":
		value = str(int(value))
		if not len(value) == 6:
			raise ValueError(f"Expected frequency to have exactly 6 digits, got {len(value)} ({value})")
		return cls(prefix=int(value[:3]), suffix=int(value[3:]))

	def __eq__(self, other):
		if not isinstance(other, self.__class__):
			raise NotImplementedError
		return other.prefix == self.prefix and other.suffix == self.suffix


DREF_COM1_FREQ = "sim/cockpit2/radios/actuators/com1_frequency_hz_833"
DREF_COM1_STBY_FREQ = "sim/cockpit2/radios/actuators/com1_standby_frequency_hz_833"
DREF_ANNUNCIATOR_TEST = "AirbusFBW/AnnunMode"
DREF_PANEL_BRIGHTNESS = "AirbusFBW/PanelBrightnessLevel"
DREF_RMP1_SWITCH = "AirbusFBW/RMP1Switch"

TOLISS_RMP1_FREQ_UP_LARGE="AirbusFBW/RMP1FreqUpLrg"
TOLISS_RMP1_FREQ_DOWN_LARGE="AirbusFBW/RMP1FreqDownLrg"
TOLISS_RMP1_FREQ_UP_SMALL="AirbusFBW/RMP1FreqUpSml"
TOLISS_RMP1_FREQ_DOWN_SMALL="AirbusFBW/RMP1FreqDownSml"
TOLISS_RMP1_SWAP="AirbusFBW/RMPSwapCapt"

CMD_SET_STANDBY_FREQ = b"\x00"
CMD_SET_ACTIVE_FREQ = b"\x01"
CMD_SET_BRIGHTNESS = b"\x02"
CMD_SET_POWER = b"\x03"

@click.command()
@click.option(
	"--serial-port", required=True,
	help="The serial Arduino port. Usually COM1 on Windows, and /dev/ttyACM0 on Linux.",
)
@click.option(
	"--xp-host", required=True,
	help="The address of the host that is running X-Plane.",
)
@click.option(
	"--xp-port", type=int, default=49000, show_default=True,
	help="The port of the host machine on which X-Plane is listening.",
)
@click.option(
	"--host", default="127.0.0.1", show_default=True,
	help="The address of the local device that is running this script.",
)
@click.option(
	"--port", type=int, default=49008, show_default=True,
	help="The address of the local device that is running this script.",
)
@click.option(
	"--scale-brightness", type=click.IntRange(min=0, max=7), metavar="LEVEL", default=3,
	help="The factor to scale the pedestal lighting by",
)
def run(serial_port, scale_brightness, host, port, xp_host, xp_port):
	"""
	This script will run the arduino-radio Radio Management Panel client. It will
	connect to an Arduino device via serial USB and a running X-Plane instance via UDP.
	"""
	XPUDP.pyXPUDPServer.initialiseUDP((host, port), (xp_host, xp_port), "hurricane")
	XPUDP.pyXPUDPServer.start()
	XPUDP.pyXPUDPServer.requestXPDref(DREF_COM1_FREQ)
	XPUDP.pyXPUDPServer.requestXPDref(DREF_COM1_STBY_FREQ)
	XPUDP.pyXPUDPServer.requestXPDref(DREF_ANNUNCIATOR_TEST)
	XPUDP.pyXPUDPServer.requestXPDref(DREF_PANEL_BRIGHTNESS)
	XPUDP.pyXPUDPServer.requestXPDref(DREF_RMP1_SWITCH)
	ser = serial.Serial(serial_port, 9600, timeout=1)
	ser.flush()
	lastActive = None
	lastStandby = None
	lastBrightness = None
	lastPower = None
	try:
		while True:
			if ser.in_waiting > 0:
				line = ser.readline().decode('utf-8').rstrip()
				if line == "reset":
					lastActive = None
					lastStandby = None
					lastBrightness = None
					lastPower = None
				elif line == "cmd=OuterUp":
					XPUDP.pyXPUDPServer.sendXPCmd(TOLISS_RMP1_FREQ_UP_LARGE)
				elif line == "cmd=OuterDown":
					XPUDP.pyXPUDPServer.sendXPCmd(TOLISS_RMP1_FREQ_DOWN_LARGE)
				elif line == "cmd=InnerUp":
					XPUDP.pyXPUDPServer.sendXPCmd(TOLISS_RMP1_FREQ_UP_SMALL)
				elif line == "cmd=InnerDown":
					XPUDP.pyXPUDPServer.sendXPCmd(TOLISS_RMP1_FREQ_DOWN_SMALL)
				elif line == "cmd=Swap":
					XPUDP.pyXPUDPServer.sendXPCmd(TOLISS_RMP1_SWAP)
				else:
					print(f"Unrecognized line {line}")
			try:
				flush = False
				switch = XPUDP.pyXPUDPServer.getData(DREF_ANNUNCIATOR_TEST)
				testing = switch == 2
				readBrightness = XPUDP.pyXPUDPServer.getData(DREF_PANEL_BRIGHTNESS)
				brightness = int(readBrightness * scale_brightness)
				active = None
				standby = None
				read_power = XPUDP.pyXPUDPServer.getData(DREF_RMP1_SWITCH)
				power = read_power == 1

				if not testing:
					com1_freq = XPUDP.pyXPUDPServer.getData(DREF_COM1_FREQ)
					com1_stby_freq = XPUDP.pyXPUDPServer.getData(DREF_COM1_STBY_FREQ)
					if com1_freq != 0:
						active = Frequency.from_digits(com1_freq)
					if com1_stby_freq != 0:
						standby = Frequency.from_digits(com1_stby_freq)
				else:
					active = Frequency.from_digits(888888)
					standby = Frequency.from_digits(888888)
				if active is not None and (lastActive is None or active != lastActive):
					encoded = active.encode()
					ser.write(b"\xff" + CMD_SET_ACTIVE_FREQ + encoded)
					lastActive = active
					flush = True
				if standby is not None and (lastStandby is None or standby != lastStandby):
					encoded = standby.encode()
					ser.write(b"\xff" + CMD_SET_STANDBY_FREQ + encoded)
					lastStandby = standby
					flush = True
				if brightness is not None and (lastBrightness is None or brightness != lastBrightness):
					ser.write(b"\xff" + CMD_SET_BRIGHTNESS + bytes([brightness]) + b"\x00")
					lastBrightness = brightness
					flush = True
				if power is not None and (lastPower is None or power != lastPower):
					ser.write(b"\xff" + CMD_SET_POWER + bytes([1 if power else 0]) + b"\x00")
					lastPower = power
					flush = True
				if flush:
					ser.flush()
			except Exception as e:
				print(e)
				pass
	finally:
		ser.close()
		XPUDP.pyXPUDPServer.quit()


if __name__ == "__main__":
	run()
