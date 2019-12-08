#include <bits/stdc++.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <vector>

using std::cout;
using std::unordered_set;
using std::string;
using std::to_string;

class Node {
 public:
  int id;     // id of this node
  int dest;   // id of destinatoin node
  int dur;    // duration of process life in seconds
  char* msg;  // message string to be sent
  int delay;  // time to wait before transmitting message in seconds
  unordered_set<int> neighbors;  // list of neighboring nodes by id

  Node(int id = 0, int dur = 0, int dest = 0, char* msg = 0, int delay = 100)
      : id(id), dur(dur), dest(dest), msg(msg), delay(delay), neighbors() {}

  void datalink_receive_from_channel() {
    int fd, bytes, j;
    char buf[101];

    for (auto n : neighbors) {
      string filename = "from" + to_string(id) + "to" + to_string(n);
      fd = open(filename.c_str(), (O_CREAT | O_RDONLY), 0x1c0);

      if (fd < 0) {
      printf("Error opening\n");
      exit(1);
      }

      while((bytes = read(fd, buf, 100)) != 0) {
        printf("Cur Buffer: %s\n", buf);
        printf("child: # bytes read %d which were: %s\n", bytes, buf);
      }

      printf("child: done\n");
      close(fd);
    }
  }
};

int main(int argc, char* argv[]) {
  Node host;

  // Initialize host with arguments
  if (atoi(argv[1]) == atoi(argv[3])) {  // destination is itself
    host = Node(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), 0, atoi(argv[2]));
    for (int i = 4; i < argc; i++) host.neighbors.insert(atoi(argv[i]));
  } else {
    host = Node(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), argv[4],
                atoi(argv[5]));
    for (int i = 6; i < argc; i++) host.neighbors.insert(atoi(argv[i]));
  }

  printf(
      "Node members:\n"
      "id: %d - dur: %d - dest: %d - message: \"%s\" - delay: %d\n",
      host.id, host.dur, host.dest, host.msg, host.delay);
  cout << "neighbors: ";
  for (auto i : host.neighbors) cout << i << " ";
  cout << "\n\n";

  for (int i=0; i < host.dur; i++) {
    host.datalink_receive_from_channel();

    
    sleep(1);
  }
}