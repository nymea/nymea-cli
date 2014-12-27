import guh

def get_stateType(stateTypeId):
    params = {}
    params['stateTypeId'] = stateTypeId
    response = guh.send_command("States.GetStateType", params);
    if "stateType" in response['params']:
        return response['params']['stateType']
    return None