#!/usr/bin/env python3
from time import sleep
from typing import List

import serial
import pyxpudpserver as XPUDP


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

	def encode(self) -> bytes:
		"""
			Returns a two byte encoding of the frequency, where the first six bits represent
			the prefix, and the remaining ten bits represent the suffix in.
		"""
		if self.prefix < self.PREFIX_START or self.prefix > self.PREFIX_END:
			raise RuntimeError("Invalid frequency")

		prefix = format(self.prefix - 100, '06b')
		suffix = format(self.suffix, '010b')
		encoded = prefix + suffix
		return bytes([int(encoded[:8], 2), int(encoded[8:], 2)])

	@classmethod
	def from_string(cls, value: str) -> "Frequency":
		return cls(prefix=int(value[:3]), suffix=int(value[4:]))

	@classmethod
	def from_float(cls, value: int) -> "Frequency":
		value = str(int(value))
		return cls(prefix=int(value[:3]), suffix=int(value[3:]))

	def __eq__(self, other):
		if not isinstance(other, self.__class__):
			raise NotImplementedError
		return other.prefix == self.prefix and other.suffix == self.suffix


DREF_COM1_FREQ = "sim/cockpit2/radios/actuators/com1_frequency_hz_833"
DREF_COM1_STBY_FREQ = "sim/cockpit2/radios/actuators/com1_standby_frequency_hz_833"

TOLISS_RMP1_FREQ_UP_LARGE="AirbusFBW/RMP1FreqUpLrg"
TOLISS_RMP1_FREQ_DOWN_LARGE="AirbusFBW/RMP1FreqDownLrg"
TOLISS_RMP1_FREQ_UP_SMALL="AirbusFBW/RMP1FreqUpSml"
TOLISS_RMP1_FREQ_DOWN_SMALL="AirbusFBW/RMP1FreqDownSml"
TOLISS_RMP1_SWAP="AirbusFBW/RMPSwapCapt"

if __name__ == '__main__':
	XPUDP.pyXPUDPServer.initialiseUDP(("127.0.0.1", 49008), ("192.168.1.222", 49000), "hurricane")
	XPUDP.pyXPUDPServer.start()
	XPUDP.pyXPUDPServer.requestXPDref(DREF_COM1_FREQ)
	XPUDP.pyXPUDPServer.requestXPDref(DREF_COM1_STBY_FREQ)
	ser = serial.Serial('/dev/ttyACM0', 9600, timeout=1)
	ser.flush()
	lastActive = None
	lastStandby = None
	while True:
		if ser.in_waiting > 0:
			line = ser.readline().decode('utf-8').rstrip()
			if line == "cmd=OuterUp":
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
			active = Frequency.from_float(XPUDP.pyXPUDPServer.getData(DREF_COM1_FREQ))
			standby = Frequency.from_float(XPUDP.pyXPUDPServer.getData(DREF_COM1_STBY_FREQ))
			if lastActive is None or active != lastActive:
				encoded = active.encode()
				ser.write(b"\xff\x01" + encoded)
				ser.flush()
				lastActive = active
			if lastStandby is None or standby != lastStandby:
				encoded = standby.encode()
				ser.write(b"\xff\x00" + encoded)
				ser.flush()
				lastStandby = standby
		except Exception as e:
			print(e)
			pass
