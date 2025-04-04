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
#include <sstream>
#include <zlib.h>

std::string directory;

std::string gzip_compress(const std::string& data) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    
    if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        std::cerr << "Failed to initialize zlib compression" << std::endl;
        return "";
    }
    
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    zs.avail_in = data.size();
    
    int ret;
    char outbuffer[32768];
    std::string outstring;
    
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        ret = deflate(&zs, Z_FINISH);
        if (ret != Z_STREAM_ERROR) {
            outstring.append(outbuffer, sizeof(outbuffer) - zs.avail_out);
        }
    } while (zs.avail_out == 0);
    
    deflateEnd(&zs);
    return (ret == Z_STREAM_END) ? outstring : "";
}

void handle_client(int client_fd) {
  char buffer[4096] = {0};
  int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
  if (bytes_read <= 0) {
    std::cerr << "Failed to read request" << std::endl;
    close(client_fd);
    return;
  }

  std::string request(buffer);
  std::cout << "Received request: " << request << std::endl;
  
  size_t start = request.find(" ") + 1;
  size_t end = request.find(" ", start);
  std::string path = request.substr(start, end - start);
  
  std::string response;
  if (request.find("GET") == 0) {
    if (path == "/") {
      response = "HTTP/1.1 200 OK\r\n\r\n";
    } else if (path.rfind("/echo/", 0) == 0) {
      std::string echo_content = path.substr(6);
      response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(echo_content.length()) + "\r\n\r\n" + echo_content;
    } else if (path.rfind("/files/", 0) == 0) {
      std::string filename = directory + path.substr(7);
      std::ifstream file(filename, std::ios::binary);
      if (!file) {
        response = "HTTP/1.1 404 Not Found\r\n\r\n";
      } else {
        std::ostringstream content;
        content << file.rdbuf();
        std::string file_data = content.str();
        response = "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(file_data.size()) + "\r\n\r\n" + file_data;
      }
    } else {
      response = "HTTP/1.1 404 Not Found\r\n\r\n";
    }
  } else if (request.find("POST") == 0 && path.rfind("/files/", 0) == 0) {
    std::string filename = directory + path.substr(7);
    size_t body_start = request.find("\r\n\r\n");
    if (body_start != std::string::npos) {
      body_start += 4;
      std::ofstream file(filename, std::ios::binary);
      if (file) {
        file.write(request.data() + body_start, request.size() - body_start);
        response = "HTTP/1.1 201 Created\r\n\r\n";
      } else {
        response = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
      }
    } else {
      response = "HTTP/1.1 400 Bad Request\r\n\r\n";
    }
  } else {
    response = "HTTP/1.1 405 Method Not Allowed\r\n\r\n";
  }
  send(client_fd, response.c_str(), response.length(), 0);
  close(client_fd);
}

int main(int argc, char **argv) {
  if (argc == 3 && std::string(argv[1]) == "--directory") {
    directory = argv[2];
  }

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    std::cerr << "Failed to create socket" << std::endl;
    return 1;
  }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in server_addr = {AF_INET, htons(4221), INADDR_ANY};
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Bind failed" << std::endl;
    return 1;
  }
  if (listen(server_fd, 5) != 0) {
    std::cerr << "Listen failed" << std::endl;
    return 1;
  }
  
  std::cout << "Server started on port 4221" << std::endl;

  while (true) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_len);
    if (client_fd >= 0) {
      std::cout << "Client connected" << std::endl;
      std::thread(handle_client, client_fd).detach();
    }
  }

  close(server_fd);
  return 0;
}