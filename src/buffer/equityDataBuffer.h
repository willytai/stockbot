#ifndef __EQUITY_DATA_BUFFER__
#define __EQUITY_DATA_BUFFER__

#include <string>
#include <mutex>
#include <unordered_map>
#include "schwabcpp/streamerField.h"

namespace stockbot {

class EquityDataBuffer {
public:
                                    EquityDataBuffer(const std::string& symbol);
                                    ~EquityDataBuffer();

    void                            addLevelOneData(const std::unordered_map<schwabcpp::StreamerField::LevelOneEquity, double>& fields);

    [[nodiscard]]
    std::lock_guard<std::mutex>     lockForAccess() { return std::lock_guard(m_mutex); }


    // -- getters (these are not thread safe, lock the buffer before calling them)
    double                          getLastPrice() const { return m_lastPrice; }

    double                          getLOD() const { return m_lod; }

    double                          getHOD() const { return m_hod; }

    double                          getNetPercentChange() const { return m_netPercentChange; }

private:
    std::string                     m_symbol;

    // -- directly from stream data, needs mutex
    double                          m_lod;
    double                          m_hod;
    double                          m_netPercentChange;
    double                          m_lastPrice;
    std::mutex                      m_mutex;
};

} // namespace stockbot

#endif
