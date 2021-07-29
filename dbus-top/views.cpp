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
#include "views.hpp"

#include "bargraph.hpp"
#include "menu.hpp"

#include <algorithm>

extern SensorSnapshot* g_sensor_snapshot;
extern BarGraph<float>* g_bargraph;
extern DBusTopStatistics* g_dbus_statistics;

namespace dbus_top_analyzer
{
extern DBusTopStatistics g_dbus_statistics;
}

// Linear interpolation
float Lerp(float a, float b, float t)
{
    return a + t * (b - a);
}

// Linear map
float Map(float value, float start1, float stop1, float start2, float stop2,
          bool within_bounds)
{
    float t = (value - start1) / (stop1 - start1);
    float ret = Lerp(start2, stop2, t);
    if (within_bounds)
    {
        if (ret < start2)
            ret = start2;
        if (ret > stop2)
            ret = stop2;
    }
    return ret;
}

template <typename T>
void HistoryBarGraph(WINDOW* win, const Rect& rect, BarGraph<T>* bargraph)
{
    const int RIGHT_MARGIN = 5;
    const int x0 = 33, y0 = 2;
    const int w = rect.w - 2 - x0 - RIGHT_MARGIN;
    const int h = 5; // height of content
    wattrset(win, 0);
    wattron(win, A_BOLD | A_UNDERLINE);
    mvwaddstr(win, 1, x0, "History     (Total msg/s)");
    wattrset(win, 0);
    // 1. Obtain data, determine Y range
    std::vector<float> data = bargraph->GetLastNValues(w - RIGHT_MARGIN - 1);
    float ymax = -1e20, ymin = 1e20;
    if (data.empty())
    {
        data.push_back(0);
        ymin = 0;
        ymax = 10;
    }
    else
    {
        for (const float x : data)
        {
            ymax = std::max(ymax, x);
            ymin = std::min(ymin, x);
        }
    }
    // Fix edge case for both == 0
    float diff = ymax - ymin;
    if (diff < 0)
    {
        diff = -diff;
    }
    const float EPS = 1e-4;
    if (diff < EPS)
    {
        ymax += 10;
        ymin -= 10;
    }
    // Choose a suitable round-up unit to snap the grid labels to
    int snap = 1;
    if (ymax < 100)
    {
        snap = 10;
    }
    else if (ymax < 10000)
    {
        snap = 100;
    }
    else
    {
        snap = 1000;
    }
    const float eps = snap / 100.0f;
    int label_ymax =
        (static_cast<int>((ymax - eps) / snap) + 1) * snap; // round up
    int label_ymin = static_cast<int>(ymin / snap) * snap;  // round down
    float y_per_row = (label_ymax - label_ymin) * 1.0f / (h - 1);
    int actual_ymax = label_ymax + static_cast<int>(y_per_row / 2);
    int actual_ymin = label_ymin - static_cast<int>(y_per_row / 2);
    // 2. Print Y axis ticks
    for (int i = 0; i < h; i++)
    {
        char buf[10];
        snprintf(
            buf, sizeof(buf), "%-6d",
            static_cast<int>(Lerp(label_ymax, label_ymin, i * 1.0f / (h - 1))));
        mvwaddstr(win, i + y0, x0 + w - RIGHT_MARGIN + 1, buf);
        mvwaddch(win, i + y0, x0, '-');
        mvwaddch(win, i + y0, x0 + w - RIGHT_MARGIN, '-');
    }
    // 3. Go through the historical data and draw on the canvas
    for (int i = 0;
         i < std::min(static_cast<int>(data.size()), w - RIGHT_MARGIN - 1); i++)
    {
        float value = data[i];
        // antialiasing: todo for now
        // float value1 = value; // value1 is 1 column to the right
        // if (i > 0) value1 = data[i-1];
        int x = x0 + w - i - RIGHT_MARGIN - 1;
        float t = Map(value, actual_ymin, actual_ymax, 0, h, true);
        int row = static_cast<int>(t);
        float remaining = t - row;
        char ch; // Last filling character
        if (remaining >= 0.66f)
        {
            ch = ':';
        }
        else if (remaining >= 0.33f)
        {
            ch = '.';
        }
        else
        {
            ch = ' ';
        }
        int y = y0 + h - row - 1;
        mvwaddch(win, y, x, ch);
        for (int j = 0; j < row; j++)
        {
            mvwaddch(win, y + j + 1, x, ':');
        }
    }
}

