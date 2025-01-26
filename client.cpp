#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <queue>
#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <memory> // For smart pointers

#pragma comment(lib, "Ws2_32.lib") // Link with the Winsock library

// Constants for request types
#define ONE 0x1
#define TWO 0x2

// Packet class to represent each data packet with relevant attributes
class Packet {
public:
    std::string symbol;  // Symbol of the stock or asset
    char indicatorBuySell;  // 'B' for Buy, 'S' for Sell
    int quantity;  // Quantity of the asset in the packet
    int price;  // Price of the asset
    int packetSequence;  // Sequence number of the packet for ordering

    // Convert the packet to a JSON-like string for easy output or logging
    std::string toJson() const {
        std::ostringstream oss;
        oss << "\t{"
            << "\"symbol\": \"" << symbol << "\", "
            << "\"buySell\": \"" << indicatorBuySell << "\", "
            << "\"quantity\": " << quantity << ", "
            << "\"price\": " << price << ", "
            << "\"packetSequence\": " << packetSequence
            << "}";
        return oss.str();
    }

    // Check if the packet is valid (all necessary fields are correct)
    bool isValid() const {
        return !symbol.empty() &&
            (indicatorBuySell == 'B' || indicatorBuySell == 'S') &&
            (quantity > 0) &&
            (price >= 0) &&
            (packetSequence > 0);
    }
};

// NetworkManager class to handle network connection and communication
class NetworkManager {
private:
    SOCKET connectSocket = INVALID_SOCKET;  // Socket handle
    struct addrinfo* result = nullptr;  // Address information for the server

    // Check if the socket is valid
    bool isSocketValid() const { return connectSocket != INVALID_SOCKET; }

    // Handle errors in sending data
    int handleSendError() const {
        printf("Error sending request: %ld\n", WSAGetLastError());
        return 1;
    }

public:
    // Constructor: Initializes the network connection using Winsock
    NetworkManager(const std::string& hostname, const std::string& port) {
        WSADATA wsaData;
        int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData); // Start Winsock
        if (iResult != 0) {
            printf("WSAStartup failed: %d\n", iResult);
            return;
        }

        // Set up address information for the server
        struct addrinfo hints;
        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_INET;  // IPv4
        hints.ai_socktype = SOCK_STREAM;  // TCP socket

        iResult = getaddrinfo(hostname.c_str(), port.c_str(), &hints, &result);  // Resolve server address
        if (iResult != 0) {
            printf("getaddrinfo failed: %d\n", iResult);
            WSACleanup();  // Cleanup Winsock if failure
            return;
        }
    }

    // Destructor: Clean up and close socket and Winsock
    ~NetworkManager() {
        if (connectSocket != INVALID_SOCKET) {
            closesocket(connectSocket);  // Close socket if it's valid
        }
        if (result) {
            freeaddrinfo(result);  // Free address info
        }
        WSACleanup();  // Cleanup Winsock
    }

    // Connect to the server
    bool connect() {
        if (!result) return false;
        connectSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);  // Create socket
        if (!isSocketValid()) return false;

        int iResult = ::connect(connectSocket, result->ai_addr, (int)result->ai_addrlen);  // Connect to server
        if (iResult == SOCKET_ERROR) {
            printf("Unable to connect to server: %ld\n", WSAGetLastError());
            closesocket(connectSocket);  // Close the socket on error
            connectSocket = INVALID_SOCKET;
            return false;
        }
        printf("Connected to server.\n");
        return true;
    }

    // Disconnect from the server
    void disconnect() {
        if (connectSocket != INVALID_SOCKET) {
            closesocket(connectSocket);  // Close the socket
            connectSocket = INVALID_SOCKET;
        }
    }

    // Send data to the server
    int sendData(const char* data, int length) const {
        if (!isSocketValid()) return -1;
        int bytes_sent = send(connectSocket, data, length, 0);  // Send the data
        if (bytes_sent == SOCKET_ERROR) {
            return handleSendError();  // Handle send error
        }
        return bytes_sent;
    }

    // Receive data from the server and store it into data packets
    int receiveData(std::queue<int>& missingPackets, int requestType, std::vector<Packet>& data) {
        if (!isSocketValid()) return -1;
        std::vector<char> buffer(1024);  // Buffer to store incoming data
        int bytesReceived;
        int packetSize = 17;  // Size of each packet in bytes
        int sequence = 1;  // Sequence number for missing packets

        // Keep receiving data until there's nothing left or we get all packets
        while ((bytesReceived = recv(connectSocket, buffer.data(), 1024, 0)) > 0) {
            int totalReceived = 0;
            while (totalReceived < bytesReceived) {
                if (bytesReceived - totalReceived < packetSize) break;

                // Process the packet
                Packet packet;
                packet.symbol = std::string(buffer.data() + totalReceived, 4);
                packet.indicatorBuySell = buffer[totalReceived + 4];
                packet.quantity = ntohl(*(int*)(buffer.data() + totalReceived + 5));
                packet.price = ntohl(*(int*)(buffer.data() + totalReceived + 9));
                packet.packetSequence = ntohl(*(int*)(buffer.data() + totalReceived + 13));

                totalReceived += packetSize;

                // Add valid packets to the data vector
                if (packet.isValid()) data.push_back(packet);
                else printf("Invalid packet received\n");

                // Handle missing packets based on the request type
                if (requestType == 1) {
                    int totalPacketsMissed = packet.packetSequence - sequence;
                    for (int i = 0; i < totalPacketsMissed; ++i) {
                        missingPackets.push(sequence + i);  // Queue missing packet sequence
                    }
                    sequence = packet.packetSequence + 1;  // Update sequence for next packet
                }
            }
            if (requestType == 2) break;  // End receiving for type 2 request
        }
        return bytesReceived;
    }
};

