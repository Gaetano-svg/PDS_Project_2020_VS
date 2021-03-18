#include "server.hpp"


//////////////////////////////////
//        PUBLIC METHODS        //
//////////////////////////////////


/*

Read Server JSON configuration file.

RETURN:

 0   ---> no error
-1   ---> error opening/creating the file
-2   ---> error converting the JSON
-3   ---> error saving the JSON inside the struct

*/
int Server::readConfiguration(std::string file) {

    // Read SERVER configuration file in the local folder
    std::ifstream serverConfFile(file);
    json jServerConf;

    // check if file was opened/created correctly
    if (!serverConfFile)
    {
        std::string error = strerror(errno);
        std::cerr << "Server Configuration File: " << file << " could not be opened!";
        std::cerr << "Error code opening Server Configuration File: " << error;
        return -1;
    }

    // check if the JSON conversion is correct
    if (!(serverConfFile >> jServerConf))
    {
        std::cerr << "The Server Configuration File couldn't be parsed";
        return -2;
    }

    // save the server configuration inside a local SERVER struct
    try {

        this->sc = {
            jServerConf["ip"].get<std::string>(),
            jServerConf["port"].get<std::string>(),
            jServerConf["backupFolder"].get<std::string>(),
            jServerConf["userInactivityMS"].get<int>(),
            jServerConf["numberActiveClients"].get<int>(),
            jServerConf["secondTimeout"].get<int>(),
            jServerConf["numberUserActiveConnections"].get<int>(),
        };

    }
    catch (...) {

        std::cerr << "Error during the saving of the configuration locally to SERVER";
        return -3;

    }

    return 0;

}

/*

Initialize logger for the server.

RETURN:

 0   ---> no error
-1   ---> error opening/creating the log file

*/
int Server::initLogger() {

    // opening/creating the log file
    try
    {

#ifdef _WIN32
        this->log = spdlog::rotating_logger_mt(this->sc.ip, this->logFile, 1048576 * 4, 100000);
#else
        this->log = spdlog::basic_logger_mt(this->sc.ip, this->logFile);
#endif
        this->log->info("Logger initialized correctly");
        this->log->flush();

    }
    catch (const spdlog::spdlog_ex& ex)
    {
        std::cerr << ex.what() << std::endl;
        return -1;
    }

    return 0;

}

/*

Main function to let the server wait for client-connections.

RETURN:

 0  ---> no error
-1  ---> initialization error

*/
int Server::startListening() {

#ifdef _WIN32

    // run user-inactivity check THREAD
    WSADATA wsaData;
    int iResult;

    ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo* result = NULL;
    struct addrinfo hints;

    int iSendResult;
    char recvbuf[512];
    int recvbuflen = 512;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return -1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, sc.port.c_str(), &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return -1;
    }

    // Create a SOCKET for connecting to server
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return -1;
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return -1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return -1;
    }

