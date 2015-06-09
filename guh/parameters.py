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

import guh

def read_params(paramTypes):
    params = []
    for paramType in paramTypes:
        print guh.print_json_format(paramType)
        if any("allowedValues" in item for item in paramType):
            # has to be a string (for sorting list)
            allowedValues = []
            for value in paramType['allowedValues']:
                allowedValues.append(str(value))
            selection = guh.get_selection("Please select one of following allowed values:", allowedValues)
            if selection == None:
                return None
            paramValue = paramType['allowedValues'][selection]
            param = {}
            param['name'] = paramType['name']
            param['value'] = paramValue
        else:
            # make bool selectable to make shore they are "true" or "false"
            if paramType['type'] == "bool":
                boolTypes = ["true","false"]
                selectionString = "Please enter value for parameter \"%s\" (type: %s): " % (paramType['name'], paramType['type'])
                selection = guh.get_selection(selectionString, boolTypes)
                if selection == None:
                    return None
                paramValue = boolTypes[selection] 
            else:
                paramValue = raw_input("Please enter value for parameter \"%s\" (type: %s): " % (paramType['name'], paramType['type']))
            param = {}
            param['name'] = paramType['name']
            param['value'] = paramValue
        params.append(param)
    return params


def edit_params(currentDeviceParams, paramTypes):
    params = []
    for paramType in paramTypes:
        print guh.print_json_format(paramType)
        if 'readOnly' in paramType:
            if paramType['readOnly'] == True:
                print "--------------------------------------------------------"
                print "\nThe param \"%s\" is not writable! (current = \"%s\")\n" %(paramType['name'], get_param_value(paramType['name'], currentDeviceParams))
                raw_input("\nPress \"enter\" to continue...\n")
                print "--------------------------------------------------------"
                continue
        param = {}
        if any("allowedValues" in item for item in paramType):
            title = "Please select one of following allowed values: (current = \"%s\")" % (get_param_value(paramType['name'], currentDeviceParams))
            selection = guh.get_selection(title, paramType['allowedValues'])
            if selection == None:
                return None
            paramValue = paramType['allowedValues'][selection]
            param['name'] = paramType['name']
            param['value'] = paramValue
            params.append(param)
        else:
            # make bool selectable to make shore they are "true" or "false"
            if paramType['type'] == "bool":
                boolTypes = ["true","false"]
                selectionString = "Please enter value (currently: \"%s\") for parameter \"%s\" (type: %s): " % (get_param_value(paramType['name'], currentDeviceParams), paramType['name'], paramType['type'])
                selection = guh.get_selection(selectionString, boolTypes)
                if selection == None:
                    return None
                paramValue = boolTypes[selection]
                param['name'] = paramType['name']
                param['value'] = paramValue
            else:
                paramValue = raw_input("Please enter value (currently: \"%s\") for parameter \"%s\" (type: %s): " % (get_param_value(paramType['name'], currentDeviceParams), paramType['name'], paramType['type']))
                param['name'] = paramType['name']
                param['value'] = paramValue
        params.append(param)
    return params


def get_param_value(name, params):
    for param in params:
        if param['name'] == name:
            return param['value']
    return None

def read_paramDescriptors(paramTypes):
    params = []
    for paramType in paramTypes:
        selectionTypes = ["yes","no"]
        selectionText = "-> Do you want to create a descriptor for the param \"%s\"?" %(paramType['name'])
        selection = guh.get_selection(selectionText, selectionTypes)
        if selectionTypes[selection] == "no":
            continue
        operator = guh.select_valueOperator(paramType['name'])
        if paramType['type'] == "bool":
            boolTypes = ["true","false"]
            selectionString = "Please enter value for parameter \"%s\" (type: %s): " % (paramType['name'], paramType['type'])
            selection = guh.get_selection(selectionString, boolTypes)
            if selection == None:
                return None
            paramValue = boolTypes[selection] 
        else:
                print "Please enter value for parameter \"%s\" (type: %s): " % (paramType['name'], paramType['type'])
                paramValue = raw_input("%s %s " % (paramType['name'], guh.get_valueOperator_string(operator)))
        param = {}
        param['name'] = paramType['name']
        param['value'] = paramValue
        param['operator'] = operator
        params.append(param)
    print "got params:", guh.print_json_format(params)
    return params
