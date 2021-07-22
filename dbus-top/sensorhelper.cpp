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

#include "sensorhelper.hpp"
#include "main.hpp"

#include <unistd.h>

#include <cassert>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

extern SensorSnapshot* g_sensor_snapshot;
extern DBusConnectionSnapshot* g_connection_snapshot;

std::vector<std::string> MySplit(const std::string& s)
{
    int idx = 0, prev_idx = 0;
    std::vector<std::string> ret;
    while (idx <= static_cast<int>(s.size()))
    {
        if (idx == static_cast<int>(s.size()) || s[idx] == '/')
        {
            if (idx > prev_idx)
            {
                ret.push_back(s.substr(prev_idx, idx - prev_idx));
            }
            prev_idx = idx + 1;
        }
        idx++;
    }
    return ret;
}

// Example: /xyz/openbmc_project/sensors/temperature/powerseq_temp
bool IsSensorObjectPath(const std::string& s)
{
    std::vector<std::string> sections = MySplit(s);
    if (sections.size() == 5 && sections[0] == "xyz" &&
        sections[1] == "openbmc_project" && sections[2] == "sensors")
    {
        return true;
    }
    else
    {
        return false;
    }
}

// Example: /xyz/openbmc_project/sensors/temperature/powerseq_temp/chassis
bool IsSensorObjectPathWithAssociation(const std::string& s,
                                       std::string* sensor_obj_path)
{
    std::vector<std::string> sections = MySplit(s);
    if (sections.size() == 6)
    {
        size_t idx = s.rfind('/');
        return IsSensorObjectPath(s.substr(0, idx));
    }
    else
    {
        return false;
    }
}

bool IsUniqueName(const std::string& x)
{
    if (x.empty())
        return false;
    if (x[0] != ':')
        return false;
    if (x[0] == ':')
    {
        for (int i = 1; i < int(x.size()); i++)
        {
            const char ch = x[i];
            if (ch >= '0' || ch <= '9')
                continue;
            else if (ch == '.')
                continue;
            else
                return false;
        }
    }
    return true;
}

std::vector<std::string> FindAllObjectPathsForService(
    const std::string& service,
    std::function<void(const std::string&, const std::vector<std::string>&)>
        on_interface_cb)
{
    // Not available for PCAP replay, only valid with actual DBus capture
    assert(false);
}

bool IsWhitespace(const char c)
{
    if (c == ' ' || c == '\r' || c == '\n')
        return true;
    else
        return false;
}

std::string Trim(const std::string& s)
{
    const int N = int(s.size());
    int idx0 = 0, idx1 = int(N - 1);
    while (idx0 < N && IsWhitespace(s[idx0]))
        idx0++;
    while (idx1 >= 0 && IsWhitespace(s[idx1]))
        idx1--;
    if (idx0 >= N || idx1 < 0)
        return "";
    return s.substr(idx0, idx1 - idx0 + 1);
}
