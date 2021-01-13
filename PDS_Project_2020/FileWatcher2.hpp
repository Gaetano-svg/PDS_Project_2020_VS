#ifndef PROVA_FILEWATCHER_H
#define PROVA_FILEWATCHER_H
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1

#include <string>
#include <chrono>
#include <filesystem>
#include <unordered_map>
#include <thread>
#include <future>
#include <functional>

#ifdef _WIN32
#include "./CryptoPP/cryptlib.h"
#include "./CryptoPP/md5.h"
#include "./CryptoPP/files.h"
#include "./CryptoPP/hex.h"
#else
#include <cryptopp/cryptlib.h>
#include <cryptopp/md5.h>
#include <cryptopp/files.h>
#include <cryptopp/hex.h>
#include <experimental/filesystem>
#endif

#ifdef _WIN32
namespace fs = std::filesystem;
#else
namespace fs = std::experimental::filesystem;
#endif

#include "client.hpp"

enum class FileStatus { created, erased, renamed };

struct info {
#ifdef _WIN32
    std::filesystem::file_time_type file_last_write;
#else
    std::experimental::filesystem::file_time_type file_last_write;
#endif
    std::uintmax_t file_size;
    std::string file_hash;
};

struct info_backup_file {
    FileStatus status;
    std::string file_path;
#ifdef _WIN32
    std::filesystem::file_time_type file_last_write;
#else
    std::experimental::filesystem::file_time_type file_last_write;
#endif
    std::uintmax_t file_size;
    std::string file_hash;
    std::string adding_info;
};



class FileWatcher2 {

private:


    std::unordered_map<std::string, info_backup_file> elements_to_backup3;
    std::unordered_map<std::string, std::future<bool>> thread_table;
    std::unordered_map<std::string, std::atomic<bool>> thread_to_stop;

    bool running_ = true;

    void initialize_maps();
    void start_threads();
    void update_threads();
    void print_backupMap();


public:

    std::unordered_map<std::string, info> paths_;
    std::string path_to_watch;
    std::chrono::duration<int, std::milli> delay;

    FileWatcher2(Client client, std::string path_to_watch, std::chrono::duration<int, std::milli> delay);
    FileWatcher2(Client client, std::chrono::duration<int, std::milli> delay);
    void start();
    bool contains(const std::string& key);
    std::string compute_hash(const std::string file_path);

};


#endif //PROVA_FILEWATCHER_H