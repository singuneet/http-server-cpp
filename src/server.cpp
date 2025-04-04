#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <fstream>

std::string directory;

void handle_client(int client_fd) {
  char buffer[1024] = {0};
  int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
  if (bytes_read <= 0) {
    std::cerr << "Failed to read request\n";
    close(client_fd);
    return;
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
    std::string headers = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(echo_content.length()) + "\r\n";
    
    // CHANGED: Improved parsing of Accept-Encoding to handle multiple encodings
    size_t enc_start = request.find("Accept-Encoding:");
    if (enc_start != std::string::npos) {
      size_t enc_end = request.find("\r\n", enc_start);
      std::string encodings = request.substr(enc_start + 16, enc_end - enc_start - 16);
      if (encodings.find("gzip") != std::string::npos) {
        headers += "Content-Encoding: gzip\r\n";
      }
    }
    
    response = headers + "\r\n" + echo_content;
  } else if (path == "/user-agent") {
    size_t ua_start = request.find("User-Agent: ");
    if (ua_start != std::string::npos) {
      ua_start += 12;
      size_t ua_end = request.find("\r\n", ua_start);
      std::string user_agent = request.substr(ua_start, ua_end - ua_start);
      response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(user_agent.length()) + "\r\n\r\n" + user_agent;
    } else {
      response = "HTTP/1.1 400 Bad Request\r\n\r\n";
    }
  } else if (path.rfind("/files/", 0) == 0) { // CHANGED: Added support for serving and storing files
    std::string filename = path.substr(7);
    if (request.rfind("POST", 0) == 0) { // CHANGED: Handle POST requests
      size_t content_length_pos = request.find("Content-Length: ");
      if (content_length_pos != std::string::npos) {
        content_length_pos += 16;
        size_t content_length_end = request.find("\r\n", content_length_pos);
        int content_length = std::stoi(request.substr(content_length_pos, content_length_end - content_length_pos));
        size_t body_pos = request.find("\r\n\r\n") + 4;
        std::string body = request.substr(body_pos, content_length);
        std::ofstream file(directory + "/" + filename, std::ios::binary);
        file.write(body.c_str(), body.size());
        response = "HTTP/1.1 201 Created\r\n\r\n";
      } else {
        response = "HTTP/1.1 400 Bad Request\r\n\r\n";
      }
    } else {
      std::ifstream file(directory + "/" + filename, std::ios::binary | std::ios::ate);
      if (file) {
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::string content(size, '\0');
        file.read(&content[0], size);
        response = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " + std::to_string(size) + "\r\n\r\n" + content;
      } else {
        response = "HTTP/1.1 404 Not Found\r\n\r\n";
      }
    }
  } else {
    response = "HTTP/1.1 404 Not Found\r\n\r\n";
  }

  send(client_fd, response.c_str(), response.length(), 0);
  std::cout << "Response sent: " << response;

  close(client_fd);
}

int main(int argc, char **argv) {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  std::cout << "Logs from your program will appear here!\n";

  if (argc == 3 && std::string(argv[1]) == "--directory") {
    directory = argv[2]; // CHANGED: Store directory argument
  }

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    std::cerr << "Failed to create server socket\n";
    return 1;
  }

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

  std::cout << "Waiting for clients to connect...\n";

  while (true) {
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    if (client_fd < 0) {
      std::cerr << "Client connection failed\n";
      continue;
    }
    std::cout << "Client connected\n";
    std::thread(handle_client, client_fd).detach();
  }

  close(server_fd);
  return 0;
}
