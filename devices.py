import guh

def add_configured_device(deviceClassId):
    deviceClass = get_deviceClass(deviceClassId)
    params = {}
    params['deviceClassId'] = deviceClassId
    deviceParams = guh.read_params(deviceClass['paramTypes'])
    if len(deviceParams) > 0:
        params['deviceParams'] = deviceParams
    print "add device command params:", params
    response = guh.send_command("Devices.AddConfiguredDevice", params)
    guh.print_device_error_code(response['params']['deviceError'])


def add_device():
    deviceClassId = select_deviceClass()
    if deviceClassId == "":
        print "    Empty deviceClass. Can't continue"
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
        print "Can't create this device manually. It'll be created automatically when hardware is discovered."
    

def add_discovered_device(deviceClassId, deviceDescriptorId):
    params = {}
    params['deviceClassId'] = deviceClassId
    params['deviceDescriptorId'] = deviceDescriptorId

    deviceClass = get_deviceClass(deviceClassId)
    if deviceClass['setupMethod'] == "SetupMethodJustAdd":
        response = guh.send_command("Devices.AddConfiguredDevice", params)
        if not response['status'] != "success":
            print "Adding device failed: %s" % response['params']['deviceError']
        else:
            print "Device added successfully. Device ID: %s" % response['params']['deviceId']
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
	if response['status'] == "success":
            print "Device paired successfully"
        else:
            print "Error pairing device: %s" % response['params']['deviceError']


def get_supported_vendors():
    return guh.send_command("Devices.GetSupportedVendors")


def get_deviceClass(deviceClassId):
    deviceClasses = get_deviceClasses()
    for deviceClass in deviceClasses:
        if deviceClass['id'] == deviceClassId:
            return deviceClass
    return None


def get_deviceClasses(vendorId = None):
    params = {};
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


def remove_configured_device():
    deviceId = select_configured_device()
    print "should remove device", deviceId
    params = {}
    params['deviceId'] = deviceId
    response = guh.send_command("Devices.RemoveConfiguredDevice", params)
    guh.print_device_error_code(response['params']['deviceError'])


def discover_device(deviceClassId = None):
    if deviceClassId == None:
        deviceClassId = select_deviceClass()
    deviceClass = get_deviceClass(deviceClassId)
    params = {}
    params['deviceClassId'] = deviceClassId
    discoveryParams = guh.read_params(deviceClass['discoveryParamTypes'])
    if len(discoveryParams) > 0:
        params['discoveryParams'] = discoveryParams
    print "\ndiscovering..."
    response = guh.send_command("Devices.GetDiscoveredDevices", params)
    deviceDescriptorList = [];
    deviceDescriptorIdList = [];
    for deviceDescriptor in response['params']['deviceDescriptors']:
        deviceDescriptorList.append("%s (%s)" % (deviceDescriptor['title'], deviceDescriptor['description']))
        deviceDescriptorIdList.append(deviceDescriptor['id'])
        
    if not deviceDescriptorIdList:
        print "\n    Timeout: no device found"
        return None
    selection = guh.get_selection("Please select a device descriptor", deviceDescriptorList)
    if selection != None:
        return deviceDescriptorIdList[selection]


def select_configured_device():
    devices = get_configured_devices()
    deviceList = []
    deviceIdList = []
    for i in range(len(devices)):
        deviceList.append(devices[i]['name'])
        deviceIdList.append(devices[i]['id'])
        
    selection = guh.get_selection("Please select a device", deviceList)
    if selection != None:
        return deviceIdList[selection]


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
        
    selection = guh.get_selection("Please select vendor", vendorList)
    if selection != None:
	return vendorIdList[selection]
    

def select_configured_device():
    devices = get_configured_devices()
    deviceList = []
    deviceIdList = []
    for device in devices:
        deviceList.append(device['name'])
        deviceIdList.append(device['id'])
        
    selection = guh.get_selection("Please select a device: ", deviceList)
    if selection != None:    
	return deviceIdList[selection]
    return None


def select_deviceClass():
    vendorId = select_vendor()
    deviceClasses = get_deviceClasses(vendorId)
    if len(deviceClasses) == 0:
        print "    No supported devices for this vendor"
        return None
    deviceClassList = []
    deviceClassIdList = []
    for i in range(0,len(deviceClasses)):
        deviceClassList.append(deviceClasses[i]['name'])
        deviceClassIdList.append(deviceClasses[i]['id'])
    selection = guh.get_selection("Please select device class", deviceClassList)
    if selection != None:
	return deviceClassIdList[selection]

