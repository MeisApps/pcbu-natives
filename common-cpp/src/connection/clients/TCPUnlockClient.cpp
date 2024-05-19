#include "TCPUnlockClient.h"

#ifdef _WIN32
#include <WinSock2.h>
#include <Ws2tcpip.h>

#define read(x,y,z) recv(x, (char*)y, z, 0)
#define write(x,y,z) send(x, y, z, 0)
#define SOCKET_INVALID INVALID_SOCKET
#define SAFE_CLOSE(x) if(x != (SOCKET)-1) { closesocket(x); x = (SOCKET)-1; }
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SOCKET_INVALID -1
#define SAFE_CLOSE(x) if(x != -1) { close(x); x = -1; }
#endif

TCPUnlockClient::TCPUnlockClient(const std::string& ipAddress, int port, const PairedDevice& device)
    : BaseUnlockServer(device) {
    m_IP = ipAddress;
    m_Port = port;
    m_ClientSocket = (SOCKET)-1;
    m_IsRunning = false;
}

bool TCPUnlockClient::Start() {
    if(m_IsRunning)
        return true;

#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        Logger::writeln("WSAStartup failed.");
        return false;
    }
#endif

    m_IsRunning = true;
    m_AcceptThread = std::thread(&TCPUnlockClient::ConnectThread, this);
    return true;
}

void TCPUnlockClient::Stop() {
    if(!m_IsRunning)
        return;

    if(m_ClientSocket != -1 && m_HasConnection)
        write(m_ClientSocket, "CLOSE", 5);

    m_IsRunning = false;
    m_HasConnection = false;
    SAFE_CLOSE(m_ClientSocket);
    if(m_AcceptThread.joinable())
        m_AcceptThread.join();
}

void TCPUnlockClient::ConnectThread() {
    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons((u_short)m_Port);

    if (inet_pton(AF_INET, m_IP.c_str(), &serv_addr.sin_addr) <= 0) {
        Logger::writeln("Invalid IP address.");
        m_IsRunning = false;
        m_UnlockState = UnlockState::UNK_ERROR;
        return;
    }

    if ((m_ClientSocket = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_INVALID) {
        Logger::writeln("socket failed.");
        m_IsRunning = false;
        m_UnlockState = UnlockState::UNK_ERROR;
        return;
    }

    int result = connect(m_ClientSocket, (struct sockaddr*)&serv_addr,sizeof(serv_addr));
    if (result == 0) {
        m_HasConnection = true;
        auto serverDataStr = GetUnlockInfoPacket();
        if(serverDataStr.empty()) {
            m_IsRunning = false;
            m_UnlockState = UnlockState::UNK_ERROR;
            SAFE_CLOSE(m_ClientSocket);
            return;
        }
        write(m_ClientSocket, serverDataStr.c_str(), (int)serverDataStr.size());

        // Read response
        char buffer[1024]{};
        long bytesRead{};
        std::vector<uint8_t> readData{};
        while((bytesRead = read(m_ClientSocket, buffer, sizeof(buffer))) > 0)
            readData.insert(readData.end(), buffer, buffer + bytesRead);
        OnResponseReceived(readData.data(), readData.size());
    } else {
        Logger::writeln("Connect failed.");
        m_UnlockState = UnlockState::CONNECT_ERROR;
    }

    m_IsRunning = false;
    SAFE_CLOSE(m_ClientSocket);
}
