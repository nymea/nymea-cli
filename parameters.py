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

def read_params(paramTypes):
    params = []
    for paramType in paramTypes:
        if any("allowedValues" in item for item in paramType):
	    selection = guh.get_selection("Please select one of following allowed values:", paramType['allowedValues'])
	    paramValue = paramType['allowedValues'][selection]
	    param = {}
	    param['name'] = paramType['name']
	    param['value'] = paramValue
	else:  
	    paramValue = raw_input("Please enter value for parameter %s (type: %s): " % (paramType['name'], paramType['type']))
	    param = {}
	    param['name'] = paramType['name']
	    param['value'] = paramValue
        params.append(param)
    return params


def read_paramDescriptors(paramTypes):
    params = []
    for paramType in paramTypes:
        operator = guh.select_valueOperator(paramType['name'])
        print "Please enter value for parameter \"%s\" (type: %s): " % (paramType['name'], paramType['type'])
        paramValue = raw_input("%s %s " % (paramType['name'], guh.get_valueOperator_string(operator)))
        param = {}
        param['name'] = paramType['name']
        param['value'] = paramValue
        param['operator'] = operator
        params.append(param)
    print "got params:", guh.print_json_format(params)
    return params
