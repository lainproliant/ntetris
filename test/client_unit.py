#!/usr/bin/env python

from enum import Enum

class MessageType(Enum):
    REGISTER_TETRAD = 0 
    REGISTER_CLIENT = 1
    UPDATE_TETRAD = 2
    UPDATE_CLIENT_STATE = 3
    DISCONNECT_CLIENT = 4
    KICK_CLIENT = 5
    CREATE_ROOM = 6
    NUM_MESSAGES = 7


class Message:
	type = None
	length = 0

class RegisterTetrad(Message):
	type = MessageType.REGISTER_TETRAD

class RegisterClient(Message):
	type = MessageType.REGISTER_CLIENT
	length = 1
	name = "" 

	def setName(self, name):
		self.name = name
		length = length + len(name)

class UpdateTetrad(Message):
	type = MessageType.UPDATE_TETRAD
	length = 20
	
	def setOldPos(self, x, y):
		self.x = x
		self.y = y
	def setNewPos(self, x, y):
		self.x0 = x
		self.y0 = y
	def setRotation(self, rot):
		self.rot = rot

class UpdateClientState(Message):
	type = MessageType.UPDATE_CLIENT_STATE
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


def main():

	print("Hello world")

main()
