// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "main.hpp"
// This is the form a sensor assumes on DBus.
// Aggregates their view from all other daemons.
#include <bitset>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
// Where is this sensor seen?
constexpr int VISIBILITY_OBJECT_MAPPER = 0;
constexpr int VISIBILITY_HWMON = 1;
constexpr int VISIBILITY_IPMITOOL_SDR = 2;
class DBusConnection
{
  public:
    std::string service;    // example: "systemd-resolved"
    std::string connection; // example: ":1.1"
    std::string cmd;        // the comm line
    std::string unit;       // example: "systemd-resolved.service"
    int pid;
    // For actual DBus capture: service name, connection name, command line,
    //                          systmed unit and PID are all known
    DBusConnection(const std::string& _s, const std::string& _c,
                   const std::string& _cmd, const std::string& _u, int _pid)
    {
        service = _s;
        connection = _c;
        cmd = _cmd;
        unit = _u;
        pid = _pid;
    }

    // During PCap replay: only service name, connection name, and PID are known
    //                     cmd and unit are not known since they are not
    //                     stored in the PCap file
    DBusConnection(const std::string& _s, const std::string& _c, int _pid)
    {
        service = _s;
        connection = _c;
        pid = _pid;
    }
};

class DBusConnectionSnapshot
{
  public:
    std::vector<DBusConnection*> connections_;
    std::unordered_map<std::string, DBusConnection*> unique_name_to_cxn;
    DBusConnection* FindDBusConnectionByService(const std::string& service)
    {
        for (DBusConnection* cxn : connections_)
        {
            if (cxn->service == service)
                return cxn;
        }
        return nullptr;
    }

    void SetConnectionPID(const std::string& connection, int pid)
    {
        DBusConnection* cxn = FindDBusConnectionByService(connection);
        if (cxn != nullptr)
        {
            cxn->pid = pid;
            unique_name_to_cxn[connection] = cxn; // Just to make sure
        }
    }

    void SetConnectionUniqueName(const std::string& service,
                                 const std::string& unique_name)
    {
        DBusConnection* cxn = FindDBusConnectionByService(service);
        if (cxn != nullptr)
        {
            cxn->connection = unique_name;
            unique_name_to_cxn[unique_name] = cxn;
        }
    }

    DBusConnection* FindDBusConnectionByConnection(const std::string& conn)
    {
        for (DBusConnection* cxn : connections_)
        {
            if (cxn->connection == conn)
                return cxn;
        }
        return nullptr;
    }


    // Only when service is known (during playback)
    void AddConnection(const std::string& _s)
    {
        connections_.push_back(new DBusConnection(_s, "", "", "", INVALID));
    }

    // When all 5 pieces of details are known (during actual capture)
    void AddConnection(const std::string& _s, const std::string& _connection,
                       const std::string& _cmd, const std::string& _unit,
                       int _pid)
    {
        DBusConnection* cxn =
            new DBusConnection(_s, _connection, _cmd, _unit, _pid);
        connections_.push_back(cxn);
        unique_name_to_cxn[_connection] = cxn;
    }

    int GetConnectionPIDFromNameOrUniqueName(const std::string& key)
    {
        if (unique_name_to_cxn.find(key) == unique_name_to_cxn.end())
        {
            return INVALID;
        }
        else
        {
            return unique_name_to_cxn[key]->pid;
        }
    }

    std::string GetConnectionCMDFromNameOrUniqueName(const std::string& key)
    {
        if (unique_name_to_cxn.find(key) == unique_name_to_cxn.end())
        {
            return "(unknown)";
        }
        else
        {
            return unique_name_to_cxn[key]->cmd;
        }
    }

    std::string GetUniqueNameIfExists(const std::string service)
    {
        for (DBusConnection* cxn : connections_)
        {
            if (cxn->service == service)
                return cxn->connection;
        }
        return service;
    }

};

// Each sensor might have different units, for example current and voltage
class Sensor
{
  public:
    DBusConnection* connection_;
    // Example: "/xyz/openbmc_project/sensors/temperature/powerseq_temp"
    std::string object_path_;
    std::string SensorID()
    {
        const size_t idx = object_path_.rfind('/');
        if (idx != std::string::npos)
        {
            return object_path_.substr(idx + 1);
        }
        else
            return ("unknown sensor");
    }

    std::string ServiceName()
    {
        if (connection_ == nullptr)
            return "";
        else
            return connection_->service;
    }

