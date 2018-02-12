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
import states
import devices
import actions
import events
import rules

import sys
import curses
import locale


def get_selection(title, options):
    global screen
    global screenHeight
    global topLineNum
    global highlightLineNum
    global up
    global down
    
    #print options
    #guh.debug_stop()
    
    sortedOptions = list(options)
    sortedOptions.sort(key=lambda x: x.lower())
    
    # create menu data from options
    menuData = {}
    menuData['type'] = "menu" 
    menuData['title'] = title
    menuOptions = []
    for i in range(0,len(sortedOptions)):
        menuItem = {}
        menuItem['title'] = sortedOptions[i]
        menuItem['type'] = "option"
        for j in range(0,len(options)):
            if menuItem['title'] == options[j]:
                menuItem['id'] = j
                continue
        menuOptions.append(menuItem)
    menuItem = {}
    menuItem['title'] = "Cancel"
    menuItem['type'] = "exitmenu" 
    menuItem['id'] = None
    menuOptions.append(menuItem)
    menuData['options'] = menuOptions
    
    # create screen and get selection    
    screen = curses.initscr()
    try:
        curses.noecho()
        curses.cbreak() 
        curses.start_color() 
        screen.clear()
        screen.keypad(1)
        curses.init_pair(1, curses.COLOR_BLACK, curses.COLOR_GREEN)
        selection = process_selection_menu(menuData)
        curses.endwin()
        if selection != None:
            return int(selection)
        return None
    finally:
        curses.endwin()    

def process_selection_menu(menu):
    global screen

    i = runmenu(menu)
    if i != len(menu['options']) :
        curses.endwin()
        screen = curses.initscr()
        screen.clear()
        curses.reset_prog_mode()
        curses.curs_set(1)
        curses.curs_set(0)
        return menu['options'][i]['id']
    else:
        return None
    

def getYesNoSelection(question):
    responseTypes = ["yes","no"]
    selection = guh.get_selection(question, responseTypes)
    if responseTypes[selection] == "yes":
        return True
    elif responseTypes[selection] == "no":
        return False
    else:
        return None
  
  
def getBoolSelection(question):
    responseTypes = ["true","false"]
    selection = guh.get_selection(question, responseTypes)
    return responseTypes[selection]
                
        
def runmenu(menu):
    global screen
    global screenHeight
    global topLineNum
    global highlightLineNum
    global up
    global down
    
    up = -1
    down = 1
    screenHeight = 0
    topLineNum = 0
    highlightLineNum = 0
    oldHighlightLineNum=None 
    screenHeight = curses.LINES - 6 # - 1 border - 5 title lines
    
    x = None 
    # Loop until return key is pressed
    while x !=ord('\n'):
        # reset screen
        screen.clear()
        screen.border(0)
        
        # print title
        screen.addstr(2,2, menu['title'].encode('utf-8'), curses.A_STANDOUT)
        
        top = topLineNum
        bottom = topLineNum + screenHeight
        # print lines
        for (index,menuItem,) in enumerate(menu['options'][top:bottom]):
            linenum = topLineNum + index 
            # print normal            
            if index != highlightLineNum:
                screen.addstr(index + 5, 5, "     " + menuItem['title'].encode('utf-8'), curses.A_NORMAL )
                # bold Cancel
                if index == len(menu['options'])  - 1:
                    screen.addstr(index + 5, 5, "     " + menuItem['title'].encode('utf-8'), curses.A_BOLD)
            else:
                # print highlight current line
                screen.addstr(index + 5, 5, " ->  " + menuItem['title'].encode('utf-8'), curses.color_pair(1))
        
        screen.refresh()
        # get user input
        x = screen.getch()
        if x == curses.KEY_UP:
            moveUpDown(up, menu)
        elif x == curses.KEY_DOWN:
            moveUpDown(down, menu)
        screen.refresh()
    
    # return index of the selected item
    return topLineNum + highlightLineNum

def moveUpDown(direction, menu):
    global screenHeight
    global topLineNum
    global highlightLineNum
    global up
    global down
    
    nextLineNum = highlightLineNum + direction

    # paging
    if direction == up and highlightLineNum == 0 and topLineNum != 0:
        topLineNum += up
        return
    elif direction == down and nextLineNum == screenHeight and (topLineNum + screenHeight) != len(menu['options']):
        topLineNum += down
        return
    elif direction == up and highlightLineNum == 0 and topLineNum == 0:
        scrollToBottom(menu)
        return
    elif direction == down and nextLineNum == screenHeight and (topLineNum + screenHeight) == len(menu['options']):
        scrollToTop(menu)
        return
    elif direction == down and nextLineNum == len(menu['options']) and len(menu['options']) < screenHeight:
        scrollToTop(menu)
        return
    
    # scroll highlight line
    if direction == up and (topLineNum != 0 or highlightLineNum != 0):
        highlightLineNum = nextLineNum
    elif direction == down and (topLineNum + highlightLineNum + 1) != len(menu['options']) and highlightLineNum != screenHeight:
        highlightLineNum = nextLineNum


def scrollToBottom(menu):
    global screenHeight
    global topLineNum
    global highlightLineNum
   
    # scroll to the bottom
    if len(menu['options']) <= screenHeight:
        topLineNum = 0
        highlightLineNum = len(menu['options']) - 1 
    else:
        topLineNum = len(menu['options']) - screenHeight
        highlightLineNum = screenHeight - 1
    

def scrollToTop(menu):
    global screenHeight
    global topLineNum
    global highlightLineNum
    
    # scroll to the top
    topLineNum = 0
    highlightLineNum = 0



