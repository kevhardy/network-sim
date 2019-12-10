#include <bits/stdc++.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <vector>

using std::array;
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
  array<array<string, 100>, 10>
      seq_strings;  // messages by their source id and sequence
  array<array<string, 100>, 10>
      seq_redundants;              // redundant messages by their seq
  unordered_set<int> seq_sources;  // tracks which sources receieved msgs from
  array<int, 10> rtable;           // Routing table
  array<int, 10> costs;            // Cost table
  unordered_map<int, int> up;      // Tracks if neighbor is up. 0 == offline
  vector<string> substrings;  // Substrings of data message for periodic sends
  int substring_seq = 0;

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
        seq_redundants(),
        seq_sources(),
        up(),
        substrings() {}

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
            cout << "Exception occured in dlink read. Scanning for next 'S'.\n";
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
  }

  void datalink_receive_from_network(const char* msg, int len, char next_hop) {
    printf(
        "- Datalink Receive From Network - START\nSending from%dto%c\nmsg: "
        "\"%s\" "
        "- "
        "len: %d\n",
        id, next_hop, msg, len);
    int fd;
    int sum = 0;

    string fname = "from" + to_string(id) + "to" + next_hop;
    fd = open(fname.c_str(), (O_CREAT | O_WRONLY | O_APPEND), 0x1c0);
    if (fd < 0) {
      printf("Error opening\n");
      exit(1);
    }

    // Construct datalink message with checksum
    string dl_msg = "S";
    dl_msg += (len < 5) ? "0" + to_string(len + 5) : to_string(len + 5);
    dl_msg += msg;
    for (auto c : dl_msg) {
      sum += c;
    }
    sum %= 100;
    dl_msg += (sum < 10) ? "0" + to_string(sum) : to_string(sum);

    // Write datalink message to file and close
    write(fd, dl_msg.c_str(), dl_msg.size());
    close(fd);
    printf("- Datalink Receieve From Network - DONE\n");
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
        printf(
            "Network Message:\nSrc: %d - Dest: %d - Length: %d\nMsg: \"%s\"\n",
            src_id, dest_id, length, tp_msg);

        // Destination is this node
        if (dest_id == id) {
          transport_receive_from_network(tp_msg);
        }
        // Destination is a neighbor
        else if (neighbors.find(dest_id) != neighbors.end()) {
          if (costs[dest_id] != 10)
            datalink_receive_from_network(msg, length + 5, msg[2]);
          // Destination is not a neighbor
        } else {
          if (costs[dest_id] != 10) {
            printf("\n\n---ABOUT TO SEND THROUGH RTABLE---\n");
            printf("Costs:");
            for (int cost : costs) printf(" %2d", cost);
            cout << "\n";
            printf("Route:");
            for (int route : rtable) printf(" %2d", route);
            cout << "\n";

            datalink_receive_from_network(msg, length + 5,
                                          rtable[dest_id] + 48);
          }
        }
        // If no conditions exist drop data message

        // Routing message
      } else {
        src_id = msg[1] - 48;
        bool triggered_update = false;
        printf("Network Message(Route Update):\nSrc: %d - Costs(B):", src_id);
        for (int cost : costs) printf(" %2d", cost);
        cout << "\n";
        printf("         Route(B):");
        for (int route : rtable) printf(" %2d", route);
        cout << "\n";
        // Refresh up timer for neighbor when route msg is receieved from them
        up[src_id] = 20;

        // Parse message for update costs
        for (int d = 0; d < 10; d++) {
          int update;
          if (msg[d + 2] != 'I')
            update = msg[d + 2] - 48;
          else
            update = 10;

          // Check if costs need to be updated
          if (d == id)
            costs[d] = 0;
          else if (update + 1 < costs[d]) {
            rtable[d] = src_id;
            costs[d] = update + 1;
            triggered_update = true;
          }
        }
        printf("Src: %d - Costs(A):", src_id);
        for (int cost : costs) printf(" %2d", cost);
        cout << "\n";
        printf("         Route(A):");
        for (int route : rtable) printf(" %2d", route);
        cout << "\n";

        // If a cost has been changed then send to neighbors
        if (triggered_update) {
          printf("--- TRIGGERED ROUTE UPDATE ---\n\n");
          network_send_dv();
        }
      }
    } catch (...) {
      cout << "Exception thrown in network layer receive from data link.\n";
    }
  }

  void network_receive_from_transport(const char* msg, int len, int dest) {
    printf("Network Received from Transport to send:\n\"%s\"\n", msg);

    // Build Network Data string
    string n_msg = "D";
    n_msg += to_string(id);
    n_msg += to_string(dest);
    n_msg += len < 10 ? "0" + to_string(len) : to_string(len);
    n_msg += msg;

    // Find next hop
    char next_hop;
    if (costs[dest] != 10) {
      next_hop = rtable[dest] + 48;
      datalink_receive_from_network(n_msg.c_str(), n_msg.length(), next_hop);
    } else {
      printf("\n--NO ROUTE FOUND FOR MESSAGE FROM %d to %d--\n\n", id, dest);
    }
  }

  void network_send_dv() {
    string dv_msg;
    array<int, 10> poisoned_costs;

    // Send update to all neighbors
    for (auto n : neighbors) {
      dv_msg = "";
      dv_msg.resize(12);
      poisoned_costs = costs;

      for (int d = 0; d < 10; d++) {
        if (rtable[d] == n)
          poisoned_costs[d] = 10;
        else
          poisoned_costs[d] = costs[d];
      }

      dv_msg[0] = 'R';
      dv_msg[1] = id + 48;

      // Constructing update message
      for (int i = 2; i < 12; i++) {
        if (poisoned_costs[i - 2] == 10)
          dv_msg[i] = 'I';
        else
          dv_msg[i] = poisoned_costs[i - 2] + 48;
      }

      datalink_receive_from_network(dv_msg.c_str(), 12, (n + 48));
    }
  }

  // Decrements all neighbor up timers by 1. If any reach 0 then we update costs
  void network_decrement_up_timer() {
    printf("- Neighbors Up Check - START\nUp(B):");
    for (int n : neighbors) printf(" %d:%2d", n, up[n]);
    cout << "\n";

    // Check if all neighbors are up
    for (auto n : neighbors) {
      // Neighbor is down
      if (up[n] == 0) {
        // handle costs by route
        for (int d = 0; d < 10; d++) {
          // If current neighbor is down, then set any routes through it to inf
          if (rtable[d] == n) costs[d] = 10;  // Set to infinity
        }
      } else {
        up[n] -= 1;
      }
    }

    printf("Up(A):");
    for (int n : neighbors) printf(" %d:%2d", n, up[n]);
    cout << "\n";
    printf("- Neighbors Up Check - DONE\n\n");
  }

  void transport_receive_from_network(char* msg) {
    printf("Transport Layer received message from Network Layer.\n");

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
      string tp_msg_s = tp_msg;
      printf("Transport Message:\nSrc: %d Sequence: %d\nMsg: %s\n", src_id,
             seq_num, tp_msg);

      // Order messages by their source and sequence number
      seq_sources.insert(src_id);  // Track what sources I've received msgs from
      seq_strings[src_id][seq_num] = tp_msg_s;

      // Transport Redundant Message
    } else {
      // TODO: Redundant Message parsing and ordering
      printf("\n====REDUNDANT MESSAGE RECIEVED====\nMsg: \"%s\"\n", msg);
    }
  }

  void transport_send_string() {
    printf("\n\nCurrent substring_seq: %d\n", substring_seq);
    // Data Message
    if (substring_seq < substrings.size()) {
      string tp_msg = "d";
      tp_msg += to_string(id);
      tp_msg += to_string(dest);
      tp_msg += substring_seq < 10 ? "0" + to_string(substring_seq)
                                   : to_string(substring_seq);
      tp_msg += substrings[substring_seq];

      network_receive_from_transport(tp_msg.c_str(), tp_msg.length(), dest);
    }

    // Redundant Message everytime 2 substrings are sent
    if (substring_seq % 2 == 1) {
      string tp_redun = "r";
      tp_redun += to_string(id);
      tp_redun += to_string(dest);
      tp_redun += ((substring_seq / 2) < 10)
                      ? "0" + to_string(substring_seq / 2)
                      : to_string(substring_seq / 2);

      string str1 = substrings[substring_seq - 1];
      string str2 = substrings[substring_seq];
      string redun_xor_msg = "";
      printf("Str1: \"%s\" - Str2: \"%s\"\n", str1.c_str(), str2.c_str());
      for (int i = 0; i < 5; i++) {
        redun_xor_msg += str1[i] ^ str2[i];
      }
      tp_redun += redun_xor_msg;
      printf(
          "\n===Redundant Message TO SEND===\nFull MSG: %s - redun: \"%s\"\n",
          tp_redun.c_str(), redun_xor_msg.c_str());
      network_receive_from_transport(tp_redun.c_str(), tp_redun.length(), dest);
    }

    substring_seq++;
  }

  void transport_output_all_received() {
    string fullmsg = "";
    for (auto n : seq_sources) {
      fullmsg = "";
      for (int i = 0; i < 100; i++) {
        fullmsg += seq_strings[n][i];
      }
      printf("Message From %d\nFull Msg: \"%s\"\n\n", n, fullmsg.c_str());
    }
  }
};

