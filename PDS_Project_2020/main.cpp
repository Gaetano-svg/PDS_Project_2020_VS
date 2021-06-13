
#include "FileWatcher2.hpp"
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <iostream>
#ifdef _WIN32
#include "./CryptoPP/cryptlib.h"
#include "./CryptoPP/md5.h"
#include "./CryptoPP/files.h"
#include "./CryptoPP/hex.h"
#include "../PDS_Project_Server/server.hpp"
#else
#include <cryptopp/cryptlib.h>
#include <cryptopp/md5.h>
#include <cryptopp/files.h>
#include <cryptopp/hex.h>
#include "server.hpp"
#endif

#include <atomic>

int main(int argc, char* argv[]) {

    Client client;
    client.readConfiguration();
    client.initLogger();
    int sock;
    std::atomic<bool> b;
    b.store(false);

    bool isMode2 = client.uc.backupFromServer;

    // bool variable to understand if some error happens during the backup from server
    bool noErrorWithBackup = false;

    // BACKUP MODE -> client receive all the backup folder from server
    // 1. run a LOCAL server
    // 2. erase all the local folder
    // 3. receive all the server backup folder
    // 4. close the server
    while (isMode2 && !noErrorWithBackup) {

        // instantiate the LOCAL SERVER
        Server server;

        // set is true server variable to false
        server.isTrueServer = false;

        server.readConfiguration("userServerConfiguration.json");
        server.logFile = client.uc.name + "_Local_Server.txt";

        // get the user-local server configuration to be sent to the SERVER
        userServerConf userSC = {
            server.sc.ip,
            server.sc.port
        };

        // start the local_Server
        server.initLogger();

        while (!noErrorWithBackup) {
            std::string userScString;
            client.fromUserServerConfToString(userSC, userScString);

            while (client.serverConnection(sock) < 0) {

                std::cout << "couldn't open the connection with the server -> go to sleep for 5 seconds" << std::endl;
#ifdef _WIN32
                Sleep(5000);
#else
                sleep(5);
#endif

            }

            // remove all the contents of the folder path of the user
            // the folder will be erased only if the connection with the server is ok
            fs::directory_iterator dir(client.uc.folderPath), end;

            while (dir != end) {

                fs::remove_all(dir->path().string());
                dir++;

            }

            // send the Local_Server configuration to the SERVER
            int resCode = client.sendToServer(sock, 7, "", "", userScString, 0, "", 0, b);
            client.serverDisconnection(sock);

            int exitCode = -1;

            try {

                exitCode = server.startListening();
                if (exitCode == 0)
                    noErrorWithBackup = true;
                else
                    noErrorWithBackup = false;

            }
            catch (...) {
                noErrorWithBackup = false;
            }

            std::cout << "SERVER EXITED with code: " << exitCode << std::endl;

        }

    }

    // Receive all the Server folder configuration

    FileWatcher2 fw{ client, client.uc.folderPath, std::chrono::milliseconds(10000) };

    // ask for the configuration on the server side
    int resCode = -1;

    while (resCode < 0) {

        while (client.serverConnection(sock) < 0) {

            std::cout << "couldn't open the connection with the server -> go to sleep for 5 seconds" << std::endl;
#ifdef _WIN32
            Sleep(5000);
#else
            sleep(5);
#endif

        }

        resCode = client.sendToServer2(sock, 5, client.uc.folderPath, "", "", 0, "", 0, b, fw.paths_);
        client.serverDisconnection(sock);

    }

    std::cout << "INITIAL CONF RECEIVED" << std::endl;

    fw.start();

    return 0;

}