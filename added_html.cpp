#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <vector>
#include <unordered_map>

#define PORT 4040
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

struct Client {
    int fd;
    std::string writeBuffer;
    size_t writePos = 0;
};

std::string readIndexHTML() {
    std::ifstream file("index.html");
    if (!file) {
        std::cerr << "Failed to open index.html" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string readCSS() {
    std::ifstream generalFile("css/general.css");
    std::ifstream styleFile("css/style.css");
    
    if (!generalFile || !styleFile) {
        std::cerr << "Failed to open CSS files" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::stringstream buffer;
    buffer << generalFile.rdbuf();
    buffer << styleFile.rdbuf();
    return buffer.str();
}

std::string readImage(const std::string& imagePath) {
    std::ifstream imageFile(imagePath, std::ios::binary);
    if (!imageFile) {
        std::cerr << "Failed to open image file: " << imagePath << std::endl;
        exit(EXIT_FAILURE);
    }

    std::stringstream buffer;
    buffer << imageFile.rdbuf();
    return buffer.str();
}

std::string generateHTTPResponse(const std::string& content, const std::string& contentType) {
    std::stringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << content.size() << "\r\n";
    response << "\r\n"; // Empty line indicating end of headers
    response << content; // Add the content
    return response.str();
}

std::string extractImagePath(const char* request) {
    std::string imagePath;
    const char* start = std::strstr(request, "GET /img/");
    if (start) {
        start += 9; // Move past "GET /img/"
        const char* end = std::strstr(start, " HTTP/1.1");
        if (end) {
            imagePath.assign(start, end); // Extract the image path
        }
    }
    return imagePath;
}

int main() {
    struct sockaddr_in address;
    int addrLen = sizeof(address);
    int serverSocket, newSocket;
    std::string indexHTML = readIndexHTML();
    std::string cssContent = readCSS();
    std::vector<struct pollfd> fds;
    std::unordered_map<int, Client> clients;

    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket failed: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(serverSocket, reinterpret_cast<struct sockaddr*>(&address), addrLen) < 0) {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    if (listen(serverSocket, MAX_CLIENTS) < 0) {
        std::cerr << "Listen failed: " << strerror(errno) << std::endl;
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    struct pollfd server_fd;
    server_fd.fd = serverSocket;
    server_fd.events = POLLIN;
    fds.push_back(server_fd);

    while (true) {
        int poll_count = poll(fds.data(), fds.size(), -1);
        if (poll_count == -1) {
            std::cerr << "Poll failed: " << strerror(errno) << std::endl;
            close(serverSocket);
            exit(EXIT_FAILURE);
        }

        for (size_t i = 0; i < fds.size(); ++i) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == serverSocket) {
                    if ((newSocket = accept(serverSocket, reinterpret_cast<struct sockaddr*>(&address), reinterpret_cast<socklen_t*>(&addrLen))) < 0) {
                        std::cerr << "Accept failed: " << strerror(errno) << std::endl;
                    } else {
                        std::cout << "New connection, socket fd is " << newSocket << ", ip is : " << inet_ntoa(address.sin_addr) << ", port : " << ntohs(address.sin_port) << std::endl;

                        struct pollfd client_fd;
                        client_fd.fd = newSocket;
                        client_fd.events = POLLIN;
                        fds.push_back(client_fd);

                        clients[newSocket] = Client{newSocket};
                    }
                } else {
                    char buffer[BUFFER_SIZE] = {0};
                    int valread = read(fds[i].fd, buffer, BUFFER_SIZE);
                    if (valread == 0) {
                        std::cout << "Client disconnected, socket fd is " << fds[i].fd << std::endl;
                        close(fds[i].fd);
                        clients.erase(fds[i].fd);
                        fds.erase(fds.begin() + i);
                        --i;
                    } else {
                        std::cout << "Received message: " << buffer << std::endl;
                        std::string contentType;
                        std::string responseContent;
                        if (strstr(buffer, "GET /styles.css")) {
                            contentType = "text/css";
                            responseContent = cssContent;
                        } else {
                            std::string imagePath = extractImagePath(buffer);
                            if (!imagePath.empty()) {
                                // Extracted image path successfully
                                contentType = "image/png"; // Adjust the content type based on the image type
                                responseContent = readImage("img/" + imagePath);
                            } else {
                                contentType = "text/html";
                                responseContent = indexHTML;
                            }
                        }
                        std::string httpResponse = generateHTTPResponse(responseContent, contentType);
                        clients[fds[i].fd].writeBuffer = httpResponse;
                        clients[fds[i].fd].writePos = 0;
                        fds[i].events = POLLIN | POLLOUT;
                    }
                }
            }

            if (fds[i].revents & POLLOUT) {
                Client &client = clients[fds[i].fd];
                if (client.writePos < client.writeBuffer.size()) {
                    int bytesSent = send(client.fd, client.writeBuffer.c_str() + client.writePos, client.writeBuffer.size() - client.writePos, 0);
                    if (bytesSent < 0) {
                        std::cerr << "Send failed: " << strerror(errno) << std::endl;
                        close(client.fd);
                        clients.erase(client.fd);
                        fds.erase(fds.begin() + i);
                        --i;
                    } else {
                        client.writePos += bytesSent;
                    }
                }

                if (client.writePos >= client.writeBuffer.size()) {
                    fds[i].events = POLLIN;
                }
            }
        }
    }

    close(serverSocket);
    return 0;
}
