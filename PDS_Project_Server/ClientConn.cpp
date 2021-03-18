
#include "server.hpp"
#include <algorithm>
#include <vector>


#ifdef _WIN32

#include <winsock2.h>
#include <Windows.h>
#include <boost/filesystem.hpp>


#else

#include <arpa/inet.h>
#include "boost/filesystem.hpp"

#endif

#define PACKET_SIZE 1024

//////////////////////////////////
//        PUBLIC METHODS        //
//////////////////////////////////


/*

Check if the connection with the server IS CLOSED.

RETURN:

    true: connection IS CLOSED
    false: connection IS OPENED

*/
bool Server::ClientConn::isClosed(int sock) {

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

/*

Client Connection Thread handle.

*/
void Server::ClientConn::handleConnection() {

    std::thread inboundChannel([this]() {

        this->serv.activeConnections++;
        // check if socket is closed
        if (true) {

            message2 msg;
            bool numberSocketSetted = false;

            try {

                // set logger for client connection using the server log file
                if (initLogger() == 0) {

                    // read message initialization from the client to understand what USER it is
                    std::string buf;
                    int resCode = readMessage(sock, buf);
                    resCode = fromStringToMessage(buf, msg);

                    this->userName = msg.userName;

                    if (this->serv.incrementNumSocket(msg.userName)) {

                        msg.body = "socket accepted";
                        sendResponse(0, "Socket Accepted", msg);

                        numberSocketSetted = true;
                        // while loop to wait for different messages from client
                        waitForMessage();

                        this->serv.decrementNumSocket(msg.userName);

                    }
                    else {

                        std::cout << "not accepted" << std::endl;
                        // send response to CONNECTION MESSAGE by the client
                        msg.body = "socket NOT accepted";
                        sendResponse(-1, "Socket Not Accepted beacuse to many sockets were opened for the USER", msg);

                    }

                    log->flush();
                    serv.unregisterClient(sock);

                }

            }
            catch (...) {

                log->flush();
                serv.unregisterClient(sock);

            }

        }
        else {

            // can't log because the logger wasn't initialized; in any case the socket will be closed
            std::cout << "Errore handle" << std::endl;
            serv.unregisterClient(sock);

        }

        });

    inboundChannel.detach();

}


///////////////////////////////////
//        PRIVATE METHODS        //
///////////////////////////////////


/*

Initialize Client Connection Logger.

RETURN:

 0 -> no error
-1 -> logger initialization error
-2 -> generic error
-3 -> unexpected error

*/
int Server::ClientConn::initLogger() {

    try
    {
        this->log = this->serv.log;

        if (this->log == nullptr)
            this->log = spdlog::basic_logger_mt("client_" + std::to_string(this->sock), "client_" + std::to_string(this->sock) + ".txt");

        this->logName = "[client_" + std::to_string(this->sock) + "]: ";
        this->log->info(logName + "Logger initialized correctly");
    }
    catch (const spdlog::spdlog_ex& ex)
    {
        std::string error = ex.what();
        std::cerr << "error during logger initialization: " + error << std::endl;
        return -1;
    }
    catch (const std::exception& ex)
    {
        std::string error = ex.what();
        std::cerr << "generic error during logger initialization: " + error << std::endl;
        return -2;
    }
    catch (...)
    {
        std::cerr << "Unexpected error appened during logger initialization" << std::endl;
        return -3;
    }

    return 0;

}

/*

Wait for client file-message.
If socket or connection errors happen, it will close the socket.

*/
void Server::ClientConn::waitForMessage() {

    message2 msg;
    int resCode;
    uint64_t sizeNumber;
    std::string initialConf;

    // declaration of response message
    message2 response;

    running.store(true);

    while (running.load()) {

        try {

            log->info(logName + "");
            log->info(logName + "wait for message from the client");

            std::string buf;

            milliseconds ms = duration_cast<milliseconds>(
                system_clock::now().time_since_epoch()
                );

            this->activeMS.store(ms.count());

            buf.clear();
            resCode = readMessage(sock, buf);

            // check if the JSON is well formatted
            if( 
                buf.find("body") == std::string::npos ||
                buf.find("fileName") == std::string::npos ||
                buf.find("folderPath") == std::string::npos ||
                buf.find("hash") == std::string::npos ||
                buf.find("packetNumber") == std::string::npos ||
                buf.find("type") == std::string::npos ||
                buf.find("typeCode") == std::string::npos ||
                buf.find("userName") == std::string::npos
            ){

                log->info(logName + "");
                log->info(logName + "[RCV HEADER]: JSON BAD FORMATTED -> " + buf);

                resCode = -1;

            }

            if (resCode == 0) {

                ms = duration_cast<milliseconds>(
                    system_clock::now().time_since_epoch()
                    );

                this->activeMS.store(ms.count());

                resCode = fromStringToMessage(buf, msg);

                if (resCode == 0) {

                    log->info(logName + "");
                    log->info(logName + "[RCV HEADER]: message " + std::to_string(msg.typeCode) + " parsed correctly");

                    switch (msg.typeCode) {

                        // file update
                    case 1:
                    case 3:

                        updateFile(resCode, buf, msg);

                        break;

                        // file rename
                    case 2:

                        renameFile(resCode, buf, msg);

                        break;

                        // file delete
                    case 4:

                        deleteFile(resCode, buf, msg);

                        break;

                        // initial configuration
                    case 5:

                        initialConfiguration(resCode, buf, msg, initialConf);

                        break;

                        // close connection
                    case 6:

                        running.store(false);

                        resCode = sendResponse(resCode, buf, msg);

                        log->info(logName + "[SND HEADER RESP]: returned code: " + std::to_string(resCode));

                        // if there are errors on sending, the thread will exit
                        if (resCode < 0)
                            this->running.store(false);

                        break;

                    case 7:

                        // start a local file watcher thread
                        resCode = sendFolderToUser(resCode, buf, msg);

                        break;

                    case 8:

                        // end of user allignment -> the server on client side will exit
                        resCode = sendResponse(resCode, buf, msg);
                        this->running.store(false);
#ifdef _WIN32
                        closesocket(this->serv.ListenSocket);
#endif
                        shutdown(this->serv.csock, 2);
                        this->serv.running.store(false);

                        break;

                    }

                }
                else {

                    running.store(false);
                    log->info(logName + "[RCV HEADER]: error parsing message received from client");

                }

            }
            else {

                resCode = sendResponse(resCode, buf, msg);
                log->info(logName + "[SND HEADER RESP]: returned code: " + std::to_string(resCode));

                running.store(false);

            }

        }
        catch (...) {

            running.store(false);
            log->error(logName + "unexpected error happened");
            log->info(logName + "going to sleep for 1 second because of error");

#ifdef _WIN32
            Sleep(1000);
#else
            sleep(1);
#endif
            return;

        }

    }

    log->info(logName + "[CLIENT-CONN-" + std::to_string(this->sock) + "]: exited from run\n");

}

/*

Receive a message from the server.

RETURN:

 0 -> no error
-1 -> message error on receive
-2 -> unexpected error

*/
int Server::ClientConn::readMessage(int fd, std::string& bufString) {

    char rcvBuf[PACKET_SIZE];    // Allocate a receive buffer
    int rcvCode = 0;
    struct timeval timeout;
    int iResult = 0;

    try {

        if (isClosed(sock)) {

            this->running.store(false);
            return -2;

        }

        bufString.clear();

        // Receive the message header
        memset(rcvBuf, 0, PACKET_SIZE);
        int rcvCode = recv(sock, rcvBuf, PACKET_SIZE, 0);

        // Receive message error
        if (rcvCode <= 0) {

            std::string sockError = "Error " + rcvCode;
            bufString = sockError;
            this->running.store(false);

            return -1;

        }

        std::string respString = (char*)rcvBuf;
        bufString = respString;

    }
    catch (...) {

        bufString = "Unexpected error reading message";
        return -2;

    }

    return 0;

}

/*

Read the file stream from the client.

RETURN:

0  -> no error
-1 -> error reading from socket
-2 -> error updating server-local file
-3 -> unexpected error

*/
int Server::ClientConn::readFileStream(int packetsNumber, int fileFd) {

    // per ogni stream ricevuto dal client scrivo su un file temporaneo
    // se lo stream � andato a buon fine e ho ricevuto tutto faccio una copia dal file temporaneo a quello ufficiale
    // ed eliminio il file temporaneo

    int i = 0;
    int sockFd = sock;
    char buffer[BUFF_SIZE];

    try {

        do {

            if (isClosed(sock)) {

                this->running.store(false);
                return -2;

            }

            i++;

            // reset char array
            memset(buffer, 0, BUFF_SIZE);

            int read_return = read(sockFd, buffer, BUFF_SIZE);
            if (read_return == -1) {

                log->error(logName + "An error occured reading packet # " + std::to_string(i) + " from socket " + std::to_string(sockFd));
                return -1;

            }

            if (write(fileFd, buffer, read_return) == -1) {

                log->error(logName + "An error occured writing packet # " + std::to_string(i) + " to file descriptor: " + std::to_string(fileFd));
                return -2;

            }

        } while (i < packetsNumber);

    }
    catch (...) {

        return -3;

    }

    return 0;

}

/*

Update the file on the server side.

*/
void Server::ClientConn::updateFile(int& resCode, std::string buf, message2& msg) {

    bool error = false;
    resCode = sendResponse(resCode, buf, msg);
    log->info(logName + "[SND HEADER RESP]: returned code: " + std::to_string(resCode));
    //log->flush();
    if (resCode < 0) error = true;
    if (isClosed(sock) || !this->running.load() || resCode < 0) goto checkCode;

    resCode = handleFileUpdate(msg, buf);
    log->info(logName + "[RCV STREAM]: returned code: " + std::to_string(resCode));
    //log->flush();
    if (resCode < 0) error = true;
    if (isClosed(sock) || !this->running.load()) goto checkCode;

    std::cout << "[SND STREAM RESP]: " << buf << std::endl;
    if (resCode < 0) error = true;
    resCode = sendResponse(resCode, buf, msg);
    log->info(logName + "[SND STREAM RESP]: returned code: " + std::to_string(resCode));

checkCode:

    if (isClosed(sock) || !this->running.load() || error) {

        this->running.store(false);
        return;

    }

    return;

}

/*

Rename the file on the server-side.

*/
void Server::ClientConn::renameFile(int& resCode, std::string buf, message2& msg) {

    resCode = handleFileRename(msg, buf);
    log->info(logName + "[RENAME]: returned code: " + std::to_string(resCode));
    if (isClosed(sock) || !this->running.load()) goto checkCode;

    resCode = sendResponse(resCode, buf, msg);
    log->info(logName + "[SND HEADER RESP]: returned code: " + std::to_string(resCode));

checkCode:

    // if there are errors on sending, the thread will exit
    if (isClosed(sock) || !this->running.load()) {

        this->running.store(false);
        return;

    }
}

/*

Delete the file on the server-side.

*/
void Server::ClientConn::deleteFile(int& resCode, std::string buf, message2& msg) {

    resCode = handleFileDelete(msg, buf);
    log->info(logName + "[DELETE]: returned code: " + std::to_string(resCode));
    if (isClosed(sock) || !this->running.load()) goto checkCode;

    resCode = sendResponse(resCode, buf, msg);
    log->info(logName + "[SND HEADER RESP]: returned code: " + std::to_string(resCode));

checkCode:

    // if there are errors on sending, the thread will exit
    if (isClosed(sock) || !this->running.load()) {

        this->running.store(false);
        return;

    }

}

/*

Obtain the actual configuration on the server-side.

*/
void Server::ClientConn::initialConfiguration(int& resCode, std::string buf, message2& msg, std::string initialConf) {

    initialConf.clear();

    resCode = selective_search(initialConf, buf, msg);

    if (resCode == 0) {

        int div = initialConf.length() / PACKET_SIZE;
        int rest = initialConf.length() % PACKET_SIZE;

        if (rest > 0)
            div++;

        // set # packets used 
        msg.packetNumber = div;

        resCode = sendResponse(resCode, buf, msg);
        log->info(logName + "[SND HEADER CONF]: returned code: " + std::to_string(resCode));
        if (resCode < 0) goto checkCode;

        resCode = readMessage(sock, buf);
        if (resCode < 0) goto checkCode;

        resCode = fromStringToMessage(buf, msg);
        log->info(logName + "[RCV HEADER CONF RESP]: returned code: " + std::to_string(resCode));
        if (resCode < 0) goto checkCode;

        resCode = SendInitialConf(initialConf, sock, initialConf.length());
        log->info(logName + "[SND STREAM]: returned code: " + std::to_string(resCode));
        if (resCode < 0) goto checkCode;

    }

checkCode:

    // if there are errors on sending, the thread will exit
    if (isClosed(sock) || !this->running.load()) {

        this->running.store(false);
        return;

    }

}

/*

Obtain the actual configuration on the server-side.

*/
int Server::ClientConn::sendFolderToUser(int& resCode, std::string buf, message2& msg) {

    std::string userName = msg.userName;

    // inside the message body there will be the user_server_IP and the user_server_PORT
    std::string body = msg.body;

    resCode = sendResponse(resCode, buf, msg);
    log->info(logName + "[SND HEADER RESP]: returned code: " + std::to_string(resCode));
    log->flush();

    // if there are errors on sending, the thread will exit
    if (isClosed(sock) || !this->running.load()) {

        log->flush();
        this->running.store(false);
        return -1;

    }

    userServerConf userServerConf;

    fromStringToUserServerConf(body, userServerConf);

    std::string serverBackupFolder = server.backupFolder;
    this->user_server_IP = userServerConf.ip;
    this->user_server_PORT = userServerConf.port;
    this->user_server_timeout = 10000;

    serverBackupFolder = server.backupFolder;

#   ifdef BOOST_POSIX_API
    std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '\\', '/');
#   else
    std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '/', '\\');
