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
#include <unordered_map>
#include <vector>

class DBusConnectionSnapshot;
class SensorSnapshot;

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
    kSenderI2CTxPerSec,
};

const std::string FieldNames[] = {"Sender",     "Destination", "Interface",
                                  "Path",       "Member",      "Sender PID",
                                  "Sender CMD", "Msg/s",       "Avg Latency",
                                  "Sender I2C Tx/s"};
const int FieldPreferredWidths[] = {18, 20, 12, 10, 10, 12, 25, 8, 12, 20};
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
    uint32_t total_i2c_tx;
};

class DBusTopStatistics
{
  public:
    int num_messages_;
    int num_mc_, num_mr_, num_sig_, num_error_;
    float seconds_since_last_sample_;
    std::vector<DBusTopSortField> fields_;
    std::map<std::vector<std::string>, DBusTopComputedMetrics> stats_;

    // For mapping to PID
    std::map<std::vector<std::string>, int> stats2pid_;

    // Todo: Think of a way to not use the mtx to protect access to i2c_tx_count_
    std::unordered_map<int, int> i2c_tx_count_;

    DBusTopStatistics() :
        num_messages_(0), num_mc_(0), num_mr_(0), num_sig_(0), num_error_(0),
        seconds_since_last_sample_(0)
    {
        fields_ = {kSender, kDestination, kSenderPID, kSenderCMD};
        stats_.clear();
        stats2pid_.clear();
        mtx_.lock();
        i2c_tx_count_.clear();
        mtx_.unlock();
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
        stats2pid_.clear();
        mtx_.lock();
        i2c_tx_count_.clear();
        mtx_.unlock();
    }
    
    void SetSortFieldsAndReset(const std::vector<DBusTopSortField>& f)
    {
        num_messages_ = 0;
        num_mc_ = 0;
        num_mr_ = 0;
        num_sig_ = 0;
        num_error_ = 0;
        stats_.clear();
        stats2pid_.clear();
        fields_ = f;
        mtx_.lock();
        i2c_tx_count_.clear();
        mtx_.unlock();
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
        out->stats2pid_ = this->stats2pid_;
        mtx_.lock();
        out->i2c_tx_count_ = this->i2c_tx_count_;
        mtx_.unlock();
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

        mtx_.lock();
        std::unordered_map<int, int> i2c_tx_count = i2c_tx_count_;
        mtx_.unlock();

        // If Sender PID or Sender is selected, populate I2C TX/s
        for (std::map<std::vector<std::string>, DBusTopComputedMetrics>::iterator itr = ret.begin();
            itr != ret.end(); itr++) {
            if (stats2pid_.find(itr->first) != stats2pid_.end()) {
                int pid = stats2pid_[itr->first];
                itr->second.total_i2c_tx = i2c_tx_count[pid];
            }
        }
        return ret;
    }

    void IncrementI2CTxCount(int pid) {
        mtx_.lock();
        ++i2c_tx_count_[pid];
        mtx_.unlock();
    }

  private:
    std::mutex mtx_;
};

void EnableKernelI2CTracing();
void DisableKernelI2CTracing();

enum I2CCmd {
  I2C_WRITE, I2C_READ, I2C_RESULT, I2C_REPLY
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
    // Methods for sending Object Mapper queries
    void ListAllSensors(sd_bus*, DBusConnectionSnapshot**, SensorSnapshot**);

    // The thread that monitors /sys/kernel/debug/tracing/trace
    void I2CMonitorThread();
} // namespace dbus_top_analyzer
