// This program reads the hwmon sensors of the user's choice and prints the I2C traces
// that happened during the operations.
//
// To run:
//   ./i2c_bmk_bmc [list]      list all available hwmon sensors
//   ./i2c_bmk_bmc SensorID    read one or more sensors denoted by their IDs; save trace to "i2c_trace.txt"
//   ./i2c_bmk_bmc all         read all hwmon sensors; save trace to "i2c_trace.txt"
#include <fstream>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <libgen.h>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <list>

std::string MyTrim(std::string s) {
  while (s.empty() == false) {
    const char ch = s.back();
    if (ch == '\r' || ch == '\t' || ch == '\n') {
      s.pop_back();
    } else break;
  }
  return s;
}

std::string ReadFileIntoString(const std::string_view file_name)
{
    std::stringstream ss;
    std::ifstream ifs(file_name.data());
    while (ifs.good())
    {
        std::string line;
        std::getline(ifs, line);
        ss << line;
        if (ifs.good())
            ss << std::endl;
    }
    return ss.str();
}


std::string g_argv0;
std::string g_hwmon_root = "/sys/class/hwmon/";
struct HwmonInput {
  std::string path;       // Location of the hwmon symbolic link
  std::string real_path;  // Where is this hwmon linked to
  std::string device_name; // Device name
  bool is_i2c;
  bool operator<(const HwmonInput& other) const {
    if (path < other.path) return true;
    else if (path == other.path) {
      if (real_path < other.real_path) return true;
      else return is_i2c < other.is_i2c;
    } else return false;
  }
};
std::vector<struct HwmonInput> g_inputs;

void FindAllHwmonPaths(const char* hwmon_root,
  std::vector<struct HwmonInput>* inputs) {
  const char* hwmon_path = hwmon_root;//"/sys/class/hwmon/";
  int device_idx = 0;
  for (const auto & hwmon_entry : std::filesystem::directory_iterator(hwmon_path)) {
    const std::string& path = hwmon_entry.path();

    bool this_is_i2c = false;
    std::string hwmon_real_path = "";
    if (std::filesystem::is_symlink(path)) {
      std::string linked = std::filesystem::read_symlink(path);
            hwmon_real_path = linked;
      if (linked.find("i2c") != std::string::npos) {
        this_is_i2c = true;
      }
    }

    std::string device_name = MyTrim(ReadFileIntoString(path + "/device/name"));
    
    for (const auto & hwmon_file : std::filesystem::directory_iterator(path)) {
      std::string p = hwmon_file.path();
      ssize_t idx = p.find("_input");
      if (idx == int(p.size()) - 6) {
        struct HwmonInput hwmon_input;
        hwmon_input.path = p;
        hwmon_input.real_path = hwmon_real_path;
        hwmon_input.is_i2c = this_is_i2c;
        hwmon_input.device_name = device_name;
        inputs->emplace_back(hwmon_input);
      }
    }
    
    device_idx ++;
  }
  std::sort(inputs->begin(), inputs->end());
}

// Enabling kernel tracing:
// cd /sys/kernel/debug/tracing/
// echo nop > current_tracer
// echo 1 > events/i2c/enable
// echo 1 > tracing_on
void EnableKernelTracing() {
  system("echo nop > /sys/kernel/debug/tracing/current_tracer");
  system("echo 1   > /sys/kernel/debug/tracing/events/i2c/enable");
  system("echo 1   > /sys/kernel/debug/tracing/tracing_on");
}

void DisableKernelTracing() {
  system("echo nop > /sys/kernel/debug/tracing/current_tracer");
  system("echo 0   > /sys/kernel/debug/tracing/events/i2c/enable");
  system("echo 0   > /sys/kernel/debug/tracing/tracing_on");
}