#   endif

    this->user_server_path = serverBackupFolder + separator() + msg.userName;

    // check if the folder path exists

    if(!fs::exists(this->user_server_path)) {
        fs::create_directory(this->user_server_path);
    }

    std::cout << this->user_server_path << std::endl;

    milliseconds ms = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()
        );

    this->activeMS.store(ms.count());

    UserFW fw{ this->user_server_path, std::chrono::milliseconds(15000), this->userName, this->user_server_IP, this->user_server_PORT,  this->user_server_timeout };
    fw.start();

    log->info(logName + "[USER FW]: returned USER FW");
    log->flush();

    return 0;

}

/*
Send the file to the client.
*/
int Server::ClientConn::sendFile2(int nOutFd, std::string path, off_t* pOffset) {

    std::ifstream fin(path.c_str(), std::ifstream::binary);

    char buffer[PACKET_SIZE];
    int byteRead = 0;
    while (!fin.eof()) {

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

Handle the File Update operation.

RETURN:

 0 -> no error
-1 -> error reading packet from socket
-2 -> error updating local-server file
-3 -> unexpected error

*/
int Server::ClientConn::handleFileUpdate(message2 msg, std::string buf) {

    if (isClosed(sock)) {

        this->running.store(false);
        return -2;

    }

    std::string serverBackupFolder, msgFolderPath, path, hashReceived;
    fs::path dstFolder, filePath;

    serverBackupFolder = server.backupFolder;
    msgFolderPath = msg.folderPath;
    hashReceived = msg.hash;

    try {

        buf.clear();

        serverBackupFolder = server.backupFolder;
        msgFolderPath = msg.folderPath;

        // this check is fundamental to distinguish the server from the user-server-side
        if (serverBackupFolder.length() > 0) {

#       ifdef BOOST_POSIX_API 
            std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '\\', '/');
            std::replace(msgFolderPath.begin(), msgFolderPath.end(), '\\', '/');
            std::replace(msgFolderPath.begin(), msgFolderPath.end(), '\:', '\_');
#       else
            std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '/', '\\');
            std::replace(msgFolderPath.begin(), msgFolderPath.end(), '/', '\\');
            std::replace(msgFolderPath.begin(), msgFolderPath.end(), '\:', '\_');
#       endif

            path = serverBackupFolder + separator() + msg.userName + separator() + msgFolderPath;

        }
        else
            path = msgFolderPath;

        dstFolder = path;

        std::cout << dstFolder << std::endl;

        if (!fs::exists(dstFolder))
#ifdef _WIN32
            std::filesystem::create_directories(path);
#else
            std::experimental::filesystem::create_directories(path);
#endif

        path += separator() + msg.fileName;

        filePath = path;

        int retryCount = 0;
        bool completed = false;

        int i = 0;
        char  buffer[BUFF_SIZE + 1];

        // string to write to file
        std::string fileWriteString;

        if (msg.packetNumber > 0) {

            fileWriteString.clear();

            do {

                if (isClosed(sock)) {

                    this->running.store(false);
                    return -2;

                }

                i++;

                // reset char array
                memset(buffer, 0, BUFF_SIZE);
                buffer[BUFF_SIZE] = '\0';

                int read_return = recv(sock, buffer, PACKET_SIZE, 0);

                if (read_return == -1) {

                    buf = "Error reading bytes from file- std::string received";
                    return -1;

                }

                if (i < msg.packetNumber && read_return != PACKET_SIZE) {

                    buf = "Error: the chunk was incomplete";
                    return -1;

                }

                fileWriteString.append((char*)buffer, read_return);

            } while (i < msg.packetNumber);

        }

        // CHECK FILE CONTENT HASH

        CryptoPP::Weak::MD5 hash;
#ifdef _WIN32
        CryptoPP::byte digest[CryptoPP::Weak::MD5::DIGESTSIZE];

        hash.CalculateDigest(digest, (CryptoPP::byte*)fileWriteString.c_str(), fileWriteString.length());
#else
        byte digest[CryptoPP::Weak::MD5::DIGESTSIZE];
        hash.CalculateDigest(digest, (byte*)fileWriteString.c_str(), fileWriteString.length());
#endif


        CryptoPP::HexEncoder encoder;
        std::string output;
        encoder.Attach(new CryptoPP::StringSink(output));
        encoder.Put(digest, sizeof(digest));
        encoder.MessageEnd();
        boost::algorithm::to_lower(output);

        if (output != msg.hash)
            return -3;

        FILE* file = fopen(path.c_str(), "wb");

        // take file descriptor from FILE pointer 
        int fileFd = fileno(file);

        if (write(fileFd, fileWriteString.c_str(), fileWriteString.size()) == -1) {

            buf = "Error updating bytes into server-local file";
            fclose(file);
            return -4;

        }

        fclose(file);

        fileWriteString.clear();

    }
    catch (std::exception e) {

        std::string error = e.what();
        buf = "Unexpected error updating file " + error;
        return -5;

    }

    return 0;

}

