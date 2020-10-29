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
import curses
import os
import ssl

import nymea

def notification_sniffer(host, port):
    global notificationSocket
    global token
    global initialSetupRequired
    global pushButtonAuthAvailable
    global authenticationRequired

    try:
        notificationSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        notificationSocket.connect((host, port));

        # Perform initial handshake
        try:
            handshakeMessage = send_command("JSONRPC.Hello", {})
        except:
            context = ssl.create_default_context()
            context.check_hostname = False
            context.verify_mode = ssl.CERT_NONE
            try:
                notificationSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                notificationSocket.connect((host, port));
                notificationSocket = context.wrap_socket(sock)
                handshakeMessage = send_command("JSONRPC.Hello", {})
            except:
                print("SSL handshake failed.")

        handshakeData = handshakeMessage['params']
        print_json_format(handshakeData)
        print("Connected to", handshakeData["server"], "\nserver version:", handshakeData["version"], "\nprotocol version:", handshakeData["protocol version"], "\n")

        initialSetupRequired  = handshakeData['initialSetupRequired']
        authenticationRequired = handshakeData['authenticationRequired']
        pushButtonAuthAvailable = handshakeData['pushButtonAuthAvailable']

        data = ''
        while "}\n" not in data:
            chunk = notificationSocket.recv(4096)
            if chunk == '':
                raise RuntimeError("socket connection broken")
            data += chunk

        dataJson = json.loads(data)
        data = ''
        if 'notification' in dataJsondataJson:
            print(dataJson)

        # If we don't need any authentication, we are done
        # if not authenticationRequired:
        #     return True
        #
        # if initialSetupRequired and not pushButtonAuthAvailable:
        #     print("\n\n##############################################")
        #     print("# Start initial setup:")
        #     print("##############################################\n\n")
        #     result = createUser()
        #     while result['params']['error'] != "UserErrorNoError":
        #         print "Error creating user: %s" % userErrorToString(result['params']['error'])
        #         result = createUser()
        #
        #     print("\n\nUser created successfully.\n\n")
        #
        #
        # # Authenticate if no token
        # if authenticationRequired and token == None:
        #     if pushButtonAuthAvailable:
        #         pushbuttonAuthentication()
        #     else:
        #         loginResponse = login()
        #         while loginResponse['params']['success'] != True:
        #             print "Login failed. Please try again."
        #             loginResponse = login()
        #
        #         token = loginResponse['params']['token']
        #
        # return True
    except socket.error, e:
        print "ERROR:", e[1]," -> could not connect to nymea."
        print "       Please check if nymea is running on %s:%s" %(host,port)
        return False






    # try:
    #     x = None
    #     while (x !=ord('\n') and x != 27):
    #         socket_list = [sys.stdin, tn.get_socket()]
    #         read_sockets, write_sockets, error_sockets = select.select(socket_list , [], [])
    #         for sock in read_sockets:
    #             # notification messages:
    #             if sock == tn.get_socket():
    #                 packet = tn.read_until("}\n")
    #                 packet = json.loads(packet)
    #                 nymea.print_json_format(packet)
    #             elif sock == sys.stdin:
    #                 x = sys.stdin.readline()
    #                 return None
    # finally:
    #     tn.close()
    #     print "Notification socket closed."

def send_command(method, params = None):
    global commandId
    global notificationSocket
    global token
    global authenticationRequired

    commandObj = {}
    commandObj['id'] = commandId
    commandObj['method'] = method

    if authenticationRequired and token is not None:
        commandObj['token'] = token

    if not params == None and len(params) > 0:
        commandObj['params'] = params

    command = json.dumps(commandObj) + '\n'
    notificationSocket.send(command)

    # wait for the response with id = commandId
    responseId = -1
    while responseId != commandId:
        data = ''

        while "}\n" not in data:
            chunk = notificationSocket.recv(4096)
            if chunk == '':
                raise RuntimeError("socket connection broken")
            data += chunk

        response = json.loads(data)
        if 'notification' in response:
            continue
        responseId = response['id']
    commandId = commandId + 1

    # If this call was unautorized, authenticate
    if response['status'] == "unauthorized":
        debug_stop()
        print("Unautorized json call")
        if pushButtonAuthAvailable:
            pushbuttonAuthentication()
            return send_command(method, params)
        else:
            loginResponse = login()
            while loginResponse['params']['success'] != True:
                print "Login failed. Please try again."
                loginResponse = login()

            token = loginResponse['params']['token']
            return send_command(method, params)

    # if this call was not successfull
    if response['status'] != "success":
        print "JSON error happened: %s" % response['error']
        return None

    # Call went fine, return the response
    return response



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
