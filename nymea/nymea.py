# -*- coding: UTF-8 -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#                                                                         #
#  Copyright (C) 2015 - 2018 Simon Stuerz <simon.stuerz@nymea.io>         #
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

import things
import events
import actions
import rules
import selector
import settings
import getpass

commandId = 0
token = None
pushButtonAuthAvailable = False
authenticationRequired = False
initialSetupRequired = False

def init_connection(host, port):
    global sock
    global token
    global initialSetupRequired
    global pushButtonAuthAvailable
    global authenticationRequired

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((host, port));

        # Perform initial handshake
        try:
            handshakeMessage = send_command("JSONRPC.Hello", {})
        except:
            context = ssl.create_default_context()
            context.check_hostname = False
            context.verify_mode = ssl.CERT_NONE
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.connect((host, port));
                sock = context.wrap_socket(sock)
                handshakeMessage = send_command("JSONRPC.Hello", {})
            except:
                print("SSL handshake failed.")

        handshakeData = handshakeMessage['params']
        print_json_format(handshakeData)
        print("Connected to", handshakeData["server"], "\nserver version:", handshakeData["version"], "\nprotocol version:", handshakeData["protocol version"], "\n")

        initialSetupRequired  = handshakeData['initialSetupRequired']
        authenticationRequired = handshakeData['authenticationRequired']
        pushButtonAuthAvailable = handshakeData['pushButtonAuthAvailable']

        # If we don't need any authentication, we are done
        if not authenticationRequired:
            return True

        if initialSetupRequired and not pushButtonAuthAvailable:
            print("\n\n##############################################")
            print("# Start initial setup:")
            print("##############################################\n\n")
            result = createUser()
            while result['params']['error'] != "UserErrorNoError":
                print "Error creating user: %s" % userErrorToString(result['params']['error'])
                result = createUser()

            print("\n\nUser created successfully.\n\n")


        # Authenticate if no token
        if authenticationRequired and token == None:
            if pushButtonAuthAvailable:
                pushbuttonAuthentication()
            else:
                loginResponse = login()
                while loginResponse['params']['success'] != True:
                    print "Login failed. Please try again."
                    loginResponse = login()

                token = loginResponse['params']['token']

        return True
    except socket.error, e:
        print "ERROR:", e[1]," -> could not connect to nymea."
        print "       Please check if nymea is running on %s:%s" %(host,port)
        return False

def createUser():
    user = raw_input("Please enter email for new user: " )
    if not user:
        user = getpass.getuser()
    pprompt = lambda: (getpass.getpass(), getpass.getpass('Retype password: '))
    p1, p2 = pprompt()
    while p1 != p2:
        print('Passwords do not match. Try again')
        p1, p2 = pprompt()

    params = {}
    params['username'] = user
    params['password'] = p1
    return send_command("JSONRPC.CreateUser", params)


def userErrorToString(error):
    return {
        'UserErrorBadPassword': "Password failed character validation",
        'UserErrorBackendError': "Error creating user database",
        'UserErrorInvalidUserId': "Invalid username. Must be an email address",
        'UserErrorDuplicateUserId': "Username does already exist",
        'UserErrorTokenNotFound': "Invalid token supplied",
        'UserErrorPermissionDenied': "Permission denied"
    }[error]


def login():
    print("\n\n##############################################")
    print("# Login:")
    print("##############################################\n\n")
    user = raw_input("Username: ")
    password = getpass.getpass()
    params = {}
    params['username'] = user
    params['password'] = password
    params['deviceName'] = "nymea-cli"
    return send_command("JSONRPC.Authenticate", params)