/*

Compute the file hash.

RETURN:

( std::string) -> hash  std::string or empty  std::string in case of error.

*/
std::string Server::ClientConn::compute_hash(const std::string file_path)
{

    std::string result;
    CryptoPP::Weak::MD5 hash;
    CryptoPP::FileSource(file_path.c_str(), true, new
        CryptoPP::HashFilter(hash, new CryptoPP::HexEncoder(new
            CryptoPP::StringSink(result), false)));
    return result;

}

/*

Handle the File Delete operation.

RETURN:

 0 -> no error
-1 -> folder doesn't exists locally to the server
-2 -> file doesn't exists locally to the server
-3 -> delete operation couldn't be completed
-4 -> unexpected error

*/
int Server::ClientConn::handleFileDelete(message2 msg, std::string buf) {

    std::string serverBackupFolder, msgFolderPath, path;
    fs::path dstFolder, filePath;

    serverBackupFolder = server.backupFolder;
    msgFolderPath = msg.folderPath;

    try {

        buf.clear();

        if (isClosed(sock)) {

            this->running.store(false);
            return -2;

        }

        serverBackupFolder = server.backupFolder;
        msgFolderPath = msg.folderPath;


#       ifdef BOOST_POSIX_API   //workaround for user-input files
        std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '\\', '/');
        std::replace(msgFolderPath.begin(), msgFolderPath.end(), '\\', '/');
        std::replace(msgFolderPath.begin(), msgFolderPath.end(), '\:', '\_');
#       else
        std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '/', '\\');
        std::replace(msgFolderPath.begin(), msgFolderPath.end(), '/', '\\');
        std::replace(msgFolderPath.begin(), msgFolderPath.end(), '\:', '\_');
