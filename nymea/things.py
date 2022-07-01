# -*- coding: UTF-8 -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#                                                                         #
#  Copyright (C) 2015 - 2022 Simon Stuerz <simon.stuerz@nymea.io>         #
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
import parameters
import plugins
import selector

def get_supported_vendors():
    return nymea.send_command("Integrations.GetVendors")


def get_thingClass(thingClassId):
    thingClasses = get_thingClasses()
    for thingClass in thingClasses:
        if thingClass['id'] == thingClassId:
            return thingClass
    return None


def get_thingClasses(vendorId = None):
    params = {}
    if vendorId != None:
        params['vendorId'] = vendorId
    return nymea.send_command("Integrations.GetThingClasses", params)['params']['thingClasses']


def get_thing(thingId):
    things = get_configured_things()
    for thing in things:
        if thing['id'] == thingId:
            return thing
    return None

def get_full_thing_name(thingId):
    things = get_configured_things()
    for thing in things:
        if thing['id'] == thingId:
            return thing['name']
    return None


def get_configured_things():
    return nymea.send_command("Integrations.GetThings")['params']['things']


def add_configured_thing(thingClassId):
    thingClass = get_thingClass(thingClassId)
    params = {}
    params['thingClassId'] = thingClassId
    params['name'] = raw_input("\nEnter the \"name\" of the thing: ")
    thingParams = parameters.read_params(thingClass['paramTypes'])
    if thingParams:
        params['thingParams'] = thingParams
    print "\nAdding thing with params:", nymea.print_json_format(params)
    response = nymea.send_command("Integrations.AddThing", params)
    nymea.print_thing_error_code(response['params']['thingError'])


def add_thing():
    thingClassId = select_thingClass()
    if thingClassId == None:
        return None
    thingClass = get_thingClass(thingClassId)
    createMethod = select_createMethod("Please select how do you want to add this thing", thingClass['createMethods'])
    print " --> Using create method \"%s\"" % (createMethod)
    nymea.debug_stop
    if createMethod == "CreateMethodUser":
        add_configured_thing(thingClassId)
    elif createMethod ==  "CreateMethodDiscovery":
        thingDescriptorId = discover_thing(thingClassId)
        if thingDescriptorId == None:
            return None
        add_discovered_thing(thingClassId, thingDescriptorId)
    elif createMethod == "CreateMethodAuto":
        print "\nCan't create this thing manually. It'll be created automatically when hardware is avaiable.\n"


def add_discovered_thing(thingClassId, thingDescriptorId):
    thingClass = get_thingClass(thingClassId)
    if thingClass['setupMethod'] == "SetupMethodJustAdd":
        params = {}
        params['thingClassId'] = thingClassId
        params['thingDescriptorId'] = thingDescriptorId
        params['name'] = raw_input("\nEnter the \"name\" of the thing: ")
        response = nymea.send_command("Integrations.AddThing", params)
        if not response['status'] != "success":
            nymea.print_thing_error_code(response['params']['thingError'])
        else:
            print "Thing added successfully. Thing ID: %s\n" % response['params']['thingId']
    elif thingClass['setupMethod'] == "SetupMethodPushButton":
        params = {}
        params['thingClassId'] = thingClassId
        params['thingDescriptorId'] = thingDescriptorId
        params['name'] = raw_input("\nEnter the \"name\" of the thing: ")
        response = nymea.send_command("Integrations.PairThing", params)
        #nymea.print_json_format(response)
        if not response['status'] == "success":
            print "Pairing failed: %s", response['params']['thingError']
            return
        print "\nPairing thing %s\n\n%s\n\n" % (thingClass['displayName'], response['params']['displayMessage'])
        if response['params']['setupMethod'] == "SetupMethodPushButton":
            raw_input("\nPress \"enter\" to confirm\n")
        params = {}
        params['pairingTransactionId'] = response['params']['pairingTransactionId']
        response = nymea.send_command("Integrations.ConfirmPairing", params)
        nymea.print_thing_error_code(response['params']['thingError'])
    elif thingClass['setupMethod'] == "SetupMethodDisplayPin":
        params = {}
        params['thingClassId'] = thingClassId
        params['thingDescriptorId'] = thingDescriptorId
        params['name'] = raw_input("\nEnter the \"name\" of the thing: ")
        response = nymea.send_command("Integrations.PairThing", params)
        #nymea.print_json_format(response)
        if not response['status'] == "success":
            print "Pairing failed: %s", response['params']['thingError']
            return
        print "\nPairing thing %s\n\n%s\n\n" % (thingClass['displayName'], response['params']['displayMessage'])
        if response['params']['setupMethod'] == "SetupMethodDisplayPin":
            pin = raw_input("Pin: ")
        params = {}
        params['secret'] = pin
        params['pairingTransactionId'] = response['params']['pairingTransactionId']
        response = nymea.send_command("Integrations.ConfirmPairing", params)
        nymea.print_thing_error_code(response['params']['thingError'])


