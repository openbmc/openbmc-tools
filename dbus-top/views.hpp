<<<<<<< HEAD
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
#include "histogram.hpp"
#include "main.hpp"
#include "menu.hpp"
#include "sensorhelper.hpp"
=======
/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "rect.hpp"
>>>>>>> parent of 6fe55b9 (dbus-top: WIP of all currently-implemented features)

#include <ncurses.h>

#include <string>
#include <vector>

constexpr int MARGIN_BOTTOM = 1;

// A warpper of the ncurses WINDOW with the following added:
// - A method to render its contents
// - Function to accept keystrokes   :todo
// - Code path to deal with the window's specific data  :todo
class DBusTopWindow
{
  public:
    DBusTopWindow()
    {
        win = newwin(25, 80, 0, 0); // Default to 80x25, will be updated
        has_border_ = true;
<<<<<<< HEAD
        focused_ = false;
        selectable_ = true;
        visible_ = true;
        maximaize_= false;
=======
>>>>>>> parent of 6fe55b9 (dbus-top: WIP of all currently-implemented features)
    }

    virtual ~DBusTopWindow()
    {}
    virtual void Render() = 0;
    virtual void OnResize(int win_w, int win_h) = 0;
   
    void UpdateWindowSizeAndPosition()
    {
        mvwin(win, rect.y, rect.x);
        wresize(win, rect.h, rect.w);
    }

    void DrawBorderIfNeeded()
    {
        wborder(win, '|', '|', '-', '-', '+', '+', '+', '+');
        wattrset(win, 0);
        wrefresh(win);
    }

    WINDOW* win;
    Rect rect;
    bool has_border_;
<<<<<<< HEAD
    bool focused_;
    bool selectable_;
    bool visible_;
    bool maximaize_;
=======
>>>>>>> parent of 6fe55b9 (dbus-top: WIP of all currently-implemented features)
};

class SummaryView : public DBusTopWindow
{
  public:
    SummaryView() : DBusTopWindow()
    {}
    void Render() override
    {
        werase(win);
        mvwaddstr(win, 1, 1, "This is window A");
        DrawBorderIfNeeded();
        wrefresh(win);
    }

    void OnResize(int win_w, int win_h) override
    {
        rect.h = 8;
        rect.w = win_w;
        rect.x = 0;
        rect.y = 0;
        UpdateWindowSizeAndPosition();
    }
<<<<<<< HEAD
    
    void UpdateDBusTopStatistics(DBusTopStatistics* stat);
    void OnKeyDown(const std::string& key) override
    {}

  private:
    float method_call_, method_return_, signal_, error_, total_;
=======
>>>>>>> parent of 6fe55b9 (dbus-top: WIP of all currently-implemented features)
};

class SensorDetailView : public DBusTopWindow
{
  public:
    SensorDetailView() : DBusTopWindow()
    {}
    void Render() override
    {
        werase(win);
        mvwaddstr(win, 1, 1, "This is window B");
        DrawBorderIfNeeded();
        wrefresh(win);
    }

    void OnResize(int win_w, int win_h) override
    {
        rect.x = 0;
        rect.y = 8 - MARGIN_BOTTOM;
        rect.w = win_w / 2;
        rect.h = win_h - rect.y - MARGIN_BOTTOM;
        UpdateWindowSizeAndPosition();
    }
};

class DBusStatListView : public DBusTopWindow
{
  public:
    DBusStatListView() : DBusTopWindow()
    {}
    void Render() override
    {
        werase(win);
        // Write print statement here
        mvwaddstr(win, 1, 1, "This is window C");
        DrawBorderIfNeeded();
        wrefresh(win);
    }

    void OnResize(int win_w, int win_h) override
    {
        rect.y = 8 - MARGIN_BOTTOM;
        rect.w =
            win_w - (win_w / 2) + 1; // Perfectly overlap on the vertical edge
        rect.x = win_w - rect.w;
        rect.h = win_h - rect.y - MARGIN_BOTTOM;
        UpdateWindowSizeAndPosition();
    }
};

class FooterView : public DBusTopWindow
{
  public:
<<<<<<< HEAD
    DBusStatListView();
    void Render() override;
    void OnResize(int win_w, int win_h) override;
    void OnKeyDown(const std::string& key) override;
    int horizontal_pan;
    int menu_h_, menu_w_, menu_margin_;
    ArrowKeyNavigationMenu* menu1_;
    ArrowKeyNavigationMenu* menu2_;
    bool is_showing_menu_;
    int last_choices_[2]; // Last choice index on either side
    enum MenuState
    {
        Hidden,
        LeftSide,  // When the user is choosing an entry on the left side
        RightSide, // When the user is choosing an entry on the right side
    };

    std::vector<DBusTopSortField> GetSortFields();
    MenuState curr_menu_state_, last_menu_state_;
    void RecreateWindow()
=======
    FooterView() : DBusTopWindow()
    {}
    void OnResize(int win_w, int win_h) override
>>>>>>> parent of 6fe55b9 (dbus-top: WIP of all currently-implemented features)
    {
        rect.h = 1;
        rect.w = win_w;
        rect.x = 0;
        rect.y = win_h - 1;
        UpdateWindowSizeAndPosition();
    }

    void Render() override
    {
        werase(win);
        const time_t now = time(nullptr);
        const char* date_time = ctime(&now);
        const std::string help_info = "PRESS ? FOR HELP";
        wbkgd(win, COLOR_PAIR(1));
        wattrset(win, COLOR_PAIR(1));
        mvwaddstr(win, 0, 1, date_time);
        mvwaddstr(win, 0, rect.w - int(help_info.size()) - 1,
                  help_info.c_str());
        wrefresh(win);
    }
};