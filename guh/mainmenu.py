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

import sys
import os
import curses
import traceback
import curses

import guh
import devices
import plugins
import events
import states
import actions
import rules
import logs
import notifications


MENU = "menu"
COMMAND = "command"
EXITMENU = "exitmenu"

menu_data = {
    'title': "guh-cli", 
    'type': MENU, 
    'subtitle': "Please select an option...",
    'options':[
        { 
            'title': "Devices", 
            'type': MENU,
            'subtitle': "Please select an option...",
            'options': [
                { 
                    'title': "Add a new device", 
                    'type': COMMAND, 
                    'command': 'method_add_device' 
                },
                {
                    'title': "Remove a device", 
                    'type': COMMAND, 
                    'command': 'method_remove_device' 
                },
                {
                    'title': "Edit device", 
                    'type': COMMAND, 
                    'command': 'method_edit_device' 
                },
                {
                    'title': "Execute an action", 
                    'type': COMMAND, 
                    'command': 'method_execute_action' 
                },
                {
                    'title': "List...", 
                    'type': MENU,
                    'subtitle': "Please select what you want to list...",
                    'options': [
                        {
                            'title': "List configured devices", 
                            'type': COMMAND, 
                            'command': 'method_list_configured_devices' 
                        },
                        {
                            'title': "List device parameters", 
                            'type': COMMAND, 
                            'command': 'method_list_configured_device_params' 
                        },
                        {
                            'title': "List device states", 
                            'type': COMMAND, 
                            'command': 'method_list_device_states' 
                        },
                        {
                            'title': "List supported vendors", 
                            'type': COMMAND, 
                            'command': 'method_list_vendors' 
                        },
                        {
                            'title': "List supported devices", 
                            'type': COMMAND, 
                            'command': 'method_list_deviceClasses' 
                        }
                    ]
                },
                {
                    'title': "Print...", 
                    'type': MENU,
                    'subtitle': "Please select what you want to print...",
                    'options': [
                        { 
                            'title': "Print DeviceClass", 
                            'type': COMMAND, 
                            'command': 'method_print_deviceClass' 
                        },
                        { 
                            'title': "Print ActionType", 
                            'type': COMMAND, 
                            'command': 'method_print_actionType' 
                        },
                        { 
                            'title': "Print EventType", 
                            'type': COMMAND, 
                            'command': 'method_print_eventType' 
                        },
                        { 
                            'title': "Print StateType", 
                            'type': COMMAND, 
                            'command': 'method_print_stateType' 
                        }
                    ]
                }
            ]
        },
        {
            'title': "Rules", 
            'type': MENU,
            'subtitle': "Please select an option...",
            'options': [
                { 
                    'title': "Add a new rule", 
                    'type': COMMAND, 
                    'command': 'method_add_rule' 
                },
                { 
                    'title': "Remove a rule", 
                    'type': COMMAND, 
                    'command': 'method_remove_rule' 
                },
                { 
                    'title': "Rule details", 
                    'type': COMMAND, 
                    'command': 'method_list_rule_detail' 
                },
                { 
                    'title': "Enable/Disable a rule", 
                    'type': COMMAND, 
                    'command': 'method_enable_disable_rule' 
                },
                { 
                    'title': "List rules", 
                    'type': COMMAND, 
                    'command': 'method_list_rules' 
                },
                { 
                    'title': "List rules containing a certain device", 
                    'type': COMMAND, 
                    'command': 'method_list_rules_containig_deviceId' 
                }
            ]
        },
        {
            'title': "Plugins", 
            'type': MENU,
            'subtitle': "Please select an option...",
            'options': [
                { 
                    'title': "Set plugin configuration", 
                    'type': COMMAND, 
                    'command': 'method_set_plugin_configuration' 
                },
                { 
                    'title': "Get plugin configuration", 
                    'type': COMMAND, 
                    'command': 'method_list_plugin_configuration' 
                },
                { 
                    'title': "List plugins", 
                    'type': COMMAND, 
                    'command': 'method_list_plugins' 
                },
                { 
                    'title': "Print plugin info", 
                    'type': COMMAND, 
                    'command': 'method_list_plugin_info' 
                }
            ]
        },
        {
            'title': "Log monitor", 
            'type': COMMAND, 
            'command': 'method_list_log_entries' 
        },  
        { 
            'title': "Notifications sniffer", 
            'type': COMMAND, 
            'command': 'method_notification_sniffer' 
        },     
        {
            'title': "System Info", 
            'type': MENU,
            'subtitle': "Please select an option...",
            'options': [
                { 
                    'title': "Version", 
                    'type': COMMAND, 
                    'command': 'method_list_server_info' 
                },
                { 
                    'title': "Print API", 
                    'type': COMMAND, 
                    'command': 'method_print_api' 
                },
                { 
                    'title': "List API method", 
                    'type': COMMAND, 
                    'command': 'method_print_api_method' 
                },
                { 
                    'title': "List API notification", 
                    'type': COMMAND, 
                    'command': 'method_print_api_notification' 
                },
                { 
                    'title': "List API type", 
                    'type': COMMAND, 
                    'command': 'method_print_api_type' 
                }
            ]
        }
    ]
}


