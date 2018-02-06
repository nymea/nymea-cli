# -*- coding: UTF-8 -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
#                                                                         #
#  Copyright (C) 2016-2018 Simon Stuerz <simon.stuerz@guh.io>             #
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
import selector
import time
import datetime

def createTimeDescriptor():
    print "\n========================================================"
    print "Create time descriptor\n"
    
    timeDescriptor = {}
    enough = False
    
    options = ["Create calendar items", "Create time event items"]    
    selection = guh.get_selection("Which kind of time items do you want to create?", options)
    
    if options[selection] == "Create calendar items":
        calendarItems = []
        while not enough:
            calendarItems.append(createCalendarItem())
            input = raw_input("Do you want to add another calendar item? (y/N): ")
            if not input == "y":
                enough = True
        
        timeDescriptor['calendarItems'] = calendarItems
        return timeDescriptor
    else:
        timeEventItems = []
        while not enough:
            timeEventItems.append(createTimeEventItem())
            input = raw_input("Do you want to add another time event item? (y/N): ")
            if not input == "y":
                enough = True
        
        timeDescriptor['timeEventItems'] = timeEventItems
        return timeDescriptor
        

def createTimeEventItem():
    print "\n========================================================"
    print "Create time event item\n"
    timeEventItem = {}
    if selector.getYesNoSelection("Do you want a time event for a certain date and time?"):
        timeString = raw_input("Please enter unix time for this time event (\"DD.MM.YYYY hh:mm\"): ")
        timeEventItem['datetime'] = int(time.mktime(time.strptime(timeString, "%d.%m.%Y %H:%M")))
        if selector.getYesNoSelection("Do you want to define a repeating option for this time event item?"):
            timeEventItem['repeating'] = createRepeatingOption(True)
            
    else:
        timeEventItem['time'] = raw_input("Please enter the time for this time event (\"hh:mm\"): ")
        if selector.getYesNoSelection("Do you want to define a repeating option for this time event item?"):
            timeEventItem['repeating'] = createRepeatingOption()
            
    return timeEventItem
    
def createCalendarItem():
    print "\n========================================================"
    print "Create calendar item\n"
    calendarItem = {}
    if selector.getYesNoSelection("Do you want a calendar entry for a certain date and time?"):
        timeString = raw_input("Please enter unix time for this calendar item (\"DD.MM.YYYY hh:mm\"): ")
        calendarItem['datetime'] = int(time.mktime(time.strptime(timeString, "%d.%m.%Y %H:%M")))
        if selector.getYesNoSelection("Do you want to define a repeating option for this calendar item?"):
            calendarItem['repeating'] = createRepeatingOption(True)
    
    else:
        calendarItem['startTime'] = raw_input("Please enter the start time of this calendar item (\"hh:mm\"): ")
        if selector.getYesNoSelection("Do you want to define a repeating option for this calendar item?"):
            calendarItem['repeating'] = createRepeatingOption()
            
    print "\n========================================================"
    calendarItem['duration'] = int(raw_input("duration of the calendar item (\"minutes\") = "))
    print calendarItem
    return calendarItem
    

def createRepeatingOption(forDateTime = False):
    print "\n========================================================"
    print "Create repeating option\n"
    repeatingOption = {}

    if forDateTime:
        options = ["Repeat yearly"]
        selection = guh.get_selection("Please select the repeating mode:", options)
        repeatingOption['mode'] = "RepeatingModeYearly"
        print repeatingOption
        return repeatingOption
        
    options = ["0. Repeat hourly", 
               "1. Repeat daily", 
               "2. Repeat weekly", 
               "3. Repeat monthly", 
               "4. Repeat yearly"]
               
    selection = guh.get_selection("Please select the repeating mode:", options)
    if selection is 0:
        repeatingOption['mode'] = "RepeatingModeHourly"

    if selection is 1:
        repeatingOption['mode'] = "RepeatingModeDaily"
    
    if selection is 2:
        repeatingOption['mode'] = "RepeatingModeWeekly"
        weekDaysString = raw_input("Please enter the list of week days (space separated [1-7]): ")
        repeatingOption['weekDays'] = [int(weekDay) for weekDay in weekDaysString.split()]
        
    if selection is 3:
        repeatingOption['mode'] = "RepeatingModeMonthly"
        monthDaysString = raw_input("Please enter the list of month days (space separated [1-31]): ")
        repeatingOption['monthDays'] = [int(monthDay) for monthDay in monthDaysString.split()]
        
    print repeatingOption
    return repeatingOption

    
def printTimeDescriptor(timeDescriptor):
    
    if 'calendarItems' in timeDescriptor:
        printCalendarItems(timeDescriptor['calendarItems'])
    
    if 'timeEventItems' in timeDescriptor:
        printTimeEventItems(timeDescriptor['timeEventItems'])
        

