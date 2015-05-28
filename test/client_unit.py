#!/usr/bin/env python3

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
ERROR = 8
NUM_MESSAGES = 9


class Message:
    type = None
    length = 0


class RegisterTetrad(Message):
    type = REGISTER_TETRAD

class RegisterClient(Message):
    type = REGISTER_CLIENT
    name = "" 

    def setName(self, name):
        self.name = name
        self.length = len(name)
    def pack(self):
        return struct.pack("!BB%ds" % (self.length,), self.type, int(self.length), bytes(self.name, 'utf-8'))

class UpdateTetrad(Message):
    type = UPDATE_TETRAD
    length = 20
    
    def getOldPos(self):
        return (self.x, self.y)

    def getNewPos(self):
        return (sekf.x0, self.y0) 

    def getRotation(self, rot):
        return self.rot

    def unpack(self, msg):
        (type,x,y,x0,y0,rot) = struct.unpack("!Biiiii", msg)

        self.type = type

        self.x = x
        self.y = y
        self.x0 = x0
        self.y0 = y0
        self.rot = rot

    def __str__(self):
        return "UPDATE_TETRAD: val=(%d,%d,%d,%d,%d)" % \
            (self.x, self.y, self.x0, self.y0, self.rot)

class UpdateClientState(Message):
    type = UPDATE_CLIENT_STATE
    linesChanged = [];

    def getLines(self):
        return self.lines 

    def getScore(self):
        return self.score

    def getLevel(self):
        return self.level

    def getStatus(self):
        return self.status
 
    def getLinesChanges(self):
        return self.lines_changed    

    def unpack(self, msg):
        (type,nl,score,lvl,stat,nlc) = struct.unpack_from('!BiiiBB', msg)
        self.lines = nl
        self.score = score
        self.level = lvl
        self.status = stat
        self.linesChanged = [struct.unpack_from('!H', msg, offset=15+x*2)[0] for x in range(nlc)] 

    def __str__(self):
        return "UPDATE_CLIENT_STATE: val=(%d,%d,%d,%d), linesChanged=" % \
            (self.lines, self.score, self.level, self.status) + str(self.linesChanged)

        
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
class ErrorMsg(Message):
    type = ERROR
    
    def unpack(self, msg):
        (type, val) = struct.unpack("!BB", msg)

        self.type = type
        self.val = val

    def __str__(self):
        return "ERROR: val=%d" % (int(self.val))

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
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("",0)) 
 
    message = RegisterClient()

    message.setName("I am a test client")   
    sock.sendto(message.pack(), (hostname, int(port)))


    while True:
        try: 
            data, addr = sock.recvfrom(1024)
        except KeyboardInterrupt:
            print("Bye!")
            exit(0)
        print("received msg from: %s" % str(addr))
        if int(data[0]) == ERROR:
            msg = ErrorMsg()
            msg.unpack(data)
            print(msg) 
        elif int(data[0]) == UPDATE_TETRAD:
            msg = UpdateTetrad()
            msg.unpack(data)
            print(msg)
        elif int(data[0]) == UPDATE_CLIENT_STATE:
            msg = UpdateClientState()
            msg.unpack(data)
            print(msg)
            
        sock.sendto(message.pack(), (hostname, int(port)))
if __name__=="__main__":
    main()
