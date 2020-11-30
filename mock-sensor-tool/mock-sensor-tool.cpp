#include "mock-sensor-tool.hpp"

#include <stdio.h>
#include <string.h>

#include <chrono>
#include <iostream>
#include <sstream>

MockSensor::MockSensor(pid_t pidIn) : isActive(true), numSensors(1), pid(pidIn)
{
    run();
}

void MockSensor::printConfigs()
{
    std::cout << std::string(50, '\n');

    std::lock_guard<std::mutex> configLock(sensorConfigsMutex);
    for (size_t i = 0; i < paths.size(); ++i)
    {
        std::cout << "[" << i << "]: " << paths[i] << " | ";

        if (!sensorConfigs[paths[i]].toOverload)
        {
            std::cout << "No configs set" << std::endl;
            continue;
        }

        if (sensorConfigs[paths[i]].setError)
        {
            std::cout << "Injecting with error "
                      << sensorConfigs[paths[i]].errnoCode << " with "
                      << sensorConfigs[paths[i]].delay << " ms of delay"
                      << std::endl;
        }
        else
        {
            std::cout << "Injecting with value "
                      << sensorConfigs[paths[i]].overloadValue << " with "
                      << sensorConfigs[paths[i]].delay << " ms of delay"
                      << std::endl;
        }
    }
}

void MockSensor::updateConfig(const int pathId)
{
    bool overload = 0;
    bool setError = 0;
    unsigned int delay = 0;
    unsigned int errnoCode = 0;
    std::string overloadValue = "0";
    try
    {
        std::cout << "Overload sensor value?" << std::endl;
        std::cout << "Insert 1 to overload, 0 to cancel/clear" << std::endl;
        std::cin >> overload;

        if (overload)
        {
            std::cout << "Inject sensor value or error value?" << std::endl;
            std::cout << "Insert 1 for error, 0 for value" << std::endl;
            std::cin >> setError;

            if (setError)
            {
                // 0, 41, 58 and numbers greater than 124 are invalid errno
                // codes:
                // https://www-numi.fnal.gov/offline_software/srt_public_context/WebDocs/Errors/unix_system_errors.html
                std::cout << "Insert errno code to inject" << std::endl;
                std::cout << "errno code must be a positive number"
                          << std::endl;
                std::cin >> errnoCode;
                if (errnoCode == 0 || errnoCode == 41 || errnoCode == 58 ||
                    errnoCode > 124)
                {
                    throw std::runtime_error("Bad errno value");
                }
            }
            else
            {
                // sensor value must fit in an int64_t
                std::cout << "Insert value to inject" << std::endl;
                std::cin >> overloadValue;

                if (overloadValue.length() > 20)
                {
                    throw std::runtime_error("Sensor value too large");
                }
            }

            std::cout << "Insert delay to read response (milliseconds)"
                      << std::endl;
            std::cin >> delay;
        }

        std::lock_guard<std::mutex> configLock(sensorConfigsMutex);
        std::string currPath = paths[pathId];
        sensorConfigs[currPath].toOverload = overload;
        sensorConfigs[currPath].setError = setError;
        sensorConfigs[currPath].delay = delay;
        sensorConfigs[currPath].errnoCode = errnoCode;
        sensorConfigs[currPath].overloadValue = overloadValue;
    }
    catch (std::runtime_error& e)
    {
        std::cout << "Invalid input" << std::endl;
        std::cout << e.what() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(ERROR_PERIOD));
    }
}

inline std::string MockSensor::extractPath(const pt_regs& regs)
{
    int i = 0;
    std::string path;
    bool reachedEnd = false;

    // Extract the file pathname out of memory
    while (!reachedEnd)
    {
        std::stringstream ss;
        ss << std::hex
           << ptrace(PTRACE_PEEKDATA, pid, regs.uregs[1] + i * sizeof(long));
        std::string pathChunk;
        ss >> pathChunk;

        if (pathChunk.size() != sizeof(long) * 2)
        {
            break;
        }

        for (int j = sizeof(long) - 1; j >= 0; --j)
        {
            char nextChar = static_cast<char>(
                std::stoul(pathChunk.substr(j * 2, 2), nullptr, 16));

            if (nextChar == '\0')
            {
                reachedEnd = true;
                break;
            }
            path += nextChar;
        }
        ++i;
    }

    return path;
}

