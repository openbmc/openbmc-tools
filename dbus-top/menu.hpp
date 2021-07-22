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
#include "main.hpp"

#include <ncurses.h>

#include <string>
#include <vector>
class DBusTopWindow;
class ArrowKeyNavigationMenu
{
  public:
    explicit ArrowKeyNavigationMenu(WINDOW* win) :
        win_(win), h_padding_(2), col_width_(15), h_spacing_(2), idx0_(INVALID),
        idx1_(INVALID), choice_(INVALID), parent_(nullptr)
    {}
    explicit ArrowKeyNavigationMenu(DBusTopWindow* view);
    void LoadDummyValues()
    {
        items_.clear();
        items_.push_back("Sender");
        items_.push_back("Destination");
        items_.push_back("Interface");
        items_.push_back("Path");
        items_.push_back("Member");
    }

    void OnKeyDown(const std::string& key);
    void Render();
    void MoveCursorAlongSecondaryAxis(int delta);
    void MoveCursorAlongPrimaryAxis(int delta);
    int DispEntriesPerRow()
    {
        int ncols = 0;
        while (true)
        {
            int next = ncols + 1;
            int w = 2 * h_padding_ + col_width_ * next;
            if (next > 1)
                w += (next - 1) * h_spacing_;
            if (w <= rect_.w - 2)
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

    int DispEntriesPerColumn()
    {
        return rect_.h;
    }

    void SetRect(const Rect& rect)
    {
        rect_ = rect;
    }

    enum Order
    {
        ColumnMajor,
        RowMajor,
    };

    void SetOrder(Order o)
    {
        order = o;
    }

    int Choice()
    {
        return choice_;
    }

    void Deselect()
    {
        choice_ = INVALID;
    }

    bool Empty()
    {
        return items_.empty();
    }

    void SetChoiceAndConstrain(int c);
    Rect rect_;
    void AddItem(const std::string& s);
    bool RemoveHighlightedItem(std::string* ret); // returns true if successful
    std::vector<std::string> Items()
    {
        return items_;
    }

    void do_Render(bool is_column_major);
    std::vector<std::string> items_;
    WINDOW* win_;
    int h_padding_;
    int col_width_;
    int h_spacing_;
    int idx0_, idx1_;
    int choice_;
    DBusTopWindow* parent_;
    Order order;
};

