#ifndef __UTILS_H__
#define __UTILS_H__

#include <string>

namespace stockbot {

namespace utils {

void toUpper(std::string& str);
void toLower(std::string& str);

void strip(std::string& str);

// strips white spaces and splits
void split(const std::string& str, std::vector<std::string>& result, const std::string& delimiter = " ");

// parse <number[%]> into the value <number/100>
// the '%' sign is optional
double parseAsPercentage(const std::string& str);

std::string join(const std::vector<std::string>& strs, const std::string& delimiter);

}

} // namespace stockbot

#endif // !__UTILS_H__
