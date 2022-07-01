# -*- coding: UTF-8 -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
#                                                                         #
#  Copyright (C) 2015 - 2018 Simon Stuerz <simon.stuerz@nymea.io>         #
#                                                                         #
#  This file is part of nymea-cli.                                        #
#                                                                         #
#  nymea-cli is free software: you can redistribute it and/or modify      #
#  it under the terms of the GNU General Public License as published by   #
#  the Free Software Foundation, version 2 of the License.                #
#                                                                         #
#  nymea-cli is distributed in the hope that it will be useful,           #
#  but WITHOUT ANY WARRANTY; without even the implied warranty of         #
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the           #
#  GNU General Public License for more details.                           #
#                                                                         #
#  You should have received a copy of the GNU General Public License      #
#  along with nymea-cli. If not, see <http://www.gnu.org/licenses/>.      #
#                                                                         #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 

import nymea
import things
import parameters
import actions
import events

def create_rule_actions(eventDescriptors = []):
    enough = False
    ruleActions = []
    while not enough:
        print "\n========================================================"
        raw_input("-> Press \"enter\" to create rule actions!  ")
        thingId = things.select_configured_thing()
        if not thingId:
            return None
        thing = things.get_thing(thingId)
        if not thing:
            return None
        actionType = actions.select_actionType(thing['thingClassId'])
        if not actionType:
            continue

        params = read_ruleActionParams(actionType['paramTypes'], eventDescriptors)
    
        action = {}
        action['thingId'] = thingId
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
        print nymea.print_json_format(paramType)
        if any("allowedValues" in item for item in paramType):
            selection = nymea.get_selection("Please select one of following allowed values:", paramType['allowedValues'])
            if selection == None:
                return None
            paramValue = paramType['allowedValues'][selection]
            param = {}
            param['paramTypeId'] = paramType['id']
            param['value'] = paramValue
            params.append(param)
        else:
            # check if we want normal action params or if we want to use an eventtypeid
            if eventDescriptors:
                selectionTypes = ["yes","no"]
                selectionText = "-> Should the ruleActionParam \"%s\" depend on an event?" %(paramType['displayName'])
                selection = nymea.get_selection(selectionText, selectionTypes)
                if selection == None:
                    return None
                if selectionTypes[selection] == "yes":
                    eventTypeIds = []
                    for i in eventDescriptors:
                        eventTypeIds.append(i['eventTypeId'])
                    selection = nymea.get_selection("Please select an eventTypeId", eventTypeIds)
                    eventTypeId = eventTypeIds[selection]
                    eventType = events.get_eventType(eventTypeId)
                    eventParamNames = []
                    eventParamTypeIds = []
                    for i in eventType['paramTypes']:
                        eventParamNames.append(i['displayName'])
                        eventParamTypeIds.append(i['id'])
                    paramSelection = nymea.get_selection("Please select the name of the eventParam for the action", eventParamNames)
                    param = {}
                    param['paramTypeId'] = paramType['id']
                    param['eventTypeId'] = eventTypeId
                    param['eventParamTypeId'] = eventParamTypeIds[paramSelection]
                    params.append(param)
                else:
                    if paramType['type'] == "Bool":
                        boolTypes = ["true","false"]
                        selectionString = "Please enter value for parameter %s (type: %s): " % (paramType['displayName'], paramType['type'])
                        selection = nymea.get_selection(selectionString, boolTypes)
                        if selection == None:
                            return None
                        paramValue = boolTypes[selection]
                    elif paramType['type'] == "Int": 
                        paramValue = int(raw_input("Please enter value for parameter \"%s\" (type: %s): " % (paramType['displayName'], paramType['type'])))
                    elif paramType['type'] == "Double": 
                        paramValue = double(raw_input("Please enter value for parameter \"%s\" (type: %s): " % (paramType['displayName'], paramType['type'])))
                    elif paramType['type'] == "Uint": 
                        paramValue = uint(raw_input("Please enter value for parameter \"%s\" (type: %s): " % (paramType['displayName'], paramType['type'])))
                    else:
                        paramValue = raw_input("Please enter value for parameter \"%s\" (type: %s): " % (paramType['displayName'], paramType['type']))
                    param = {}
                    param['paramTypeId'] = paramType['id']
                    param['value'] = paramValue
                    params.append(param)
            else:
                # make bool selectable to make shore they are "true" or "false"
                if paramType['type'] == "Bool":
                    boolTypes = ["true","false"]
                    selectionString = "Please enter value for parameter %s (type: %s): " % (paramType['displayName'], paramType['type'])
                    selection = nymea.get_selection(selectionString, boolTypes)
                    if selection == None:
                        return None
                    paramValue = boolTypes[selection]
                elif paramType['type'] == "Int": 
                    paramValue = int(raw_input("Please enter value for parameter \"%s\" (type: %s): " % (paramType['displayName'], paramType['type'])))
                elif paramType['type'] == "Double": 
                    paramValue = float(raw_input("Please enter value for parameter \"%s\" (type: %s): " % (paramType['displayName'], paramType['type'])))
                elif paramType['type'] == "Uint": 
                    paramValue = int(raw_input("Please enter value for parameter \"%s\" (type: %s): " % (paramType['displayName'], paramType['type'])))
                else:
                    paramValue = raw_input("Please enter value for parameter \"%s\" (type: %s): " % (paramType['displayName'], paramType['type']))
                
                param = {}
                param['paramTypeId'] = paramType['id']
                param['value'] = paramValue
                params.append(param)
    return params


def print_ruleActionList(actionList):
    for i in range(len(actionList)):
        action = actionList[i]
        thingName = things.get_full_thing_name(action['thingId'])
        actionType = actions.get_actionType(actionList[i]['actionTypeId'])
        actionParams = actionList[i]['ruleActionParams']
        print  "%5s. -> %40s -> action: \"%s\"" %(i, thingName, actionType['displayName'])
        for i in range(len(actionParams)):
            if 'eventTypeId' in actionParams[i].keys():
                eventTypeId = actionParams[i]['eventTypeId']
                eventType = events.get_eventType(eventTypeId)
                print "%50s: -> value from event:  \"%s\" %s -> param \"%s\"" %(parameters.getParamName(actionParams[i]['paramTypeId'], actionType['paramTypes']), eventType['displayName'], eventTypeId, parameters.getParamName(actionParams[i]['eventParamTypeId'], eventType['paramTypes']))
            else:
                print "%50s: %s" %(parameters.getParamName(actionParams[i]['paramTypeId'], actionType['paramTypes']), actionParams[i]['value'])
    


