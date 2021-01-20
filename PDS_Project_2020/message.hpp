#include <string>
#ifndef Msg
#define Msg

struct message2 {

    std::string  type;
    int     typeCode;
    unsigned long long int timestamp;
    std::string  hash;
    long     packetNumber;
    std::string  folderPath;
    std::string  fileName;
    std::string userName;

    std::string  body;

};

///////////////////////////////////
///*** INITIAL CONFIGURATION ***///
///////////////////////////////////

struct initialConf {

    std::string path;
    std::string hash;

};

struct userServerConf {

    std::string ip;
    std::string port;

};
#endif

