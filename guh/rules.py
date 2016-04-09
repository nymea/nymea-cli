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
import selector
import timedescriptor

def add_rule():
    params = {}
    params['name'] = raw_input("Please enter the name of the rule: ")
    
    print "\n========================================================"
    if selector.getYesNoSelection("Do you want to create a TimeDescriptor"):
        params['timeDescriptor'] = timedescriptor.createTimeDescriptor()

    if selector.getYesNoSelection("Do you want to define \"Events\" for this rule?"):
        eventDescriptors = events.create_eventDescriptors()
        print guh.print_json_format(eventDescriptors)
        params['eventDescriptors'] = eventDescriptors
        
        if selector.getYesNoSelection("Do you want to add conditions (\"States\") for the events?"):
            print "\n========================================================"
            raw_input("-> Create a state descriptor!  ")
            stateEvaluator = states.create_stateEvaluator()
            params['stateEvaluator'] = stateEvaluator
        
        params['actions'] = ruleactions.create_rule_actions(eventDescriptors)
    
    else:  

        if selector.getYesNoSelection("Do you want to define \"States\" for this rule?"):
            print "\n========================================================"
            raw_input("-> Press \"enter\" to create a state descriptor!  ")
            params['stateEvaluator'] = states.create_stateEvaluator()

        params['actions'] = ruleactions.create_rule_actions()
        
        if selector.getYesNoSelection("Do you want to add (\"ExitActions\") for this rule?"):
            params['exitActions'] = ruleactions.create_rule_actions()

    params['enabled'] = selector.getBoolSelection("-> Should the rule initially be enabled?")
    params['executable'] = selector.getBoolSelection("-> Should the rule be executable?")
    
    print "\n========================================================\n"    
    print "Adding rule with params:\n", guh.print_json_format(params)
    response = guh.send_command("Rules.AddRule", params)
    guh.print_rule_error_code(response['params']['ruleError'])

    
def edit_rule():
    ruleDescription = select_rule()
    if ruleDescription == None:
        print "\n    No rules found"
        return None
    
    boolTypes = ["yes","no"]
    
    params = {}
    params['ruleId'] = ruleDescription['id']
    originalRule = guh.send_command("Rules.GetRuleDetails", params)['params']['rule']
    print "\n========================================================"
    print "Original rule JSON:\n"
    print guh.print_json_format(originalRule)

    if selector.getYesNoSelection("Do you want to chane the name (current = \"%s\"): " % (originalRule['name'])):
        print "\n========================================================"
        params['name'] = raw_input("Please enter the new name of the rule (current = \"%s\"): " % (originalRule['name']))
    else:
        params['name'] = originalRule['name']
        
        
    if len(originalRule['eventDescriptors']) > 0:
        print "\nCurrent \"Events\":\n"
        events.print_eventDescriptors(originalRule['eventDescriptors'])
        print "\n========================================================\n"

        input = raw_input("Do you want change this \"Events\" (y/N): ")
        if input == "y":
            eventDescriptors = events.create_eventDescriptors()
            print guh.print_json_format(eventDescriptors)
            params['eventDescriptors'] = eventDescriptors
        else:
            params['eventDescriptors'] = originalRule['eventDescriptors']
        
        if originalRule['stateEvaluator']:
            print "\nStates:\n",
            states.print_stateEvaluator(originalRule['stateEvaluator'])
        
        
        if selector.getYesNoSelection("Do you want to add conditions (\"States\") for the events?"):
            print "\n========================================================"
            raw_input("-> Create a state descriptor!  ")
            stateEvaluator = states.create_stateEvaluator()
            params['stateEvaluator'] = stateEvaluator
        
        
        print "\ncurrent \"Actions\":\n"
        ruleactions.print_ruleActionList(originalRule['actions'])
        print "\n========================================================\n"
        input = raw_input("Do you want change this \"Actions\" (y/N): ")
        if input == "y":
            params['actions'] = ruleactions.create_rule_actions()
        else:
            params['actions'] = originalRule['actions']
    else:
        if selector.getYesNoSelection("Do you want to define \"States\" for this rule?"):
            print "\n========================================================"
            raw_input("-> Press \"enter\" to create a state descriptor!  ")
            params['stateEvaluator'] = states.create_stateEvaluator()

        # there will always be at least one action
        print "\n========================================================\n"
        print "Current \"Actions\":\n"
        ruleactions.print_ruleActionList(originalRule['actions'])
        
        print "\n========================================================\n"
        input = raw_input("Do you want change this \"Actions\" (y/N): ")
        if input == "y":
            params['actions'] = ruleactions.create_rule_actions()
        else:
            params['actions'] = originalRule['actions']
        
        if len(originalRule['exitActions']) > 0:
            print "\n========================================================\n"
            print "Current \"Actions\":\n"
            ruleactions.print_ruleActionList(originalRule['exitActions'])
            input = raw_input("Do you want change this \"Events\" (y/N): ")
            if input == "y":
                params['exitActions'] = ruleactions.create_rule_actions()
            else:
                params['exitActions'] = originalRule['exitActions']
                
        else:        
            if selector.getYesNoSelection("Do you want to add (\"ExitActions\") for this rule?"):
                params['exitActions'] = ruleactions.create_rule_actions()
        
    params['enabled'] = selector.getBoolSelection("-> Should the rule initially be enabled?")
    params['executable'] = selector.getBoolSelection("-> Should the rule be executable?")
    
    print "\n========================================================\n"    
    print "Edit rule with params:\n", guh.print_json_format(params)
    response = guh.send_command("Rules.EditRule", params)
    guh.print_rule_error_code(response['params']['ruleError'])
    

def select_rule():
    ruleDescriptions = []
    ruleDescriptions = guh.send_command("Rules.GetRules", {})['params']['ruleDescriptions']
    if not ruleDescriptions:
        print "\n    No rule found"
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

def print_rule_executable_status(status):
    if status == True:
        return "executable"
    else:
        return "not executable"


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
    print "-> Details for rule \"%s\" (%s):\n" % (rule['name'], rule['id'])
    print "Rule is %s" % (print_rule_enabled_status(bool(rule['enabled'])))
    print "Rule is %s" % (print_rule_active_status(bool(rule['active'])))
    print "Rule is %s" % (print_rule_executable_status(bool(rule['executable'])))

    print "\nTimeDescriptor:\n"
    timedescriptor.printTimeDescriptor(rule['timeDescriptor'])
        
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


def execute_rule_actions():
    ruleDescription = select_rule()
    if ruleDescription == None:
            print "\n    No rules found"
            return None
    ruleId = ruleDescription['id']
    params = {}
    params['ruleId'] = ruleId

    rule = guh.send_command("Rules.GetRuleDetails", params)['params']['rule']

    if len(rule['exitActions']) == 0:
        options = ["Execute actions"]
        selection = guh.get_selection("Which actions of the rule should be executed?", options)
        response = guh.send_command("Rules.ExecuteActions", params)
        guh.print_rule_error_code(response['params']['ruleError'])
    else:
        options = ["Execute actions","Execute exit actions"]
        selection = guh.get_selection("Which actions of the rule should be executed?", options)
        if selection == None:
            return None

        if (options[selection] == "Execute actions"):
            response = guh.send_command("Rules.ExecuteActions", params)
            guh.print_rule_error_code(response['params']['ruleError'])
        else:
            response = guh.send_command("Rules.ExecuteExitActions", params)
            guh.print_rule_error_code(response['params']['ruleError'])
    
    return None


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



