# -*- coding: UTF-8 -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
#                                                                         #
#  Copyright (C) 2015-2018 Simon Stuerz <simon.stuerz@guh.io>             #
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
import plugins
import selector

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

def get_full_device_name(deviceId):
    devices = get_configured_devices()
    for device in devices:
        if device['id'] == deviceId:
            return device['name']                    
    return None


def get_configured_devices():
    return guh.send_command("Devices.GetConfiguredDevices")['params']['devices']
    
    
def add_configured_device(deviceClassId):
    deviceClass = get_deviceClass(deviceClassId)
    params = {}
    params['deviceClassId'] = deviceClassId
    params['name'] = raw_input("\nEnter the \"name\" of the device: ")
    deviceParams = parameters.read_params(deviceClass['paramTypes'])
    if deviceParams:
        params['deviceParams'] = deviceParams
    print "\nAdding device with params:", guh.print_json_format(params)
    response = guh.send_command("Devices.AddConfiguredDevice", params)
    guh.print_device_error_code(response['params']['deviceError'])

    
def add_device():
    deviceClassId = select_deviceClass()
    if deviceClassId == None:
        return None
    deviceClass = get_deviceClass(deviceClassId)
    createMethod = select_createMethod("Please select how do you want to add this device", deviceClass['createMethods'])    
    print " --> Using create method \"%s\"" % (createMethod)
    guh.debug_stop
    if createMethod == "CreateMethodUser":
        add_configured_device(deviceClassId)
    elif createMethod ==  "CreateMethodDiscovery":
        deviceDescriptorId = discover_device(deviceClassId)
        if deviceDescriptorId == None:
            return None
        add_discovered_device(deviceClassId, deviceDescriptorId)
    elif createMethod == "CreateMethodAuto":
        print "\nCan't create this device manually. It'll be created automatically when hardware is avaiable.\n"
    

def add_discovered_device(deviceClassId, deviceDescriptorId):
    deviceClass = get_deviceClass(deviceClassId)
    if deviceClass['setupMethod'] == "SetupMethodJustAdd":
        params = {}
        params['deviceClassId'] = deviceClassId
        params['deviceDescriptorId'] = deviceDescriptorId
        params['name'] = raw_input("\nEnter the \"name\" of the device: ")
        response = guh.send_command("Devices.AddConfiguredDevice", params)
        if not response['status'] != "success":
            guh.print_device_error_code(response['params']['deviceError'])
        else:
            print "Device added successfully. Device ID: %s\n" % response['params']['deviceId']
    elif deviceClass['setupMethod'] == "SetupMethodPushButton":
        params = {}
        params['deviceClassId'] = deviceClassId
        params['deviceDescriptorId'] = deviceDescriptorId
        params['name'] = raw_input("\nEnter the \"name\" of the device: ")
        response = guh.send_command("Devices.PairDevice", params)
        #guh.print_json_format(response)
        if not response['status'] == "success":
            print "Pairing failed: %s", response['params']['deviceError']
            return
        print "\nPairing device %s\n\n%s\n\n" % (deviceClass['displayName'], response['params']['displayMessage'])
        if response['params']['setupMethod'] == "SetupMethodPushButton":
            raw_input("\nPress \"enter\" to confirm\n")
        params = {}
        params['pairingTransactionId'] = response['params']['pairingTransactionId']
        response = guh.send_command("Devices.ConfirmPairing", params)
        guh.print_device_error_code(response['params']['deviceError'])
    elif deviceClass['setupMethod'] == "SetupMethodDisplayPin":
        params = {}
        params['deviceClassId'] = deviceClassId
        params['deviceDescriptorId'] = deviceDescriptorId
        params['name'] = raw_input("\nEnter the \"name\" of the device: ")
        response = guh.send_command("Devices.PairDevice", params)
        #guh.print_json_format(response)
        if not response['status'] == "success":
            print "Pairing failed: %s", response['params']['deviceError']
            return
        print "\nPairing device %s\n\n%s\n\n" % (deviceClass['displayName'], response['params']['displayMessage'])
        if response['params']['setupMethod'] == "SetupMethodDisplayPin":
            pin = raw_input("Pin: ")
        params = {}
        params['secret'] = pin
        params['pairingTransactionId'] = response['params']['pairingTransactionId']
        response = guh.send_command("Devices.ConfirmPairing", params)
        guh.print_device_error_code(response['params']['deviceError'])


def remove_configured_device():
    deviceId = select_configured_device()
    if not deviceId:
        return None
    print "Removing device with DeviceId%s" % deviceId
    params = {}
    params['deviceId'] = deviceId
    response = guh.send_command("Devices.RemoveConfiguredDevice", params)

    if response['params']['deviceError'] == 'DeviceErrorDeviceInRule':
            selectionString = "This device is used in a rule. How do you want to remove the device from the rules?"
            removePolicys = ["Remove the device from the rules","Delete all offending rules"]
            selection = guh.get_selection(selectionString, removePolicys)
            
            if removePolicys[selection] == "Remove the device from the rules":
                params['removePolicy'] = "RemovePolicyUpdate"
            else:
                params['removePolicy'] = "RemovePolicyCascade"
            
            response = guh.send_command("Devices.RemoveConfiguredDevice", params)
            

  
    guh.print_device_error_code(response['params']['deviceError'])

    

