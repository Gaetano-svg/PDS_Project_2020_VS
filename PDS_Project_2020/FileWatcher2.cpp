
#include <iostream>
#include "FileWatcher2.hpp"
#include <set>

Client client2;

std::string getFileName2(const std::string& s) {

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

std::string getFilePath2(const std::string& s) {

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

bool fun2(info_backup_file i, std::atomic<bool>& b) {
    /*std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 5000));
    return true;*/
    std::string path = getFilePath2(i.file_path);
    std::string name = getFileName2(i.file_path);

    int operation;

    switch (i.status)
    {

    case FileStatus::created:
        operation = 1;
        break;

    case FileStatus::renamed:
        operation = 2;
        break;

    case FileStatus::erased:
        operation = 4;
        break;

    }

    int resCode = 0;

    // SEND MESSAGE TO SERVER

    std::cout << "NAME: " << name << " BOOL: " << b.load() << std::endl;

    // connection
    //CActiveSocket socketObj;
    int sock;
    client2.serverConnection(sock);

    if (i.status == FileStatus::renamed)
    {
        std::string rename = i.adding_info;

        /*if (client2.isClosed(sock))
            client2.serverConnection(sock);*/

        resCode = client2.sendToServer(sock, operation, path, name, rename, i.file_size, i.file_hash, 0, b);

        std::cout << "ERROR CODE RETURN: " << resCode << " PATH: " << path << " " << name << std::endl;

    }

    else {

        /*if (client2.isClosed(sock))
            client2.serverConnection(sock);*/

        resCode = client2.sendToServer(sock, operation, path, name, "", i.file_size, i.file_hash, 0, b);

        std::cout << "ERROR CODE RETURN: " << resCode << " PATH: " << path << " " << name << std::endl;

    }

    client2.serverDisconnection(sock);

    if (resCode < 0) {

        /*if (resCode == -10)
            return true;*/

        return false;
    }


    //std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 5000));
    return true;
}

FileWatcher2::FileWatcher2(Client client, std::chrono::duration<int, std::milli> delay) : delay{ delay } { client2 = client; }

FileWatcher2::FileWatcher2(Client client, std::string path_to_watch, std::chrono::duration<int, std::milli> delay) : path_to_watch{ path_to_watch }, delay{ delay } {

    // initialize the client object
    /*client2.readConfiguration();
    client2.initLogger();*/

    client2 = client;

    /*initialize_maps();

    print_backupMap();

    start_threads();*/

}

void FileWatcher2::start() {

    std::cout << paths_.size() << std::endl;

    std::cout << std::endl << std::endl << "STARTED" << std::endl;

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
                    while (j != paths_.end()) {
                        if (hash_to_check == j->second.file_hash &&
#ifdef _WIN32 
                            file.file_size() == j->second.file_size
#else 
                            fs::file_size(file) == j->second.file_size
#endif 
                            && !fs::exists(j->first)) {

                            //RENAME---------------------------------------------------------------------------------------------------------------------------------


                            if (thread_table.find(j->first) != thread_table.end()/*thread_table.contains(j->first)*/) { //se c'� un thread che sta lavorando quel file gli si dice di smetterla e poi BISOGNA NON AGGIORNARE IL FILEWATCHER PERCHE' AL GIRO SUCCESSIVO SI DEVE DI NUOVO ACCORGERE DI QUESTO EVENTO
                                thread_to_stop[j->first] = true;
                                break; // <- questo perch� il file watcher non deve accorgersi del nuovo file
                            }

                            if (elements_to_backup3.find(j->first) != elements_to_backup3.end() /*elements_to_backup3.contains(j->first)*/) {
                                if (elements_to_backup3[j->first].status == FileStatus::created) { //CREATED
                                    elements_to_backup3.erase(j->first);
                                    info_backup_file k;
                                    k.status = FileStatus::created;
                                    k.file_path = file_name;
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
                            else {
                                //looking for a chain of rename   ----- BUG= NON BISOGNA PERMETTERE AD UN FILE DI RINOMINARSI IN SE STESSO (RISOLTO)
                                bool flag_chain = false;
                                bool flag_same = false;
                                bool no_update = false;
                                auto it = elements_to_backup3.begin();
                                while (it != elements_to_backup3.end()) {
                                    if (it->second.status == FileStatus::renamed && it->second.adding_info == j->first && it->first != file_name) {

                                        if (thread_table.find(it->first) != thread_table.end()  /*thread_table.contains(it->first)*/) { //se c'� un thread che sta lavorando quel file gli si dice di smetterla
                                            thread_to_stop[it->first] = true;
                                            no_update = true;
                                            flag_chain = true;
                                            break;
                                        }
                                        else {
                                            flag_chain = true;
                                            info_backup_file k;
                                            k.status = FileStatus::renamed;
                                            k.file_path = it->first;
                                            k.file_hash = it->second.file_hash;
                                            k.file_last_write = it->second.file_last_write;
                                            k.file_size = it->second.file_size;
                                            k.adding_info = file.path().string();
                                            elements_to_backup3[it->first] = k;
                                            break;
                                        }


                                    }
                                    else if (it->second.status == FileStatus::renamed && it->second.adding_info == j->first && it->first == file_name) {

                                        if (thread_table.find(it->first) != thread_table.end() /*thread_table.contains(it->first)*/) { //se c'� un thread che sta lavorando quel file gli si dice di smetterla
                                            thread_to_stop[it->first] = true;
                                            no_update = true;
                                            flag_same = true;
                                            break;
                                        }
                                        else {

                                            flag_same = true;
                                            elements_to_backup3.erase(it->first);
                                            break;
                                        }
                                    }
                                    it++;
                                }

                                if (no_update == true)
                                    break;




                                if (!flag_chain && !flag_same) {
                                    //aggiungo rename 



                                    elements_to_backup3.erase(j->first);
                                    info_backup_file k;
                                    k.status = FileStatus::renamed;
                                    k.file_path = j->first;
                                    k.file_hash = j->second.file_hash;
                                    k.file_last_write = j->second.file_last_write;
                                    k.file_size = j->second.file_size;
                                    k.adding_info = file.path().string();
                                    elements_to_backup3[j->first] = k;
                                }
                            }
                            paths_[file_name].file_last_write = current_file_last_write_time;
                            paths_[file_name].file_size = j->second.file_size;
                            paths_[file_name].file_hash = j->second.file_hash;
                            paths_.erase(j);
                            count--;
                            break;

                        }
                        j++;
                        count++;


                    }
                    if (count == paths_.size()) {
                        count = 0;

                        //CREATION-----------------------------------------------------------------------------------------

                        if (thread_table.find(file_name) != thread_table.end() /*thread_table.contains(file_name)*/) { //se c'� un thread che sta lavorando quel file gli si dice di smetterla
                            thread_to_stop[file_name] = true;
                        }
                        else if (elements_to_backup3.find(file_name) != elements_to_backup3.end() /*elements_to_backup3.contains(file_name)*/ && elements_to_backup3[file_name].status == FileStatus::renamed) {
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

                            if (elements_to_backup3.find(file_name) != elements_to_backup3.end()  /*elements_to_backup3.contains(file_name)*/ && elements_to_backup3[file_name].status == FileStatus::erased) {
                                info_backup_file k;
                                k.status = FileStatus::created;
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
                                info_backup_file k;
                                k.status = FileStatus::created;
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
                // MODIFICATION-------------------------------------------------------------------------------------------
                else {
                    
                    if (thread_table.find(file_name) != thread_table.end()  /*thread_table.contains(file_name)*/ && (paths_[file.path().string()].file_last_write < current_file_last_write_time)) { //se c'� un thread che sta lavorando quel file gli si dice di smetterla
                        thread_to_stop[file_name] = true;
                    }
                    else {

                        if (paths_[file.path().string()].file_last_write < current_file_last_write_time || paths_[file.path().string()].checkHash) {
                            std::string  hash_to_add = this->compute_hash(file_name);

                            // if we have to check the file hash and it is the same on server side, we'll continue checking
                            if (paths_[file.path().string()].checkHash && hash_to_add == paths_[file.path().string()].file_hash)
                                continue;

                            std::cout << "continued" << std::endl;

                            paths_[file.path().string()].file_last_write = current_file_last_write_time;
                            paths_[file.path().string()].file_hash = hash_to_add;

#ifdef _WIN32 
                            paths_[file.path().string()].file_size = file.file_size();
#else 
                            paths_[file.path().string()].file_size = fs::file_size(file);
#endif
                            //paths_[file.path().string()].file_size = file.file_size();

                            if (elements_to_backup3.find(file_name) != elements_to_backup3.end() /*elements_to_backup3.contains(file_name)*/ && elements_to_backup3[file_name].status == FileStatus::created) {
                                elements_to_backup3[file_name].file_hash = hash_to_add;
                                elements_to_backup3[file_name].file_last_write = current_file_last_write_time;

#ifdef _WIN32 
                                elements_to_backup3[file_name].file_size = file.file_size();
#else 
                                elements_to_backup3[file_name].file_size = fs::file_size(file);
#endif

                                //elements_to_backup3[file_name].file_size = file.file_size();
                            }
                            else if (elements_to_backup3.find(file_name) == elements_to_backup3.end() /*!elements_to_backup3.contains(file_name)*/) {
                                info_backup_file k;
                                k.status = FileStatus::created;
                                k.file_path = file_name;
                                k.file_hash = hash_to_add;
                                k.file_last_write = current_file_last_write_time;

#ifdef _WIN32 
                                k.file_size = file.file_size();
#else 
                                k.file_size = fs::file_size(file);
#endif

                                elements_to_backup3[file_name] = k;
                            }
                            else {
                                std::cout << "\n\nMODIFICATION AFTER DELETE OR RENAME --> ERROR\n\n";
                                exit(666);
                            }
                        }
                    }
                }
            }

        }

        //ERASED ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
        auto it = paths_.begin();
        while (it != paths_.end()) {
            if (!fs::exists(it->first)) {

                if (thread_table.find(it->first) != thread_table.end() /*thread_table.contains(it->first)*/) { //se c'� un thread che sta lavorando quel file gli si dice di smetterla
                    thread_to_stop[it->first] = true;
                    it++;

                }
                else {

                    if (elements_to_backup3.find(it->first) != elements_to_backup3.end() /*elements_to_backup3.contains(it->first)*/ && elements_to_backup3[it->first].status == FileStatus::created) {
                        elements_to_backup3.erase(it->first);
                        it = paths_.erase(it);
                    }
                    else if (elements_to_backup3.find(it->first) == elements_to_backup3.end() /*!elements_to_backup3.contains(it->first)*/) {
                        bool b = false;
                        auto iter = elements_to_backup3.begin();
                        while (iter != elements_to_backup3.end()) {
                            if (iter->second.adding_info == it->first && iter->second.status == FileStatus::renamed) {
                                if (thread_table.find(iter->first) != thread_table.end()  /*thread_table.contains(iter->first)*/) { //se c'� un thread che sta lavorando quel file gli si dice di smetterla
                                    thread_to_stop[iter->first] = true;
                                    b = true;
                                    it++;
                                    break;
                                }
                                info_backup_file k;
                                k.status = FileStatus::erased;
                                k.file_path = iter->first;
                                elements_to_backup3[iter->first] = k;
                                b = true;
                                it = paths_.erase(it);
                                break;
                            }
                            iter++;
                        }
                        if (!b) {
                            info_backup_file k;
                            k.status = FileStatus::erased;
                            k.file_path = it->first;
                            elements_to_backup3[it->first] = k;
                            it = paths_.erase(it);
                        }

                    }
                    else {
                        std::cout << "\n\DELETE AFTER DELETE OR RENAME --> ERROR\n\n";
                        exit(666);
                    }

                }
            }
            else {
                it++;
            }
        }

        try {

            print_backupMap();

            update_threads();

        }
        catch (...) {
            std::cout << "erroreeee" << std::endl;
        }

    }
}

bool FileWatcher2::contains(const std::string& key) {
    auto el = paths_.find(key);
    return el != paths_.end();
}

std::string FileWatcher2::compute_hash(const std::string file_path)
{
    std::string result;
    CryptoPP::Weak::MD5 hash;
    CryptoPP::FileSource(file_path.c_str(), true, new
        CryptoPP::HashFilter(hash, new CryptoPP::HexEncoder(new
            CryptoPP::StringSink(result), false)));
    return result;
}

void FileWatcher2::initialize_maps() {

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
            info_backup_file k;
            k.status = FileStatus::created;
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

void FileWatcher2::start_threads() {

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

void FileWatcher2::update_threads() {

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
            
            // reset boolean check after table update

            if (paths_.find(thr->first) != paths_.end())
                paths_[thr->first].checkHash = false;

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

void FileWatcher2::print_backupMap() {
    auto k = elements_to_backup3.begin();
    while (k != elements_to_backup3.end()) {
        if (k == elements_to_backup3.begin()) {
            std::cout << "\n";
        }
        switch (k->second.status) {
        case FileStatus::erased:
            std::cout << k->first << "  ------>  erased\n";
            break;
        case FileStatus::created:
            std::cout << k->first << "  ------>  created\n";
            break;
        case FileStatus::renamed:
            std::cout << k->first << "  ------>  renamed in " << k->second.adding_info << std::endl;
            break;
        }
        k++;
    }

}
