#include "streamDataBuffer.h"
#include "equityDataBuffer.h"

namespace stockbot {

StreamDataBuffer::StreamDataBuffer()
{

}

StreamDataBuffer::~StreamDataBuffer()
{

}

std::weak_ptr<EquityDataBuffer>
StreamDataBuffer::addLevelOneEquityData(const std::string& ticker,
                                        const std::unordered_map<schwabcpp::StreamerField::LevelOneEquity, double>& fields)
{
    std::shared_ptr<EquityDataBuffer> targetBuffer;
    {
        std::lock_guard lock(m_mutex);
        if (!m_equityDataBuffers.contains(ticker)) {
            m_equityDataBuffers[ticker] = std::make_shared<EquityDataBuffer>(ticker);
        }
        targetBuffer = m_equityDataBuffers[ticker];
    }

    targetBuffer->addLevelOneData(fields);

    return targetBuffer;
}

}