#       endif

        path = serverBackupFolder + separator() + msg.userName + separator() + msgFolderPath;

        dstFolder = path;

        // folder doesn't exist
        if (!fs::exists(dstFolder)) {

            buf = "folder doesn't exist locally to the server";
            return -1;

        }

        path += separator() + msg.fileName;

        filePath = path;

        // the file doesn't exist
        if (!fs::exists(filePath)) {

            buf = "file doesn't exist locally to the server";
            return 0; // file doesn't exist

        }

        int retryCount = 0;
        bool completed = false;

        while (!completed && retryCount < 5)
        {
            try
            {

                if (isClosed(sock)) {

                    this->running.store(false);
                    return -2;

                }

                retryCount++;
                fs::remove_all(path);
                completed = true;

                // try to delete the folder if empty
                fs::remove(dstFolder);
            }
            catch (...)
            {
#ifdef _WIN32
                Sleep(1000);
#else
                sleep(1);
#endif
            }
        }

        if (!completed) {

            buf = "couldn't be able to complete delete-operation";
            return -3;

        }

    }
    catch (...) {

        buf = "unexpected error during delete-operation";
        return -4;

    }

    return 0;

}

/*

Handle the File Rename operation.

RETURN:

 0 -> no error
-1 -> folder doesn't exists locally to the server
-2 -> file doesn't exists locally to the server
-3 -> rename operation couldn't be completed
-4 -> unexpected error

*/
int Server::ClientConn::handleFileRename(message2 msg, std::string buf) {

    std::string serverBackupFolder, msgFolderPath, path, oldPathString, newPathString, newMsgFolderPath;
    fs::path dstFolder;

    try {

        buf.clear();

        if (isClosed(sock)) {

            this->running.store(false);
            return -2;

        }

        serverBackupFolder = server.backupFolder;
        msgFolderPath = msg.folderPath;
        newMsgFolderPath = msg.body;

#       ifdef BOOST_POSIX_API   //workaround for user-input files
        std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '\\', '/');
        std::replace(msgFolderPath.begin(), msgFolderPath.end(), '\\', '/');
        std::replace(msgFolderPath.begin(), msgFolderPath.end(), '\:', '\_');
        std::replace(newMsgFolderPath.begin(), newMsgFolderPath.end(), '\:', '\_');
#       else
        std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '/', '\\');
        std::replace(msgFolderPath.begin(), msgFolderPath.end(), '/', '\\');
        std::replace(msgFolderPath.begin(), msgFolderPath.end(), '\:', '\_');
        std::replace(newMsgFolderPath.begin(), newMsgFolderPath.end(), '\:', '\_');
