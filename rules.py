import guh
import devices
import events
import actions

def add_rule():
    params = {}
    params['eventDescriptorList'] = events.create_eventDescriptors()
    if len(params['eventDescriptorList']) > 1:
        params['stateEvaluator'] = guh.select_stateOperator()
    params['actions'] = actions.create_actions()
    print "adding rule with params:", params
    response = guh.send_command("Rules.AddRule", params)
    guh.print_rule_error_code(response['params']['ruleError'])


def select_rule():
    ruleIds = guh.send_command("Rules.GetRules", {})['params']['ruleIds']
    if not ruleIds:
        return None
    selection = guh.get_selection("Please select rule:", ruleIds)
    if selection != None:
	return ruleIds[selection]


def remove_rule():
    ruleId = select_rule()
    if ruleId == "":
        print "\n    No rule found"
        return None
    params = {}
    params['ruleId'] = ruleId
    response = guh.send_command("Rules.RemoveRule", params)
    guh.print_rule_error_code(response['params']['ruleError'])
    

def get_rule_status(ruleId):
    response = get_rule_detail(ruleId)
    if response['enabled'] == True:
	return "enabled"
    else:
	return "disabled"


def get_rule_detail(ruleId):
    params = {}
    params['ruleId'] = ruleId
    response = guh.send_command("Rules.GetRuleDetails", params)
    if 'rule' in response['params']:
        return response['params']['rule']
    return None


def enable_disable_rule():
    ruleId = select_rule()
    if ruleId == "":
        print "\n    No rules found"
        return
    actionTypes = ["enable", "disable"]
    selection = guh.get_selection("What do you want to do with this rule: ", actionTypes)     
    if selection == 0:
	params = {}
	params['ruleId'] = ruleId
	response = guh.send_command("Rules.EnableRule", params)
        guh.print_rule_error_code(response['params']['ruleError'])
    else:
	params = {}
	params['ruleId'] = ruleId
	response = guh.send_command("Rules.DisableRule", params)
        guh.print_rule_error_code(response['params']['ruleError'])


def list_rules():
    response = guh.send_command("Rules.GetRules", {})
    if not response['params']['ruleIds']:
        print "\n    No rules found."
        return None
    print "\nRules found:"
    for i in range(len(response['params']['ruleIds'])):
	ruleId = response['params']['ruleIds'][i]
	params = {}
	params['ruleId'] = ruleId
	ruleDetail = guh.send_command("Rules.GetRuleDetails", params)
	print response['params']['ruleIds'][i], "(", get_rule_status(ruleId), ")"


def list_rule_details():
    ruleId = select_rule()
    if ruleId == "":
        print "\n    No rules found"
        return None
    params = {}
    params['ruleId'] = ruleId
    response = guh.send_command("Rules.GetRuleDetails", params)
    print response
    print "\nDetails for rule", ruleId, "which currently is", get_rule_status(ruleId) 
    print "\nEvents ->", guh.get_stateEvaluator_string(response['params']['rule']['stateEvaluator']['operator']), ":"
    for i in range(len(response['params']['rule']['eventDescriptors'])):
        eventDescriptor = response['params']['rule']['eventDescriptors'][i]
        device = devices.get_device(eventDescriptor['deviceId'])
        eventType = events.get_eventType(eventDescriptor['eventTypeId'])
        paramDescriptors = eventDescriptor['paramDescriptors']
        print  "%5s. -> %40s -> eventTypeId: %10s: " %(i, device['name'], eventType['name'])
        for i in range(len(paramDescriptors)):
            print "%58s %s %s" %(paramDescriptors[i]['name'], guh.get_valueOperator_string(paramDescriptors[i]['operator']), paramDescriptors[i]['value'])
    print "\nActions:"
    for i in range(len(response['params']['rule']['actions'])):
        action = response['params']['rule']['actions'][i]
        device = devices.get_device(action['deviceId'])
        actionType = actions.get_actionType(response['params']['rule']['actions'][i]['actionTypeId'])
        actionParams = response['params']['rule']['actions'][i]['params']
        print  "%5s. ->  %40s -> action: %s" %(i, device['name'], actionType['name'])
        for i in range(len(actionParams)):
            print "%61s: %s" %(actionParams[i]['name'], actionParams[i]['value'])



def list_rules_containig_deviceId():
    deviceId = devices.select_configured_device()
    device = devices.get_device(deviceId)
    params = {}
    params['deviceId'] = deviceId
    response = guh.send_command("Rules.FindRules", params)
    if not response['params']['ruleIds']:
        print "\nThere is no rule containig this device."
        return None
    print "\nFollowing rules contain this device"
    for i in range(len(response['params']['ruleIds'])):
        print "Device ", deviceId, "found in rule", response['params']['ruleIds'][i]
