#!/usr/bin/env python

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

from distutils.core import setup

setup(	name = "guh-cli",
	author = "Simon Stuerz",
	author_email = "simon.stuerz@guh.guru",
	description = "guh command line interface - python",
	long_description = """\
	The guh-cli (command line interface) is an admin tool written in python to communicate 
	with the guh JSON-RPC API and test functionality of guh.
	""",
	url = "https://github.com/guh/guh-cli.git",
	version = "1.0.0",
	py_modules = [
		"guh.actions",
		"guh.devices",
		"guh.events",
		"guh.guh",
		"guh.logs",
		"guh.notifications",
		"guh.parameters",
		"guh.ruleactions",
		"guh.rules",
		"guh.selector",
		"guh.states"
	],
	scripts = ["guh-cli"],
	data_files = [ 
		("/usr/share/man/man1", ["debian/guh-cli.1"]),
		("/usr/share/doc/guh-cli/", ["debian/changelog"]) 
	]
)