#       endif

        path = serverBackupFolder + separator() + msg.userName + separator() + msgFolderPath;

        dstFolder = path;

        // if the path doesn't exist return because there isn't any files inside
        if (!fs::exists(dstFolder))
            return 0;

        oldPathString = path + separator() + msg.fileName;

        newPathString = serverBackupFolder + separator() + msg.userName + separator() + newMsgFolderPath;
        //newPathString = path + separator() + msg.body;

        fs::path oldPath = oldPathString;
        fs::path newPath = newPathString;

        // if the file doesn't exist return 
        if (!fs::exists(oldPath)) {

            buf = "file doesn't exists locally to the server";
            return -2;

        }
         
        std::string newMessageFolder;

        size_t i = newMsgFolderPath.rfind(separator(), newMsgFolderPath.length());
        if (i != std::string::npos) {
            newMessageFolder = (newMsgFolderPath.substr(0, i));
        }

        std::cout << msgFolderPath + " " + newMessageFolder << std::endl;

        if (msgFolderPath != newMessageFolder) {

            //check if the new folder exists
            std::string newPathCheck = serverBackupFolder + separator() + msg.userName + separator() + newMessageFolder;

            std::cout <<"PATHCHECK:" + newPathCheck << std::endl;
            
            if (!fs::exists(newPathCheck)) {

                fs::create_directory(newPathCheck);

            }

        }

        bool completed = false;
        int retryCount = 0;
        while (!completed && retryCount < 5)
        {

            try
            {

                if (isClosed(sock)) {

                    this->running.store(false);
                    return -2;

                }

                retryCount++;
#ifdef _WIN32
                fs::rename(oldPath, newPath);
#else
                fs::rename(oldPath, newPath);
#endif
                completed = true;
            }
            catch (std::exception e)
            {
#ifdef _WIN32
                Sleep(5000);
#else
                sleep(5);
#endif
            }

                }

        if (!completed || retryCount >= 5) {

            buf = "couldn't be able to complete renaming-operation";
            return -3;

        }


        // if the folder
        if (fs::is_empty(path)) {

            fs::remove(path);

        }

    }
    catch (...) {

        buf = "unexpected error during the renaming-operation";
        return -4;

    }

    return 0;

 }

