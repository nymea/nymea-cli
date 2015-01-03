import guh
import devices

def get_stateType(stateTypeId):
    params = {}
    params['stateTypeId'] = stateTypeId
    response = guh.send_command("States.GetStateType", params);
    if "stateType" in response['params']:
        return response['params']['stateType']
    return None
    

def get_stateTypes(deviceClassId):
    params = {}
    params['deviceClassId'] = deviceClassId
    response = guh.send_command("Devices.GetStateTypes", params);
    if "stateTypes" in response['params']:
        return response['params']['stateTypes']
    else:
	return None


def select_stateType(deviceClassId):
    stateTypes = get_stateTypes(deviceClassId)
    if not stateTypes:
	print "\n    This device has no states.\n"
        return None
    stateTypeList = []
    for i in range(len(stateTypes)):
        stateTypeList.append(stateTypes[i]['name'])
    selection = guh.get_selection("Please select a state type:", stateTypeList)
    return stateTypes[selection]
    

def create_stateDescriptor():
    print "-> Creating a new stateDescriptor:\n"
    deviceId = devices.select_configured_device()
    device = devices.get_device(deviceId)
    stateType = select_stateType(device['deviceClassId']);
    if stateType == None:
	return None
    valueOperator = guh.select_valueOperator(stateType['name'])
    print "Please enter the value for state \"%s\" (type: %s): " % (stateType['name'], stateType['type'])
    stateValue = raw_input("%s %s " % (stateType['name'], guh.get_valueOperator_string(valueOperator))) 
    stateDescriptor = {}
    stateDescriptor['deviceId'] = deviceId
    stateDescriptor['stateTypeId'] = stateType['id']
    stateDescriptor['value'] = stateValue
    stateDescriptor['operator'] = valueOperator
    print guh.print_json_format(stateDescriptor)
    return stateDescriptor


def create_stateDescriptors():
    enough = False
    stateDescriptors = []
    while not enough:
        stateDescriptor = create_stateDescriptor()
        if stateDescriptor != None:
	    stateDescriptors.append(stateDescriptor)
	    input = raw_input("Do you want to add another stateDescriptor? (y/N): ")
	    if not input == "y":
		enough = True
    return stateDescriptors


def create_stateEvaluator(stateDescriptors = None):
    # check if we already have created a stateDescriptor
    if stateDescriptors == None:
	stateDescriptors = create_stateDescriptors()
	# if we have more than one stateDescriptor created...
	if len(stateDescriptors) > 1:
	    # create child operators
	    childEvaluators = []
	    for i in range(len(stateDescriptors)):
		# recursive call... create a stateEvaluator for each stateDescriptor
		stateDescriptor = []							# has to be a list to enable recursive calls
		stateDescriptor.append(stateDescriptors[i])				# append the single stateDescriptor to list (count =1)
		childEvaluators.append(create_stateEvaluator(stateDescriptor))		# append the new childEvaluator to the real child... recursive call
	    stateEvaluator = {}
	    stateEvaluator['childEvaluators'] = childEvaluators
	    stateEvaluator['operator'] = guh.select_stateOperator()
	    return stateEvaluator
	else:
	    # add single stateDescriptor to the stateEvaluator
	    stateEvaluator = {}
	    stateEvaluator['stateDescriptor'] = stateDescriptors[0]
	    return stateEvaluator
    else:
	# we got already a stateDescriptors list
	if len(stateDescriptors) > 1:
	    # create child operators
	    childEvaluators = []
	    for i in range(len(stateDescriptors)):
		# recursive call... create a stateEvaluator for each stateDescriptor
		stateDescriptor = []							# has to be a list to enable recursive calls
		stateDescriptor.append(stateDescriptors[i])				# append the single stateDescriptor to list (count =1)
		childEvaluators.append(create_stateEvaluator(stateDescriptor))		# append the new childEvaluator to the real child... recursive call
	    stateEvaluator = {}
	    stateEvaluator['childEvaluators'] = childEvaluators
	    stateEvaluator['operator'] = guh.select_stateOperator()
	    return stateEvaluator
	else:
	    # add single stateDescriptor to the stateEvaluator
	    stateEvaluator = {}
	    stateEvaluator['stateDescriptor'] = stateDescriptors[0]
	    return stateEvaluator


def print_stateEvaluator(stateEvaluator):
    print "\nStates:\n     ",
    if 'stateDescriptor' in stateEvaluator:
	stateType = get_stateType(stateEvaluator['stateDescriptor']['stateTypeId'])
	device = devices.get_device(stateEvaluator['stateDescriptor']['deviceId'])
	print stateType['name'],
	print guh.get_valueOperator_string(stateEvaluator['stateDescriptor']['operator']),
	print stateEvaluator['stateDescriptor']['value']
    else:
	for i in range(len(stateEvaluator['childEvaluators'])):
	    device = devices.get_device(stateEvaluator['childEvaluators'][i]['stateDescriptor']['deviceId'])
	    stateType = get_stateType(stateEvaluator['childEvaluators'][i]['stateDescriptor']['stateTypeId'])
	    print "(%s:%s" % (device['name'], stateType['name']),
	    print guh.get_valueOperator_string(stateEvaluator['childEvaluators'][i]['stateDescriptor']['operator']),
	    print stateEvaluator['childEvaluators'][i]['stateDescriptor']['value'], ")", 
	    if i != (len(stateEvaluator['childEvaluators']) - 1):
		print "%s%s" % (guh.get_stateEvaluator_string(stateEvaluator['operator']), guh.get_stateEvaluator_string(stateEvaluator['operator'])),
	print "\n"