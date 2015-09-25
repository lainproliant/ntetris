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
NUM_MESSAGES = 13


class Message:
    type = None
    length = 0
    version = 0


class RegisterTetrad(Message):
    type = REGISTER_TETRAD

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
        return "ROOM ANNOUNCE: %s %d/%d %s" % (self.roomName,
                                               self.numJoinedPlayers,
                                               self.numPlayers,
                                               "LOCKED" if self.passwordProtected else "OPEN")
    def getRoomName(self):
        return str(self.roomName)



class UserAction(Message):
    type = USER_ACTION
    length = 8

    def setCmd(self, cmd):
        self.cmd = cmd
class ErrorMsg(Message):
    type = ERROR
    
    def unpack(self, msg):
        (version, type, val) = struct.unpack("!BBB", msg)

        self.version = version
        self.type = type
        self.val = val

    def __str__(self):
        return "ERROR: val=%d" % (int(self.val))

