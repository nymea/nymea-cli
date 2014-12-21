import sys
import telnetlib
import json

import devices
import events
import actions
import rules

commandId=0

def init():
    HOST='localhost'
    PORT=1234
    if len(sys.argv) > 1:
	HOST = sys.argv[1]
    global tn
    tn = telnetlib.Telnet(HOST, PORT)
    packet = tn.read_until("\n}\n")

    packet = json.loads(packet)
    print "connected to", packet["server"], "\nserver version:", packet["version"], "\nprotocol version:", packet["protocol version"], "\n"


def send_command(method, params = None):
    global commandId
    global tn
    commandObj = {}
    commandObj['id'] = commandId
    commandObj['method'] = method
    if not params == None and len(params) > 0:
        commandObj['params'] = params

    command = json.dumps(commandObj) + '\n'
    commandId = commandId + 1
    tn.write(command)
    response = json.loads(tn.read_until("\n}\n"))
    if response['status'] != "success":
        print "JSON error happened: %s" % response
        return None
    
    return response


def get_selection(title, options):
    print "\n\n", title
    for i in range(0,len(options)):
        print "%5i: %s" % (i, options[i])
        
    selection = raw_input("Enter selection: ")
    if not selection:
	print "\n   -> error in selection"
	return None
    return int(selection)


def get_valueOperator_string(valueOperator):
    if valueOperator == "ValueOperatorEquals":
        return "="
    elif valueOperator == "ValueOperatorNotEquals":
        return "!="
    elif valueOperator == "ValueOperatorLess":
        return "<" 
    elif valueOperator == "ValueOperatorGreater":
        return ">" 
    elif valueOperator == "ValueOperatorLessOrEqual":
        return "<=" 
    elif valueOperator == "ValueOperatorGreaterOrEqual":
        return ">=" 
    else:
        return "<unknown value operator>"
    
    
def get_stateEvaluator_string(stateEvaluator):
    if stateEvaluator == "StateOperatorAnd":
        return "&"
    elif stateEvaluator == "StateOperatorOr":
        return "|"
    else:
        return "<unknown state evaluator>"


def read_params(paramTypes):
    params = []
    for paramType in paramTypes:
        if any("allowedValues" in item for item in paramType):
	    selection = get_selection("Please select one of following allowed values:", paramType['allowedValues'])
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
        paramValue = raw_input("Please enter value for parameter <%s> (type: %s): " % (paramType['name'], paramType['type']))
        operator = select_valueOperator()
        param = {}
        param['name'] = paramType['name']
        param['value'] = paramValue
        param['operator'] = operator
        params.append(param)
        
    print "got params:", params
    return params


def print_device_error_code(deviceError):
    if deviceError == "DeviceErrorNoError":
        print "\nSuccess! (", deviceError, ")"
    elif deviceError == "DeviceErrorPluginNotFound":
        print "\nERROR: the plugin could not be found. (", deviceError, ")"
    elif deviceError == "DeviceErrorDeviceNotFound":
        print "\nERROR: the device could not be found. (", deviceError, ")"
    elif deviceError == "DeviceErrorDeviceClassNotFound":
        print "\nERROR: the deviceClass could not be found. (", deviceError, ")"
    elif deviceError == "DeviceErrorActionTypeNotFound":
        print "\nERROR: the actionType could not be found. (", deviceError, ")"
    elif deviceError == "DeviceErrorStateTypeNotFound":
        print "\nERROR: the stateType could not be found. (", deviceError, ")"
    elif deviceError == "DeviceErrorEventTypeNotFound":
        print "\nERROR: the eventType could not be found. (", deviceError, ")"
    elif deviceError == "DeviceErrorDeviceDescriptorNotFound":
        print "\nERROR: the deviceDescriptor could not be found. (", deviceError, ")"
    elif deviceError == "DeviceErrorMissingParameter":
        print "\nERROR: some parameters are missing. (", deviceError, ")"
    elif deviceError == "DeviceErrorInvalidParameter":
        print "\nERROR: invalid parameter. (", deviceError, ")"
    elif deviceError == "DeviceErrorSetupFailed":
        print "\nERROR: setup failed. (", deviceError, ")"
    elif deviceError == "DeviceErrorDuplicateUuid":
        print "\nERROR: uuid allready exists. (", deviceError, ")"
    elif deviceError == "DeviceErrorCreationMethodNotSupported":
        print "\nERROR: the selected CreationMethod is not supported for this device. (", deviceError, ")"
    elif deviceError == "DeviceErrorSetupMethodNotSupported":
        print "\nERROR: the selected SetupMethod is not supported for this device. (", deviceError, ")"
    elif deviceError == "DeviceErrorHardwareNotAvailable":
        print "\nERROR: the hardware is not available. (", deviceError, ")"
    elif deviceError == "DeviceErrorHardwareFailure":
        print "\nERROR: hardware failure. Something went wrong with the hardware. (", deviceError, ")"
    elif deviceError == "DeviceErrorAsync":
        print "\nINFO: the response will need some time. (", deviceError, ")"
    elif deviceError == "DeviceErrorDeviceInUse":
        print "\nERROR: the device is currently in use. Try again later. (", deviceError, ")"
    elif deviceError == "DeviceErrorPairingTransactionIdNotFound":
        print "\nERROR: the pairingTransactionId could not be found. (", deviceError, ")"
    else:
        print "\nERROR: Unknown error code: ", deviceError,  "Please take a look at the newest API version."


def print_rule_error_code(ruleError):
    if ruleError == "RuleErrorNoError":
        print "\nSuccess! (", ruleError, ")"
    elif ruleError == "RuleErrorInvalidRuleId":
        print "\nERROR: the ruleId is not valid. (", ruleError, ")"
    elif ruleError == "RuleErrorRuleNotFound":
        print "\nERROR: the rule could not be found. (", ruleError, ")"
    elif ruleError == "RuleErrorDeviceNotFound":
        print "\nERROR: the device could not be found for this rule. (", ruleError, ")"
    elif ruleError == "RuleErrorEventTypeNotFound":
        print "\nERROR: the eventType could not be found for this rule. (", ruleError, ")"
    elif ruleError == "RuleErrorActionTypeNotFound":
        print "\nERROR: the actionType could not be found for this rule. (", ruleError, ")"
    elif ruleError == "RuleErrorInvalidParameter":
        print "\nERROR: invalid parameter in this rule. (", ruleError, ")"
    elif ruleError == "RuleErrorMissingParameter":
        print "\nERROR: missing parameter in this rule. (", ruleError, ")"
    else:
        print "\nERROR: Unknown error code: ", ruleError,  "Please take a look at the newest API version."


def select_valueOperator():
    valueOperators = ["ValueOperatorEquals", "ValueOperatorNotEquals", "ValueOperatorLess", "ValueOperatorGreater", "ValueOperatorLessOrEqual", "ValueOperatorGreaterOrEqual"]
    selection = get_selection("Please select an operator to compare this parameter: ", valueOperators)
    if selection != None:
        return valueOperators[selection]
    
    return None


def select_stateOperator():
    stateOperators = ["StateOperatorAnd", "StateOperatorOr"]
    selection = get_selection("Please select an operator to compare this state: ", stateOperators)
    if selection != None:
        return stateOperators[selection]
    
    return None

  
