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
#include "main.hpp"

#include <ncurses.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>

#include <unordered_map>

bool IS_USER_BUS = false; // User bus or System bus?
extern sd_bus* g_bus;
extern DBusConnectionSnapshot* g_connection_snapshot;
static std::unordered_map<uint64_t, uint64_t> in_flight_methodcalls;

namespace dbus_top_analyzer
{
    extern DBusTopStatistics g_dbus_statistics;
    extern Histogram<float> g_mc_time_histogram;
} // namespace dbus_top_analyzer

static void TrackMessage(sd_bus_message* m)
{}
// Obtain a Monitoring DBus connection
int AcquireBus(sd_bus** ret)
{
    int r;
    r = sd_bus_new(ret);
    if (r < 0)
    {
        printf("Could not allocate bus: %s\n", strerror(-r));
        return 0;
    }
    r = sd_bus_set_monitor(*ret, true);
    if (r < 0)
    {
        printf("Could not set monitor mode: %s\n", strerror(-r));
        return 0;
    }
    r = sd_bus_negotiate_creds(*ret, true, _SD_BUS_CREDS_ALL);
    if (r < 0)
    {
        printf("Could not enable credentials: %s\n", strerror(-r));
        return 0;
    }
    r = sd_bus_negotiate_timestamp(*ret, true);
    if (r < 0)
    {
        printf("Could not enable timestamps: %s\n", strerror(-r));
        return 0;
    }
    r = sd_bus_negotiate_fds(*ret, true);
    if (r < 0)
    {
        printf("Could not enable fds: %s\n", strerror(-r));
        return 0;
    }
    r = sd_bus_set_bus_client(*ret, true);
    if (r < 0)
    {
        printf("Could not enable bus client: %s\n", strerror(-r));
        return 0;
    }
    if (IS_USER_BUS)
    {
        r = sd_bus_set_address(*ret, "haha");
        if (r < 0)
        {
            printf("Could not set user bus: %s\n", strerror(-r));
            return 0;
        }
    }
    else
    {
        r = sd_bus_set_address(*ret, "unix:path=/run/dbus/system_bus_socket");
        if (r < 0)
        {
            printf("Could not set system bus: %s\n", strerror(-r));
            return 0;
        }
    }
    r = sd_bus_start(*ret);
    if (r < 0)
    {
        printf("Could not connect to bus: %s\n", strerror(-r));
        return 0;
    }
    return r;
}

void DbusCaptureThread()
{
    int r;
    // Become Monitor
    uint32_t flags = 0;
    sd_bus_message* message;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    r = sd_bus_message_new_method_call(
        g_bus, &message, "org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus.Monitoring", "BecomeMonitor");
    if (r < 0)
    {
        printf("Could not create the BecomeMonitor function call\n");
        exit(0);
    }
    // Match conditions
    r = sd_bus_message_open_container(message, 'a', "s");
    if (r < 0)
    {
        printf("Could not open container\n");
        exit(0);
    }
    r = sd_bus_message_close_container(message);
    if (r < 0)
    {
        printf("Could not close container\n");
        exit(0);
    }
    r = sd_bus_message_append_basic(message, 'u', &flags);
    if (r < 0)
    {
        printf("Could not append flags\n");
        exit(0);
    }
    r = sd_bus_call(g_bus, message, 0, &error, nullptr);
    if (r < 0)
    {
        printf("Could not call org.freedesktop.DBus.Monitoring.BecomeMonitor "
               "%d, %s\n",
               r, strerror(-r));
        exit(0);
    }
    const char* unique_name;
    r = sd_bus_get_unique_name(g_bus, &unique_name);
    if (r < 0)
    {
        printf("Could not get unique name: %s\n", strerror(-r));
        exit(0);
    }
    // Enter packet processing loop
    while (true)
    {
        struct sd_bus_message* m = nullptr;
        r = sd_bus_process(g_bus, &m);
        if (m != nullptr)
        {
            if (r < 0)
            {
                printf("Failed to call sd_bus_process: %s\n", strerror(-r));
            }
            uint8_t type;
            r = sd_bus_message_get_type(m, &type);
            const char* path = sd_bus_message_get_path(m);
            // const char* iface = sd_bus_message_get_interface(m);
            const char* sender = sd_bus_message_get_sender(m);
            const char* destination = sd_bus_message_get_destination(m);
            const char* interface = sd_bus_message_get_interface(m);
            const char* member = sd_bus_message_get_member(m);
            // TODO: This is for the bottom-left window
            TrackMessage(m);

            // Look up the unique connection name for sender and destination
            std::string sender_uniq, dest_uniq;
            if (sender != nullptr)
            {
                sender_uniq =
                    g_connection_snapshot->GetUniqueNameIfExists(sender);
            }
            if (destination != nullptr)
            {
                dest_uniq =
                    g_connection_snapshot->GetUniqueNameIfExists(destination);
            }
            // This is for the bottom-right window
            dbus_top_analyzer::g_dbus_statistics.OnNewDBusMessage(
            sender_uniq.c_str(), dest_uniq.c_str(), interface, path, member,
                type, m);
            sd_bus_message_unref(m);
        }
        r = sd_bus_wait(g_bus,
                        (uint64_t)(GetSummaryIntervalInMillises() * 1000));
        if (r < 0)
        {
            printf("Failed to wait on bus: %s\n", strerror(-r));
            abort();
        }
        // Perform one analysis step
        dbus_top_analyzer::Process();
    }
}