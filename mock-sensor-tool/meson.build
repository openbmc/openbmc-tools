project('mock-sensor-tool', 'cpp', 
    default_options : ['cpp_std=c++17', 
                       'warning_level=2'])

main_target_name = 'mock-sensor-tool'
sources = ['mock-sensor-tool.hpp',
           'mock-sensor-tool.cpp',
           'main.cpp']

thread_dep = dependency('threads')
executable(main_target_name, sources, dependencies: thread_dep, install: true)