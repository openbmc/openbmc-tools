#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/ptrace.h>
// asm/ptrace.h MUST come after sys/ptrace.h for symbol definition purposes
// clang-format off
#include <asm/ptrace.h>
// clang-format on
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

static constexpr auto SYS_READ = 3;
static constexpr auto SYS_OPEN = 322;

/* @brief Number of seconds for the mock sensor tool to detect sensors on
 * startup */
static constexpr auto START_PERIOD = 5;

/* @brief Number of seconds to wait after user input error */
static constexpr auto ERROR_PERIOD = 2;

/** @struct MockSensorSettings
 *  @brief Represents one user defined sensor configuration.
 *
 *  Used for ptrace to determine which sensors to overload and with what values.
 */
struct MockSensorSettings
{
    bool toOverload;
    bool setError;
    uint64_t delay;
    uint8_t errnoCode;
    std::string overloadValue;
};

/** @class MockSensorInterface
 *  @brief Abstract base class allowing testing of the Mock Sensor Tool.
 *
 *  This is used to provide testing of behaviors within MockSensor.
 */
class MockSensorInterface
{
  public:
    virtual ~MockSensorInterface() = default;

  private:
    virtual void init() = 0;
    virtual void run() = 0;
};

/** @class MockSensor
 *  @brief Wrapper class for Mock Sensor Tool.
 *
 *  Allows the user to inject values and errors into
 *  specific sensors at a kernel interface level.
 */
class MockSensor : MockSensorInterface
{
  public:
    ~MockSensor() override = default;
    MockSensor() = delete;
    MockSensor(const MockSensor&) = delete;
    MockSensor& operator=(const MockSensor&) = delete;
    MockSensor(MockSensor&&) = default;
    MockSensor& operator=(MockSensor&&) = default;

    /** @brief Constructor
     *
     *  @param[in] pidIn - pid of the phosphor-hwmon or dbus-sensors
     *                      instance to overload.
     */
    explicit MockSensor(const pid_t pidIn);

  private:
    /** @brief Print all existing configurations.
     *
     *  Prints all existing sensors along with the values/errors the user
     *  is currently injecting into those sensors, if any.
     */
    void printConfigs();

    /** @brief Update a sensor's configuration.
     *
     *  Takes in a user specified path id variable and allows the user
     *  to update values for the sensor associated with that path id.
     *
     *  @param[in] pathId - The path whose configs the user wants to update.
     */
    void updateConfig(const int pathId);

    /** @brief Helper fuinction for init()
     *
     *  Retrieves the path of a file out of memory
     */
    inline std::string extractPath(const pt_regs& regs);

    /** @brief Retrieves all connected sensors and overloads sensor values
     *         based on user specified configurations.
     *
     *  Picks up all files that are opened within a window of time specified by
     * START_PERIOD.
     */
    void init() override;

    /** @brief Main function for the MockSensor class.
     *
     *  This function is called in the constructor. This is the initial code
     *  the user interacts with to input the PID of the process to overload.
     */
    void run() override;

    /** @brief Determines whether the user is still interacting with the tool.
     *
     *  Used for cleanup purposes
     */
    bool isActive;

    /** @brief Number of detected sensors */
    int numSensors;

    /** @brief PID of the tracee process */
    pid_t pid;

    /** @brief The map of all sensor paths to their configurations
     *
     *  Since each physical hwmon sensor is set to a defined sysfs file,
     *  we can get a unique identifer for each sensor with its path.
     */
    std::unordered_map<std::string, MockSensorSettings> sensorConfigs;

    /** @brief Maps a file descriptor to its associated path at any time
     *
     *  As file descriptors change, this map continuously updates the file
     *  descriptors associated with each path.
     */
    std::unordered_map<long, std::string> fdToPath;

    /** @brief list of all relevant sensor paths */
    std::vector<std::string> paths;

    /** @brief used to lock the isActive data member */
    std::mutex isActiveMutex;

    /** @brief used to lock the sensorConfigs and fdToPath data members */
    std::mutex sensorConfigsMutex;

    /** @brief used to keep the init() thread running throughout the duration
     *         of the program
     */
    std::thread initThread;
};
