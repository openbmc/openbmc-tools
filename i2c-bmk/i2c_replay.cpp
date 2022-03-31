// This program replays an I2C trace, which can look like the following:
// ------------------------------8<----------------------------
// i2c_write: i2c-201 #0 a=060 f=0004 l=1 [97]
// i2c_read: i2c-201 #1 a=060 f=0005 l=3
// i2c_write: i2c-201 #0 a=060 f=0004 l=1 [f2]
// i2c_read: i2c-201 #1 a=060 f=0005 l=3
// ------------------------------>8----------------------------
//
// Usage:
// ./i2c_replay_bmc FILE_NAME              replays FILE_NAME
//
// If FILE_NAME is not supplied, the program will try reading "i2c_trace.txt"
//
// This program operates in i2c_rdwr_ioctl_data's. The trace will be
// automagically broken into various i2c_rdwr_ioctl_data's, and each of
// the might contain one or more I2C requests.
//
// I2C commands might be WRITE or READ. A WRITE request will get a RESULT
// message as a response. A READ request will get a REPLY message as a response.
//
// The REPLY and RESULT message in a trace are ignored for now.
//
// TODO:
//   Check RESULT and REPLY to make sure the nesting patterns are as expected
//   and perhaps also compare results to make sure their differences are smaller
//   than a threshold
//
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <assert.h>
#include <unordered_map>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
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
#include <chrono>
#include <iostream>
#include <map>
#include <unordered_set>
#include "i2c_topology.hpp"

std::string ByteArrayToString(const std::vector<unsigned char>& v) {
  std::stringstream ss;
  ss.setf(std::ios_base::hex, ss.basefield);
  for (const unsigned char c : v) {
    ss << std::hex << std::setw(2) << std::uppercase << std::setfill('0') << int(c&0xff) << " ";
  }
  return ss.str();
}

std::string g_argv0;
bool g_verbose = false;
bool g_mock = false;
bool g_debug = false;
int g_sleep_secs = 0;

long GetUptime() { // in seconds
  struct sysinfo info;
  int error = sysinfo(&info);
  if (error != 0) {
    printf("error in sysinfo: %s\n", strerror(error));
  }
  return info.uptime;
}

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

void FindTraceEntries(double t0, double t1) {
    std::ifstream ifs("/sys/kernel/debug/tracing/trace");
    std::string line;
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
                if (seconds >= t0 && seconds <= t1) {
                    std::string payload = line.substr(x0+1);
                    printf("%g: %s\n", seconds, payload.c_str());
                }
            }
        }
    }
}

class I2CCommand {
public:
  enum I2CCommandType {
    WRITE, READ, REPLY, RESULT
  };

private:
  I2CCommand(I2CCommandType _type, int _id, int _seq, int _addr, int _flag, int _len, std::vector<unsigned char>* _payload) {
    id = _id; seq = _seq;
    type = _type; addr = _addr; flag = _flag; len = _len;
    if (_payload) { 
      payload = *_payload;
      len = int(_payload->size());
    }
  }
public:
  
  bool IsRequest() { return (type == I2CCommandType::WRITE || type == I2CCommandType::READ); }
  bool IsResponse() { return !IsRequest(); }
  
  // c1 is the request
  static bool IsRequestResponsePair(I2CCommand* c1, I2CCommand* c2) {
    if (c1->id != c2->id) return false;
    else {
      switch (c1->type) {
        case I2CCommandType::WRITE:
          return c2->type == I2CCommandType::RESULT;
        case I2CCommandType::READ:
          return c2->type == I2CCommandType::REPLY;
        default: return false;
      }
    }
  }

  static I2CCommand* Write(int _id, int _seq, int _addr, int _flag, std::vector<unsigned char>* _payload) {
    return new I2CCommand(I2CCommandType::WRITE, _id, _seq, _addr,
        _flag, int(_payload->size()), _payload);
  }

  static I2CCommand* Read(int _id, int _seq, int _addr, int _flag, int _len) {
    return new I2CCommand(I2CCommandType::READ, _id, _seq, _addr,
        _flag, _len, nullptr);
  }

  static I2CCommand* Reply(int _id, int _seq, int _addr, int _flag, std::vector<unsigned char>* _payload) {
    return new I2CCommand(I2CCommandType::REPLY, _id, _seq, _addr,
        _flag, int(_payload->size()), _payload);
  }

  static I2CCommand* Result(int _id, int _result) {
    return new I2CCommand(I2CCommandType::RESULT, _id, 0, 0,
        0, _result, nullptr);
  }

  std::string ToJSONForViz() {
    std::stringstream ss;
    ss << "[\"";
    ss << std::to_string(id);
    ss << "\", \"";
    switch (type) {
      case I2CCommandType::WRITE:
        ss << "write"; break;
      case I2CCommandType::READ:
        ss << "read"; break;
      case I2CCommandType::REPLY:
        ss << "reply"; break;
      case I2CCommandType::RESULT:
        ss << "result"; break;
    }
    ss << "\"]";
    return ss.str();
  }
  
  std::string ToString() {
    std::stringstream ss;
    switch (type) {
      case I2CCommandType::WRITE:
        ss << "i2c_write: "; break;
      case I2CCommandType::READ:
        ss << "i2c_read: "; break;
      case I2CCommandType::REPLY:
        ss << "i2c_reply: "; break;
      case I2CCommandType::RESULT:
        ss << "i2c_result: "; break;
    }

    ss << "i2c-" << id << " ";

    // address, flag, length
    switch (type) {
      case I2CCommandType::WRITE:
      case I2CCommandType::READ:
      case I2CCommandType::REPLY:
        ss << "#0 ";
        char buf[100];
        sprintf(buf, "a=%03x f=%04x l=%d ", addr, flag, len);
        ss << std::string(buf);
          break; // Is this ever used?
      case I2CCommandType::RESULT:
        ss << "n=" << len << " ret=" << result;
        break;
    }

    switch (type) {
      case I2CCommandType::WRITE:
      case I2CCommandType::REPLY:
        ss << "[";
        for (int i=0; i<int(payload.size()); i++) {
          if (i > 0) { ss << " "; }
          char buf[10];
          sprintf(buf, "%02x", payload[i]);
          ss << std::string(buf);
        }
        ss << "]";
        break;
    }
    return ss.str();
  }

