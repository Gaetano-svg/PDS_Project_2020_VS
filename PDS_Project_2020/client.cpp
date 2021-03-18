#include "client.hpp"

#ifdef _WIN32

#include <winsock2.h>
#include <Windows.h>
#include <boost/filesystem.hpp>
//namespace fs = boost::filesystem;

#else

#include <arpa/inet.h>
#include "boost/filesystem.hpp"
//namespace fs = std::experimental::filesystem;
//namespace fs = boost::filesystem;

#endif


//////////////////////////////////
//        PUBLIC METHODS        //
//////////////////////////////////


/*

Read the user configuration JSON file.

RETURN:

 0   ---> no error
-1   ---> error opening/creating the file
-2   ---> error converting the JSON
-3   ---> error saving the JSON inside the struct

*/
int Client::readConfiguration(std::string userName, std::string user_server_IP, std::string user_server_PORT, std::string folderPath, int secondTimeout) {

    // save the user configuration inside a local struct
    try {

        uc = conf::user
        {
            user_server_IP,
            user_server_PORT,
            userName + "_Server_FW",
            folderPath,
            secondTimeout
        };

    }
    catch (...) {

        std::cerr << "Error during the saving of the configuration locally to CLIENT";
        return -3;

    }

    return 0;

}

/*

Read the user configuration JSON file.

RETURN:

 0   ---> no error
-1   ---> error opening/creating the file
-2   ---> error converting the JSON
-3   ---> error saving the JSON inside the struct

*/
int Client::readConfiguration() {

    isServerSide = false;

    std::ifstream userConfFile("userConfiguration.json");
    json jUserConf;

    if (!userConfFile)
    {
        std::cerr << "User Configuration File could not be opened!\n"; // Report error
        std::cerr << "Error code: " << strerror(errno); // Get some info as to why
        return -1;
    }

    if (!(userConfFile >> jUserConf))
    {
        std::cerr << "The User Configuration File couldn't be parsed";
        return -2;
    }

    // save the user configuration inside a local struct
    try {

        uc = conf::user
        {
            jUserConf["serverIp"].get<std::string>(),
            jUserConf["serverPort"].get<std::string>(),
            jUserConf["name"].get<std::string>(),
            jUserConf["folderPath"].get<std::string>(),
            jUserConf["secondTimeout"].get<int>(),
            jUserConf["backupFromServer"].get<bool>()
        };

    }
    catch (...) {

        std::cerr << "Error during the saving of the configuration locally to CLIENT";
        return -3;

    }

    return 0;

}

/*

Initialize the Log File for the client.

RETURN:

 0   ---> no error
-1   ---> error opening/creating the log file

*/
int Client::initLogger() {

    // Logger initialization
    try
    {

#ifdef _WIN32
        myLogger = spdlog::rotating_logger_mt(uc.name, uc.name + "_log.txt", 1048576 * 4, 100000);
#else
        myLogger = spdlog::basic_logger_mt(uc.name, uc.name + "_log.txt");
#endif
        myLogger->info("Logger initialized correctly with file " + uc.name + "_log.txt");
        myLogger->flush();

    }
    catch (const spdlog::spdlog_ex& ex)
    {
        return -1;
    }

    return 0;

}

/*

Open a connection channel with the Server.

RETURN:

 0 ---> no error
-1 ---> error creating socket
-2 ---> socket closed

*/
int Client::serverConnection(int& sock) {

    // Create socket
    std::string response;

    myLogger->info("try to connect to server - IP: " + uc.serverIp + " PORT: " + uc.serverPort);
    myLogger->flush();

#ifdef _WIN32
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL,
        * ptr = NULL,
        hints;
    const char* sendbuf = "this is a test";
    char recvbuf[PACKET_SIZE];
    int iResult;
    int recvbuflen = 0;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return -1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(uc.serverIp.c_str(), uc.serverPort.c_str(), &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return -2;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return -3;
        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return -4;
    }

    DWORD timeout = uc.secondTimeout;

    setsockopt(ConnectSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(DWORD));
    setsockopt(ConnectSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(DWORD));

    u_long non_blocking = 0;

    ioctlsocket(ConnectSocket, FIONBIO, &non_blocking);

    sock = ConnectSocket;

#else

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        std::string error = strerror(errno);
        myLogger->error("Can't create socket, Error: " + error);
        shutdown(sock, 2);
        return -1;
    }

    myLogger->info("Socket " + std::to_string(sock) + " was created");

    // Fill in a hint structure
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(atoi(uc.serverPort.c_str()));

    // Convert IPv4 and IPv6 addresses from text to binary form 
    if ((inet_pton(AF_INET, uc.serverIp.c_str(), &hint.sin_addr)) <= 0)
    {
        myLogger->error("Invalid address: address " + uc.serverIp + " not supported");
        shutdown(sock, 2);
        return -2;
    }

    myLogger->info("try to connect to server - IP: " + uc.serverIp + " PORT: " + uc.serverPort);
    myLogger->flush();

    // Connect to server
    while (connect(sock, (sockaddr*)&hint, sizeof(hint)) < 0)
    {
        std::string error = strerror(errno);
        myLogger->error("Can't connect to server, Error: " + error);
        myLogger->flush();
        sleep(5);
    }

