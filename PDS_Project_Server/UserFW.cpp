
#include <iostream>
#include "UserFW.hpp"
#include <set>

Client client3;

std::atomic<int> numberOfFileSent;
std::atomic<int> numberOfFileToCheck;



std::string getFileName3(const std::string& s) {

#ifdef _WIN32
    char sep = '\\';
#else
    char sep = '\/';
#endif

    size_t i = s.rfind(sep, s.length());
    if (i != std::string::npos) {
        return(s.substr(i + 1, s.length()));
    }

    return("");
};

std::string getFilePath3(const std::string& s) {

#ifdef _WIN32
    char sep = '\\';
#else
    char sep = '\/';
#endif

    size_t i = s.rfind(sep, s.length());
    if (i != std::string::npos) {
        return(s.substr(0, i));
    }

    return("");
};

bool fun2(info_backup_file_user i, std::atomic<bool>& b) {

    std::string path = getFilePath3(i.file_path);
    std::string name = getFileName3(i.file_path);

    int operation;



    switch (i.status)
    {

    case FileStatusUser::created:
        operation = 1;
        break;

    case FileStatusUser::renamed:
        operation = 2;
        break;

    case FileStatusUser::erased:
        operation = 4;
        break;

    }

    int resCode = 0;

    // SEND MESSAGE TO SERVER

    std::cout << "NAME: " << name << " BOOL: " << b.load() << std::endl;

    // connection
    //CActiveSocket socketObj;
    int sock;
    client3.serverConnection(sock);


    if (i.status == FileStatusUser::renamed)
    {
        std::string rename = getFileName3(i.adding_info);

        std::string oldPath = path;
        std::string newPath = getFilePath3(i.adding_info);

        // folder rename happened
        if (oldPath != newPath) {

            // first delete the old file
            resCode = client3.sendToServer(sock, 4, path, name, "", i.file_size, i.file_hash, 0, b);

            std::cout << "ERROR CODE RETURN FROM DELETE: " << resCode << " PATH: " << path << " " << name << std::endl;

            if (resCode < 0) {

                client3.serverDisconnection(sock);
                return false;

            }

            // then create a new file
            resCode = client3.sendToServer(sock, 1, newPath, rename, "", i.file_size, i.file_hash, 0, b);

            std::cout << "ERROR CODE RETURN FROM CREATE: " << resCode << " PATH: " << path << " " << name << std::endl;

            if (resCode < 0) {

                client3.serverDisconnection(sock);
                return false;

            }

        }
        else {

            resCode = client3.sendToServer(sock, operation, path, name, rename, i.file_size, i.file_hash, 0, b);

            std::cout << "ERROR CODE RETURN: " << resCode << " PATH: " << path << " " << name << std::endl;

        }

        /*if (client2.isClosed(sock))
            client2.serverConnection(sock);*/

        resCode = client3.sendToServer(sock, operation, path, name, rename, i.file_size, i.file_hash, 0, b);

        std::cout << "ERROR CODE RETURN: " << resCode << " PATH: " << path << " " << name << std::endl;

    }

    else {

        /*if (client2.isClosed(sock))
            client2.serverConnection(sock);*/

        resCode = client3.sendToServer(sock, operation, path, name, "", i.file_size, i.file_hash, 0, b);

        std::cout << "ERROR CODE RETURN: " << resCode << " PATH: " << path << " " << name << std::endl;

    }

    client3.serverDisconnection(sock);

    if (resCode < 0) {

        /*if (resCode == -10)
            return true;*/

        return false;
    }
    else {
        numberOfFileSent++;
    }


    //std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 5000));
    return true;
}

UserFW::UserFW(std::string path_to_watch, std::chrono::duration<int, std::milli> delay, std::string userName, std::string user_server_IP, std::string user_server_PORT, int secondTimeout) : path_to_watch{ path_to_watch }, delay{ delay }{

    // initialize atomic variables
    numberOfFileSent.store(0);
    numberOfFileToCheck.store(0);

    // initialize the client object
    client3.isServerSide = true;
    client3.readConfiguration(userName, user_server_IP, user_server_PORT, path_to_watch, secondTimeout);
    client3.initLogger();
    for (auto& file : fs::recursive_directory_iterator(path_to_watch))
        if (fs::is_regular_file(file.path().string()))
            numberOfFileToCheck++;

    initialize_maps();

    print_backupMap();

    start_threads();

}

