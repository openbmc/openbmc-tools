#include "mock-sensor-tool.hpp"

#include <string.h>

#include <chrono>
#include <sstream>
#include <iostream>
#include <stdio.h>

MockSensor::MockSensor(pid_t pid_in) :
    _is_active(true), _num_sensors(1), _pid(pid_in)
{
    run();
}

void MockSensor::printConfigs()
{
    std::cout << std::string(50, '\n');

    std::lock_guard<std::mutex> config_lock(_sensor_configs_mutex);
    for (size_t i = 0; i < _paths.size(); ++i)
    {
        std::cout << "[" << i << "]: " << _paths[i] << " | ";

        if (!_sensor_configs[_paths[i]]._to_overload)
        {
            std::cout << "No configs set" << std::endl;
            continue;
        }

        if (_sensor_configs[_paths[i]]._set_error)
        {
            std::cout << "Injecting with error " 
                      << _sensor_configs[_paths[i]]._errno_code 
                      << " with "
                      << _sensor_configs[_paths[i]]._delay 
                      << " ms of delay" << std::endl;
        }
        else
        {
            std::cout << "Injecting with value " 
                      << _sensor_configs[_paths[i]]._overload_value 
                      << " with "
                      << _sensor_configs[_paths[i]]._delay 
                      << " ms of delay" << std::endl;
        }
    }
}

