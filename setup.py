#!/usr/bin/python
# -*- coding: UTF-8 -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
#                                                                         #
#  Copyright (C) 2015 Simon Stürz <simon.stuerz@guh.guru>                 #
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

try:	
	from ez_setup import use_setuptools
	use_setuptools()
	from setuptools import setup	
except ImportError:
	from distutils.core import setup

config = {
	'author': 'Simon Stuerz',
	'author_email':"simon.stuerz@guh.guru",
	'name': 'guh-cli',
	'description': 'guh command line interface - python',
	"long_description":"""\
	The guh-cli (command line interface) is an admin tool written in python to communicate 
	with the guh JSON-RPC API and test functionality of guh.
	""",
	'url': 'https://github.com/guh/guh-cli.git',
	'version': '1.0',
	'package_dir': {
		'guh': 'guh/guh',
	},
	'scripts': 'guh-cli',
	'classifiers': [
		"Development Status :: 5 - unstable",
		"Topic :: Utilities",
		"License :: GPLv2"
	],
	
	
}
setup(**config)

