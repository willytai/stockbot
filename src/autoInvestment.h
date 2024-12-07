#ifndef __AUTO_INVESTMENT__
#define __AUTO_INVESTMENT__

#include "schwabcpp/schema/registrationHelper.h"
#include "schwabcpp/utils/clock.h"

namespace stockbot {

using json = nlohmann::json;
using clock = schwabcpp::clock;

struct AutoInvestment {

    std::string   id;
    std::string   ticker;

    std::vector<std::string>
                  accounts;

    enum Frequency {
        Daily,
        Weekly,

        Unknown,
    }               frequency;

    int             shares;   // # of shares to buy
    int             extras;   // # of extra shares to add when the stock is down a tigger threshold and rebounced from previous low

    double          averageInThreshold;     // the threshold to average in extra shares when the stock is down
    double          skipThreshold;          // the threshold to skip the purchase when the stock is up

    clock::rep      createdTime;
    clock::rep      lastTriggerTime;

    int             accumulatedShares = 0;
    double          accumulatedValue = 0.0;

static void to_json(json& j, const AutoInvestment& self);
static void from_json(const json& j, AutoInvestment& data);

private:
    static std::string freq_to_string(Frequency freq);
    static Frequency string_to_frequency(const std::string& str);
};

// linker is having a seizure, it doesn't link when I put this in the cpp file (wtf...)
inline void AutoInvestment::to_json(json& j, const AutoInvestment& self)
{
    j = json {
        {"id", self.id},
        {"ticker", self.ticker},
        {"accounts", self.accounts},
        {"frequency", freq_to_string(self.frequency)},
        {"shares", self.shares},
        {"extras", self.extras},
        {"average_in_threshold", self.averageInThreshold},
        {"skip_threshold", self.skipThreshold},
        {"created_time", self.createdTime},
        {"last_trigger_time", self.lastTriggerTime},
        {"accumulated_shares", self.accumulatedShares},
        {"accumulated_value", self.accumulatedValue},
    };
}

inline void AutoInvestment::from_json(const json& j, AutoInvestment& data)
{
    j.at("id").get_to(data.id);
    j.at("ticker").get_to(data.ticker);
    j.at("accounts").get_to(data.accounts);

    std::string freqStr;
    j.at("frequency").get_to(freqStr);
    data.frequency = string_to_frequency(freqStr);

    j.at("shares").get_to(data.shares);
    j.at("extras").get_to(data.extras);
    j.at("average_in_threshold").get_to(data.averageInThreshold);
    j.at("skip_threshold").get_to(data.skipThreshold);
    j.at("created_time").get_to(data.createdTime);
    j.at("last_trigger_time").get_to(data.lastTriggerTime);
    j.at("accumulated_shares").get_to(data.accumulatedShares);
    j.at("accumulated_value").get_to(data.accumulatedValue);
}

}

REGISTER_TO_JSON(stockbot::AutoInvestment);

#endif // !__AUTO_INVESTMENT__