  I2CCommandType type;
  int id, seq, addr, flag;
     union {
    int len;
    int result;
  };
  std::vector<unsigned char> payload;
};

std::string MunchWord(const std::string& line, int* idx) {
  int idx0;
  while (line[*idx] == ' ' && ((*idx)+1) <= line.size()) { (*idx) ++; }
  idx0 = *idx;
  while (line[*idx] != ' ' && ((*idx)+1) <= line.size()) { (*idx) ++; }
  std::string ret = line.substr(idx0, (*idx)-idx0);
  while (line[*idx] == ' ' && ((*idx)+1) <= line.size()) { (*idx) ++; }
  return ret;
}

int MunchI2C(const std::string& line, int* idx) {
  int idx0 = *idx;
  std::string w = MunchWord(line, idx);
  int ret = -999;
  if (w.find("i2c-") == 0 && w.size() > 4) {
    ret = std::atoi(w.substr(4).c_str());
    *idx = idx0 + w.size();
  } else { *idx = idx0; }
  return ret;
}

int do_Munch1(const std::string& line, const std::string prefix, int* idx, int base) {
  int idx0 = *idx;
  std::string w = MunchWord(line, idx);
  int ret = -1;
  if (w.find(prefix) == 0 && w.size() > prefix.length()) {
    ret = std::stoi(w.substr(prefix.size()).c_str(), 0, base);
  } else { *idx = idx0; }
  return ret;
}

int MunchAddress(const std::string& line, int* idx) {
  return do_Munch1(line, "a=", idx, 16);
}

int MunchFlag(const std::string& line, int* idx) {
  return do_Munch1(line, "f=", idx, 16);
}

int MunchLength(const std::string& line, int* idx) {
  int ret = do_Munch1(line, "l=", idx, 10);
  return ret;
}

int MunchRet(const std::string& line, int* idx) {
  int ret = do_Munch1(line, "ret=", idx, 10);
  return ret;
}

int MunchSeq(const std::string& line, int* idx) {
  int ret = do_Munch1(line, "#", idx, 10);
  return ret;
}

std::vector<unsigned char> MunchByteArray(const std::string& line, int* idx) {
  std::string w = MunchWord(line, idx);
  std::vector<unsigned char> ret; 

  int widx = 0;
  if (w[0] == '[') {
    while (widx < int(w.size())) {
      widx ++;
      if (w[widx] == ']') goto DONE;
      std::string b = w.substr(widx, 2);
      widx += 2;
      if (b.size() < 1) { goto DONE; }
      ret.push_back(std::stoi(b.c_str(), 0, 16));
    }
  }
DONE:
  return ret;
}

// Just grab READ and WRITE events
I2CCommand* ParseLine(const std::string& line) {
  I2CCommand* ret = nullptr;
  int idx = 0;
  
  bool masked = false;
  while (line[idx] == '#' && (idx + 1 <= line.size())) {
    masked = true;
       idx ++; 
  }

  std::string verb = MunchWord(line, &idx);
  if (verb == "i2c_read:") { // i2c_read: i2c-2 #1 a=04c f=0001 l=1
    int id = MunchI2C(line, &idx);
    if (id != -999) {
      int seq = MunchSeq(line, &idx);
      int addr = MunchAddress(line, &idx);
      if (addr != -1) {
        int flag = MunchFlag(line, &idx);
        if (flag != -1) {
          int len = MunchLength(line, &idx);
          if (g_debug) {
            printf(">>> READ %d %d %x %x %d\n", id, seq, addr, flag, len);
          }
          ret = I2CCommand::Read(id, seq, addr, flag, len);
        }
      }
    }
  } else if (verb == "i2c_write:") { // i2c_write: i2c-2 #0 a=04c f=0000 l=1 [01]
    int id = MunchI2C(line, &idx);
    if (id != -999) {
      int seq = MunchSeq(line, &idx);
      int addr = MunchAddress(line, &idx);
      if (addr != -1) {
        int flag = MunchFlag(line, &idx);
        if (flag != -1) {
          int len = MunchLength(line, &idx);
          std::vector<unsigned char> payload = MunchByteArray(line, &idx);
          if (g_debug) {
            printf(">>> WRITE %d %x %x %d [", id, addr, flag, len);
            for (unsigned char uc : payload) { printf("%02X", 0xff & uc); }
            printf("]\n");
          }
          ret = I2CCommand::Write(id, seq, addr, flag, &payload);
        }
      }
    }
  } else if (verb == "i2c_reply:") { // i2c_reply: i2c-2 #1 a=04c f=0001 l=1 [34]
    int id = MunchI2C(line, &idx);
    if (id != -999) {
      int seq = MunchSeq(line, &idx);
      int addr = MunchAddress(line, &idx);
      if (addr != -1) {
        int flag = MunchFlag(line, &idx);
        if (flag != -1) {
          int len = MunchLength(line, &idx);
          std::vector<unsigned char> payload = MunchByteArray(line, &idx);
          if (g_debug) {
            printf(">>> REPLY %d %x %x %d [", id, addr, flag, len);
            for (unsigned char uc : payload) { printf("%02X ", 0xff & uc); }
            printf("]\n");
          }
          ret = I2CCommand::Reply(id, seq, addr, flag, &payload);
        }
      }
    }
  } else if (verb == "i2c_result:") {
    int id = MunchI2C(line, &idx);
    if (id != -999) {
      MunchWord(line, &idx);
      int result = MunchRet(line, &idx);
      if (g_debug) {
        printf(">>> RESULT %d %d\n", id, ret);
      }
      ret = I2CCommand::Result(id, result);
    }
  }
  return ret;
}

void PrintSomething(const char* header, struct i2c_msg* m) {
  printf("[%s] addr=%04x flags=%04x len=%d data=[", header, m->addr, m->flags, m->len);
  for (int i=0; i<m->len; i++) {
    printf("%02x ", m->buf[i] & 0xFF);
  }
  printf("]\n");
}

class I2CExecutor {
public:
  I2CExecutor() {
    total_usec = 0;
  }

  std::unordered_map<int, int> id2fd; // I2C ID to File Descriptor

