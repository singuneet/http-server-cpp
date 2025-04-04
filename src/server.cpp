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
    std::cerr << "Failed to read request\n";  
    close(client_fd);
    close(server_fd);
    return 1;
  }

  std::string request(buffer);
  

  size_t start = request.find(" ") + 1;  
  size_t end = request.find(" ", start);
  std::string path = request.substr(start, end - start);  

  
  std::string response;
  if (path == "/") {
    response = "HTTP/1.1 200 OK\r\n\r\n";
  } else if (path.rfind("/echo/", 0) == 0) {
    std::string echo_content = path.substr(6);
    response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(echo_content.length()) + "\r\n\r\n" + echo_content;
  } else if (path == "/user-agent") { // New feature: User-Agent handling
    size_t ua_start = request.find("User-Agent: ");
    if (ua_start != std::string::npos) {
      ua_start += 12; // Move past "User-Agent: "
      size_t ua_end = request.find("\r\n", ua_start);
      std::string user_agent = request.substr(ua_start, ua_end - ua_start);
      response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(user_agent.length()) + "\r\n\r\n" + user_agent;
    } else {
      response = "HTTP/1.1 400 Bad Request\r\n\r\n";
    }
  } else {
    response = "HTTP/1.1 404 Not Found\r\n\r\n";
  }

  send(client_fd, response.c_str(), response.length(), 0);
  
  std::cout << "Response sent: " << response;  

  close(client_fd);

  close(server_fd);

  return 0;
}