void SummaryView::UpdateDBusTopStatistics(DBusTopStatistics* stat)
{
    if (!stat)
        return;
    float interval_secs = stat->seconds_since_last_sample_;
    if (interval_secs == 0)
    {
        interval_secs = GetSummaryIntervalInMillises() / 1000.0f;
    }
    // Per-second
    method_call_ = stat->num_mc_ / interval_secs;
    method_return_ = stat->num_mr_ / interval_secs;
    signal_ = stat->num_sig_ / interval_secs;
    error_ = stat->num_error_ / interval_secs;
    total_ = stat->num_messages_ / interval_secs;
    g_bargraph->AddValue(total_);
}

std::string Ellipsize(const std::string& s, int len_limit)
{
    if (len_limit <= 3)
        return s.substr(0, len_limit);
    if (static_cast<int>(s.size()) < len_limit)
    {
        return s;
    }
    else
    {
        return s.substr(0, len_limit - 3) + "...";
    }
}

void SummaryView::Render()
{
    // Draw text
    werase(win);
    wattron(win, A_BOLD | A_UNDERLINE);
    mvwaddstr(win, 1, 1, "Message Type          | msg/s");
    wattrset(win, 0);
    const int xend = 30;
    std::string s;
    s = FloatToString(method_call_);
    mvwaddstr(win, 2, 1, "Method Call");
    mvwaddstr(win, 2, xend - s.size(), s.c_str());
    s = FloatToString(method_return_);
    mvwaddstr(win, 3, 1, "Method Return ");
    mvwaddstr(win, 3, xend - s.size(), s.c_str());
    s = FloatToString(signal_);
    mvwaddstr(win, 4, 1, "Signal");
    mvwaddstr(win, 4, xend - s.size(), s.c_str());
    s = FloatToString(error_);
    mvwaddstr(win, 5, 1, "Error ");
    mvwaddstr(win, 5, xend - s.size(), s.c_str());
    wattron(win, A_UNDERLINE);
    s = FloatToString(total_);
    mvwaddstr(win, 6, 1, "Total");
    mvwaddstr(win, 6, xend - s.size(), s.c_str());
    wattroff(win, A_UNDERLINE);
    wattrset(win, 0);
    // Draw history bar graph
    HistoryBarGraph(win, rect, g_bargraph);
    DrawBorderIfNeeded();
    wrefresh(win);
}

