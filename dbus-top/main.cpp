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

void PrintHello()
{
    border('|', '|', '-', '-', '+', '+', '+', '+');
    mvaddstr(1, 1, "Hello World");
    mvaddstr(2, 1, "This is the beginning of the proposed dbus-top tool.");
    mvaddstr(3, 1, "Thank you.");
}
int main()
{
    initscr(); /* Start curses mode*/
    PrintHello();
    refresh(); /* Print it on to the real screen */
    getch();   /* Wait for user input */
    endwin();  /* End curses mode*/

    return 0;
}