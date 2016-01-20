#!/usr/bin/env python3
from ntetrislib import *
import queue
import struct
import socket
import sys
import random
import time
import threading
from select import select
from client_gui import *

g_msgQ = queue.Queue()
g_sendQ = queue.Queue()

g_app_exiting = False
g_room_dict = {}
g_cur_room = None
g_user_dict = {}
g_uid = None

def listener(sock):
    global g_msgQ
    global g_app_exiting

    print("[*] Starting recv thread")

    while not g_app_exiting:
        rlist,wlist,xlist = select([sock],[],[],1)
        if not rlist:
            continue
        for s in rlist:
            data, addr = s.recvfrom(1024)
            ip,port = addr
            print("[-] received msg from: %s" % str(ip))
            g_msgQ.put(data)

    print("[*] Leaving recv thread")

def sender(config):
    global g_sendQ
    global g_app_exiting

    sock, hostname, port = config

    print("[*] Starting send thread for server %s:%d" % (hostname, int(port)))

    while not g_app_exiting:
        if not g_sendQ.empty():
            data = g_sendQ.get()
            sock.sendto(data, (hostname, int(port)))
        time.sleep(0.5)
    if g_uid != None:
        print("[*] Disconnecting")
        sock.sendto(DisconnectClient(g_uid).pack(), (hostname, int(port)))
        sock.close()

    print("[*] Leaving send thread")

def pinger():
    global g_app_exiting
    global g_uid
    global g_sendQ

    if not g_app_exiting:
        if g_uid != None:
            g_sendQ.put(Ping(g_uid).pack())
            threading.Timer(2, pinger).start()

def commander():
    global g_app_exiting
    global g_uid
    global g_sendQ
    global g_cur_room
    print("[*] Starting command processor")

    while not g_app_exiting:
        data = input("")
        data = data.strip()
        
        if data == "panda":
            while not g_app_exiting:
                if g_uid != None:
                    g_sendQ.put(DisconnectClient(g_uid).pack())
                    g_uid = None
        if data == "ls":
            print("Rooms:")
            for room in g_room_dict:
                print(g_room_dict[room])
        if data == "create" and g_uid != None:
            name = input("Name: ")
            numPlayers = input("Number of players: ")
            password = input("Password: ")
            if name and numPlayers:
                msg = CreateRoom(g_uid, int(numPlayers), name, password)
            print(msg)
            g_sendQ.put(msg.pack())
        if data == "update" and g_uid != None:
            msg = ListRoom(g_uid)
            g_sendQ.put(msg.pack())
        if data == "join" and g_uid != None:
            rid = input("RoomId: ")
            password = input("Password: ")
            msg = JoinRoom(g_uid, int(rid), password) 
            g_sendQ.put(msg.pack())
            g_cur_room = int(rid)
        if data == "say" and g_uid != None:
            message = input("Say: ")
            msg = OpponentMessage(g_uid, message)
            g_sendQ.put(msg.pack())
            print("Me: " + message)
        if data == "quit":
            print("[*] Exiting!")
            g_app_exiting = True
        if data == "":
            print("ls create update join say quit")
    print("[*] Leaving command processor")

def gameThread(msgQ):
    t = Tetris(msgQ)
    t.show()


def startGame():
    tetradMsgQ = queue.Queue()
    stateMsgQ = queue.Queue()
    clientMsgQ = queue.Queue()

    game_thread = threading.Thread(target=gameThread, args={(tetradMsgQ, clientMsgQ, stateMsgQ),})
    game_thread.start()
    return (game_thread, tetradMsgQ, clientMsgQ, stateMsgQ)
 

def main():
    global g_msgQ
    global g_sendQ
    global g_app_exiting
    global g_uid
    global g_room_list
    global g_user_dict

    argv = sys.argv
    
    if len(argv) != 4:
        print("Usage: ./test.py hostname port name")
        exit(0)

    # Pop the head and ignore
    argv = argv[1:]

    hostname, port, name = argv[0], argv[1], argv[2]

    sock = socket.socket(socket.AF_INET,
                         socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("",0)) 
 
    listen_thread = threading.Thread(target=listener, args={sock,})
    listen_thread.start()

    command_thread = threading.Thread(target=commander, args={})
    command_thread.start()

    send_thread = threading.Thread(target=sender, args={(sock, hostname, port),})
    send_thread.start()

    message = RegisterClient()

    message.setName(name)   

    g_sendQ.put(message.pack())

    clientMsgQ = []
    tetradMsgQ = []
    stateMsgQ = []
    game_thread = []
    app = QtGui.QApplication(sys.argv)

    try:
        while not g_app_exiting:
            if not g_msgQ.empty():
                data = g_msgQ.get()
                if int(data[1]) == ERROR:
                    msg = ErrorMsg()
                    msg.unpack(data)
                    print(msg) 
                    if msg.getVal() == GAME_BEGIN:
                        print("Starting game")
                        game_thread, tetradMsgQ, clientMsgQ, stateMsgQ = startGame()
                elif int(data[1]) == UPDATE_TETRAD:
                    msg = UpdateTetrad()
                    msg.unpack(data)
                    print(msg)
                elif int(data[1]) == REGISTER_TETRAD:
                    msg = RegisterTetrad()
                    msg.unpack(data)
                    print("Got a tetrad")
                    tetradMsgQ.put(msg)

                elif int(data[1]) == UPDATE_CLIENT_STATE:
                    msg = UpdateClientState()
                    msg.unpack(data)
                    print(msg)
                elif int(data[1]) == REG_ACK:
                    msg = RegAck()
                    msg.unpack(data)
                    
                    print(msg)
                    g_sendQ.put(msg.pack())
                    
                    if g_uid == None:
                        g_uid = msg.getUid()
                        pinger()

                elif int(data[1]) == ROOM_ANNOUNCE:
                    msg = RoomAnnounce()
                    msg.unpack(data)

                    print(msg)

                    g_room_dict[msg.getId()] = msg
                elif int(data[1]) == KICK_CLIENT:
                    msg = KickClient()
                    msg.unpack(data)
                    
                    print(msg)
                    g_uid = None
                    g_sendQ.put(message.pack())
                elif int(data[1]) == OPPONENT_ANNOUNCE:
                    msg = OpponentAnnounce()
                    msg.unpack(data)
                    
                    print(msg)
                    
                    g_user_dict[msg.getPid()] = msg
                elif int(data[1]) == CHAT_MSG:
                    msg = OpponentMessage()
                    msg.unpack(data)

                    if msg.getPid() in g_user_dict.keys():
                        print(str(g_user_dict[msg.getPid()].getName()) + ": " + str(msg.getMessage()))


                else:
                    print('unrecognized pkt type=%d' % int(data[1]))
                    print('len(data) = %lu' % (len(data),))
                    print(data)
            time.sleep(0.1)
        print("[*] Leaving main thread")

    except KeyboardInterrupt:
          g_app_exiting = True

if __name__=="__main__":
    main()
