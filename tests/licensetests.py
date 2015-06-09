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

import sys
import os
import subprocess
import unittest

class TestStringMethods(unittest.TestCase):
    def test_license(self):
        licenseResult = subprocess.Popen("licensecheck -r -c '\.(py)$' . | grep -v 'GPL (v2)'", shell=True, stdout=subprocess.PIPE).stdout.read()
        if licenseResult != "":
            print "Missing license GPL (v2) in following files:"
            print licenseResult
        self.assertEqual(licenseResult, "")

    def test_copyright(self):
        copyrightResult = subprocess.Popen("licensecheck -r -c '\.(py)$' . | grep 'No copyright'", shell=True, stdout=subprocess.PIPE).stdout.read()
        if copyrightResult != "":
            print "Missing copyright in following files:"
            print copyrightResult
        self.assertEqual(copyrightResult, "")


if __name__ == '__main__':
    unittest.main()