#!/usr/bin/env python

from enum import Enum
import struct
import socket
import sys

REGISTER_TETRAD = 0 
REGISTER_CLIENT = 1
UPDATE_TETRAD = 2
UPDATE_CLIENT_STATE = 3
DISCONNECT_CLIENT = 4
KICK_CLIENT = 5
CREATE_ROOM = 6
USER_ACTION = 7
NUM_MESSAGES = 8 


class Message:
	type = None
	length = 0


class RegisterTetrad(Message):
	type = REGISTER_TETRAD

	def pack(self):
		return struct.pack("!Bh", self.type, 0)

class RegisterClient(Message):
	type = REGISTER_CLIENT
	name = "" 

	def setName(self, name):
		self.name = name
		self.length = self.length + len(name)
	def pack(self):
		return struct.pack("!Bhs", self.type, int(self.length), bytes(self.name, 'utf-8'))

		

class UpdateTetrad(Message):
	type = UPDATE_TETRAD
	length = 20
	
	def setOldPos(self, x, y):
		self.x = x
		self.y = y
	def setNewPos(self, x, y):
		self.x0 = x
		self.y0 = y
	def setRotation(self, rot):
		self.rot = rot
	def pack(self):
		return struct.pack("!Bhiiiii", self.x, self.y, self.x0, self.y0, self.rot)

class UpdateClientState(Message):
	type = UPDATE_CLIENT_STATE
	length = 14

	def setLines(self, lines):
		self.lines = lines
	
	def setScore(self, score):
		self.score = score

	def setLevel(self, level):
		self.level = level

	def setStatus(self, status):
		self.status = status
	
	def setLinesChanges(self, lines_changed):
		self.lines_changed = lines_changed

class DisconnectClient(Message):
	type = DISCONNECT_CLIENT
		
class KickClient(Message):
	type = KICK_CLIENT

class CreateRoom(Message):
	type = CREATE_ROOM
	length = 16

	def setNumPlayers(self, num):
		self.numPlayers = num
	
	def setNameLen(self, length):
		self.nameLength = length

	def setName(self, name):
		self.name = name

class UserAction(Message):
	type = USER_ACTION
	length = 8

	def setCmd(self, cmd):
		self.cmd = cmd
def main():
	argv = sys.argv
	
	if len(argv) != 3:
		print("Usage: ./test.py hostname port")
		exit(0)

	# Pop the head and ignore
	argv = argv[1:]

	hostname, port = argv[0], argv[1]

	sock = socket.socket(socket.AF_INET,
	                     socket.SOCK_DGRAM)

	message = RegisterClient()

	message.setName("I am a test client")	
	sock.sendto(message.pack(), (hostname, int(port)))
main()
