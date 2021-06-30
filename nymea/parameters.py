# -*- coding: UTF-8 -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#                                                                         #
#  Copyright (C) 2015 - 2018 Simon Stuerz <simon.stuerz@guh.io>           #
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

import nymea

def read_params(paramTypes):
    params = []
    for paramType in paramTypes:
        print "\nThe ParamType looks like this:\n", nymea.print_json_format(paramType)
        if 'readOnly' in paramType and paramType['readOnly']:
            print "--------------------------------------------------------"
            print "\nThe param \"%s\" is not writable!)\n" %(paramType['displayName'])
            raw_input("\nPress \"enter\" to continue...\n")
            print "--------------------------------------------------------"
            param = {}
            param['paramTypeId'] = paramType['id']
            param['value'] = paramType.get('defaultValue', '')
        elif any("allowedValues" in item for item in paramType) and len(paramType['allowedValues']) != 0:
            # has to be a string (for sorting list)
            allowedValues = []
            for value in paramType['allowedValues']:
                allowedValues.append(str(value))
            selection = nymea.get_selection("Please select one of following allowed values:", allowedValues)
            if selection == None:
                return None
            paramValue = paramType['allowedValues'][selection]
            param = {}
            param['paramTypeId'] = paramType['id']
            param['value'] = paramValue
        else:
            # make bool selectable to make shore they are "true" or "false"
            if paramType['type'] == "Bool":
                boolTypes = ["true","false"]
                selectionString = "Please enter value for parameter \"%s\" (type: %s): " % (paramType['displayName'], paramType['type'])
                selection = nymea.get_selection(selectionString, boolTypes)
                if selection == None:
                    return None
                paramValue = boolTypes[selection]
            elif paramType['type'] == "Int":
                paramValue = int(raw_input("Please enter value for parameter \"%s\" (type: %s): " % (paramType['displayName'], paramType['type'])))
            elif paramType['type'] == "Double":
                paramValue = float(raw_input("Please enter value for parameter \"%s\" (type: %s): " % (paramType['displayName'], paramType['type'])))
            elif paramType['type'] == "Uint":
                paramValue = int(raw_input("Please enter value for parameter \"%s\" (type: %s): " % (paramType['displayName'], paramType['type'])))
            else:
                paramValue = raw_input("Please enter value for parameter \"%s\" (type: %s): " % (paramType['displayName'], paramType['type']))
            param = {}
            param['paramTypeId'] = paramType['id']
            param['value'] = paramValue
        params.append(param)
    return params


def edit_params(currentDeviceParams, paramTypes):
    params = []
    for paramType in paramTypes:
        print "\nThe ParamType looks like this:\n", nymea.print_json_format(paramType)
        if 'readOnly' in paramType and paramType['readOnly']:
            print "--------------------------------------------------------"
            print "\nThe param \"%s\" is not writable! (current = \"%s\")\n" %(paramType['displayName'], get_param_value(paramType['id'], currentDeviceParams))
            raw_input("\nPress \"enter\" to continue...\n")
            print "--------------------------------------------------------"
            continue
        param = {}
        if any("allowedValues" in item for item in paramType) and len(paramType['allowedValues']) != 0:
            title = "Please select one of following allowed values: (current = \"%s\")" % (get_param_value(paramType['id'], currentDeviceParams))
            selection = nymea.get_selection(title, paramType['allowedValues'])
            if selection == None:
                return None
            paramValue = paramType['allowedValues'][selection]
            param['paramTypeId'] = paramType['id']
            param['value'] = paramValue
            params.append(param)
        else:
            # make bool selectable to make shore they are "true" or "false"
            if paramType['type'] == "Bool":
                boolTypes = ["true","false"]
                selectionString = "Please enter value (currently: \"%s\") for parameter \"%s\" (type: %s): " % (get_param_value(paramType['id'], currentDeviceParams), paramType['displayName'], paramType['type'])
                selection = nymea.get_selection(selectionString, boolTypes)
                if selection == None:
                    return None
                paramValue = boolTypes[selection]
                param['paramTypeId'] = paramType['id']
                param['value'] = paramValue
            else:
                paramValue = raw_input("Please enter value (currently: \"%s\") for parameter \"%s\" (type: %s): " % (get_param_value(paramType['id'], currentDeviceParams), paramType['displayName'], paramType['type']))
                param['paramTypeId'] = paramType['id']
                param['value'] = paramValue
        params.append(param)
    return params

def getParamName(paramTypeId, paramTypes):
    for paramType in paramTypes:
        if paramType['id'] == paramTypeId:
            return paramType['displayName']

    return ""

def get_param_value(paramTypeId, params):
    for param in params:
        if param['paramTypeId'] == paramTypeId:
            return param['value']
    return None

def read_paramDescriptors(paramTypes):
    params = []
    for paramType in paramTypes:
        selectionTypes = ["yes","no"]
        selectionText = "-> Do you want to create a descriptor for the param \"%s\"?" %(paramType['displayName'])
        selection = nymea.get_selection(selectionText, selectionTypes)
        if selectionTypes[selection] == "no":
            continue
        operator = nymea.select_valueOperator(paramType['displayName'])
        if paramType['type'] == "Bool":
            boolTypes = ["true", "false"]
            selectionString = "Please enter value for parameter \"%s\" (type: %s): " % (paramType['displayName'], paramType['type'])
            selection = nymea.get_selection(selectionString, boolTypes)
            if selection == None:
                return None
            paramValue = boolTypes[selection]
        else:
                print "Please enter value for parameter \"%s\" (type: %s): " % (paramType['displayName'], paramType['type'])
                paramValue = raw_input("%s %s " % (paramType['displayName'], nymea.get_valueOperator_string(operator)))
        param = {}
        param['paramTypeId'] = paramType['id']
        param['value'] = paramValue
        param['operator'] = operator
        params.append(param)
    #print "got params:", nymea.print_json_format(params)
    return params
