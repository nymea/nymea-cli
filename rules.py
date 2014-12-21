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
        print "\nNo rule found"
        return None
    
    params = {}
    params['ruleId'] = ruleId
    response = guh.send_command("Rules.RemoveRule", params)
    print "removeRule response", response

def get_rule_status(ruleId):
    params = {}
    params['ruleId'] = ruleId
    response = guh.send_command("Rules.GetRuleDetails", params)
    if response['params']['rule']['enabled'] == True:
	return "enabled"
    else:
	return "disabled"

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
