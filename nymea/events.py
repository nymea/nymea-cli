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

def get_eventType(eventTypeId):
    params = {}
    params['eventTypeId'] = eventTypeId
    response = nymea.send_command("Events.GetEventType", params)
    return response['params']['eventType']


def get_eventTypes(thingClassId):
    params = {}
    params['thingClassId'] = thingClassId
    eventTypes = nymea.send_command("Integrations.GetEventTypes", params)['params']['eventTypes']
    if eventTypes:
        return eventTypes
    return None


def select_eventType(thingClassId):
    eventTypes = get_eventTypes(thingClassId)
    if not eventTypes:
        return None
    eventTypeList = []
    for i in range(len(eventTypes)):
        eventTypeList.append(eventTypes[i]['displayName'])
    selection = nymea.get_selection("Please select an event type:", eventTypeList)
    if selection != None:
        return eventTypes[selection]
    return None


def create_eventDescriptors():
    enough = False
    eventDescriptors = []
    while not enough:
        eventDescriptor = create_eventDescriptor()
        if not eventDescriptor:
            continue
        eventDescriptors.append(eventDescriptor)
        input = raw_input("Do you want to add another EventDescriptor? (y/N): ")
        if not input == "y":
            enough = True
    return eventDescriptors


def create_eventDescriptor():
    print " -> Creating EventDescriptor:\n"
    thingId = things.select_configured_thing()
    thing = things.get_thing(thingId)
    if not thing:
        return None
    eventType = select_eventType(thing['thingClassId']); 
    if not eventType:
        print "\n    This thing has no events"
        return None
    params = parameters.read_paramDescriptors(eventType['paramTypes'])
    eventDescriptor = {}
    eventDescriptor['thingId'] = thingId
    eventDescriptor['eventTypeId'] = eventType['id']
    if len(params) > 0:
        eventDescriptor['paramDescriptors'] = params
    return eventDescriptor


def print_eventDescriptors(eventDescriptors):
    for i in range(len(eventDescriptors)):
        eventDescriptor = eventDescriptors[i]
        thingName = things.get_full_thing_name(eventDescriptor['thingId'])
        eventType = get_eventType(eventDescriptor['eventTypeId'])
        paramDescriptors = eventDescriptor['paramDescriptors']
        print  "%5s. -> %40s -> event: \"%s\"" %(i, thingName, eventType['displayName'])
        for i in range(len(paramDescriptors)):
            print "%50s %s %s" %(paramDescriptors[i]['name'], nymea.get_valueOperator_string(paramDescriptors[i]['operator']), paramDescriptors[i]['value'])


def print_eventType():
    thingId = things.select_configured_thing()
    thing = things.get_thing(thingId)
    if not thing:
        return None
    eventType = select_eventType(thing['thingClassId']); 
    if not eventType:
        print "\n    This thing has no events"
        return None
    nymea.print_json_format(eventType)



