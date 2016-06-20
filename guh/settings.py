# -*- coding: UTF-8 -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
#                                                                         #
#  Copyright (C) 2016 Simon Stuerz <simon.stuerz@guh.guru>                #
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

def cloud_authenticate():
    params = {}
    params['username'] = raw_input("\nEnter the \"username\" of your cloud account: ")
    params['password'] = raw_input("\nEnter the \"password\" of your cloud account: ")
    response = guh.send_command("Cloud.Authenticate", params)   
    guh.print_cloud_error_code(response['params']['cloudConnectionError'])
    
def print_cloud_status():
    params = {}
    response = guh.send_command("Cloud.GetConnectionStatus", params)
    guh.print_json_format(response['params'])

    
def enable_cloud_connection():
    params = {}
    response = guh.send_command("Cloud.Enable", params)
    guh.print_json_format(response['params'])

def disable_cloud_connection():
    params = {}
    response = guh.send_command("Cloud.Disable", params)
    guh.print_json_format(response['params'])

