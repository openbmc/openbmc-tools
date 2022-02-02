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

#include "analyzer.hpp"
#include "bargraph.hpp"
#include "main.hpp"
#include "menu.hpp"
#include "sensorhelper.hpp"

#include <ncurses.h>

#include <string>
#include <vector>
constexpr int MARGIN_BOTTOM = 1;
class DBusTopWindow
{
  public:
    DBusTopWindow()
    {
        win = newwin(25, 80, 0, 0); // Default to 80x25, will be updated
        has_border_ = true;
        focused_ = false;
        selectable_ = true;
        visible_ = true;
        maximize_ = false;
    }

    virtual ~DBusTopWindow()
    {}
    virtual void OnKeyDown(const std::string& key) = 0;
    virtual void Render() = 0;
    virtual void OnResize(int win_w, int win_h) = 0;
    void UpdateWindowSizeAndPosition()
    {
        mvwin(win, rect.y, rect.x);
        wresize(win, rect.h, rect.w);
    }

    void DrawBorderIfNeeded()
    {
        if (focused_)
        {
            wborder(win, '*', '*', '*', '*', '*', '*', '*', '*');
        }
        else
        {
            wborder(win, '|', '|', '-', '-', '+', '+', '+', '+');
        }
        wrefresh(win);
    }

    virtual void RecreateWindow()
    {
        delwin(win);
        win = newwin(25, 80, 0, 0);
        UpdateWindowSizeAndPosition();
    }

    virtual std::string GetStatusString() = 0;
    WINDOW* win;
    Rect rect;
    bool has_border_;
    bool focused_;
    bool selectable_;
    bool maximize_;
    bool visible_;
};

class SummaryView : public DBusTopWindow
{
  public:
    SummaryView() : DBusTopWindow()
    {}
    void Render() override;
    void OnResize(int win_w, int win_h) override
    {
        rect.h = 8;
        rect.w = win_w;
        rect.x = 0;
        rect.y = 0;
        UpdateWindowSizeAndPosition();
    }

    void UpdateDBusTopStatistics(DBusTopStatistics* stat);
    void OnKeyDown(const std::string& key) override
    {}
    std::string GetStatusString() override
    {
        return "Summary View";
    }

  private:
    float method_call_, method_return_, signal_, error_, total_;
};

class InventoryView : public DBusTopWindow
{
  public:
    InventoryView() : DBusTopWindow()
    {
        h_padding = 2;
        h_spacing = 3;
        col_width = 15;
        idx0 = idx1 = -999;
        state = SensorList;
        sensors_menu_ = new ArrowKeyNavigationMenu(this);
        sensors_menu_->show_overflow_marks = true;
    }

    void Render() override;
    int DispSensorsPerColumn()
    {
        return rect.h - 3;
    }

    int DispSensorsPerRow()
    {
        int ncols = 0;
        while (true)
        {
            int next = ncols + 1;
            int w = 2 * h_padding + col_width * next;
            if (next > 1)
                w += (next - 1) * h_spacing;
            if (w <= rect.w - 2)
            {
                ncols = next;
            }
            else
            {
                break;
            }
        }
        return ncols;
    }

    void OnKeyDown(const std::string& key) override
    {
        if (state == SensorList)
        { // Currently in sensor list
            if (key == "right")
            {
                // MoveChoiceCursorHorizontally(1);
                sensors_menu_->OnKeyDown(key);
            }
            else if (key == "left")
            {
                // MoveChoiceCursorHorizontally(-1);
                sensors_menu_->OnKeyDown(key);
            }
            else if (key == "up")
            {
                // MoveChoiceCursor(-1, true);
                sensors_menu_->OnKeyDown(key);
            }
            else if (key == "down")
            {
                // MoveChoiceCursor(1, true);
                sensors_menu_->OnKeyDown(key);
            }
            else if (key == "enter")
            {
                if (sensors_menu_->choice_ != INVALID)
                {
                    MoveChoiceCursor(0, true);
                    state = SensorDetail;
                }
            }
            else if (key == "escape")
            {
                // choice_ = -999;
                sensors_menu_->Deselect();
            }
        }
        else if (state == SensorDetail)
        { // Currently focusing on a sensor
            if (key == "right" || key == "down")
            {
                MoveChoiceCursor(1, true);
            }
            else if (key == "left" || key == "up")
            {
                MoveChoiceCursor(-1, true);
            }
            else if (key == "escape")
            {
                state = SensorList;
            }
        }

        Render(); // This window is already on top, redrawing won't corrupt
    }

    void MoveChoiceCursor(int delta, bool wrap_around = true)
    {
        const int ns = sensor_ids_.size();
        if (ns < 1)
            return;
        // First of all, if cursor is inactive, activate it
        int& choice = sensors_menu_->choice_;
        if (choice == -999)
        {
            if (delta > 0)
            {
                choice = 0;
                curr_sensor_id_ = sensor_ids_[0];
                return;
            }
            else
            {
                choice = ns - 1;
                curr_sensor_id_ = sensor_ids_.back();
                return;
            }
        }
        int choice_next = choice + delta;
        while (choice_next >= ns)
        {
            if (wrap_around)
            {
                choice_next -= ns;
            }
            else
            {
                choice_next = ns - 1;
            }
        }
        while (choice_next < 0)
        {
            if (wrap_around)
            {
                choice_next += ns;
            }
            else
            {
                choice_next = 0;
            }
        }
        choice = choice_next;
        curr_sensor_id_ = sensor_ids_[choice];
    }