  int GetFD(int i2c_id) {
    if (g_mock) { return i2c_id; }
    else {
      if (id2fd.find(i2c_id) == id2fd.end()) {
        char path[100];
        sprintf(path, "/dev/i2c-%d", i2c_id);
        int fd = open(path, O_RDWR);
        id2fd[i2c_id] = fd;
        printf("Opened %s, FD=%d\n", path, fd);
      }
      return id2fd[i2c_id];
    }
  }
  
  void CloseFD(int i2c_id) { id2fd.erase(i2c_id); }

  void FixMuxState(int i2c_id, const std::vector<int>& mux_state) {
    const int N = int(mux_state.size());
    if (N < 1) return;
    
    for (int i=0; i<N; i++) {
      struct i2c_msg msg;
      unsigned char buf[8];
      
      msg.addr = 0x71 + i;
      msg.flags = 0; // write
      msg.len = 1;
      msg.buf = buf;
      buf[0] = mux_state[i] & 0xff;
    
      struct i2c_rdwr_ioctl_data i2c_data;
      i2c_data.msgs = &msg;
      i2c_data.nmsgs = 1;
      const int fd = GetFD(i2c_id);
      int result;
      
      if (fd < 0) {
        printf("Error getting FD while trying to fix mux state\n");
        goto DONE;
      }
      
      result = ioctl(fd, I2C_RDWR, &i2c_data);
      if (result < 0) {
        printf("Error performing ioctl while trying to fix mux state: %s\n", strerror(errno));
        goto DONE;
      }
    }
    
    DONE:
      { }
  }
  
  // Must be a read-write pair
  // Must be of the same device ID
  // mux_state may be used to restore the mux state in case of a device not found error
  std::vector<I2CCommand*> ExecuteI2CCommandList(
      const std::vector<int>& mux_state, 
      const std::vector<I2CCommand*>& cmds) {
    const int N = int(cmds.size());
    if (g_verbose) {
      printf("Executing a command list of size %d\n", N);
    }

    std::vector<I2CCommand*> resp(N, nullptr), ret;
    struct i2c_msg *msgs = new struct i2c_msg[N];
    unsigned char *buf = new unsigned char[N*32];
    memset(buf, N*32, 0x00);
    int i2c_id = -999;

    for (int i=0; i<N; i++) {
      I2CCommand* cmd = cmds[i];
      assert(cmd->type == I2CCommand::I2CCommandType::WRITE ||
           cmd->type == I2CCommand::I2CCommandType::READ);
      if (i2c_id == -999) {
        i2c_id = cmd->id;
      } else { assert(cmd->id == i2c_id); }
      int fd = GetFD(cmd->id);
      
      struct i2c_msg *msg = &(msgs[i]);

      msg->addr = cmd->addr;
      msg->flags= cmd->type == I2CCommand::I2CCommandType::WRITE ? 0 : 1;
      msg->len  = cmd->type == I2CCommand::I2CCommandType::WRITE ? int(cmd->payload.size()) : cmd->len;
      msg->buf  = cmd->type == I2CCommand::I2CCommandType::WRITE ? cmd->payload.data() : &buf[i*32];
    }

    if (g_mock) {
      for (int i=0; i<N; i++) {
        printf("[Mock] %s\n", cmds[i]->ToString().c_str());
      }
    } else {
      struct i2c_rdwr_ioctl_data i2c_data;
      i2c_data.msgs = msgs;
      i2c_data.nmsgs = N;
      
      // print request
      if (g_verbose) {
        for (int i=0; i<N; i++) {
          char buf[22];
          sprintf(buf, "Req %d", i);
          PrintSomething(buf, &msgs[i]);
        }
      }
  
      int fd = GetFD(i2c_id);
  
      std::chrono::time_point<std::chrono::high_resolution_clock> t0, t1;
      t0 = std::chrono::high_resolution_clock::now();
      
      const int ATTEMPT_LIMIT = 2;
      
      int result;
      for (int a=0; a<ATTEMPT_LIMIT; a++) {
        result = ioctl(fd, I2C_RDWR, &i2c_data);
        if (result < 0) {
          fflush(stderr); fflush(stdout);
          fprintf(stderr, "ioctl error: i2c_id=%d, errno=%d, strerror=%s\n",
            i2c_id,
            errno, strerror(errno));
          if (a == 0) {
            printf("Trying to fix mux state\n");
            FixMuxState(i2c_id, mux_state);
          } else {
            printf("Trying to recreate FD\n");
            close(fd);
            CloseFD(i2c_id);
            fd = GetFD(i2c_id);
          }
        } else {
          if (a > 0) {
            printf("command list succeeded after %d attempts\n", a+1);
          };
          break;
        }
      }
      
      if (result < 0) { has_errors.push_back(true); } else { has_errors.push_back(false); }
      
      t1 = std::chrono::high_resolution_clock::now();
  
      float ms = std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
      total_usec += ms;
      
      if (g_verbose) {
        printf("Command list time: %g us\n", ms);
      }
      
      if (result >= 0 && g_verbose) {
        for (int i=0; i<N; i++) {
          char buf[22];
          sprintf(buf, "Result %d", i);
          PrintSomething(buf, &msgs[i]);
        }
      }
    }
    
    // Append results to the responses buffer
    // Only READ operations have responses
    for (int i=0; i<N; i++) {
      if (cmds[i]->type == I2CCommand::I2CCommandType::READ) {
        std::vector<unsigned char> r;
        int len = msgs[i].len;
        if (g_mock) {
          for (int j=0; j<len; j++) { r.push_back(0x00); }
        } else {
          for (int j=0; j<len; j++) { r.push_back(msgs[i].buf[j]); }
        }
        responses.push_back(r);
        i2c_idxes.push_back(cmds[i]->id);
        mux_states.push_back(mux_state);
      }
    }

    delete[] msgs; delete[] buf;
    
    return ret;
  }

  // Mux states, I2C indices and responses for each READ I2C operation
  std::vector<std::vector<int> > mux_states;
  std::vector<int> i2c_idxes;
  std::vector<std::vector<unsigned char> > responses;
  std::vector<bool> has_errors;
  float total_usec;
  void PrintSummary() {
    printf("[I2C Executor] total %g ms, %lu responses\n", total_usec/1000.0f, responses.size());
  }
  
