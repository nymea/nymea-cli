# -*- coding: UTF-8 -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
#                                                                         #
#  Copyright (C) 2015 - 2018 Simon Stuerz <simon.stuerz@guh.io>           #
#                                                                         #
#  This file is part of nymea-cli.                                        #
#                                                                         #
#  nymea-cli is free software: you can redistribute it and/or modify      #
#  it under the terms of the GNU General Public License as published by   #
#  the Free Software Foundation, version 2 of the License.                #
#                                                                         #
#  nymea-cli is distributed in the hope that it will be useful,           #
#  but WITHOUT ANY WARRANTY; without even the implied warranty of         #
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the           #
#  GNU General Public License for more details.                           #
#                                                                         #
#  You should have received a copy of the GNU General Public License      #
#  along with nymea-cli. If not, see <http://www.gnu.org/licenses/>.      #
#                                                                         #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 

import sys
import socket
import json
import select
import telnetlib

import nymea

def notification_sniffer(nymeaHost, nymeaPort):
    global commandId
    
    commandId = 0
    print "Connecting notification handler..."
    try:
        tn = telnetlib.Telnet(nymeaHost, nymeaPort)
    except :
        print "ERROR: notification socket could not connect the to nymea-server. \n"
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
                    packet = tn.read_until("}\n")
                    packet = json.loads(packet)
                    nymea.print_json_format(packet)
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