int main(int argc, char* argv[]) {
  Node host;

  // Initialize host with arguments
  if (atoi(argv[1]) == atoi(argv[3])) {  // destination is itself
    host =
        Node(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), 0, atoi(argv[2]) + 1);
    for (int i = 4; i < argc; i++) host.neighbors.insert(atoi(argv[i]));
  } else {
    host = Node(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), argv[4],
                atoi(argv[5]));
    for (int i = 6; i < argc; i++) host.neighbors.insert(atoi(argv[i]));
  }

  // Open/creating file channels for reading and save file descriptors in map
  for (auto n : host.neighbors) {
    string filename = "from" + to_string(n) + "to" + to_string(host.id);
    int fd = open(filename.c_str(), (O_CREAT | O_RDONLY), 0x1c0);

    if (fd < 0) {
      printf("Error opening\n");
      exit(1);
    }
    host.files[n] = fd;
  }

  // Setting cost of all nodes to infinity
  for (int i = 0; i < 10; i++) {
    host.costs[i] = 10;
    host.rtable[i] = 10;
  }

  // Setting neighbor costs to 1
  for (auto n : host.neighbors) {
    host.costs[n] = 1;
    host.rtable[n] = n;
    host.up[n] = 0;  // Neighbors are initially down
  }
  host.costs[host.id] = 0;
  host.rtable[host.id] = host.id;

  printf("INITIAL COST/ROUTES:\nCosts(B):");
  for (int cost : host.costs) printf(" %2d", cost);
  cout << "\n";
  printf("Route(B):");
  for (int route : host.rtable) printf(" %2d", route);
  cout << "\n";

  printf(
      "-- Host Info --:\n"
      "id: %d - dur: %d - dest: %d - message: \"%s\" - delay: %d\n",
      host.id, host.dur, host.dest, host.data, host.delay);
  cout << "neighbors: ";
  for (auto i : host.neighbors) cout << i << " ";
  cout << "\n\n";

  // Divide message up if it exists
  if (host.id != host.dest) {
    string data_str = host.data;
    int num_of_msgs = (strlen(host.data) / 5);
    if (strlen(host.data) % 5 != 0) num_of_msgs += 1;
    host.substrings.resize(num_of_msgs);

    // Divide message into substrings for sending later
    for (int i = 0; i < num_of_msgs; i++) {
      // substring messages without padding
      if (i != num_of_msgs - 1) {
        host.substrings[i] = data_str.substr((i * 5), 5);
      } else {
        host.substrings[i] = data_str.substr((i * 5), 5);
        if (num_of_msgs != 1) {
          for (int j = 0; j < (5 - (strlen(host.data) % 5)); j++)
            host.substrings[i] += " ";
        }
        if (num_of_msgs == 1 && strlen(host.data) % 5 != 0) {
          for (int j = 0; j < (5 - (strlen(host.data) % 5)); j++)
            host.substrings[i] += " ";
        }
      }
    }
  }

  for (auto arr : host.seq_strings) arr.fill("");
  for (auto arr : host.seq_redundants) arr.fill("");

  int send_msg_counter = 0;

  // Main program loop
  for (int i = 0; i < host.dur; i++) {
    printf("--TIME %d--\n", i);

    host.datalink_receive_from_channel();

    // Only send out update costs every 15 seconds
    if (i % 15 == 0) {
      printf("--- SENDING 15 SECOND ROUTE UPDATE ---\n\n");
      host.network_send_dv();
    }

    host.network_decrement_up_timer();

    if (i >= host.delay) {
      // Only send string every two seconds
      if (send_msg_counter % 2 == 0) {
        host.transport_send_string();
        send_msg_counter = 0;
      }
      send_msg_counter++;
    }
    sleep(1);
  }

  host.transport_output_all_received();

  // Closing all files
  for (auto fd : host.files) close(fd.second);
}