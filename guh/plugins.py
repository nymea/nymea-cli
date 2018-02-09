# -*- coding: UTF-8 -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
#                                                                         #
#  Copyright (C) 2015-2018 Simon Stuerz <simon.stuerz@guh.io>             #
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
import parameters

def get_plugins():
    return guh.send_command("Devices.GetPlugins")['params']['plugins']


def get_plugin_configuration(pluginId):
    params = {}
    params['pluginId'] = pluginId
    return guh.send_command("Devices.GetPluginConfiguration", params)['params']['configuration']


def set_plugin_configuration():
    pluginId = select_configurable_plugin()
    plugin = get_plugin(pluginId)
    paramTypes = plugin['paramTypes']
    params = {}
    params['pluginId'] = pluginId
    params['configuration'] = parameters.read_params(paramTypes)
    response = guh.send_command("Devices.SetPluginConfiguration", params)
    guh.print_device_error_code(response['params']['deviceError'])
    

def get_plugin(pluginId):
    plugins = get_plugins()
    for plugin in plugins:
        if plugin['id'] == pluginId:
            return plugin
    return None


def list_plugins():
    plugins = get_plugins();
    if not plugins:
        print "\n    No plugins found. Please install guh-plugins and restart guhd."
        return None
    print "-> List of supported plugins:\n"
    for plugin in plugins:
        print "%35s %s" % (plugin['displayName'], plugin['id'])
    
def getParamName(paramTypes, paramTypeId):
    for paramType in paramTypes:
        if paramType['id'] == paramTypeId:
            return paramType['displayName']
    
    
def list_plugin_configuration():
    pluginId = select_configurable_plugin()
    if not pluginId:
        return None
    plugin = get_plugin(pluginId)
    pluginConfiguration = get_plugin_configuration(pluginId)
    if not pluginConfiguration:
        print "\n    The plugin \"%s\" has no configuration parameters.\n" % (plugin['displayName'])
        return None
    print "-> The plugin \"%s\" %s has following configurations:\n" % (plugin['displayName'], plugin['id'])
    for i in range(len(pluginConfiguration)):
        paramTypeId = pluginConfiguration[i]['paramTypeId']
        print("%35s (%s): %s" % (getParamName(plugin['paramTypes'], paramTypeId), paramTypeId, pluginConfiguration[i]['value']))
    

def list_plugin_info():
    pluginId = select_plugin()
    if not pluginId:
        return None
    guh.print_json_format(get_plugin(pluginId))
    

def select_plugin():
    plugins = get_plugins()
    if not plugins:
        print "\n    No plugins found. Please install guh-plugins and restart guhd."
        return None
    pluginList = []
    pluginIdList = []
    for i in range(0,len(plugins)):
        pluginList.append(plugins[i]['displayName'])
        pluginIdList.append(plugins[i]['id'])
    selection = guh.get_selection("Please select a plugin", pluginList)
    if selection != None:
        return pluginIdList[selection]
    return None


def select_configurable_plugin():
    plugins = get_plugins()
    if not plugins:
        print "\n    No plugins found. Please install guh-plugins and restart guhd."
        return None
    pluginList = []
    pluginIdList = []
    for i in range(0,len(plugins)):
        if (len(plugins[i]['paramTypes']) > 0):
            #guh.print_json_format(plugins[i])
            pluginList.append(plugins[i]['displayName'])
            pluginIdList.append(plugins[i]['id'])
    selection = guh.get_selection("Please select a plugin", pluginList)
    if selection != None:
        return pluginIdList[selection]
    return None