void MockSensor::updateConfig(const int path_id)
{
    bool overload = 0;
    bool set_error = 0;
    unsigned int delay = 0;
    unsigned int errno_code = 0;
    std::string overload_value = "0";
    try
    {
        std::cout << "Overload sensor value?" << std::endl;
        std::cout << "Insert 1 to overload, 0 to cancel/clear" << std::endl;
        std::cin >> overload;

        if (overload)
        {
            std::cout << "Inject sensor value or error value?" << std::endl;
            std::cout << "Insert 1 for error, 0 for value" << std::endl;
            std::cin >> set_error;

            if (set_error)
            {
                // 0, 41, 58 and numbers greater than 124 are invalid errno codes
                std::cout << "Insert errno code to inject" << std::endl;
                std::cin >> errno_code;
                if (errno_code == 0 || errno_code == 41 || 
                    errno_code == 58 || errno_code > 124)
                {
                    throw std::runtime_error("Bad errno value");
                }
            }
            else
            {
                // sensor value must fit in an int64_t
                std::cout << "Insert value to inject" << std::endl;
                std::cin >> overload_value;

                if (overload_value.length() > 20)
                {
                    throw std::runtime_error("Sensor value too large");
                }
            }

            std::cout << "Insert delay to read response (milliseconds)"
                      << std::endl;
            std::cin >> delay;
        }

        std::lock_guard<std::mutex> config_lock(_sensor_configs_mutex);
        std::string curr_path = _paths[path_id];
        _sensor_configs[curr_path]._to_overload = overload;
        _sensor_configs[curr_path]._set_error = set_error;
        _sensor_configs[curr_path]._delay = delay;
        _sensor_configs[curr_path]._errno_code = errno_code;
        _sensor_configs[curr_path]._overload_value = overload_value;
    }
    catch (std::runtime_error& e)
    {
        std::cout << "Invalid input" << std::endl;
        std::cout << e.what() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void MockSensor::init() 
{
    if (ptrace(PTRACE_ATTACH, _pid, 0, 0) < 0) 
    {
        std::perror("ptrace(PTRACE_ATTACH) error");
        exit(EXIT_FAILURE);
    }

    int wstatus;
    while (true)
    {
        waitpid(_pid, &wstatus, 0);
        if (WIFSTOPPED(wstatus) && WSTOPSIG(wstatus) == SIGSTOP) 
        {
            break;
        }
        else 
        {
            ptrace(PTRACE_CONT, _pid, (caddr_t)1, 0);
        }
    }

    ptrace(PTRACE_SETOPTIONS, _pid, 0, PTRACE_O_TRACESYSGOOD);
    ptrace(PTRACE_SYSCALL, _pid, 0, 0);
    while (true)
    {
        std::unique_lock<std::mutex> active_lock(_is_active_mutex);
        if (!_is_active)
        {
            break;
        }
        active_lock.unlock();

        waitpid(_pid, &wstatus, 0);
        if (WIFEXITED(wstatus))
        {
            break;
        }
        else {
            struct pt_regs regs;
            ptrace(static_cast<__ptrace_request>(PTRACE_GETREGS), _pid, 0,
                   &regs);

            if (regs.uregs[7] != SYS_READ && regs.uregs[7] != SYS_OPEN)
            {
                ptrace(PTRACE_SYSCALL, _pid, 0, 0);
                continue;
            }

            if (regs.uregs[7] == SYS_OPEN)
            {
                ptrace(static_cast<__ptrace_request>(PTRACE_GETREGS), _pid,
                       0, &regs);
                int i = 0;
                std::string path;
                bool reached_end = false;

                // extract the file pathname out of memory
                while (!reached_end) 
                {
                    std::stringstream ss;
                    ss << std::hex << ptrace(PTRACE_PEEKDATA, _pid, 
                                             regs.uregs[1] + i*sizeof(long));
                    std::string path_chunk;
                    ss >> path_chunk;
                    
                    if (path_chunk.size() != sizeof(long)*2) 
                    {
                        break;
                    }

                    for (int j = sizeof(long) - 1; j >= 0; --j) 
                    {
                        char next_char = static_cast<char>(
                            std::stoul(path_chunk.substr(j*2, 2), nullptr, 16)
                        );

                        if (next_char == '\0') 
                        {
                            reached_end = true;
                            break;
                        }
                        path += next_char;
                    }
                    ++i;
                }

                ptrace(PTRACE_SYSCALL, _pid, 0, 0);
                waitpid(_pid, &wstatus, 0);

                ptrace(static_cast<__ptrace_request>(PTRACE_GETREGS), _pid, 
                       0, &regs);

                // update file descriptor to path mapping
                _fd_to_path[regs.uregs[0]] = path;

                std::lock_guard<std::mutex> configs_lock(_sensor_configs_mutex);
                if (_sensor_configs.find(path) == _sensor_configs.end()) 
                {
                    _sensor_configs[path] = {false, false, 0, 0, ""};
                    _paths.push_back(path);
                }

                ptrace(PTRACE_SYSCALL, _pid, 0, 0);
                continue;
            }

            // at this point, the call must be a read call
            std::unique_lock<std::mutex> configs_lock(_sensor_configs_mutex);
            if (_fd_to_path.find(regs.uregs[0]) !=  _fd_to_path.end() && 
                _sensor_configs[_fd_to_path[regs.uregs[0]]]._to_overload)
            {
                std::string curr_path = _fd_to_path[regs.uregs[0]];

                long new_syscall = -1;
                ptrace(static_cast<__ptrace_request>(PTRACE_SET_SYSCALL), _pid, 
                       0, new_syscall);

                ptrace(PTRACE_SYSCALL, _pid, 0, 0);
                waitpid(_pid, &wstatus, 0);

                ptrace(static_cast<__ptrace_request>(PTRACE_GETREGS), _pid,
                       0, &regs);

                // error injection
                if (_sensor_configs[curr_path]._set_error)
                {
                    long error_return = -1;
                    ptrace(PTRACE_POKEUSER, _pid, 0, error_return);
                    errno = _sensor_configs[curr_path]._errno_code;
                }
                // value injection
                else
                {
                    size_t val_length = _sensor_configs[curr_path]._overload_value.length();
                    int padding = (val_length % sizeof(long) == 0) ? 4 : 
                    ((val_length / sizeof(long)) + 1)*sizeof(long) - (val_length);

                    char overload[val_length + padding];
                    memcpy(overload, 
                           _sensor_configs[curr_path]._overload_value.data(),
                           val_length);

                    int upper_bound = (val_length + padding)/sizeof(long);

                    for (size_t i = 0; i < padding; ++i)
                    {
                        overload[val_length + i] = '\0';
                    }

                    for (size_t i = 0; i < upper_bound; ++i)
                    {
                        long new_sensor_val;
                        memcpy(&new_sensor_val,
                               overload + sizeof(long)*i,
                               sizeof(long));

                        ptrace(PTRACE_POKEDATA, _pid,
                               regs.uregs[1] + (sizeof(long)*i), new_sensor_val);
                    }
                    
                    ptrace(PTRACE_POKEUSER, _pid, 0, regs.uregs[2]);

                    std::this_thread::sleep_for(std::chrono::milliseconds(
                                            _sensor_configs[curr_path]._delay));
                }
            }

            ptrace(PTRACE_SYSCALL, _pid, 0, 0);
        }
    }
}

void MockSensor::run() 
{
    std::thread t1(&MockSensor::init, this);
    _init_thread = std::move(t1);

    std::cout << "Detecting sensors..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));

    while (true)
    {
        printConfigs();
        int option;
        try
        {
            std::cout << "Enter -1 to exit, -2 to sleep, ID number to overload" << std::endl;
            std::cin >> option;
        } 
        catch (...)
        {
            std::cout << "Invalid input" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }
        
        // a bit messy...
        if (option == -1)
        {
            break;
        }
        else if (option == -2)
        {
            std::cout << "Enter amount to sleep in seconds" << std::endl;
            std::cin >> option;

            if (option < 0)
            {
                std::cout << "Invalid input" << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }

            std::cout << "Sleeping for " << option << " seconds." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(option));
            continue;
        }
        else if (option < -2 || option >= static_cast<int>(_paths.size()))
        {
            std::cout << "Invalid input" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }
        else
        {
            updateConfig(option);   
        }
    }

    _is_active_mutex.lock();
    _is_active = false;
    _is_active_mutex.unlock();

    _init_thread.join();
}