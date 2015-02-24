# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
#                                                                         #
#  Copyright (C) 2015 guh                                                 #
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
import parameters

def get_supported_vendors():
    return guh.send_command("Devices.GetSupportedVendors")


def get_deviceClass(deviceClassId):
    deviceClasses = get_deviceClasses()
    for deviceClass in deviceClasses:
        if deviceClass['id'] == deviceClassId:
            return deviceClass
    return None


def get_deviceClasses(vendorId = None):
    params = {}
    if vendorId != None:
        params['vendorId'] = vendorId
    return guh.send_command("Devices.GetSupportedDevices", params)['params']['deviceClasses']


def get_device(deviceId):
    devices = get_configured_devices()
    for device in devices:
        if device['id'] == deviceId:
            return device
    return None


def get_configured_devices():
    return guh.send_command("Devices.GetConfiguredDevices")['params']['devices']
    

def get_plugins():
    return guh.send_command("Devices.GetPlugins")['params']['plugins']


def get_plugin(pluginId):
    plugins = get_plugins()
    for plugin in plugins:
	if plugin['id'] == pluginId:
	    return plugin
    return None


def get_plugin_configuration(pluginId):
    params = {}
    params['pluginId'] = pluginId
    return guh.send_command("Devices.GetPluginConfiguration", params)['params']['configuration']
    
    
def add_configured_device(deviceClassId):
    deviceClass = get_deviceClass(deviceClassId)
    params = {}
    params['deviceClassId'] = deviceClassId
    deviceParams = parameters.read_params(deviceClass['paramTypes'])
    if len(deviceParams) > 0:
        params['deviceParams'] = deviceParams
    print "add device command params:", params
    response = guh.send_command("Devices.AddConfiguredDevice", params)
    guh.print_device_error_code(response['params']['deviceError'])


def add_device():
    deviceClassId = select_deviceClass()
    if deviceClassId == None:
        return None
    deviceClass = get_deviceClass(deviceClassId)
    print "createmethods are", deviceClass['createMethods']
    if "CreateMethodUser" in deviceClass['createMethods']:
        add_configured_device(deviceClassId)
    elif "CreateMethodDiscovery" in deviceClass['createMethods']:
        deviceDescriptorId = discover_device(deviceClassId)
        if deviceDescriptorId == None:
            return None
        add_discovered_device(deviceClassId, deviceDescriptorId)
    elif "CreateMethodAuto" in deviceClass['createMethods']:
        print "\nCan't create this device manually. It'll be created automatically when hardware is discovered.\n"
    

def add_discovered_device(deviceClassId, deviceDescriptorId):
    params = {}
    params['deviceClassId'] = deviceClassId
    params['deviceDescriptorId'] = deviceDescriptorId

    deviceClass = get_deviceClass(deviceClassId)
    if deviceClass['setupMethod'] == "SetupMethodJustAdd":
        response = guh.send_command("Devices.AddConfiguredDevice", params)
        if not response['status'] != "success":
            guh.print_device_error_code(response['params']['deviceError'])
        else:
            print "Device added successfully. Device ID: %s\n" % response['params']['deviceId']
    else:
        params = {}
        params['deviceClassId'] = deviceClassId
        params['deviceDescriptorId'] = deviceDescriptorId
        response = guh.send_command("Devices.PairDevice", params)
        print "pairdevice response:", response
        if not response['status'] == "success":
            print "Pairing failed: %s", response['params']['deviceError']
            return
        else:
            print "\nPairing device %s\n\n%s" % (deviceClass['name'], response['params']['displayMessage'])
            if response['params']['setupMethod'] == "SetupMethodPushButton":
                raw_input("Press enter to confirm")
            params = {}
            params['pairingTransactionId'] = response['params']['pairingTransactionId']
            response = guh.send_command("Devices.ConfirmPairing", params)
	    guh.print_device_error_code(response['params']['deviceError'])


def remove_configured_device():
    deviceId = select_configured_device()
    if not deviceId:
	return None
    print "should remove device", deviceId
    params = {}
    params['deviceId'] = deviceId
    response = guh.send_command("Devices.RemoveConfiguredDevice", params)
    guh.print_device_error_code(response['params']['deviceError'])


def discover_device(deviceClassId = None):
    if deviceClassId == None:
        deviceClassId = select_deviceClass()
    deviceClass = get_deviceClass(deviceClassId)
    if not deviceClass:
	return None
    params = {}
    params['deviceClassId'] = deviceClassId
    discoveryParams = parameters.read_params(deviceClass['discoveryParamTypes'])
    if len(discoveryParams) > 0:
        params['discoveryParams'] = discoveryParams
    print "\ndiscovering..."
    response = guh.send_command("Devices.GetDiscoveredDevices", params)
    print response
    deviceDescriptorList = [];
    deviceDescriptorIdList = [];
    for deviceDescriptor in response['params']['deviceDescriptors']:
        deviceDescriptorList.append("%s (%s)" % (deviceDescriptor['title'], deviceDescriptor['description']))
        deviceDescriptorIdList.append(deviceDescriptor['id'])
    if not deviceDescriptorIdList:
        print "\n    Timeout: no device found."
        return None
    selection = guh.get_selection("Please select a device descriptor", deviceDescriptorList)
    if selection != None:
        return deviceDescriptorIdList[selection]
    return None


