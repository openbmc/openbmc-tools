#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>
#include <string>
#include <cstdio>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

using namespace std;

int main(int argc, char** argv) {
    ifstream inFile;
    int64_t sensor_val = 9999;
    cout << getpid() << endl;
    while (true) {
        inFile.open("reading.txt");
        inFile >> sensor_val;
        inFile.close();
        cout << "SENSOR: " << sensor_val << endl;
        std:this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}

int main2(int argc, char** argv) {
  char sensor_val[8192];
  cout << getpid() << endl;
  while (true) {
    int fd = open("reading.txt", O_RDONLY);
    printf("BYTES READ: %d\n", read(fd, (void*)&sensor_val, 8191));
    write(0, sensor_val, 4);
    write(0,"\n", 1);
    close(fd);
    std:this_thread::sleep_for(std::chrono::seconds(1));
  }
  return 0;
}
