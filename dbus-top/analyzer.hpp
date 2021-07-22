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

#include "histogram.hpp"

#include <systemd/sd-bus.h>

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>

enum DBusTopSortField
{
    // DBus Message properties
    kSender,
    kDestination,
    kInterface,
    kPath,
    kMember,
    kSenderPID,
    kSenderCMD,
    // Computed metrics
    kMsgPerSec,
    kAverageLatency,
};

const std::string FieldNames[] = {"Sender",     "Destination", "Interface",
                                  "Path",       "Member",      "Sender PID",
                                  "Sender CMD", "Msg/s",       "Avg Latency"};
const int FieldPreferredWidths[] = {18, 20, 12, 10, 10, 10, 25, 8, 12};
bool DBusTopSortFieldIsNumeric(DBusTopSortField field);

struct DBusTopComputedMetrics
{
    DBusTopComputedMetrics()
    {
        num_signals = 0;
        num_errors = 0;
        num_method_returns = 0;
        num_method_calls = 0;
        total_latency_usec = 0;
    }
    int num_method_returns = 0;
    int num_errors = 0;
    int num_signals = 0;
    int num_method_calls;
    uint64_t total_latency_usec;
};

class DBusTopStatistics
{
  public:
    int num_messages_;
    int num_mc_, num_mr_, num_sig_, num_error_;
    float seconds_since_last_sample_;
    std::vector<DBusTopSortField> fields_;
    std::map<std::vector<std::string>, DBusTopComputedMetrics> stats_;
    DBusTopStatistics() :
        num_messages_(0), num_mc_(0), num_mr_(0), num_sig_(0), num_error_(0),
        seconds_since_last_sample_(0)
    {
        fields_ = {kSender, kDestination, kSenderPID, kSenderCMD};
        stats_.clear();
    }

    std::vector<DBusTopSortField> GetFields()
    {
        return fields_;
    }

    std::vector<std::string> GetFieldNames()
    {
        const int N = fields_.size();
        std::vector<std::string> ret(N);
        for (int i = 0; i < static_cast<int>(fields_.size()); i++)
        {
            ret[i] = FieldNames[static_cast<int>(fields_[i])];
        }
        return ret;
    }

    std::vector<int> GetFieldPreferredWidths()
    {
        const int N = fields_.size();
        std::vector<int> ret(N);
        for (int i = 0; i < static_cast<int>(fields_.size()); i++)
        {
            ret[i] = FieldPreferredWidths[static_cast<int>(fields_[i])];
        }
        return ret;
    }

    void Reset()
    {
        num_messages_ = 0;
        num_mc_ = 0;
        num_mr_ = 0;
        num_sig_ = 0;
        num_error_ = 0;
        stats_.clear();
    }
    
    void SetSortFieldsAndReset(const std::vector<DBusTopSortField>& f)
    {
        num_messages_ = 0;
        num_mc_ = 0;
        num_mr_ = 0;
        num_sig_ = 0;
        num_error_ = 0;
        stats_.clear();
        fields_ = f;
    }

    void Assign(DBusTopStatistics* out)
    {
        out->num_messages_ = this->num_messages_;
        out->num_mc_ = this->num_mc_;
        out->num_mr_ = this->num_mr_;
        out->num_sig_ = this->num_sig_;
        out->num_error_ = this->num_error_;
        out->seconds_since_last_sample_ = this->seconds_since_last_sample_;
        out->fields_ = this->fields_;
        out->stats_ = this->stats_;
    }

    void OnNewDBusMessage(const char* sender, const char* destination,
                          const char* interface, const char* path,
                          const char* message, const char type,
                          sd_bus_message* m);
    std::string CheckAndFixNullString(const char* x)
    {
        if (x == nullptr)
            return "(null)";
        else
            return std::string(x);
    }

    std::map<std::vector<std::string>, DBusTopComputedMetrics> StatsSnapshot()
    {
        std::map<std::vector<std::string>, DBusTopComputedMetrics> ret;
        ret = stats_;
        return ret;
    }

  private:
    std::mutex mtx_;
};

int GetSummaryIntervalInMillises();
// Monitor sensor details-related DBus method calls/replies
// typedef void (*SetDBusTopConnection)(const char* conn);
namespace dbus_top_analyzer
{
    void Process();
    void Finish();
    typedef void (*DBusTopStatisticsCallback)(DBusTopStatistics*,
                                            Histogram<float>*);
    void SetDBusTopStatisticsCallback(DBusTopStatisticsCallback cb);
    void AnalyzerThread();
    // Methods for sending Object Mapper queries
    void ListAllSensors();
} // namespace dbus_top_analyzer