def method_add_device():
    devices.add_device()

def method_remove_device():
    devices.remove_configured_device()

def method_edit_device():
    devices.edit_device()

def method_execute_action():
    actions.execute_action()

def method_add_rule():
    rules.add_rule()

def method_remove_rule():
    rules.remove_rule()

def method_enable_disable_rule():
    rules.enable_disable_rule()

def method_list_configured_device_params():
    devices.list_configured_device_params()

def method_list_device_states():
    devices.list_device_states()

def method_list_vendors():
    devices.list_vendors()
 
def method_list_plugins():
    plugins.list_plugins()
    
def method_set_plugin_configuration():
    plugins.set_plugin_configuration()
    
def method_list_plugin_configuration():
    plugins.list_plugin_configuration()
    
def method_list_plugin_info():
    plugins.list_plugin_info()
    
def method_list_configured_devices():
    devices.list_configured_devices()
    
def method_list_deviceClasses(vendorId = None):
    devices.list_deviceClasses()

def method_list_deviceClasses_by_vendor():
    method_list_deviceClasses(devices.select_vendor())
    
def method_list_rule_detail():
    rules.list_rule_details()
    
def method_list_rules():
    rules.list_rules()
    
def method_list_rules_containig_deviceId():
    rules.list_rules_containig_deviceId()

def method_list_log_entries():
    global guhHost
    global guhPort
    logs.log_window(guhHost, guhPort)
    
def method_notification_sniffer():
    global guhHost
    global guhPort
    notifications.notification_sniffer(guhHost, guhPort)
    
def method_list_server_info():
    guh.print_server_version()
    
def method_print_deviceClass():
    devices.print_deviceClass()

def method_print_actionType():
    actions.print_actionType()

def method_print_eventType():
    events.print_eventType()

def method_print_stateType():
    states.print_stateType()

def method_print_api():
    guh.print_api()

def method_print_api_method():
    guh.print_api_method()
    
def method_print_api_notification():
    guh.print_api_notifications()

def method_print_api_type():
    guh.print_api_type()

def start(host, port):
    global menu_data
    global screen
    global highlightColor
    global normalColor
    global guhHost
    global guhPort
    guhHost = host
    guhPort = port
    
    os.system('clear')
    screen = curses.initscr()
    curses.noecho()
    curses.cbreak() 
    curses.start_color() 
    screen.keypad(1)
    curses.init_pair(1,curses.COLOR_BLACK, curses.COLOR_GREEN)
    highlightColor = curses.color_pair(1) 
    normalColor = curses.A_NORMAL 
    processmenu(menu_data)

def runmenu(menu, parent):
    global screen
    global highlightColor
    global normalColor
    
    if parent is None:
        lastoption = "Exit"
    else:
        lastoption = "<-- %s menu" % parent['title']
    
    optioncount = len(menu['options']) 
    pos=0 
    oldpos=None 
    x = None 
    
    # Loop until return key is pressed
    while x !=ord('\n'):
        if pos != oldpos:
            oldpos = pos
        screen.border(0)
        screen.addstr(2,2, menu['title'], curses.A_STANDOUT) 
        screen.addstr(4,2, menu['subtitle'], curses.A_BOLD) 
        # Display all the menu items, showing the 'pos' item highlighted
        for index in range(optioncount):
            textstyle = normalColor
            if pos==index:
                textstyle = highlightColor
            screen.addstr(5 + index, 4, "%d - %s" % (index + 1, menu['options'][index]['title']), textstyle)
        textstyle = normalColor
        if pos==optioncount:
            textstyle = highlightColor
        screen.addstr( 5 + optioncount, 4, "%d - %s" % (optioncount + 1, lastoption), textstyle)
        screen.refresh()
        x = screen.getch()
        if x >= ord('1') and x <= ord(str(optioncount + 1)):
            pos = x - ord('0') - 1 
        if x == curses.KEY_DOWN:
            if pos < optioncount:
                pos += 1
            else: 
                pos = 0
        elif x == curses.KEY_UP: 
            if pos > 0:
                pos += -1
            else: 
                pos = optioncount
        elif x == curses.KEY_BACKSPACE:
            pos = optioncount
        elif x == 27:
            pos = optioncount
    # return index of the selected item
    return pos
    

def processmenu(menu, parent=None):
    global screen
    global highlightColor
    global normalColor
    optioncount = len(menu['options'])
    exitmenu = False
    while not exitmenu:
        getin = runmenu(menu, parent)
        if getin == optioncount:
            exitmenu = True
        elif menu['options'][getin]['type'] == COMMAND:
            ###############################
            curses.endwin()
            print "========================================================"
            methodCall = globals()[menu['options'][getin]['command']]
            methodCall()
            print "--------------------------------------------------------"
            raw_input("\nPress \"enter\" to return to the menu...\n")
            ###############################
            screen = curses.initscr()
            screen.clear()
            curses.reset_prog_mode()
            curses.curs_set(1)
            curses.curs_set(0)
        elif menu['options'][getin]['type'] == MENU:
            screen.clear()
            processmenu(menu['options'][getin], menu)
            screen.clear()
        elif menu['options'][getin]['type'] == EXITMENU:
            exitmenu = True