#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <vector>

#include "../parallel.h"
#include "hamiltonian.h"

template <class S>
class HcServer {
 public:
  HcServer(S& system, Hamiltonian<S>& hamiltonian) : system(system), hamiltonian(hamiltonian) {}

  void run();

 private:
  int new_socket;

  size_t n;

  S& system;

  Hamiltonian<S>& hamiltonian;

  constexpr static int PORT = 2018;

  void start_server();

  std::vector<double> read_double_array(std::ofstream&) const;
};

template <class S>
void HcServer<S>::run() {
  n = system.coefs.size();
  if (Parallel::is_master()) {
    start_server();
  }
  Parallel::barrier();

  char cmd_buffer[32];
  memset(cmd_buffer, 0, sizeof(cmd_buffer));
  std::vector<double> output(n);
  std::string cmd;

  std::ofstream log_file;
  log_file.open("hc_server." + std::to_string(Parallel::get_proc_id()) + ".log");

  const char* ack = "ACK";

  while (true) {
    // Get command.
    if (Parallel::is_master()) {
      read(new_socket, cmd_buffer, 32);
      cmd = std::string(cmd_buffer);
      memset(cmd_buffer, 0, sizeof(cmd_buffer));
    }
    Parallel::barrier();
    fgpl::broadcast(cmd);
    log_file << cmd << std::endl;
    log_file.flush();

    if (cmd == "getCoefs") {
      if (Parallel::is_master()) {
        send(new_socket, system.coefs.data(), sizeof(double) * n, 0);
      }
    } else if (cmd == "Hc") {
      if (Parallel::is_master()) {
        send(new_socket, ack, strlen(ack), 0);
        output = read_double_array(log_file);
      }
      Parallel::barrier();
      size_t n_cast = 0;
      const size_t TRUNK_SIZE = 1 << 20;
      while (n - n_cast > TRUNK_SIZE) {
        MPI_Bcast(output.data() + n_cast, TRUNK_SIZE, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        n_cast += TRUNK_SIZE;
      }
      MPI_Bcast(output.data() + n_cast, n - n_cast, MPI_DOUBLE, 0, MPI_COMM_WORLD);
      output = hamiltonian.matrix.mul(output);
      if (Parallel::is_master()) {
        send(new_socket, output.data(), sizeof(double) * n, 0);
      }
    } else if (cmd == "exit") {
      return;
    }
  }
}

template <class S>
void HcServer<S>::start_server() {
  int server_fd;
  struct sockaddr_in address;
  int opt = 1;
  int addrlen = sizeof(address);

  // Creating socket file descriptor
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  // Forcefully attaching socket to the port 8080
#if defined(SO_REUSEPORT)
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }
#else
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }
#endif
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  // Forcefully attaching socket to the port 8080
  if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 3) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  printf("Hc server ready\n");
  printf("%zu\n", n);

  if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
    perror("accept");
    exit(EXIT_FAILURE);
  }
}

template <class S>
std::vector<double> HcServer<S>::read_double_array(std::ofstream&) const {
  long long total = sizeof(double) * n;
  std::vector<double> result(n);

  long long n_read = read(new_socket, reinterpret_cast<char*>(result.data()), total);
  while (n_read < total) {
    n_read += read(new_socket, reinterpret_cast<char*>(result.data()) + n_read, total - n_read);
  }

  return result;
}
