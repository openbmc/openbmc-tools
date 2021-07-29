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

#include "analyzer.hpp"

#include "histogram.hpp"
#include "sensorhelper.hpp"
#include "views.hpp"
#include "xmlparse.hpp"

#include <systemd/sd-bus.h>
#include <unistd.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>

extern SensorSnapshot* g_sensor_snapshot;
extern DBusConnectionSnapshot* g_connection_snapshot;
extern sd_bus* g_bus;
extern SensorDetailView* g_sensor_detail_view;

int g_update_interval_millises = 2000;
int GetSummaryIntervalInMillises()
{
    return g_update_interval_millises;
}

namespace dbus_top_analyzer
{
    DBusTopStatistics g_dbus_statistics;
    Histogram<float> g_mc_time_histogram;
    std::unordered_map<uint32_t, uint64_t> in_flight_methodcalls;
    std::atomic<bool> g_program_done = false;
    std::chrono::time_point<std::chrono::steady_clock> g_last_update;
    DBusTopStatisticsCallback g_callback;
    void SetDBusTopStatisticsCallback(DBusTopStatisticsCallback cb)
    {
        g_callback = cb;
    }

    int UserInputThread()
    {
        return 0;
    }

    std::string g_dbus_top_conn = " ";
    void SetDBusTopConnectionForMonitoring(const std::string& conn)
    {
        g_dbus_top_conn = conn;
    }

    // Performs one step of analysis
    void Process()
    {
        std::chrono::time_point<std::chrono::steady_clock> t =
            std::chrono::steady_clock::now();
        std::chrono::time_point<std::chrono::steady_clock> next_update =
            g_last_update + std::chrono::milliseconds(g_update_interval_millises);
        if (t >= next_update)
        {
            float seconds_since_last_sample =
                std::chrono::duration_cast<std::chrono::microseconds>(t -
                                                                    g_last_update)
                    .count() /
                1000000.0f;
            g_dbus_statistics.seconds_since_last_sample_ =
                seconds_since_last_sample;
            // Update snapshot
            if (g_callback)
            {
                g_callback(&g_dbus_statistics, &g_mc_time_histogram);
            }
            g_dbus_statistics.Reset();
            g_last_update = t;
        }
    }

    void Finish()
    {
        g_program_done = true;
    }

    std::vector<std::string> FindAllObjectPathsForService(
        const std::string& service,
        std::function<void(const std::string&, const std::vector<std::string>&)>
            on_interface_cb)
    {
        sd_bus_error err = SD_BUS_ERROR_NULL;
        sd_bus_message *m, *reply;
        std::vector<std::string> paths; // Current iteration
        std::vector<std::string>
            all_obj_paths; // All object paths under the supervision of ObjectMapper
        paths.push_back("/");
        // busctl call xyz.openbmc_project.ObjectMapper        /
        // org.freedesktop.DBus.Introspectable        Introspect
        while (!paths.empty())
        {
            // printf("%d paths to explore, total %d paths so far\n",
            // int(paths.size()), int(all_obj_paths.size()));
            std::vector<std::string> new_paths;
            for (const std::string& obj_path : paths)
            {
                all_obj_paths.push_back(obj_path);
                int r = sd_bus_message_new_method_call(
                    g_bus, &m, service.c_str(), obj_path.c_str(),
                    "org.freedesktop.DBus.Introspectable", "Introspect");
                if (r < 0)
                {
                    printf("Oh! Cannot create new method call. r=%d, strerror=%s\n",
                        r, strerror(-r));
                    continue;
                }
                r = sd_bus_call(g_bus, m, 0, &err, &reply);
                if (r < 0)
                {
                    printf("Could not execute method call, r=%d, strerror=%s\n", r,
                        strerror(-r));
                }
                const char* sig = sd_bus_message_get_signature(reply, 0);

                if (!strcmp(sig, "s"))
                {
                    const char* s;
                    int r = sd_bus_message_read(reply, "s", &s);
                    std::string s1(s);
                    if (r < 0)
                    {
                        printf("Could not read string payload, r=%d, strerror=%s\n",
                            r, strerror(-r));
                    }
                    else
                    {
                        XMLNode* t = ParseXML(s1);
                        std::vector<std::string> ch = t->GetChildNodeNames();
                        if (on_interface_cb != nullptr)
                        {
                            on_interface_cb(obj_path, t->GetInterfaceNames());
                        }
                        DeleteTree(t);
                        for (const std::string& cn : ch)
                        {
                            std::string ch_path = obj_path;
                            if (obj_path.back() == '/')
                            {}
                            else
                                ch_path.push_back('/');
                            ch_path += cn;
                            new_paths.push_back(ch_path);
                        }
                    }
                }
            }
            paths = new_paths;
        }
        return all_obj_paths;
    }

