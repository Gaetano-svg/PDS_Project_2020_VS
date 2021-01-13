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
#include "./CryptoPP/cryptlib.h"
#include "./CryptoPP/md5.h"
#include "./CryptoPP/files.h"
#include "./CryptoPP/hex.h"
#include "client.hpp"

enum class FileStatus { created, erased, renamed };

struct info {
    std::filesystem::file_time_type file_last_write;
    std::uintmax_t file_size;
    std::string file_hash;
};

struct info_backup_file {
    FileStatus status;
    std::string file_path;
    std::filesystem::file_time_type file_last_write;
    std::uintmax_t file_size;
    std::string file_hash;
    std::string adding_info;
};




class FileWatcher {

private:

    std::unordered_map<std::string, info> paths_;
    std::unordered_map<std::string, info_backup_file> elements_to_backup3;
    std::unordered_map<std::string, std::future<bool>> thread_table;
    std::unordered_map<std::string, std::atomic<bool>> thread_to_stop;

    bool running_ = true;



public:
    std::string path_to_watch;
    std::chrono::duration<int, std::milli> delay;

    FileWatcher(std::string path_to_watch, std::chrono::duration<int, std::milli> delay);
    void start(const std::function<void(std::string, FileStatus)>& action);
    bool contains(const std::string& key);
    std::string compute_hash(const std::string file_path);

};


#endif //PROVA_FILEWATCHER_H