def remove_configured_thing():
    thingId = select_configured_thing()
    if not thingId:
        return None
    print "Removing thing with ThingId%s" % thingId
    params = {}
    params['thingId'] = thingId
    response = nymea.send_command("Integrations.RemoveThing", params)

    if response['params']['thingError'] == 'ThingErrorThingInRule':
            selectionString = "This thing is used in a rule. How do you want to remove the thing from the rules?"
            removePolicys = ["Remove the thing from the rules","Delete all offending rules"]
            selection = nymea.get_selection(selectionString, removePolicys)

            if removePolicys[selection] == "Remove the thing from the rules":
                params['removePolicy'] = "RemovePolicyUpdate"
            else:
                params['removePolicy'] = "RemovePolicyCascade"

            response = nymea.send_command("Integrations.RemoveThing", params)



    nymea.print_thing_error_code(response['params']['thingError'])



def edit_thing():
    thingId = select_configured_thing()
    params = {}
    params['thingId'] = thingId
    params['name'] = raw_input("\nEnter the new \"name\" of the thing: ")
    response = nymea.send_command("Integrations.EditThing", params)
    nymea.print_thing_error_code(response['params']['thingError'])


def reconfigure_thing():
    thingId = select_configured_thing()
    thing = get_thing(thingId)
    if not thing:
        return None
    thingClass = get_thingClass(thing['thingClassId'])
    if not thingClass:
        return None
    thingParamTypes = thingClass['paramTypes']
    currentThingParams = thing['params']
    createMethod = select_createMethod("Please select how do you want to edit this thing", thingClass['createMethods'])
    print " --> Using create method \"%s\"\n" % (createMethod)
    params = {}
    if createMethod == "CreateMethodUser":
        newThingParams = parameters.edit_params(currentThingParams, thingParamTypes)
        params['thingId'] = thingId
        params['thingParams'] = newThingParams
        response = nymea.send_command("Integrations.ReconfigureThing", params)
        nymea.print_thing_error_code(response['params']['thingError'])
    elif createMethod ==  "CreateMethodDiscovery":
        thingDescriptorId = discover_thing(thing['thingClassId'])
        if not thingDescriptorId:
            return None
        print "using descriptorId %s" % (thingDescriptorId)
        params['thingId'] = thingId
        params['thingDescriptorId'] = thingDescriptorId
        response = nymea.send_command("Integrations.ReconfigureThing", params)
        nymea.print_thing_error_code(response['params']['thingError'])
    elif createMethod == "CreateMethodAuto":
        newThingParams = parameters.edit_params(currentThingParams, thingParamTypes)
        params['thingId'] = thingId
        params['thingParams'] = newThingParams
        response = nymea.send_command("Integrations.ReconfigureThing", params)
        nymea.print_thing_error_code(response['params']['thingError'])


def discover_thing(thingClassId = None):
    if thingClassId == None:
        thingClassId = select_thingClass()
    thingClass = get_thingClass(thingClassId)
    if not thingClass:
        return None
    params = {}
    params['thingClassId'] = thingClassId
    discoveryParams = parameters.read_params(thingClass['discoveryParamTypes'])
    if len(discoveryParams) > 0:
        params['discoveryParams'] = discoveryParams
    print "\ndiscovering..."
    response = nymea.send_command("Integrations.DiscoverThings", params)
    if not 'thingDescriptors' in response['params']:
        print "no things found"
    thingDescriptorList = [];
    thingDescriptorIdList = [];
    for thingDescriptor in response['params']['thingDescriptors']:
        thingDescriptorList.append("%s (%s)" % (thingDescriptor['title'], thingDescriptor['description']))
        thingDescriptorIdList.append(thingDescriptor['id'])
    if not thingDescriptorIdList:
        print "\n    Timeout: no thing found."
        return None
    selection = nymea.get_selection("Please select a thing descriptor", thingDescriptorList)
    if selection != None:
        return thingDescriptorIdList[selection]
    return None