#endif

    if ((sock) < 0)
    {
        std::string error = strerror(errno);
        myLogger->error("Can't create socket, Error: " + error);
        shutdown(sock, 2);
        return -5;
    }

    std::string logName = "[client_" + std::to_string(sock) + "]: ";

    myLogger->info(logName + "Socket " + std::to_string(sock) + " was created");

    message2 msg = {

        "",
        0,
        0,
        "",
        0,
        "",
        "",
        this->uc.name,
        ""

    };

    std::atomic_bool b(false);

    while (sendMessage(sock, msg, b) < 0) {

        // the connection was closed so exit
        myLogger->info(logName + "Won't be able to send to the server user credential");
        myLogger->flush();
        /*
        shutdown(sock, 2);

        return -6;*/

        if (isClosed(sock)) {

            // the connection was closed so exit
            myLogger->info(logName + "The socket was closed on the server side");
            myLogger->flush();

            shutdown(sock, 2);

            return -6;

        }
        else {

            myLogger->info(logName + "2 Can't connect to server " + uc.serverIp + " port: " + uc.serverPort);
            myLogger->info(logName + "2 Try again to connect in 5 seconds");
            myLogger->flush();

#ifdef _WIN32
            Sleep(5000);
#else
            sleep(5);
#endif

        }

    }

    // wait for response from connection
    while (readMessageResponse(sock, response) < 0) {

        if (isClosed(sock)) {

            // the connection was closed so exit
            myLogger->info(logName + "The socket was closed on the server side");
            myLogger->flush();

            shutdown(sock, 2);

            return -6;

        }
        else {

            myLogger->info(logName + "2 Can't connect to server " + uc.serverIp + " port: " + uc.serverPort);
            myLogger->info(logName + "2 Try again to connect in 5 seconds");
            myLogger->flush();

#ifdef _WIN32
            Sleep(5000);
#else
            sleep(5);
#endif

        }

    }


    fromStringToMessage(response, msg);

    if (msg.typeCode < 0) {

        shutdown(sock, 2);

        return -6;

    }

    myLogger->info(logName + "connected to server " + uc.serverIp + ":" + uc.serverPort);
    myLogger->flush();

    return 0;

}

/*

Convert the userServerConf into a string.

RETURN:

0 -> no error
-1 -> unexpected error

*/
int Client::fromUserServerConfToString(userServerConf conf, std::string& msg) {

    try {

        json jMsg = json{ {"ip", conf.ip},{"port", conf.port} };
        msg = jMsg.dump();

    }
    catch (...) {
        return -1;
    }

    return 0;

}

/*

Convert the  std::string received into a message object.

RETURN:

0 -> no error
-1 -> unexpected error

*/
int Client::fromStringToMessage(std::string msg, message2& message) {

    try {

        auto jsonMSG = json::parse(msg);

        jsonMSG.at("type").get_to(message.type);
        jsonMSG.at("typeCode").get_to(message.typeCode);
        jsonMSG.at("folderPath").get_to(message.folderPath);
        jsonMSG.at("userName").get_to(message.userName);
        jsonMSG.at("fileName").get_to(message.fileName);
        jsonMSG.at("packetNumber").get_to(message.packetNumber);
        jsonMSG.at("body").get_to(message.body);

        msg.clear();

    }
    catch (...) {

        msg.clear();
        msg = "Error parsing  std::string received into Message Object";
        return -1;

    }

    return 0;

}

