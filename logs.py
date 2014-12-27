import datetime

import guh
import states
import devices
import actions
import events
import rules

def list_logEntries():
    params = {}
    response = guh.send_command("Logging.GetLogEntries", params)
    stateTypeIdCache = {}
    actionTypeIdCache = {}
    eventTypeIdCache = {}
    deviceIdCache = {}
    ruleIdCache = {}
    for i in range(len(response['params']['logEntries'])):
        entry = response['params']['logEntries'][i]
        if entry['loggingLevel'] == "LoggingLevelInfo":
            levelString = "(I)"
            error = ""
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
        sourceType = sourceType.ljust(20)
        deviceName = deviceName.ljust(38)
        sourceName = sourceName.ljust(38)
        value = value.ljust(30)
        print levelString, timestamp, ":", sourceType, ":", deviceName, ":", sourceName, symbolString, value, ":", error
