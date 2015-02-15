#!/usr/bin/python

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
#                                                                         #
#  Copyright (C) 2015 guh                                                 #
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
import telnetlib
import json
import curses
import traceback
import atexit
import time

import guh
import devices
import events
import actions
import rules
import logs

from time import sleep
import curses
import os


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
		    'title': "Execute an action", 
		    'type': COMMAND, 
		    'command': 'method_execute_action' 
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
		},
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
		},
	    ]
	},
	{
	    'title': "Logs", 
	    'type': MENU,
	    'subtitle': "Please select an option...",
	    'options': [
		{ 
		    'title': "List log entries", 
		    'type': COMMAND, 
		    'command': 'method_list_log_entries' 
		}
	    ]
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
		    'title': "List plugins", 
		    'type': COMMAND, 
		    'command': 'method_list_plugins' 
		},
		{ 
		    'title': "List plugin configuration", 
		    'type': COMMAND, 
		    'command': 'method_list_plugin_configuration' 
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
    devices.list_plugins()
    
def method_list_plugin_configuration():
    devices.list_plugin_configuration()
    
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
    logs.log_window()

def method_list_server_info():
    guh.print_server_version()

def method_print_api():
    guh.print_api()

def method_print_api_method():
    guh.print_api_method()
    
def method_print_api_notification():
    guh.print_api_notifications()

def method_print_api_type():
    guh.print_api_type()

def runmenu(menu, parent):
    # work out what text to display as the last menu option
    if parent is None:
	lastoption = "Exit"
    else:
	lastoption = "Return to %s menu" % parent['title']
    
    optioncount = len(menu['options']) # how many options in this menu
    pos=0 #pos is the zero-based index of the hightlighted menu option. Every time runmenu is called, position returns to 0, when runmenu ends the position is returned and tells the program what opt$
    oldpos=None # used to prevent the screen being redrawn every time
    x = None #control for while loop, let's you scroll through options until return key is pressed then returns pos to program
    
    # Loop until return key is pressed
    while x !=ord('\n'):
	if pos != oldpos:
	    oldpos = pos
	screen.border(0)
	screen.addstr(2,2, menu['title'], curses.A_STANDOUT) # Title for this menu
	screen.addstr(4,2, menu['subtitle'], curses.A_BOLD) #Subtitle for this menu
	# Display all the menu items, showing the 'pos' item highlighted
	for index in range(optioncount):
	    textstyle = n
	    if pos==index:
		textstyle = h
	    screen.addstr(5+index,4, "%d - %s" % (index+1, menu['options'][index]['title']), textstyle)
	# Now display Exit/Return at bottom of menu
	textstyle = n
	if pos==optioncount:
	    textstyle = h
	screen.addstr(5+optioncount,4, "%d - %s" % (optioncount+1, lastoption), textstyle)
	screen.refresh()
	# finished updating screen
	x = screen.getch() # Gets user input
	# What is user input?
	if x >= ord('1') and x <= ord(str(optioncount+1)):
	    pos = x - ord('0') - 1 # convert keypress back to a number, then subtract 1 to get index
	elif x == 258: # down arrow
	    if pos < optioncount:
		pos += 1
	    else: pos = 0
	elif x == 259: # up arrow
	    if pos > 0:
		pos += -1
	    else: pos = optioncount
    # return index of the selected item
    return pos
    

def processmenu(menu, parent=None):
    optioncount = len(menu['options'])
    exitmenu = False
    while not exitmenu: #Loop until the user exits the menu
	getin = runmenu(menu, parent)
	if getin == optioncount:
	    exitmenu = True
	elif menu['options'][getin]['type'] == COMMAND:
	    #screen.clear()
	    ###############################
	    curses.endwin()
	    print "========================================================"
	    methodCall = globals()[menu['options'][getin]['command']]
	    methodCall()
	    print "--------------------------------------------------------"
	    raw_input("\nPress \"enter\" to return to the menu...\n")
	    ###############################
	    global screen
	    screen = curses.initscr()
	    screen.clear()
	    curses.reset_prog_mode()   # reset to 'current' curses environment
	    curses.curs_set(1)         # reset doesn't do this right
	    curses.curs_set(0)
	elif menu['options'][getin]['type'] == MENU:
	    screen.clear()
	    processmenu(menu['options'][getin], menu)
	    screen.clear() 
	elif menu['options'][getin]['type'] == EXITMENU:
	    exitmenu = True
    

# Main 
if not guh.init_connection():
    exit()
else:
    
    os.system('clear')
    screen = curses.initscr() #initializes a new window for capturing key presses
    try:
	curses.noecho() # Disables automatic echoing of key presses (prevents program from input each key twice)
	curses.cbreak() # Disables line buffering (runs each key as it is pressed rather than waiting for the return key to pressed)
	curses.start_color() # Lets you use colors when highlighting selected menu option
	screen.keypad(1) # Capture input from keypad
    
	curses.init_pair(1,curses.COLOR_BLACK, curses.COLOR_GREEN) # Sets up color pair #1, it does black text with green background
	h = curses.color_pair(1) #h is the coloring for a highlighted menu option
	n = curses.A_NORMAL #n is the coloring for a non highlighted menu option
    
	processmenu(menu_data)
    finally:
	curses.endwin()
	os.system('clear')

    
