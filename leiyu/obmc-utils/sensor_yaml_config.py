#!/usr/bin/env python3

import yaml
import argparse

from typing import NamedTuple


class RptSensor(NamedTuple):
    name: str
    entityId: int
    typeId: int
    evtType: int
    sensorId: int
    fru: int
    targetPath: str


entityIds = {
    'dimm': 32,
    'core': 208,
    'cpu': 3,
    'occ': 210,
    'gpu': 216,
    'gpu_mem': 217,
    'tpm': 3,
    'state/host0': 33,  # Different interfaces using different entity ID
    # and this requires extra fix.
    #    {'state/host0', 34},
    #    {'state/host0', 35},
    'turbo': 3,
    'fan': 29,
    'vdd_temp': 218,
    'power': 10,
    'voltage': 10,
    'current': 10,
    'temperature/pcie': 35,
    'temperature/ambient': 64,
    'control/volatile': 33,
}

extraIds = {
    'RebootPolicy': 33,
    'Progress': 34,
    'RebootAttempts': 34,
    'OperatingSystem.Status': 35
}


sampleDimmTemp = {
    'bExp': 0,
    'entityID': 32,
    'entityInstance': 2,
    'interfaces': {
        'xyz.openbmc_project.Sensor.Value': {
            'Value': {
                'Offsets': {
                    255: {
                        'type': 'int64_t'
                    }
                }
            }
        }
    },
    'multiplierM': 1,
    'mutability': 'Mutability::Write|Mutability::Read',
    'offsetB': -127,
    'path': '/xyz/openbmc_project/sensors/temperature/dimm0_temp',
    'rExp': 0,
    'readingType': 'readingData',
    'scale': -3,
    'sensorNamePattern': 'nameLeaf',
    'sensorReadingType': 1,
    'sensorType': 1,
    'serviceInterface': 'org.freedesktop.DBus.Properties',
    'unit': 'xyz.openbmc_project.Sensor.Value.Unit.DegreesC'
}
sampleCoreTemp = {
    'bExp': 0,
    'entityID': 208,
    'entityInstance': 2,
    'interfaces': {
        'xyz.openbmc_project.Sensor.Value': {
            'Value': {
                'Offsets': {
                    255: {
                        'type': 'int64_t'
                    }
                }
            }
        }
    },
    'multiplierM': 1,
    'mutability': 'Mutability::Write|Mutability::Read',
    'offsetB': -127,
    'path': '/xyz/openbmc_project/sensors/temperature/p0_core0_temp',
    'rExp': 0,
    'readingType': 'readingData',
    'scale': -3,
    'sensorNamePattern': 'nameLeaf',
    'sensorReadingType': 1,
    'sensorType': 1,
    'serviceInterface': 'org.freedesktop.DBus.Properties',
    'unit': 'xyz.openbmc_project.Sensor.Value.Unit.DegreesC'
}
sampleDcmiSensor = {
    "instance": 1,
    "dbus": "/xyz/openbmc_project/sensors/temperature/p0_core0_temp",
    "record_id": 91
}


def openYaml(f):
    return yaml.load(open(f))


def saveYaml(y, f, safe=True):
    if safe:
        noaliasDumper = yaml.dumper.SafeDumper
        noaliasDumper.ignore_aliases = lambda self, data: True
        yaml.dump(y, open(f, "w"), default_flow_style=False,
                  Dumper=noaliasDumper)
    else:
        yaml.dump(y, open(f, "w"))


def getEntityIdAndNamePattern(p, intfs, m):
    key = (p, intfs)
    match = m.get(key, None)
    if match is None:
        raise Exception('Unable to find sensor', key, 'from map')
    return (m[key]['entityID'], m[key]['sensorNamePattern'])


# Global entity instances
entityInstances = {}


def getEntityInstance(id):
    instanceId = entityInstances.get(id, 0)
    instanceId = instanceId + 1
    entityInstances[id] = instanceId
    print("EntityId:", id, "InstanceId:", instanceId)
    return instanceId


def loadRpt(rptFile):
    sensors = []
    with open(rptFile) as f:
        next(f)
        next(f)
        for line in f:
            fields = line.strip().split('|')
            fields = list(map(str.strip, fields))
            sensor = RptSensor(
                fields[0],
                int(fields[2], 16) if fields[2] else None,
                int(fields[3], 16) if fields[3] else None,
                int(fields[4], 16) if fields[4] else None,
                int(fields[5], 16) if fields[5] else None,
                int(fields[7], 16) if fields[7] else None,
                fields[9])
            # print(sensor)
            sensors.append(sensor)
    return sensors


def getDimmTempPath(p):
    # Convert path like: /sys-0/node-0/motherboard-0/dimmconn-0/dimm-0
    # to: /xyz/openbmc_project/sensors/temperature/dimm0_temp
    import re
    dimmconn = re.search(r'dimmconn-\d+', p).group()
    dimmId = re.search(r'\d+', dimmconn).group()
    return '/xyz/openbmc_project/sensors/temperature/dimm{}_temp'.format(dimmId)


