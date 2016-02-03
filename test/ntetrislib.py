import struct

REGISTER_TETRAD = 0
REGISTER_CLIENT = 1
UPDATE_TETRAD = 2
UPDATE_CLIENT_STATE = 3
DISCONNECT_CLIENT = 4
KICK_CLIENT = 5
CREATE_ROOM = 6
LIST_ROOMS = 7
ROOM_ANNOUNCE = 8
JOIN_ROOM = 9
USER_ACTION = 10
ERROR = 11
REG_ACK = 12
PING = 13
GAME_OVER = 14 
OPPONENT_ANNOUNCE = 15
CHAT_MSG = 16
NUM_MESSAGES = 17

UNSUPPORTED_MSG = 1 
ILLEGAL_MSG = 2 
BAD_LEN = 3 
BAD_PROTOCOL = 4 
BAD_NAME = 5 
BAD_ROOM_NAME = 6 
SUCCESS = 7 
BAD_PASSWORD = 8 
BAD_ROOM_NUM = 9 
BAD_NUM_PLAYERS = 10 
ROOM_FULL = 11 
ROOM_SUCCESS = 12 
GAME_BEGIN = 13


class Message:
    type = None
    length = 0
    version = 0

class OpponentMessage(Message):
    type = CHAT_MSG

    def __init__(self, pid=0, message=""):
        self.pid = pid
        self.message = message
    def unpack(self, msg):
        (version,type,self.pid, length) = struct.unpack_from("!BBIH", msg)
        self.message = struct.unpack_from("!%ds" % length, msg, offset=8)[0]
    def getPid(self):
        return self.pid
    def getMessage(self):
        return self.message
    def pack(self):
        return struct.pack("!BBIH%ds" % len(self.message), self.version, self.type, self.pid, len(self.message), bytes(self.message, 'utf-8'))


class OpponentAnnounce(Message):
    type = OPPONENT_ANNOUNCE

    def unpack(self, msg):
        (version,type,self.pid, length, self.status) = struct.unpack_from("!BBIBB", msg)
        self.name = struct.unpack_from('!%ds' % length, msg, offset=8)[0]
    def getName(self):
        return self.name
    def getPid(self):
        return self.pid
    def __str__(self):
        if self.status == 0:
            direction = "JOIN "
        else:
            direction = "PART "
        return direction + "OPPONENT: %s:%d" % (self.name, self.pid)


class RegisterTetrad(Message):
    type = REGISTER_TETRAD

    def unpack(self, msg):
        (version,type,self.shape) = struct.unpack("!BBB",msg)

    def getShape(self):
        return self.shape

class RegisterClient(Message): 
    type = REGISTER_CLIENT
    name = "" 

    def setName(self, name):
        self.name = name
        self.length = len(name)

    def pack(self):
        return struct.pack("!BBB%ds" % (self.length,), self.version, self.type, int(self.length), bytes(self.name, 'utf-8'))

    def __str__(self):
        return "REGISTER_CLIENT: val=(%s,%d)" % \
            (self.name, self.length,)

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
        (version,type,x,y,x0,y0,rot) = struct.unpack("!BBiiiii", msg)
        self.version = version
        self.type = type

        self.x = x
        self.y = y
        self.x0 = x0
        self.y0 = y0
        self.rot = rot

    def __str__(self):
        return "UPDATE_TETRAD: val=(%d,%d,%d,%d,%d)" % \
            (self.x, self.y, self.x0, self.y0, self.rot)

class RegAck(Message):
    type = REG_ACK
    
    def getUid(self):
        return self.uid

    def unpack(self, msg):
        (self.version,self.type,self.uid) = struct.unpack("!BBI", msg)
    
    def pack(self):
        return struct.pack("!BBI", self.version, self.type, self.uid)

    def __str__(self):
        return "REG_ACK: val=%d" % (self.uid)


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
        (version,type,nl,score,lvl,stat,nlc) = struct.unpack_from('!BBiiiBB', msg)
        self.version = version
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
    
    def __init__(self, uid):
        self.uid = uid
 
    def pack(self):
        return struct.pack("!BBI", self.version, self.type, self.uid)
       
class KickClient(Message):
    type = KICK_CLIENT

    def unpack(self, msg):
        (version,type,self.kickStatus,reasonLength) = struct.unpack_from('!BBBH',msg)
        self.reason = struct.unpack_from('!%ds' % reasonLength, msg, offset=5)  
    def __str__(self):
        return "Kicked because %s" % str(self.reason)

class ListRoom(Message):
    type = LIST_ROOMS

    def __init__(self, uid):
        self.uid = uid

    def pack(self):
        return struct.pack("!BBI", self.version, self.type, self.uid) 

class CreateRoom(Message):
    type = CREATE_ROOM
    length = 9

    def __init__(self, uid, nPlayers, roomName, password):
        self.uid = uid
        self.nPlayers = nPlayers
        self.roomName = roomName
        self.password = password
    
    def pack(self):
        string_len = len(self.roomName) + len(self.password)
        self.length = self.length + string_len 
        return struct.pack("!BBIBBB%ds" % string_len, 
                                         self.version, 
                                         self.type, 
                                         self.uid,
                                         self.nPlayers,
                                         len(self.roomName),
                                         len(self.password),
                                         bytes(self.roomName+self.password,'utf-8'))

    def __str__(self):
        return "New Room: %s %d %s" % (self.roomName,
                                       self.nPlayers,
                                       self.password)

class JoinRoom(Message):
    type = JOIN_ROOM
    length = 5

    def __init__(self, uid, rid, password = ''):
        self.uid = uid
        self.password = password
        self.rid = rid

    def pack(self):
        string_len = len(self.password)
        self.length = self.length + string_len
        return struct.pack("!BBIIB%ds" % string_len,
                                        self.version,
                                        self.type,
                                        self.uid,
                                        self.rid,
                                        len(self.password),
                                        bytes(self.password, 'utf-8'))

    def __str__(self):
        return "Join Room: id=%d" % (self.rid)
   
class RoomAnnounce(Message):
    type = ROOM_ANNOUNCE
    length = 18

    def unpack(self,msg):
        (self.version,\
        self.type,\
        self.roomId,\
        self.numPlayers,\
        self.numJoinedPlayers,\
        self.passwordProtected,\
        self.roomNameLen,) = struct.unpack_from('!BBIBBBB', msg)
        self.roomName = struct.unpack_from('!%ds' % self.roomNameLen, msg, offset=10)[0]

    def __str__(self):
        return "\t\tROOM ANNOUNCE: %d %s %d/%d %s" % (self.roomId,
                                               self.roomName,
                                               self.numJoinedPlayers,
                                               self.numPlayers,
                                               "LOCKED" if self.passwordProtected else "OPEN")
    def getRoomName(self):
        return str(self.roomName)
    def getId(self):
        return int(self.roomId)

class Ping(Message):
    type = PING
    
    def __init__(self, guid):
        self.guid = guid

    def pack(self):
        return struct.pack("!BBI", self.version, self.type, self.guid)

class UserAction(Message):
    type = USER_ACTION
    length = 8

    def setCmd(self, cmd):
        self.cmd = cmd

class ErrorMsg(Message):
    type = ERROR
   
    msg_dict = {}
    msg_dict[12] = "ROOM_SUCCESS"

    def unpack(self, msg):
        (version, type, val) = struct.unpack("!BBB", msg)

        self.version = version
        self.type = type
        self.val = val
    def getVal(self):
        return self.val

    def __str__(self):
        return "%s: val=%d" % (self.msg_dict.get(int(self.val),"ERROR"), int(self.val))