void from_json2(const nlohmann::json& jsonMSG, message2& message) {

    jsonMSG.at("type").get_to(message.type);
    jsonMSG.at("typeCode").get_to(message.typeCode);
    jsonMSG.at("folderPath").get_to(message.folderPath);
    jsonMSG.at("userName").get_to(message.userName);
    jsonMSG.at("fileName").get_to(message.fileName);
    jsonMSG.at("packetNumber").get_to(message.packetNumber);
    jsonMSG.at("hash").get_to(message.hash);
    jsonMSG.at("body").get_to(message.body);
    jsonMSG.at("timestamp").get_to(message.timestamp);

}
/*
Send a message to the server.
Connection must be opened in order to send message correctly.

PARAMETERS:
    operation: (int) the operation code type number
    folderPath: (string) the folder Path of the file
    fileName: (string) the name of the file to update on the server
    content: (string) optional content to send with the request (Rename)

RETURN:
    0 -> no error
    -1 -> error sending msg header
    -2 -> error receiving msg header response
    -3 -> error sending msg STREAM
    -4 -> error receiving msg STREAM response
    -5 -> error sending CONF response
    -6 -> error receiving CONF STREAM
    -7 -> error sending CONF STREAM response
    -8 -> error receving END STREAM
    -10 -> filePath wasn't found on the client side
    -11 -> interrupt from file watcher
*/
int Client::sendToServer(int sock, int operation, std::string folderPath, std::string fileName, std::string content, std::uintmax_t file_size, std::string hash, unsigned long long timestamp, std::atomic<bool>& b) {

    if (b.load())
        return -11;

    std::atomic <bool> a(false);
    int resCode = 0;
    std::string response;
    message2 msg;
    std::string filePath = folderPath + separator() + fileName;

    std::string logName = "[client_" + std::to_string(sock) + "]: ";

    myLogger->info(logName + "[OPERATION_" + std::to_string(operation) + "]:");
    myLogger->info(logName + "[OPERATION_" + std::to_string(operation) + "]: path " + filePath);
    myLogger->info(logName + "[OPERATION_" + std::to_string(operation) + "]:");
    myLogger->flush();

    // CREATE the Message Object
    msg = {

        "",
        operation,
        timestamp,
        hash,
        0,
        folderPath,
        fileName,
        this->uc.name,
        content

    };

    // Set the # of packets because of FILE size
    if (operation == 1 || operation == 3) {

        if (b.load())
            return -11;

        FILE* file = fopen(filePath.c_str(), "r");

        // check if the File exists in the local folder
        if (file == NULL) {

            myLogger->error(logName + "[OPERATION_" + std::to_string(operation) + "]:" + "the file " + filePath + " wasn't found! ");
            //fclose(file);
            //b.store(true);
            //b.store(true);
            return -10;

        }

        // obtain file size
        // ?? FILE_SIZE DA FILE WATCHER ??
        fseek(file, 0L, SEEK_END);
        int fileSize = ftell(file);

        int div = fileSize / PACKET_SIZE;
        int rest = fileSize % PACKET_SIZE;
        int numberOfPackets = div;
        if (rest > 0)
            numberOfPackets++;

        // set the # of packets 
        msg.packetNumber = numberOfPackets;

        fclose(file);

        // if the client runs on Server Side, the folder Path sent to the user side is relative to the user sub dir
        if (isServerSide) {

            std::string folderToUser = folderPath.substr(this->uc.folderPath.length() + separator().length(), folderPath.length());

#ifdef _WIN32
            std::string preFolder = folderToUser.substr(0, folderToUser.find(separator()));

#       ifdef BOOST_POSIX_API 
            std::replace(preFolder.begin(), preFolder.end(), '\_', '\:');
#       else
            std::replace(preFolder.begin(), preFolder.end(), '\_', '\:');
#       endif

            std::string extraFolder = folderToUser.substr(folderToUser.find(separator()), folderToUser.length());
            std::string folder = preFolder + extraFolder;
#else
            std::string folder = folderToUser;
#endif
            msg.folderPath = folder;

        }

    }

    resCode = sendMessage(sock, msg, b);

    myLogger->info(logName + "[OPERATION_" + std::to_string(operation) + "]:" + "SND MSG returned code: " + std::to_string(resCode));
    myLogger->flush();

    if (resCode == -11)
        return -11;

    if (resCode < 0)
        return -1;

    resCode = readMessageResponse(sock, response);

    myLogger->info(logName + "[OPERATION_" + std::to_string(operation) + "]: RCV RESP returned code: " + std::to_string(resCode));
    myLogger->flush();

    if (resCode < 0)
        return -2;

    message2 respMSG;
    this->fromStringToMessage(response, respMSG);

    if (respMSG.typeCode < 0) {

        myLogger->info(logName + "[OPERATION_" + std::to_string(operation) + "]: error on the server");
        myLogger->flush();

        return -20;

    }

    // UPDATE OR CREATE OPERATION
    if (operation == 1 || operation == 3) {

        resCode = sendFileStream(sock, filePath, b);

        myLogger->info(logName + "[OPERATION_" + std::to_string(operation) + "]: SND FILE STREAM returned code: " + std::to_string(resCode));
        myLogger->flush();

        if (resCode == -11)
            return -11;

        if (resCode < 0) {
            return -3;
        }

        resCode = readMessageResponse(sock, response);

        //std::cout << response << std::endl;

        myLogger->info(logName + "[OPERATION_" + std::to_string(operation) + "]: RCV STREAM RESP returned code: " + std::to_string(resCode));
        myLogger->flush();

        if (resCode < 0) {
            return -4;
        }

        message2 respMSG;
        this->fromStringToMessage(response, respMSG);

        if (respMSG.typeCode < 0) {

            myLogger->info(logName + "[OPERATION_" + std::to_string(operation) + "]: error on the server");
            myLogger->flush();

            return -20;

        }

    }

    // INITIAL CONFIGURATION FROM THE SERVER
    else if (operation == 5) {

        message2 msgResp;
        this->fromStringToMessage(response, msgResp);

        std::string initialConf;
        msg.typeCode = 0;
        msg.type = "ok";
        int numberPackets = msgResp.packetNumber;

        //std::cout << numberPackets << std::endl;

        resCode = sendMessage(sock, msg, a);

        myLogger->info(logName + "[OPERATION_" + std::to_string(operation) + "]: SEND CONF RESP returned code: " + std::to_string(resCode));
        myLogger->flush();

        if (resCode < 0)
            return -5;

        resCode = readInitialConfStream(sock, numberPackets, initialConf);

        // try to transform the json string received into a json array
        json jsonArray = json::parse(initialConf);
        int arraySize = jsonArray.size();

        std::vector<message2> msgVector;

        size_t index = 0;
        for (auto& item : jsonArray) {
            message2 msg = {};
            from_json2(item, msg);
            msgVector.push_back(msg);
        }

        std::cout << initialConf << std::endl;

        // MESSAGE VECTOR obtained
        //std::cout << "[" << ' ';
        for (auto i = msgVector.begin(); i != msgVector.end(); ++i) {
            //std::cout << (*i).fileName << ', ';
            info inf;
            inf.file_hash = (*i).hash;
            inf.file_size = (*i).packetNumber;
            //inserire in qualche modo conversione tra timestamp e file_time_type
            //paths_[(*i).folderPath] = inf;
        }
        //std::cout << "]";

        myLogger->info(logName + "[OPERATION_" + std::to_string(operation) + "]: RCV CONF STREAM returned code: " + std::to_string(resCode));
        myLogger->flush();

        if (resCode < 0)
            return -6;

        // MESSAGE VECTOR obtained
        /*std::cout << "[" << ' ';
        for (auto i = msgVector.begin(); i != msgVector.end(); ++i)
            std::cout << (*i).fileName << ', ';
        std::cout << "]";

        myLogger->info("[OPERATION_" + std::to_string(operation) + "]: RCV CONF STREAM returned code: " + std::to_string(resCode));
        myLogger->flush();

        if (resCode < 0)
            return -6;*/

    }

    return 0;

}

