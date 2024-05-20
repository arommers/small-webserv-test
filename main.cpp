#include <iostream>       // Input and output through streams
#include <cstring>        // Manipulate C-style strings and arrays
#include <arpa/inet.h>    // Provides functions for manipulating IP addresses (htonl, htons)
#include <unistd.h>       // Provides access to the POSIX operating system API (close, read, write)
#include <sys/socket.h>   // Provides declarations for the socket API functions (socket, bind, listen, access)
#include <netinet/in.h>   // Constants and structures needed for internet domain addresses ( sockaddr_in AF_INET)

# define PORT 4040

int main()
{
    struct      sockaddr_in address;
    int         addrLen = sizeof(address);
    int         serverSocket, newSocket;
    const char* message = "HTTP/1.1 200 OK\r\nContent-Length: 12\r\nContent-Type: text/plain\r\n\r\nHello World!";


    // Creating socket file descriptor
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket failed: " << strerror(errno) << std::endl;
        exit (EXIT_FAILURE);
    }

    // Configuring the address struct of the socket
    address.sin_family = AF_INET; //address family
    address.sin_addr.s_addr = INADDR_ANY; // sets ip to 0.0.0.0, accepts connections from any ip on the host
    address.sin_port = htons(4040); // ensures the port nr. is correctly formatted

    // Binding the socket to the address and the port
    if (bind(serverSocket, reinterpret_cast<struct sockaddr*>(&address), addrLen) < 0) {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    // Socket starts listening mode
    if (listen(serverSocket, 10) < 0) {
        std::cerr << "Listen failed: " << strerror(errno) << std::endl;
        close (serverSocket);
        exit(EXIT_FAILURE);
    }

    while (true)
    {
        std::cout << "Waiting for new connection..." << std::endl;

        //accepting new connection
        if ((newSocket = accept(serverSocket, reinterpret_cast<struct sockaddr*>(&address), reinterpret_cast<socklen_t*>(&addrLen))) < 0) {
            std::cerr << "Accept failed: " << strerror(errno) << std::endl;
            close (serverSocket);
            exit(EXIT_FAILURE);
        }

        // Print out client address details
        std::cout << "Connection accepted from " << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port) << "\n";

        // Send back a response
        if (send(newSocket, message, strlen(message), 0) < 0) {
            std::cerr << "Send failed: " << strerror(errno) << std::endl;
            close (newSocket);
            close (serverSocket);
            exit(EXIT_FAILURE);
        }
        close(newSocket);
    }
    close(serverSocket);
    return (0);
}
