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

#include <ncurses.h>

#include <ctime>
#include <sstream>
#include <string>
#include <vector>

#include "views.hpp"

std::vector<std::string> g_sensor_list;

int maxx, maxy, halfx, halfy;

constexpr int NUM_WINDOWS = 4;
// [0]: summary and history
// [1]: detail info for 1 sensor
// [2]: list of traffic
// [3]: status bar

WINDOW *g_windows[NUM_WINDOWS];

Rect g_window_rects[NUM_WINDOWS];

void CreateWindows()
{
    for (int i = 0; i < NUM_WINDOWS; i++)
    {
        g_windows[i] = newwin(maxy, maxx, 0, 0);
    }
}

void InitColorPairs()
{
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    init_pair(2, COLOR_WHITE, COLOR_BLACK);
}

SummaryView *g_summary_window;
SensorDetailView *g_sensor_detail_window;
DBusStatListView *g_dbus_stat_list_view;
FooterView *g_footer_view;
std::vector<DBusTopWindow *> g_views;

void UpdateWindowSizes()
{
    /* calculate window sizes and locations */
    getmaxyx(stdscr, maxy, maxx);
    halfx = maxx >> 1;
    halfy = maxy >> 1;
    for (DBusTopWindow *v : g_views)
    {
        v->OnResize(maxx, maxy);
    }
}

// Refresh all views, but do not touch data
void DBusTopRefresh()
{
    UpdateWindowSizes();
    for (DBusTopWindow *v : g_views)
    {
        v->Render();
    }
}

int main()
{
    // ncurses initialization
    initscr();
    use_default_colors();
    start_color();
    noecho();

    // dbus-top initialization
    InitColorPairs();
    CreateWindows();

    // Initialize views
    g_summary_window = new SummaryView();
    g_sensor_detail_window = new SensorDetailView();
    g_dbus_stat_list_view = new DBusStatListView();
    g_footer_view = new FooterView();
    g_views.push_back(g_summary_window);
    g_views.push_back(g_sensor_detail_window);
    g_views.push_back(g_dbus_stat_list_view);
    g_views.push_back(g_footer_view);
    UpdateWindowSizes();
    DBusTopRefresh();

    return 0;
}

__attribute__((destructor)) void ResetWindowMode()
{
    endwin();
    echo();
}
