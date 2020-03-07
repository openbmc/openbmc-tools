#!/usr/bin/python3
"""
 Copyright 2017,2020 IBM Corporation

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
"""
import argparse
import requests
import json
import getpass
import re
import sys

jsonHeader = {'Content-Type': 'application/json'}
xAuthHeader = {}


def createCommandParser():
    """
         creates the parser for the command line along with help for each
         command and subcommand

         @return: returns the parser for the command line
    """
    parser = argparse.ArgumentParser(description='Process arguments')
    parser.add_argument("-H", "--host", help='A hostname or IP for the BMC')
    parser.add_argument("-U", "--user", help='The username to login with')
    group = parser.add_mutually_exclusive_group()
    group.add_argument("-A", "--askpw", action='store_true',
                       help='prompt for password')
    group.add_argument("-P", "--PW", help='Provide the password in-line')
    parser.add_argument('-j', '--json', action='store_true',
                        help='output json data only')
    parser.add_argument('-f', '--file',
                        help='The path and filename of where to save the API dump')
    return parser


def jsonDiver(theDict, startingEndpoint, compends):
    endpointdata = {}
    newEnumerations = []
    for key in theDict.keys():
        if isinstance(theDict[key], dict):
            info = jsonDiver(theDict[key], startingEndpoint+key, compends)
            newEnumerations = newEnumerations + info[1]
            endpointdata.update(info[0])
        elif 'Members' in key.split('/')[-1]:
            endpointdata[startingEndpoint+'/'+key] = theDict[key]
            if isinstance(theDict[key], list):
                for item in theDict[key]:
                    if isinstance(item, dict):
                        newEndpoint = item['@odata.id']
                        if newEndpoint not in compends:
                            newEnumerations.append(newEndpoint)
        else:
            endpointdata[startingEndpoint+'/'+key] = theDict[key]
            if '@odata.id' in key:
                if theDict[key] not in compends:
                    newEnumerations.append(theDict[key])
    return [endpointdata, newEnumerations]


def parseAPI(host, user, pw, endpoint, mysess, compends):
    response = mysess.get('https://{host}{endpoint}'.format(
        host=host, endpoint=endpoint),
        headers=jsonHeader,
        verify=False)
    compends.append(endpoint)
    endpoints = {endpoint: {}}
    jsonData = response.json()
    info = jsonDiver(jsonData, endpoint, compends)
    endpoints[endpoint] = info[0]
    for newEndpoint in info[1]:
        print(info[1])
        updatedData = parseAPI(host, user, pw, newEndpoint, mysess, compends)
        if updatedData is not None:
            endpoints.update(updatedData)
    return endpoints


def login(host, user, pw):
    mysess = requests.session()
    data = json.dumps({"UserName": user, "Password": pw})
    response = mysess.post(
        'https://{host}/redfish/v1/SessionService/Sessions'.format(host=host),
        headers=jsonHeader,
        data=data,
        verify=False
        )
    if response.status_code in [200, 201]:
        xAuthHeader['X-Auth-Token'] = response.headers['X-Auth-Token']
        jsonHeader.update(xAuthHeader)
        loginMessage = response.json()
        print('Logged in successfully')
        return mysess
    else:
        print('Login Failed, terminating')
        sys.exit()


def main(argv=None):
    parser = createCommandParser()
    args = parser.parse_args(argv)
    requests.packages.urllib3.disable_warnings(
        requests.packages.urllib3.exceptions.InsecureRequestWarning
        )

    if(hasattr(args, 'host') and hasattr(args, 'user')):
        if (args.askpw):
            pw = getpass.getpass()
        elif(args.PW is not None):
            pw = args.PW
        else:
            print("You must specify a password")
            sys.exit()
        mysess = login(args.host, args.user, pw)
        completedEndpoints = []
        apicontents = parseAPI(args.host, args.user, pw, '/redfish/v1',
                               mysess, completedEndpoints)
        if hasattr(args, 'file'):
            with open(args.file, 'w') as f:
                f.write(json.dumps(apicontents, sort_keys=True, indent=4,
                                   separators=(',', ': '),
                                   ensure_ascii=False))
        else:
            print(json.dumps(apicontents, sort_keys=True, indent=4,
                             separators=(',', ': '),
                             ensure_ascii=False))


if __name__ == '__main__':
    """
         main function when called from the command line

    """
    import sys

    isTTY = sys.stdout.isatty()
    assert sys.version_info >= (2, 7)
    main()
