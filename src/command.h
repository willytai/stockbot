#ifndef __COMMAND_H__
#define __COMMAND_H__

#include <string>

namespace stockbot {

class Command
{
public:
    Command() = delete;
    ~Command() = delete;


    inline static const std::string PING = "ping";
    inline static const std::string KILL = "kill";
    inline static const std::string ALL_ACCOUNT_INFO = "all_account_info";
};

}

#endif
