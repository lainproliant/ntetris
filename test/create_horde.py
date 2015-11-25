#!/usr/bin/env python3
from ntetrislib import *
import queue
import struct
import socket
import sys
import random
import time
import threading
import string
from select import select

g_msgQ = queue.Queue()
g_sendQ = queue.Queue()

g_app_exiting = False
g_uid_dict = {}

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
    global g_uid_dict
    sock, hostname, port = config

    print("[*] Starting send thread for server %s:%d" % (hostname, int(port)))

    while not g_app_exiting:
        if not g_sendQ.empty():
            data = g_sendQ.get()
            sock.sendto(data, (hostname, int(port)))
        time.sleep(0.5)
    print("[*] Disconnecting")
    for g_guid in g_uid_dict:
        sock.sendto(DisconnectClient(g_guid).pack(), (hostname, int(port)))
    sock.close()

    print("[*] Leaving send thread")

def pinger():
    global g_app_exiting
    global g_uid_dict
    global g_sendQ

    if not g_app_exiting:
        for g_guid in g_uid_dict:
            g_sendQ.put(Ping(g_guid).pack())
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
        if data == "quit":
            print("[*] Exiting!")
            g_app_exiting = True
    print("[*] Leaving command processor")

def create_random_room(g_uid):
    obj = CreateRoom(g_uid, 4, generate_random_name(), "")
    return obj

def generate_random_name():
    return ''.join(random.choice(string.ascii_uppercase + string.digits + string.ascii_lowercase) for _ in range(30))

def main():
    global g_msgQ
    global g_sendQ
    global g_app_exiting
    global g_uid_dict

    argv = sys.argv
    
    if len(argv) != 4:
        print("Usage: ./test.py hostname port max_clients")
        exit(0)

    # Pop the head and ignore
    argv = argv[1:]

    hostname, port, max_clients = argv[0], argv[1], int(argv[2])

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

    pinger()

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
                    
                    g_uid = msg.getUid()
                    g_uid_dict[g_uid] = {}

                    msg = create_random_room(g_uid)
                    g_sendQ.put(msg.pack())                    

                elif int(data[1]) == OPPONENT_ANNOUNCE:
                    msg = OpponentAnnounce()
                    msg.unpack(data)
                    
                    print(msg)
                    
                elif int(data[1]) == CHAT_MSG:
                    msg = OpponentMessage()
                    msg.unpack(data)

                    print(str(msg.getMessage()))

                else:
                    print('unrecognized pkt type=%d' % int(data[1]))
                    print('len(data) = %lu' % (len(data),))
                    print(data)

            if len(g_uid_dict) < max_clients:
                name = generate_random_name()
                message = RegisterClient()
                message.setName(name)   
                g_sendQ.put(message.pack())
            else:
                print("FULL")

            time.sleep(0.1)
        print("[*] Leaving main thread")

    except KeyboardInterrupt:
          g_app_exiting = True

if __name__=="__main__":
    main()

