#include "app_layer.h"
#include <string.h>
#include <sstream>

namespace pcpp
{

AppLayer::AppLayer(uint64_t service_time, uint64_t sent_time) {
    const size_t header_len = sizeof(apphdr);
    m_DataLen = header_len;
    m_Data = new uint8_t[header_len];
    memset(m_Data, 0, header_len);
    apphdr* app_hdr = (apphdr*)m_Data;
    m_Protocol = GenericPayload;

    // TODO: Check that the NIC actually adjusts the endianness
    app_hdr->service_time = htobe64(service_time);
    app_hdr->sent_time = htobe64(sent_time);
}

std::string AppLayer::toString() const {
    std::ostringstream output_string;
    return output_string.str();
}

}