// The kernel timestamp & the uptime fetched via sysinfo might "drift".
// Example:
// Time in kernel trace = 2666326, time from sysinfo = 2666341
// Thus this function fetches the last (t1-t0) seconds worth of trace
void FindTraceEntries(double t0, double t1) {
  
  std::ifstream ifs("/sys/kernel/debug/tracing/trace");
  std::string line;
  std::list<std::pair<double, std::string> > moving_window;
  double thresh = std::max(0.0, t1 - t0);
  
  fprintf(stderr, "[FindTraceEntries] t0=%f t1=%f\n", t0, t1);
  while (ifs.good()) {
    getline(ifs, line);
    
    // Get timestamp
    if (line.find(g_argv0) != std::string::npos) {
      ssize_t x = line.find(":"), x0 = x;
      if (x != std::string::npos) {
        x--;
        while (true) {
          char c = line[x];
          if ((c >= '0' && c <= '9') || (c == '.')) x--;
          else break;
        }
        double seconds = std::atof(line.substr(x, x0-x).c_str());

        std::string payload = line.substr(x0+1);
//          printf("%g: %s\n", seconds, payload.c_str());
//          printf("%s\n", payload.c_str());
          moving_window.emplace_back(seconds, payload);
          while (!moving_window.empty() &&
                 moving_window.front().first < seconds - thresh) {
            moving_window.pop_front();
          }
//        }
      }
    }
  }
  
  const char* FILE_NAME = "i2c_trace.txt";
  std::ofstream ofs(FILE_NAME);
  if (!ofs.good()) {
    printf("Error: Could not open file %s to save trace\n", FILE_NAME);
  }
  fprintf(stderr, "Found %lu interesting I2C trace entries:\n", moving_window.size());
  fflush(stdout);
  for (const std::pair<double, std::string>& p : moving_window) {
    const std::string line = MyTrim(p.second);
    printf("%s\n", MyTrim(p.second).c_str());
    if (ofs.good()) {
      ofs << line << std::endl;
    }
  }
  if (ofs.good()) {
    ofs.close();
  }
  fflush(stdout);
}

long GetUptime() { // in seconds
  struct sysinfo info;
  int error = sysinfo(&info);
  if (error != 0) {
    fprintf(stderr, "error in sysinfo: %s\n", strerror(error));
  }
  return info.uptime;
}

void PrintSomething(const char* header, struct i2c_msg* m) {
  printf("[%s] addr=%04x flags=%04x len=%d data=[", header, m->addr, m->flags, m->len);
  for (int i=0; i<m->len; i++) {
    printf("%02x ", m->buf[i] & 0xFF);
  }
  printf("]\n");
}

void hwmon_test(int argc, char** argv) {
  std::vector<struct HwmonInput> inputs;
  FindAllHwmonPaths(g_hwmon_root.c_str(), &g_inputs);
  if ((argc <= 1) || (argc > 1 && !strcmp(argv[1], "list"))) { // Just list all sensors
    for (int i=0; i<int(g_inputs.size()); i++) {
      printf("%d: %s (%s)", i, g_inputs[i].path.c_str(), g_inputs[i].device_name.c_str());
      if (g_inputs[i].is_i2c) {
        printf("(I2C)");
      }
      printf("\n");
    }
    return;
  }

  std::string_view argv1 = argv[1];
  if (argc > 1) {
    const std::size_t idx = argv1.find("-");
    if (argv1 == "all") {
      inputs = g_inputs;
    } else if (idx != std::string_view::npos) {
      const int from = std::atoi(argv1.substr(0, idx).data());
      const int to   = std::atoi(argv1.substr(idx+1).data());
      fprintf(stderr, "Processing inputs %d through %d\n", from, to);
      for (int i=from; i<=to; i++) { inputs.push_back(g_inputs[i]); }
    } else {
      for (int i=1; i<argc; i++) {
        int idx = std::atoi(argv[i]);
        fprintf(stderr, "idx=%d\n", idx);
        if (idx >= 0 && idx < int(g_inputs.size())) {
          inputs.push_back(g_inputs[idx]);
        }
      }
    }
  }

  fprintf(stderr, "Processing %u inputs\n", (unsigned)(inputs.size()));

  EnableKernelTracing();
  double uptime0 = GetUptime();

  for (int i=0; i<int(inputs.size()); i++) {
    std::string the_input = inputs[i].path;
    std::string real_path = inputs[i].real_path;
    std::ifstream ifs(the_input);
    std::string line;
    std::getline(ifs, line);
    fprintf(stderr, "%s (%s): %s\n", the_input.c_str(), real_path.c_str(), line.c_str());
  }

  double uptime1 = GetUptime();

  const double DELTA = 2.0;

  DisableKernelTracing();

  FindTraceEntries(uptime0-DELTA, uptime1);
}

int main(int argc, char** argv) {
  g_argv0 = basename(argv[0]);
  hwmon_test(argc, argv);
  return 0;
}
