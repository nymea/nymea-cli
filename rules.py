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
import devices
import events
import actions
import states

def add_rule():
    ruleTypes = ["Create a reaction (Event based rule)", "Create a behaviour (State based rule)"]
    boolTypes = ["True","False"]
    ruleType = guh.get_selection("Please select a rule type:", ruleTypes)
    if ruleType == 0:	#Create a reaction (Event based rule)
	params = {}
	eventDescriptors = events.create_eventDescriptors()
	print guh.print_json_format(eventDescriptors)
	if len(eventDescriptors) > 1:
	    params['eventDescriptorList'] = eventDescriptors
	else:
	    params['eventDescriptor'] = eventDescriptors[0]
	
	input = raw_input("-> Do you want to add state conditions to your reaction? (y/N): ")
	if input == "y":
	    	stateEvaluator = states.create_stateEvaluator()
	    	params['stateEvaluator'] = stateEvaluator
	params['actions'] = actions.create_actions()
	params['enabled'] = boolTypes[guh.get_selection("-> Should the rule initially be enabled?", boolTypes)]
	print "adding rule with params:\n", guh.print_json_format(params)
	response = guh.send_command("Rules.AddRule", params)
	guh.print_rule_error_code(response['params']['ruleError'])
    if ruleType == 1:	#Create a behaviour (State based rule)
	stateEvaluator = states.create_stateEvaluator()
	params = {}
	params['stateEvaluator'] = stateEvaluator
	params['actions'] = actions.create_actions()
	params['enabled'] = boolTypes[guh.get_selection("-> Should the rule initially be enabled?", boolTypes)]
	print "adding rule with params:\n", guh.print_json_format(params)
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
        print "\n    No rules found. \n"
        return None
    print "-> List of configured Rules:"
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
    print guh.print_json_format(response)
    print "========================================================"
    print "-> Details for rule %s which currently is %s:" % (ruleId, get_rule_status(ruleId))
    if len(response['params']['rule']['eventDescriptors']) > 0:
	events.print_eventDescriptors(response['params']['rule']['eventDescriptors'])
    if response['params']['rule']['stateEvaluator'] > 0:
	states.print_stateEvaluator(response['params']['rule']['stateEvaluator'])
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