void SensorDetailView::Render()
{
    werase(win);
    // If some sensor is focused, show details regarding that sensor
    if (state == SensorList)
    { // Otherwise show the complete list
        const int ncols = DispSensorsPerRow(); // Number of columns in viewport
        const int nrows = DispSensorsPerColumn(); // # rows in viewport
        int sensors_per_page = nrows * ncols;
        // Just in case the window gets invisibly small
        if (sensors_per_page < 1)
            return;
        int num_sensors = sensor_ids_.size();
        int total_num_columns = (num_sensors - 1) / nrows + 1;
        bool is_cursor_out_of_view = false;
        if (idx0 > choice_ || idx1 <= choice_)
        {
            is_cursor_out_of_view = true;
        }
        if (idx0 == -999 || idx1 == -999)
        {
            is_cursor_out_of_view = true;
        }
        if (is_cursor_out_of_view)
        {
            idx0 = 0, idx1 = sensors_per_page;
        }
        while (idx1 <= choice_)
        {
            idx0 += nrows;
            idx1 += nrows;
        }
        const int y0 = 2; // to account for the border and info line
        const int x0 = 4; // to account for the left overflow marks
        int y = y0, x = x0;
        for (int i = 0; i < sensors_per_page; i++)
        {
            int idx = idx0 + i;
            if (idx < static_cast<int>(sensor_ids_.size()))
            {
                if (idx == choice_)
                {
                    wattrset(win, A_REVERSE);
                }
                std::string s = sensor_ids_[idx];
                while (static_cast<int>(s.size()) < col_width)
                {
                    s.push_back(' ');
                }
                mvwprintw(win, y, x, s.c_str());
                wattrset(win, 0);
            }
            else
                break;
            y++;
            if (i % nrows == nrows - 1)
            {
                y = y0;
                x += col_width + h_spacing;
            }
        }
        // Print overflow marks to the right of the screen
        for (int i = 0; i < nrows; i++)
        {
            int idx = idx0 + sensors_per_page + i;
            if (idx < num_sensors)
            {
                mvwaddch(win, y0 + i, x, '>');
            }
        }
        // Print overflow marks to the left of the screen
        for (int i = 0; i < nrows; i++)
        {
            int idx = idx0 - nrows + i;
            if (idx >= 0)
            {
                mvwaddch(win, y0 + i, 2, '<');
            }
        }
        // idx1 is one past the visible range, so no need to +1
        const int col0 = idx0 / nrows + 1, col1 = idx1 / nrows;
        mvwprintw(win, 1, 2, "Columns %d-%d of %d", col0, col1,
                  total_num_columns);
        mvwprintw(win, 1, rect.w - 15, "%d sensors", sensor_ids_.size());
    }
    else if (state == SensorDetail)
    {
        // sensor_ids_ is the cached list of sensors, it should be the same size
        // as the actual number of sensors in the snapshot
        mvwprintw(win, 1, 2, "Details of sensor %s", curr_sensor_id_.c_str());
        mvwprintw(win, 1, rect.w - 15, "Sensor %d/%u", choice_ + 1,
                  sensor_ids_.size()); // 1-based
        std::vector<Sensor*> sensors =
            g_sensor_snapshot->FindSensorsBySensorID(curr_sensor_id_);
        const int N = static_cast<int>(sensors.size());
        const int w = rect.w - 5;
        mvwprintw(win, 3, 2, "There are %d sensors with the name %s", N,
                  curr_sensor_id_.c_str());
        int y = 5;
        int x = 2;
        if (N > 0)
        {
            for (int j = 0; j < N; j++)
            {
                Sensor* sensor = sensors[j];
                mvwprintw(win, y, x, "%d/%d", j + 1, N);
                char buf[200];
                snprintf(buf, sizeof(buf), "DBus Service    : %s",
                         sensor->ServiceName().c_str());
                y += DrawTextWithWidthLimit(win, buf, y, x, w, "/");
                snprintf(buf, sizeof(buf), "DBus Connection : %s",
                         sensor->ConnectionName().c_str());
                y += DrawTextWithWidthLimit(win, buf, y, x, w, "/");
                snprintf(buf, sizeof(buf), "DBus Object Path: %s",
                         sensor->ObjectPath().c_str());
                y += DrawTextWithWidthLimit(win, buf, y, x, w, "/");
                y++;
            }
        }
        else
        {
            mvwprintw(win, y, x, "Sensor details not found");
        }
    }
    DrawBorderIfNeeded();
    wrefresh(win);
}

DBusStatListView::DBusStatListView() : DBusTopWindow()
{
    horizontal_pan = 0;
    menu1_ = new ArrowKeyNavigationMenu(this);
    menu2_ = new ArrowKeyNavigationMenu(this);
    // Load all available field names
    std::set<std::string> inactive_fields;
    std::set<std::string> active_fields;
    const int N = static_cast<int>(sizeof(FieldNames) / sizeof(FieldNames[0]));
    for (int i = 0; i < N; i++)
    {
        inactive_fields.insert(FieldNames[i]);
    }
    for (const std::string& s :
         dbus_top_analyzer::g_dbus_statistics.GetFieldNames())
    {
        inactive_fields.erase(s);
        active_fields.insert(s);
    }
    for (const std::string& s : inactive_fields)
    {
        menu1_->AddItem(s);
    }
    for (const std::string& s : active_fields)
    {
        menu2_->AddItem(s);
    }
    is_showing_menu_ = false;
    curr_menu_state_ = LeftSide;
    menu_h_ = 6;
    menu_w_ = 24; // Need at least 2*padding + 15 for enough space, see menu.hpp
    menu_margin_ = 6;
}