  void PrintResponses() {
    for (int i=0; i<int(responses.size()); i++) {
      printf("%d:", i);
      for (unsigned char ch : responses[i]) {
        printf(" %02x", (ch & 0xFF));
      }
      printf("\n");
    }
  }
  
  void Reset() {
    total_usec = 0;
    responses.clear(); i2c_idxes.clear(); mux_states.clear(); has_errors.clear();
    CloseAllFDs();
  }
  
  void CloseAllFDs() {
    for (const std::pair<int, int>& p : id2fd) { close(p.second); }
    id2fd.clear();
  }
};

// A mux path is a list of numbers:
// For 1-level mux: [ byte written to addr 0x71 ]
// For 2-level mux: [ byte written to addr 0x71 ][ byte written to addr 0x72 ] 
class MuxState {
public:
  MuxState() {
    level0_addrs.clear();
    level1_addrs.clear();
  }
  void SetValue(int i2c_id, int level, int value) { 
    if (level == 0) {
      level0_addrs[i2c_id] = value;
    } else {
      const int level0_addr = level0_addrs[i2c_id];
      level1_addrs[i2c_id][level0_addr] = value;
    }
  }
  
  // The current path pointed to by the current configuration
  std::vector<int> GetCurrentPath(int i2c_id) const {
    std::vector<int> ret;
    const int level0_addr = level0_addrs.find(i2c_id) != level0_addrs.end() ? level0_addrs.at(i2c_id) : 0;
    ret.push_back(level0_addr);
    
    if (level1_addrs.find(i2c_id) != level1_addrs.end()) {
      const std::map<int, int>* l1 = &(level1_addrs.at(i2c_id));
      if (l1->find(level0_addr) != l1->end()) {
        ret.push_back(l1->at(level0_addr));
      } else {
        ret.push_back(0); // 0-padded
      }
    }
    return ret;
  }
  
  static std::string PathToString(const std::vector<int>& p) {
    std::stringstream ss;
    for (size_t i=0; i<p.size(); i++) {
      if (i > 0) ss << "-";
      ss << p[i];
    }
    return ss.str();
  }
  
  std::string GetCurrentPathAsString(int i2c_id) const {
    return PathToString(GetCurrentPath(i2c_id));
  }
  
  std::string ToString() {
    std::stringstream ss;
    for (const std::pair<int, int>& p : level0_addrs) {
      ss << "i2c-" << p.first << ": ";
      const int level0_addr = level0_addrs[p.first];
      ss << "Level0=" << level0_addr << " Level1: ";
      for (const std::pair<int, int>& p : level1_addrs[p.first]) {
        ss << " " << p.first << "=" << p.second;
      }
      ss << "\n";
    }
    return ss.str(); 
  }

private:
  std::map<int, int>                 level0_addrs; // I2C ID -> Level0 Addr
  std::map<int, std::map<int, int> > level1_addrs; // I2C ID -> 0x71 -> 0x72
};

// Non-overlapping blocks
class I2CTxn {
public:
  // The cmdlists might contain multiple transactions
  static std::vector<I2CTxn*> ParseCmds(int indent, const std::vector<I2CCommand*>& cmdlists,
    int lb, int ub) { // lower bound, upper bound
  
    std::vector<I2CTxn*> ret;
    if (lb > ub) return ret;
  
    if (g_verbose) {
      for (int i=0; i<indent; i++) { printf("  "); }
      printf("[ParseCmds] [%d,%d]\n", lb, ub);
    }
    
    std::vector<I2CCommand*> req, resp;
    
    // Find all requests
    const int N = int(cmdlists.size());
    int i=lb, j=lb;
    
    while (true) {
      req.clear(); resp.clear();
      int i2c_id = cmdlists[i]->id;
      int i0 = i;
      for (; i<N; i++) {
        I2CCommand* cmd = cmdlists[i];
        if (cmd->id == i2c_id && cmd->IsRequest()) {
          req.push_back(cmdlists[i]);
        } else {
          break;
        }
      }
      
      if (req.size() < 1) break;
      int iidx = int(req.size()-1);
      int j0 = lb;
      for (j=i; j<N; j++) {
        if (I2CCommand::IsRequestResponsePair(req[iidx], cmdlists[j])) {
          j0 = j-1;
          for (; j<N; j++) {
            if (I2CCommand::IsRequestResponsePair(req[iidx], cmdlists[j])) {
              iidx --;
              resp.push_back(cmdlists[j]);
              if (iidx < 0) {
                j++;
                goto DONE; 
              }
            }
          }
        }
      }
      DONE: {}
      std::reverse(resp.begin(), resp.end());
      if (g_verbose) {
        for (int k=0; k<indent; k++) printf("  ");
        printf("[%d,%d] ", i0, j-1);
        printf("|req|=%lu |resp|=%lu  ", req.size(), resp.size());
        printf("req:"); for (I2CCommand* cmd : req) { printf(" %s", cmd->ToString().c_str()); }
        printf(" resp:"); for (I2CCommand* cmd : req) { printf(" %s", cmd->ToString().c_str()); }
        printf("\n");
      }
        
      I2CTxn* txn = new I2CTxn(req, resp, ParseCmds(indent+1, cmdlists, i, j0));
      ret.push_back(txn);
      if (j > ub) break;
      else i = j;
    }
    
    return ret;
  }
  I2CTxn(std::vector<I2CCommand*> req, std::vector<I2CCommand*> resp,
         std::vector<I2CTxn*> ch) {
    request = req; response = resp; children = ch;
  }
  
  // For usage with the corresponding p5js sketch
  std::string ToJSON() {
    std::stringstream ss;
    ss << "{ req : [ ";
    for (I2CCommand* cmd : request) {
      ss << cmd->ToJSONForViz();
      ss << ", ";
    }
    ss << "], \nchildren: ";
    if (children.empty()) { ss << "[],"; }
    else { 
      ss << "[ ";
      for (I2CTxn* ch : children) {
        ss << ch->ToJSON() << ",";
      }
      ss << "],";
    }
    ss << "\nresp: [";
    for (int i=int(response.size())-1; i>=0; i--) {
      ss << response[i]->ToJSONForViz();
      ss << ", ";
    }
    ss << "] }";
    return ss.str();
  }
  
