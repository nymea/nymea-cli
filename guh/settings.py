# -*- coding: UTF-8 -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
#                                                                         #
#  Copyright (C) 2016 Simon Stuerz <simon.stuerz@guh.guru>                #
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

import guh
import selector
import uuid

        
def list_configurations():
    params = {}
    response = guh.send_command("Configuration.GetConfigurations", params)
    guh.print_json_format(response['params'])
        
        
def list_timezones():
    params = {}
    response = guh.send_command("Configuration.GetTimeZones", params)
    guh.print_json_format(response['params'])


def set_timezone():
    params = {}
    timeZones = []
    timeZones = get_timezones()
    selection = guh.get_selection("Please select one of following allowed values:", timeZones)
    if selection == None:
        return None
         
    params['timeZone'] = timeZones[selection]
    response = guh.send_command("Configuration.SetTimeZone", params)
    guh.print_json_format(response['params'])


def set_language():
    params = {}
    languages = get_languages()
    selection = guh.get_selection("Please select one of following allowed values:", languages)
    if selection == None:
        return None

    params['language'] = languages[selection]
    response = guh.send_command("Configuration.SetLanguage", params)
    guh.print_json_format(response['params'])


def set_serverName():
    params = {}         
    params['serverName'] = raw_input("Please enter the server name:")
    response = guh.send_command("Configuration.SetServerName", params)
    guh.print_json_format(response['params'])


def get_languages():
    params = {}
    response = guh.send_command("Configuration.GetAvailableLanguages", params)
    return response['params']['languages']


def get_timezones():
    params = {}
    response = guh.send_command("Configuration.GetTimeZones", params)
    return response['params']['timeZones']


def show_tcpServer_configuration():
    response = guh.send_command("Configuration.GetConfigurations")
    print "TCP server configuration\n"
    guh.print_json_format(response['params']['tcpServerConfigurations'])


def configure_tcpServer():
    configurations = guh.send_command("Configuration.GetConfigurations")
    tcpConfigs = configurations['params']['tcpServerConfigurations']
    configList = []
    for i in range(len(tcpConfigs)):
        configList.append("%s:%s   SSL:%s   Auth:%s" % (tcpConfigs[i]['address'], tcpConfigs[i]['port'], tcpConfigs[i]['sslEnabled'], tcpConfigs[i]['authenticationEnabled']))
    configList.append("New entry")
    selection = guh.get_selection("Please select a configuration", configList)
    if selection == None:
        return None

    params = {}
    configuration = {}
    if selection == len(configList) - 1:
        # new config
        configuration['id'] = str(uuid.uuid4())
        configuration['address'] = raw_input("\nEnter the \"address\" of the TCP server: ")
        configuration['port'] = raw_input("\nEnter the \"port\" of the TCP server: ")
        configuration['sslEnabled'] = raw_input("\nEnter whether SSL should be enabled or not: ")
        configuration['authenticationEnabled'] = raw_input("\nEnter whether authentication should be enabled or not: ")
    else:

        selectedConfig = tcpConfigs[selection]

        editList = []
        editList.append("Modify")
        editList.append("Delete")
        editSelection = guh.get_selection("Do you want to edit or delete the server interface?", editList)
        if editSelection == None:
            return None

        if editSelection == 1: #delete
            params = {}
            params['id'] = selectedConfig['id']
            guh.send_command("Configuration.DeleteTcpServerConfiguration", params)
            return None



        configuration['id'] = selectedConfig['id']
        configuration['address'] = raw_input("\nEnter the \"address\" of the TCP server (current \"%s\"): " % (selectedConfig['address']))
        configuration['port'] = raw_input("\nEnter the \"port\" of the TCP server (current %s): "% (selectedConfig['port']))
        configuration['sslEnabled'] = raw_input("\nEnter whether SSL should be enabled or not (current \"%s\"): " % (selectedConfig['sslEnabled']))
        configuration['authenticationEnabled'] = raw_input("\nEnter whether authentication should be enabled or not (current %s): "% (selectedConfig['authenticationEnabled']))
    params['configuration'] = configuration
    response = guh.send_command("Configuration.SetTcpServerConfiguration", params)
    guh.print_json_format(response['params'])