def pushbuttonAuthentication():
    global sock
    global commandId
    global token

    if token != None:
        return

    print "\n\nUsing push button authentication method...\n\n"

    params = {}
    params['deviceName'] = 'nymea-cli'

    commandObj = {}
    commandObj['id'] = commandId
    commandObj['params'] = params
    commandObj['method'] = 'JSONRPC.RequestPushButtonAuth'
    command = json.dumps(commandObj) + '\n'
    sock.send(command)

    # wait for the response with id = commandId
    responseId = -1
    while responseId != commandId:

        data = ''
        while "}\n" not in data:
            chunk = sock.recv(4096)
            if chunk == '':
                raise RuntimeError("socket connection broken")
            data += chunk

        response = json.loads(data)
        responseId = response['id']

    commandId = commandId + 1

    # check response
    print("Initialized push button authentication. Response:")
    print_json_format(response)

    print("\n\n##############################################")
    print("# Please press the pushbutton on the device. #")
    print("##############################################\n\n")
    # wait for push button notification
    while True:
        data = ''
        while "}\n" not in data:
            chunk = sock.recv(4096)
            if chunk == '':
                raise RuntimeError("socket connection broken")
            data += chunk

        response = json.loads(data)
        if ('notification' in response) and response['notification'] == "JSONRPC.PushButtonAuthFinished":
            print("Notification received:")
            print_json_format(response)
            if response['params']['success'] == True:
                print("\nAuthenticated successfully!\n")
                print("Token: %s" % response['params']['token'])
                debug_stop()
                token = response['params']['token']
                return



def send_command(method, params = None):
    global commandId
    global sock
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
    sock.send(command)

    # wait for the response with id = commandId
    responseId = -1
    while responseId != commandId:
        data = ''

        while "}\n" not in data:
            chunk = sock.recv(4096)
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


def get_selection(title, options):
    return selector.get_selection(title, options)


def debug_stop():
    raw_input("\nDEBUG STOP: Press \"enter\" to continue...\n")


def get_unit_string(unit):
    if unit == "UnitSeconds":
        return "s"
    elif unit == "UnitMinutes":
        return "m"
    elif unit == "UnitHours":
        return "h"
    elif unit == "UnitUnixTime":
        return "s"
    elif unit == "UnitMeterPerSecond":
        return "m/s"
    elif unit == "UnitKiloMeterPerHour":
        return "km/h"
    elif unit == "UnitDegree":
        return "°"
    elif unit == "UnitRadiant":
        return "rad"
    elif unit == "UnitDegreeCelsius":
        return "°C"
    elif unit == "UnitDegreeKelvin":
        return "°K"
    elif unit == "UnitMilliBar":
        return "mbar"
    elif unit == "UnitBar":
        return "bar"
    elif unit == "UnitPascal":
        return "Pa"
    elif unit == "UnitHectoPascal":
        return "hPa"
    elif unit == "UnitAtmosphere":
        return "atm"
    elif unit == "UnitLumen":
        return "lm"
    elif unit == "UnitLux":
        return "lx"
    elif unit == "UnitCandela":
        return "cd"
    elif unit == "UnitMilliMeter":
        return "mm"
    elif unit == "UnitCentiMeter":
        return "cm"
    elif unit == "UnitMeter":
        return "m"
    elif unit == "UnitKiloMeter":
        return "km"
    elif unit == "UnitGram":
        return "g"
    elif unit == "UnitKiloGram":
        return "kg"
    elif unit == "UnitDezibel":
        return "db"
    elif unit == "UnitKiloByte":
        return "kB"
    elif unit == "UnitMegaByte":
        return "MB"
    elif unit == "UnitGigaByte":
        return "GB"
    elif unit == "UnitTeraByte":
        return "TB"
    elif unit == "UnitMilliWatt":
        return "mW"
    elif unit == "UnitWatt":
        return "W"
    elif unit == "UnitKiloWatt":
        return "kW"
    elif unit == "UnitKiloWattHour":
        return "kWh"
    elif unit == "UnitEuroPerMegaWattHour":
        return "€/MWh"
    elif unit == "UnitPercentage":
        return "%"
    elif unit == "UnitEuro":
        return "€"
    elif unit == "UnitDollar":
        return "$"
    else:
        return "<unknown unit>"


def get_valueOperator_string(valueOperator):
    if valueOperator == "ValueOperatorEquals":
        return "="
    elif valueOperator == "ValueOperatorNotEquals":
        return "!="
    elif valueOperator == "ValueOperatorLess":
        return "<"
    elif valueOperator == "ValueOperatorGreater":
        return ">"
    elif valueOperator == "ValueOperatorLessOrEqual":
        return "<="
    elif valueOperator == "ValueOperatorGreaterOrEqual":
        return ">="
    else:
        return "<unknown value operator>"


