#!/usr/bin/python

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

from time import sleep
import curses
import os
screen = curses.initscr() #initializes a new window for capturing key presses
curses.noecho() # Disables automatic echoing of key presses (prevents program from input each key twice)
curses.cbreak() # Disables line buffering (runs each key as it is pressed rather than waiting for the return key to pressed)
curses.start_color() # Lets you use colors when highlighting selected menu option
screen.keypad(1) # Capture input from keypad
 
# Change this to use different colors when highlighting
curses.init_pair(1,curses.COLOR_BLACK, curses.COLOR_WHITE) # Sets up color pair #1, it does black text with white background
h = curses.color_pair(1) #h is the coloring for a highlighted menu option
n = curses.A_NORMAL #n is the coloring for a non highlighted menu option
 
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
		    'title': "List configured devices", 
		    'type': COMMAND, 
		    'command': 'method_list_configured_devices' 
		},
		{ 
		    'title': "List supported devices", 
		    'type': COMMAND, 
		    'command': 'method_list_deviceClasses' 
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
		    'title': "List configured rules", 
		    'type': COMMAND, 
		    'command': 'method_list_rules' 
		},
		{ 
		    'title': "List rules containing a certain device", 
		    'type': COMMAND, 
		    'command': 'method_list_rules_containig_deviceId' 
		},
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
    deviceId = devices.select_configured_device()
    device = devices.get_device(deviceId)
    deviceParams = device['params']
    print "\nParams of the device with the id ", deviceId, "\n"
    print "=== Params ==="
    for i in range(len(deviceParams)):
        print "%30s: %s" % (deviceParams[i]['name'], deviceParams[i]['value'])
    print "=== Params ==="


def method_list_device_states():
    deviceId = devices.select_configured_device()
    device = devices.get_device(deviceId)
    deviceClass = devices.get_deviceClass(device['deviceClassId'])
    print "\n\n=== States for device %s (%s) ===" % (device['name'], deviceId)
    for i in range(len(deviceClass['stateTypes'])):
        params = {}
        params['deviceId'] = deviceId
        params['stateTypeId'] = deviceClass['stateTypes'][i]['id']
        response = send_command("Devices.GetStateValue", params)
        print "%30s: %s" % (deviceClass['stateTypes'][i]['name'], response['params']['value'])
    print "=== States ==="  

  
def method_list_vendors():
    response = devices.get_supported_vendors();
    print "=== Vendors ==="
    for vendor in response['params']['vendors']:
        print "%30s  %s" % (vendor['name'], vendor['id'])
    print "=== Vendors ==="
    

def method_list_configured_devices():
    deviceList = devices.get_configured_devices()
    print "=== Configured Devices ==="
    for device in deviceList:
        print "Name: %40s, ID: %s, DeviceClassID: %s" % (device['name'], device['id'], device['deviceClassId'])
    print "=== Configured Devices ==="


def method_list_deviceClasses(vendorId = None):
    response = devices.get_deviceClasses(vendorId)
    print "=== DeviceClasses ==="
    for deviceClass in response:
        print "%40s  %s" % (deviceClass['name'], deviceClass['id'])
    print "=== DeviceClasses ==="


def method_list_deviceClasses_by_vendor():
    vendorId = devices.select_vendor()
    method_list_deviceClasses(vendorId)


def method_list_rule_detail():
    ruleId = rules.select_rule()
    if ruleId == "":
        print "\n    No rules found"
        return None
    params = {}
    params['ruleId'] = ruleId
    response = guh.send_command("Rules.GetRuleDetails", params)
    print response
    print "\nDetails for rule", ruleId, "which currently is", rules.get_rule_status(ruleId) 
    print "\nEvents ->", guh.get_stateEvaluator_string(response['params']['rule']['stateEvaluator']['operator']), ":"
    for i in range(len(response['params']['rule']['eventDescriptors'])):
        eventDescriptor = response['params']['rule']['eventDescriptors'][i]
        device = devices.get_device(eventDescriptor['deviceId'])
        eventType = events.get_eventType(eventDescriptor['eventTypeId'])
        paramDescriptors = eventDescriptor['paramDescriptors']
        print  "%5s. -> %40s -> eventTypeId: %10s: " %(i, device['name'], eventType['name'])
        for i in range(len(paramDescriptors)):
            print "%58s %s %s" %(paramDescriptors[i]['name'], guh.get_valueOperator_string(paramDescriptors[i]['operator']), paramDescriptors[i]['value'])
    print "\nActions:"
    for i in range(len(response['params']['rule']['actions'])):
        action = response['params']['rule']['actions'][i]
        device = devices.get_device(action['deviceId'])
        actionType = actions.get_actionType(response['params']['rule']['actions'][i]['actionTypeId'])
        actionParams = response['params']['rule']['actions'][i]['params']
        print  "%5s. ->  %40s -> action: %s" %(i, device['name'], actionType['name'])
        for i in range(len(actionParams)):
            print "%61s: %s" %(actionParams[i]['name'], actionParams[i]['value'])


def method_list_rules():
    response = guh.send_command("Rules.GetRules", {})
    if not response['params']['ruleIds']:
        print "\n    No rules found."
        return None
    print "\nRules found:"
    for i in range(len(response['params']['ruleIds'])):
	ruleId = response['params']['ruleIds'][i]
	params = {}
	params['ruleId'] = ruleId
	ruleDetail = guh.send_command("Rules.GetRuleDetails", params)
	print response['params']['ruleIds'][i], "(", rules.get_rule_status(ruleId), ")"


def method_list_rules_containig_deviceId():
    deviceId = devices.select_configured_device()
    device = devices.get_device(deviceId)
    params = {}
    params['deviceId'] = deviceId
    response = guh.send_command("Rules.FindRules", params)
    if not response['params']['ruleIds']:
        print "\nThere is no rule containig this device."
        return None
    
    print "\nFollowing rules contain this device"
    for i in range(len(response['params']['ruleIds'])):
        print "Device ", deviceId, "found in rule", response['params']['ruleIds'][i]



# This function displays the appropriate menu and returns the option selected
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
 
# This function calls showmenu and then acts on the selected item
def processmenu(menu, parent=None):
    optioncount = len(menu['options'])
    exitmenu = False
    while not exitmenu: #Loop until the user exits the menu
	getin = runmenu(menu, parent)
	if getin == optioncount:
	    exitmenu = True
	elif menu['options'][getin]['type'] == COMMAND:
	    screen.clear()
	    ###############################
	    curses.endwin()
	    os.system('clear')
	    methodCall = globals()[menu['options'][getin]['command']]
	    methodCall()
	    raw_input("\nPress any key...")
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
guh.init()
processmenu(menu_data)
curses.endwin()