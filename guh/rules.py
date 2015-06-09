# -*- coding: UTF-8 -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
#                                                                         #
#  Copyright (C) 2015 Simon Stuerz <simon.stuerz@guh.guru>                #
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
    if ruleType == 0:    #Create a reaction (Event based rule)
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
    if ruleType == 1:    #Create a behaviour (State based rule)
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
    ruleDescriptions = []
    ruleDescriptions = guh.send_command("Rules.GetRules", {})['params']['ruleDescriptions']
    if not ruleDescriptions:
        return None
    descriptions = []
    for ruleDescription in ruleDescriptions:
        descriptions.append("%s (%s) -> %s / %s" % (ruleDescription['name'], ruleDescription['id'], print_rule_enabled_status(ruleDescription['enabled']), print_rule_active_status(ruleDescription['active'])))
    selection = guh.get_selection("Please select rule:", descriptions)
    if selection != None:
        return ruleDescriptions[selection]
    return None


def remove_rule():
    ruleDescription = select_rule()
    if ruleDescription == None:
        print "\n    No rule found"
        return None
    params = {}
    params['ruleId'] = ruleDescription['id']
    response = guh.send_command("Rules.RemoveRule", params)
    guh.print_rule_error_code(response['params']['ruleError'])
    

def print_rule_enabled_status(status):
    if status == True:
        return "enabled"
    else:
        return "disabled"


def print_rule_active_status(status):
    if status == True:
        return "active"
    else:
        return "inactive"


def get_rule_description(ruleId):
    ruleDescriptions = []
    ruleDescriptions = guh.send_command("Rules.GetRules", {})['params']['ruleDescriptions']
    for ruleDescription in ruleDescriptions:
        if ruleDescription['id'] == ruleId:
            return ruleDescription
    return None


def enable_disable_rule():
    ruleDescription = select_rule()
    if ruleDescription == None:
        print "\n    No rules found"
        return
    actionTypes = ["enable", "disable"]
    selection = guh.get_selection("What do you want to do with this rule: ", actionTypes)     
    if selection == 0:
        params = {}
        params['ruleId'] = ruleDescription['id']
        response = guh.send_command("Rules.EnableRule", params)
        guh.print_rule_error_code(response['params']['ruleError'])
    else:
        params = {}
        params['ruleId'] = ruleDescription['id']
        response = guh.send_command("Rules.DisableRule", params)
        guh.print_rule_error_code(response['params']['ruleError'])


def list_rules():
    response = guh.send_command("Rules.GetRules", {})
    if not response['params']['ruleDescriptions']:
        print "\n    No rules found. \n"
        return None
    print "-> List of configured Rules:\n"
    for i in range(len(response['params']['ruleDescriptions'])):
        ruleDescription = response['params']['ruleDescriptions'][i]
        print "%20s ( %s ) -> %s / %s" % (ruleDescription['name'], ruleDescription['id'], print_rule_enabled_status(bool(ruleDescription['enabled'])), print_rule_active_status(bool(ruleDescription['active'])))


def list_rule_details():
    ruleDescription = select_rule()
    if ruleDescription == None:
        print "\n    No rules found"
        return None
    params = {}
    params['ruleId'] = ruleDescription['id']
    rule = guh.send_command("Rules.GetRuleDetails", params)['params']['rule']
    print guh.print_json_format(rule)
    print "========================================================"
    print "-> Details for rule \"%s\" (%s) which currently is %s / %s :" % (rule['name'], rule['id'], print_rule_enabled_status(bool(rule['enabled'])), print_rule_active_status(bool(rule['active'])))
    if len(rule['eventDescriptors']) > 0:
        print "\nEvents:\n"
        events.print_eventDescriptors(rule['eventDescriptors'])
    if rule['stateEvaluator']:
        print "\nStates:\n",
        states.print_stateEvaluator(rule['stateEvaluator'])
    print "\nActions:\n"
    ruleactions.print_ruleActionList(rule['actions'])
    if len(rule['exitActions']) > 0:
        print "\nExit actions:\n"
        ruleactions.print_ruleActionList(rule['exitActions'])


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
    print "\nFollowing rules contain this device %s\n" %(deviceId)
    for i in range(len(response['params']['ruleIds'])):
        ruleDescription = get_rule_description(response['params']['ruleIds'][i])
        print "%20s ( %s ) -> %s / %s" % (ruleDescription['name'], ruleDescription['id'], print_rule_enabled_status(ruleDescription['enabled']), print_rule_active_status(ruleDescription['active']))
