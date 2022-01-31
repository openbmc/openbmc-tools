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

#include "main.hpp"

#include "analyzer.hpp"
#include "bargraph.hpp"
#include "dbus_capture.hpp"
#include "histogram.hpp"
#include "menu.hpp"
#include "sensorhelper.hpp"
#include "views.hpp"

#include <fmt/printf.h>
#include <ncurses.h>
#include <stdio.h>
#include <unistd.h>

#include <cassert>
#include <iomanip>
#include <sstream>
#include <thread>

DBusTopWindow* g_current_active_view;
SummaryView* g_summary_window;
InventoryView* g_sensor_detail_view;
DBusStatListView* g_dbus_stat_list_view;
FooterView* g_footer_view;
BarGraph<float>* g_bargraph = nullptr;
Histogram<float>* g_histogram;
std::vector<DBusTopWindow*> g_views;
int g_highlighted_view_index = INVALID;
sd_bus* g_bus = nullptr;

int GetConnectionNumericID(const std::string& unique_name)
{
    size_t idx = unique_name.find('.');
    if (idx == std::string::npos)
    {
        return -999;
    }
    try
    {
        int ret = std::atoi(unique_name.substr(idx + 1).c_str());
        return ret;
    }
    catch (const std::exception& e)
    {
        return -999;
    }
}

// Whenever an update of SensorSnapshot and DBusConnectionSnapshot is needed,
// they are populated into the "staging" copies and a pointer swap is done
// by the main thread (the thread that constructs the snapshots shall not touch
// the copy used for UI rendering)
bool g_sensor_update_thread_active = false;
std::string g_snapshot_update_bus_cxn =
    ""; // The DBus connection used by the updater thread.
int g_snapshot_update_bus_cxn_id = -999;
SensorSnapshot *g_sensor_snapshot, *g_sensor_snapshot_staging = nullptr;
DBusConnectionSnapshot *g_connection_snapshot,
    *g_connection_snapshot_staging = nullptr;
std::mutex g_mtx_snapshot_update;

DBusTopStatistics* g_dbus_statistics; // At every update interval,
                                      // dbus_top_analyzer::g_dbus_statistics's
                                      // value is copied to this one for display
void ReinitializeUI();
int maxx, maxy, halfx, halfy;

void ActivateWindowA()
{
    g_views[0]->visible_ = true;
    g_views[0]->maximize_ = true;
    g_views[1]->visible_ = false;
    g_views[2]->visible_ = false;
}

void ActivateWindowB()
{
    g_views[1]->visible_ = true;
    g_views[1]->maximize_ = true;
    g_views[0]->visible_ = false;
    g_views[2]->visible_ = false;
}
void ActivateWindowC()
{
    g_views[2]->visible_ = true;
    g_views[2]->maximize_ = true;
    g_views[0]->visible_ = false;
    g_views[1]->visible_ = false;
}
void ActivateAllWindows()
{
    g_views[0]->maximize_ = false;
    g_views[1]->maximize_ = false;
    g_views[2]->maximize_ = false;
    g_views[0]->visible_ = true;
    g_views[1]->visible_ = true;
    g_views[2]->visible_ = true;
}

void InitColorPairs()
{
    init_pair(1, COLOR_WHITE, COLOR_BLACK); // Does not work on actual machine
    init_pair(2, COLOR_BLACK, COLOR_WHITE);
}

// Returns number of lines drawn
int DrawTextWithWidthLimit(WINDOW* win, std::string txt, int y, int x,
                           int width, const std::string& delimiters)
{
    int ret = 0;
    std::string curr_word, curr_line;
    while (txt.empty() == false)
    {
        ret++;
        if (static_cast<int>(txt.size()) > width)
        {
            mvwprintw(win, y, x, txt.substr(0, width).c_str());
            txt = txt.substr(width);
        }
        else
        {
            mvwprintw(win, y, x, txt.c_str());
            break;
        }
        y++;
    }
    return ret;
}

void UpdateWindowSizes()
{
    /* calculate window sizes and locations */
    if (getenv("FIXED_TERMINAL_SIZE"))
    {
        maxx = 100;
        maxy = 30;
    }
    else
    {
        getmaxyx(stdscr, maxy, maxx);
        halfx = maxx >> 1;
        halfy = maxy >> 1;
    }
    for (DBusTopWindow* v : g_views)
    {
        v->OnResize(maxx, maxy);
        if (v->maximize_)
        {
            v->rect = {0, 0, maxx, maxy - MARGIN_BOTTOM};
            v->UpdateWindowSizeAndPosition();
        }
    }
}

