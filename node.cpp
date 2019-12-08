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
using std::string;
using std::to_string;
using std::unordered_map;
using std::unordered_set;

class Node {
 public:
  int id;      // id of this node
  int dest;    // id of destinatoin node
  int dur;     // duration of process life in seconds
  char* data;  // message string to be sent
  int delay;   // time to wait before transmitting message in seconds
  unordered_set<int> neighbors;   // list of neighboring nodes by id
  unordered_map<int, int> files;  // file descriptors for each neighbor

  Node(int id = 0, int dur = 0, int dest = 0, char* data = 0, int delay = 100)
      : id(id),
        dur(dur),
        dest(dest),
        data(data),
        delay(delay),
        neighbors(),
        files() {}

  void datalink_receive_from_channel() {
    int bytes;
    char buf[101];

    // Looping through all channels
    for (auto fdpair : files) {
      int n = fdpair.first;    // current neighbor
      int fd = fdpair.second;  // fd for this neighbor's channel

      struct Message {
        int length;
        string data;
        int checksum;
      } msg;

      string tempLen;
      string tempCheck;
      bool parsing = false;

      printf("neighbor id %d read: start\n", n);
      while ((bytes = read(fd, buf, 100)) != 0) {
        for (int i = 0; i < bytes; i++) {
          try {
            char cur = buf[i];

            // Start of message
            if (cur == 'S' && !parsing) {
              parsing = true;

              // Getting message length
              tempLen += buf[++i];
              tempLen += buf[++i];
              if (!isdigit(tempLen[0]) || !isdigit(tempLen[1]) ) throw 1;
              msg.length = stoi(tempLen);

              printf("LENGTH IS: %d\n", msg.length);

              continue;
            }
          } catch (...) {
            cout << "Exception occured in buffer read. Scanning for next 'S'.\n";
            parsing = false;
            tempLen = "";
            tempCheck = "";
            continue;
          }

        }
      }

      printf("neighbor id %d read: done\n\n", n);
    }
    printf("host id %d datalink read: done\n\n", id);
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

  // Open/creating file channels for reading and save file descriptors in map
  for (auto n : host.neighbors) {
    string filename = "from" + to_string(host.id) + "to" + to_string(n);
    int fd = open(filename.c_str(), (O_CREAT | O_RDONLY), 0x1c0);

    if (fd < 0) {
      printf("Error opening\n");
      exit(1);
    }
    host.files[n] = fd;
  }

  printf(
      "Node members:\n"
      "id: %d - dur: %d - dest: %d - message: \"%s\" - delay: %d\n",
      host.id, host.dur, host.dest, host.data, host.delay);
  cout << "neighbors: ";
  for (auto i : host.neighbors) cout << i << " ";
  cout << "\n\n";

  // Main program loop
  for (int i = 0; i < host.dur; i++) {
    host.datalink_receive_from_channel();

    sleep(1);
  }

  // Closing all files
  for (auto fd : host.files) close(fd.second);
}