def getCoreTempPath(p):
    # Convert path like: /sys-0/node-0/motherboard-0/proc_socket-0/module-0/p9_proc_s/eq0/ex0/core0
    # to: /xyz/openbmc_project/sensors/temperature/p0_core0_temp
    import re
    splitted = p.split('/')
    socket = re.search(r'\d+', splitted[4]).group()
    core = re.search(r'\d+', splitted[9]).group()
    return '/xyz/openbmc_project/sensors/temperature/p{}_core{}_temp'.format(socket, core)


def getDimmTempConfig(s):
    r = sampleDimmTemp.copy()
    r['entityInstance'] = getEntityInstance(r['entityID'])
    r['path'] = getDimmTempPath(s.targetPath)
    return r


def getCoreTempConfig(s):
    r = sampleCoreTemp.copy()
    r['entityInstance'] = getEntityInstance(r['entityID'])
    r['path'] = getCoreTempPath(s.targetPath)
    return r


def isCoreTemp(p):
    import re
    m = re.search(r'p\d+_core\d+_temp', p)
    return m is not None


def getDcmiSensor(i, sensor):
    import re
    path = sensor['path']
    name = path.split('/')[-1]
    m = re.findall(r'\d+', name)
    socket, core = int(m[0]), int(m[1])
    instance = socket * 24 + core + 1
    r = sampleDcmiSensor.copy()
    r['instance'] = instance
    r['dbus'] = path
    r['record_id'] = i
    return r


def saveJson(data, f):
    import json
    with open(f, 'w') as outfile:
        json.dump(data, outfile, indent=4)


def main():
    parser = argparse.ArgumentParser(
        description='Yaml tool for updating ipmi sensor yaml config')
    parser.add_argument('-i', '--input', required=True, dest='input',
                        help='The ipmi sensor yaml config')
    parser.add_argument('-o', '--output', required=True, dest='output',
                        help='The output yaml file')
    parser.add_argument('-m', '--map', dest='map', default='sensor_map.yaml',
                        help='The sample map yaml file')
    parser.add_argument('-r', '--rpt', dest='rpt',
                        help='The .rpt file generated by op-build')
    parser.add_argument('-f', '--fix', action='store_true',
                        help='Fix entities and sensorNamePattern')

    # -g expects output as yaml for mapping of entityID/sensorNamePattern
    # -d expects output as json config for dcmi sensors
    # Do not mess the output by enforcing only one argument is passed
    # TODO: -f and -r could be used together, and they are conflicted with -g or -d
    group = parser.add_mutually_exclusive_group()
    group.add_argument('-g', '--generate', action='store_true',
                       help='Generate maps for entityID and sensorNamePattern')
    group.add_argument('-d', '--dcmi', action='store_true',
                       help='Generate dcmi sensors json config')

    args = parser.parse_args()
    args = vars(args)

    if args['input'] is None or args['output'] is None:
        parser.print_help()
        exit(1)

    y = openYaml(args['input'])

    sensorIds = list(y.keys())
    if args['rpt']:
        rptSensors = loadRpt(args['rpt'])
        for s in rptSensors:
            if s.sensorId is not None and s.sensorId not in sensorIds:
                print("Sensor ID:", s.sensorId,
                      " not in yaml, path", s.targetPath)
        # TODO: add new items in yaml with missing sensors
        return

    if args['fix']:
        # Fix entities and sensorNamePattern
        m = openYaml(args['map'])

        for i in y:
            path = y[i]['path']
            intfs = tuple(sorted(list(y[i]['interfaces'].keys())))
            entityId, namePattern = getEntityIdAndNamePattern(path, intfs, m)
            y[i]['entityID'] = entityId
            y[i]['entityInstance'] = getEntityInstance(entityId)
            y[i]['sensorNamePattern'] = namePattern
            print(y[i]['path'], "id:", entityId,
                  "instance:", y[i]['entityInstance'])

    sensorIds = list(y.keys())
    if args['rpt']:
        rptSensors = loadRpt(args['rpt'])
        for s in rptSensors:
            if s.sensorId is not None and s.sensorId not in sensorIds:
                print("Sensor ID", s.sensorId, "not in yaml:",
                      s.name, ", path:", s.targetPath)
                if 'temp' in s.name.lower():
                    if 'dimm' in s.targetPath.lower():
                        y[s.sensorId] = getDimmTempConfig(s)
                        print('Added sensor id:', s.sensorId,
                              ', path:', y[s.sensorId]['path'])
                    if 'core' in s.targetPath.lower():
                        y[s.sensorId] = getCoreTempConfig(s)
                        print('Added sensor id:', s.sensorId,
                              ', path:', y[s.sensorId]['path'])

    if args['generate']:
        m = {}
        for i in y:
            path = y[i]['path']
            intfs = tuple(sorted(list(y[i]['interfaces'].keys())))
            entityId = y[i]['entityID']
            sensorNamePattern = y[i]['sensorNamePattern']
            m[(path, intfs)] = {'entityID': entityId,
                                'sensorNamePattern': sensorNamePattern}
        y = m

    if args['dcmi']:
        d = []
        for i in y:
            if isCoreTemp(y[i]['path']):
                s = getDcmiSensor(i, y[i])
                d.append(s)
                print(s)
        saveJson(d, args['output'])
        return

    safe = False if args['generate'] else True
    saveYaml(y, args['output'], safe)


if __name__ == "__main__":
    main()
