#ifndef __COMMAND_H__
#define __COMMAND_H__

#include <string>
#include "dpp/dispatcher.h"

namespace stockbot {

#define REGISTER_COMMAND(ClassName, NameStr) \
    struct ClassName { \
        ClassName() = delete; \
        ~ClassName() = delete; \
        static std::string Name() { return NameStr; } \
    };

    namespace command {

        REGISTER_COMMAND(Ping, "ping");

        REGISTER_COMMAND(Kill, "kill");
        REGISTER_COMMAND(Authorize, "authorize");

        REGISTER_COMMAND(AllAccountInfo, "all_account_info");
        REGISTER_COMMAND(SetupRecurringInvestment, "setup_recurring_investment");

    }

class SlashCommandDispatcher
{
    using eventFn = std::function<void(const dpp::slashcommand_t&)>;
public:
    SlashCommandDispatcher(const dpp::slashcommand_t& event) : _event(event) {}

    template <typename T>
    void dispatch(eventFn fn) {
        if (_event.command.get_command_name() == T::Name()) {
            fn(_event);
        }
    }

private:
    const dpp::slashcommand_t& _event;
};

}

#endif