void MockSensor::init()
{
    if (ptrace(PTRACE_ATTACH, pid, 0, 0) < 0)
    {
        std::perror("ptrace(PTRACE_ATTACH) error");
        exit(EXIT_FAILURE);
    }

    int waitStatus;

    // wait until waitStatus confirms that we've attached to the tracee
    while (true)
    {
        waitpid(pid, &waitStatus, 0);
        if (WIFSTOPPED(waitStatus) && WSTOPSIG(waitStatus) == SIGSTOP)
        {
            break;
        }
        else
        {
            ptrace(PTRACE_CONT, pid, (caddr_t)1, 0);
        }
    }

    ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD);
    ptrace(PTRACE_SYSCALL, pid, 0, 0);
    while (true)
    {
        // isActive is used to check whether the user has exited the tool or
        // not, letting us exit this thread more gracefully.
        std::unique_lock<std::mutex> active_lock(isActiveMutex);
        if (!isActive)
        {
            break;
        }
        active_lock.unlock();

        waitpid(pid, &waitStatus, 0);
        // Detect if the tracee process has exited, if so, break out of the loop
        // and exit mock-sensor-tool
        if (WIFEXITED(waitStatus))
        {
            break;
        }

        struct pt_regs regs;
        ptrace(static_cast<__ptrace_request>(PTRACE_GETREGS), pid, 0, &regs);

        // If the syscall is not a read() or openat() call, resume executing the
        // tracee
        if (regs.uregs[7] != SYS_READ && regs.uregs[7] != SYS_OPEN)
        {
            ptrace(PTRACE_SYSCALL, pid, 0, 0);
            continue;
        }

        if (regs.uregs[7] == SYS_OPEN)
        {
            ptrace(static_cast<__ptrace_request>(PTRACE_GETREGS), pid, 0,
                   &regs);

            std::string path = extractPath(regs);

            ptrace(PTRACE_SYSCALL, pid, 0, 0);
            waitpid(pid, &waitStatus, 0);

            ptrace(static_cast<__ptrace_request>(PTRACE_GETREGS), pid, 0,
                   &regs);

            // Update file descriptor to path mapping
            fdToPath[regs.uregs[0]] = path;

            std::lock_guard<std::mutex> configsLock(sensorConfigsMutex);
            if (sensorConfigs.find(path) == sensorConfigs.end())
            {
                sensorConfigs[path] = {false, false, 0, 0, ""};
                paths.push_back(path);
            }

            ptrace(PTRACE_SYSCALL, pid, 0, 0);
            continue;
        }

        // At this point, the call must be a read call
        std::unique_lock<std::mutex> configsLock(sensorConfigsMutex);
        if (fdToPath.find(regs.uregs[0]) != fdToPath.end() &&
            sensorConfigs[fdToPath[regs.uregs[0]]].toOverload)
        {
            std::string currPath = fdToPath[regs.uregs[0]];

            long new_syscall = -1;
            ptrace(static_cast<__ptrace_request>(PTRACE_SET_SYSCALL), pid, 0,
                   new_syscall);

            ptrace(PTRACE_SYSCALL, pid, 0, 0);
            waitpid(pid, &waitStatus, 0);

            ptrace(static_cast<__ptrace_request>(PTRACE_GETREGS), pid, 0,
                   &regs);

            // error injection
            if (sensorConfigs[currPath].setError)
            {
                long errorReturn = -1;
                ptrace(PTRACE_POKEUSER, pid, 0, errorReturn);
                errno = sensorConfigs[currPath].errnoCode;
            }
            // value injection
            else
            {
                size_t valLength =
                    sensorConfigs[currPath].overloadValue.length();

                // padding will ensure that the size of the overloaded sensor
                // value will be byte-aligned. In the case that the sensor value
                // isn't already byte-aligned, we calculate the number of extra
                // null characters it needs at the end of it to be byte-aligned.
                int padding =
                    (valLength % sizeof(long) == 0)
                        ? 4
                        : ((valLength / sizeof(long)) + 1) * sizeof(long) -
                              (valLength);

                char overload[valLength + padding];
                memcpy(overload, sensorConfigs[currPath].overloadValue.data(),
                       valLength);

                int upperBound = (valLength + padding) / sizeof(long);

                for (size_t i = 0; i < padding; ++i)
                {
                    overload[valLength + i] = '\0';
                }

                for (size_t i = 0; i < upperBound; ++i)
                {
                    long newSensorVal;
                    memcpy(&newSensorVal, overload + sizeof(long) * i,
                           sizeof(long));

                    ptrace(PTRACE_POKEDATA, pid,
                           regs.uregs[1] + (sizeof(long) * i), newSensorVal);
                }

                ptrace(PTRACE_POKEUSER, pid, 0, regs.uregs[2]);

                std::this_thread::sleep_for(
                    std::chrono::milliseconds(sensorConfigs[currPath].delay));
            }
        }

        ptrace(PTRACE_SYSCALL, pid, 0, 0);
    }
}

void MockSensor::run()
{
    std::thread t1(&MockSensor::init, this);
    initThread = std::move(t1);

    std::cout << "Detecting sensors..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(START_PERIOD));

    while (true)
    {
        printConfigs();
        int option;
        try
        {
            std::cout << "Enter -1 to exit, -2 to sleep, ID number to overload"
                      << std::endl;
            std::cin >> option;
        }
        catch (std::runtime_error& e)
        {
            std::cout << "Input error:" << std::endl;
            std::cout << e.what() << std::endl;
            continue;
        }

        if (option == -1)
        {
            break;
        }

        if (option == -2)
        {
            std::cout << "Enter duration to sleep in seconds" << std::endl;
            std::cin >> option;

            if (option < 0)
            {
                std::cout << "Invalid input" << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(ERROR_PERIOD));
                continue;
            }

            std::cout << "Sleeping for " << option << " seconds." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(option));
            continue;
        }

        if (option < -2 || option >= static_cast<int>(paths.size()))
        {
            std::cout << "Invalid input" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(ERROR_PERIOD));
            continue;
        }

        updateConfig(option);
    }

    isActiveMutex.lock();
    isActive = false;
    isActiveMutex.unlock();

    initThread.join();
}