void DBusStatListView::OnKeyDown(const std::string& key)
{
    {
        ArrowKeyNavigationMenu* m = nullptr;
        switch (curr_menu_state_)
        {
            case LeftSide:
            {
                if (key == "up")
                {
                    menu1_->OnKeyDown("up");
                }
                else if (key == "down")
                {
                    menu1_->OnKeyDown("down");
                }
                else if (key == "right")
                {
                    SetMenuState(RightSide);
                }
                else if (key == "enter")
                {
                    SetMenuState(Hidden);
                }
                else if (key == "space")
                {
                    std::string ch;
                    if (menu1_->RemoveHighlightedItem(&ch))
                    {
                        menu2_->AddItem(ch);
                    }
                }
                break;
            }
            case RightSide:
            {
                if (key == "up")
                {
                    menu2_->OnKeyDown("up");
                }
                else if (key == "down")
                {
                    menu2_->OnKeyDown("down");
                }
                else if (key == "left")
                {
                    SetMenuState(LeftSide);
                }
                else if (key == "enter")
                {
                    SetMenuState(Hidden);
                }
                else if (key == "space")
                {
                    std::string ch;
                    if (menu2_->RemoveHighlightedItem(&ch))
                    {
                        menu1_->AddItem(ch);
                    }
                }
                break;
            }
            case Hidden:
            {
                if (key == "enter")
                {
                    switch (last_menu_state_)
                    {
                        case LeftSide:
                        case RightSide:
                            SetMenuState(last_menu_state_);
                            break;
                        default:
                            SetMenuState(LeftSide);
                    }
                }
                else if (key == "left")
                { // scroll the "scroll bar" to the left
                    --horizontal_pan;
                }
                else if (key == "right")
                {
                    ++horizontal_pan;
                }
                break;
            }
        }
        if (m)
        {
            if (key == "up")
            {
                m->OnKeyDown(key);
            }
            else if (key == "down")
            {
                m->OnKeyDown(key);
            }
            else if (key == "left")
            {
                m->OnKeyDown("left");
            }
            else if (key == "right")
            {
                m->OnKeyDown("right");
            }
        }
    }
    Render();
}