/*

Recursive search in order to get the local configuration.

RETURN:

0 -> no error
-1 -> error initializing recursive search locally to the server
-2 -> error during recursive search

*/
int Server::ClientConn::selective_search(std::string& response, std::string buf, message2& msg)
{
    std::string serverBackupFolder, msgFolderPath, path;

    try {

        // reset response  std::string
        response.clear();
        buf.clear();

        serverBackupFolder = server.backupFolder;
        msgFolderPath = msg.folderPath;


#       ifdef BOOST_POSIX_API   //workaround for user-input files
        std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '\\', '/');
        std::replace(msgFolderPath.begin(), msgFolderPath.end(), '\\', '/');
        std::replace(msgFolderPath.begin(), msgFolderPath.end(), '\:', '\_');
#       else
        std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '/', '\\');
        std::replace(msgFolderPath.begin(), msgFolderPath.end(), '/', '\\');
        std::replace(msgFolderPath.begin(), msgFolderPath.end(), '\:', '\_');
#       endif

        path = serverBackupFolder + separator() + msg.userName + separator() + msgFolderPath;

    }
    catch (...) {

        response = "[]";
        buf = "unexpected error initializing recursive search locally to the server";
        return -1;

    }

    boost::filesystem::path dstFolder = path;

    if (!boost::filesystem::exists(dstFolder)) {

        response = "[]";
        buf = "path doesn't exists -> no configuration available";
        return 0;

    }

    try
    {

        fs::recursive_directory_iterator dir(path), end;
        auto jsonObjects = json::array();

        // this sub-path represent the one to remove from the path to be sent to the user
        std::string subPath = serverBackupFolder + separator() + msg.userName + separator();

        // iterating over the local folder of the client
        while (dir != end)
        {
#ifdef _WIN32
            if (dir->is_directory()) {
                dir++;
                continue;
            }
#else
            if (fs::is_directory(dir->path())) {
                dir++;
                continue;
            }
#endif

            std::string folderToUser = dir->path().string().substr(subPath.length(), dir->path().string().length());

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
            // folder directory to be sent to the user
            std::string folder = folderToUser;
#endif
            std::string filename = path;
            struct stat result;
            long lastModify = 0;
            long fileSize = 0;

            auto test = fs::last_write_time(dir->path());
            unsigned long long testMS = time_point_cast<std::chrono::microseconds>(test).time_since_epoch().count();
            testMS *= 10;

            // get file size reading the file

            if (stat(dir->path().string().c_str(), &result) == 0)
            {
                //lastModify = result.st_mtime;
                fileSize = result.st_size;
                std::cout << dir->path().filename().string() << " " << fileSize << std::endl;
            }
            message2 msgConf{

                "",
                7,
                testMS,
                this->compute_hash(dir->path().string()),
                fileSize,
                folder,
                dir->path().filename().string(),
                this->userName,
                ""

            };
            /*initialConf conf{
                dir->path().filename().string(),
                this -> compute_hash(dir->path().filename().string())
            };*/

            // JSON to be added inside the Json Array
            json obj = json{
                {"type", msgConf.type},
                {"typeCode", msgConf.typeCode},
                {"timestamp", msgConf.timestamp},
                {"hash", msgConf.hash},
                {"packetNumber", msgConf.packetNumber},
                {"folderPath", msgConf.folderPath},
                {"fileName", msgConf.fileName},
                {"userName", msgConf.userName},
                {"body", msgConf.body}
            };
            std::string  stringa = obj.dump();
            jsonObjects.push_back(obj);

            ++dir;
        }

        response = jsonObjects.dump();

    }
    catch (...)
    {

        response = "[]";
        buf = "unexpected error during recursive search -> no configuration available";
        return -2;

    }

    buf = "server configuration read correctly";
    return 0;

}