#else

    int opt = 1;

    this->activeConnections.store(0);

    // initialize socket
    sock = socket(AF_INET, SOCK_STREAM, 0);

    log->info("socket created: " + std::to_string(sock));
    log->flush();

    if (sock == 0) {

        log->error("socket creation failed");
        return -1;

    }

    // set socket
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {

        std::string error = strerror(errno);
        log->error("setsockopt error: " + error);

        return -2;

    }

    sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(atoi(sc.port.c_str()));
    saddr.sin_addr.s_addr = INADDR_ANY;

    // bind socket
    if (bind(sock, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {

        std::string error = strerror(errno);
        log->error("bind error: " + error);

        return -3;

    }

    // listen to N different clients
    if (::listen(sock, 0) < 0) {

        std::string error = strerror(errno);
        log->error("listen error: " + error);

        return -4;

    }

#endif

    returningCode = 0;
    running.store(true);
    requestReceived.store(true);

    clients.clear();
    clientsNumberSocket.clear();
    activeConnections = 0;

    std::thread checkUserInactivityThread([&]() {

        checkUserInactivity();

    });

    //checkUserInactivityThread.detach();
    /*std::thread checkServerInactivityThread;

    // IF it is the LOCAL USER_SERVER
    if (!this->isTrueServer) {

        checkServerInactivityThread = std::thread([&]() {

            checkServerInactivity();

        });

    }*/



    while (running.load()) {

        try {

            sockaddr_in caddr;
#ifdef _WIN32
            int addrlen = sizeof(caddr);
#else
            socklen_t addrlen = sizeof(caddr);
#endif

            // before accepting the socket check the number of active client connections alive
            if (this->activeConnections < this->sc.numberActiveClients) {

                log->info("waiting for client connection");
                log->flush();

                // wait until client connection
#ifdef _WIN32

                // Accept a client socket
                ClientSocket = accept(ListenSocket, NULL, NULL);
                if (ClientSocket == INVALID_SOCKET) {
                    printf("accept failed with error: %d\n", WSAGetLastError());
                    closesocket(ListenSocket);
                    WSACleanup();
                    //return 1;
                }
                this->requestReceived.store(true);
                csock = ClientSocket;

                DWORD timeout = sc.secondTimeout;

                setsockopt(csock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(DWORD));
                setsockopt(csock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(DWORD));

                u_long non_blocking = 0;

                ioctlsocket(csock, FIONBIO, &non_blocking);

#else

                // wait until client connection
                csock = accept(sock, (struct sockaddr*)&caddr, &addrlen);

#endif

                if (!isClosed(csock)) {

                    if (csock <= 0) {

                        std::string error = strerror(errno);
                        log->error("accept error: " + error);

                    }
                    else {

                        log->info("the socket " + std::to_string(csock) + " was accepted");
                        log->flush();

                        //get socket ip address
                        struct sockaddr* ccaddr = (struct sockaddr*)&caddr;
                        std::string clientIp = ccaddr->sa_data;

                        // for each client allocate a ClientConnection object
                        auto client = std::shared_ptr<ClientConn>(new ClientConn(*this, this->logFile, this->sc, csock));

                        // this keeps the client alive until it's destroyed
                        {
                            std::unique_lock<std::mutex> lg(m);
                            clients[csock] = client;
                        }

                        // handle connection should return immediately
                        client->handleConnection();

                    }

                }
                else {

                    log->error("socket " + std::to_string(csock) + " was closed; the thread won't be created");
                    shutdown(csock, 2);

                }

            }
            else {

                try {

                    log->info("There are " + to_string(this->activeConnections) + " alive; go to sleep before accepting another one");
                    log->flush();
#ifdef _WIN32
                    Sleep(10000);
#else
                    sleep(10);
#endif

                }
                catch (...) {

                    log->error("an error occured while sleeping for 10 seconds");

                }

            }

        }
        catch (const std::exception& exc) {

            std::string error = exc.what();
            log->error("an error occured: " + error);
            log->flush();

        }
        catch (...) {

            log->error("an unexpected error occured ");
            log->flush();

            // interrupt 1 second before restart listening to socket connection
            try {

                log->info("sleep 1 second before restart listening");

#ifdef _WIN32
                Sleep(1000);
#else
                sleep(1);
#endif

            }
            catch (...) {

                log->error("an error occured during sleep");

            }

        }

    }

    // wait for all the threads
    std::cout << "[SERVER]: wait for check user thread" << std::endl;
    checkUserInactivityThread.join();
    std::cout << "[SERVER]: check user thread joined" << std::endl;
    /*if (!this->isTrueServer) {

        std::cout << "[SERVER]: wait for check server thread" << std::endl;
        checkServerInactivityThread.join();
        std::cout << "[SERVER]: check server thread joined" << std::endl;

    }*/

    std::cout << "[SERVER]: exited" << std::endl;
    log->error("[SERVER]: exited from run");


    return returningCode;

}

/*
Server destructor. Closes and remove all structures created.
*/
Server::~Server() {

    if (sock != -1) {

#ifdef _WIN32
        _close(sock);
#else
        close(sock);
#endif
    }

}


///////////////////////////////////
//        PRIVATE METHODS        //
///////////////////////////////////


/*

Thread to check user inactivity state.

*/
void Server::checkUserInactivity() {

    while (running.load()) {

        log->info("[CHECK-INACTIVITY]: go to sleep for 60 seconds");
        log->flush();

        requestReceived.store(false);

#ifdef _WIN32
        Sleep(60000);
#else
        sleep(60);
#endif

        if (this->clients.size() > 0) {

            for (auto it = clients.begin(); it != clients.end(); ++it)
            {

                // client socket
                int sock = it->first;

                // client object
                auto client = it->second;

                try {

                    long msClient = client->activeMS.load();

                    milliseconds ms = duration_cast<milliseconds>(
                        system_clock::now().time_since_epoch()
                        );

                    long msNow = ms.count();

                    long msTotInactiveTime = msNow - msClient;

                    log->info("socket " + std::to_string(sock) + " was inactive for " + std::to_string(msTotInactiveTime));
                    log->flush();

                    if (msTotInactiveTime >= this->sc.userInactivityMS) {

                        log->info("[CHECK-INACTIVITY]: client - socket " + std::to_string(sock) + " will be closed for inactivity (" + std::to_string(sc.userInactivityMS) + " milliSeconds)");
                        log->flush();
                        client->running.store(false);

                        // shutdown both receive and send operations
                        shutdown(sock, 2);

                    }

                }
                catch (...) {

                    log->error("an error occured checking inactivity for socket " + std::to_string(sock));
                }

            }

        }

        // check also the request received
        if (!isTrueServer && !requestReceived.load() && running.load()) {

            returningCode = -1;
            running.store(false);

#ifdef _WIN32

            closesocket(ListenSocket);
#endif
            shutdown(csock, 2);

        }

    }

    log->info("[CHECK-INACTIVITY]: exited");
    log->flush();

}

/*

Thread to check user inactivity state.

*/
void Server::checkServerInactivity() {

    do {
        requestReceived.store(false);
        log->info("[CHECK-SERVER-INACTIVITY]: go to sleep for 60 seconds");
        log->flush();

        try {

#ifdef _WIN32
            Sleep(60000);
#else
            sleep(60);
#endif
        }
        catch (...) {
            std::cout << "ERRORE" << std::endl;
        }


        if (!requestReceived.load()) {

            returningCode = -1;
            running.store(false);

#ifdef _WIN32

            closesocket(ListenSocket);
#endif
            shutdown(csock, 2);

        }

    } while (requestReceived.load() && running.load());

    log->info("[CHECK-SERVER-INACTIVITY]: exited");
    log->flush();

}

/*

Close one client-socket and erase it from the local map.

*/
void Server::unregisterClient(int csock) {

    std::unique_lock <std::mutex> lg(m);

    try {

        clients.erase(csock);
        log->info("Exited from waiting messages from socket: " + std::to_string(csock) + " \n");
        log->flush();

        this->activeConnections--;

    }
    catch (...) {
        log->error("an error occured unregistring client socket " + std::to_string(csock));
    }

}

/*

Increment the number of socket active for the userName passed by arguments.

*/
bool Server::incrementNumSocket(std::string userName) {

    std::unique_lock <std::mutex> lg(mNumSock);

    try {

        int numberSocket;

        if (this->clientsNumberSocket.find(userName) == this->clientsNumberSocket.end())
            numberSocket = 0;
        else
            numberSocket = this->clientsNumberSocket[userName];

        if (numberSocket >= sc.numberUserActiveConnections)
            return false;

        if (this->clientsNumberSocket.find(userName) == this->clientsNumberSocket.end()) {

            this->clientsNumberSocket[userName] = 1;

        }
        else {

            this->clientsNumberSocket.find(userName)->second++;

        }

        return true;

    }
    catch (...) {

        log->error("an error occured incrementing num sock of user " + userName);
        return false;

    }

}

/*

Decrement the number of socket active for the userName passed by arguments.

*/
void Server::decrementNumSocket(std::string userName) {

    std::unique_lock <std::mutex> lg(mNumSock);

    try {

        this->clientsNumberSocket.find(userName)->second--;

    }
    catch (...) {
        log->error("an error occured decrementing num sock of user " + userName);
    }

}

/*

Check if the connection with the server IS CLOSED.

RETURN:

    true: connection IS CLOSED
    false: connection IS OPENED

*/
bool Server::isClosed(int sock) {

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

Convert the  std::string received into a message object.

RETURN:

0 -> no error
-1 -> unexpected error

*/
int Server::ClientConn::fromStringToUserServerConf(std::string msg, userServerConf& conf) {

    try {

        auto jsonMSG = json::parse(msg);

        jsonMSG.at("ip").get_to(conf.ip);
        jsonMSG.at("port").get_to(conf.port);

        msg.clear();

    }
    catch (...) {

        msg.clear();
        msg = "Error parsing  std::string received into UerServerConf Object";
        return -1;

    }

    return 0;

}