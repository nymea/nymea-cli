# -*- coding: UTF-8 -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#                                                                         #
#  Copyright (C) 2015 - 2020gu  de Simon Stuerz <simon.stuerz@guh.io>           #
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
import selector

def list_available_adapters():
    params = {}
    response = nymea.send_command("Zigbee.GetAdapters", params)
    if len(response['params']['adapters']) == 0:
        print "There are no adapters available."
        return None

    nymea.print_json_format(response['params'])


def list_networks():
    params = {}
    response = nymea.send_command("Zigbee.GetNetworks", params)
    nymea.print_json_format(response['params'])


def add_network():
    # First get the list of adapters
    response = nymea.send_command("Zigbee.GetAdapters", {})
    if len(response['params']['adapters']) == 0:
        print "There are no adapters available."
        return None

    adapterList = [];
    for adapter in response['params']['adapters']:
        adapterList.append("%s (%s) - %s" % (adapter['description'], adapter['serialPort'], adapter['name']))

    selection = nymea.get_selection("Please select a device descriptor", adapterList)
    selectedAdapter = {}
    if selection != None:
        selectedAdapter = response['params']['adapters'][selection]
    else:
        print "ERROR: invalid adapter selection."
        return None

    print("Selected adapter:")
    nymea.print_json_format(selectedAdapter)
    params = {}
    params["serialPort"] = selectedAdapter["serialPort"]
    params["baudRate"] = selectedAdapter["baudRate"]
    params["backendType"] = selectedAdapter["backendType"]
    response = nymea.send_command("Zigbee.AddNetwork", params)
    print("Add network returned %s" % response["params"]["zigbeeError"])
    if response["params"]["zigbeeError"] == "ZigbeeErrorNoError":
        print("New network has the uuid %s" % response["params"]["networkUuid"])


def remove_network():
    params = {}
    response = nymea.send_command("Zigbee.GetNetworks", params)
    nymea.print_json_format(response['params'])
    if len(response['params']['zigbeeNetworks']) == 0:
        print "ERROR: there are no networks configured."
        return None

    networkList = [];
    for network in response['params']['zigbeeNetworks']:
        networkList.append("%s (channel %s) - %s" % (network['macAddress'], network['channel'], network['serialPort']))

    selection = nymea.get_selection("Please select the network you want to remove", networkList)
    selectedNetwork = {}
    if selection != None:
        selectedNetwork = response['params']['zigbeeNetworks'][selection]
    else:
        print "ERROR: invalid network selection."
        return None

    print("Selected network:")
    nymea.print_json_format(selectedNetwork)
    params = {}
    params["networkUuid"] = selectedNetwork["networkUuid"]
    response = nymea.send_command("Zigbee.RemoveNetwork", params)
    print("Remove network returned %s" % response["params"]["zigbeeError"])


def factory_reset_network():
    params = {}
    response = nymea.send_command("Zigbee.GetNetworks", params)
    nymea.print_json_format(response['params'])
    if len(response['params']['zigbeeNetworks']) == 0:
        print "ERROR: there are no networks configured."
        return None

    networkList = [];
    for network in response['params']['zigbeeNetworks']:
        networkList.append("%s (channel %s) - %s" % (network['macAddress'], network['channel'], network['serialPort']))

    selection = nymea.get_selection("Please select the network you want to factory reset", networkList)
    selectedNetwork = {}
    if selection != None:
        selectedNetwork = response['params']['zigbeeNetworks'][selection]
    else:
        print "ERROR: invalid network selection."
        return None

    print("Selected network:")
    nymea.print_json_format(selectedNetwork)
    params = {}
    params["networkUuid"] = selectedNetwork["networkUuid"]
    response = nymea.send_command("Zigbee.FactoryResetNetwork", params)
    print("Remove network returned %s" % response["params"]["zigbeeError"])


def permit_join_network():
    params = {}
    response = nymea.send_command("Zigbee.GetNetworks", params)
    nymea.print_json_format(response['params'])
    if len(response['params']['zigbeeNetworks']) == 0:
        print "ERROR: there are no networks configured."
        return None

    networkList = [];
    for network in response['params']['zigbeeNetworks']:
        networkList.append("%s (channel %s) - %s" % (network['macAddress'], network['channel'], network['serialPort']))

    selection = nymea.get_selection("Please select the network you want to allow/deny joining nodes", networkList)
    selectedNetwork = {}
    if selection != None:
        selectedNetwork = response['params']['zigbeeNetworks'][selection]
    else:
        print "ERROR: invalid network selection."
        return None

    print("Selected network:")
    timeout = int(raw_input("Please enter the duration (0 - 255 seconds) for how long the network should be open (0 for closing it): "))
    params = {}
    params["duration"] = timeout
    params["networkUuid"] = selectedNetwork["networkUuid"]
    response = nymea.send_command("Zigbee.SetPermitJoin", params)
    print("SetPermitJoin returned %s" % response["params"]["zigbeeError"])
