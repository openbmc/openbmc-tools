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

#include "bargraph.hpp"

#include <ncurses.h>

#include <string>

const int INVALID = -999; // Constant indicating invalid indices
struct Rect
{
    int x, y, w, h; // X, Y, Width, Height
    Rect(int _x, int _y, int _w, int _h) : x(_x), y(_y), w(_w), h(_h)
    {}
    Rect() : x(0), y(0), w(1), h(1)
    {}
};

int DrawTextWithWidthLimit(WINDOW* win, std::string txt, int y, int x,
                           int width, const std::string& delimiters);
std::string FloatToString(float value);
template <typename T>
void HistoryBarGraph(WINDOW* win, const Rect& rect, BarGraph<T>* bargraph);
