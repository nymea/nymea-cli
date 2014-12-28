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
