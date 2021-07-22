# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

project('dbus-top', 'cpp',
    version: '0.1',
    default_options: 
    [
        'cpp_std=c++17',
    ],
)

cmake = import('cmake')
fmt_proj = cmake.subproject('fmt')
fmt_dep = fmt_proj.dependency('fmt')

bindir = get_option('prefix') / get_option('bindir')
executable('dbus-top',
    [
        'menu.cpp',
        'analyzer.cpp',
        'dbus_capture.cpp',
        'sensorhelper.cpp',
        'xmlparse.cpp',
        'views.cpp',
        'main.cpp',
    ],
    dependencies:
    [
        dependency('systemd'),
        dependency('sdbusplus'),
        dependency('ncurses'),
        dependency('threads'),
        fmt_dep
    ],
    install: true,
    install_dir: bindir
)
