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
#include "main.hpp"
#include "sensorhelper.hpp"
#include "views.hpp"
#include "xmlparse.hpp"

#include <unistd.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <cassert>

int AcquireBus(sd_bus** ret);

extern SensorSnapshot* g_sensor_snapshot;
extern DBusConnectionSnapshot* g_connection_snapshot;
extern sd_bus* g_bus;
extern SensorDetailView* g_sensor_detail_view;

bool ParseI2CTraceLine(std::string line,
  int& pid, double& timestamp, I2CCmd& cmd, int& i2cid);

static std::unordered_map<uint64_t, uint64_t>
    in_flight_methodcalls; // serial => microseconds
uint64_t Microseconds()
{
    long us;  // usec
    time_t s; // Seconds
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    s = spec.tv_sec;
    us = round(spec.tv_nsec / 1000); // Convert nanoseconds to milliseconds
    if (us > 999999)
    {
        s++;
        us = 0;
    }
    return s * 1000000 + us;
}

int g_update_interval_millises = 2000;
int GetSummaryIntervalInMillises()
{
    return g_update_interval_millises;
}

bool DBusTopSortFieldIsNumeric(DBusTopSortField field)
{
    switch (field)
    {
        case kSender:
        case kDestination:
        case kInterface:
        case kPath:
        case kMember:
        case kSenderCMD:
            return false;
        case kSenderPID:
        case kMsgPerSec:
        case kAverageLatency:
        case kSenderI2CTxPerSec:
            return true;
    }
    return false;
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
        sd_bus* bus,
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
                    bus, &m, service.c_str(), obj_path.c_str(),
                    "org.freedesktop.DBus.Introspectable", "Introspect");
                if (r < 0)
                {
                    printf("Oh! Cannot create new method call. r=%d, strerror=%s\n",
                        r, strerror(-r));
                    continue;
                }
                r = sd_bus_call(bus, m, 0, &err, &reply);
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

    void ListAllSensors(sd_bus* bus,
                        DBusConnectionSnapshot** cxn_snapshot,
                        SensorSnapshot** sensor_snapshot)
    {
        // Create new snapshots
        (*cxn_snapshot) = new DBusConnectionSnapshot();
        (*sensor_snapshot) = new SensorSnapshot(*cxn_snapshot);

//        printf("1. Getting names\n");
        char** names;
        int r = sd_bus_list_names(bus, &names, nullptr);
        std::vector<std::string> services;
        std::vector<int> pids;
        std::vector<std::string> comms;
        for (char** ptr = names; ptr && *ptr; ++ptr)
        {
            services.push_back(*ptr);
            free(*ptr);
        }
        free(names);
//        printf("2. Getting creds of each name\n");
        for (int i = 0; i < static_cast<int>(services.size()); i++)
        {
            const std::string& service = services[i];
            sd_bus_creds* creds = nullptr;
            r = sd_bus_get_name_creds(bus, services[i].c_str(),
                                    SD_BUS_CREDS_AUGMENT | SD_BUS_CREDS_EUID |
                                        SD_BUS_CREDS_PID | SD_BUS_CREDS_COMM |
                                        SD_BUS_CREDS_UNIQUE_NAME |
                                        SD_BUS_CREDS_UNIT | SD_BUS_CREDS_SESSION |
                                        SD_BUS_CREDS_DESCRIPTION,
                                    &creds);
            // PID
            int pid = INVALID;
            if (r < 0)
            {
//                printf("Oh! Cannot get creds for %s\n", services[i].c_str());
            }
            else
            {
                r = sd_bus_creds_get_pid(creds, &pid);
            }
            pids.push_back(pid);
            // comm
            std::string comm;
            if (pid != INVALID)
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
//                printf("Oh! Could not get unique name for %s\n", service.c_str());
            }
            std::string unit;
            r = sd_bus_creds_get_unit(creds, &u);
            if (r >= 0)
            {
                unit = u;
            }
            else
            {
//                printf("Oh! Could not get unit name for %s\n", unit.c_str());
            }
//            printf("AddConnection    %s    %s    %s    %s    %d\n", service.c_str(),
//                connection.c_str(), comm.c_str(), unit.c_str(), pid);
            (*cxn_snapshot)->AddConnection(service, connection, comm, unit,
                                                pid);
        }
