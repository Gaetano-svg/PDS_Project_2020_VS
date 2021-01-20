#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#define _CRT_SECURE_NO_WARNINGS 1
#define _CRT_NONSTDC_NO_DEPRECATE 1

#define BUFF_SIZE 1024

#include <iostream>
#include <fstream>
#include <cstdint>
#include <sys/types.h>
#include <stdexcept>
#include <sys/types.h>
#include <string>
#include <thread>
#include <boost/crc.hpp>
#ifdef _WIN32
#include "../PDS_Project_2020/json.hpp"
#include "../PDS_Project_2020/configuration.hpp"
#include "../PDS_Project_2020/message.hpp"
#else
#include "json.hpp"
#include "configuration.hpp"
#include "message.hpp"
#endif
#ifdef _WIN32
#include "spdlog/spdlog.h"
#else
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#endif

#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <chrono>
#include <fcntl.h>
#include <ios>
#include <list>
#include <boost/algorithm/string.hpp>

#ifdef _WIN32
#include "../PDS_Project_2020/cryptopp/cryptlib.h"
#include "../PDS_Project_2020/cryptopp/md5.h"
#include "../PDS_Project_2020/cryptopp/files.h"
#include "../PDS_Project_2020/cryptopp/hex.h"
#else
#include <cryptopp/cryptlib.h>
#include <cryptopp/md5.h>
#include <cryptopp/files.h>
#include <cryptopp/hex.h>
#endif

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <filesystem>
#include <functional>
#include <sys/stat.h>
#include <ios>
#include "./UserFW.hpp"

#else

#include <experimental/filesystem>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
using namespace nlohmann;

#endif

using namespace nlohmann;
using namespace std::chrono;

#define IDLE_TIMEOUT 60
#define PACKET_SIZE SOCKET_SENDFILE_BLOCKSIZE 

class Server {

    // Client Connection CLASS
    class ClientConn {

    private:

        // User FW params
        std::string user_server_path;
        std::string user_server_IP;
        std::string user_server_PORT;
        int user_server_timeout = -1;

        Server& serv;
        conf::server server;
        std::string logFile;
        std::shared_ptr <spdlog::logger> log;
        std::string logName;

        inline std::string separator()
        {
            #ifdef _WIN32
                return "\\";
            #else
                return "\/";
            #endif
        }

        int initLogger();

        void waitForMessage();

        int readMessage(int fd, std::string& bufString);
        int readFileStream(int packetsNumber, int fileFd);

        void updateFile(int& resCode, std::string buf, message2& msg);
        void renameFile(int& resCode, std::string buf, message2& msg);
        void deleteFile(int& resCode, std::string buf, message2& msg);
        void initialConfiguration(int& resCode, std::string buf, message2& msg, std::string initConf);
        int sendFolderToUser(int& resCode, std::string buf, message2& msg);
        int sendFile2(int nOutFd, std::string path, off_t* pOffset);

        int handleFileUpdate(message2 msg, std::string buf);
        int handleFileDelete(message2 msg, std::string buf);
        int handleFileRename(message2 msg, std::string buf);

        int selective_search(std::string& response, std::string buf, message2& msg);

        int sendResponse(int resCode, std::string buf, message2 msg);
        int SendInitialConf(std::string conf, int nOutFd, int nCount);

        int handleOkResponse(message2& response, message2& msg);
        int handleErrorResponse(message2& response, message2& msg, int errorCode);

        int fromStringToMessage(std::string msg, message2& message);
        int fromStringToUserServerConf(std::string msg, userServerConf& conf);
        int fromMessageToString(std::string& messageString, message2& msg);
        bool isClosed(int sock);

    public:

        int sock;
        std::string userName;
        std::string ip;
        std::atomic_bool running;
        std::atomic_long activeMS;

        ClientConn(Server& serv, std::string& logFile, conf::server server, int sock) : serv(serv), logFile(logFile), server(server), sock(sock) {
            
            std::cout << "[CLIENT-CONN]: for user " + userName + " created!" << std::endl;

            // start with running bool setted at true
            running.store(true);

        };

        void handleConnection();
        std::string compute_hash(const std::string file_path);

        ~ClientConn() {

            std::cout << "[CLIENT-CONN]: for user " + userName + " closed!"<< std::endl;

            // close socket
            shutdown(sock, 2);

        };

    };

private:

    int sock = -1, port;
    std::string ip;

    // log reference
    std::shared_ptr <spdlog::logger> log;

    // server JSON configuration
    nlohmann::json jServerConf;

    // number of active connections to check if the next connection request will be satisfied
    std::atomic_int activeConnections;

    // atomic variable to controll the server from shutting down
    std::atomic_bool running;

    // mutex to access shared structures like "clients"
    std::mutex m;

    // mutex to access shared structure clientsNumberSocket
    std::mutex mNumSock;

    // structure to map socket to ClientConnection object
    std::map<int, std::shared_ptr<ClientConn>> clients;

    // structure to map socket to ClientConnection object
    std::map<std::string, std::atomic_int> clientsNumberSocket;

    void checkUserInactivity();
    void unregisterClient(int csock);
    bool isClosed(int sock);

public:

    int csock;

#ifdef _WIN32

    SOCKET ListenSocket = INVALID_SOCKET;
#endif
    // the server configuration containing ip and port informations
    conf::server sc;
    std::string logFile = "server_log.txt";

    Server() {};

    int readConfiguration(std::string file);
    int initLogger();
    int startListening();

    bool incrementNumSocket(std::string userName);
    void decrementNumSocket(std::string userName);

    ~Server();

};

