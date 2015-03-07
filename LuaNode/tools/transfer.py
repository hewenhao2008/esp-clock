#!/usr/bin/env python
#
# ESP8266 ROM batch file transfer utility
#

import sys
import serial
from time import sleep
import argparse
import glob
import os

# Serial port and inter char timeouts
_timeout = 0.5
# Interval between line writes
_line_interval = 0.5

# Convert a given string into aHEX sequence
def to_hex(str):
    return ":".join("{:02x}".format(ord(c)) for c in str)

# Readout all MCU output
def read_output():
    output = ""
    char = ''
    while True:
        char = _port.read(1)
        if char == '':
            break
        output = output + char 
        
    return output

# Transfer a single line
def write_line(line, verbose, force):
    while _port.inWaiting() > 0:
        _port.flushInput()
    
    if len(line) == 0:
        if verbose: print "skipping empty string"
        return
    
    if verbose:
        print "--> %s" % line
    
    _port.write(line + '\n')
    
    # If not forcing - expect output and input be the same (plus '> ')
    if not force:
        result = read_output()
        
        lines = result.split('\n')
        if lines[len(lines) - 1] != '> ':
            raise Exception("Invalid MCU result response: [%s]" % lines[len(lines) - 1])
        if lines[len(lines) - 2] != line + '\r':
            raise Exception("Invalid MCU transfer response: [%s]" % lines[len(lines) - 2])
    else:
        sleep(_line_interval)
        
    sys.stdout.flush()

# Transfer a file line by line
def transfer_file(path, verbose, force):
    message = "Transferring '%s' " % path
    if verbose:
        print message 
    else:
        sys.stdout.write(message)

    # Commands array - prepare all commands to be transfered
    commands = []
    base_name = os.path.basename(path)
    
    # Remove previous file instance and open file again for writing
    commands.append('file.open("%s", "w")' % base_name)
    commands.append('file.close()')
    commands.append('file.remove("%s")' % base_name)
    commands.append('file.open("%s", "w+")' % base_name)
    
    with open(path, 'r') as f:
        for line in f:
            commands.append('file.writeline([=====[%s]=====])' % line.rstrip('\n'))
            
    commands.append('file.flush()')
    commands.append('file.close()')
    
    # Experimental - compile and close files
    if base_name != 'init.lua':
        commands.append('node.compile("%s")' % base_name)
        commands.append('file.remove("%s")' % base_name)
    
    # Percent calculations
    size = len(commands)
    seq = 0
    percent_message = ''
    
    for line in commands:
        # Transfer line
        write_line(line, verbose, force)

        if not verbose:
            # Report progress
            progress = 100 * (seq + 1) / size
            seq += 1
            percent_message = '{:3d}%'.format(progress)
                 
            sys.stdout.write(percent_message)
            sys.stdout.flush()
    
            # Overwrite percentage
            if len(percent_message) > 0:
                sys.stdout.write(''.join('\b' for c in percent_message))
                sys.stdout.flush()

    if not verbose: print "done"
    sys.stdout.flush()
 

# Turn off the MCU and start it again
# RTS = CH_PD (i.e reset)
# DTR = GPIO0
def open_mcu():
    sys.stdout.write("Connecting...")
    _port.setDTR(True)
    _port.setRTS(True)
    sleep(0.5)
    _port.setDTR(False)
    _port.setRTS(False)
    sleep(0.5)
    print "done"
    sys.stdout.flush()
    

if __name__ == '__main__':
    # parse arguments or use defaults
    parser = argparse.ArgumentParser(description='ESP8266 Lua scripts uploader.')
    parser.add_argument('-p', '--port',    default='/dev/ttyUSB0', help='Device name, default /dev/ttyUSB0')
    parser.add_argument('-b', '--baud',    default=9600,           help='Baudrate, default 9600')
    parser.add_argument('-s', '--src',     default='./',           help='Source folder, default ./')
    parser.add_argument('-i', '--input',   default=False,          help='Read MCU output for N seconds. Do not transfer any files')
    parser.add_argument('-v', '--verbose', action='store_true',    help="Show progress messages.")
    parser.add_argument('-e', '--expected',default='> ',           help="Expected startup message ending.")
    parser.add_argument('-f', '--force',   action='store_true',    help="Ignore node output - force data transfer.")
    parser.add_argument('-r', '--restart', action='store_true',    help='Restart MCU after upload')
    parser.add_argument('-m', '--mask',    default='/*.lua',       help='File mask')
    args = parser.parse_args()
    
    _port = None
    try:
        print "Opening a serial port %s" % args.port
        _port = serial.Serial(args.port, args.baud)
        _port.timeout = _timeout
        _port.interCharTimeout = _timeout

        # Open the MCU and read initial output if not forced to transfer
        open_mcu()
        
        # Stop all timers
        if not args.input:
            for i in range(0, 6):
                write_line('tmr.stop(%d)' % i, False, True)
        
        startup = read_output()
        
        if not args.force and not args.input and args.expected != '*':
            if startup[-len(args.expected):] != args.expected:
                if args.verbose: print "[%s] : %s" % (to_hex(startup), startup)
                raise Exception("Invalid startup sequence: [%s] instead of [%s]" % (to_hex(startup[-len(args.expected):]), to_hex(args.expected)))
        
        # Exit if read-only parameter set
        if args.input:
            for i in range(int(args.input)):
                startup = startup.rstrip('\n')
                if len(startup) > 0:
                    print startup 
                startup = read_output()
                sys.stdout.flush()
                sleep(1)
            sys.exit(0)
        
        # Get list of files
        files = glob.glob(args.src + args.mask) 
        for f in files:
            transfer_file(f, args.verbose, args.force)
            
        if args.restart:
            write_line('node.restart()', args.verbose, args.force)
        
        print "File transfer done!"
    except Exception as details:
        print "Transfer failed: %s" % details
        sys.exit(1)
    finally:
        sys.stdout.flush()
        sys.stderr.flush()
        
        if _port != None:
            _port.close()