  // Determines whether a sequence of operations is a MUX operation
  static std::vector<std::vector<int> > FindMuxOperations(
    const std::vector<I2CTxn*>& txns) {
  
    printf("FindMuxOperations %lu txns\n", txns.size());
  
    std::vector<std::vector<int> > ranges; // [ In LB, In UB, Out LB, Out UB ]

    bool has_in = false, has_out = false;
    int in_lb, in_ub, out_lb, out_ub;
    
    for (int i=0; i<int(txns.size()); i++) {
      I2CTxn *t0 = txns[i], *t1 = txns[i+1], *t2 = txns[i+2], *t3 = txns[i+3];
      
      if (t0->request.size() == 1) {
        // Mux Close contains 4 transactions:
        // 1) write X          to address 0x71
        // 2) write 0          to address 0x72
        // 3) write 0          to address 0x71
        int id = t0->request.back()->id;
        
        if (t0->IsWriteSingleValueToAddress(id, 0x71, -1) && // This must match
            t1->IsWriteSingleValueToAddress(id, 0x72, 0) && // -1: wildcard
            t2->IsWriteSingleValueToAddress(id, 0x71, 0)) {
          has_out = true; out_lb = i; out_ub = i+2;
          printf("I2C End Sequence found, i=%d\n", i);
        }
        // Mux Open contains 4 transactions:
        // 1) write X          to address 0x71
        // 2) write some value to address 0x72
        // 3) write 0          to address 0x71
        else if (t0->IsWriteSingleValueToAddress(id, 0x71, -1) &&
            t1->IsWriteSingleValueToAddress(id, 0x72, -1) && // -1: wildcard
            t2->IsWriteSingleValueToAddress(id, 0x71, 0)) {
          printf("I2C Start Sequence found, i=%d, 0x%02x\n", i,
            t1->request[0]->payload[0]);
          has_in = true; in_lb = i; in_ub = i+2;
        }
      }
                                                             
      if (has_in && has_out) {
        ranges.push_back({ in_lb, in_ub, out_lb, out_ub });
      }
    }
    
    return ranges;
  }
  std::vector<I2CCommand*> request, response;
  std::vector<I2CTxn*> children;
  
  static bool IsNotMuxWrite(I2CTxn* txn) {
    if (txn == nullptr) return true;
    const int id = txn->request.back()->id;
    if (txn->IsWriteSingleValueToAddress(id, 0x71, -1) == false) { return true; }
    if (txn->IsWriteSingleValueToAddress(id, 0x72, -1) == false) { return true; }
    return false;
  }
  
  // returns # of operations
  // 2-layer Muxes: 3 operations, 0x71 then 0x72
  // 1-layer      : 1 operation,  0x71
  static int IsMuxOpenOrCloseSequence(const std::vector<I2CTxn*> txns,
    int idx,
    bool* is_open, MuxState* mux_state) {
    I2CTxn* t0 = nullptr, *t1 = nullptr, *t2 = nullptr;
    size_t N = txns.size();
    if (idx   < N) t0 = txns[idx];
    if (idx+1 < N) t1 = txns[idx+1];
    if (idx+2 < N) t2 = txns[idx+2];
    const int id = t0->request.back()->id;
    /*
    if (t0 && t1 && // Close 2-level Mux
        t0->IsWriteSingleValueToAddress(id, 0x71, -1) && // Should match
        t1->IsWriteSingleValueToAddress(id, 0x72, 0)) {
      if (is_open) { *is_open = false; }
      return 2;
    } else if (t0 && t1 && // Open 2-level Mux
        t0->IsWriteSingleValueToAddress(id, 0x71, -1) &&
        t1->IsWriteSingleValueToAddress(id, 0x72, -1) // -1: wildcard
        ) {
      if (is_open) { *is_open = true; }
      if (mux_path) { 
        mux_path->SetValue(0, t0->request[0]->payload[0]);
        mux_path->SetValue(1, t1->request[0]->payload[0]);
      }
      return 2;
    */
    if (t0->IsWriteSingleValueToAddress(id, 0x72, 0)) {
      if (is_open) { *is_open = false; }
      if (mux_state) { mux_state->SetValue(id, 1, 0); }
      return 1;
    } else if (t0->IsWriteSingleValueToAddress(id, 0x72, -1)) {
      if (is_open) { *is_open = true; }
      if (mux_state) { mux_state->SetValue(id, 1, t0->request[0]->payload[0]); }
      return 1;
    } else if (t0->IsWriteSingleValueToAddress(id, 0x71,  0)) {
      if (mux_state) { mux_state->SetValue(id, 0, 0); }
      if (is_open) { *is_open = false; }
      return 1;
    } else if (t0->IsWriteSingleValueToAddress(id, 0x71, -1)) {
      if (is_open) { *is_open = true; }
      if (mux_state) { mux_state->SetValue(id, 0, t0->request[0]->payload[0]);  }
      return 1;
    } 
    return 0;
  }
  
  static std::vector<I2CTxn*> GetInnermostLevelTxns(const std::vector<I2CTxn*>& txns) {
    std::vector<I2CTxn*> ret;
    for (I2CTxn* t : txns) { do_GetInnermostLevelTxns(&ret, t); }
    return ret;
  }
  
  int GetI2CID() const { return request[0]->id; }
  
  class TxnIterator {
  public:
    TxnIterator(const std::vector<I2CTxn*> txns) { 
      this->txns = txns;
      Init();
    }
    TxnIterator& operator++() {
      return *this;
    }
    bool Ended() {
      return (idx >= txns.size()-1);
    }
    
    void Init() {
      mux_states.clear();
      mux_op_idxes.clear();
      const int N = int(txns.size());
      
      std::unordered_map<int, MuxState> allmuxstates;
      
      int i=0;
      while (i < N) {
        bool is_open;
        const int i2c_idx = txns[i]->GetI2CID();
        MuxState* ms = &(allmuxstates[i2c_idx]);
        
        // Mux state of the I2C device that's touched by this transaction
        mux_states[txns[i]] = *ms;
        
        int num_ops = IsMuxOpenOrCloseSequence(txns, i, &is_open, ms);
        
        if (num_ops > 0) {
          mux_op_idxes.push_back(i); 
          i += num_ops;
          allmuxstates[i2c_idx] = *ms;
        } else {
          i++;
          allmuxstates[i2c_idx] = *ms;
        }
      }
    }
    
