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

#include "menu.hpp"

#include "views.hpp"

ArrowKeyNavigationMenu::ArrowKeyNavigationMenu(DBusTopWindow* view) :
    win_(view->win), parent_(view), idx0_(INVALID), idx1_(INVALID),
    h_padding_(2), col_width_(15), h_spacing_(2), choice_(INVALID)
{}

void ArrowKeyNavigationMenu::do_Render(bool is_column_major)
{
    const int nrows = DispEntriesPerColumn();
    const int ncols = DispEntriesPerRow();
    const int items_per_page = nrows * ncols;
    if (items_per_page < 1)
        return;
    int tot_num_items = items_.size();
    // int tot_num_columns = (tot_num_items - 1) / nrows + 1;
    // Determine whether cursor is outside the current rectangular viewport
    bool is_cursor_out_of_view = false;
    if (idx0_ > choice_ || idx1_ <= choice_)
    {
        is_cursor_out_of_view = true;
    }
    if (idx0_ == INVALID || idx1_ == INVALID)
    {
        is_cursor_out_of_view = true;
    }
    // Scroll the viewport such that it contains the cursor
    if (is_cursor_out_of_view)
    {
        idx0_ = 0;
        idx1_ = items_per_page;
    }
    while (idx1_ <= choice_)
    {
        if (is_column_major)
        {
            idx0_ += nrows;
            idx1_ += nrows;
        }
        else
        {
            idx0_ += ncols;
            idx1_ += ncols;
        }
    }
    int y0 = rect_.y, x0 = rect_.x;
    int y = y0, x = x0;
    for (int i = 0; i < items_per_page; i++)
    {
        int idx = idx0_ + i;
        if (idx < tot_num_items)
        {
            if (idx == choice_)
            {
                wattrset(win_, A_REVERSE);
            }
            std::string s = items_[idx];
            while (s.size() < col_width_)
            {
                s.push_back(' ');
            }
            mvwprintw(win_, y, x, s.c_str());
            wattrset(win_, 0);
        }
        else
        {
            break;
        }
        if (is_column_major)
        {
            y++;
            if (i % nrows == nrows - 1)
            {
                y = y0;
                x += col_width_ + h_spacing_;
            }
        }
        else
        {
            x += col_width_ + h_spacing_;
            if (i % ncols == ncols - 1)
            {
                x = x0;
                y++;
            }
        }
    }
}

void ArrowKeyNavigationMenu::Render()
{
    do_Render(order == ColumnMajor);
}

void ArrowKeyNavigationMenu::OnKeyDown(const std::string& key)
{
    switch (order)
    {
        case ColumnMajor:
            if (key == "up")
            {
                MoveCursorAlongPrimaryAxis(-1);
            }
            else if (key == "down")
            {
                MoveCursorAlongPrimaryAxis(1);
            }
            else if (key == "left")
            {
                MoveCursorAlongSecondaryAxis(-1);
            }
            else if (key == "right")
            {
                MoveCursorAlongSecondaryAxis(1);
            }
            break;
        case RowMajor:
            if (key == "up")
            {
                MoveCursorAlongSecondaryAxis(-1);
            }
            else if (key == "down")
            {
                MoveCursorAlongSecondaryAxis(1);
            }
            else if (key == "left")
            {
                MoveCursorAlongPrimaryAxis(-1);
            }
            else if (key == "right")
            {
                MoveCursorAlongPrimaryAxis(1);
            }
            break;
            break;
    }
}

void ArrowKeyNavigationMenu::MoveCursorAlongPrimaryAxis(int delta)
{
    const int N = items_.size();
    if (N < 1)
        return;
    // If the cursor is inactive, activate it
    if (choice_ == INVALID)
    {
        if (delta > 0)
        {
            choice_ = 0;
        }
        else
        {
            choice_ = N - 1;
        }
        return;
    }
    int choice_next = choice_ + delta;
    while (choice_next >= N)
    {
        choice_next -= N;
    }
    while (choice_next < 0)
    {
        choice_next += N;
    }
    choice_ = choice_next;
}

void ArrowKeyNavigationMenu::MoveCursorAlongSecondaryAxis(int delta)
{
    if (delta != 0 && delta != 1 && delta != -1)
        return;
    const int N = items_.size();
    if (N < 1)
        return;
    // If the cursor is inactive, activate it
    if (choice_ == INVALID)
    {
        if (delta > 0)
        {
            choice_ = 0;
        }
        else
        {
            choice_ = N - 1;
        }
        return;
    }
    const int nrows =
        (order == ColumnMajor) ? DispEntriesPerColumn() : DispEntriesPerRow();
    const int tot_columns = (N - 1) / nrows + 1;
    const int num_rows_last_column = N - nrows * (tot_columns - 1);
    int y = choice_ % nrows, x = choice_ / nrows;
    if (delta == 1)
    {
        x++;
    }
    else
    {
        x--;
    }
    bool overflow_to_right = false;
    if (y < num_rows_last_column && x >= tot_columns)
    {
        overflow_to_right = true;
    }
    if (y >= num_rows_last_column && x >= tot_columns - 1)
    {
        overflow_to_right = true;
    }
    bool overflow_to_left = false;
    if (x < 0)
    {
        overflow_to_left = true;
    }
    if (overflow_to_right)
    {
        y++;
        if (y >= nrows)
        {
            choice_ = 0;
            return;
        }
        else
        {
            choice_ = y;
            return;
        }
    }
    else if (overflow_to_left)
    {
        y--;
        if (y < 0)
        {
            if (num_rows_last_column == nrows)
            {
                choice_ = N - 1;
            }
            else
            {
                choice_ = N - num_rows_last_column - 1;
            }
            return;
        }
        else
        {
            if (y < num_rows_last_column)
            {
                choice_ = nrows * (tot_columns - 1) + y;
            }
            else
            {
                choice_ = nrows * (tot_columns - 2) + y;
            }
        }
    }
    else
    {
        choice_ = y + x * nrows;
    }
}

void ArrowKeyNavigationMenu::SetChoiceAndConstrain(int c)
{
    if (Empty())
    {
        choice_ = INVALID;
        return;
    }
    if (c < 0)
        c = 0;
    if (c >= static_cast<int>(items_.size()))
    {
        c = static_cast<int>(items_.size() - 1);
    }
    choice_ = c;
}

void ArrowKeyNavigationMenu::AddItem(const std::string& s)
{
    items_.push_back(s);
}

bool ArrowKeyNavigationMenu::RemoveHighlightedItem(std::string* ret)
{
    if (choice_ < 0 || choice_ >= items_.size())
        return false;
    std::string r = items_[choice_];
    items_.erase(items_.begin() + choice_);
    if (items_.empty())
    {
        Deselect();
    }
    else
    {
        if (choice_ >= items_.size())
        {
            choice_ = items_.size() - 1;
        }
    }
    if (ret)
    {
        *ret = r;
    }
    return true;
}