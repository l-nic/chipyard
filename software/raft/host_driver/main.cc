#include <getopt.h>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <map>
#include <thread>
#include <mutex>
#include <fstream>



#define UNUSED(expr) do { (void)(expr); } while (0)

#define LEN_MASK     0x000000000000ffff
#define IP_MASK      0xffffffff00000000
#define SERVER_PORT 5001
const int CONN_BACKLOG = 32;
void handle_client_reads(uint32_t ip_addr);

using namespace std;

int _server_fd = -1;
map<uint32_t, int> _ip_handles;
map<uint32_t, mutex> _send_mutex;
vector<thread> _all_threads;

int start_server_socket(uint16_t port) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    cerr << "Server socket creation failed." << endl;
    return -1;
  }
  int sockopt_enable = 1;
  int setopt_retval = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sockopt_enable, sizeof(int));
  if (setopt_retval < 0) {
    cerr << "Server socket bind bind failure." << endl;
    return -1;
  }
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  int bind_retval = bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
  if (bind_retval < 0) {
    cerr << "Server socket bind failure." << endl;
    return -1;
  }
  int listen_retval = listen(sockfd, CONN_BACKLOG);
  if (listen_retval < 0) {
    cerr << "Server socket listen failure." << endl;
    return -1;
  }
  return sockfd;
}

int accept_client_connections(uint32_t num_connections) {
  for (uint32_t i = 0; i < num_connections; i++) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    printf("Waiting for client connection %d of %d...\n", i, num_connections);
    int connectionfd = accept(_server_fd, (struct sockaddr*)&addr, &addr_len);
    if (connectionfd < 0) {
      cerr << "Server connection accept attempt " << i << " failed." << endl;
      return -1;
    }
    if (addr.sin_family != AF_INET) {
      cerr << "Server accepted non-internet connection on iteration " << i << endl;
      return -1;
    }
    uint32_t ip_addr = 0;
    ssize_t actual_len = read(connectionfd, &ip_addr, sizeof(uint32_t));
    if (actual_len <= 0) {
      cerr << "Unable to read ipaddr from new connection " << i << endl;
      return -1;
    }
    printf("Got ip addr %#x\n", ip_addr);
    _ip_handles[ip_addr] = connectionfd;
    _send_mutex[ip_addr].lock();
    _send_mutex[ip_addr].unlock();
    _all_threads.emplace_back(move(thread(handle_client_reads, ip_addr)));
  }
  return 0;
}

void send_to_client(uint32_t dst_ip, char* buffer, uint64_t actual_len) {
  printf("Writing to ip addr %#x\n", dst_ip);
  if (_send_mutex.count(dst_ip) == 0) {
      printf("Ip addr %x is not known to switch.\n", dst_ip);
      return;
  }
  _send_mutex.at(dst_ip).lock();
  int write_fd = _ip_handles[dst_ip];
  ssize_t total_len = 0;
  do {
    ssize_t written_len = write(write_fd, buffer + total_len, actual_len - total_len);
    total_len += written_len;
    if (written_len <= 0) {
      cerr << "Error writing to dst ip " << dst_ip << endl;
      return;
    }
  } while (total_len < actual_len);
  _send_mutex.at(dst_ip).unlock();
}

void handle_client_reads(uint32_t ip_addr) {
  int ip_fd = _ip_handles[ip_addr];
  while (true) {
    // Wait for messages to arrive from a client, then read in the header
    printf("Waiting for messages arriving from addr %x\n", ip_addr);
    uint64_t header;
    ssize_t total_len = 0;
    ssize_t actual_len = 0;
    do {
        actual_len = read(ip_fd, (char*)&header + total_len, sizeof(uint64_t) - total_len);
        total_len += actual_len;
        if (actual_len <= 0) {
            fprintf(stderr, "Read error for ip addr %x\n", ip_addr);
            return;
        }
    } while (total_len < sizeof(uint64_t));
    printf("Read in message header\n");

    // Read in the rest of the message
    uint64_t msg_len = header & LEN_MASK;
    char* buffer = new char[msg_len + sizeof(uint64_t)];
    memcpy(buffer, &header, sizeof(uint64_t)); // TODO: This will get the message size across and in the correct position, but the rest of the format is wrong
    total_len = 0;
    do {
        actual_len = read(ip_fd, buffer + sizeof(uint64_t) + total_len, msg_len - total_len);
        total_len += actual_len;
        if (actual_len <= 0) {
            fprintf(stderr, "Read error for ip addr %x\n", ip_addr);
            return;
        }
    } while (total_len < msg_len);

    uint32_t dst_ip = (header & IP_MASK) >> 32;
    send_to_client(dst_ip, buffer, msg_len + sizeof(uint64_t));
    delete [] buffer;
  }
}

int main( int argc, char* argv[] )
{
  // This should basically just be a switch for the moment.

  _server_fd = start_server_socket(SERVER_PORT);
  if (_server_fd < 0) {
    return -1;
  }
  int client_connections_retval = accept_client_connections(3);
  if (client_connections_retval < 0) {
    return -1;
  }

  printf("Sending test data to server\n");
  uint64_t header = sizeof(uint64_t); // Just the message size
  uint64_t msg_data = 2; // Client message
  char* buffer = new char[2*sizeof(uint64_t)];
  memcpy(buffer, &header, sizeof(uint64_t));
  memcpy(buffer + sizeof(uint64_t), &msg_data, sizeof(uint64_t));
  //send_to_client(0xa000002, buffer, 2*sizeof(uint64_t));

  for (auto& t : _all_threads) {
    t.join();
  }
  return 0;
}