/*

Send response message to the client.

RETURN:

0 -> no error
-1 -> error sending response to the client
-2 -> error parsing response into  std::string
-3 -> unexpected error

*/
int Server::ClientConn::sendResponse(int resCode, std::string buf, message2 msg) {

    message2 response;
    std::string responseString;
    int sendCode = 0;

    try {

        response.body = buf;

        if (isClosed(sock)) {

            this->running.store(false);
            return -2;

        }

        if (resCode == 0)
            handleOkResponse(response, msg);

        else
            handleErrorResponse(response, msg, resCode);

        resCode = fromMessageToString(responseString, response);

        if (resCode == 0) {

            sendCode = send(sock, responseString.c_str(), responseString.length(), 0);

            if (sendCode <= 0) {

                // se non riesco a mandare nulla esco dalla run
                shutdown(sock, 2);
                this->running.store(false);
                return -1;

            }

        }
        else {

            return -2;

        }

    }
    catch (...) {

        return -3;

    }

    return 0;

}

/*

Send initial configuration to the client side.

RETURN:

0 -> no error
-1 -> error sending configuration Packet
-2 -> unexpected error

*/
int Server::ClientConn::SendInitialConf(std::string conf, int nOutFd, int nCount)
{

    int  nOutCount = 0;
    int       nInCount = 0;
    char szData[PACKET_SIZE];

    try {

        while (nOutCount < nCount)
        {

            if (isClosed(sock)) {

                this->running.store(false);
                return -2;

            }

            nInCount = (nCount - nOutCount) < PACKET_SIZE ? (nCount - nOutCount) : PACKET_SIZE;

            // iterate over the  std::string piece by piece
            std::string subConfString = conf.substr(nOutCount, nOutCount + nInCount);

            if ((send(nOutFd, subConfString.c_str(), nInCount, 0)) != (int)nInCount)
            {
                shutdown(sock, 2);
                this->running.store(false);
                conf.clear();
                return -1;
            }

            nOutCount += nInCount;
        }

        conf.clear();

    }
    catch (...) {

        return -2;

    }

    return nOutCount;
}

