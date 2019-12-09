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
using std::vector;

class Node {
 public:
  int id;      // id of this node
  int dest;    // id of message's destination node
  int dur;     // duration of process life in seconds
  char* data;  // message string to be sent
  int delay;   // time to wait before transmitting message in seconds
  unordered_set<int> neighbors;          // list of neighboring nodes by id
  unordered_map<int, int> files;         // file descriptors for each neighbor
  unordered_map<int, int> distance_map;  // Tracks distance to other nodes
  unordered_map<int, vector<string>>
      seq_strings;  // messages by their source id and sequence
  unordered_map<int, vector<string>>
      seq_redundants;  // redundant messages by their seq

  Node(int id = 0, int dur = 0, int dest = 0, char* data = 0, int delay = 100)
      : id(id),
        dur(dur),
        dest(dest),
        data(data),
        delay(delay),
        neighbors(),
        files(),
        distance_map(),
        seq_strings(),
        seq_redundants() {}

  void datalink_receive_from_channel() {
    int bytes;
    char buf[101];
    struct Message {
      int length = 0;
      string data = "";
      int checksum = 0;

      bool verify_checksum() {
        int sum = 'S';
        string temp = to_string(length);
        // Pad with a 0 if length is less than 10 for accurate sum calculation
        if (length < 10) temp = "0" + temp;

        sum += temp[0];
        sum += temp[1];
        for (auto c : data) {
          sum += c;
        }

        if (((sum % 100) == checksum))
          cout << "PASSED\n";
        else
          printf("FAILED - Checksum calc: %d\n", (sum % 100));

        return ((sum % 100) == checksum);
      }
    } msg;

    // Looping through all channels
    for (auto fdpair : files) {
      int n = fdpair.first;    // current receiving neighbor
      int fd = fdpair.second;  // fd for this neighbor's channel
      string tempStr;
      bool parsing = false;
      int parseCount = 0;

      printf("- Neighbor ID #%d Read: start -\n", n);
      while ((bytes = read(fd, buf, 100)) != 0) {
        for (int i = 0; i < bytes; i++) {
          try {
            char cur = buf[i];

            // Start of message
            if (cur == 'S' && !parsing) {
              parsing = true;
              msg.data = "";
              parseCount = 0;

              // Getting message length
              tempStr = "";
              tempStr += buf[++i];
              tempStr += buf[++i];
              if (!isdigit(tempStr[0]) || !isdigit(tempStr[1]))
                throw "Message length must be digits.";
              msg.length = stoi(tempStr);
              if (msg.length < 6)
                throw "Message length must be higher than 5";  // invalid length

              printf("Length: %d\nData Parse: ", msg.length);
            }
            // Parsing data
            else if (parsing && parseCount < (msg.length - 5)) {
              msg.data += cur;
              parseCount++;
              cout << cur;
            }
            // Data finished. Parse checksum
            else if (parsing && parseCount == (msg.length - 5)) {
              cout << "\n";
              parsing = false;
              tempStr = "";

              tempStr += buf[i];
              tempStr += buf[++i];
              if (!isdigit(tempStr[0]) || !isdigit(tempStr[1])) throw 1;
              msg.checksum = stoi(tempStr);
              printf("CheckSum: %d - ", msg.checksum);

              // Message passed checksum. Send to network layer
              if (msg.verify_checksum()) {
                char* netmsg = new char[msg.data.length() + 1];
                strcpy(netmsg, msg.data.c_str());
                network_receive_from_datalink(netmsg, n);
              }

              msg.length = 0;
              msg.data = "";
              msg.checksum = 0;
              printf("\n");
            }
          } catch (...) {
            cout
                << "Exception occured in buffer read. Scanning for next 'S'.\n";
            parsing = false;
            tempStr = "";
            parseCount = 0;
            msg.length = 0;
            msg.data = "";
            msg.checksum = 0;
            continue;
          }
        }
      }

      printf("- Neighbor ID #%d Read: done -\n\n", n);
    }
    printf("host id %d datalink read: done\n\n", id);
  }

  void network_receive_from_datalink(char* msg, int neighbor_id) {
    printf("Network Layer received message from Data Link Layer.\n");
    printf("From ID: %d - Msg: %s\n", neighbor_id, msg);
    char msg_type;
    int src_id, dest_id, length;
    string temp = "";  // used to concat length chars then convert to int

    try {
      msg_type = msg[0];
      printf("Message Type: %c\n", msg_type);
      // Incorrect message format (Network message)
      if (msg_type != 'D' && msg_type != 'R') return;

      // Data Message
      if (msg_type == 'D') {
        src_id = msg[1] - 48;
        dest_id = msg[2] - 48;
        temp += msg[3];
        temp += msg[4];
        if (!isdigit(temp[0]) || !isdigit(temp[1]))
          throw "Message length must be digits.";
        length = stoi(temp);

        char* tp_msg = &msg[5];
        printf("Network Message:\nSrc: %d - Dest: %d - Length: %d\nMsg: %s\n",
               src_id, dest_id, length, tp_msg);

        // Destination is this node
        if (dest_id = id) {
          transport_receive_from_network(tp_msg);
        }

        // Destination is a neighbor
        // TODO

        // Destination is farther
        // TODO

        // Routing message
      } else {
        // TODO
      }
    } catch (...) {
      cout << "Exception thrown in network layer receive from data link.\n";
    }
  }

  void transport_receive_from_network(char* msg) {
    char msg_type = msg[0];
    int src_id, seq_num;
    string temp = ""; 

    // Check for incorrect message types
    if (msg_type != 'd' && msg_type != 'r') return;

    // Transport Data Message
    if (msg_type == 'd') {
      src_id = msg[1] - 48;
      temp += msg[3];
      temp += msg[4];
      if (!isdigit(temp[0]) || !isdigit(temp[1]))
        throw "Message length must be digits.";
      seq_num = stoi(temp);

      char* tp_msg = &msg[5];
      printf("Transport Message:\nSrc: %d Sequence: %d\nMsg: %s\n",
              src_id, seq_num, tp_msg);

      // TODO: order messages by their source and sequence number

      // Transport Redundant Message
    } else {
      // TODO: Redundant Message parsing and ordering
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