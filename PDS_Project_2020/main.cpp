
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

    // MODALITA' 2 -> CLIENT RICEVE DAL SERVER TUTTO IL FILE SYSTEM
    if (isMode2) {

        // remove all the contents of the folder path of the user
        fs::directory_iterator dir(client.uc.folderPath), end;
        while (dir != end) {
            //if (dir->path().string() != client.uc.folderPath)
                fs::remove_all(dir->path().string());
            dir++;
        }
        /*fs::remove_all(client.uc.folderPath);
        fs::create_directory(client.uc.folderPath);*/

        // instantiate the LOCAL SERVER
        Server server;
        server.readConfiguration("userServerConfiguration.json");
        server.logFile = "gaetano_Local_Server.txt";

        // get the user-local server configuration to be sent to the SERVER
        userServerConf userSC = {
            server.sc.ip,
            server.sc.port
        };

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

        // send the Local_Server configuration to the SERVER
        int resCode = client.sendToServer(sock, 7, "", "", userScString, 0, "", 0, b);
        client.serverDisconnection(sock);

        // start the local_Server
        server.initLogger();
        int exitCode = -1;
        try {

            exitCode = server.startListening();
        }
        catch (...) {
            std::cout << "catched" << std::endl;
        }

        std::cout << "SERVER EXITED with code: " << exitCode << std::endl;

        // start the FW
        /*FileWatcher2 fw{ client, "C:\\Users\\gabuscema\\Desktop\\UserFolder", std::chrono::milliseconds(15000) };
        fw.start();*/

    }
    /*else */{

        FileWatcher2 fw{ client, client.uc.folderPath, std::chrono::milliseconds(15000) };

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

            //std::cout << "TENTETIVE " << std::endl;
            resCode = client.sendToServer2(sock, 5, client.uc.folderPath, "", "", 0, "", 0, b, fw.paths_);
            client.serverDisconnection(sock);

        }
        //Sleep(10000);
        std::cout << "INITIAL CONF RECEIVED" << std::endl;
        /*Sleep(50000);
        std::cout << "INITIAL CONF RECEIVED" << std::endl;*/

        fw.start();
    }

    return 0;
}


// Per eseguire il programma: CTRL+F5 oppure Debug > Avvia senza eseguire debug
// Per eseguire il debug del programma: F5 oppure Debug > Avvia debug

// Suggerimenti per iniziare: 
//   1. Usare la finestra Esplora soluzioni per aggiungere/gestire i file
//   2. Usare la finestra Team Explorer per connettersi al controllo del codice sorgente
//   3. Usare la finestra di output per visualizzare l'output di compilazione e altri messaggi
//   4. Usare la finestra Elenco errori per visualizzare gli errori
//   5. Passare a Progetto > Aggiungi nuovo elemento per creare nuovi file di codice oppure a Progetto > Aggiungi elemento esistente per aggiungere file di codice esistenti al progetto
//   6. Per aprire di nuovo questo progetto in futuro, passare a File > Apri > Progetto e selezionare il file con estensione sln
