#ifndef __AUTO_INVESTMENT__
#define __AUTO_INVESTMENT__

#include "schwabcpp/schema/registrationHelper.h"
#include "schwabcpp/utils/clock.h"

namespace stockbot {

using json = nlohmann::json;
using clock = schwabcpp::clock;

struct AutoInvestment {

    std::string   ticker;

    enum {
        Daily,
        Weekly,
    }               frequency;

    int             shares;   // # of shares to buy
    int             extras;   // # of extra shares to add when the stock is down a tigger threshold and rebounced from previous low

    double          averageInThreshold;     // the threshold (in percentage) to average in extra shares when the stock is down
    double          skipThreshold;          // the threshold (in percentage) to skip the purchase when the stock is up

    clock::rep      createdTime;
    clock::rep      lastTriggerTime;

    int             accumulatedShares;
    double          accumulatedValue;

static void to_json(json& j, const AutoInvestment& self);
static void from_json(const json& j, AutoInvestment& data);
};

}

REGISTER_TO_JSON(stockbot::AutoInvestment);

#endif // !__AUTO_INVESTMENT__