    // Make a copy of the SensorSnapshot object for display usage
    void UpdateSensorSnapshot(SensorSnapshot* snapshot)
    {
        int& choice = sensors_menu_->choice_;
        sensor_snapshot_ = *snapshot;
        std::string old_sensor_id = "";
        if (choice != -999)
        {
            old_sensor_id = sensor_ids_[choice];
        }
        std::vector<std::string> new_sensors =
            snapshot->GetDistinctSensorNames();
        if (new_sensors == sensor_ids_)
        {
            return; // Nothing is changed
        }

        int old_ch = sensors_menu_->choice_;
        sensors_menu_->RemoveAllItems();
        for (const std::string& sn : new_sensors)
        {
            sensors_menu_->AddItem(sn);
        }
        if (old_ch < int(new_sensors.size()))
        {
            sensors_menu_->choice_ = old_ch;
        }

        // Assume changed
        sensor_ids_ = new_sensors;
        choice = -999;
        for (int i = 0; i < static_cast<int>(new_sensors.size()); i++)
        {
            if (new_sensors[i] == old_sensor_id)
            {
                choice = i;
                break;
                curr_sensor_id_ = sensor_ids_[choice];
            }
        }
    }

    void OnResize(int win_w, int win_h) override
    {
        rect.x = 0;
        rect.y = 8 - MARGIN_BOTTOM;
        rect.w = win_w / 2;
        rect.h = win_h - rect.y - MARGIN_BOTTOM;
        UpdateWindowSizeAndPosition();
        sensors_menu_->SetRect(Rect(1, 2, rect.w - 2, rect.h - 3));
    }

    std::vector<std::string> sensor_ids_;
    // We need to keep track of the currently-selected sensor ID because
    // the sensor ID might theoretically become invalidated at any moment, and
    // we should allow the UI to show an error gracefully in that case.
    std::string curr_sensor_id_;
    int h_padding;
    int h_spacing;
    int col_width;
    int idx0, idx1; // Range of sensors on display
    enum State
    {
        ModeSelection,
        SensorList,
        SensorDetail,
    };

    State state;
    std::string GetStatusString() override;
    SensorSnapshot sensor_snapshot_;
    ArrowKeyNavigationMenu* sensors_menu_;
};

class DBusStatListView : public DBusTopWindow
{
  public:
    DBusStatListView();
    void Render() override;
    void OnResize(int win_w, int win_h) override;
    void OnKeyDown(const std::string& key) override;
    int horizontal_pan_;
    int menu_h_, menu_w_, menu_margin_;
    ArrowKeyNavigationMenu* menu1_;
    ArrowKeyNavigationMenu* menu2_;
    int highlight_col_idx_; // Currently highlighted column
    int row_idx_;           // Currently highlighted row

    int sort_col_idx_; // Column used for sorting
    enum SortOrder
    {
        Ascending,
        Descending,
    };
    SortOrder sort_order_;

    int disp_row_idx_;    // From which row to start displaying? (essentially a
                          // vertical scroll bar)
    int last_choices_[2]; // Last choice index on either side
    enum MenuState
    {
        Hidden,
        LeftSide,  // When the user is choosing an entry on the left side
        RightSide, // When the user is choosing an entry on the right side
    };

    std::vector<DBusTopSortField> GetSortFields();
    MenuState curr_menu_state_, last_menu_state_;
    std::string GetStatusString() override;
    void RecreateWindow()
    {
        delwin(win);
        win = newwin(25, 80, 0, 0);
        menu1_->win_ = win;
        menu2_->win_ = win;
        UpdateWindowSizeAndPosition();
    }

  private:
    void SetMenuState(MenuState s)
    {
        last_menu_state_ = curr_menu_state_;
        // Moving out from a certain side: save the last choice of that side
        switch (curr_menu_state_)
        {
            case LeftSide:
                if (s == RightSide)
                {
                    last_choices_[0] = menu1_->Choice();
                    menu1_->Deselect();
                }
                break;
            case RightSide:
                if (s == LeftSide)
                {
                    last_choices_[1] = menu2_->Choice();
                    menu2_->Deselect();
                }
                break;
            default:
                break;
        }
        // Moving into a certain side: save the cursor
        switch (s)
        {
            case LeftSide:
                if (!menu1_->Empty())
                {
                    menu1_->SetChoiceAndConstrain(last_choices_[0]);
                }
                break;
            case RightSide:
                if (!menu2_->Empty())
                {
                    menu2_->SetChoiceAndConstrain(last_choices_[1]);
                }
                break;
            default:
                break;
        }
        curr_menu_state_ = s;
    }
    void PanViewportOrMoveHighlightedColumn(const int delta_x);
    // ColumnHeaders and ColumnWidths are the actual column widths used for
    // display. They are "msg/s" or "I2c/s" prepended to the chosen set of
    // fields.
    std::vector<std::string> ColumnHeaders();
    std::vector<int> ColumnWidths();
    // X span, for checking visibility
    std::pair<int, int> GetXSpanForColumn(const int col_idx);
    bool IsXSpanVisible(const std::pair<int, int>& xs,
                        const int tolerance); // uses horizontal_pan_
    std::vector<std::string> visible_columns_;
    std::unordered_map<std::string, int> column_widths_;
    std::map<std::vector<std::string>, DBusTopComputedMetrics> stats_snapshot_;
};

class FooterView : public DBusTopWindow
{
  public:
    FooterView() : DBusTopWindow()
    {
        selectable_ = false; // Cannot be selected by the tab key
    }

    void OnKeyDown(const std::string& key) override
    {}
    void OnResize(int win_w, int win_h) override
    {
        rect.h = 1;
        rect.w = win_w;
        rect.x = 0;
        rect.y = win_h - 1;
        UpdateWindowSizeAndPosition();
    }

    void Render() override;
    std::string GetStatusString() override
    {
        return "";
    }

    void SetStatusString(const std::string& s)
    {
        status_string_ = s;
    }
    std::string status_string_;
};
