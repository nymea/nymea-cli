#!/usr/bin/env python

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

from nymea import __version__
from distutils.core import setup

setup(name = "nymea-cli",
    author = "Simon Stuerz",
    author_email = "simon.stuerz@guh.io",
    description = "nymea command line interface - python",
    long_description = """\
    The nymea-cli (command line interface) is an admin tool written in python to communicate 
    with the nymea JSON-RPC API and test functionality of nymea.
    """,
    url = "https://github.com/guh/nymea-cli",
    version = __version__,
    keywords = ["nymea", "cli", "command line interface"],
    scripts = ["nymea-cli"],
    py_modules = [
        "nymea.actions",
        "nymea.devices",
        "nymea.events",
        "nymea.nymea",
        "nymea.logs",
        "nymea.mainmenu",
        "nymea.notifications",
        "nymea.parameters",
        "nymea.plugins",
        "nymea.ruleactions",
        "nymea.rules",
        "nymea.selector",
        "nymea.states",
        "nymea.timedescriptor",
        "nymea.settings",
        "nymea.zigbeemanager",
        "tests.licensetests"
    ],
    data_files = [
        ("/usr/share/man/man1", ["debian/nymea-cli.1"]),
        ("/usr/share/doc/nymea-cli/", ["debian/changelog"])
    ],
    classifiers = [
        "Programming Language :: Python",
        "Programming Language :: Python :: 2",
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: GNU General Public License version 2 (GPLv2)",
        "Operating System :: OS Independent",
        "Topic :: Software Development :: Libraries :: Python Modules",
        "Topic :: Testing :: Home Automation"
    ]
)