//        printf("There are %d DBus names.\n", int(services.size()));
//        for (int i = 0; i < int(services.size()); i++)
//        {
//            printf("    %d: %s [%s]\n", i, services[i].c_str(), comms[i].c_str());
//        }
        
        // busctl call xyz.openbmc_project.ObjectMapper /
        // org.freedesktop.DBus.Introspectable Introspect
//        printf("3. See which sensors are visible from Object Mapper\n");
//        printf("3.1. Introspect Object Mapper for object paths\n");
        std::vector<std::string> all_obj_paths = FindAllObjectPathsForService(
            bus,
            "xyz.openbmc_project.ObjectMapper", nullptr);
        sd_bus_error err = SD_BUS_ERROR_NULL;
        sd_bus_message *m, *reply;
//        printf("%d paths found while introspecting ObjectMapper.\n",
//            int(all_obj_paths.size()));
//        printf("3.2. Call ObjectMapper's GetObject method against the sensor "
//            "object paths that represent sensors\n");
        for (const std::string& p : all_obj_paths)
        {
            if (IsSensorObjectPath(p))
            {
                err = SD_BUS_ERROR_NULL;
                r = sd_bus_message_new_method_call(
                    bus, &m, "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetObject");
                if (r < 0)
                {
//                    printf("Cannot create new method call. r=%d, strerror=%s\n", r,
//                        strerror(-r));
                    continue;
                }
                r = sd_bus_message_append_basic(m, 's', p.c_str());
                if (r < 0)
                {
//                    printf("Could not append a string parameter to m\n");
                    continue;
                }
                // empty array
                r = sd_bus_message_open_container(m, 'a', "s");
                if (r < 0)
                {
//                    printf("Could not open a container for m\n");
                    continue;
                }
                r = sd_bus_message_close_container(m);
                if (r < 0)
                {
//                    printf("Could not close container for m\n");
                    continue;
                }
                r = sd_bus_call(bus, m, 0, &err, &reply);
                if (r < 0)
                {
//                    printf("Error performing dbus method call\n");
                }
                const char* sig = sd_bus_message_get_signature(reply, 0);
                if (!strcmp(sig, "a{sas}"))
                {
                    r = sd_bus_message_enter_container(reply, 'a', "{sas}");
                    if (r < 0)
                    {
//                        printf("Could not enter the level 0 array container\n");
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
//                                printf("Could not read interface_map_first\n");
                                goto DONE;
                            }
                            r = sd_bus_message_enter_container(reply, 'a', "s");
                            if (r < 0)
                            {
//                                printf("Could not enter the level 2 array "
//                                    "container\n");
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
//                                    printf("Could not read interface_map_second\n");
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
                                (*sensor_snapshot)->SerSensorVisibleFromObjectMapper(
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

        // 3.5: Find all objects under ObjectMapper that implement the xyz.openbmc_project.Association interface
        // busctl call xyz.openbmc_project.ObjectMapper
        //   /xyz/openbmc_project/object_mapper xyz.openbmc_project.ObjectMapper
        //   GetSubTreePaths sias /xyz/openbmc_project/inventory 0 1 xyz.openbmc_project.Association
        err = SD_BUS_ERROR_NULL;
        r = sd_bus_message_new_method_call(
            bus, &m, "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper",
            "GetSubTreePaths");
        if (r < 0) { assert(0); }
        r = sd_bus_message_append_basic(m, 's', "/"); if (r < 0) { assert(0); }
        const int zero = 0;
        r = sd_bus_message_append_basic(m, 'i', &zero); if (r < 0) { assert(0); }
        r = sd_bus_message_open_container(m, 'a', "s"); if (r < 0) { assert(0); }
        r = sd_bus_message_append_basic(m, 's', "xyz.openbmc_project.Association"); if (r < 0) { assert(0); }
        r = sd_bus_message_close_container(m); if (r < 0) { assert(0); }
        r = sd_bus_call(bus, m, 0, &err, &reply); if (r < 0) { assert(0); }
        const char* sig = sd_bus_message_get_signature(reply, 0);
        if (strcmp(sig, "as")) { assert(0); }
        r = sd_bus_message_enter_container(reply, 'a', "s"); if (r < 0) { assert(0); }
        std::set<std::string> assoc_paths;
        while (true) {
            const char* p;
            r = sd_bus_message_read_basic(reply, 's', &p);
            if (r <= 0) break;
            else { assoc_paths.insert(p); }
        }
        r = sd_bus_message_exit_container(reply); if (r < 0) { assert(0); }

        // 3.6: Examine the objects in assoc_paths
        // Example:
        // busctl call xyz.openbmc_project.ObjectMapper
        // /xyz/openbmc_project/sensors/voltage/XXX/inventory
        // org.freedesktop.DBus.Properties
        // Get ss xyz.openbmc_project.Association endpoints
        for (const std::string& assoc_path : assoc_paths) {
            err = SD_BUS_ERROR_NULL;
            r = sd_bus_message_new_method_call(
                bus, &m, "xyz.openbmc_project.ObjectMapper",
                assoc_path.c_str(),
                "org.freedesktop.DBus.Properties", "Get"
            );
            r = sd_bus_message_append_basic(m, 's', "xyz.openbmc_project.Association"); if (r < 0) { assert(0); }
            r = sd_bus_message_append_basic(m, 's', "endpoints"); if (r < 0) { assert(0); }
            r = sd_bus_call(bus, m, 0, &err, &reply);
            if (r < 0) { 
                // The object may not have any endpoints
                continue;
            }
            const char* sig = sd_bus_message_get_signature(reply, 0);
            if (strcmp(sig, "v")) { assert(0); }
            r = sd_bus_message_enter_container(reply, 'v', "as"); if (r < 0) { assert(0); }
            r = sd_bus_message_enter_container(reply, 'a', "s"); if (r < 0) { assert(0); }
            std::set<std::string> entries;
            while (true) {
                const char* p;
                r = sd_bus_message_read_basic(reply, 's', &p);
                if (r <= 0) break;
                entries.insert(p);
            }
            r = sd_bus_message_exit_container(reply); if (r < 0) { assert(0); }
            r = sd_bus_message_exit_container(reply); if (r < 0) { assert(0); }

            (*sensor_snapshot)->AddAssociationEndpoints(assoc_path, entries);
        }

        // 3.7: Store the Association Definitions
        // busctl call xyz.openbmc_project.ObjectMapper 
        // /xyz/openbmc_project/object_mapper xyz.openbmc_project.ObjectMapper
        // GetSubTree sias / 0 1 xyz.openbmc_project.Association.Definitions
        // Returns a{sa{sas}}
        err = SD_BUS_ERROR_NULL;
        r = sd_bus_message_new_method_call(
            bus, &m, "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper",
            "GetSubTree"
        );
        std::vector<std::pair<std::string, std::string>> services_and_objects;  // Record the Associations from those pairs
        r = sd_bus_message_append_basic(m, 's', "/"); if (r < 0) { assert(0); }
        r = sd_bus_message_append_basic(m, 'i', &zero); if (r < 0) { assert(0); }
        r = sd_bus_message_open_container(m, 'a', "s"); if (r < 0) { assert(0); }
        r = sd_bus_message_append_basic(m, 's', "xyz.openbmc_project.Association.Definitions"); if (r < 0) { assert(0); }
        r = sd_bus_message_close_container(m); if (r < 0) { assert(0); }
        r = sd_bus_call(bus, m, 0, &err, &reply); if (r < 0) { assert(0); }
        sig = sd_bus_message_get_signature(reply, 0);
        if (strcmp(sig, "a{sa{sas}}")) { assert(0); }
        r = sd_bus_message_enter_container(reply, 'a', "{sa{sas}}"); if (r <= 0) { assert(0); }
        while (true) {
            r = sd_bus_message_enter_container(reply, 'e', "sa{sas}"); if (r <= 0) { 
                break; }  // e denotes 'dict entry'
            const char* p;  // path
            r = sd_bus_message_read_basic(reply, 's', &p);
            if (r <= 0) break;
            r = sd_bus_message_enter_container(reply, 'a', "{sas}"); if (r <= 0) { assert(0); }
            while (true) {
                const char* service; // service
                r = sd_bus_message_enter_container(reply, 'e', "sas"); if (r <= 0) { 
                    break; }
                r = sd_bus_message_read_basic(reply, 's', &service); if (r < 0) { assert(0); }
                services_and_objects.emplace_back(std::string(service), std::string(p));
                r = sd_bus_message_enter_container(reply, 'a', "s"); if (r <= 0) { assert(0); }
                while (true) {
                    const char* iface;
                    r = sd_bus_message_read_basic(reply, 's', &iface); if (r <= 0) {
                        break;
                    }
                }
                r = sd_bus_message_exit_container(reply); if (r < 0) { assert(0); }  // exit a
                r = sd_bus_message_exit_container(reply); if (r < 0) { assert(0); }  // exit e
            }
            r = sd_bus_message_exit_container(reply); if (r < 0) { assert(0); }  // exit a
            r = sd_bus_message_exit_container(reply); if (r < 0) { assert(0); }  // exit e
        }
        r = sd_bus_message_exit_container(reply); if (r < 0) { assert(0); }  // exit a

        //printf("%lu entries in services_and_objects\n", services_and_objects.size());

        // 3.7.1: Obtain the Associations for the above objects
        // busctl call xyz.openbmc_project.PSUSensor  
        // /xyz/openbmc_project/sensors/current/XXXX 
        // org.freedesktop.DBus.Properties Get ss 
        //  xyz.openbmc_project.Association.Definitions Associations
        for (const std::pair<std::string, std::string>& serv_and_obj : services_and_objects) {
            //printf("Finding associations under %s, %s\n",
            //    serv_and_obj.first.c_str(), serv_and_obj.second.c_str());
            err = SD_BUS_ERROR_NULL;
            r = sd_bus_message_new_method_call(
                bus, &m, serv_and_obj.first.c_str(),
                serv_and_obj.second.c_str(), "org.freedesktop.DBus.Properties", "Get");
            if (r < 0) { assert(0); }
            r = sd_bus_message_append_basic(m, 's', "xyz.openbmc_project.Association.Definitions"); if (r < 0) { assert(0); }
            r = sd_bus_message_append_basic(m, 's', "Associations"); if (r < 0) { assert(0); }
            r = sd_bus_call(bus, m, 0, &err, &reply); if (r <= 0) { continue; }
            sig = sd_bus_message_get_signature(reply, 0);
            if (strcmp(sig, "v")) { assert(0); }
            r = sd_bus_message_enter_container(reply, 'v', "a(sss)"); if (r <= 0) { continue; }
            r = sd_bus_message_enter_container(reply, 'a', "(sss)"); if (r <= 0) { continue; }
            while (true) {
                r = sd_bus_message_enter_container(reply, 'r', "sss"); if (r <= 0) { break; }  // struct
                const char* forward, *reverse, *endpoint;
                r = sd_bus_message_read_basic(reply, 's', &forward); if (r < 0) { break; }
                r = sd_bus_message_read_basic(reply, 's', &reverse); if (r < 0) { assert(0); }
                r = sd_bus_message_read_basic(reply, 's', &endpoint); if (r < 0) { assert(0); }
                (*sensor_snapshot)->AddAssociationDefinition(serv_and_obj.second, std::string(forward), std::string(reverse), std::string(endpoint));
                r = sd_bus_message_exit_container(reply); if (r < 0) { assert(0); }  // exit struct
            }
            r = sd_bus_message_exit_container(reply); if (r < 0) { assert(0); }  // exit a
            r = sd_bus_message_exit_container(reply); if (r < 0) { assert(0); }  // exit v
        }
        (*sensor_snapshot)->DumpAssociationDefinitionGraphToFile();

//        printf("4. Check Hwmon's DBus objects\n");
        for (int i = 0; i < int(comms.size()); i++)
        {
            const std::string& comm = comms[i];
            const std::string& service = services[i];
            if (comm.find("phosphor-hwmon-readd") != std::string::npos &&
                !IsUniqueName(service))
            {
                // printf("Should introspect %s\n", service.c_str());
                std::vector<std::string> objpaths =
                    FindAllObjectPathsForService(bus, service, nullptr);
                for (const std::string& op : objpaths)
                {
                    if (IsSensorObjectPath(op))
                    {
                        (*sensor_snapshot)->SetSensorVisibleFromHwmon(service, op);
                    }
                }
            }
        }

        // Call `ipmitool sdr list` and see which sensors exist.
//        printf("5. Checking ipmitool SDR List\n");
        std::string out;
        bool skip_sdr_list = false;
        if (getenv("SKIP"))
        {
            skip_sdr_list = true;
        }
        if (!skip_sdr_list)
        {
            constexpr int MAX_BUFFER = 255;
            char buffer[MAX_BUFFER];
            FILE* stream = popen("ipmitool sdr list", "r");
            while (fgets(buffer, MAX_BUFFER, stream) != NULL)
            {
                out.append(buffer);
            }
            pclose(stream);
        }
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
                (*sensor_snapshot)->SetSensorVisibleFromIpmitoolSdr(Trim(sensor_id));
            }
            else
                break;
        }
//        printf("=== Sensors snapshot summary: ===\n");
//        g_sensor_snapshot->PrintSummary();
    }

    void I2CMonitorThread() {
        while (true) {
            std::ifstream ifs("/sys/kernel/debug/tracing/trace");
            while (ifs.good()) {
                std::string line;
                std::getline(ifs, line);
                
                int pid; double timestamp; I2CCmd cmd; int i2cid;
                if (ParseI2CTraceLine(line, pid, timestamp, cmd, i2cid)) {

                    // Assume only I2C0 through I2C15 are physical buses and
                    // any bus greater than and including 16 are Muxed I2C buses
                    if (i2cid >= 0 && i2cid <= 15) {
                        if (cmd == I2C_WRITE || cmd == I2C_READ) {
                            g_dbus_statistics.IncrementI2CTxCount(pid);
                        }
                    }
                }
            }
            sleep(1);
        }
    }
} // namespace dbus_top_analyzer

void DBusTopStatistics::OnNewDBusMessage(const char* sender,
                                         const char* destination,
                                         const char* interface,
                                         const char* path, const char* member,
                                         const char type, sd_bus_message* m)
{
    num_messages_++;
    std::vector<std::string> keys;

    std::string sender_orig = CheckAndFixNullString(sender);
    std::string dest_orig = CheckAndFixNullString(destination);
    // For method return messages, we actually want to show the sender
    // and destination of the original method call, so we swap the
    // sender and destination
    if (type == 2)
    { // DBUS_MESSAGE_TYPE_METHOD_METHOD_RETURN
        std::swap(sender_orig, dest_orig);
    }

    // Special case: when PID == 1 (init), the DBus unit would be systemd.
    // It seems it was not possible to obtain the connection name of systemd
    // so we manually set it here.
    const int sender_orig_pid =
        g_connection_snapshot->GetConnectionPIDFromNameOrUniqueName(
            sender_orig);

    if (sender_orig_pid == 1)
    {
        sender_orig = "systemd";
    }
    const int dest_orig_pid =
        g_connection_snapshot->GetConnectionPIDFromNameOrUniqueName(dest_orig);
    if (dest_orig_pid == 1)
    {
        dest_orig = "systemd";
    }

    bool has_sender_field = false;
    bool has_i2c_field = false;
    for (DBusTopSortField field : fields_)
    {
        switch (field)
        {
            case kSender:
                keys.push_back(sender_orig);
                has_sender_field = true;
                break;
            case kDestination:
                keys.push_back(dest_orig);
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
            {
                has_sender_field = true;
                if (sender_orig_pid != INVALID)
                {
                    keys.push_back(std::to_string(sender_orig_pid));
                }
                else
                {
                    keys.push_back("(unknown)");
                }
                break;
            }
            case kSenderCMD:
            {
                has_sender_field = true;
                keys.push_back(
                    g_connection_snapshot->GetConnectionCMDFromNameOrUniqueName(
                        sender_orig));
                break;
            }
            case kMsgPerSec:
            case kAverageLatency:
                break; // Don't populate "keys" using these 2 fields
            case kSenderI2CTxPerSec:
                has_i2c_field = true;
                break;
        }
    }
    // keys = combination of fields of user's choice

    if (stats_.count(keys) == 0)
    {
        stats_[keys] = DBusTopComputedMetrics();
    }

    // If Sender, SenderPID or SenderCMD is selected, add an entry to stats2pid_ so
    // that we can look up I2C stats later
    if (has_sender_field && has_i2c_field && sender_orig_pid != INVALID) {
        stats2pid_[keys] = sender_orig_pid;
    }

    // Need to update msg/s regardless
    switch (type)
    {
        case 1: // DBUS_MESSAGE_TYPE_METHOD_CALL
            stats_[keys].num_method_calls++;
            break;
        case 2: // DBUS_MESSAGE_TYPE_METHOD_METHOD_RETURN
            stats_[keys].num_method_returns++;
            break;
        case 3: // DBUS_MESSAGE_TYPE_ERROR
            stats_[keys].num_errors++;
            break;
        case 4: // DBUS_MESSAGE_TYPE_SIGNAL
            stats_[keys].num_signals++;
            break;
    }
    // Update global latency histogram
    // For method call latency
    if (type == 1) // DBUS_MESSAGE_TYPE_METHOD_CALL
    {
        uint64_t serial; // serial == cookie
        sd_bus_message_get_cookie(m, &serial);
        in_flight_methodcalls[serial] = Microseconds();
    }
    else if (type == 2) // DBUS_MESSAGE_TYPE_MEHOTD_RETURN
    {
        uint64_t reply_serial = 0; // serial == cookie
        sd_bus_message_get_reply_cookie(m, &reply_serial);
        if (in_flight_methodcalls.count(reply_serial) > 0)
        {
            float dt_usec =
                Microseconds() - in_flight_methodcalls[reply_serial];
            in_flight_methodcalls.erase(reply_serial);
            dbus_top_analyzer::g_mc_time_histogram.AddSample(dt_usec);

            // Add method call count and total latency to the corresponding key
            stats_[keys].total_latency_usec += dt_usec;
        }
    }
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

void EnableKernelI2CTracing() {
  system("echo nop > /sys/kernel/debug/tracing/current_tracer");
  system("echo 1   > /sys/kernel/debug/tracing/events/i2c/enable");
  system("echo 1   > /sys/kernel/debug/tracing/tracing_on");
}

void DisableKernelI2CTracing() {
  system("echo nop > /sys/kernel/debug/tracing/current_tracer");
  system("echo 0   > /sys/kernel/debug/tracing/events/i2c/enable");
  system("echo 0   > /sys/kernel/debug/tracing/tracing_on");
}

bool ParseI2CTraceLine(std::string line,
  int& pid, double& timestamp, I2CCmd& cmd, int& i2cid) {
    int x = line.find('-');
    if (x == -1) return false;

    line = line.substr(x+1);
    x = line.find(' ');
    if (x == -1) return false;

    std::string pid_str = line.substr(0, x);
    pid = std::atoi(pid_str.c_str());
    
    x = line.find(':');
    if (x == -1) return false;
    std::string part1 = line.substr(0, x); // until timestamp
    std::string part2 = line.substr(x+2);
    
    x = part1.rfind(' ');
    if (x == -1) return false;
    part1 = part1.substr(x+1);
    timestamp = std::atof(part1.c_str());
    
    x = part2.find(':');
    if (x == -1) return false;
    std::string part3 = part2.substr(x+2);
    part2 = part2.substr(0, x);
    if (part2 == "i2c_write") {
        cmd = I2C_WRITE;
    } else if (part2 == "i2c_read") {
        cmd = I2C_READ;
    } else if (part2 == "i2c_reply") {
        cmd = I2C_REPLY;
    } else if (part2 == "i2c_result") {
        cmd = I2C_RESULT;
    }
    
    x = part3.find(' ');
    if (x == -1) return false;
    part3 = part3.substr(0, x);
    x = part3.find('-');
    if (x == -1) return false;
    part3 = part3.substr(x+1);
    i2cid = std::atoi(part3.c_str());
    return true;
}