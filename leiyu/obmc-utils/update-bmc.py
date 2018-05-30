#!/usr/bin/env python
"""
Usage: upload_and_update.py <--file tarball>
                            --bmc <bmc-ip>
                            [-v]

This scripts upload the tarball to bmc and ask bmc to flash it.
"""

import argparse
import json
import os
import subprocess
from subprocess import check_call, check_output, CalledProcessError
from time import sleep


class BmcUpdate:

    def __init__(self, bmc, tarball):
        self.bmc = bmc
        self.tarball = tarball

    def checkAndLogin(self):
        if self.checkAlive():
            print 'BMC is alive'
        else:
            print 'BMC is down, check it first'
            exit(1)

        while not self.login():
            print 'Login fails, retry...'
            sleep(5)

    def checkAlive(self):
        cmds = ['ping', '-c', '1', self.bmc]
        try:
            check_call(cmds, stdout=FNULL, stderr=FNULL)
        except CalledProcessError:
            return False
        else:
            return True

    def login(self):
        url = 'https://%s/login' % self.bmc
        cmds = ['curl', '-s', '-c', 'cjar', '-k', '-X', 'POST', '-H',
                'Content-Type: application/json', '-d',
                '{"data": [ "root", "0penBmc"]}', url]
        try:
            check_call(cmds, stdout=FNULL, stderr=FNULL)
        except CalledProcessError:
            return False
        else:
            return True

    def applyUpdate(self):
        url = 'https://%s/xyz/openbmc_project/software/%s/attr/RequestedActivation' % (
            self.bmc, self.version)
        cmds = ['curl', '-s', '-b', 'cjar', '-k', '-X', 'PUT', '-H',
                'Content-Type: application/json', '-d',
                '{"data": "xyz.openbmc_project.Software.Activation.RequestedActivations.Active"}', url]
        check_call(cmds, stdout=FNULL, stderr=FNULL)

    def getProgress(self):
        url = 'https://%s/xyz/openbmc_project/software/%s' % (
            self.bmc, self.version)
        cmds = ['curl', '-s', '-b', 'cjar', '-k', url]
        output = subprocess.check_output(cmds, stderr=FNULL)
        if FNULL is None:  # Do not print log when FNULL is devnull
            print output
        data = json.loads(output)['data']
        return (data['Activation'], data.get('Progress'))

    def reboot(self):
        url = 'https://%s/xyz/openbmc_project/state/bmc0/attr/RequestedBMCTransition' % self.bmc
        cmds = ['curl', '-s', '-b', 'cjar', '-k', '-X', 'PUT', '-H',
                'Content-Type: application/json', '-d',
                '{"data": "xyz.openbmc_project.State.BMC.Transition.Reboot"}', url]
        check_call(cmds, stdout=FNULL, stderr=FNULL)

    def waitForState(self, state):
        status, progress = self.getProgress()
        while state not in status:
            if 'Failed' in status or 'Invalid' in status:
                raise Exception(status)
            print 'Waiting for status: \'%s\', current: \'%s\', Progress: %s' % (state, status.split('\n', 1)[0], str(progress))
            sleep(5)
            status, progress = self.getProgress()

    def upload(self):
        print 'Uploading \'%s\' to \'%s\' ...' % (self.tarball, self.bmc)
        url = 'https://%s/upload/image' % self.bmc
        cmds = ['curl', '-s', '-b', 'cjar', '-k', '-X', 'POST', '-H',
                'Content-Type: application/octet-stream', '-T', self.tarball, url]
        output = subprocess.check_output(cmds, stderr=FNULL)
        if FNULL is None:  # Do not print log when FNULL is devnull
            print output
        ret = json.loads(output)
        if ret['message'] != '200 OK':
            raise Exception(ret)
        self.version = ret['data']

    def update(self):
        print 'Update...'
        print 'Apply image...'
        self.applyUpdate()
        self.waitForState('Active')

        print 'Reboot BMC...'
        self.reboot()
        sleep(30)
        while not self.checkAlive():
            sleep(5)


def main():
    parser = argparse.ArgumentParser(
        description='Upload tarball to remote TFTP server and update it on BMC')
    parser.add_argument('-f', '--file', required=True, dest='tarball',
                        help='The tarball to upload and update')
    parser.add_argument('-b', '--bmc', required=True, dest='bmc',
                        help='The BMC IP address')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Print verbose log')

    args = parser.parse_args()
    args = vars(args)

    if args['tarball'] is None or args['bmc'] is None:
        parser.print_help()
        exit(1)
    global FNULL
    if args['verbose']:
        FNULL = None  # Print log to stdout/stderr, for debug purpose
    else:
        FNULL = open(os.devnull, 'w')  # Redirect stdout/stderr to devnull

    bmcUpdate = BmcUpdate(args['bmc'], args['tarball'])

    bmcUpdate.checkAndLogin()
    bmcUpdate.upload()
    bmcUpdate.update()

    print 'Completed!'

if __name__ == "__main__":
    main()
