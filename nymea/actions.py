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

def get_actionType(actionTypeId):
    params = {}
    params['actionTypeId'] = actionTypeId
    response = nymea.send_command("Actions.GetActionType", params)
    if not 'actionType' in response['params']:
        return None
    return response['params']['actionType']


def get_actionTypes(thingClassId):
    params = {}
    params['thingClassId'] = thingClassId
    return nymea.send_command("Integrations.GetActionTypes", params)['params']['actionTypes']


def create_actions():
    enough = False
    actions = []
    while not enough:
        print "\n========================================================"
        raw_input("-> Press \"enter\" to create actions!  ")
        thingId = things.select_configured_thing()
        if not thingId:
            return None
        thing = things.get_thing(thingId)
        actionType = select_actionType(thing['thingClassId'])
        if not actionType:
            continue
        params = parameters.read_params(actionType['paramTypes'])
        action = {}
        action['thingId'] = thingId
        action['actionTypeId'] = actionType['id']
        if len(params) > 0:
            action['params'] = params
        actions.append(action)
        input = raw_input("Do you want to add another action? (y/N): ")
        if not input == "y":
            enough = True            
    return actions


def execute_action():
    thingId = things.select_configured_thing()
    if thingId == None:
        return None
    thing = things.get_thing(thingId)
    actionType = select_actionType(thing['thingClassId'])
    #print nymea.print_json_format(actionType)
    if actionType == None:
        print "\n    This thing has no actions"
        return None
    actionTypeId = actionType['id']
    params = {}
    params['actionTypeId'] = actionTypeId
    params['thingId'] = thingId
    actionType = get_actionType(actionTypeId)
    actionParams = parameters.read_params(actionType['paramTypes'])
    params['params'] = actionParams
    response = nymea.send_command("Actions.ExecuteAction", params)
    if response:  
        nymea.print_thing_error_code(response['params']['thingError'])
    
    
def select_actionType(thingClassId):
    actions = get_actionTypes(thingClassId)
    if not actions:
        return None
    actionList = []
    #print "got actions", actions
    for i in range(len(actions)):
        #print "got actiontype", actions[i]
        actionList.append(actions[i]['displayName'])
    selection = nymea.get_selection("Please select an action type:", actionList)
    if selection != None:
        return actions[selection]
    return None


def print_actionList(actionList):
    for i in range(len(actionList)):
        action = actionList[i]
        thing = things.get_thing(action['thingId'])
        actionType = get_actionType(actionList[i]['actionTypeId'])
        actionParams = actionList[i]['params']
        print  "%5s. ->  %40s -> action: \"%s\"" %(i, thing['name'], actionType['displayName'])
        for i in range(len(actionParams)):
            print "%50s: %s" %(actionParams[i]['displayName'], actionParams[i]['value'])


def print_actionType():
    thingId = things.select_configured_thing()
    if thingId == None:
        return None
    thing = things.get_thing(thingId)
    actionType = select_actionType(thing['thingClassId'])
    #print nymea.print_json_format(actionType)
    if actionType == None:
        print "\n    This thing has no actions"
        return None
    actionType = get_actionType(actionType['id'])
    nymea.print_json_format(actionType)