/*
* This method will only be used to receive the configuration from the server and manipulate the local _paths structure
*/
int Client::sendToServer2(int sock, int operation, std::string folderPath, std::string fileName, std::string content, std::uintmax_t file_size, std::string hash, unsigned long long timestamp, std::atomic<bool>& b, std::unordered_map<std::string, struct info>& paths_) {

    if (b.load())
        return -11;

    std::atomic <bool> a(false);
    int resCode = 0;
    std::string response;
    message2 msg;
    std::string filePath = folderPath + separator() + fileName;

    std::string logName = "[client_" + std::to_string(sock) + "]: ";

    myLogger->info("");
    myLogger->info(logName + "[OPERATION_" + std::to_string(operation) + "]: path " + filePath);
    myLogger->info("");
    myLogger->flush();

    // CREATE the Message Object
    msg = {

        "",
        operation,
        timestamp,
        hash,
        0,
        folderPath,
        fileName,
        this->uc.name,
        content

    };

    resCode = sendMessage(sock, msg, b);

    myLogger->info(logName + "[OPERATION_" + std::to_string(operation) + "]: SND MSG returned code: " + std::to_string(resCode));
    myLogger->flush();

    if (resCode == -11)
        return -11;

    if (resCode < 0)
        return -1;

    resCode = readMessageResponse(sock, response);

    myLogger->info(logName + "[OPERATION_" + std::to_string(operation) + "]: RCV RESP returned code: " + std::to_string(resCode));
    myLogger->flush();

    if (resCode < 0)
        return -2;

    message2 respMSG;
    this->fromStringToMessage(response, respMSG);

    if (respMSG.typeCode < 0) {

        myLogger->info(logName + "[OPERATION_" + std::to_string(operation) + "]: error on the server");
        myLogger->flush();

        return -20;

    }

    // INITIAL CONFIGURATION FROM THE SERVER
    else if (operation == 5) {

        message2 msgResp;
        this->fromStringToMessage(response, msgResp);

        std::string initialConf;
        msg.typeCode = 0;
        msg.type = "ok";
        int numberPackets = msgResp.packetNumber;

        resCode = sendMessage(sock, msg, a);

        myLogger->info(logName + "[OPERATION_" + std::to_string(operation) + "]: SEND CONF RESP returned code: " + std::to_string(resCode));
        myLogger->flush();

        if (resCode < 0)
            return -5;

        resCode = readInitialConfStream(sock, numberPackets, initialConf);

        // try to transform the json string received into a json array
        json jsonArray = json::parse(initialConf);
        int arraySize = jsonArray.size();

        std::vector<message2> msgVector;

        size_t index = 0;
        for (auto& item : jsonArray) {
            message2 msg = {};
            from_json2(item, msg);
            msgVector.push_back(msg);
        }

        for (auto i = msgVector.begin(); i != msgVector.end(); ++i) {
            //std::cout << (*i).fileName << ', ';
            info inf;
            inf.file_hash = (*i).hash;
            inf.file_size = (*i).packetNumber;

            using ft = std::filesystem::file_time_type;
            auto the_time = ft(ft::duration((*i).timestamp));

            inf.file_last_write = the_time;
            //inserire in qualche modo conversione tra timestamp e file_time_type
            paths_[(*i).folderPath] = inf;
            paths_[(*i).folderPath].checkHash = true;
        }
        
        myLogger->info(logName + "[OPERATION_" + std::to_string(operation) + "]: RCV CONF STREAM returned code: " + std::to_string(resCode));
        myLogger->flush();

        if (resCode < 0)
            return -6;

    }

    return 0;

}

