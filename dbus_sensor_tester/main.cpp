#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/Formatter.hpp>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

boost::asio::io_context io;
std::vector<std::shared_ptr<sdbusplus::asio::dbus_interface>> sensorInterfaces;

int update_interval_seconds = 1;

size_t reads = 0;

void on_loop(boost::asio::steady_timer *timer,
             const boost::system::error_code &error) {

  if (error) {
    return;
  }
  std::chrono::steady_clock::time_point start =
      std::chrono::steady_clock::now();

  static double value = -100.0;

  for (auto &sensor : sensorInterfaces) {
    if (!sensor->set_property("Value", value)) {
      std::cout << "Can't set property for sensor\n";
    }
    value += 10.0;
    if (value >= 100.0) {
      value = -100.0;
    }
  }
  if (!sensorInterfaces.empty()) {
    std::cout << sensorInterfaces.size() << " updates took "
              << std::chrono::duration_cast<std::chrono::duration<float>>(
                     std::chrono::steady_clock::now() - start)
                     .count()
              << " seconds\n";
  }

  if (reads > 0) {
    std::cout << "Read " << reads << " sensor updates\n";
    reads = 0;
  }

  timer->expires_from_now(std::chrono::seconds(update_interval_seconds));
  timer->async_wait(std::bind_front(on_loop, timer));
};

int main(int argc, const char **argv) {
  CLI::App app{"dbus performance test application"};

  size_t number_of_sensors = 0;
  app.add_option("-n", number_of_sensors, "Number of sensors to create");

  bool watch_sensor_updates = false;
  app.add_flag("-w", watch_sensor_updates,
               "Watch for all sensor values from dbus");
  CLI11_PARSE(app, argc, argv);

  if (number_of_sensors == 0 && watch_sensor_updates == false) {
    std::cout << "Nothing to do\n";
    app.exit(CLI::CallForHelp());
    return -1;
  }

  std::shared_ptr<sdbusplus::asio::connection> connection =
      std::make_shared<sdbusplus::asio::connection>(io);
  sdbusplus::asio::object_server objectServer(connection);

  std::string name = "foobar";
  sensorInterfaces.reserve(number_of_sensors);
  for (size_t sensorIndex = 0; sensorIndex < number_of_sensors; sensorIndex++) {
    sdbusplus::message::object_path path(
        "/xyz/openbmc_project/sensors/temperature/");
    path /= name + std::to_string(sensorIndex);
    std::shared_ptr<sdbusplus::asio::dbus_interface> sensorInterface =
        objectServer.add_interface(path.str,
                                   "xyz.openbmc_project.Sensor.Value");
    sensorInterface->register_property<std::string>(
        "Unit", "xyz.openbmc_project.Sensor.Unit.DegreesC");
    sensorInterface->register_property<double>("MaxValue", 100);
    sensorInterface->register_property<double>("MinValue", -100);
    sensorInterface->register_property<double>("Value", 42);

    sensorInterface->initialize();
    sensorInterfaces.emplace_back(sensorInterface);
  }

  std::cout << "Done initializing\n";

  boost::asio::steady_timer timer(io);
  timer.expires_from_now(std::chrono::seconds(update_interval_seconds));
  timer.async_wait(std::bind_front(on_loop, &timer));
  std::optional<sdbusplus::bus::match_t> match;
  if (watch_sensor_updates) {
    std::string expr = "type='signal',member='PropertiesChanged',path_"
                       "namespace='/xyz/openbmc_project/sensors'";

    match.emplace(
        static_cast<sdbusplus::bus_t &>(*connection), expr,
        [](sdbusplus::message_t &message) {
          std::string objectName;
          std::vector<std::pair<std::string, std::variant<double>>> result;
          try {
            message.read(objectName, result);
          } catch (const sdbusplus::exception_t &) {
            std::cerr << "Error reading match data\n";
            return;
          }
          for (auto &property : result) {
            if (property.first == "Value") {
              reads++;
            }
          }
        });
  }

  io.run();

  return 0;
}