def select_createMethod(title, createMethods):
    if len(createMethods) == 0:
        print "ERROR: this thing has no createMethods. Please check the plugin code!!!"
        return None
    elif len(createMethods) == 1:
        return createMethods[0]
    else:
        selection = nymea.get_selection(title, createMethods)
        if selection == None:
            print "ERROR: could not get selection of CreateMethod"
            return None
        return createMethods[selection]


def select_configured_thing():
    things = get_configured_things()
    if not things:
        print "\n    No configured thing found.\n"
        return None
    thingList = []
    thingIdList = []
    for i in range(len(things)):
        thingList.append(things[i]['name'])
        thingIdList.append(things[i]['id'])
    selection = nymea.get_selection("Please select a thing", thingList)
    if selection != None:
        return thingIdList[selection]
    return None


def select_vendor():
    vendors = get_supported_vendors()['params']['vendors']
    if not vendors:
        print "\n    No vendors found. Please install nymea-plugins and restart nymead."
        return None
    vendorList = []
    vendorIdList = []
    for i in range(0,len(vendors)):
        vendorList.append(vendors[i]['displayName'])
        vendorIdList.append(vendors[i]['id'])
    selection = nymea.get_selection("Please select a vendor", vendorList)
    if selection != None:
        return vendorIdList[selection]
    return None


def select_thingClass():
    vendorId = select_vendor()
    if not vendorId:
        return None
    thingClasses = get_thingClasses(vendorId)
    if not thingClasses:
        print "\n    No supported things for this vendor\n"
        return None
    thingClassList = []
    thingClassIdList = []
    for i in range(0,len(thingClasses)):
        thingClassList.append(thingClasses[i]['displayName'])
        thingClassIdList.append(thingClasses[i]['id'])
    selection = nymea.get_selection("Please select thing class", thingClassList)
    if selection != None:
        return thingClassIdList[selection]
    return None


def list_configured_things():
    thingList = get_configured_things()
    if not thingList:
        print "\n    No configured thing found.\n"
        return None
    print "-> List of configured things:\n"
    for thing in thingList:
        print "%45s, id: %s, thingClassId: %s" % (get_full_thing_name(thing['id']), thing['id'], thing['thingClassId'])

def list_thingClasses(vendorId = None):
    response = get_thingClasses(vendorId)
    print "-> List of all thing classes:\n"
    for thingClass in response:
        print "%35s  %s" % (thingClass['displayName'], thingClass['id'])


def list_thing_states():
    thingId = select_configured_thing()
    if thingId == None:
        return None
    thing = get_thing(thingId)
    thingClass = get_thingClass(thing['thingClassId'])

    nymea.print_json_format(thing)
    print "-> States of thing \"%s\" %s:\n" % (get_full_thing_name(thingId), thing['id'])
    for i in range(len(thingClass['stateTypes'])):
        params = {}
        params['thingId'] = thingId
        params['stateTypeId'] = thingClass['stateTypes'][i]['id']
        response = nymea.send_command("Integrations.GetStateValue", params)
        print "%35s: %s" % (thingClass['stateTypes'][i]['displayName'], response['params']['value'])


def list_configured_thing_params():
    thingId = select_configured_thing()
    thing = get_thing(thingId)
    if not thing:
        return None
    paramTypes = get_thingClass(thing['thingClassId'])['paramTypes']
    thingParams = thing['params']
    print "-> Params of thing \"%s\" %s:\n" %(get_full_thing_name(thingId), thing['id'])
    for i in range(len(thingParams)):
        print "%35s: %s" % (parameters.getParamName(thingParams[i]['paramTypeId'], paramTypes), thingParams[i]['value'])


def list_vendors():
    response = get_supported_vendors();
    print "-> List of supported vendors:\n"
    for vendor in response['params']['vendors']:
        print "%35s  %s" % (vendor['displayName'], vendor['id'])


def print_thingClass():
    thingClassId = select_thingClass()
    if thingClassId == None:
        return None
    nymea.print_json_format(get_thingClass(thingClassId))