def printCalendarItems(calendarItems):
    for i in range(len(calendarItems)):
        calendarItem = calendarItems[i]
        
        #############################################
        if 'datetime' in calendarItem and calendarItem['datetime'] != 0:
            timeStamp = int(calendarItem['datetime'])
            
            if 'repeating' in calendarItem:
                startTime = datetime.datetime.fromtimestamp(timeStamp).strftime("%d.%m %H:%M")
                endTime = datetime.datetime.fromtimestamp(timeStamp + int(calendarItem['duration'])*60).strftime("%d.%m %H:%M")
                print  "%5s. -> Every year from  %s \n %37s" % (i, startTime, endTime)
            else:
                startTime = datetime.datetime.fromtimestamp(timeStamp).strftime("%d.%m.%Y %H:%M")
                endTime = datetime.datetime.fromtimestamp(timeStamp + int(calendarItem['duration'])*60).strftime("%d.%m.%Y %H:%M")
                print  "%5s. -> From %s   \n %30s" % (i, startTime, endTime)
        
        #############################################
        elif 'startTime' in calendarItem:
            if 'repeating' in calendarItem:
                repeatingOption = calendarItem['repeating']
                # Hourly
                if repeatingOption['mode'] == "RepeatingModeHourly":
                    print  "%5s. -> Every hour at %s for %s minutes." % (i, calendarItem['startTime'], calendarItem['duration'])
                # Daily
                if repeatingOption['mode'] == "RepeatingModeDaily":
                    print  "%5s. -> Every day at %s for %s minutes." % (i, calendarItem['startTime'], calendarItem['duration'])
                # Weekly
                if repeatingOption['mode'] == "RepeatingModeWeekly":
                    print  "%5s. -> Every week at %s for %s minutes on following week days:" % (i, calendarItem['startTime'], calendarItem['duration'])
                    printWeekDays(repeatingOption)
                # Monthly
                if repeatingOption['mode'] == "RepeatingModeMonthly":
                    print  "%5s. -> Every month at %s for %s minutes on following month days:" % (i, calendarItem['startTime'], calendarItem['duration'])
                    print "%22s" % repeatingOption['monthDays']
                    
            else:
                print  "%5s. -> Every day at %s for %s minutes." % (i, calendarItem['startTime'], calendarItem['duration'])

        else:           
            print timeEventItem 

def printTimeEventItems(timeEventItems):    
    for i in range(len(timeEventItems)):
        timeEventItem = timeEventItems[i]
        
        #############################################
        if 'datetime' in timeEventItem and timeEventItem['datetime'] != 0:
            timeStamp = int(timeEventItem['datetime'])
            if 'repeating' in timeEventItem:
                eventTime = datetime.datetime.fromtimestamp(timeStamp).strftime("%d.%m %H:%M")
                print  "%5s. -> Every year at %s" % (i, eventTime)
            else:
                eventTime = datetime.datetime.fromtimestamp(timeStamp).strftime("%d.%m.%Y %H:%M")
                print  "%5s. -> Trigger at %s" % (i, eventTime)
        #############################################
        elif 'time' in timeEventItem:
            if 'repeating' in timeEventItem:
                repeatingOption = timeEventItem['repeating']
                # Hourly
                if repeatingOption['mode'] == "RepeatingModeHourly":
                    print  "%5s. -> Every hour at %s." % (i, timeEventItem['time'])
                # Daily
                if repeatingOption['mode'] == "RepeatingModeDaily":
                    print  "%5s. -> Every day at %s." % (i, timeEventItem['time'])
                # Weekly
                if repeatingOption['mode'] == "RepeatingModeWeekly":
                    print  "%5s. -> Every week at %s on following week days:" % (i, timeEventItem['time'])
                    printWeekDays(repeatingOption)
                # Monthly
                if repeatingOption['mode'] == "RepeatingModeMonthly":
                    print  "%5s. -> Every month at %s on following month days:" % (i, timeEventItem['time'])
                    print "%22s" % repeatingOption['monthDays']
                    
            else:
                print  "%5s. -> Every day at %s." % (i, timeEventItem['time'])

        else:
            print timeEventItem

    
    
def printWeekDays(repeatingOption):
    weekString = ""
    if 1 in repeatingOption['weekDays']:
        weekString += "Mo[#]   "
    else:
        weekString += "Mo[ ]   "
    
    if 2 in repeatingOption['weekDays']:
        weekString += "Tu[#]   "
    else:
        weekString += "Tu[ ]   "
    
    if 3 in repeatingOption['weekDays']:
        weekString += "We[#]   "
    else:
        weekString += "We[ ]   "
    
    if 4 in repeatingOption['weekDays']:
        weekString += "Th[#]   "
    else:
        weekString += "Th[ ]   "
    
    if 5 in repeatingOption['weekDays']:
        weekString += "Fr[#]   "
    else:
        weekString += "Fr[ ]   "
    
    if 6 in repeatingOption['weekDays']:
        weekString += "Sa[#]   "
    else:
        weekString += "Sa[ ]   "
            
    if 7 in repeatingOption['weekDays']:
        weekString += "Su[#]"
    else:
        weekString += "Su[ ]"
    
    print "           %s" % (weekString)
    
    
    
