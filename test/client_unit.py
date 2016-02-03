#!/usr/bin/env python3
from ntetrislib import *
import queue
import struct
import socket
import random
import time
import threading
from select import select
class NTetrisClient:
    def processor(self):
        while not self.g_app_exiting:
            if not self.g_msgQ.empty():
                data = self.g_msgQ.get()
                if int(data[1]) == ERROR:
                    msg = ErrorMsg()
                    msg.unpack(data)
                    if msg.getVal() == GAME_BEGIN:
                        print("Game begin")
                        self.stateMsgQ.put(msg)
                    else:
                        print(msg)
                elif int(data[1]) == UPDATE_TETRAD:
                    msg = UpdateTetrad()
                    msg.unpack(data)
                    print(msg)
                elif int(data[1]) == REGISTER_TETRAD:
                    msg = RegisterTetrad()
                    msg.unpack(data)
                    if self.tetradMsgQ.empty():
                        self.tetradMsgQ.put(msg)

                elif int(data[1]) == UPDATE_CLIENT_STATE:
                    msg = UpdateClientState()
                    msg.unpack(data)
                    print(msg)
                elif int(data[1]) == REG_ACK:
                    msg = RegAck()
                    msg.unpack(data)
                    
                    print(msg)
                    self.g_sendQ.put(msg.pack())
                    
                    if self.g_uid == None:
                        self.g_uid = msg.getUid()
                        self.pinger()

                elif int(data[1]) == ROOM_ANNOUNCE:
                    msg = RoomAnnounce()
                    msg.unpack(data)

                    print(msg)

                    self.g_room_dict[msg.getId()] = msg
                elif int(data[1]) == KICK_CLIENT:
                    msg = KickClient()
                    msg.unpack(data)
                    
                    print(msg)
                    self.g_uid = None
                    self.g_sendQ.put(message.pack())
                elif int(data[1]) == OPPONENT_ANNOUNCE:
                    msg = OpponentAnnounce()
                    msg.unpack(data)
                    
                    print(msg)
                    
                    self.g_user_dict[msg.getPid()] = msg
                elif int(data[1]) == CHAT_MSG:
                    msg = OpponentMessage()
                    msg.unpack(data)

                    if msg.getPid() in self.g_user_dict.keys():
                        print(str(self.g_user_dict[msg.getPid()].getName()) + ": " + str(msg.getMessage()))


                else:
                    print('unrecognized pkt type=%d' % int(data[1]))
                    print('len(data) = %lu' % (len(data),))
                    print(data)
            time.sleep(0.1)
        print("[*] Leaving main thread")

    def __init__(self, hostname, port, name):
        self.g_msgQ = queue.Queue()
        self.g_sendQ = queue.Queue()
        self.g_app_exiting = False
        self.g_room_dict = {}
        self.g_cur_room = None
        self.g_user_dict = {}
        self.g_uid = None


        sock = socket.socket(socket.AF_INET,
                             socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("",0)) 
     
        listen_thread = threading.Thread(target=self.listener, args={sock,})
        listen_thread.start()

        command_thread = threading.Thread(target=self.commander, args={})
        command_thread.start()

        send_thread = threading.Thread(target=self.sender, args={(sock, hostname, port),})
        send_thread.start()

        message_process_thread = threading.Thread(target=self.processor, args={})
        message_process_thread.start()

        message = RegisterClient()

        message.setName(name)   

        self.g_sendQ.put(message.pack())

        self.clientMsgQ = []
        self.tetradMsgQ = []
        self.stateMsgQ = []


    def listener(self, sock):

        print("[*] Starting recv thread")

        while not self.g_app_exiting:
            rlist,wlist,xlist = select([sock],[],[],1)
            if not rlist:
                continue
            for s in rlist:
                data, addr = s.recvfrom(1024)
                ip,port = addr
                self.g_msgQ.put(data)

        print("[*] Leaving recv thread")

    def sender(self, config):

        sock, hostname, port = config

        print("[*] Starting send thread for server %s:%d" % (hostname, int(port)))

        while not self.g_app_exiting:
            if not self.g_sendQ.empty():
                data = self.g_sendQ.get()
                sock.sendto(data, (hostname, int(port)))
            time.sleep(0.5)
        if self.g_uid != None:
            print("[*] Disconnecting")
            sock.sendto(DisconnectClient(self.g_uid).pack(), (hostname, int(port)))
            sock.close()

        print("[*] Leaving send thread")

    def pinger(self):

        if not self.g_app_exiting:
            if self.g_uid != None:
                self.g_sendQ.put(Ping(self.g_uid).pack())
                threading.Timer(2, self.pinger).start()

    def commander(self):
        
        print("[*] Starting command processor")

        while not self.g_app_exiting:
            data = input("")
            data = data.strip()
            
            if data == "ls":
                print("Rooms:")
                for room in self.g_room_dict:
                    print(self.g_room_dict[room])
            if data == "create" and self.g_uid != None:
                name = input("Name: ")
                numPlayers = input("Number of players: ")
                password = input("Password: ")
                if name and numPlayers:
                    msg = CreateRoom(self.g_uid, int(numPlayers), name, password)
                print(msg)
                self.g_sendQ.put(msg.pack())
            if data == "update" and self.g_uid != None:
                msg = ListRoom(self.g_uid)
                self.g_sendQ.put(msg.pack())
            if data == "join" and self.g_uid != None:
                rid = input("RoomId: ")
                password = input("Password: ")
                msg = JoinRoom(self.g_uid, int(rid), password) 
                self.g_sendQ.put(msg.pack())
                self.g_cur_room = int(rid)
            if data == "say" and self.g_uid != None:
                message = input("Say: ")
                msg = OpponentMessage(self.g_uid, message)
                self.g_sendQ.put(msg.pack())
            if data == "quit":
                print("[*] Exiting!")
                self.g_app_exiting = True
            if data == "":
                print("ls create update join say quit")
        print("[*] Leaving command processor")


    def startGame(self):
        self.tetradMsgQ = queue.Queue()
        self.stateMsgQ = queue.Queue()
        self.clientMsgQ = queue.Queue()

        return (self.tetradMsgQ, self.clientMsgQ, self.stateMsgQ)



