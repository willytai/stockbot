#include "utils/utils.h"
#include <boost/algorithm/string.hpp>
#include <numeric>

namespace stockbot {

void utils::toUpper(std::string& str) 
{
    boost::to_upper(str);
}

void utils::toLower(std::string& str) 
{
    boost::to_lower(str);
}

void utils::strip(std::string& str)
{
    boost::trim(str);
}

void utils::split(const std::string& str, std::vector<std::string>& result, const std::string& delimiter)
{
    boost::split(result, boost::trim_copy(str), boost::is_any_of(delimiter));
}

double utils::parseAsPercentage(const std::string& str)
{
    std::string trimmed(boost::trim_copy(str));

    // remove the last % sign
    if (trimmed.back() == '%') {
        trimmed.pop_back();
    }

    // convert to double, will throw if got an illegal value
    return std::stod(trimmed) / 100.0;
}

std::string utils::join(const std::vector<std::string>& strs, const std::string& delimiter)
{
    return std::accumulate(
        strs.begin(),
        strs.end(),
        std::string(),
        [&delimiter](std::string acc, const std::string& val) {
            if (!acc.empty()) {
                acc += delimiter;
            }
            return acc + val;
        }
    );
}

}
