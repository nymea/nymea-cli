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

import sys
import os
import curses
import traceback
import curses

import nymea
import things
import plugins
import events
import states
import actions
import rules
import logs
import notifications
import settings
import zigbeemanager


MENU = "menu"
COMMAND = "command"
EXITMENU = "exitmenu"

menu_data = {
    'title': "nymea-cli",
    'type': MENU,
    'subtitle': "Please select an option...",
    'options':[
        {
            'title': "Things",
            'type': MENU,
            'subtitle': "Please select an option...",
            'options': [
                {
                    'title': "Add a new thing",
                    'type': COMMAND,
                    'command': 'method_add_thing'
                },
                {
                    'title': "Remove a thing",
                    'type': COMMAND,
                    'command': 'method_remove_thing'
                },
                {
                    'title': "Edit thing",
                    'type': COMMAND,
                    'command': 'method_edit_thing'
                },
                {
                    'title': "Reconfigure thing",
                    'type': COMMAND,
                    'command': 'method_reconfigure_thing'
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
                            'title': "List configured things",
                            'type': COMMAND,
                            'command': 'method_list_configured_things'
                        },
                        {
                            'title': "List thing parameters",
                            'type': COMMAND,
                            'command': 'method_list_configured_thing_params'
                        },
                        {
                            'title': "List thing states",
                            'type': COMMAND,
                            'command': 'method_list_thing_states'
                        },
                        {
                            'title': "List supported vendors",
                            'type': COMMAND,
                            'command': 'method_list_vendors'
                        },
                        {
                            'title': "List supported things",
                            'type': COMMAND,
                            'command': 'method_list_thingClasses'
                        },
                        {
                            'title': "List thing hirarchy",
                            'type': COMMAND,
                            'command': 'method_printSupportedThingsStructure'
                        }
                    ]
                },
                {
                    'title': "Print...",
                    'type': MENU,
                    'subtitle': "Please select what you want to print...",
                    'options': [
                        {
                            'title': "Print thingClass",
                            'type': COMMAND,
                            'command': 'method_print_thingClass'
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
                    'title': "Edit Rule",
                    'type': COMMAND,
                    'command': 'method_edit_rule'
                },
                {
                    'title': "Rule details",
                    'type': COMMAND,
                    'command': 'method_list_rule_detail'
                },
                {
                    'title': "Execute rule actions",
                    'type': COMMAND,
                    'command': 'method_execute_rule_actions'
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
                    'title': "List rules containing a certain thing",
                    'type': COMMAND,
                    'command': 'method_list_rules_containig_thingId'
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
            'title': "Logs",
            'type': MENU,
            'subtitle': "Please select an option...",
            'options': [
                {
                    'title': "Thing logs",
                    'type': COMMAND,
                    'command': 'method_thing_logs'
                },
                {
                    'title': "Thing state logs",
                    'type': COMMAND,
                    'command': 'method_thing_state_logs'
                },
                {
                    'title': "Rule logs",
                    'type': COMMAND,
                    'command': 'method_rule_logs'
                },
                {
                    'title': "Logs from the last...",
                    'type': MENU,
                    'subtitle': "Please select an option...",
                    'options': [
                        {
                            'title': "...30 minutes",
                            'type': COMMAND,
                            'command': 'method_last_30_minutes'
                        },
                        {
                            'title': "...60 minutes",
                            'type': COMMAND,
                            'command': 'method_last_60_minutes'
                        },
                        {
                            'title': "...120 minutes",
                            'type': COMMAND,
                            'command': 'method_last_120_minutes'
                        },
                        {
                            'title': "...6 hours",
                            'type': COMMAND,
                            'command': 'method_last_6_hours'
                        },
                        {
                            'title': "...12 hours",
                            'type': COMMAND,
                            'command': 'method_last_12_hours'
                        },
                        {
                            'title': "...24 hours",
                            'type': COMMAND,
                            'command': 'method_last_24_hours'
                        },
                        {
                            'title': "...48 hours",
                            'type': COMMAND,
                            'command': 'method_last_48_hours'
                        },
                        {
                            'title': "...72 hours",
                            'type': COMMAND,
                            'command': 'method_last_72_hours'
                        }
                    ]
                },
                {
                    'title': "Create Custom Log Filter",
                    'type': COMMAND,
                    'command': 'method_create_logfilter'
                },
                {
                    'title': "Logmonitor (all logs)",
                    'type': COMMAND,
                    'command': 'method_list_log_entries'
                }
            ]
        },
        {
            'title': "Notifications sniffer",
            'type': COMMAND,
            'command': 'method_notification_sniffer'
        },
        {
            'title': "System info",
            'type': MENU,
            'subtitle': "Please select an option...",
            'options': [
                {
                    'title': "Version",
                    'type': COMMAND,
                    'command': 'method_list_server_info'
                },
                {
                    'title': "Print API JSON",
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
                    'title': "List API object",
                    'type': COMMAND,
                    'command': 'method_print_api_object'
                },
                {
                    'title': "List API flags",
                    'type': COMMAND,
                    'command': 'method_print_api_flags'
                },
                {
                    'title': "List API enums",
                    'type': COMMAND,
                    'command': 'method_print_api_enums'
                }
            ]
        },
        {
            'title': "Settings",
            'type': MENU,
            'subtitle': "Please select an option...",
            'options': [
                {
                    'title': "Show configurations",
                    'type': COMMAND,
                    'command': 'method_list_configurations'
                },
                {
                    'title': "Set server name",
                    'type': COMMAND,
                    'command': 'method_set_serverName'
                },
                {
                    'title': "Set language",
                    'type': COMMAND,
                    'command': 'method_set_language'
                },
                {
                    'title': "Set time zone",
                    'type': COMMAND,
                    'command': 'method_set_timezone'
                },
                {
                    'title': "Configure debug server",
                    'type': COMMAND,
                    'command': 'method_set_debug_server_interface'
                },
                {
                    'title': "Configure server interfaces",
                    'type': MENU,
                    'subtitle': "Please select an option...",
                    'options': [
                        {
                            'title': "TCP server",
                            'type': MENU,
                            'subtitle': "Please select an option...",
                            'options': [
                                {
                                    'title': "Show TCP server configuration",
                                    'type': COMMAND,
                                    'command': 'method_show_tcpServer_configuration'
                                },
                                {
                                    'title': "Configure TCP server",
                                    'type': COMMAND,
                                    'command': 'method_configure_tcpServer'
                                }
                            ]
                        },
                        {
                            'title': "Web server",
                            'type': MENU,
                            'subtitle': "Please select an option...",
                            'options': [
                                {
                                    'title': "Show web server configuration",
                                    'type': COMMAND,
                                    'command': 'method_show_webServer_configuration'
                                },
                                {
                                    'title': "Configure web server",
                                    'type': COMMAND,
                                    'command': 'method_configure_webServer'
                                }
                            ]
                        },
                        {
                            'title': "Web socket server",
                            'type': MENU,
                            'subtitle': "Please select an option...",
                            'options': [
                                {
                                    'title': "Show web socket server configuration",
                                    'type': COMMAND,
                                    'command': 'method_show_webSocketServer_configuration'
                                },
                                {
                                    'title': "Configure web socket server",
                                    'type': COMMAND,
                                    'command': 'method_configure_webSocketServer'
                                }
                            ]
                        }
                    ]
                },
                {
                    'title': "Network manager",
                    'type': MENU,
                    'subtitle': "Please select an option...",
                    'options': [
                        {
                            'title': "Show network status",
                            'type': COMMAND,
                            'command': 'method_show_network_status'
                        },
                        {
                            'title': "Network things",
                            'type': COMMAND,
                            'command': 'method_list_network_things'
                        },
                        {
                            'title': "Enable / Disable networking",
                            'type': COMMAND,
                            'command': 'method_enable_networking'
                        },
                        {
                            'title': "Enable / Disable wireless networking",
                            'type': COMMAND,
                            'command': 'method_enable_wirelessnetworking'
                        },
                        {
                            'title': "List wireless access points",
                            'type': COMMAND,
                            'command': 'method_list_wirelessaccesspoints'
                        },
                        {
                            'title': "Scan wireless access points",
                            'type': COMMAND,
                            'command': 'method_scan_wirelessaccesspoints'
                        },
                        {
                            'title': "Connect to wifi network",
                            'type': COMMAND,
                            'command': 'method_connect_wifi'
                        },
                        {
                            'title': "Disconnect network thing",
                            'type': COMMAND,
                            'command': 'method_disconnect_networkthing'
                        }
                    ]
                }
            ]
        },
        {
            'title': "Zigbee Manager",
            'type': MENU,
            'subtitle': "Please select an option...",
            'options': [
                {
                    'title': "List zigbee adapters",
                    'type': COMMAND,
                    'command': 'method_list_zigbee_adapters'
                },
                {
                    'title': "List available backends",
                    'type': COMMAND,
                    'command': 'method_list_zigbee_backends'
                },
                {
                    'title': "List zigbee networks",
                    'type': COMMAND,
                    'command': 'method_list_zigbee_networks'
                },
                {
                    'title': "Add zigbee network",
                    'type': COMMAND,
                    'command': 'method_add_zigbee_network'
                },
                {
                    'title': "Remove zigbee network",
                    'type': COMMAND,
                    'command': 'method_remove_zigbee_network'
                },
                {
                    'title': "Factory reset zigbee network",
                    'type': COMMAND,
                    'command': 'method_factory_reset_zigbee_network'
                },
                {
                    'title': "Allow/deny nodes to join zigbee network",
                    'type': COMMAND,
                    'command': 'method_permit_join_zigbee_network'
                }
            ]
        }
    ]
}

######################################################################
# Things
######################################################################
def method_add_thing():
    things.add_thing()

def method_remove_thing():
    things.remove_configured_thing()

def method_edit_thing():
    things.edit_thing()

def method_reconfigure_thing():
    things.reconfigure_thing()

def method_execute_action():
    actions.execute_action()

def method_list_configured_thing_params():
    things.list_configured_thing_params()

def method_list_thing_states():
    things.list_thing_states()

def method_list_vendors():
    things.list_vendors()

def method_list_plugins():
    plugins.list_plugins()

def method_set_plugin_configuration():
    plugins.set_plugin_configuration()

def method_list_plugin_configuration():
    plugins.list_plugin_configuration()

def method_list_plugin_info():
    plugins.list_plugin_info()

def method_list_configured_things():
    things.list_configured_things()

def method_list_thingClasses(vendorId = None):
    things.list_thingClasses()

def method_list_thingClasses_by_vendor():
    method_list_thingClasses(things.select_vendor())

def method_printSupportedThingsStructure():
    nymea.printSupportedThingsStructure()

######################################################################
# Rules
######################################################################

def method_add_rule():
    rules.add_rule()

def method_remove_rule():
    rules.remove_rule()

def method_enable_disable_rule():
    rules.enable_disable_rule()

def method_edit_rule():
    rules.edit_rule()

def method_list_rule_detail():
    rules.list_rule_details()

def method_execute_rule_actions():
    rules.execute_rule_actions()

def method_list_rules():
    rules.list_rules()

def method_list_rules_containig_thingId():
    rules.list_rules_containig_thingId()

######################################################################
# Logs
######################################################################

def method_list_log_entries():
    global nymeaHost
    global nymeaPort
    logs.log_window(nymeaHost, nymeaPort)

def method_thing_logs():
    global nymeaHost
    global nymeaPort
    params = logs.create_thing_logfilter()
    print "\nThe filter:\n"
    nymea.print_json_format(params)
    nymea.debug_stop()
    if params:
        logs.log_window(nymeaHost, nymeaPort, params)

def method_thing_state_logs():
    global nymeaHost
    global nymeaPort
    params = logs.create_thing_state_logfilter()
    print "\nThe filter:\n"
    nymea.print_json_format(params)
    nymea.debug_stop()
    if params:
        logs.log_window(nymeaHost, nymeaPort, params)

def method_rule_logs():
    global nymeaHost
    global nymeaPort
    params = logs.create_rule_logfilter()
    print "\nThe filter:\n"
    nymea.print_json_format(params)
    nymea.debug_stop()
    if params:
        logs.log_window(nymeaHost, nymeaPort, params)

def method_last_30_minutes():
    global nymeaHost
    global nymeaPort
    params = logs.create_last_time_logfilter(30)
    print "\nThe filter:\n"
    nymea.print_json_format(params)
    nymea.debug_stop()
    if params:
        logs.log_window(nymeaHost, nymeaPort, params)

def method_last_60_minutes():
    global nymeaHost
    global nymeaPort
    params = logs.create_last_time_logfilter(60)
    print "\nThe filter:\n"
    nymea.print_json_format(params)
    nymea.debug_stop()
    if params:
        logs.log_window(nymeaHost, nymeaPort, params)

def method_last_120_minutes():
    global nymeaHost
    global nymeaPort
    params = logs.create_last_time_logfilter(120)
    print "\nThe filter:\n"
    nymea.print_json_format(params)
    nymea.debug_stop()
    if params:
        logs.log_window(nymeaHost, nymeaPort, params)

def method_last_6_hours():
    global nymeaHost
    global nymeaPort
    params = logs.create_last_time_logfilter(360)
    print "\nThe filter:\n"
    nymea.print_json_format(params)
    nymea.debug_stop()
    if params:
        logs.log_window(nymeaHost, nymeaPort, params)

def method_last_12_hours():
    global nymeaHost
    global nymeaPort
    params = logs.create_last_time_logfilter(720)
    print "\nThe filter:\n"
    nymea.print_json_format(params)
    nymea.debug_stop()
    if params:
        logs.log_window(nymeaHost, nymeaPort, params)

def method_last_24_hours():
    global nymeaHost
    global nymeaPort
    params = logs.create_last_time_logfilter(24 * 60)
    print "\nThe filter:\n"
    nymea.print_json_format(params)
    nymea.debug_stop()
    if params:
        logs.log_window(nymeaHost, nymeaPort, params)

def method_last_48_hours():
    global nymeaHost
    global nymeaPort
    params = logs.create_last_time_logfilter(48 * 60)
    print "\nThe filter:\n"
    nymea.print_json_format(params)
    nymea.debug_stop()
    if params:
        logs.log_window(nymeaHost, nymeaPort, params)

def method_last_72_hours():
    global nymeaHost
    global nymeaPort
    params = logs.create_last_time_logfilter(72 * 60)
    print "\nThe filter:\n"
    nymea.print_json_format(params)
    nymea.debug_stop()
    if params:
        logs.log_window(nymeaHost, nymeaPort, params)

def method_create_logfilter():
    global nymeaHost
    global nymeaPort
    params = logs.create_logfilter()
    logs.log_window(nymeaHost, nymeaPort, params)

######################################################################
# Notification sniffer
######################################################################

def method_notification_sniffer():
    global nymeaHost
    global nymeaPort
    notifications.notification_sniffer(nymeaHost, nymeaPort)

######################################################################
# System info
######################################################################

def method_list_server_info():
    nymea.print_server_version()

def method_print_thingClass():
    things.print_thingClass()

def method_print_actionType():
    actions.print_actionType()

def method_print_eventType():
    events.print_eventType()

def method_print_stateType():
    states.print_stateType()

def method_print_api():
    nymea.print_api()

def method_print_api_method():
    nymea.print_api_method()

def method_print_api_notification():
    nymea.print_api_notifications()

def method_print_api_object():
    nymea.print_api_object()

def method_print_api_enums():
    nymea.print_api_enum()

def method_print_api_flags():
    nymea.print_api_flag()


######################################################################
# System settings
######################################################################

def method_list_configurations():
    settings.list_configurations()

def method_list_timezones():
    settings.list_timezones()

def method_set_language():
    settings.set_language()

def method_set_timezone():
    settings.set_timezone()

def method_set_serverName():
    settings.set_serverName()

def method_set_debug_server_interface():
    settings.set_debug_server_interface()

def method_show_tcpServer_configuration():
    settings.show_tcpServer_configuration()

def method_configure_tcpServer():
    settings.configure_tcpServer()

def method_configure_webServer():
    settings.configure_webServer()

def method_show_webServer_configuration():
    settings.show_webServer_configuration()

def method_configure_webSocketServer():
    settings.configure_webSocketServer()

def method_show_webSocketServer_configuration():
    settings.show_webSocketServer_configuration()

def method_cloud_authenticate():
    settings.cloud_authenticate()

def method_cloud_status():
    settings.print_cloud_status()

def method_enable_cloud_connection():
    settings.enable_cloud_connection()

def method_list_wirelessaccesspoints():
    settings.list_wirelessaccesspoints()

def method_scan_wirelessaccesspoints():
    settings.scan_wirelessaccesspoints()

def method_show_network_status():
    settings.show_network_status()

def method_list_network_things():
    settings.list_network_things()

def method_connect_wifi():
    settings.connect_wifi()

def method_enable_networking():
    settings.enable_networking()

def method_enable_wirelessnetworking():
    settings.enable_wirelessnetworking()

def method_disconnect_networkthing():
    settings.disconnect_networkthing()

######################################################################
# Zigbee manager
######################################################################

def method_list_zigbee_adapters():
    zigbeemanager.list_available_adapters()

def method_list_zigbee_backends():
    zigbeemanager.list_backends()

def method_list_zigbee_networks():
    zigbeemanager.list_networks()

def method_add_zigbee_network():
    zigbeemanager.add_network()

def method_remove_zigbee_network():
    zigbeemanager.remove_network()

def method_factory_reset_zigbee_network():
    zigbeemanager.factory_reset_network()

def method_permit_join_zigbee_network():
    zigbeemanager.permit_join_network()

######################################################################
# Menu functions
#####################################################################

def start(host, port):
    global menu_data
    global screen
    global highlightColor
    global normalColor
    global nymeaHost
    global nymeaPort
    nymeaHost = host
    nymeaPort = port

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


######################################################################
# Process menu
######################################################################

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