/*

Close connection channel with the server.

RETURN:

 0 -> no error
-1 -> error sending disconnection request
-2 -> error receiving disconnection request RESPONSE
-3 -> unexpected error

*/
int Client::serverDisconnection(int sock) {

    std::atomic <bool> a(false);
    std::string logName = "[client_" + std::to_string(sock) + "]: ";

    try {

        int resCode = 0;
        myLogger->info("");
        myLogger->info(logName + "try to disconnect from server - IP: " + uc.serverIp + " PORT: " + uc.serverPort);
        myLogger->flush();

        std::string response;
        message2 fcu2 = {

                "",
                6,
                0, // timestamp
                "",// hash
                0,
                "",
                "",
                this->uc.name,
                ""

        };

        resCode = sendMessage(sock, fcu2, a);

        if (resCode < 0) {

            shutdown(sock, 2);
            return -1;

        }

        resCode = readMessageResponse(sock, response);

        shutdown(sock, 2);

        myLogger->info(logName + "disconnected from server " + uc.serverIp + ":" + uc.serverPort);
        myLogger->flush();

        message2 respMSG;
        this->fromStringToMessage(response, respMSG);

        if (respMSG.typeCode < 0) {

            myLogger->info(logName + "error on the server");
            myLogger->flush();

            return -20;

        }

        return resCode;

    }
    catch (...) {

        shutdown(sock, 2);
        myLogger->error(logName + "Unexpected error happened during server-disconnection");
        return -3;

    }

}

/*

Check if the connection with the server IS CLOSED.

RETURN:

    true: connection IS CLOSED
    false: connection IS OPENED

*/
bool Client::isClosed(int sock) {

#ifdef _WIN32

    fd_set rfd;
    FD_ZERO(&rfd);
    FD_SET(sock, &rfd);
    timeval tv = { 0 };
    select(sock + 1, &rfd, 0, 0, &tv);

    if (!FD_ISSET(sock, &rfd))
        return false;

    u_long n = 0;
    ioctlsocket(sock, FIONREAD, &n);

    return n == 0;

#else

    fd_set rfd;
    FD_ZERO(&rfd);
    FD_SET(sock, &rfd);
    timeval tv = { 0 };
    select(sock + 1, &rfd, 0, 0, &tv);

    if (!FD_ISSET(sock, &rfd))
        return false;

    int n = 0;
    ioctl(sock, FIONREAD, &n);

    return n == 0;

#endif

}


