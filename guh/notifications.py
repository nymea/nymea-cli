# -*- coding: UTF-8 -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
#                                                                         #
#  Copyright (C) 2015 Simon Stuerz <simon.stuerz@guh.guru>                #
#                                                                         #
#  This file is part of guh-cli.                                          #
#                                                                         #
#  guh-cli is free software: you can redistribute it and/or modify        #
#  it under the terms of the GNU General Public License as published by   #
#  the Free Software Foundation, version 2 of the License.                #
#                                                                         #
#  guh-cli is distributed in the hope that it will be useful,             #
#  but WITHOUT ANY WARRANTY; without even the implied warranty of         #
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the           #
#  GNU General Public License for more details.                           #
#                                                                         #
#  You should have received a copy of the GNU General Public License      #
#  along with guh. If not, see <http://www.gnu.org/licenses/>.            #
#                                                                         #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 

import sys
import socket
import json
import select
import telnetlib

import guh

def notification_sniffer():
    global commandId
    
    commandId = 0
    
    # Create notification handler
    host='localhost'
    port=1234
    if len(sys.argv) > 1:
	host = sys.argv[1]
    if len(sys.argv) > 2:
	port = sys.argv[2]
    
    print "Connecting notification handler..."
    try:
	tn = telnetlib.Telnet(host, port)
    except :
        print "ERROR: notification socket could not connect the to guh-server. \n"
        return None
    print "...OK \n"
    
    #enable_notification(notificationSocket)
    enable_notification(tn.get_socket())
    
    try:
	x = None
	while (x !=ord('\n') and x != 27):
	    socket_list = [sys.stdin, tn.get_socket()]
	    read_sockets, write_sockets, error_sockets = select.select(socket_list , [], [])
	    for sock in read_sockets:
		# notification messages:
		if sock == tn.get_socket():
		    packet = tn.read_until("\n}\n")
		    packet = json.loads(packet)
		    guh.print_json_format(packet)
		elif sock == sys.stdin:
		    x = sys.stdin.readline()
		    return None
    finally:
	tn.close()
	print "Notification socket closed."

    
def enable_notification(notifySocket):
    global commandId
    
    params = {}
    commandObj = {}
    commandObj['id'] = commandId
    commandObj['method'] = "JSONRPC.SetNotificationStatus"
    params['enabled'] = "true"
    commandObj['params'] = params
    command = json.dumps(commandObj) + '\n'
    commandId = commandId + 1
    notifySocket.send(command)