    std::string ConnectionName()
    {
        if (connection_ == nullptr)
            return "";
        else
            return connection_->connection;
    }

    std::string ObjectPath()
    {
        return object_path_;
    }

    // Should contain the following:
    // 1. "org.freedesktop.DBus.Introspectable"
    // 2. "org.freedesktop.DBus.Peer"
    // 3. "org.freedesktop.DBus.Properties"
    // 4. "xyz.openbmc_project.Sensor.Value"
    // 5. "xyz.openbmc_project.State.Decorator.OperationalStatus"
    std::set<std::string> interfaces_;
    std::bitset<4> visibility_flags_;
    std::set<std::string> associations_;
};

class SensorSnapshot
{
  public:
    std::vector<std::string> GetDistinctSensorNames()
    {
        std::unordered_set<std::string> seen;
        std::vector<std::string> ret;
        for (Sensor* s : sensors_)
        {
            std::string sn = s->SensorID();
            if (seen.find(sn) == seen.end())
            {
                ret.push_back(sn);
                seen.insert(sn);
            }
        }
        return ret;
    }

    explicit SensorSnapshot(DBusConnectionSnapshot* cs)
    {
        connection_snapshot_ = cs;
    }

    ~SensorSnapshot()
    {
        for (Sensor* s : sensors_)
        {
            delete s;
        }
    }

    int SensorCount()
    {
        return int(sensors_.size());
    }

    Sensor* FindOrCreateSensorByServiceAndObject(const std::string& service,
                                                 const std::string& object)
    {
        Sensor* ret = nullptr;
        for (Sensor* s : sensors_)
        {
            if (s->ServiceName() == service && s->object_path_ == object)
            {
                ret = s;
                break;
            }
        }
        if (ret == nullptr)
        {
            DBusConnection* cxn =
                connection_snapshot_->FindDBusConnectionByService(service);
            ret = new Sensor();
            ret->connection_ = cxn;
            ret->object_path_ = object;
            sensors_.push_back(ret);
        }
        return ret;
    }

    // Note: one sensor_id might correspond to multiple sensors.
    // Example: "VDD" can have all 3 of power, current and voltage.
    std::vector<Sensor*> FindSensorsBySensorID(const std::string& sensor_id)
    {
        std::vector<Sensor*> ret;
        for (Sensor* s : sensors_)
        {
            const std::string& p = s->object_path_;
            if (p.find(sensor_id) == p.size() - sensor_id.size())
            {
                ret.push_back(s);
            }
        }
        return ret;
    }

    // This sensor is visible from Object Mapper
    void SerSensorVisibleFromObjectMapper(const std::string& service,
                                          const std::string& object)
    {
        Sensor* s = FindOrCreateSensorByServiceAndObject(service, object);
        s->visibility_flags_.set(VISIBILITY_OBJECT_MAPPER);
    }
    
    // This sensor is visible from Hwmon
    void SetSensorVisibleFromHwmon(const std::string& service,
                                   const std::string& object)
    {
        Sensor* s = FindOrCreateSensorByServiceAndObject(service, object);
        s->visibility_flags_.set(VISIBILITY_HWMON);
    }

    // This sensor is visible from `ipmitool sdr`
    // The first column is referred to as "sensorid".
    void SetSensorVisibleFromIpmitoolSdr(const std::string& sensor_id)
    {
        std::vector<Sensor*> sensors = FindSensorsBySensorID(sensor_id);
        for (Sensor* s : sensors)
            s->visibility_flags_.set(VISIBILITY_IPMITOOL_SDR);
    }

    void PrintSummary()
    {
        for (Sensor* s : sensors_)
        {
            printf("%50s %50s %9s\n", s->ServiceName().c_str(),
                   s->object_path_.c_str(),
                   s->visibility_flags_.to_string().c_str());
        }
    }

    Sensor* FindSensorByDBusUniqueNameOrServiceName(const std::string& key)
    {
        for (Sensor* s : sensors_)
        {
            if (s->ConnectionName() == key || s->ServiceName() == key)
                return s;
        }
        return nullptr;
    }

  private:
    std::vector<Sensor*> sensors_;
    std::unordered_map<std::string, int> conn2pid_;
    DBusConnectionSnapshot* connection_snapshot_;
};

bool IsSensorObjectPath(const std::string& s);
bool IsUniqueName(const std::string& x);
std::string Trim(const std::string& s);
