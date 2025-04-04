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
        return "";
    }
    
    zs.next_in = (Bytef*)data.data();
    zs.avail_in = data.size();
    
    int ret;
    char outbuffer[32768];
    std::string outstring;
    
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        ret = deflate(&zs, Z_FINISH);
        if (outstring.size() < zs.total_out) {
            outstring.append(outbuffer, zs.total_out - outstring.size());
        }
    } while (ret == Z_OK);
    
    deflateEnd(&zs);
    return (ret == Z_STREAM_END) ? outstring : "";
}

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
    std::string headers = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n";
    
    size_t enc_start = request.find("Accept-Encoding:");
    bool gzip_supported = false;
    if (enc_start != std::string::npos) {
      size_t enc_end = request.find("\r\n", enc_start);
      std::string encodings = request.substr(enc_start + 16, enc_end - enc_start - 16);
      if (encodings.find("gzip") != std::string::npos) {
        gzip_supported = true;
      }
    }
    
    if (gzip_supported) {
      std::string compressed = gzip_compress(echo_content);
      headers += "Content-Encoding: gzip\r\nContent-Length: " + std::to_string(compressed.length()) + "\r\n\r\n";
      response = headers + compressed;
    } else {
      headers += "Content-Length: " + std::to_string(echo_content.length()) + "\r\n\r\n";
      response = headers + echo_content;
    }
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
  } else if (path.rfind("/files/", 0) == 0) {
    std::string filename = path.substr(7);
    if (request.rfind("POST", 0) == 0) {
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
  close(client_fd);
}

int main(int argc, char **argv) {
  if (argc == 3 && std::string(argv[1]) == "--directory") {
    directory = argv[2];
  }

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) return 1;

  struct sockaddr_in server_addr = {AF_INET, htons(4221), INADDR_ANY};
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) return 1;
  if (listen(server_fd, 5) != 0) return 1;

  while (true) {
    struct sockaddr_in client_addr;
    int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, nullptr);
    if (client_fd >= 0) std::thread(handle_client, client_fd).detach();
  }

  close(server_fd);
  return 0;
}
