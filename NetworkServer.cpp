#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <unordered_map>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define CLOSESOCKET closesocket
#else
#include <netinet/in.h>
#include <unistd.h>
#define CLOSESOCKET close
#endif

#define PORT 8080
#define MAX_CLIENTS 10000

std::mutex mtx;
std::condition_variable cv;
std::queue<int> clientQueue;
std::atomic<bool> running(true);

void handleClient(int clientSocket) {
    char buffer[1024] = { 0 };
    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) {
            std::cout << "Client " << clientSocket << " disconnected\n";
            CLOSESOCKET(clientSocket);
            return;
        }
        buffer[bytesReceived] = '\0';
        std::cout << "Received from client " << clientSocket << ": " << buffer << std::endl;
        send(clientSocket, buffer, bytesReceived, 0);
    }
}

void workerThread() {
    while (running) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [] { return !clientQueue.empty() || !running; });

        if (!running) break;

        int clientSocket = clientQueue.front();
        clientQueue.pop();
        lock.unlock();

        handleClient(clientSocket);
    }
}

int main() {
    std::cout << "Starting server on port " << PORT << std::endl;

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed!" << std::endl;
        return -1;
    }
#endif

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Socket creation failed!" << std::endl;
        return -1;
    }

    sockaddr_in serverAddr{}, clientAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Bind failed!" << std::endl;
        return -1;
    }

    if (listen(serverSocket, MAX_CLIENTS) < 0) {
        std::cerr << "Listen failed!" << std::endl;
        return -1;
    }

    std::cout << "Server listening on port " << PORT << std::endl;

    std::vector<std::thread> workers;
    for (int i = 0; i < std::thread::hardware_concurrency(); i++) {
        workers.emplace_back(workerThread);
    }

    while (running) {
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            std::cerr << "Failed to accept client" << std::endl;
            continue;
        }

        std::cout << "Accepted new client: " << clientSocket << std::endl;

        {
            std::lock_guard<std::mutex> lock(mtx);
            clientQueue.push(clientSocket);
        }
        cv.notify_one();
    }

    for (auto& worker : workers) {
        worker.join();
    }

    CLOSESOCKET(serverSocket);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}