void DBusStatListView::Render()
{
    werase(win);
    int num_lines_shown = rect.h - 3;
    if (curr_menu_state_ == LeftSide || curr_menu_state_ == RightSide)
    {
        menu1_->Render();
        menu2_->Render();
        num_lines_shown -= (menu_h_ + 3);
        // Draw the arrow
        const int x1 = menu1_->rect_.x;
        const int h1 = menu1_->rect_.h;
        const int x2 = menu2_->rect_.x;
        const int w2 = menu2_->rect_.w;
        const int y1 = menu1_->rect_.y;
        const int arrow_x = (x1 + x2 + w2) / 2 - 2;
        const int arrow_y = y1 + 2;
        const int caption_x = x1;
        const int caption_y = y1 + h1;
        for (int x = 1; x < rect.w - 1; x++)
        {
            mvwaddch(win, y1 - 3, x, '-');
        }
        mvwprintw(win, y1 - 3, arrow_x - 8, "Press [Enter] to show/hide");
        mvwprintw(win, y1 - 2, caption_x - 5,
                  "DBus fields for aggregating and sorting results:");
        if (curr_menu_state_ == LeftSide)
        {
            mvwprintw(win, y1 - 1, x1 - 4, "--[ Available Fields ]--");
            mvwprintw(win, y1 - 1, x2 - 4, "--- Active Fields ---");
        }
        else
        {
            mvwprintw(win, y1 - 1, x1 - 4, "--- Available Fields ---");
            mvwprintw(win, y1 - 1, x2 - 4, "--[ Active Fields ]--");
        }
        if (curr_menu_state_ == LeftSide)
        {
            mvwprintw(win, arrow_y, arrow_x, "-->");
            mvwprintw(win, caption_y, caption_x,
                      "Press [Space] to move to the right");
        }
        else
        {
            mvwprintw(win, arrow_y, arrow_x, "<--");
            mvwprintw(win, caption_y, caption_x,
                      "Press [Space] to move to the left");
        }
    }
    std::vector<std::string> headers = {"Msg/s"}; // Mug's is always present
    std::vector<int> widths = {8};
    std::vector<std::string> agg_headers = g_dbus_statistics->GetFieldNames();
    std::vector<int> agg_widths = g_dbus_statistics->GetFieldPreferredWidths();
    headers.insert(headers.end(), agg_headers.begin(), agg_headers.end());
    widths.insert(widths.end(), agg_widths.begin(), agg_widths.end());
    std::vector<int> xs;
    int curr_x = 2 - horizontal_pan;
    for (const int w : widths)
    {
        xs.push_back(curr_x);
        curr_x += w;
    }
    const int N = headers.size();

    wattron(win, A_BOLD | A_UNDERLINE);
    for (int i = 0; i < N; i++)
    {
        std::string s = headers[i];
        // 1 char outside boundary = start printing from the second character,
        // etc
        int x = xs[i];
        if (x < 0)
        {
            if (-x < static_cast<int>(s.size()))
            {
                s = s.substr(-x);
            }
            else
                s = "";
            x = 0;
        }
        mvwaddstr(win, 1, x, s.c_str());
    }
    wattrset(win, 0);
    std::vector<std::vector<std::string>> all_keys;
    std::map<std::vector<std::string>, int> stats_snapshot =
        g_dbus_statistics->StatsSnapshot();
    for (std::map<std::vector<std::string>, int>::iterator itr =
             stats_snapshot.begin();
         itr != stats_snapshot.end(); itr++)
    {
        all_keys.push_back(itr->first);
    }
    std::sort(all_keys.begin(), all_keys.end());
    // Minus 2 because of "msgs/s" and "+"
    const int num_fields = N - 1;
    float interval_secs = g_dbus_statistics->seconds_since_last_sample_;
    if (interval_secs == 0)
    {
        interval_secs = GetSummaryIntervalInMillises() / 1000.0f;
    }
    // Key is sender, destination, interface, path, etc
    for (int i = 0, j = 0;
         i < static_cast<int>(all_keys.size()) && j < num_lines_shown; i++, j++)
    {
        std::string s;
        int x;
        const std::vector<std::string> key = all_keys[i];
        for (int j = 0; j < num_fields; j++)
        {
            x = xs[j + 1];
            s = key[j];
            // Determine column width limit for this particular column
            int col_width = 100;
            if (j < num_fields - 1)
            {
                col_width = xs[j + 2] - xs[j + 1] - 1;
            }
            s = Ellipsize(s, col_width);
            if (x < 0)
            {
                if (-x < static_cast<int>(s.size()))
                    s = s.substr(-x);
                else
                    s = "";
                x = 0;
            }
            // Trim if string overflows to the right
            if (x + static_cast<int>(s.size()) > rect.w)
            {
                s = s.substr(0, rect.w - x);
            }
            mvwaddstr(win, 2 + i, x, s.c_str());
        }
        // Number of messages per second
        int num_msgs = stats_snapshot[key];
        float num_msgs_per_sec = num_msgs / interval_secs;
        x = xs[0];
        s = FloatToString(num_msgs_per_sec);
        if (x < 0)
        {
            if (-x < static_cast<int>(s.size()))
                s = s.substr(-x);
            else
                s = "";
            x = 0;
        }
        mvwaddstr(win, 2 + i, x, s.c_str());
    }
    DrawBorderIfNeeded();
    wrefresh(win);
}

void DBusStatListView::OnResize(int win_w, int win_h)
{
    rect.y = 8 - MARGIN_BOTTOM;
    rect.w = win_w - (win_w / 2) + 1; // Perfectly overlap on the vertical edge
    rect.x = win_w - rect.w;
    rect.h = win_h - rect.y - MARGIN_BOTTOM;
    const int x0 = rect.w / 2 - menu_w_ - menu_margin_ / 2;
    const int x1 = x0 + menu_margin_ + menu_w_;
    const int menu_y = rect.h - menu_h_;
    menu1_->SetRect(Rect(x0, menu_y, menu_w_, menu_h_)); // Local coordinates
    menu1_->SetOrder(ArrowKeyNavigationMenu::Order::ColumnMajor);
    menu2_->SetRect(Rect(x1, menu_y, menu_w_, menu_h_));
    menu2_->SetOrder(ArrowKeyNavigationMenu::Order::ColumnMajor);
    UpdateWindowSizeAndPosition();
}

std::vector<DBusTopSortField> DBusStatListView::GetSortFields()
{
    std::vector<DBusTopSortField> ret;
    const int N = sizeof(FieldNames) / sizeof(FieldNames[0]);
    for (const std::string& s : menu2_->Items())
    {
        for (int i = 0; i < N; i++)
        {
            if (FieldNames[i] == s)
            {
                ret.push_back(static_cast<DBusTopSortField>(i));
                break;
            }
        }
    }
    return ret;
}