def edit_device():
    deviceId = select_configured_device()
    params = {}
    params['deviceId'] = deviceId
    params['name'] = raw_input("\nEnter the new \"name\" of the device: ")
    response = guh.send_command("Devices.EditDevice", params)   
    guh.print_device_error_code(response['params']['deviceError'])


def reconfigure_device():
    deviceId = select_configured_device()
    device = get_device(deviceId)
    if not device:
        return None
    deviceClass = get_deviceClass(device['deviceClassId'])
    if not deviceClass:
        return None
    deviceParamTypes = deviceClass['paramTypes']
    currentDeviceParams = device['params']
    createMethod = select_createMethod("Please select how do you want to edit this device", deviceClass['createMethods'])    
    print " --> Using create method \"%s\"\n" % (createMethod)
    params = {}
    if createMethod == "CreateMethodUser":
        newDeviceParams = parameters.edit_params(currentDeviceParams, deviceParamTypes)
        params['deviceId'] = deviceId
        params['deviceParams'] = newDeviceParams
        response = guh.send_command("Devices.ReconfigureDevice", params)
        guh.print_device_error_code(response['params']['deviceError'])
    elif createMethod ==  "CreateMethodDiscovery":
        deviceDescriptorId = discover_device(device['deviceClassId'])
        if not deviceDescriptorId:
            return None
        print "using descriptorId %s" % (deviceDescriptorId)
        params['deviceId'] = deviceId
        params['deviceDescriptorId'] = deviceDescriptorId
        response = guh.send_command("Devices.ReconfigureDevice", params)
        guh.print_device_error_code(response['params']['deviceError'])
    elif createMethod == "CreateMethodAuto":
        newDeviceParams = parameters.edit_params(currentDeviceParams, deviceParamTypes)
        params['deviceId'] = deviceId
        params['deviceParams'] = newDeviceParams
        response = guh.send_command("Devices.ReconfigureDevice", params)
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
    if not 'deviceDescriptors' in response['params']:
        print "no devices found"
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


def select_createMethod(title, createMethods):
    if len(createMethods) == 0:
        print "ERROR: this device has no createMethods. Please check the plugin code!!!"
        return None
    elif len(createMethods) == 1:
        return createMethods[0]
    else:
        selection = guh.get_selection(title, createMethods)
        if selection == None:
            print "ERROR: could not get selection of CreateMethod"
            return None
        return createMethods[selection]
    

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
        vendorList.append(vendors[i]['displayName'])
        vendorIdList.append(vendors[i]['id'])
    selection = guh.get_selection("Please select a vendor", vendorList)
    if selection != None:
        return vendorIdList[selection]
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
        deviceClassList.append(deviceClasses[i]['displayName'])
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
        print "%45s, id: %s, deviceClassId: %s" % (get_full_device_name(device['id']), device['id'], device['deviceClassId'])

def list_deviceClasses(vendorId = None):
    response = get_deviceClasses(vendorId)
    print "-> List of all device classes:\n"
    for deviceClass in response:
        print "%35s  %s" % (deviceClass['displayName'], deviceClass['id'])


def list_device_states():
    deviceId = select_configured_device()
    if deviceId == None:
        return None
    device = get_device(deviceId)
    deviceClass = get_deviceClass(device['deviceClassId'])
    print "-> States of device \"%s\" %s:\n" % (get_full_device_name(deviceId), device['id'])
    for i in range(len(deviceClass['stateTypes'])):
        params = {}
        params['deviceId'] = deviceId
        params['stateTypeId'] = deviceClass['stateTypes'][i]['id']
        response = guh.send_command("Devices.GetStateValue", params)
        print "%35s: %s" % (deviceClass['stateTypes'][i]['displayName'], response['params']['value'])


def list_configured_device_params():
    deviceId = select_configured_device()
    device = get_device(deviceId)
    if not device:
        return None
    paramTypes = get_deviceClass(device['deviceClassId'])['paramTypes']
    deviceParams = device['params']
    print "-> Params of device \"%s\" %s:\n" %(get_full_device_name(deviceId), device['id'])
    for i in range(len(deviceParams)):
        print "%35s: %s" % (parameters.getParamName(deviceParams[i]['paramTypeId'], paramTypes), deviceParams[i]['value'])


def list_vendors():
    response = get_supported_vendors();
    print "-> List of supported vendors:\n"
    for vendor in response['params']['vendors']:
        print "%35s  %s" % (vendor['displayName'], vendor['id'])


def print_deviceClass():
    deviceClassId = select_deviceClass()
    if deviceClassId == None:
        return None
    guh.print_json_format(get_deviceClass(deviceClassId))



