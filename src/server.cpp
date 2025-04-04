#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  // You can use print statements as follows for debugging, they'll be visible when running tests.
  std::cout << "Logs from your program will appear here!\n";

  // Uncomment this block to pass the first stage
  //
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(4221);

  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 4221\n";
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }

  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);

  std::cout << "Waiting for a client to connect...\n";

  int client_fd=accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
  if (client_fd<0){
    std::cerr<<"Client connection failed\n";
    return 1;
  }
  std::cout << "Client connected\n";

  char buffer[1024]={0};
  int bytes_read=read(client_fd, buffer, sizeof(buffer)-1);
  if (bytes_read <= 0) {
    std::cerr << "Failed to read request\n";  // CHANGED: Added error handling for read failures
    close(client_fd);
    close(server_fd);
    return 1;
  }

  std::string request(buffer);
  
  // Extract the path from the request line
  size_t start = request.find(" ") + 1;  // CHANGED: Extract path correctly from request
  size_t end = request.find(" ", start);
  std::string path = request.substr(start, end - start);  // CHANGED: Extract only the path

  // Determine response
  std::string response;
  if (path == "/") {
    response = "HTTP/1.1 200 OK\r\n\r\n";  // CHANGED: Handle root path correctly
  } else {
    response = "HTTP/1.1 404 Not Found\r\n\r\n";  // CHANGED: Return 404 for other paths
  }

  send(client_fd, response.c_str(), response.length(), 0);
  
  std::cout << "Response sent: " << response;  // CHANGED: Print actual response sent

  close(client_fd);

  close(server_fd);

  return 0;
}
