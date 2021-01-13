#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#define _CRT_SECURE_NO_WARNINGS 1
#define _CRT_NONSTDC_NO_DEPRECATE 1

#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <boost/algorithm/string.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN    
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#else

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <boost/filesystem.hpp>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#endif

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

#include <sys/types.h>
#ifdef _WIN32
#include <nlohmann/json.hpp>
#include <filesystem>
#include <functional>
#else
#include "json.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#endif
#include "configuration.hpp"
#include <spdlog/spdlog.h>
#include <functional>
#include "message.hpp"

#include "FileWatcher2.hpp"

using namespace nlohmann;
using json = nlohmann::json;

#ifndef CLientClass
#define CLientClass

#define PACKET_SIZE 8192

class Client {

    std::shared_ptr <spdlog::logger> myLogger;

    // send bytes
    int sendMessage(int sock, message2 msg, std::atomic<bool>& b);
    int sendFileStream(int sock, std::string filePath, std::atomic<bool>& b);

    // receive bytes
    int readMessageResponse(int sock, std::string& response);
    int readInitialConfStream(int sock, int packetsNumber, std::string& conf);

    int fromStringToMessage(std::string msg, message2& message);

    // separator to create correct paths
    inline std::string separator()
    {
#ifdef _WIN32
        return "\\";
#else
        return "\/";
#endif
    }

public:

    conf::user uc;
    bool isServerSide;

    int fromUserServerConfToString(userServerConf conf, std::string& msg);

    int readConfiguration();
    int readConfiguration(std::string userName, std::string user_server_IP, std::string user_server_PORT, std::string folderPath, int secondTimeout);
    int initLogger();

    int serverConnection(int& sock);
    int sendToServer(int sock, int operation, std::string folderPath, std::string fileName, std::string content, std::uintmax_t file_size, std::string hash, unsigned long long timestamp, std::atomic<bool>& b);
    int sendToServer2(int sock, int operation, std::string folderPath, std::string fileName, std::string content, std::uintmax_t file_size, std::string hash, unsigned long long timestamp, std::atomic<bool>& b, std::unordered_map<std::string, struct info>& paths_);
    int serverDisconnection(int sock);

    bool isClosed(int sock);

};

#endif
