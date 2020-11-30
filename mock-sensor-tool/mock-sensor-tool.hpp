#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <asm/ptrace.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <unistd.h>
#include <errno.h>

#include <string>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <vector>

static constexpr auto SYS_READ = 3;
static constexpr auto SYS_OPEN = 322;

/** @struct MockSensorSettings
 *  @brief Represents one user defined sensor configuration.
 * 
 *  Used for ptrace to determine which sensors to overload and with what values.
 */
struct MockSensorSettings {
    bool _to_overload; 
    bool _set_error;
    int _delay;
    int _errno_code;
    std::string _overload_value;
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
     *  @param[in] pid_in - pid of the phosphor-hwmon or dbus-sensors
     *                      instance to overload.
     */
    explicit MockSensor(const pid_t pid_in);

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
     *  @param[in] path_id - The path whose configs the user wants to update.
     */
    void updateConfig(const int path_id);

    /** @brief Retrieves all connected sensors and overloads sensor values
     *         based on user specified configurations.
     *  
     *  Picks up all files that are opened within a 5 second window.
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
    bool _is_active;

    /** @brief Number of detected sensors */
    int _num_sensors;

    /** @brief PID of the tracee process */
    pid_t _pid;

    /** @brief The map of all sensor paths to their configurations
     * 
     *  Since each physical hwmon sensor is set to a defined sysfs file,
     *  we can get a unique identifer for each sensor with its path.
     */ 
    std::unordered_map<std::string, MockSensorSettings> _sensor_configs;

    /** @brief Maps a file descriptor to its associated path at any time
     * 
     *  As file descriptors change, this map continuously updates the file
     *  descriptors associated with each path.
     */
    std::unordered_map<long, std::string> _fd_to_path;

    /** @brief list of all relevant sensor paths */
    std::vector<std::string> _paths;

    /** @brief used to lock the _is_active data member */
    std::mutex _is_active_mutex;

    /** @brief used to lock the _sensor_configs and _fd_to_path data members */
    std::mutex _sensor_configs_mutex;

    /** @brief used to keep the init() thread running throughout the duration
     *         of the program 
     */
    std::thread _init_thread;
};