    // TODO: Fix this function
    std::vector<bool> FindSkippableMuxOps() const {
      // Iterative
      std::vector<bool> ret;
      const int NS = int(mux_op_idxes.size());
      std::map<int, MuxState> temp; // mux state of the active I2C device at the beginning of [0], [1], ..., etc
      temp[0] = MuxState();
      
      for (int n=0; n<NS; n++) {
        
        if (n == 0 || (n % 100 == 99) || n == NS-1) {
          printf("[FindSkippableMuxOps] Progress: %d / %d      \n", (n+1), NS);
          fflush(stdout);
        }
        
        const int tidx_victim = mux_op_idxes[n];
        const I2CTxn* victim = txns[tidx_victim];
        const int i2c_id = victim->GetI2CID();
        
        int tidx1 = temp.rbegin()->first;
        int i2c_id1 = txns[tidx1]->GetI2CID();
        std::map<int, MuxState> allmuxstates; // for all i2c devices at the current point in time
        allmuxstates[i2c_id1] = temp.at(tidx1);
        
        //printf("Attempting to skip txn#%d on i2c-%d; tidx1=%d\n", tidx_victim, i2c_id, tidx1);
        
        std::unordered_set<int> victims;
        for (size_t i=0; i<ret.size(); i++) {
          if (ret[i]) { victims.insert(mux_op_idxes[i]); }
        }
        victims.insert(tidx_victim);
        
        bool done = false, ok = true;
        while (tidx1 < int(txns.size())) {
          I2CTxn* curr = txns[tidx1];
          MuxState* ms = &(allmuxstates[curr->GetI2CID()]);
          temp[tidx1] = *ms;
          
          if (victims.count(tidx1)) { tidx1++; continue; }
          int num_ops = IsMuxOpenOrCloseSequence(txns, tidx1, nullptr, ms);
          if (num_ops > 0) {
            tidx1 += num_ops;
          } else {
            if (!done) {
              const int i2c_id        = txns[tidx1]->GetI2CID();
              const int i2c_id_victim = victim->GetI2CID();
              if (i2c_id == i2c_id_victim) {
                MuxState ms_orig = mux_states.at(txns.at(tidx1));
                if (ms_orig.GetCurrentPath(i2c_id) == ms->GetCurrentPath(i2c_id)) {
                  ok = true; 
                  done = true;
                } else {
                  ok = false;
                  done = true;
                }
              }
            }
            tidx1++;
          }
        }
        
        //printf("Reached the end; ok=%d\n", ok);
        
        if (ok) {
          ret.push_back(true);
        } else {
          ret.push_back(false);
        }
        
        {
          while (!temp.empty() && temp.rbegin()->first >= tidx_victim-2 &&
                                  temp.rbegin()->first > 0) {
            temp.erase(temp.rbegin()->first);
          }
        }
      }
      return ret;
    }
    
    // skips are used to index into mux_op_idxes
    std::vector<I2CTxn*> SkipMuxOps(const std::vector<bool>& skips) {
      std::vector<I2CTxn*> ret;
      const int N = int(txns.size());
      int midx = 0;
      for (int i=0; i<N; i++) {
        if (midx < mux_op_idxes.size() && i == mux_op_idxes[midx]) { // Is a Mux Op
          if (skips[midx] == false) {
            ret.push_back(txns[i]);
          }
          midx ++;
        } else {
          ret.push_back(txns[i]);
        }
      }
      return ret;
    }
  
  const std::vector<int>& GetMuxOpIdxes() { return mux_op_idxes; }
  const std::unordered_map<I2CTxn*, MuxState> GetMuxStates() { return mux_states; }
  
  std::string GetSummary() {
    std::stringstream ss;
    ss << txns.size() << " txns, " << mux_op_idxes.size() << " mux ops, "
       << mux_states.size() << " non-mux ops\n";
    return ss.str();
  }
  
  private:
    std::unordered_map<I2CTxn*, MuxState> mux_states;
    std::vector<I2CTxn*> txns;
    std::vector<int> mux_op_idxes;
    int idx = 0;
  };
  
  // Input: list of I2C transactions
  // Output: list of I2C transactions with possible redundant Mux ops removed
  static std::vector<I2CTxn*> SimplifyMuxOps(const std::vector<I2CTxn*>& txns) {

    // Simulate Mux states
    const int N = int(txns.size());
    TxnIterator itr(txns);
    printf("Getting Mux Op Idxes\n");
    std::vector<int> mux_op_idxes = itr.GetMuxOpIdxes();
    printf("Getting Mux States\n");
    std::unordered_map<I2CTxn*, MuxState> mux_states = itr.GetMuxStates();
    printf("Getting list of Mux Ops\n");
    const int NS = int(mux_op_idxes.size());
    std::vector<I2CTxn*> mux_ops;
    for (const int idx : mux_op_idxes) { mux_ops.push_back(txns[idx]); }
    
    printf("%lu vs %d\n", mux_ops.size(), NS);
    std::vector<int> i2c_ids(NS, -1);
    for (int i=0; i<NS; i++) { i2c_ids[i] = mux_ops[i]->GetI2CID();  }
    
    if (g_debug) {
      printf("%s\n", itr.GetSummary().c_str());
      for (size_t i=0; i<txns.size(); i++) {
        printf("[%d]: %s\n", i, txns[i]->ToString().c_str());
      }
    }
    
    std::vector<bool> skips(NS);
    
    std::vector<I2CTxn*> ret = txns;
    
    std::vector<bool> skipped = itr.FindSkippableMuxOps();
    ret = itr.SkipMuxOps(skipped);
    
    if (g_debug) { // Slow but safe method
      printf("\n");
      for (int i=0; i<NS; i++) {
        if (i == 0 || (i % 100 == 99) || i == NS-1) {
          printf("\rProgress: %d / %d      ", (i+1), NS);
          fflush(stdout);
        }
        std::vector<bool> skips1 = skips;
        // Attempt to remove each of the mux operations and see how many we can 
        // remove without affecting the read operations
        skips1[i] = true;
        const int i2c_id_affected = i2c_ids[i];
        
        std::vector<I2CTxn*> txns2 = itr.SkipMuxOps(skips1);
        
        TxnIterator iter2(txns2); // This constructor takes O(n) to run
        
        std::unordered_map<I2CTxn*, MuxState> mux_states2 = iter2.GetMuxStates();
        
        bool ok = true;
        if (mux_states2.size() != mux_states.size()) {
          ok = false;
        } else {
          for (const std::pair<I2CTxn*, MuxState>& p : mux_states2) {
            I2CTxn* txn = p.first;
            const int i2c_id = txn->GetI2CID();
            if (i2c_id_affected != i2c_id) { continue; }
            if (mux_states.find(txn) != mux_states.end()) {
              std::vector<int> path2 = p.second.GetCurrentPath(i2c_id);
              std::vector<int> path1 = mux_states[txn].GetCurrentPath(i2c_id);
              if (path2 != path1) {
                ok = false; break;
              }
            }
          }
        }
        
        if (ok) {
          if (g_debug) { printf("Mux operation #%d is safe to skip\n", i); }
          skips = skips1;
          ret = txns2;
        }
      }
      printf("\n");
    }

    return ret;
  }
  
