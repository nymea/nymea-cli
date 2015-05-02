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
import parameters
import actions
import events

def create_rule_actions(eventDescriptors = []):
    enough = False
    ruleActions = []
    while not enough:
	print "\n========================================================"
	raw_input("-> Press \"enter\" to create rule actions!  ")
        deviceId = devices.select_configured_device()
        if not deviceId:
	    return None
        device = devices.get_device(deviceId)
        if not device:
	    return None
        actionType = actions.select_actionType(device['deviceClassId'])
        if not actionType:
	    continue
	
	params = read_ruleActionParams(actionType['paramTypes'], eventDescriptors)
	
        action = {}
        action['deviceId'] = deviceId
        action['actionTypeId'] = actionType['id']
        if len(params) > 0:
            action['ruleActionParams'] = params
        ruleActions.append(action)
        input = raw_input("Do you want to add another action? (y/N): ")
        if not input == "y":
            enough = True            
    return ruleActions


def read_ruleActionParams(paramTypes, eventDescriptors = []):
    params = []
    for paramType in paramTypes:
	print guh.print_json_format(paramType)
        if any("allowedValues" in item for item in paramType):
	    selection = guh.get_selection("Please select one of following allowed values:", paramType['allowedValues'])
	    if selection == None:
		return None
	    paramValue = paramType['allowedValues'][selection]
	    param = {}
	    param['name'] = paramType['name']
	    param['value'] = paramValue
	    params.append(param)
	else:
	    # check if we want normal action params or if we want to use an eventtypeid
	    if eventDescriptors:
		selectionTypes = ["yes","no"]
		selectionText = "-> Should the ruleActionParam \"%s\" depend on an event?" %(paramType['name'])
		selection = guh.get_selection(selectionText, selectionTypes)
		if selection == None:
		    return None
		if selectionTypes[selection] == "yes":
		    eventTypeIds = []
		    for i in eventDescriptors:
			eventTypeIds.append(i['eventTypeId'])
		    selection = guh.get_selection("Please select an eventTypeId", eventTypeIds)
		    eventTypeId = eventTypeIds[selection]
		    eventType = events.get_eventType(eventTypeId)
		    eventParamNames = []
		    for i in eventType['paramTypes']:
			eventParamNames.append(i['name'])
		    paramNameSelection = guh.get_selection("Please select the name of the eventParam for the action", eventParamNames)
		    param = {}
		    param['name'] = paramType['name']
		    param['eventTypeId'] = eventTypeId
		    param['eventParamName'] = eventParamNames[paramNameSelection]
		    params.append(param)
		else:
		    # make bool selectable to make shore they are "true" or "false"
		    if paramType['type'] == "bool":
			boolTypes = ["true","false"]
			selectionString = "Please enter value for parameter %s (type: %s): " % (paramType['name'], paramType['type'])
			selection = guh.get_selection(selectionString, boolTypes)
			if selection == None:
			    return None
			paramValue = boolTypes[selection] 
		    else:
			paramValue = raw_input("Please enter value for parameter %s (type: %s): " % (paramType['name'], paramType['type']))
		    param = {}
		    param['name'] = paramType['name']
		    param['value'] = paramValue
		    params.append(param)
	    else:
		# make bool selectable to make shore they are "true" or "false"
		if paramType['type'] == "bool":
		    boolTypes = ["true","false"]
		    selectionString = "Please enter value for parameter %s (type: %s): " % (paramType['name'], paramType['type'])
		    selection = guh.get_selection(selectionString, boolTypes)
		    if selection == None:
			return None
		    paramValue = boolTypes[selection] 
		else:
		    paramValue = raw_input("Please enter value for parameter %s (type: %s): " % (paramType['name'], paramType['type']))
		param = {}
		param['name'] = paramType['name']
		param['value'] = paramValue
		params.append(param)
    return params



def print_ruleActionList(actionList):
    for i in range(len(actionList)):
        action = actionList[i]
        device = devices.get_device(action['deviceId'])
        actionType = actions.get_actionType(actionList[i]['actionTypeId'])
        actionParams = actionList[i]['ruleActionParams']
        print  "%5s. -> %40s -> action: \"%s\"" %(i, device['name'], actionType['name'])
        for i in range(len(actionParams)):
	    if 'eventTypeId' in actionParams[i].keys():
		eventTypeId = actionParams[i]['eventTypeId']
		print "%50s: -> value from event:  \"%s\" %s -> param \"%s\"" %(actionParams[i]['name'], events.get_eventType(eventTypeId)['name'], eventTypeId,actionParams[i]['eventParamName'])
	    else:
		print "%50s: %s" %(actionParams[i]['name'], actionParams[i]['value'])
    