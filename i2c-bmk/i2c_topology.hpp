#ifndef __I2C_TOPOLOGY_MAP__
#define __I2C_TOPOLOGY_MAP__

#include <vector>
#include <set>
#include <string>

namespace {
  std::string ExtractRelativePath(const std::string& s) {
    std::string::size_type idx = s.rfind("/");
    if (idx == std::string::npos) return s;
    else return s.substr(idx+1);
  }
}

class I2CTopologyMap {
public:
  struct I2CNode {
    int parent_idx;
    std::set<int> children; 
    I2CNode() {
      parent_idx = -1;
    }
  };
  std::vector<I2CNode*> nodes;
  
  I2CTopologyMap(int n) {
    nodes.resize(n);
  }
  
  I2CTopologyMap() {
    nodes.resize(256);
  }
 
  I2CNode* GetNodeByIndex(int idx) {
    if (nodes[idx] == nullptr) { 
      nodes[idx] = new I2CNode();
    }
    return nodes[idx];
  }

  void AddEdge(int parent, int child) {
    printf("addedge %d %d\n", parent, child);
    if (parent != -1) {
      I2CNode* p = GetNodeByIndex(parent), *c = GetNodeByIndex(child);
      p->children.insert(child);
      c->parent_idx = parent;
    } else { // add root node
      GetNodeByIndex(child);
    }
  }
  
  // Extracts the I2C nodes that lead to an HwMon
  void ReadHwmonPath(const std::string& path) {
    printf("[ReadHwmonPath]\n");
    std::string::size_type idx = 0, idx_next;
    std::vector<int> i2c_ids;
    while (true) {
      idx_next = path.find("/", idx);
      if (idx_next == std::string::npos) { break; }
      const std::string& chunk = path.substr(idx, idx_next-idx);
      if (chunk.find("i2c-") == 0) {
        printf("%s\n", chunk.c_str());
        int i2c_id = std::atoi(chunk.substr(4).c_str());
        i2c_ids.push_back(i2c_id);
      }
      idx = idx_next + 1;
    }
    
    for (size_t i=0; i+1<i2c_ids.size(); i++) {
      AddEdge(i2c_ids[i], i2c_ids[i+1]);
    }
  }
  
private:
  // Example: /sys/class/i2c-dev/i2c-0/
  void do_TraverseI2C(const std::string& d,
    std::vector<int>* parents,              // the path taken
    std::vector<std::vector<int>>* parents1 // includes all devices under the node
    ) {
    printf(">> %s ", d.c_str());
    for (int x : *parents) { printf(" %d", x); }
    printf("\n");
    std::string d1 = d;
    if (parents->size() > 1) { d1 += "/device"; }
    
    bool has_self = false;
    std::vector<int> children;
    std::vector<std::string> children_paths;
    
    if (std::filesystem::exists(d1)) {
      for (const auto& entry : std::filesystem::directory_iterator(d1)) {
        const std::string& path = entry.path();
        const std::string& relpath = ExtractRelativePath(path);
        if (relpath.find("i2c-") == 0) {
          std::string i2c_id;
          for (int i=4; i<int(relpath.size()); i++) {
            const char c = relpath[i];
            if (c >= '0' && c <= '9') {
              i2c_id.push_back(c);
            }
          }
          int x = std::atoi(i2c_id.c_str());
          bool found = false;
          for (const std::vector<int> yy : *parents1) {
            for (const int y : yy) {
              if (y == x) {
                found = true;
              }
            }
          }
          if (!found && !i2c_id.empty()) {
            children.push_back(x);
            children_paths.push_back(path);
            if (x == parents->back()) {
              has_self = true;
            }
          }
        }
      }
    }
    
    bool is_root = (parents->size() == 1);
    
    if (!has_self) {
      if (!is_root) { parents1->push_back(children); }
      for (int i=0; i<int(children.size()); i++) {
        AddEdge(parents->back(), children[i]);
        parents->push_back(children[i]);
        do_TraverseI2C(children_paths[i], parents, parents1);
        parents->pop_back();
      }
      if (!is_root) { parents1->pop_back(); }
    }
  }
  
public:
  // Traverse "/sys/class/i2c-dev" for I2C topology
  void TraverseI2C() {
    std::vector<int> par = { -1 };
    std::vector<std::vector<int> > par1;
    do_TraverseI2C("/sys/class/i2c-dev", &par, &par1);
    root_buses_ = FindRootBuses();
    for (int n : root_buses_) {
      printf("Root bus %d, apb addr: %s\n", n, apb_addresses_[n].c_str());
    }
  }

  std::vector<int> GetRootBuses() {
    return root_buses_;
  }
  
  std::string GetAPBAddress(int bus_id) {
    if (apb_addresses_.count(bus_id) == 0) {
      return "";
    }
    return apb_addresses_[bus_id];
  }
  
private:
  // Traverse "/sys/devices/platform/ahb/ahb:apb" for a list of physical
  // I2C buses.
  // The results from TraverseI2C() and FindRootBuses() should be the same.
  std::vector<int> FindRootBuses() {
    std::vector<int> ret;
    std::string ahb_apb_path = "/sys/devices/platform/ahb/ahb:apb/";
    if (!std::filesystem::exists(ahb_apb_path)) {
      // log error
      printf("Error: Could not find %s\n", ahb_apb_path.c_str());
      return ret;
    }
    for (const std::filesystem::directory_entry& entry : 
      std::filesystem::directory_iterator(ahb_apb_path)) {
      const std::string& path = entry.path();
      printf(">> %s\n", path.c_str());
      if (path.find(".i2c") == path.size()-4) {
        int num_i2c_devices = 0;
        int first_i2c_idx = -999;
        for (const std::filesystem::directory_entry& entry1 :
          std::filesystem::directory_iterator(path)) {
          const std::string path1 = entry1.path();
          printf("  >> %s\n", path1.c_str());
          const std::string file_path = ExtractRelativePath(path1);
          if (file_path.find("i2c-") == 0) {
            if (num_i2c_devices == 0) {
              first_i2c_idx = std::atoi(file_path.c_str() + 4);
            }
            num_i2c_devices ++;
          }
        }
        if (num_i2c_devices == 1) {
          root_buses_.push_back(first_i2c_idx);
          // Example value: "f0082000.i2c"
          const std::string file_path = ExtractRelativePath(path);
          apb_addresses_[first_i2c_idx] = file_path.substr(0, file_path.size()-4);
          ret.push_back(first_i2c_idx);
        }
      }
    }
    return ret;
  }
  
public:
  void do_Print(int indent, int idx) {
    for (int i=0; i<indent; i++) {
      printf("  ");
    }
    printf("%d\n", idx);
    if (idx != -1) {
      I2CNode* node = nodes.at(idx);
      for (int child : node->children) {
        do_Print(indent+1, child);
      }
    }
  }
  
  void Print() {
    for (size_t i=0; i<nodes.size(); i++) {
      I2CNode* n = nodes[i];
      if (n && n->parent_idx == -1) {
        do_Print(0, i);
      }
    }
  }
  
  void LoadDummyData();
  
private:
  std::vector<int> root_buses_;
  std::unordered_map<int, std::string> apb_addresses_;
};

#endif