  // Input: list of transactions
  // Output: I2C command batches (list of list of WRITE and READ commands)
  //         extracted from the list of transactions;
  static std::vector<std::pair<std::vector<int>, // Current I2C Mux Path
                               std::vector<I2CCommand*>  // One I2C Command List
                               > > ToI2CCommandList(const std::vector<I2CTxn*> txns, bool must_innermost) {
    std::vector<std::pair<std::vector<int>, std::vector<I2CCommand*> > > ret;
    
    const int WRITE_LIMIT = 1, READ_LIMIT = 1;
    int nwrite = 0, nread = 0;
    
    // Compute I2C mux states
    TxnIterator itr(txns);
    const std::unordered_map<I2CTxn*, MuxState>& mux_states = itr.GetMuxStates();
    
    std::vector<I2CCommand*> batch;
    int last_i2c_idx = -999;
    int i2c_id = 0;
    MuxState ms;
    for (I2CTxn* t : txns) {
      assert(mux_states.count(t));
      i2c_id = t->GetI2CID();
      ms = mux_states.at(t);
      
      if (must_innermost) {
        assert(t->children.empty() == true);
      }
      
      std::vector<I2CCommand*> reqs = t->request;
      for (int i=0; i<int(reqs.size()); i++) {
        I2CCommand* cmd = reqs[i];
        const int i2c_idx = cmd->id;
        
        int nread_next = nread, nwrite_next = nwrite;
        switch (cmd->type) {
          case I2CCommand::I2CCommandType::READ: nread_next = nread+1; break;
          case I2CCommand::I2CCommandType::WRITE: nwrite_next = nwrite+1; break;
          default: break;
        }
        
        if (i2c_idx != last_i2c_idx || nwrite_next > WRITE_LIMIT || nread_next > READ_LIMIT) {
          if (!batch.empty()) {
            ret.push_back(std::make_pair(ms.GetCurrentPath(i2c_id), batch));
          }
          batch.clear();
          nwrite_next -= nwrite;
          nread_next -= nread;
        }
        
        batch.push_back(cmd);
        
        last_i2c_idx = i2c_idx;
        nwrite = nwrite_next;
        nread = nread_next;
      }
    }
    
    if (!batch.empty()) {
      ret.push_back(std::make_pair(ms.GetCurrentPath(i2c_id), batch));
    }
    
    return ret;
  }
  
  std::string ToString() {
    std::stringstream ss;
    for (I2CCommand* r : request) {
      ss << r->ToString() << "\n";
    }
    for (I2CCommand* r : response) {
      ss << r->ToString() << "\n";
    }
    return ss.str();
  }
  
private:
  bool IsWriteSingleValueToAddress(int id, int address, int write_val) {
    if (request.size() > 1) return false;
    I2CCommand* cmd = request.back();
    if (cmd->type != I2CCommand::I2CCommandType::WRITE) return false;
    if (cmd->addr != address) return false;
    if (write_val != -1 && cmd->payload[0] != write_val) return false;
    if (cmd->id != id) return false;
    return true;
  }
  
  static void do_GetInnermostLevelTxns(std::vector<I2CTxn*>* out, I2CTxn* txn) {
    if (txn->children.empty()) {
      out->push_back(txn);
      return;
    } else {
      for (I2CTxn* ch : txn->children) {
        do_GetInnermostLevelTxns(out, ch);
      }
    }
  }
};

void ReadHwmonPath(const std::string& filename) {
  I2CTopologyMap m;
  std::ifstream ifs(filename);
  while (ifs.good()) {
    std::string line;
    std::getline(ifs, line);
    m.ReadHwmonPath(line);
  }
  m.Print();
}