def select_configured_device():
    devices = get_configured_devices()
    if not devices:
	print "\n    No configured device found.\n"
	return None
    deviceList = []
    deviceIdList = []
    for i in range(len(devices)):
        deviceList.append(devices[i]['name'])
        deviceIdList.append(devices[i]['id'])
    selection = guh.get_selection("Please select a device", deviceList)
    if selection != None:
        return deviceIdList[selection]
    return None


def select_vendor():
    vendors = get_supported_vendors()['params']['vendors']
    if not vendors:
        print "\n    No vendors found. Please install guh-plugins and restart guhd."
        return None
    vendorList = []
    vendorIdList = []
    for i in range(0,len(vendors)):
        vendorList.append(vendors[i]['name'])
        vendorIdList.append(vendors[i]['id'])
    selection = guh.get_selection("Please select a vendor", vendorList)
    if selection != None:
	return vendorIdList[selection]
    return None
    
    
def select_plugin():
    plugins = get_plugins()
    if not plugins:
        print "\n    No plugins found. Please install guh-plugins and restart guhd."
        return None
    pluginList = []
    pluginIdList = []
    for i in range(0,len(plugins)):
        pluginList.append(plugins[i]['name'])
        pluginIdList.append(plugins[i]['id'])
    selection = guh.get_selection("Please select a plugin", pluginList)
    if selection != None:
	return pluginIdList[selection]
    return None

def select_deviceClass():
    vendorId = select_vendor()
    if not vendorId:
	return None
    deviceClasses = get_deviceClasses(vendorId)
    if not deviceClasses:
        print "\n    No supported devices for this vendor\n"
        return None
    deviceClassList = []
    deviceClassIdList = []
    for i in range(0,len(deviceClasses)):
        deviceClassList.append(deviceClasses[i]['name'])
        deviceClassIdList.append(deviceClasses[i]['id'])
    selection = guh.get_selection("Please select device class", deviceClassList)
    if selection != None:
	return deviceClassIdList[selection]
    return None


def list_configured_devices():
    deviceList = get_configured_devices()
    if not deviceList:
	print "\n    No configured device found.\n"
	return None
    print "-> List of configured devices:\n"
    for device in deviceList:
        print "%35s, ID: %s, DeviceClassID: %s" % (device['name'], device['id'], device['deviceClassId'])


def list_deviceClasses(vendorId = None):
    response = get_deviceClasses(vendorId)
    print "-> List of device classes:\n"
    for deviceClass in response:
        print "%35s  %s" % (deviceClass['name'], deviceClass['id'])


def list_device_states():
    deviceId = select_configured_device()
    if deviceId == None:
	return None
    device = get_device(deviceId)
    deviceClass = get_deviceClass(device['deviceClassId'])
    print "-> States of device \"%s\" %s:\n" % (device['name'], device['id'])
    for i in range(len(deviceClass['stateTypes'])):
        params = {}
        params['deviceId'] = deviceId
        params['stateTypeId'] = deviceClass['stateTypes'][i]['id']
        response = guh.send_command("Devices.GetStateValue", params)
        print "%35s: %s" % (deviceClass['stateTypes'][i]['name'], response['params']['value'])


def list_configured_device_params():
    deviceId = select_configured_device()
    device = get_device(deviceId)
    if not device:
	return None
    deviceParams = device['params']
    print "-> Params of device \"%s\" %s\n:" %(device['name'], device['id'])
    for i in range(len(deviceParams)):
        print "%35s: %s" % (deviceParams[i]['name'], deviceParams[i]['value'])


def list_vendors():
    response = get_supported_vendors();
    print "-> List of supported vendors:\n"
    for vendor in response['params']['vendors']:
        print "%35s  %s" % (vendor['name'], vendor['id'])


def list_plugins():
    plugins = get_plugins();
    if not plugins:
        print "\n    No plugins found. Please install guh-plugins and restart guhd."
	return None
    print "-> List of supported plugins:\n"
    for plugin in plugins:
	print "%35s %s" % (plugin['name'], plugin['id'])

def list_plugin_configuration():
    pluginId = select_plugin()
    if not pluginId:
	return None
    plugin = get_plugin(pluginId)
    pluginConfiguration = get_plugin_configuration(pluginId)
    if not pluginConfiguration:
	print "\n    The plugin \"%s\" has no configuration parameters.\n" % (plugin['name'])
	return None
    print "-> The plugin \"%s\" %s has following configurations:\n" % (plugin['name'], plugin['id'])
    for i in range(len(pluginConfiguration)):
	print "%35s: %s" % (pluginConfiguration[i]['name'], pluginConfiguration[i]['value'])
    
    
    
    