void UserFW::start() {
    while (running_) {

        // Wait for "delay" milliseconds


        std::this_thread::sleep_for(delay);



        // Check if a file was created or renamed
        for (auto& file : fs::recursive_directory_iterator(path_to_watch)) {
            if (fs::is_regular_file(file.path().string())) {
                auto current_file_last_write_time = last_write_time(file);
                std::string file_name = file.path().string();
                // File creation / rename
                if (!contains(file_name)) {
                    //vedi se l'hash coincide con un file in path_
                    std::string hash_to_check = this->compute_hash(file.path().string());
                    auto j = paths_.begin();
                    int count = 0;
                    if (count == paths_.size()) {
                        count = 0;

                        //CREATION-----------------------------------------------------------------------------------------

                        if (thread_table.find(file_name) != thread_table.end() /*thread_table.contains(file_name)*/) { //se c'� un thread che sta lavorando quel file gli si dice di smetterla
                            thread_to_stop[file_name] = true;
                        }
                        else if (elements_to_backup3.find(file_name) != elements_to_backup3.end() /*elements_to_backup3.contains(file_name)*/ && elements_to_backup3[file_name].status == FileStatusUser::renamed) {
                            //nulla perch� � necessario che prima venga processata la rename, la new viene ignorata.
                        }
                        else {

                            paths_[file.path().string()].file_last_write = current_file_last_write_time;

#ifdef _WIN32 
                            paths_[file.path().string()].file_size = file.file_size();
#else 
                            paths_[file.path().string()].file_size = fs::file_size(file);
#endif

                            //paths_[file.path().string()].file_size = file.file_size();
                            paths_[file.path().string()].file_hash = hash_to_check;

                            if (elements_to_backup3.find(file_name) != elements_to_backup3.end()  /*elements_to_backup3.contains(file_name)*/ && elements_to_backup3[file_name].status == FileStatusUser::erased) {
                                info_backup_file_user k;
                                k.status = FileStatusUser::created;
                                k.file_path = file.path().string();
                                k.file_hash = hash_to_check;
                                k.file_last_write = current_file_last_write_time;

#ifdef _WIN32 
                                k.file_size = file.file_size();
#else 
                                k.file_size = fs::file_size(file);
#endif

                                elements_to_backup3[file_name] = k;
                            }
                            else {
                                info_backup_file_user k;
                                k.status = FileStatusUser::created;
                                k.file_path = file.path().string();
                                k.file_hash = hash_to_check;
                                k.file_last_write = current_file_last_write_time;

#ifdef _WIN32 
                                k.file_size = file.file_size();
#else 
                                k.file_size = fs::file_size(file);
#endif

                                elements_to_backup3[file_name] = k;
                            }
                        }
                    }


                }
            }

        }

        if (numberOfFileSent == numberOfFileToCheck) {
            std::cout << std::endl << std::endl << std::endl << "INVIATO TUTTO" << std::endl;
            running_ = false;

            int sock;
            client3.serverConnection(sock);
            std::atomic_bool b(false);
            client3.sendToServer(sock, 8, "", "", "", 0, "", 0, b);
            client3.serverDisconnection(sock);
            return;
        }
        else {

            print_backupMap();

            update_threads();
        }

    }
}

bool UserFW::contains(const std::string& key) {
    auto el = paths_.find(key);
    return el != paths_.end();
}

std::string UserFW::compute_hash(const std::string file_path)
{
    std::string result;
    CryptoPP::Weak::MD5 hash;
    CryptoPP::FileSource(file_path.c_str(), true, new
        CryptoPP::HashFilter(hash, new CryptoPP::HexEncoder(new
            CryptoPP::StringSink(result), false)));
    return result;
}

void UserFW::initialize_maps() {

    //all'inizio del programma inizializza le mappe paths_ ed elements_to_backup3 calcolando anche tutti gli hash ----- INSERIRE CONTROLLO ECCEZIONI

    for (auto& file : fs::recursive_directory_iterator(path_to_watch)) {
        if (fs::is_regular_file(file.path().string())) {
            auto current_file_last_write_time = fs::last_write_time(file);
            paths_[file.path().string()].file_last_write = current_file_last_write_time;

#ifdef _WIN32 
            paths_[file.path().string()].file_size = file.file_size();
#else 
            paths_[file.path().string()].file_size = fs::file_size(file);
#endif

            //paths_[file.path().string()].file_size = file.file_size();
            paths_[file.path().string()].file_hash = this->compute_hash(file.path().string());
            info_backup_file_user k;
            k.status = FileStatusUser::created;
            k.file_path = file.path().string();
            k.file_hash = this->compute_hash(file.path().string());
            k.file_last_write = current_file_last_write_time;

#ifdef _WIN32 
            k.file_size = file.file_size();
#else 
            k.file_size = fs::file_size(file);
#endif

            elements_to_backup3[file.path().string()] = k;
        }
    }
}

