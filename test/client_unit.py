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

g_msgQ = queue.Queue()
g_sendQ = queue.Queue()

g_app_exiting = False
g_room_list = []

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

    sock.close()

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
    if g_uid != None:
        print("[*] Disconnecting")
        sock.sendto(DisconnectClient(g_uid).pack(), (hostname, int(port)))

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

    print("[*] Starting command processor")

    while not g_app_exiting:
        data = input("")
        data = data.strip()
        
        if data == "ls":
            print("Rooms:")
            for room in g_room_list:
                print(room)
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
            pass
        if data == "quit":
            print("[*] Exiting!")
            g_app_exiting = True
        if data == "":
            print("ls create update join quit")
    print("[*] Leaving command processor")


def main():
    global g_msgQ
    global g_sendQ
    global g_app_exiting
    global g_uid
    global g_room_list

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

    try:
        while not g_app_exiting:
            if not g_msgQ.empty():
                data = g_msgQ.get()
                if int(data[1]) == ERROR:
                    msg = ErrorMsg()
                    msg.unpack(data)
                    print(msg) 
                elif int(data[1]) == UPDATE_TETRAD:
                    msg = UpdateTetrad()
                    msg.unpack(data)
                    print(msg)
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

                    g_room_list.append(msg.getRoomName())

                else:
                    print('unrecognized pkt type')
                    print('len(data) = %lu' % (len(data),))
                    print(data)
        print("[*] Leaving main thread")

    except KeyboardInterrupt:
          g_app_exiting = True

if __name__=="__main__":
    main()
