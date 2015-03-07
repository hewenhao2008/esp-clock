#!/usr/bin/env python
#
# ESP8266 ROM wireless file transfer utility
#

import os
import sys
import socket, select, string
import argparse
from time import sleep

QUEUED      = 0 # Command enqueued but still not sent
SENT        = 1 # Command sent but not yet confirmed
CONFIRMED   = 2 # Command sent and confirmed
FAILED      = 3 # Command failed

commands = []
buffer = ""
last_print = ""
allow_send = True
packet_size = 50

# Enqueue a command into send queue
def enqueue(data):
    commands.append({ "T" : data, "+" : "Y|%s" % data, "-" : "N|%s" % data, "?" : QUEUED})

# Break a file into chunks and schedule send
def prepare_file(path, chunk):
    full_text = "" 
    with open(path, 'r') as f:
        full_text = f.read()
    
    # Prepare open file packet
    base_name = os.path.basename(path)
    enqueue("O|%d|%s" % (len(base_name), base_name))
    
    compiled_name = base_name[:-3:] + "lc"
    command = 'file.remove("%s")' % compiled_name
    enqueue("G|%d|%s" % (len(command), command))
    
    # Prepare open data packets
    while len(full_text) != 0:
        size = min(chunk, len(full_text))
        data = full_text[:size]
        full_text = full_text[size:]
        
        enqueue("W|%d|%s" % (len(data), data))
        
    enqueue("C|%d|%s" % (len(base_name), base_name))
    
    # If not initial script - compile it and remove the original
    if base_name != "init.lua" and False:
        command = 'node.compile("%s")' % base_name
        enqueue("G|%d|%s" % (len(command), command))
        
        command = 'file.remove("%s")' % base_name
        enqueue("G|%d|%s" % (len(command), command))

# Process received data - mark packets as transfered of failed
def process_packet(data):
    global buffer
    global allow_send
    
    buffer = buffer + data
    
    # We have enough data to parse a packet
    p_size = len("Y|?|") # prefix size
    if len(buffer) > p_size:
        index = string.find(buffer, "|", p_size)
        if index != -1:
            size = int(buffer[p_size:index])
            if index + size < len(buffer):
                payload = buffer[:index + size + 1]
                buffer = buffer[index + size + 1:]
                
                # We got a confirmation message
                for command in commands:
                    # Success
                    if command["+"] == payload and command["?"] == SENT: 
                        command["?"] = CONFIRMED
                        allow_send = True
                        
                    if command["-"] == payload and command["?"] == SENT: 
                        command["?"] = FAILED
                        raise Exception("Transport failure: %s" % payload)
                        
                if len(buffer) > 0:
                    process_packet("")

# Check if all packets are sent and confirmed
def check_completion():
    global commands
    global last_print
    
    sent = sum(item["?"] == SENT for item in commands)
    confirmed = sum(item["?"] == CONFIRMED for item in commands)
    total = len(commands)
    last_print = "Packets [ %d | %d | %d ]" % (confirmed, sent, total)
    
    sys.stdout.write(last_print)
    sys.stdout.flush()
    
    # Overwrite percentage
    if len(last_print) > 0:
        sys.stdout.write(''.join('\b' for c in last_print))
    
    return confirmed == total 

if __name__ == '__main__':
    # parse arguments or use defaults
    parser = argparse.ArgumentParser(description='ESP8266 Lua wireless scripts uploader.')
    parser.add_argument('-a', '--address', required = True,     help='Node IP Address.')
    parser.add_argument('-p', '--port',    default  = 20123,    help='Node port.')
    parser.add_argument('-s', '--src',     required = False,     help='Source file to be transfered.')
    # parser.add_argument('-z', '--packet',  default  = 50,       help='Maximal data packet size.')
    parser.add_argument('-r', '--restart', action='store_true', help="Restart node on transfer finish")
    parser.add_argument('-e', '--execute', action='store_true', help="Execute the uploaded file")
    parser.add_argument('-c', '--command', default='',          help="Run a given command")
    args = parser.parse_args()

    try:
        # packet_size = args.packet
        if len(args.command) > 0:
            enqueue("G|%d|%s" % (len(args.command), args.command))
        elif  len(args.src) > 0:
            prepare_file(args.src, packet_size)
            if args.restart:
                command = "tmr.alarm(5, 2000, 0, function() node.restart() end)"
                enqueue("G|%d|%s" % (len(command), command))
            elif args.execute:
                if args.src[-3:] != 'lua':
                    print ".lua file expected, got " + args.src[-3:]
                    exit(1)
                else:
                    if base_name != "init.lua":
                        base_name = os.path.basename(args.src)[:-3:] + "lc"
                    command = "dofile(%s)" % base_name
                    enqueue("G|%d|%s" % (len(command), command))
        else:
            print "Nothing to do!"
            exit(0)
        
        if len(commands) == 0:
            print "Nothing to do!"
            exit(0)
        
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2)
        s.connect((args.address, args.port))
        
        send_num = 0
        spin = True
        
        print "Transfering data [ confirmed | sent | total ]"
        
        # Wait until transfer finished
        while spin:
            socket_list = [s]
            
            # Get the list sockets which are readable
            read_sockets, write_sockets, error_sockets = select.select(socket_list , socket_list, [])

            # Receive data
            for sock in read_sockets:
                #incoming message from remote server
                if sock == s:
                    data = sock.recv(4096)
                    if not data:
                        raise Exception("Connection closed by MCU")
                    else :
                        # Handle commands
                        process_packet(data)
                        spin = not check_completion()
            
            # Transfer data
            for sock in write_sockets:
                if send_num >= len(commands):
                    break
                
                if commands[send_num]["?"] == QUEUED and allow_send:
                    sock.send(commands[send_num]["T"])
                    commands[send_num]["?"] = SENT
                    send_num = send_num + 1
                    allow_send = False

        s.shutdown(socket.SHUT_RDWR)
        s.close()
        print "\nFile transfer done!"
    except Exception as details:
        print "\nTransfer failed: %s" % details
        sys.exit(1)
    finally:
        sys.stdout.flush()
        sys.stderr.flush()