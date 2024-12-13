#include "equityDataBuffer.h"

namespace stockbot {

EquityDataBuffer::EquityDataBuffer(const std::string& symbol)
    : m_symbol(symbol)
    , m_lod(std::numeric_limits<double>::quiet_NaN())
    , m_hod(std::numeric_limits<double>::quiet_NaN())
    , m_netPercentChange(std::numeric_limits<double>::quiet_NaN())
    , m_lastPrice(std::numeric_limits<double>::quiet_NaN())
{

}

EquityDataBuffer::~EquityDataBuffer()
{

}

void EquityDataBuffer::addLevelOneData(const std::unordered_map<schwabcpp::StreamerField::LevelOneEquity, double>& fields)
{
    // this entire function should be protected
    std::lock_guard lock(m_mutex);

    for (const auto& [field, value] : fields) {
        switch (field) {

            case schwabcpp::StreamerField::LevelOneEquity::LowPrice:
            {
                m_lod = value;
                break;
            }

            case schwabcpp::StreamerField::LevelOneEquity::HighPrice:
            {
                m_hod = value;
                break;
            }

            case schwabcpp::StreamerField::LevelOneEquity::LastPrice:
            {
                m_lastPrice = value;
                break;
            }

            case schwabcpp::StreamerField::LevelOneEquity::NetPercentChange:
            {
                m_netPercentChange = value;
                break;
            }

            default: break;
        }
    }
}

}