void ProcessFile(const std::string& filename, bool dump_json) {
  printf("[ProcessFile] filename=%s\n", filename.c_str());
  double uptime0, uptime1;
  if (!g_mock) { 
    EnableKernelTracing();
    uptime0 = GetUptime();
  }

  std::ifstream ifs(filename.c_str());
  std::vector<I2CCommand*> cmds;
  while (ifs.good()) {
    std::string line;
    std::getline(ifs, line);
    I2CCommand* cmd = ParseLine(line);
    if (cmd != nullptr) cmds.push_back(cmd);
  }

  printf("%d commands total in the file\n", int(cmds.size()));
  
  std::vector<I2CTxn*> txns = I2CTxn::ParseCmds(0, cmds, 0, int(cmds.size()-1));
  
  //std::vector<std::vector<int> > i2cmuxs = I2CTxn::FindMuxOperations(txns);
  //printf("%lu Mux op pairs\n", i2cmuxs.size());
  
  std::vector<I2CTxn*> txns_innermost = I2CTxn::GetInnermostLevelTxns(txns);
  
  printf("%lu innermost transactions in the file\n", txns_innermost.size());
  if (g_debug) {
    const int ni = int(txns_innermost.size());
    for (int i=0; i<ni; i++) {
      printf("%d: %s\n", i, txns_innermost[i]->ToString().c_str());
      if (i < ni-3) {
        bool is_open = false; MuxState mux_state; int num_ops = -999;
        num_ops = I2CTxn::IsMuxOpenOrCloseSequence(txns_innermost, i, &is_open, &mux_state);
        if (num_ops > 0) {
          if (is_open) {
            printf("[%d] is a MUX Open; it opens MUX at device %d address %s\n",
              i, txns_innermost[i]->GetI2CID(), mux_state.ToString().c_str());
          } else {
            printf("[%d] is a MUX Close; it closes MUX at device %d\n",
              i, txns_innermost[i]->GetI2CID());
          }
        }
      }
    }
  }
  
  std::vector<I2CTxn*> txns_innermost_simp = I2CTxn::SimplifyMuxOps(txns_innermost);
  
  printf("Simplified innermost txn set has %lu txns\n", txns_innermost_simp.size());
  
  if (g_debug) {
    const int ni2 = int(txns_innermost_simp.size());
    for (int i=0; i<ni2; i++) {
      printf("%d: %s\n", i, txns_innermost_simp[i]->ToString().c_str());
    }
  }
  
  // Dump the original transaction list (may contain nested transactions)
  // into a JSON file for visualization
  if (dump_json) {
    printf("[");
    for (I2CTxn* t : txns) { printf("%s,\n", t->ToJSON().c_str()); }
    printf("]\n");  
    return;
  }

  // May need to correct I2C mux state before a command list in case it was modified by other programs
  std::vector<std::pair<std::vector<int>, std::vector<I2CCommand*> > >
      batches_orig = I2CTxn::ToI2CCommandList(txns_innermost, true),
      batches_simp = I2CTxn::ToI2CCommandList(txns_innermost_simp, true);
  printf("Original transaction list has %d batches, simplified has %d batches\n",
    int(batches_orig.size()), int(batches_simp.size()));
  
  std::vector<std::vector<unsigned char> > responses_orig, responses_opt;
  std::vector<int> i2c_idxes_orig, i2c_idxes_opt;
  std::vector<std::vector<int> > mux_states_orig, mux_states_opt;
  std::vector<bool> has_errors_orig, has_errors_opt;
  
  I2CExecutor exec;
  printf("Executing the original batches consisting of %lu batches\n", batches_orig.size());
  exec.Reset();
  for (const std::pair<std::vector<int>, std::vector<I2CCommand*> > & batch : batches_orig) {
    exec.ExecuteI2CCommandList(batch.first, batch.second);
  }
  responses_orig = exec.responses;
  i2c_idxes_orig = exec.i2c_idxes;
  mux_states_orig = exec.mux_states;
  has_errors_orig = exec.has_errors;
  exec.PrintSummary();
  
  if (g_sleep_secs > 0) {
    printf("Sleeping for %d secs\n", g_sleep_secs);
    sleep(g_sleep_secs);
  }
  
  exec.Reset();
  printf("Executing the opt batches consisting of %lu batches\n", batches_simp.size());
  for (const std::pair<std::vector<int>, std::vector<I2CCommand*> > & batch : batches_simp) {
    exec.ExecuteI2CCommandList(batch.first, batch.second);
  }
  responses_opt = exec.responses;
  i2c_idxes_opt = exec.i2c_idxes;
  mux_states_opt = exec.mux_states;
  has_errors_opt = exec.has_errors;
  exec.PrintSummary();
  exec.Reset();
  
  printf("Results comparison\n");
  std::cout << std::setw(5) << " ";
  std::cout << std::setw(8) << "I2C#" << " " << std::setw(10) << "MuxState" << " " <<
               std::setw(20) << "original" << " " <<
               std::setw(4)  << "err?" << 
               std::setw(20) << "optimized" << " " <<
               std::setw(4)  << "err?" <<
               std::endl;
  
  if (responses_orig.size() != responses_opt.size()) {
    printf("Error! Sizes of response arrays do not match (orig=%lu vs opt=%lu).\n",
      responses_orig.size(), responses_opt.size());
  }
               
  const size_t sz = std::min(responses_opt.size(), responses_orig.size());
  for (size_t i=0; i<sz; i++) {
    std::stringstream ss;
    ss.setf(std::ios::right);
    ss << std::setw(5) << i << " ";
    ss << std::setw(8) << i2c_idxes_orig[i] << " ";
    ss << std::setw(10) << MuxState::PathToString(mux_states_orig[i]) << " ";
    ss << std::setw(20); 
    if (i < responses_orig.size()) {
      ss << ByteArrayToString(responses_orig[i]);
    } else { ss << " "; }
    ss << std::setw(4);
    if (has_errors_orig[i]) { ss << " x "; } else { ss << " "; }
    ss << std::setw(20);
    if (i < responses_opt.size()) {
      ss << ByteArrayToString(responses_opt[i]);
    } else { ss << " "; }
    ss << std::setw(4);
    if (has_errors_opt[i]) { ss << " x "; } else { ss << " "; }
    printf("%s\n", ss.str().c_str());
  }
  
  if (!g_mock) {
    uptime1 = GetUptime();
    const double DELTA = 2.0;
    DisableKernelTracing();
    if (g_verbose) FindTraceEntries(uptime0-DELTA, uptime1);
  }
}

int main(int argc, char** argv) {
  
  char* x = getenv("MOCK");
  if (x && std::atoi(x) == 1) {
    g_mock = true;
  }
  
  x = getenv("VERBOSE"); if (x && std::atoi(x) == 1) { g_verbose = true; }
  x = getenv("DEBUG"); if (x) { g_debug = true; }
  x = getenv("SLEEP"); if (x) { g_sleep_secs = std::max(0, std::atoi(x)); }
  
  g_argv0 = basename(argv[0]);
  if (argc <= 1) {
    printf("Usage: \n");
    printf("%s I2C_TRACE_FILE          -> Process I2C trace file\n");
  }
  else if (argc >= 2) {
    std::string argv1(argv[1]);
    if (argv1 == "readhwmonpath" && argc == 3) {
      ReadHwmonPath(std::string(argv[2]));
    } else if (argv1 == "i2ctopomap" && argc == 2) {
      I2CTopologyMap m;
      m.TraverseI2C();
      printf("I2C Topo Map:\n");
      m.Print();
    } else if (argv1 == "dumpjson" && argc == 3) {
      ProcessFile(std::string(argv[2]), true);
    } else {
      ProcessFile(argv1, false);
    }
    return 0;
  }
}
