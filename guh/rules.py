# -*- coding: UTF-8 -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
#                                                                         #
#  Copyright (C) 2015 Simon Stürz <simon.stuerz@guh.guru>                 #
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
import ruleactions
import states
import parameters

def add_rule():
    ruleTypes = ["Create a reaction (Event based rule)", "Create a behaviour (State based rule)"]
    boolTypes = ["true","false"]
    ruleType = guh.get_selection("Please select a rule type:", ruleTypes)
    if ruleType == None:
	return None
    if ruleType == 0:	#Create a reaction (Event based rule)
	params = {}
	params['name'] = raw_input("Please enter the name of the rule: ")
	print "\n========================================================"
	raw_input("-> Press \"enter\" to create an event descriptor!  ")
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
	
	params['actions'] = ruleactions.create_rule_actions(eventDescriptors)
	
	selection = guh.get_selection("-> Should the rule initially be enabled?", boolTypes)
	if selection == None:
	    return None
	params['enabled'] = boolTypes[selection]
	print "adding rule with params:\n", guh.print_json_format(params)
	response = guh.send_command("Rules.AddRule", params)
	guh.print_rule_error_code(response['params']['ruleError'])
    if ruleType == 1:	#Create a behaviour (State based rule)
	params = {}
	params['name'] = raw_input("Please enter the name of the rule: ")
	print "\n========================================================"
	raw_input("-> Press \"enter\" to create a state descriptor!  ")
	params['stateEvaluator'] = states.create_stateEvaluator()
	params['actions'] = ruleactions.create_rule_actions()
	print "\n========================================================"
	input = raw_input("Do you want to add exitAction? (Y/n): ")
        if not input == "n":
            params['exitActions'] = ruleactions.create_rule_actions()
	selection = guh.get_selection("-> Should the rule initially be enabled?", boolTypes)
	if selection == None:
	    return None
	params['enabled'] = boolTypes[selection]
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
    return None


def remove_rule():
    ruleId = select_rule()
    if ruleId == None:
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

def get_rule_name(ruleId):
    response = get_rule_detail(ruleId)
    return response['name']


def get_rule_detail(ruleId):
    params = {}
    params['ruleId'] = ruleId
    response = guh.send_command("Rules.GetRuleDetails", params)
    if 'rule' in response['params']:
        return response['params']['rule']
    return None


def enable_disable_rule():
    ruleId = select_rule()
    if ruleId == None:
        print "\n    No rules found"
        return
    actionTypes = ["enable", "disable"]
    selection = guh.get_selection("What do you want to do with this rule: ", actionTypes)     
    if selection == None:
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
	print "%20s : %s : %s" %(get_rule_name(ruleId), ruleId, get_rule_status(ruleId))


def list_rule_details():
    ruleId = select_rule()
    if ruleId == None:
        print "\n    No rules found"
        return None
    params = {}
    params['ruleId'] = ruleId
    response = guh.send_command("Rules.GetRuleDetails", params)
    print guh.print_json_format(response)
    print "========================================================"
    print "-> Details for rule \"%s\" (%s) which currently is %s:" % (response['params']['rule']['name'], ruleId, get_rule_status(ruleId))
    if len(response['params']['rule']['eventDescriptors']) > 0:
	print "\nEvents:\n"
	events.print_eventDescriptors(response['params']['rule']['eventDescriptors'])
    if response['params']['rule']['stateEvaluator']:
	print "\nStates:\n",
	states.print_stateEvaluator(response['params']['rule']['stateEvaluator'])
    print "\nActions:\n"
    ruleactions.print_ruleActionList(response['params']['rule']['actions'])
    if len(response['params']['rule']['exitActions']) > 0:
	print "\nExit actions:\n"
	ruleactions.print_ruleActionList(response['params']['rule']['exitActions'])


def list_rules_containig_deviceId():
    deviceId = devices.select_configured_device()
    if not deviceId:
	return None
    device = devices.get_device(deviceId)
    params = {}
    params['deviceId'] = deviceId
    response = guh.send_command("Rules.FindRules", params)
    if not response['params']['ruleIds']:
        print "\nThere is no rule containig this device."
        return None
    print "\nFollowing rules contain this device %s" %(deviceId)
    for i in range(len(response['params']['ruleIds'])):
	ruleId = response['params']['ruleIds'][i]
	print "%20s : %s : %s" %(get_rule_name(ruleId), ruleId, get_rule_status(ruleId))