std::string FloatToString(float value)
{
    return fmt::sprintf("%.2f", value);
}

void DBusTopRefresh()
{
    g_mtx_snapshot_update.lock();
    UpdateWindowSizes();
    for (DBusTopWindow* v : g_views)
    {
        v->Render();
    }
    DBusTopWindow* focused_view = g_current_active_view;
    if (focused_view)
    {
        focused_view->DrawBorderIfNeeded(); // focused view border: on top
    }
    refresh();
    g_mtx_snapshot_update.unlock();
}

void DBusTopUpdateFooterView()
{
    g_mtx_snapshot_update.lock();
    g_footer_view->Render();
    g_mtx_snapshot_update.unlock();
}

// This function is called by the Capture thread
void DBusTopStatisticsCallback(DBusTopStatistics* stat, Histogram<float>* hist)
{
    if (stat == nullptr)
        return;
    // Makes a copy for display
    // TODO: Add a mutex here for safety
    stat->Assign(g_dbus_statistics);
    hist->Assign(g_histogram);
    float interval_secs = stat->seconds_since_last_sample_;
    if (interval_secs == 0)
    {
        interval_secs = GetSummaryIntervalInMillises() / 1000.0f;
    }
    g_summary_window->UpdateDBusTopStatistics(stat);
    stat->SetSortFieldsAndReset(g_dbus_stat_list_view->GetSortFields());

    g_mtx_snapshot_update.lock();
    if (g_sensor_snapshot_staging != nullptr &&
        g_connection_snapshot_staging != nullptr)
    {

        std::swap(g_sensor_snapshot_staging, g_sensor_snapshot);
        std::swap(g_connection_snapshot_staging, g_connection_snapshot);

        delete g_connection_snapshot_staging;
        delete g_sensor_snapshot_staging;

        g_sensor_snapshot_staging = nullptr;
        g_connection_snapshot_staging = nullptr;
    }
    g_mtx_snapshot_update.unlock();

    g_sensor_detail_view->UpdateSensorSnapshot(g_sensor_snapshot);
    // ReinitializeUI(); // Don't do it here, only when user presses [R]
    DBusTopRefresh();
}

void CycleHighlightedView()
{
    int new_index = 0;
    if (g_highlighted_view_index == INVALID)
    {
        new_index = 0;
    }
    else
    {
        new_index = g_highlighted_view_index + 1;
    }
    while (new_index < static_cast<int>(g_views.size()) &&
           g_views[new_index]->selectable_ == false)
    {
        new_index++;
    }
    if (new_index >= static_cast<int>(g_views.size()))
    {
        new_index = INVALID;
    }
    // Un-highlight all
    for (DBusTopWindow* v : g_views)
    {
        v->focused_ = false;
    }
    if (new_index != INVALID)
    {
        g_views[new_index]->focused_ = true;
        g_current_active_view = g_views[new_index];
    }
    else
    {
        g_current_active_view = nullptr;
    }
    g_highlighted_view_index = new_index;
    DBusTopRefresh();
}

int UserInputThread()
{
    while (true)
    {
        int c = getch();
        DBusTopWindow* curr_view = g_current_active_view;
        // If a view is currently focused on
        if (curr_view)
        {
            switch (c)
            {
                case '\e': // 27 in dec, 0x1B in hex, escape key
                {
                    getch();
                    c = getch();
                    switch (c)
                    {
                        case 'A':
                            curr_view->OnKeyDown("up");
                            break;
                        case 'B':
                            curr_view->OnKeyDown("down");
                            break;
                        case 'C':
                            curr_view->OnKeyDown("right");
                            break;
                        case 'D':
                            curr_view->OnKeyDown("left");
                            break;
                        case '\e':
                            curr_view->OnKeyDown("escape");
                            break;
                    }
                    break;
                }
                case '\n': // 10 in dec, 0x0A in hex, line feed
                {
                    curr_view->OnKeyDown("enter");
                    break;
                }
                case 'q':
                case 'Q': // Q key
                {
                    curr_view->OnKeyDown("escape");
                    break;
                }
                case 'a':
                case 'A': // A key
                {
                    curr_view->OnKeyDown("a");
                    break;
                }
                case 'd':
                case 'D': // D key
                {
                    curr_view->OnKeyDown("d");
                    break;
                }
                case 33: // Page up
                {
                    curr_view->OnKeyDown("pageup");
                    break;
                }
                case 34: // Page down
                {
                    curr_view->OnKeyDown("pagedown");
                    break;
                }
                case ' ': // Spacebar
                {
                    curr_view->OnKeyDown("space");
                    break;
                }
            }
        }
        // The following keys are registered both when a view is selected and
        // when it is not
        switch (c)
        {
            case '\t': // 9 in dec, 0x09 in hex, tab
            {
                CycleHighlightedView();
                break;
            }
            case 'r':
            case 'R':
            {
                ReinitializeUI();
                DBusTopRefresh();
                break;
            }
            case 'x':
            case 'X':
            {
                clear();
                ActivateWindowA();
                break;
            }
            case 'y':
            case 'Y':
            {
                clear();
                ActivateWindowB();
                break;
            }
            case 'z':
            case 'Z':
            {
                clear();
                ActivateWindowC();
                break;
            }
            case 'h':
            case 'H':
            {
                ActivateAllWindows();
                DBusTopRefresh();
            }
            default:
                break;
        }
    }
    exit(0);
}

