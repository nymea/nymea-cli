import guh
import devices
import parameters

def get_eventType(eventTypeId):
    params = {}
    params['eventTypeId'] = eventTypeId
    response = guh.send_command("Events.GetEventType", params)
    return response['params']['eventType']


def get_eventTypes(deviceClassId):
    params = {}
    params['deviceClassId'] = deviceClassId
    eventTypes = guh.send_command("Devices.GetEventTypes", params)['params']['eventTypes']
    if eventTypes:
	return eventTypes
    return None


def select_eventType(deviceClassId):
    eventTypes = get_eventTypes(deviceClassId)
    if not eventTypes:
        return None
    eventTypeList = []
    for i in range(len(eventTypes)):
        eventTypeList.append(eventTypes[i]['name'])
    selection = guh.get_selection("Please select an event type:", eventTypeList)
    return eventTypes[selection]


def create_eventDescriptors():
    enough = False
    eventDescriptors = []
    while not enough:
        eventDescriptor = create_eventDescriptor()
        eventDescriptors.append(eventDescriptor)
        input = raw_input("Do you want to add another EventDescriptor? (y/N): ")
        if not input == "y":
            enough = True
    return eventDescriptors


def create_eventDescriptor():
    print " -> Creating EventDescriptor:\n"
    deviceId = devices.select_configured_device()
    device = devices.get_device(deviceId)
    eventType = select_eventType(device['deviceClassId']);
    params = parameters.read_paramDescriptors(eventType['paramTypes'])
    eventDescriptor = {}
    eventDescriptor['deviceId'] = deviceId
    eventDescriptor['eventTypeId'] = eventType['id']
    if len(params) > 0:
        eventDescriptor['paramDescriptors'] = params
    return eventDescriptor



def print_eventDescriptors(eventDescriptors):
    print "\nEvents:"
    for i in range(len(eventDescriptors)):
	eventDescriptor = eventDescriptors[i]
	device = devices.get_device(eventDescriptor['deviceId'])
	eventType = get_eventType(eventDescriptor['eventTypeId'])
	paramDescriptors = eventDescriptor['paramDescriptors']
	print  "%5s. -> %40s -> eventTypeId: %10s: " %(i, device['name'], eventType['name'])
	for i in range(len(paramDescriptors)):
	    print "%58s %s %s" %(paramDescriptors[i]['name'], guh.get_valueOperator_string(paramDescriptors[i]['operator']), paramDescriptors[i]['value'])