/*

Handle Ok Response if there were no error.

RETURN:

 0 -> no error
-1 -> unexpected error

*/
int Server::ClientConn::handleOkResponse(message2& response, message2& msg) {

    try {

        response.type = "ok";
        response.typeCode = 0;
        response.hash = msg.hash;
        response.folderPath = msg.folderPath;
        response.fileName = msg.fileName;
        response.userName = msg.userName;
        response.packetNumber = msg.packetNumber;
        response.body = msg.body;

    }
    catch (...) {

        return -1;

    }

    return 0;

}

/*

Handle Error Response in case of error.

RETURN:

0 -> no error
-1 -> unexpected error

*/
int Server::ClientConn::handleErrorResponse(message2& response, message2& msg, int errorCode) {

    try
    {

        response.typeCode = errorCode;
        response.folderPath = msg.folderPath;
        response.fileName = msg.fileName;
        response.userName = msg.userName;
        response.packetNumber = 1;
        response.type = "error";

    }
    catch (...)
    {

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
int Server::ClientConn::fromStringToMessage(std::string msg, message2& message) {

    try {

        auto jsonMSG = json::parse(msg);

        jsonMSG.at("type").get_to(message.type);
        jsonMSG.at("typeCode").get_to(message.typeCode);
        jsonMSG.at("folderPath").get_to(message.folderPath);
        jsonMSG.at("userName").get_to(message.userName);
        jsonMSG.at("fileName").get_to(message.fileName);
        jsonMSG.at("packetNumber").get_to(message.packetNumber);
        jsonMSG.at("hash").get_to(message.hash);
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

/*

Convert the message received into a  std::string, in order to be sent through a socket channel.

RETURN:

0 -> no error
-1 -> unexpected error

*/
int Server::ClientConn::fromMessageToString(std::string& messageString, message2& msg) {

    try
    {
        json jMsg = json{ {"packetNumber", msg.packetNumber},{"userName", msg.userName},{"type", msg.type}, {"typeCode", msg.typeCode}, {"fileName", msg.fileName}, {"folderPath", msg.folderPath}, {"body", msg.body}, {"hash", msg.hash} };
        messageString = jMsg.dump();
    }
    catch (...)
    {

        log->error(logName + "An error appened converting the message to  std::string: " + msg.type);

        return -1;
    }

    return 0;

}