void UserFW::start_threads() {

    //inizializzazione della tabella di thread e della tabella di booleani.
    //vengono creati tot thread a cui si passa la funzione da svolgere , il file e le sue info e il booleano che indica se fermarsi ----- INSERIRE CONTROLLO ECCEZIONI

    for (auto file = elements_to_backup3.begin(); file != elements_to_backup3.end(); ++file) {
        if (thread_table.size() < std::thread::hardware_concurrency()) {
            thread_to_stop[file->first].store(false);
            thread_table.insert(std::make_pair(file->first, std::async(fun2, elements_to_backup3[file->first], std::ref(thread_to_stop[file->first]))));
        }
        else {
            break;
        }
    }
}
/*
void FileWatcher2::update_threads() {

    //AGGIORNAMENTO DELLA THREAD TABLE
    // (1) Se un task � terminato aggiorno  la tabella dei thread, la tabella booleani ed eventualmente elements_to_backup3

    for (auto thr = thread_table.begin(), next_thr = thr; thr != thread_table.end(); thr = next_thr) {
        ++next_thr;
        if (thr->second.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            if (thr->second.get() == true)
                elements_to_backup3.erase(thr->first);
            thread_to_stop.erase(thr->first); //possibile problema: se distruggo il booleano qua ma il thread che lo modifica deve ancora terminare?
            thread_table.erase(thr);
        }
    }

    // (2) Faccio partire nuovi thread se possibile

    for (auto file = elements_to_backup3.begin(); file != elements_to_backup3.end(); ++file) {
        if (thread_table.size() < std::thread::hardware_concurrency()) {
            if (thread_table.find(file->first) == thread_table.end() /* !thread_table.contains(file->first)*//*) {
                thread_to_stop[file->first].store(false);
                thread_table.insert(std::make_pair(file->first, std::async(fun2, elements_to_backup3[file->first], std::ref(thread_to_stop[file->first]))));
            }
        }
        else {
            break;
        }
    }
}*/

void UserFW::update_threads() {

    std::set<std::string> to_skip;

    //AGGIORNAMENTO DELLA THREAD TABLE
    // (1) Se un task � terminato aggiorno  la tabella dei thread, la tabella booleani ed eventualmente elements_to_backup3

    for (auto thr = thread_table.begin(), next_thr = thr; thr != thread_table.end(); thr = next_thr) {
        ++next_thr;
        if (thr->second.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            if (thr->second.get() == true)
                elements_to_backup3.erase(thr->first);
            else
                to_skip.insert(thr->first);
            thread_to_stop.erase(thr->first); //possibile problema: se distruggo il booleano qua ma il thread che lo modifica deve ancora terminare?
            thread_table.erase(thr);
        }
    }

    // (2) Faccio partire nuovi thread se possibile

    for (auto file = elements_to_backup3.begin(); file != elements_to_backup3.end(); ++file) {
        if (thread_table.size() < std::thread::hardware_concurrency()) {
            if (thread_table.find(file->first) == thread_table.end() /* !thread_table.contains(file->first)*/ && to_skip.find(file->first) == to_skip.end()) {
                thread_to_stop[file->first] = false;
                thread_table.insert(std::make_pair(file->first, std::async(fun2, elements_to_backup3[file->first], std::ref(thread_to_stop[file->first]))));
            }
        }
        else {
            break;
        }
    }
}

void UserFW::print_backupMap() {
    auto k = elements_to_backup3.begin();
    while (k != elements_to_backup3.end()) {
        if (k == elements_to_backup3.begin()) {
            std::cout << "\n";
        }
        switch (k->second.status) {
        case FileStatusUser::erased:
            std::cout << k->first << "  ------>  erased\n";
            break;
        case FileStatusUser::created:
            std::cout << k->first << "  ------>  created\n";
            break;
        case FileStatusUser::renamed:
            std::cout << k->first << "  ------>  renamed in " << k->second.adding_info << std::endl;
            break;
        }
        k++;
    }

}