    void ListAllSensors()
    {
        g_connection_snapshot = new DBusConnectionSnapshot();
        printf("1. Getting names\n");
        char** names;
        int r = sd_bus_list_names(g_bus, &names, nullptr);

        std::vector<std::string> services;
        std::vector<int> pids;
        std::vector<std::string> comms;
        for (char** ptr = names; ptr && *ptr; ++ptr)
        {
            services.push_back(*ptr);
            free(*ptr);
        }
        free(names);
        printf("2. Getting creds of each name\n");
        for (int i = 0; i < static_cast<int>(services.size()); i++)
        {
            const std::string& service = services[i];
            sd_bus_creds* creds = nullptr;
            r = sd_bus_get_name_creds(g_bus, services[i].c_str(),
                                    SD_BUS_CREDS_AUGMENT | SD_BUS_CREDS_EUID |
                                        SD_BUS_CREDS_PID | SD_BUS_CREDS_COMM |
                                        SD_BUS_CREDS_UNIQUE_NAME |
                                        SD_BUS_CREDS_UNIT | SD_BUS_CREDS_SESSION |
                                        SD_BUS_CREDS_DESCRIPTION,
                                    &creds);
            // PID
            int pid = -999;
            if (r < 0)
            {
                printf("Oh! Cannot get creds for %s\n", services[i].c_str());
            }
            else
            {
                r = sd_bus_creds_get_pid(creds, &pid);
            }
            pids.push_back(pid);
            // comm
            std::string comm;
            if (pid != -999)
            {
                std::ifstream ifs("/proc/" + std::to_string(pid) + "/cmdline");
                std::string line;
                std::getline(ifs, line);
                for (char c : line)
                {
                    if (c < 32 || c >= 127)
                        c = ' ';
                    comm.push_back(c);
                }
            }
            comms.push_back(comm);
            // unique name, also known as "Connection"
            std::string connection;
            const char* u;
            r = sd_bus_creds_get_unique_name(creds, &u);
            if (r >= 0)
            {
                connection = u;
            }
            else
            {
                printf("Oh! Could not get unique name for %s\n", service.c_str());
            }
            std::string unit;
            r = sd_bus_creds_get_unit(creds, &u);
            if (r >= 0)
            {
                unit = u;
            }
            else
            {
                printf("Oh! Could not get unit name for %s\n", unit.c_str());
            }
            printf("AddConnection    %s    %s    %s    %s    %d\n", service.c_str(),
                connection.c_str(), comm.c_str(), unit.c_str(), pid);
            g_connection_snapshot->AddConnection(service, connection, comm, unit,
                                                pid);
        }
        printf("There are %d DBus names.\n", int(services.size()));
        for (int i = 0; i < int(services.size()); i++)
        {
            printf("    %d: %s [%s]\n", i, services[i].c_str(), comms[i].c_str());
        }
        g_sensor_snapshot = new SensorSnapshot(g_connection_snapshot);
        // busctl call xyz.openbmc_project.ObjectMapper /
        // org.freedesktop.DBus.Introspectable Introspect
        printf("3. See which sensors are visible from Object Mapper\n");
        printf("3.1. Introspect Object Mapper for object paths\n");
        std::vector<std::string> all_obj_paths = FindAllObjectPathsForService(
            "xyz.openbmc_project.ObjectMapper", nullptr);
        sd_bus_error err = SD_BUS_ERROR_NULL;
        sd_bus_message *m, *reply;
        printf("%d paths found while introspecting ObjectMapper.\n",
            int(all_obj_paths.size()));
        printf("3.2. Call ObjectMapper's GetObject method against the sensor "
            "object paths that represent sensors\n");
        for (const std::string& p : all_obj_paths)
        {
            if (IsSensorObjectPath(p))
            {
                err = SD_BUS_ERROR_NULL;
                r = sd_bus_message_new_method_call(
                    g_bus, &m, "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetObject");
                if (r < 0)
                {
                    printf("Cannot create new method call. r=%d, strerror=%s\n", r,
                        strerror(-r));
                    continue;
                }
                r = sd_bus_message_append_basic(m, 's', p.c_str());
                if (r < 0)
                {
                    printf("Could not append a string parameter to m\n");
                    continue;
                }

                // empty array
                r = sd_bus_message_open_container(m, 'a', "s");
                if (r < 0)
                {
                    printf("Could not open a container for m\n");
                    continue;
                }
                r = sd_bus_message_close_container(m);
                if (r < 0)
                {
                    printf("Could not close container for m\n");
                    continue;
                }
                r = sd_bus_call(g_bus, m, 0, &err, &reply);
                if (r < 0)
                {
                    printf("Error performing dbus method call\n");
                }
                const char* sig = sd_bus_message_get_signature(reply, 0);
                if (!strcmp(sig, "a{sas}"))
                {
                    r = sd_bus_message_enter_container(reply, 'a', "{sas}");
                    if (r < 0)
                    {
                        printf("Could not enter the level 0 array container\n");
                        continue;
                    }
                    while (true)
                    {
                        r = sd_bus_message_enter_container(
                            reply, SD_BUS_TYPE_DICT_ENTRY, "sas");
                        if (r < 0)
                        {
                            // printf("Could not enter the level 1 dict
                            // container\n");
                            goto DONE;
                        }
                        else if (r == 0)
                        {}
                        else
                        {
                            // The following 2 correspond to `interface_map` in
                            // phosphor-mapper
                            const char* interface_map_first;
                            r = sd_bus_message_read_basic(reply, 's',
                                                        &interface_map_first);
                            if (r < 0)
                            {
                                printf("Could not read interface_map_first\n");
                                goto DONE;
                            }
                            r = sd_bus_message_enter_container(reply, 'a', "s");
                            if (r < 0)
                            {
                                printf("Could not enter the level 2 array "
                                    "container\n");
                                goto DONE;
                            }
                            bool has_value_interface = false;
                            while (true)
                            {
                                const char* interface_map_second;
                                r = sd_bus_message_read_basic(
                                    reply, 's', &interface_map_second);
                                if (r < 0)
                                {
                                    printf("Could not read interface_map_second\n");
                                }
                                else if (r == 0)
                                    break;
                                else
                                {
                                    // printf("    %s\n", interface_map_second);
                                    if (!strcmp(interface_map_second,
                                                "xyz.openbmc_project.Sensor.Value"))
                                    {
                                        has_value_interface = true;
                                    }
                                }
                            }
                            if (has_value_interface)
                            {
                                g_sensor_snapshot->SerSensorVisibleFromObjectMapper(
                                    std::string(interface_map_first), p);
                            }
                            r = sd_bus_message_exit_container(reply);
                        }
                        r = sd_bus_message_exit_container(reply);
                    }
                    r = sd_bus_message_exit_container(reply);
                }
            DONE:
            {}
            }
        }
        printf("4. Check Hwmon's DBus objects\n");
        for (int i = 0; i < int(comms.size()); i++)
        {
            const std::string& comm = comms[i];
            const std::string& service = services[i];
            if (comm.find("phosphor-hwmon-readd") != std::string::npos &&
                !IsUniqueName(service))
            {
                // printf("Should introspect %s\n", service.c_str());
                std::vector<std::string> objpaths =
                    FindAllObjectPathsForService(service, nullptr);
                for (const std::string& op : objpaths)
                {
                    if (IsSensorObjectPath(op))
                    {
                        g_sensor_snapshot->SetSensorVisibleFromHwmon(service, op);
                    }
                }
            }
        }
        // Call `ipmitool sdr list` and see which sensors exist.
        printf("5. Checking ipmitool SDR List\n");
        std::string out;
        constexpr int MAX_BUFFER = 255;
        char buffer[MAX_BUFFER];
        FILE* stream = popen("ipmitool sdr list", "r");
        while (fgets(buffer, MAX_BUFFER, stream) != NULL)
        {
            out.append(buffer);
        }
        pclose(stream);
        std::stringstream ss(out);
        while (true)
        {
            std::string sensor_id, reading, status;
            std::getline(ss, sensor_id, '|');
            std::getline(ss, reading, '|');
            std::getline(ss, status);
            // printf("%s %s %s\n", sensor_id.c_str(), reading.c_str(),
            // status.c_str());
            if (sensor_id.size() > 0 && reading.size() > 0 && status.size() > 0)
            {
                g_sensor_snapshot->SetSensorVisibleFromIpmitoolSdr(Trim(sensor_id));
            }
            else
                break;
        }
        printf("=== Sensors snapshot summary: ===\n");
        g_sensor_snapshot->PrintSummary();
}

} // namespace dbus_top_analyzer
void DBusTopStatistics::OnNewDBusMessage(const char* sender,
                                         const char* destination,
                                         const char* interface,
                                         const char* path, const char* member,
                                         const char type)
{
    num_messages_++;
    std::vector<std::string> keys;
    for (DBusTopSortField field : fields_)
    {
        switch (field)
        {
            case kSender:
                keys.push_back(CheckAndFixNullString(sender));
                break;
            case kDestination:
                keys.push_back(CheckAndFixNullString(destination));
                break;
            case kInterface:
                keys.push_back(CheckAndFixNullString(interface));
                break;
            case kPath:
                keys.push_back(CheckAndFixNullString(path));
                break;
            case kMember:
                keys.push_back(CheckAndFixNullString(member));
                break;
            case kSenderPID:
                const int pid =
                    g_connection_snapshot->GetConnectionPIDFromNameOrUniqueName(
                        sender);
                if (pid != -999)
                {
                    keys.push_back(std::to_string(pid));
                }
                else
                {
                    keys.push_back("(unknown)");
                }
                break;
        }
    }
    stats_[keys]++;
    // For meaning of type see here
    // https://dbus.freedesktop.org/doc/api/html/group__DBusProtocol.html#ga4a9012edd7f22342f845e98150aeb858
    switch (type)
    {
        case 1:
            num_mc_++;
            break;
        case 2:
            num_mr_++;
            break;
        case 3:
            num_error_++;
            break;
        case 4:
            num_sig_++;
            break;
    }
}