///////////////////////////////////
//        PRIVATE METHODS        //
///////////////////////////////////


/*

RETURN:

 0 ---> no error
-1 ---> error parsing message json
-2 ---> error sending message
-3 ---> unexpected error

*/
int Client::sendMessage(int sock, message2 msg, std::atomic<bool>& b) {

    json jMsg;
    int sendCode = 0;

    try {

        jMsg = json{ {"packetNumber", msg.packetNumber},{"userName", msg.userName},{"type", msg.type}, {"typeCode", msg.typeCode}, {"fileName", msg.fileName}, {"folderPath", msg.folderPath}, {"body", msg.body}, {"hash", msg.hash} };

    }
    catch (...) {

        return -1;

    }

    // SENDING MESSAGE HEADER

    try {

        std::string jMsgString = jMsg.dump();

        if (b.load()) {

            return -11;

        }

        if (isClosed(sock)) {

            return -2;

        }

        uint64_t sizeNumber = jMsgString.length();
        sendCode = send(sock, jMsgString.c_str(), sizeNumber, 0);

        if (sendCode <= 0) {

            return -2;

        }

    }
    catch (...) {

        return -3;

    }

    return 0;

}

int sendFile2(int nOutFd, std::string path, off_t* pOffset, int nCount) {

    std::ifstream fin(path.c_str(), std::ifstream::binary);
    char buffer[PACKET_SIZE];
    int byteRead = 0;
    while (!fin.eof()) {

        memset(buffer, 0, PACKET_SIZE);
        fin.read(buffer, PACKET_SIZE);
        byteRead += fin.gcount();

        if ((send(nOutFd, buffer, fin.gcount(), 0)) != fin.gcount())
        {
            return -1;
        }
    }

    fin.close();

    return byteRead;
}

/*

RETURN:

 0 ---> no error
-1 ---> error opening filePath
-2 ---> socket was closed yet
-3 ---> error sending file stream
-4 ---> unexpected error

*/
int Client::sendFileStream(int sock, std::string filePath, std::atomic<bool>& b) {

    json jMsg;
    int sendCode = 0;
    off_t offset = 0;

    try {

        int sendFileReturnCode = sendFile2(sock, filePath, &offset, 0);

    }
    catch (...) {

        return -4;

    }

    return 0;

}

/*

RETURN:

 0 ---> no error
-1 ---> error receiving message RESPONSE
-2 ---> unexpected error

*/
int Client::readMessageResponse(int sock, std::string& response) {

    char rcvBuf[PACKET_SIZE];
    int rcvCode = 0;
    int iResult = 0;

    try {

        if (isClosed(sock)) {

            return -2;

        }

        memset(rcvBuf, 0, PACKET_SIZE);
        int rcvCode = recv(sock, &(rcvBuf[0]), PACKET_SIZE, 0);

        if (rcvCode <= 0) {

            return -1;

        }

        response.clear();
        response = (char*)rcvBuf;

    }
    catch (...) {

        return -2;

    }

    return 0;

}

/*

RETURN:

 0 ---> no error
-1 ---> error receiving configuration Stream PACKET
-2 ---> unexpected error

*/
int Client::readInitialConfStream(int sock, int packetsNumber, std::string& conf) {

    // per ogni stream ricevuto dal client scrivo su un file temporaneo
    // se lo stream � andato a buon fine e ho ricevuto tutto faccio una copia dal file temporaneo a quello ufficiale
    // ed eliminio il file temporaneo

    int i = 0;
    int sockFd = sock;
    char buffer[PACKET_SIZE + 1];

    // string to write to file
    std::string confWriteString;
    try {

        do {

            if (isClosed(sock)) {

                return -2;

            }

            i++;

            // reset char array
            memset(buffer, 0, PACKET_SIZE);
            buffer[PACKET_SIZE] = '\0';

            int rcvCode = recv(sock, buffer, PACKET_SIZE, 0);

            if (rcvCode <= 0) {

                return -1;

            }

            confWriteString.append((char*)buffer, rcvCode);

        } while (i < packetsNumber);

        conf = confWriteString;

    }
    catch (...) {

        return -2;

    }

    return 0;

}



