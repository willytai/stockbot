#include "autoInvestment.h"

namespace stockbot {

std::string AutoInvestment::freq_to_string(AutoInvestment::Frequency freq)
{
    switch (freq) {
        case AutoInvestment::Daily:  return "Daily";
        case AutoInvestment::Weekly: return "Weekly";

        case AutoInvestment::Unknown: return "Unknown";
    }
}

AutoInvestment::Frequency AutoInvestment::string_to_frequency(const std::string& str)
{
    AutoInvestment::Frequency result;

    if (str == "Daily") {
        result = AutoInvestment::Daily;
    } else if (str == "Weekly") {
        result = AutoInvestment::Weekly;
    } else {
        result = AutoInvestment::Unknown;
    }

    return result;
}

// void AutoInvestment::to_json(json& j, const AutoInvestment& self)
// {
//     j = json {
//         {"ticker", self.ticker},
//         {"frequency", freq_to_string(self.frequency)},
//         {"shares", self.shares},
//         {"extras", self.extras},
//         {"average_in_threshold", self.averageInThreshold},
//         {"skip_threshold", self.skipThreshold},
//         {"created_time", self.createdTime},
//         {"last_trigger_time", self.lastTriggerTime},
//         {"accumulated_shares", self.accumulatedShares},
//         {"accumulated_value", self.accumulatedValue},
//     };
// }
//
// void AutoInvestment::from_json(const json& j, AutoInvestment& data)
// {
//     j.at("ticker").get_to(data.ticker);
//     j.at("frequency").get_to(data.frequency);
//     j.at("shares").get_to(data.shares);
//     j.at("extras").get_to(data.extras);
//     j.at("average_in_threshold").get_to(data.averageInThreshold);
//     j.at("skip_threshold").get_to(data.skipThreshold);
//     j.at("created_time").get_to(data.createdTime);
//     j.at("last_trigger_time").get_to(data.lastTriggerTime);
//     j.at("accumulated_shares").get_to(data.accumulatedShares);
//     j.at("accumulated_value").get_to(data.accumulatedValue);
// }

}
