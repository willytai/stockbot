#ifndef __STREAM_DATA_BUFFER__
#define __STREAM_DATA_BUFFER__

#include <mutex>
#include <memory>
#include <unordered_map>
#include "schwabcpp/streamerField.h"

namespace stockbot {

class EquityDataBuffer;

class StreamDataBuffer
{
public:
                                            StreamDataBuffer();
                                            ~StreamDataBuffer();

    // Returns a weak reference to the buffer which the data was added to
    [[nodiscard]]
    std::weak_ptr<EquityDataBuffer>         addLevelOneEquityData(const std::string& ticker,
                                                                  const std::unordered_map<schwabcpp::StreamerField::LevelOneEquity, double>& fields);

private:
    std::mutex                              m_mutex;
    std::unordered_map<std::string, std::shared_ptr<EquityDataBuffer>>
                                            m_equityDataBuffers;
};

} // namespace stockbot

#endif