def get_stateEvaluator_string(stateEvaluator):
    if stateEvaluator == "StateOperatorAnd":
        return "&"
    elif stateEvaluator == "StateOperatorOr":
        return "|"
    else:
        return "<unknown state evaluator>"


def print_thing_error_code(thingError):

    if thingError == None:
        print "timeout"
        return

    if thingError == "ThingErrorNoError":
        print "\nSuccess! (", thingError, ")"
    elif thingError == "ThingErrorPluginNotFound":
        print "\nERROR: the plugin could not be found. (", thingError, ")"
    elif thingError == "ThingErrorVendorNotFound":
        print "\nERROR: the vendor could not be found. (", thingError, ")"
    elif thingError == "ThingErrorThingNotFound":
        print "\nERROR: the thing could not be found. (", thingError, ")"
    elif thingError == "ThingErrorthingClassNotFound":
        print "\nERROR: the thingClass could not be found. (", thingError, ")"
    elif thingError == "ThingErrorActionTypeNotFound":
        print "\nERROR: the actionType could not be found. (", thingError, ")"
    elif thingError == "ThingErrorStateTypeNotFound":
        print "\nERROR: the stateType could not be found. (", thingError, ")"
    elif thingError == "ThingErrorEventTypeNotFound":
        print "\nERROR: the eventType could not be found. (", thingError, ")"
    elif thingError == "ThingErrorThingDescriptorNotFound":
        print "\nERROR: the thingDescriptor could not be found. (", thingError, ")"
    elif thingError == "ThingErrorMissingParameter":
        print "\nERROR: some parameters are missing. (", thingError, ")"
    elif thingError == "ThingErrorInvalidParameter":
        print "\nERROR: invalid parameter. (", thingError, ")"
    elif thingError == "ThingErrorSetupFailed":
        print "\nERROR: setup failed. (", thingError, ")"
    elif thingError == "ThingErrorDuplicateUuid":
        print "\nERROR: uuid allready exists. (", thingError, ")"
    elif thingError == "ThingErrorCreationMethodNotSupported":
        print "\nERROR: the selected CreationMethod is not supported for this thing. (", thingError, ")"
    elif thingError == "ThingErrorSetupMethodNotSupported":
        print "\nERROR: the selected SetupMethod is not supported for this thing. (", thingError, ")"
    elif thingError == "ThingErrorHardwareNotAvailable":
        print "\nERROR: the hardware is not available. (", thingError, ")"
    elif thingError == "ThingErrorHardwareFailure":
        print "\nERROR: hardware failure. Something went wrong with the hardware. (", thingError, ")"
    elif thingError == "ThingErrorAsync":
        print "\nINFO: the response will need some time. (", thingError, ")"
    elif thingError == "ThingErrorThingInUse":
        print "\nERROR: the thing is currently in use. Try again later. (", thingError, ")"
    elif thingError == "ThingErrorPairingTransactionIdNotFound":
        print "\nERROR: the pairingTransactionId could not be found. (", thingError, ")"
    elif thingError == "ThingErrorParameterNotWritable":
        print "\nERROR: one of the parameters is not writable. (", thingError, ")"
    elif thingError == "ThingErrorAuthentificationFailure":
        print "\nERROR: could not authenticate. (", thingError, ")"
    elif thingError == "ThingErrorThingIsChild":
        print "\nERROR: this thing is a child thing (", thingError, "). Please remove the parent thing."
    elif thingError == "ThingErrorThingInRule":
        print "\nERROR: this thing gets used in a rule (", thingError, "). Please specify the remove policy."
    else:
        print "\nERROR: Unknown error code: ", thingError,  "Please take a look at the newest API version."