// Function to write the received packet data to a JSON file
void writeToJsonFile(const std::vector<Packet>& data) {
    std::ofstream outFile("output.json");  // Open output file
    if (!outFile) {
        printf("Error creating JSON file!\n");
        return;
    }

    outFile << "[\n";
    for (size_t i = 0; i < data.size(); ++i) {
        outFile << data[i].toJson();  // Convert each packet to JSON format
        if (i < data.size() - 1) {
            outFile << ",\n";  // Separate entries with commas
        } else {
            outFile << '\n';
        }
    }
    outFile << "]";  // Close JSON array
    outFile.close();
    printf("successfully written to output.json\n");
}

// Main function to control the flow of the program
int main() {
    std::string hostname = "127.0.0.1";  // Localhost address
    std::string port = "3000";  // Port for connection

    // Create and connect network manager
    NetworkManager networkManager(hostname, port);
    if (!networkManager.connect()) return 1;

    std::vector<Packet> dataPackets;  // To store received data packets
    std::queue<int> missingPackets;  // To track missing packets for re-request

    char requestType1[2] = { ONE, 0 };  // Initial request for data
    if (networkManager.sendData(requestType1, 2) == -1) return 1;
    printf("Request sent to server.\n");

    // Receive the first batch of data and handle missing packets
    if(networkManager.receiveData(missingPackets, 1, dataPackets) == -1) return 1;

    // Request and receive missing packets
    while (!missingPackets.empty()) {
        NetworkManager missingPacketManager(hostname, port);  // Create new connection for each missing packet
        if (!missingPacketManager.connect()) {
            printf("Failed to connect for missing packet request.\n");
            missingPackets.pop();  // Avoid infinite loop
            continue;
        }

        char reqPacket = missingPackets.front();  // Get next missing packet
        missingPackets.pop();
        char requestType2[2] = { TWO, reqPacket };  // Request for the missing packet

        if (missingPacketManager.sendData(requestType2, 2) == -1) {
            printf("Failed to send request for missing packet.\n");
        } else if(missingPacketManager.receiveData(missingPackets, 2, dataPackets) == -1) {
            printf("Failed to receive data for missing packet.\n");
        }

        missingPacketManager.disconnect();  // Close the connection
    }

    // Sort packets by sequence number
    std::sort(dataPackets.begin(), dataPackets.end(), [](const Packet& a, const Packet& b) {
        return a.packetSequence < b.packetSequence;
    });

    // Write the sorted packets to a JSON file
    writeToJsonFile(dataPackets);

    printf("Total packets received: %zu\n", dataPackets.size());
    printf("Server Disconnected.\n");

    return 0;
}