void ReinitializeUI()
{
    endwin();
    initscr();
    use_default_colors();
    noecho();
    for (int i = 0; i < static_cast<int>(g_views.size()); i++)
    {
        g_views[i]->RecreateWindow();
    }
}

void ListAllSensorsThread()
{
    // Create a temporary connection
    assert(g_sensor_update_thread_active == false);
    sd_bus* bus;
    int r;
    r = AcquireBus(&bus);
    printf("r=%d\n", r);
    const char* bus_name;
    sd_bus_get_unique_name(bus, &bus_name);
    printf("bus_name=%s\n", bus_name);
    g_snapshot_update_bus_cxn = std::string(bus_name);
    g_snapshot_update_bus_cxn_id =
        GetConnectionNumericID(g_snapshot_update_bus_cxn);

    g_sensor_update_thread_active = true;
    DBusConnectionSnapshot* cxn_snapshot;
    SensorSnapshot* sensor_snapshot;
    dbus_top_analyzer::ListAllSensors(bus, &cxn_snapshot, &sensor_snapshot);

    g_mtx_snapshot_update.lock();
    g_connection_snapshot = cxn_snapshot;
    g_sensor_snapshot = sensor_snapshot;
    g_mtx_snapshot_update.unlock();
    g_sensor_update_thread_active = false;
    g_snapshot_update_bus_cxn = "";
    g_snapshot_update_bus_cxn_id = -999;

    sd_bus_close(bus);
}

int main(int argc, char** argv)
{
    int r = AcquireBus(&g_bus);
    if (r <= 0)
    {
        printf("Error acquiring bus for monitoring\n");
        exit(0);
    }

    printf("Listing all sensors for display\n");
    // ListAllSensors creates connection snapshot and sensor snapshot

    g_connection_snapshot = new DBusConnectionSnapshot();
    g_sensor_snapshot = new SensorSnapshot(g_connection_snapshot);

    g_bargraph = new BarGraph<float>(300);
    g_histogram = new Histogram<float>();

    initscr();
    use_default_colors();
    start_color();
    noecho();

    clear();
    g_dbus_statistics = new DBusTopStatistics();
    g_summary_window = new SummaryView();
    g_sensor_detail_view = new InventoryView();
    g_dbus_stat_list_view = new DBusStatListView();
    g_footer_view = new FooterView();
    g_views.push_back(g_summary_window);
    g_views.push_back(g_sensor_detail_view);
    g_views.push_back(g_dbus_stat_list_view);
    g_views.push_back(g_footer_view);

    // Do the scan in a separate thread. This uses the Footer View so it has to
    // be after g_footer_view = new FooterView()
    std::thread list_all_sensors_thread(ListAllSensorsThread);

    g_sensor_detail_view->UpdateSensorSnapshot(g_sensor_snapshot);
    UpdateWindowSizes();
    dbus_top_analyzer::SetDBusTopStatisticsCallback(&DBusTopStatisticsCallback);
    std::thread capture_thread(DbusCaptureThread);
    std::thread user_input_thread(UserInputThread);
    EnableKernelI2CTracing();
    std::thread i2c_monitor_thread(dbus_top_analyzer::I2CMonitorThread);
    capture_thread.join();

    return 0;
}

__attribute__((destructor)) void DTOR()
{
    DisableKernelI2CTracing();
}