def print_rule_error_code(ruleError):
    if ruleError == "RuleErrorNoError":
        print "\nSuccess! (", ruleError, ")"
    elif ruleError == "RuleErrorInvalidRuleId":
        print "\nERROR: the ruleId is not valid. (", ruleError, ")"
    elif ruleError == "RuleErrorRuleNotFound":
        print "\nERROR: the rule could not be found. (", ruleError, ")"
    elif ruleError == "RuleErrorThingNotFound":
        print "\nERROR: the thing could not be found for this rule. (", ruleError, ")"
    elif ruleError == "RuleErrorEventTypeNotFound":
        print "\nERROR: the eventType could not be found for this rule. (", ruleError, ")"
    elif ruleError == "RuleErrorStateTypeNotFound":
        print "\nERROR: the stateType could not be found for this rule. (", ruleError, ")"
    elif ruleError == "RuleErrorActionTypeNotFound":
        print "\nERROR: the actionType could not be found for this rule. (", ruleError, ")"
    elif ruleError == "RuleErrorInvalidParameter":
        print "\nERROR: invalid parameter in this rule. (", ruleError, ")"
    elif ruleError == "RuleErrorMissingParameter":
        print "\nERROR: missing parameter in this rule. (", ruleError, ")"
    elif ruleError == "RuleErrorInvalidRuleActionParameter":
        print "\nERROR: one of the actions parameters in this rule is not valid. (", ruleError, ")"
    elif ruleError == "RuleErrorInvalidRuleFormat":
        print "\nERROR: this rule has not the correct format. (", ruleError, ")"
    elif ruleError == "RuleErrorInvalidStateEvaluatorValue":
        print "\nERROR: this rule has an invalid state evaluator value. (", ruleError, ")"
    elif ruleError == "RuleErrorTypesNotMatching":
        print "\nERROR: the event and the action params have not the same type. (", ruleError, ")"
    elif ruleError == "RuleErrorNotExecutable":
        print "\nERROR: the rule is not executable. (", ruleError, ")"
    elif ruleError == "RuleErrorContainsEventBasesAction":
        print "\nERROR: the rule contains an event value based action. (", ruleError, ")"
    elif ruleError == "RuleErrorNoExitActions":
        print "\nERROR: the rule has no exit actions which can be executed. (", ruleError, ")"
    else:
        print "\nERROR: Unknown error code: ", ruleError,  "Please take a look at the newest API version."

def print_cloud_error_code(cloudError):
    if cloudError == "CloudErrorNoError":
        print "\nSuccess! (", cloudError, ")"
    elif cloudError == "CloudErrorAuthenticationFailed":
        print "\nERROR: the given username or password is not valid. (", cloudError, ")"
    elif cloudError == "CloudErrorCloudConnectionDisabled":
        print "\nERROR: the cloud connection is disabled. (", cloudError, ")"
    elif cloudError == "CloudErrorCloudServerNotReachable":
        print "\nERROR: the cloud server is not reachable. (", cloudError, ")"
    elif cloudError == "CloudErrorIdentityServerNotReachable":
        print "\nERROR: the cloud identity server is not reachable. (", cloudError, ")"
    elif cloudError == "CloudErrorProxyServerNotReachable":
        print "\nERROR: the cloud proxy server is not reachable. (", cloudError, ")"
    elif cloudError == "CloudErrorLoginCredentialsMissing":
        print "\nERROR: the cloud login credentials are missing (token or username and password neede). (", cloudError, ")"
    else:
        print "\nERROR: Unknown error code: ", cloudError,  "Please take a look at the newest API version."

def print_networkmanager_error_code(networkManagerError):
    if networkManagerError == "NetworkManagerErrorNoError":
        print "\nSuccess! (", networkManagerError, ")"
    elif networkManagerError == "NetworkManagerErrorUnknownError":
        print "\nERROR: an unknown error occured (", networkManagerError, ")"
    elif networkManagerError == "NetworkManagerErrorWirelessNotAvailable":
        print "\nERROR: there is no wireless thing available. (", networkManagerError, ")"
    elif networkManagerError == "NetworkManagerErrorAccessPointNotFound":
        print "\nERROR: the wifi access point could not be found. (", networkManagerError, ")"
    elif networkManagerError == "NetworkManagerErrorWirelessNetworkingDisabled":
        print "\nERROR: the wireless networking is disabled. (", networkManagerError, ")"
    elif networkManagerError == "NetworkManagerErrorWirelessConnectionFailed":
        print "\nERROR: could not connect to wireless network. (", networkManagerError, ")"
    elif networkManagerError == "NetworkManagerErrorNetworkingDisabled":
        print "\nERROR: the networking is disabled. (", networkManagerError, ")"
    elif networkManagerError == "NetworkManagerErrorNetworkManagerNotAvailable":
        print "\nERROR: the network manager (dbus) is not available. (", networkManagerError, ")"
    else:
        print "\nERROR: Unknown error code: ", networkManagerError,  "Please take a look at the newest API version."