def configure_webServer():
    configurations = guh.send_command("Configuration.GetConfigurations")
    webConfigs = configurations['params']['webServerConfigurations']
    configList = []
    for i in range(len(webConfigs)):
        configList.append("%s:%s   SSL:%s   Auth:%s   Data: %s" % (webConfigs[i]['address'], webConfigs[i]['port'], webConfigs[i]['sslEnabled'], webConfigs[i]['authenticationEnabled'], webConfigs[i]['publicFolder']))
    configList.append("New entry")
    selection = guh.get_selection("Please select a configuration", configList)
    if selection == None:
        return None

    params = {}
    configuration = {}
    if selection == len(configList) - 1:
        # new config
        configuration['id'] = str(uuid.uuid4())
        configuration['address'] = raw_input("\nEnter the \"address\" of the Web server: ")
        configuration['port'] = raw_input("\nEnter the \"port\" of the Web server: ")
        configuration['sslEnabled'] = raw_input("\nEnter whether SSL should be enabled or not: ")
        configuration['authenticationEnabled'] = raw_input("\nEnter whether authentication should be enabled or not: ")
        configuration['publicFolder'] = raw_input("\nEnter the public folder for the webserver: ")
    else:

        selectedConfig = webConfigs[selection]

        editList = []
        editList.append("Modify")
        editList.append("Delete")
        editSelection = guh.get_selection("Do you want to edit or delete the server interface?", editList)
        if editSelection == None:
            return None

        if editSelection == 1: #delete
            params = {}
            params['id'] = selectedConfig['id']
            guh.send_command("Configuration.DeleteWebServerConfiguration", params)
            return None

        configuration['id'] = selectedConfig['id']
        configuration['address'] = raw_input("\nEnter the \"address\" of the TCP server (current \"%s\"): " % (selectedConfig['address']))
        configuration['port'] = raw_input("\nEnter the \"port\" of the TCP server (current %s): "% (selectedConfig['port']))
        configuration['sslEnabled'] = raw_input("\nEnter whether SSL should be enabled or not (current \"%s\"): " % (selectedConfig['sslEnabled']))
        configuration['authenticationEnabled'] = raw_input("\nEnter whether authentication should be enabled or not (current %s): "% (selectedConfig['authenticationEnabled']))
        configuration['publicFolder'] = raw_input("\nEnter the public folder for the webserver (current: %s): " % selectedConfig['publicFolder'])
    params['configuration'] = configuration
    response = guh.send_command("Configuration.SetWebServerConfiguration", params)
    guh.print_json_format(response['params'])


def show_webServer_configuration():
    response = guh.send_command("Configuration.GetConfigurations")
    print "Web server configuration\n"
    guh.print_json_format(response['params']['webServerConfigurations'])
    

def configure_webSocketServer():
    configurations = guh.send_command("Configuration.GetConfigurations")
    webSocketConfigs = configurations['params']['webSocketServerConfigurations']
    configList = []
    for i in range(len(webSocketConfigs)):
        configList.append("%s:%s   SSL:%s   Auth:%s" % (webSocketConfigs[i]['address'], webSocketConfigs[i]['port'], webSocketConfigs[i]['sslEnabled'], webSocketConfigs[i]['authenticationEnabled']))
    configList.append("New entry")
    selection = guh.get_selection("Please select a configuration", configList)
    if selection == None:
        return None

    params = {}
    configuration = {}
    if selection == len(configList) - 1:
        # new config
        configuration['id'] = str(uuid.uuid4())
        configuration['address'] = raw_input("\nEnter the \"address\" of the WebSocket server: ")
        configuration['port'] = raw_input("\nEnter the \"port\" of the WebSocket server: ")
        configuration['sslEnabled'] = raw_input("\nEnter whether SSL should be enabled or not: ")
        configuration['authenticationEnabled'] = raw_input("\nEnter whether authentication should be enabled or not: ")
    else:

        selectedConfig = webSocketConfigs[selection]

        editList = []
        editList.append("Modify")
        editList.append("Delete")
        editSelection = guh.get_selection("Do you want to edit or delete the server interface?", editList)
        if editSelection == None:
            return None

        if editSelection == 1: #delete
            params = {}
            params['id'] = selectedConfig['id']
            guh.send_command("Configuration.DeleteWebSocketServerConfiguration", params)
            return None



        configuration['id'] = selectedConfig['id']
        configuration['address'] = raw_input("\nEnter the \"address\" of the WebSocket server (current \"%s\"): " % (selectedConfig['address']))
        configuration['port'] = raw_input("\nEnter the \"port\" of the WebSocket server (current %s): "% (selectedConfig['port']))
        configuration['sslEnabled'] = raw_input("\nEnter whether SSL should be enabled or not (current \"%s\"): " % (selectedConfig['sslEnabled']))
        configuration['authenticationEnabled'] = raw_input("\nEnter whether authentication should be enabled or not (current %s): "% (selectedConfig['authenticationEnabled']))
    params['configuration'] = configuration
    response = guh.send_command("Configuration.SetWebSocketServerConfiguration", params)
    guh.print_json_format(response['params'])


def show_webSocketServer_configuration():
    response = guh.send_command("Configuration.GetConfigurations")
    print "Web socket server configuration\n"
    guh.print_json_format(response['params']['webSocketServerConfigurations'])


def cloud_authenticate():
    params = {}
    params['username'] = raw_input("\nEnter the \"username\" of your cloud account: ")
    params['password'] = raw_input("\nEnter the \"password\" of your cloud account: ")
    response = guh.send_command("Cloud.Authenticate", params)
    guh.print_cloud_error_code(response['params']['cloudError'])
    
    
def print_cloud_status():
    params = {}
    response = guh.send_command("Cloud.GetConnectionStatus", params)
    guh.print_json_format(response['params'])

    
