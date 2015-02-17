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

import datetime
import curses
import sys
import socket
import asyncore    
import telnetlib
import json

import guh
import states
import devices
import actions
import events
import rules

class NotificationHandler(asyncore.dispatcher):

    def __init__(self, host, port):
	self.screen = None
	self.allLines = []
	self.up = -1
	self.down = 1
	self.screenHeight = 0
	self.topLineNum = 0
	self.highlightLineNum = 0
	self.commandId=0
        
        asyncore.dispatcher.__init__(self)
        self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
        self.connect( (host, port) )

    def handle_connect(self):
	self.enable_notification()
	self.create_log_window()
	
    def create_log_window(self):
	print " -> Notification socket connected..."
	print " -> Open log window...\n"
	# init
	self.screen = curses.initscr()
	curses.noecho()
	curses.cbreak()
	curses.start_color() 
	curses.init_pair(1,curses.COLOR_BLACK, curses.COLOR_GREEN) 
	self.screen.keypad(1)
	self.screen.clear()
	
	self.allLines = get_log_entry_lines()
	hilightColors = curses.color_pair(1) 
	normalColors = curses.A_NORMAL 
	self.screenHeight = curses.LINES - 2
	x = None

	# scroll to bottom
	if len(self.allLines) <= self.screenHeight:
	    self.topLineNum = 0
	    self.highlightLineNum = len(self.allLines) - 1 
	else:
	    self.topLineNum = len(self.allLines) - self.screenHeight
	    self.highlightLineNum = self.screenHeight - 1
	    
	try:
	    while x !=ord('\n'):
		#draw screen
		self.screen.erase()
		self.screen.border(0)
		curses.curs_set(1)   
		curses.curs_set(0)
		top = self.topLineNum
		bottom = self.topLineNum + self.screenHeight
		
		for (index,line,) in enumerate(self.allLines[top:bottom]):
		    linenum = self.topLineNum + index 
		    # highlight current line            
		    if index != self.highlightLineNum:
			self.screen.addstr(index + 1, 2, line, normalColors)
		    else:
			self.screen.addstr(index + 1, 2, line, hilightColors)
		
		# get user input
		x = self.screen.getch()
		if x == curses.KEY_UP:
		    self.moveUpDown(self.up)
		elif x == curses.KEY_DOWN:
		    self.moveUpDown(self.down)
		self.screen.refresh()    
	finally:
	    curses.endwin()
	    print " -> Log window closed."
	    self.close()
	    
    def moveUpDown(self,direction):
	nextLineNum = self.highlightLineNum + direction
	# paging
	if direction == self.up and self.highlightLineNum == 0 and self.topLineNum != 0:
	    self.topLineNum += self.up
	    return
	elif direction == self.down and nextLineNum == self.screenHeight and (self.topLineNum + self.screenHeight) != len(self.allLines):
	    self.topLineNum += self.down
	    return
	# scroll highlight line
	if direction == self.up and (self.topLineNum != 0 or self.highlightLineNum != 0):
	    self.highlightLineNum = nextLineNum
	elif direction == self.down and (self.topLineNum + self.highlightLineNum + 1) != len(self.allLines) and self.highlightLineNum != self.screenHeight:
	    self.highlightLineNum = nextLineNum

    def enable_notification(self):
	params = {}
	commandObj = {}
	commandObj['id'] = self.commandId
	commandObj['method'] = "JSONRPC.SetNotificationStatus"
	params['enabled'] = "true"
	commandObj['params'] = params
	command = json.dumps(commandObj) + '\n'
	self.commandId = self.commandId + 1
	self.send(command)

    def handle_close(self):
        curses.endwin()
        print " -> Log window closed."
        self.close()
        print " -> Notification socket closed."

    def handle_read(self):
        print self.recv(10000)
	curses.flash()
    def writable(self):
        return None

    def handle_write(self):
        sent = self.send()
    
def log_window():
    host='localhost'
    port=1234
    if len(sys.argv) > 1:
	host = sys.argv[1]
    client = NotificationHandler(host, port)
    asyncore.loop()

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

def get_log_line_from_notification(notificationMessage):
    print notificationMessage

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
    line = "%s %s | %15s | %38s | %30s %5s %10s | %20s" %(levelString, timestamp, sourceType, deviceName, sourceName, symbolString, value, error)
    return line
    