def printSupportedThingsStructure():
    vendors = things.get_supported_vendors()
    for vendor in vendors['params']['vendors']:
        print "- %s" % vendor['displayName']
        thingClasses = things.get_thingClasses(vendor['id'])
        for thingClass in thingClasses:
            print "    -> %s" % thingClass['displayName']
            if len(thingClass['actionTypes']) != 0:
                print "       Actions:"
                for actionType in thingClass['actionTypes']:
                    print "         - %s" % actionType['displayName']

            if len(thingClass['stateTypes']) != 0:
                print "       States:"
                for stateType in thingClass['stateTypes']:
                    print "         - %s" % (stateType['displayName'])

            if len(thingClass['eventTypes']) != 0:
                print "       Events:"
                for eventType in thingClass['eventTypes']:
                    print "         - %s" % eventType['displayName']


def print_server_version():
    response = send_command("JSONRPC.Version")
    print "nymea version: %5s" % (response['params']['version'])
    print "API version: %5s" % (response['params']['protocol version'])


def print_api():
    print_json_format(send_command("JSONRPC.Introspect"))


def print_json_format(string):
    print json.dumps(string, sort_keys=True, indent=4, separators=(',', ': '))
    #print "\n"


def print_api_method():
    methods = send_command("JSONRPC.Introspect")['params']['methods']
    #print_json_format(methods)
    methodList = []
    for item in methods:
        methodList.append(item)
    selection = get_selection("Please select a method:", methodList)
    if selection == None:
        return None
    method = {}
    method[methodList[selection]] = methods[methodList[selection]]
    print print_json_format(method)


def print_api_notifications():
    notifications = send_command("JSONRPC.Introspect")['params']['notifications']
    notificationList = []
    for item in notifications:
        notificationList.append(item)
    selection = get_selection("Please select a notification:", notificationList)
    if selection == None:
        return None
    notification = {}
    notification[notificationList[selection]] = notifications[notificationList[selection]]
    print print_json_format(notification)


def print_api_object():
    types = send_command("JSONRPC.Introspect")['params']['types']
    typesList = []
    for item in types:
        typesList.append(item)
    selection = get_selection("Please select an object:", typesList)
    if selection == None:
        return None
    type = {}
    type[typesList[selection]] = types[typesList[selection]]
    print print_json_format(type)

def print_api_enum():
    enums = send_command("JSONRPC.Introspect")['params']['enums']
    enumsList = []
    for item in enums:
        enumsList.append(item)
    selection = get_selection("Please select an enum:", enumsList)
    if selection == None:
        return None
    enum = {}
    enum[enumsList[selection]] = enums[enumsList[selection]]
    print print_json_format(enum)


def print_api_flag():
    flags = send_command("JSONRPC.Introspect")['params']['flags']
    flagsList = []
    for item in flags:
        flagsList.append(item)
    selection = get_selection("Please select a flag:", flagsList)
    if selection == None:
        return None
    flag = {}
    flag[flagsList[selection]] = flags[flagsList[selection]]
    print print_json_format(flag)




def select_valueOperator(value):
    valueOperators = ["ValueOperatorEquals", "ValueOperatorNotEquals", "ValueOperatorLess", "ValueOperatorGreater", "ValueOperatorLessOrEqual", "ValueOperatorGreaterOrEqual"]
    valueOperatorsSymbols = []
    for i in valueOperators:
        valueOperatorsSymbols.append(get_valueOperator_string(i))
    selection = get_selection("Please select an operator to compare this value: \n %s" % value, valueOperatorsSymbols)
    if selection != None:
        return valueOperators[selection]
    return None


def select_stateOperator():
    stateOperators = ["StateOperatorAnd", "StateOperatorOr"]
    stateOperatorsSymbols = []
    for i in stateOperators:
        stateOperatorsSymbols.append(get_stateEvaluator_string(i))
    selection = get_selection("Please select a state operator to compare this states: ", stateOperatorsSymbols)
    if selection != None:
        return stateOperators[selection]
    return None
