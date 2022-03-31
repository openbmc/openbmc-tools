#include "i2c_topology.hpp"

void I2CTopologyMap::LoadDummyData() {
  AddEdge(0, 16);
  AddEdge(0, 17);
  AddEdge(0, 18);
  AddEdge(0, 19);
  AddEdge(1, 20);
  AddEdge(1, 21);
  AddEdge(1, 22);
  AddEdge(1, 23);
  AddEdge(-1, 2);
  AddEdge(5, 24);
  AddEdge(5, 25);
  AddEdge(5, 26);
  AddEdge(5, 27);
  AddEdge(6, 28);
  AddEdge(6, 29);
  AddEdge(6, 30);
  AddEdge(6, 31);
  AddEdge(-1, 8);
  AddEdge(9, 32);
  AddEdge(9, 33);
  AddEdge(9, 34);
  AddEdge(9, 35);
  AddEdge(10, 36);
  AddEdge(10, 37);
  AddEdge(10, 38);
  AddEdge(10, 39);
  AddEdge(-1, 12);
  AddEdge(-1, 13);
  AddEdge(-1, 14);
}