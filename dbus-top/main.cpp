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
<<<<<<< HEAD

=======
<<<<<<< HEAD
>>>>>>> cf7590f (Revert "dbus-top: WIP of all currently-implemented features")
#include "main.hpp"

#include "analyzer.hpp"
#include "bargraph.hpp"
#include "dbus_capture.hpp"
#include "histogram.hpp"
#include "menu.hpp"
#include "sensorhelper.hpp"
#include "views.hpp"
=======
>>>>>>> parent of 6fe55b9 (dbus-top: WIP of all currently-implemented features)

#include <ncurses.h>

#include <ctime>
#include <sstream>
#include <string>
#include <vector>

<<<<<<< HEAD
DBusTopWindow* g_current_active_view;
SummaryView* g_summary_window;
SensorDetailView* g_sensor_detail_view;
DBusStatListView* g_dbus_stat_list_view;
FooterView* g_footer_view;
BarGraph<float>* g_bargraph = nullptr;
Histogram<float>* g_histogram;
std::vector<DBusTopWindow*> g_views;
int g_highlighted_view_index = -999;
sd_bus* g_bus = nullptr;
SensorSnapshot* g_sensor_snapshot;
DBusConnectionSnapshot* g_connection_snapshot;
DBusTopStatistics* g_dbus_statistics; // At every update interval,
                                      // dbus_top_analyzer::g_dbus_statistics's
                                      // value is copied to this one for display
void ReinitializeUI();
int maxx, maxy, halfx, halfy;
int WIN_W, WIN_H;

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
    }
}
=======
#include "views.hpp"

std::vector<std::string> g_sensor_list;

int maxx, maxy, halfx, halfy;

constexpr int NUM_WINDOWS = 4;
// [0]: summary and history
// [1]: detail info for 1 sensor
// [2]: list of traffic
// [3]: status bar
>>>>>>> parent of 6fe55b9 (dbus-top: WIP of all currently-implemented features)

WINDOW *g_windows[NUM_WINDOWS];

<<<<<<< HEAD
void DBusTopRefresh()
{
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
=======
Rect g_window_rects[NUM_WINDOWS];

void CreateWindows()
{
    for (int i = 0; i < NUM_WINDOWS; i++)
>>>>>>> parent of 6fe55b9 (dbus-top: WIP of all currently-implemented features)
    {
        g_windows[i] = newwin(maxy, maxx, 0, 0);
    }
<<<<<<< HEAD
    g_summary_window->UpdateDBusTopStatistics(stat);
    stat->SetSortFieldsAndReset(g_dbus_stat_list_view->GetSortFields());
    // ReinitializeUI(); // Don't do it here, only when user presses [R]
    DBusTopRefresh();
<<<<<<< HEAD
=======
   
=======
>>>>>>> parent of 6fe55b9 (dbus-top: WIP of all currently-implemented features)
>>>>>>> cf7590f (Revert "dbus-top: WIP of all currently-implemented features")
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
<<<<<<< HEAD
        int c = getch();
        DBusTopWindow* curr_view = g_current_active_view;
        switch (c)
        {
            case 27:
            {
                getch();
                switch (getch())
                {
                    case 'A':
                        if (curr_view)
                        {
                            curr_view->OnKeyDown("up");
                        }
                        break;
                    case 'B':
                        if (curr_view)
                        {
                            curr_view->OnKeyDown("down");
                        }
                        break;
                    case 'C': // right
                        if (curr_view)
                        {
                            curr_view->OnKeyDown("right");
                        }
                        break;
                    case 'D': // left
                        if (curr_view)
                        {
                            curr_view->OnKeyDown("left");
                        }
                        break;
                    case 27:
                        if (curr_view)
                        {
                            curr_view->OnKeyDown("escape");
                        }
                        break;
                    default:
                    {
                        break;
                    }
                }
                break;
            }
            case 9: // Tab
                CycleHighlightedView();
                break;
            case 10: // Line Feed (Enter)
                if (curr_view)
                {
                    curr_view->OnKeyDown("enter");
                }
                break;
            case 'q':
            case 'Q':
            {
                if (curr_view)
                {
                    curr_view->OnKeyDown("escape");
                }
                break;
            }
            case 33:
            { // page up
                if (curr_view)
                {
                    curr_view->OnKeyDown("pageup");
                }
                break;
            }
            case 34:
            { // page down
                if (curr_view)
                {
                    curr_view->OnKeyDown("pagedown");
                }
                break;
            }
            case ' ':
            {
                if (curr_view)
                {
                    curr_view->OnKeyDown("space");
                }
                break;
            }
            case 'r':
            case 'R':
            {
                ReinitializeUI();
                DBusTopRefresh();
                break;
            }
            default:
                break;
        }
        if (needs_refresh)
        {
            DBusTopRefresh();
        }
=======
        v->OnResize(maxx, maxy);
>>>>>>> parent of 6fe55b9 (dbus-top: WIP of all currently-implemented features)
    }
}

// Refresh all views, but do not touch data
void DBusTopRefresh()
{
<<<<<<< HEAD
    endwin();
    initscr();
    use_default_colors();
    noecho();
    for (int i = 0; i < static_cast<int>(g_views.size()); i++)
=======
    UpdateWindowSizes();
    for (DBusTopWindow *v : g_views)
>>>>>>> parent of 6fe55b9 (dbus-top: WIP of all currently-implemented features)
    {
        v->Render();
    }
<<<<<<< HEAD
=======
<<<<<<< HEAD
    refresh();
>>>>>>> cf7590f (Revert "dbus-top: WIP of all currently-implemented features")
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
    dbus_top_analyzer::ListAllSensors();
    g_bargraph = new BarGraph<float>(300);
    g_histogram = new Histogram<float>();
=======
}

int main()
{
    // ncurses initialization
>>>>>>> parent of 6fe55b9 (dbus-top: WIP of all currently-implemented features)
    initscr();
    use_default_colors();
    start_color();
    noecho();
<<<<<<< HEAD
    clear();
    g_dbus_statistics = new DBusTopStatistics();
=======

    // dbus-top initialization
    InitColorPairs();
    CreateWindows();

    // Initialize views
>>>>>>> parent of 6fe55b9 (dbus-top: WIP of all currently-implemented features)
    g_summary_window = new SummaryView();
    g_sensor_detail_window = new SensorDetailView();
    g_dbus_stat_list_view = new DBusStatListView();
    g_footer_view = new FooterView();
    g_views.push_back(g_summary_window);
    g_views.push_back(g_sensor_detail_window);
    g_views.push_back(g_dbus_stat_list_view);
    g_views.push_back(g_footer_view);
<<<<<<< HEAD
    g_sensor_detail_view->UpdateSensorSnapshot(g_sensor_snapshot);
=======
>>>>>>> cf7590f (Revert "dbus-top: WIP of all currently-implemented features")
    UpdateWindowSizes();
<<<<<<< HEAD
    dbus_top_analyzer::SetDBusTopStatisticsCallback(&DBusTopStatisticsCallback);
    std::thread capture_thread(DbusCaptureThread);
    std::thread user_input_thread(UserInputThread);
    capture_thread.join();
=======
    DBusTopRefresh();

>>>>>>> parent of 6fe55b9 (dbus-top: WIP of all currently-implemented features)
    return 0;
}

__attribute__((destructor)) void ResetWindowMode()
{
    endwin();
    echo();
}
