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
#include "histogram.hpp"
#include "menu.hpp"
#include <string.h>
#include <algorithm>

extern SensorSnapshot* g_sensor_snapshot;
extern BarGraph<float>* g_bargraph;
extern DBusTopStatistics* g_dbus_statistics;
extern Histogram<float>* g_histogram;
extern DBusTopWindow* g_current_active_view;
extern const std::string FieldNames[];
extern const int FieldPreferredWidths[];

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
    const int x0 = rect.x, y0 = 2;
    const int w = rect.w - 2 - RIGHT_MARGIN;
    const int h = rect.h - 3; // height of content
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

template <typename T>
void DrawHistogram(WINDOW* win, const Rect& rect, Histogram<T>* histogram)
{
    // const int MARGIN = 7;  // 5 digits margin
    const int LEFT_MARGIN = 7;
    // const int max_bucket_h = histogram->MaxBucketHeight();
    const int H_PAD = 0, V_PAD = 1;
    // x0, x1, y0 and y1 are the bounding box of the contents to be printed
    const int x0 = rect.x + H_PAD;
    const int x1 = rect.x + rect.w - H_PAD;
    const int y0 = rect.y + V_PAD;
    const int y1 = rect.y + rect.h - 1 - V_PAD;
    // Title
    wattron(win, A_BOLD | A_UNDERLINE);
    mvwaddstr(win, y0, x0, "Method Call Time (us) Histogram");
    wattrset(win, 0);
    // x2 is the beginning X of the histogram itself (not containing the margin)
    const int x2 = x0 + LEFT_MARGIN;
    if (histogram->Empty())
    {
        mvwaddstr(win, (y1 + y0) / 2, (x0 + x1) / 2, "(Empty)");
        return;
    }
    histogram->SetBucketCount(x1 - x2 + 1);
    histogram->ComputeHistogram();
    // Draw X axis labels
    char buf[22];
    snprintf(buf, sizeof(buf), "%.2f",
             static_cast<float>(histogram->LowPercentile()));
    mvwaddstr(win, y1, x0 + LEFT_MARGIN, buf);
    snprintf(buf, sizeof(buf), "%.2f",
             static_cast<float>(histogram->HighPercentile()));
    mvwaddstr(win, y1, x1 + 1 - strlen(buf), buf);
    snprintf(buf, sizeof(buf), "%d%%-%d%%",
             static_cast<int>(histogram->LowCumDensity() * 100),
             static_cast<int>(histogram->HighCumDensity() * 100));
    mvwaddstr(win, y1, x0, buf);
    // Draw Y axis labels
    const float hist_ymax = y1 - 1;
    const float hist_ymin = y0 + 1;
    const int max_histogram_h = histogram->MaxBucketHeight();
    if (hist_ymax <= hist_ymin)
        return; // Not enough space for rendering
    if (max_histogram_h <= 0)
        return;
    bool LOG_TRANSFORM = true;
    float lg_maxh = 0;
    if (LOG_TRANSFORM)
    {
        lg_maxh = log(max_histogram_h);
    }
    for (int y = hist_ymin; y <= hist_ymax; y++)
    {
        // There are (hist_ymax - hist_ymin + 1) divisions
        float fullness;
        fullness = (hist_ymax - y + 1) * 1.0f / (hist_ymax - hist_ymin + 1);
        int h;
        if (!LOG_TRANSFORM)
        {
            h = static_cast<int>(max_histogram_h * fullness);
        }
        else
        {
            h = static_cast<int>(exp(fullness * lg_maxh));
        }
        snprintf(buf, sizeof(buf), "%6d-", h);
        mvwaddstr(win, y, x0 + LEFT_MARGIN - strlen(buf), buf);
    }
    const int bar_height = hist_ymax - hist_ymin + 1; // Height of a full bar
    for (int x = x2, bidx = 0; x <= x1; x++, bidx++)
    {
        int h = histogram->BucketHeight(bidx);
        float lines_visible;
        if (!LOG_TRANSFORM)
        {
            lines_visible = h * 1.0f / max_histogram_h * bar_height;
        }
        else
        {
            if (h <= 0)
                lines_visible = 0;
            else
                lines_visible = log(h) * 1.0f / lg_maxh * bar_height;
        }
        // The histogram's top shall start from this line
        int y = hist_ymax - static_cast<int>(lines_visible);
        float y_frac = lines_visible - static_cast<int>(lines_visible);
        char ch; // Last filling character
        if (y >= hist_ymin)
        { // At the maximum bucket the Y overflows, so skip
            if (y_frac >= 0.66f)
            {
                ch = ':';
            }
            else if (y_frac >= 0.33f)
            {
                ch = '.';
            }
            else
            {
                if (y < hist_ymax)
                {
                    ch = ' ';
                }
                else
                {
                    if (y_frac > 0)
                    {
                        ch =
                            '.'; // Makes long-tailed distribution easier to see
                    }
                }
            }
            mvwaddch(win, y, x, ch);
        }
        y++;
        for (; y <= hist_ymax; y++)
        {
            mvwaddch(win, y, x, ':');
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
    if (!visible_)
        return;
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
    Rect bargraph_rect = rect;
    const int bargraph_x = 64;
    bargraph_rect.x += bargraph_x;
    bargraph_rect.w -= bargraph_x;
    HistoryBarGraph(win, bargraph_rect, g_bargraph);
    // Draw histogram
    Rect histogram_rect = rect;
    histogram_rect.x += 32;
    histogram_rect.w = bargraph_rect.x - histogram_rect.x - 3;
    DrawHistogram(win, histogram_rect, g_histogram);
    // Draw border between summary and histogram
    for (int y = bargraph_rect.y; y <= bargraph_rect.y + bargraph_rect.h; y++)
    {
        mvwaddch(win, y, histogram_rect.x - 1, '|');
        mvwaddch(win, y, bargraph_rect.x - 1, '|');
    }
    DrawBorderIfNeeded();
    wrefresh(win);
}

void SensorDetailView::Render()
{
    werase(win);
    if (!visible_)
        return;
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
        if (idx0 == INVALID || idx1 == INVALID)
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
                if (static_cast<int>(s.size()) > col_width) {
                    s = s.substr(0, col_width - 2) + "..";
                } else {
                    while (static_cast<int>(s.size()) < col_width)
                    {
                        s.push_back(' ');
                    }
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

std::string SensorDetailView::GetStatusString()
{
    if (state == SensorList)
    {
        return "[Arrow Keys]=Move Cursor [Q]=Deselect [Enter]=Show Sensor "
               "Detail";
    }
    else
    {
        return "[Arrow Keys]=Cycle Through Sensors [Esc/Q]=Exit";
    }
}

DBusStatListView::DBusStatListView() : DBusTopWindow()
{
    highlight_col_idx_ = 0;
    sort_col_idx_ = 0;
    sort_order_ = SortOrder::Ascending;
    horizontal_pan_ = 0;
    row_idx_ = INVALID;
    disp_row_idx_ = 0;
    horizontal_pan_ = 0;
    menu1_ = new ArrowKeyNavigationMenu(this);
    menu2_ = new ArrowKeyNavigationMenu(this);
    // Load all available field names
    std::set<std::string> inactive_fields;
    std::set<std::string> active_fields;

    // Default choice of field names
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
    for (int i = 0; i < N; i++)
    {
    const std::string s = FieldNames[i];
        if (inactive_fields.count(s) > 0)
        {
            menu1_->AddItem(s);
        }
        else
        {
            menu2_->AddItem(s);
        }
    }
    
    curr_menu_state_ = LeftSide;
    menu_h_ = 5;
    menu_w_ = 24; // Need at least 2*padding + 15 for enough space, see menu.hpp
    menu_margin_ = 6;
    // Populate preferred column widths
    for (int i = 0; i < N; i++)
    {
        column_widths_[FieldNames[i]] = FieldPreferredWidths[i];
    }
}

std::pair<int, int> DBusStatListView::GetXSpanForColumn(const int col_idx)
{
    std::vector<int> cw = ColumnWidths();
    if (col_idx < 0 || col_idx >= static_cast<int>(cw.size()))
    {
        return std::make_pair(INVALID, INVALID);
    }
    int x0 = 0, x1 = 0;
    for (int i = 0; i < col_idx; i++)
    {
        if (i > 0)
        {
            x0 += cw[i];
        }
    }
    x1 = x0 + cw[col_idx] - 1;
    return std::make_pair(x0, x1);
}

// If tolerance > 0, consider overlap before 2 intervals intersect
// If tolerance ==0, consider overlap if 2 intervals exactly intersect
// If tolerance < 0, consider overlap if Minimal Translate Distance is >=
// -threshold
bool IsSpansOverlap(const std::pair<int, int>& s0,
                    const std::pair<int, int>& s1, int tolerance)
{
    if (tolerance >= 0)
    {
        if (s0.second < s1.first - tolerance)
            return false;
        else if (s1.second < s0.first - tolerance)
            return false;
        else
            return true;
    }
    else
    {
        // Compute overlapping distance
        std::vector<std::pair<int, int>> tmp(
            4); // [x, 1] means the start of interval
                // [x,-1] means the end of interval
        tmp[0] = std::make_pair(s0.first, 1);
        tmp[1] = std::make_pair(s0.second, -1);
        tmp[2] = std::make_pair(s1.first, 1);
        tmp[3] = std::make_pair(s1.second, -1);
        std::sort(tmp.begin(), tmp.end());
        int overlap_x0 = -INVALID, overlap_x1 = -INVALID;
        int idx = 0;
        const int N = static_cast<int>(tmp.size());
        int level = 0;
        while (idx < N)
        {
            const int x = tmp[idx].first;
            while (idx < N && x == tmp[idx].first)
            {
                level += tmp[idx].second;
                idx++;
            }
            // The starting position of the overlap
            if (level == 2)
            {
                overlap_x0 = idx - 1;
            }
            // The ending position of the overlap
            if (overlap_x0 != -INVALID && level < 2 && overlap_x1 == -INVALID)
            {
                overlap_x1 = idx - 1;
            }
        }
        const int overlap_length = overlap_x1 - overlap_x0 + 1;
        if (overlap_length >= -tolerance)
            return true;
        else
            return false;
    }
}

bool DBusStatListView::IsXSpanVisible(const std::pair<int, int>& xs,
                                      int tolerance)
{
    const std::pair<int, int> vxs = {horizontal_pan_, horizontal_pan_ + rect.w};
    return IsSpansOverlap(xs, vxs, tolerance);
}
std::vector<std::string> DBusStatListView::ColumnHeaders()
{
    return visible_columns_;
}

std::vector<int> DBusStatListView::ColumnWidths()
{
    std::vector<int> widths = {8}; // for "Msg/s"
    std::vector<std::string> agg_headers = visible_columns_;
    std::vector<int> agg_widths(agg_headers.size(), 0);
    for (int i = 0; i < static_cast<int>(agg_headers.size()); i++)
    {
        agg_widths[i] = column_widths_[agg_headers[i]];
    }
    widths.insert(widths.end(), agg_widths.begin(), agg_widths.end());
    return widths;
}

// Coordinate systems are in world space, +x faces to the right
// Viewport:            [horizontal_pan_,   horizontal_pan_ + rect.w]
// Contents:  [  column_width[0]  ][  column_width[1] ][  column_width[2]  ]
void DBusStatListView::PanViewportOrMoveHighlightedColumn(const int delta_x)
{
    // If the column to the left is visible, highlight it
    const int N = static_cast<int>(ColumnHeaders().size());
    bool col_idx_changed = false;
    if (delta_x < 0)
    { // Pan left
        if (highlight_col_idx_ > 0)
        {
            std::pair<int, int> xs_left =
                GetXSpanForColumn(highlight_col_idx_ - 1);
            if (IsXSpanVisible(xs_left, -1))
            {
                highlight_col_idx_--;
                col_idx_changed = true;
            }
        }
        if (!col_idx_changed)
        {
            horizontal_pan_ += delta_x;
        }
    }
    else if (delta_x > 0)
    { // Pan right
        if (highlight_col_idx_ < N - 1)
        {
            std::pair<int, int> xs_right =
                GetXSpanForColumn(highlight_col_idx_ + 1);
            if (IsXSpanVisible(xs_right, -1))
            {
                highlight_col_idx_++;
                col_idx_changed = true;
            }
        }
        if (!col_idx_changed)
        {
            horizontal_pan_ += delta_x;
        }
    }
}

void DBusStatListView::OnKeyDown(const std::string& key)
{
    {
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
                {
                    PanViewportOrMoveHighlightedColumn(-2);
                }
                else if (key == "right")
                {
                    PanViewportOrMoveHighlightedColumn(2);
                }
                else if (key == "up")
                {
                    disp_row_idx_--;
                    if (disp_row_idx_ < 0)
                    {
                        disp_row_idx_ = 0;
                    }
                }
                else if (key == "down")
                {
                    disp_row_idx_++;
                    const int N = static_cast<int>(stats_snapshot_.size());
                    if (disp_row_idx_ >= N)
                    {
                        disp_row_idx_ = N - 1;
                    }
                }
                else if (key == "a")
                {
                    sort_order_ = SortOrder::Ascending;
                    sort_col_idx_ = highlight_col_idx_;
                    break;
                }
                else if (key == "d")
                {
                    sort_order_ = SortOrder::Descending;
                    sort_col_idx_ = highlight_col_idx_;
                    break;
                }
                break;
            }
        }
    }
    Render();
}

// Window C
void DBusStatListView::Render()
{
    werase(win);
    if (!visible_)
        return;
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
    std::vector<std::string> headers;
    std::vector<int> widths;
    visible_columns_ = g_dbus_statistics->GetFieldNames();
    std::vector<std::string> agg_headers = visible_columns_;
    std::vector<int> agg_widths(agg_headers.size(), 0);
    for (int i = 0; i < static_cast<int>(agg_headers.size()); i++)
    {
        agg_widths[i] = column_widths_[agg_headers[i]];
    }
    headers.insert(headers.end(), agg_headers.begin(), agg_headers.end());
    widths.insert(widths.end(), agg_widths.begin(), agg_widths.end());
    std::vector<int> xs;
    int curr_x = 2 - horizontal_pan_;
    for (const int w : widths)
    {
        xs.push_back(curr_x);
        curr_x += w;
    }
    const int N = headers.size();
    // Bound col_idx_
    if (highlight_col_idx_ >= N)
    {
        highlight_col_idx_ = N - 1;
    }
    // Render column headers
    for (int i = 0; i < N; i++)
    {
        std::string s = headers[i];
        // 1 char outside boundary = start printing from the second character,
        // etc

        // Print "<" for Ascending order (meaning: row 0 < row 1 < row 2 ... )
        // Print ">" for Descending order (meaning: row 0 > row 1 > row 2 ... )
        if (sort_col_idx_ == i)
        {
            if (sort_order_ == SortOrder::Ascending)
            {
                s.push_back('<');
            }
            else
            {
                s.push_back('>');
            }
        }

        // Highlight the "currently-selected column"
        if (highlight_col_idx_ == i)
        {
            wattrset(win, 0);
            wattron(win, A_REVERSE);
        }
        else
        {
            wattrset(win, 0);
            wattron(win, A_UNDERLINE);
        }
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
    // Time since the last update of Window C
    float interval_secs = g_dbus_statistics->seconds_since_last_sample_;
    if (interval_secs == 0)
    {
        interval_secs = GetSummaryIntervalInMillises() / 1000.0f;
    }

    stats_snapshot_ = g_dbus_statistics->StatsSnapshot();
    const int nrows = static_cast<int>(stats_snapshot_.size());
    const std::vector<DBusTopSortField> fields = g_dbus_statistics->GetFields();
    const int ncols = static_cast<int>(fields.size());
    // Merge the list of DBus Message properties & computed metrics together
    std::map<std::vector<std::string>, DBusTopComputedMetrics>::iterator itr =
        stats_snapshot_.begin();
    struct StringOrFloat
    { // Cannot use union so using struct
        std::string s;
        float f;
    };

    // "Stage" the snapshot for displaying in the form of a spreadsheet
    std::vector<std::pair<StringOrFloat, std::vector<std::string>>>
        stats_snapshot_staged;
    const DBusTopSortField sort_field = fields[sort_col_idx_];
    const bool is_sort_key_numeric = DBusTopSortFieldIsNumeric(sort_field);

    for (int i = 0; i < nrows; i++) // One row of cells
    {
        int idx0 = 0; // indexing into the std::vector<string> of each row
        std::vector<std::string> row;

        StringOrFloat sort_key; // The key used for sorting
        for (int j = 0; j < ncols; j++) // one column in the row
        {
            DBusTopSortField field = fields[j];
            // Populate the content of stats_snapshot_staged

            StringOrFloat sof; // Represents this column
            
            // When we haven't used up all
            if (idx0 < static_cast<int>(itr->first.size()))
            {
                sof.s = itr->first[idx0];
            }
            switch (field)
            {
                case kSender:      // string
                case kDestination: // string
                case kInterface:   // string
                case kPath:        // string
                case kMember:      // string
                case kSenderPID:   // numeric
                case kSenderCMD:   // string
                    row.push_back(itr->first[idx0]);
                    idx0++;
                    if (field == kSenderPID)
                    {
                        // Note: attempting to std::atof("(unknown)") on the BMC
                        // will cause hang. And GDB won't show backtrace.
                        if (sof.s == "(unknown)")
                        {
                            if (sort_order_ == Ascending)
                            {
                                sof.f = -1;
                            }
                            else
                            {
                                sof.f = 1e20;
                            }
                        }
                        else
                        {
                            sof.f = std::atof(sof.s.c_str());
                        }
                    }
                    break;
                case kMsgPerSec: // Compute "messages per second"
                {
                    int numbers[] = {
                        itr->second.num_method_calls,
                        itr->second.num_method_returns,
                        itr->second.num_signals,
                        itr->second.num_errors,
                    };
                    int the_sum = 0; // For sorting

                    std::string s; // String representation in the form or
                                   // "1.00/2.00/3.00/4.00"
                    for (int i = 0; i < 4; i++)
                    {
                        the_sum += numbers[i];
                        if (i > 0)
                            s += "/";
                        float per_sec = numbers[i] / interval_secs;
                        s += FloatToString(per_sec);
                    }

                    row.push_back(s);
                    sof.f = the_sum;
                    break;
                }
                case kAverageLatency: // Compute "average Method Call latency"
                    const DBusTopComputedMetrics& m = itr->second;
                    if (m.num_method_calls == 0)
                    {
                        row.push_back("n/a");
                        if (sort_order_ == Ascending)
                        {
                            sof.f = -1; // Put to the top
                        }
                        else
                        {
                            sof.f = 1e20; // Put to the top
                        }
                    }
                    else
                    {
                        float avg_latency_usec =
                            m.total_latency_usec / m.num_method_calls;
                        row.push_back(FloatToString(avg_latency_usec));
                        sof.f = avg_latency_usec;
                    }
                    break;
            }
            if (j == sort_col_idx_)
            {
                sort_key = sof;
            }
        }
        stats_snapshot_staged.push_back(std::make_pair(sort_key, row));
        itr++;
    }
    
    // Sort the "staged snapshot" using the sort_key, using different functions
    // depending on whether sort key is numeric or string
    if (is_sort_key_numeric)
    {
        std::sort(
            stats_snapshot_staged.begin(), stats_snapshot_staged.end(),
            [](const std::pair<StringOrFloat, std::vector<std::string>>& a,
               const std::pair<StringOrFloat, std::vector<std::string>>& b) {
                return a.first.f < b.first.f;
            });
    }
    else
    {
        std::sort(
            stats_snapshot_staged.begin(), stats_snapshot_staged.end(),
            [](const std::pair<StringOrFloat, std::vector<std::string>>& a,
               const std::pair<StringOrFloat, std::vector<std::string>>& b) {
                return a.first.s < b.first.s;
            });
    }
    
    if (sort_order_ == Descending)
    {
        std::reverse(stats_snapshot_staged.begin(),
                     stats_snapshot_staged.end());
    }
    // Minus 2 because of "msgs/s" and "+"
    const int num_fields = N;
    // The Y span of the area for rendering the "spreadsheet"
    const int y0 = 2, y1 = y0 + num_lines_shown - 1;
    // Key is sender, destination, interface, path, etc
    for (int i = 0, shown = 0;
        i + disp_row_idx_ < static_cast<int>(stats_snapshot_staged.size()) &&
        shown < num_lines_shown;
        i++, shown++)
    {
        std::string s;
        int x = 0;
        const std::vector<std::string> key =
            stats_snapshot_staged[i + disp_row_idx_].second;
        for (int j = 0; j < num_fields; j++)
        {
            x = xs[j];
            s = key[j];
            // Determine column width limit for this particular column
            int col_width = 100;
            if (j < num_fields - 1)
            {
                col_width = xs[j + 1] - xs[j] - 1;
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
    }
    // Overflows past the top ...
    if (disp_row_idx_ > 0)
    {
        std::string x = " [+" + std::to_string(disp_row_idx_) + " rows above]";
        mvwaddstr(win, y0, rect.w - static_cast<int>(x.size()) - 1, x.c_str());
    }
    // Overflows past the bottom ...
    const int last_disp_row_idx = disp_row_idx_ + num_lines_shown - 1;
    if (last_disp_row_idx < nrows - 1)
    {
        std::string x = " [+" +
                        std::to_string((nrows - 1) - last_disp_row_idx) +
                        " rows below]";
        mvwaddstr(win, y1, rect.w - static_cast<int>(x.size()) - 1, x.c_str());
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

std::string DBusStatListView::GetStatusString()
{
    if (curr_menu_state_ == LeftSide || curr_menu_state_ == RightSide)
    {
        return "[Enter]=Hide Panel [Space]=Choose Entry [Arrow Keys]=Move "
               "Cursor";
    }
    else
    {
        return "[Enter]=Show Column Select Panel [Arrow Keys]=Pan View";
    }
}

void FooterView::Render()
{
    werase(win);
    const time_t now = time(nullptr);
    const char* date_time = ctime(&now);
    wattrset(win, 0);
    std::string help_info;
    if (g_current_active_view == nullptr)
    {
        help_info = "Press [Tab] to cycle through views";
    }
    else
    {
        help_info = g_current_active_view->GetStatusString();
    }
    mvwaddstr(win, 0, 1, date_time);
    mvwaddstr(win, 0, rect.w - int(help_info.size()) - 1, help_info.c_str());
    wrefresh(win);
}
