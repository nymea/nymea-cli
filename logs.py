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

import guh
import states
import devices
import actions
import events
import rules
import datetime
import curses
import sys
import socket
import threading

screen = None
allLines = []
up = -1
down = 1
screenHeight = 0
topLineNum = 0
highlightLineNum = 0
    
def log_window():
    print "Open log window..."
    global screenHeight
    global allLines
    global topLineNum
    global highlightLineNum
    global up
    global down
    
    # init
    screen = curses.initscr()
    curses.noecho()
    curses.cbreak()
    curses.start_color() 
    curses.init_pair(1,curses.COLOR_BLACK, curses.COLOR_GREEN) 
    screen.keypad(1)
    screen.clear()
    
    allLines = get_log_entry_lines()
    highlightLineNum = len(allLines)
    hilightColors = curses.color_pair(1) 
    normalColors = curses.A_NORMAL 
    screenHeight = curses.LINES - 2
    x = None
    moveUpDown(down)

    try:
	while x !=ord('\n'):
	    #draw screen
	    screen.erase()
	    screen.border(0)
	    curses.curs_set(1)   
	    curses.curs_set(0)
	    top = topLineNum
	    bottom = topLineNum + screenHeight
	    
	    for (index,line,) in enumerate(allLines[top:bottom]):
		linenum = topLineNum + index 
		# highlight current line            
		if index != highlightLineNum:
		    screen.addstr(index + 1, 2, line, normalColors)
		else:
		    screen.addstr(index + 1, 2, line, hilightColors)
	    # get user input
	    x = screen.getch()
	    if x == curses.KEY_UP:
                moveUpDown(up)
            elif x == curses.KEY_DOWN:
                moveUpDown(down)
            screen.refresh()
            
    finally:
	curses.endwin()
	print "Log window closed."
	

def moveUpDown(direction):
    global screenHeight
    global topLineNum
    global highlightLineNum
    global allLines
    global up
    global down
    
    nextLineNum = highlightLineNum + direction
 
    # paging
    if direction == up and highlightLineNum == 0 and topLineNum != 0:
        topLineNum += up
        return
    elif direction == down and nextLineNum == screenHeight and (topLineNum + screenHeight) != len(allLines):
        topLineNum += down
        return
    # scroll highlight line
    if direction == up and (topLineNum != 0 or highlightLineNum != 0):
        highlightLineNum = nextLineNum
    elif direction == down and (topLineNum + highlightLineNum + 1) != len(allLines) and highlightLineNum != screenHeight:
        highlightLineNum = nextLineNum

def create_notification_thread():
    HOST='localhost'
    PORT=1234
    if len(sys.argv) > 1:
	HOST = sys.argv[1]


def list_logEntries():
    params = {}
    response = guh.send_command("Logging.GetLogEntries", params)
    for i in range(len(response['params']['logEntries'])):
        line = get_log_entry_line(response['params']['logEntries'][i])
        print line


def get_log_entry_lines():
    params = {}
    lines = []
    response = guh.send_command("Logging.GetLogEntries", params)
    for i in range(len(response['params']['logEntries'])):
        line = get_log_entry_line(response['params']['logEntries'][i])
        lines.append(line)
    return lines

def get_log_entry_line(entry):
    stateTypeIdCache = {}
    actionTypeIdCache = {}
    eventTypeIdCache = {}
    deviceIdCache = {}
    ruleIdCache = {}
    if entry['loggingLevel'] == "LoggingLevelInfo":
        levelString = "(I)"
        error = "-"
    else:
        levelString = "(A)"
        error = entry['errorCode']
    if entry['source'] == "LoggingSourceSystem":
        deviceName = "Guh Server"
        sourceType = "System Event"
        symbolString = "->"
        sourceName = "Active changed"
        if entry['active'] == True:
            value = "active"
        else:
            value = "inactive"
    if entry['source'] == "LoggingSourceStates":
        typeId = entry['typeId']
        sourceType = "State Changed"
        symbolString = "->"
        if typeId in stateTypeIdCache:
	    sourceName = stateTypeIdCache[typeId]
        else:
            stateType = states.get_stateType(typeId)
            if stateType is not None:
                sourceName = stateType["name"]
                stateTypeIdCache[typeId] = sourceName
            else:
                sourceName = typeId
        value = entry['value']
        if entry['deviceId'] in deviceIdCache:
            deviceName = deviceIdCache[entry['deviceId']]
        else:
            device = devices.get_device(entry['deviceId'])
            if device is not None:
                deviceName = device['name']
            else:
                deviceName = entry['deviceId']
            deviceIdCache[entry['deviceId']] = deviceName
    if entry['source'] == "LoggingSourceActions":
        typeId = entry['typeId']
        sourceType = "Action executed"
        symbolString = "()"
        if typeId in actionTypeIdCache:
            sourceName = actionTypeIdCache[typeId]
        else:
            actionType = actions.get_actionType(typeId)
            if actionType is not None:
                sourceName = actionType['name']
            else:
                sourceName = typeId
            actionTypeIdCache[typeId] = sourceName
        value = entry['value']
        if entry['deviceId'] in deviceIdCache:
            deviceName = deviceIdCache[entry['deviceId']]
        else:
            device = devices.get_device(entry['deviceId'])
            if device is not None:
                deviceName = device['name']
            else:
                deviceName = entry['deviceId']
            deviceIdCache[entry['deviceId']] = deviceName
    if entry['source'] == "LoggingSourceEvents":
        typeId = entry['typeId']
        sourceType = "Event triggered"
        symbolString = "()"
        if typeId in eventTypeIdCache:
            sourceName = eventTypeIdCache[typeId]
        else:
            eventType = events.get_eventType(typeId)
            sourceName = eventType['name']
            eventTypeIdCache[typeId] = sourceName
        value = entry['value']
        if entry['deviceId'] in deviceIdCache:
            deviceName = deviceIdCache[entry['deviceId']]
        else:
            device = devices.get_device(entry['deviceId'])
            if device is not None:
                deviceName = device['name']
            else:
                deviceName = entry['deviceId']
            deviceIdCache[entry['deviceId']] = deviceName
    if entry['source'] == "LoggingSourceRules":
        typeId = entry['typeId']
        if entry['eventType'] == "LoggingEventTypeTrigger":
            sourceType = "Rule triggered"
            sourceName = "triggered"
            symbolString = "()"
            value = ""
        else:
            sourceType = "Rule active changed"
            symbolString = "()"
            sourceName = "active"
            if entry['active']:
                value = "active"
            else:
                value = "inactive"
        if typeId in ruleIdCache:
            deviceName = ruleIdCache[typeId]
        else:
            rule = rules.get_rule_detail(typeId)
            if rule is not None and 'name' in rule:
                deviceName = rule['name']
            else:
                deviceName = typeId
            ruleIdCache[typeId] = deviceName
    timestamp = datetime.datetime.fromtimestamp(entry['timestamp']/1000)
    line = "%s %s : %20s : %38s : %30s %5s %20s : %20s" %(levelString, timestamp, sourceType, deviceName, sourceName, symbolString, value, error)
    return line
    
