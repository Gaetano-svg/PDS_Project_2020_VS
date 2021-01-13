#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1

#include <string>
#include <chrono>
#include <filesystem>
#include <unordered_map>
#include <thread>
#include <future>
#include <functional>

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
#include <experimental/filesystem>
#endif

#ifdef _WIN32
namespace fs = std::filesystem;
#include "../PDS_Project_2020/client.hpp"
#else
namespace fs = std::experimental::filesystem;
#include "./client.hpp"
#endif


enum class FileStatusUser { created, erased, renamed };

struct infoUser {
#ifdef _WIN32
    std::filesystem::file_time_type file_last_write;
#else
    std::experimental::filesystem::file_time_type file_last_write;
#endif
    std::uintmax_t file_size;
    std::string file_hash;
};

struct info_backup_file_user {
    FileStatusUser status;
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



class UserFW {

private:

    std::unordered_map<std::string, infoUser> paths_;
    std::unordered_map<std::string, info_backup_file_user> elements_to_backup3;
    std::unordered_map<std::string, std::future<bool>> thread_table;
    std::unordered_map<std::string, std::atomic<bool>> thread_to_stop;

    bool running_ = true;

    void initialize_maps();
    void start_threads();
    void update_threads();
    void print_backupMap();


public:
    std::string path_to_watch;
    std::chrono::duration<int, std::milli> delay;

    UserFW(std::string path_to_watch, std::chrono::duration<int, std::milli> delay, std::string userName, std::string user_server_IP, std::string user_server_PORT, int secondTimeout);
    void start();
    bool contains(const std::string& key);
    std::string compute_hash(const std::string file_path);

};