def enable_cloud_connection():
    params = {}
    options = ["enable", "disable"]
    selection = guh.get_selection("Do you want to do with the cloud connection: ", options)     
    if selection == 0:
        params['enable'] = True
    else:
        params['enable'] = False
    
    response = guh.send_command("Cloud.Enable", params)
    guh.print_json_format(response['params'])


def show_network_status():
    params = {}
    response = guh.send_command("NetworkManager.GetNetworkStatus", params)
    if 'status' in response['params']:
        guh.print_json_format(response['params']['status'])
    else:
        guh.print_networkmanager_error_code(response['params']['networkManagerError'])

      
def enable_networking():
    params = {}
    options = ["enable", "disable"]
    selection = guh.get_selection("Do you want to do with the networking: ", options)     
    if selection == 0:
        params['enable'] = True
    else:
        params['enable'] = False

    response = guh.send_command("NetworkManager.EnableNetworking", params)
    guh.print_networkmanager_error_code(response['params']['networkManagerError'])


def enable_wirelessnetworking():
    params = {}
    options = ["enable", "disable"]
    selection = guh.get_selection("Do you want to do with the wirless networking: ", options)     
    if selection == 0:
        params['enable'] = True
    else:
        params['enable'] = False

    response = guh.send_command("NetworkManager.EnableWirelessNetworking", params)
    guh.print_networkmanager_error_code(response['params']['networkManagerError'])
    

def selectWirelessInterface():
    params = {}    
    response = guh.send_command("NetworkManager.GetNetworkDevices", params)
    if response['params']['networkManagerError'] != 'NetworkManagerErrorNoError':
        print ("There is no wireless interface available")
        guh.print_networkmanager_error_code(response['params']['networkManagerError'])
        return None
    
    if len(response['params']['wirelessNetworkDevices']) is 1:
        return response['params']['wirelessNetworkDevices'][0]['interface']
    else:
        interfaces = []
        for wirelessNetworkDevice in response['params']['wirelessNetworkDevices']:
            interfaces.append(wirelessNetworkDevice['interface'])
        
        selection = guh.get_selection("Please select a wifi interface:", interfaces)     
        return interfaces[selection]
    
    
def list_wirelessaccesspoints():
    interface = selectWirelessInterface()
    if interface is None:
        return
        
    params = {}
    params['interface'] = interface 
    response = guh.send_command("NetworkManager.GetWirelessAccessPoints", params)
    if response['params']['networkManagerError'] != 'NetworkManagerErrorNoError':
        guh.print_networkmanager_error_code(response['params']['networkManagerError'])
    else:
        print ("Wireless accesspoints for interface %s" % interface)
        print ("---------------------------------------------------------------------")
        for accessPoint in response['params']['wirelessAccessPoints']:
            print("%20s | %5s%s | %6s %6s | %s | %s" % (accessPoint['ssid'], accessPoint['signalStrength'], '%', accessPoint['frequency'], '[GHz]', accessPoint['macAddress'], accessPoint['protected']))
    

def scan_wirelessaccesspoints():
    interface = selectWirelessInterface()
    if interface is None:
        print "There is no wireless interface available"
        return
        
    params = {}
    params['interface'] = interface 
    response = guh.send_command("NetworkManager.ScanWifiNetworks", params)
    guh.print_networkmanager_error_code(response['params']['networkManagerError'])        
        
        
def list_network_devices():
    params = {}
    response = guh.send_command("NetworkManager.GetNetworkDevices", params)
    guh.print_json_format(response['params'])
    

def connect_wifi():
    interface = selectWirelessInterface()
    if interface is None:
        print "There is no wireless interface available"
        return
        
    params = {}
    params['interface'] = interface 
    wifiNetworks = []
    wifiNetworkStrings = []
    response = guh.send_command("NetworkManager.GetWirelessAccessPoints", params)
    wifiNetworks = response['params']['wirelessAccessPoints']
    for accessPoint in wifiNetworks:
        wifiNetworkStrings.append(("%10s | %s%s | %s %s | %s" % (accessPoint['ssid'], accessPoint['signalStrength'], '%', accessPoint['frequency'], '[GHz]', accessPoint['macAddress'])))
    
    selection = selector.get_selection("Wifi access points", wifiNetworkStrings)
    if not selection:
        return None
    
    params['ssid'] = wifiNetworks[selection]['ssid']
    if wifiNetworks[selection]['protected'] == True:
        params['password'] = raw_input("Please enter the password for wifi network %s: " % (params['ssid']))
        
    response = guh.send_command("NetworkManager.ConnectWifiNetwork", params)
    guh.print_networkmanager_error_code(response['params']['networkManagerError'])


def disconnect_networkdevice():
    interface = selectWirelessInterface()
    if interface is None:
        print "There is no wireless interface available"
        return
        
    params = {}
    params['interface'] = interface
    response = guh.send_command("NetworkManager.DisconnectInterface", params)
    guh.print_networkmanager_error_code(response['params']['networkManagerError'])


