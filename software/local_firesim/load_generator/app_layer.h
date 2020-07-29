#ifndef PACKETPP_APP_LAYER
#define PACKETPP_APP_LAYER
 
#include "Layer.h"
 
namespace pcpp
{

#pragma pack(push,1)
    struct apphdr {
        uint64_t service_time;
        uint64_t sent_time;
    };
#pragma pack(pop)
 
 
    class AppLayer : public Layer
    {
    public:
        AppLayer(uint8_t* data, size_t dataLen, Layer* prevLayer, Packet* packet) : Layer(data, dataLen, prevLayer, packet) { m_Protocol = GenericPayload; }

        AppLayer(uint64_t service_time, uint64_t sent_time);

        apphdr* getAppHeader() const { return (apphdr*)m_Data; }

        void parseNextLayer() { return; }

        size_t getHeaderLen() const { return sizeof(apphdr); }

        void computeCalculateFields() { return; }

        std::string toString() const;

        OsiModelLayer getOsiModelLayer() const { return OsiModelApplicationLayer; }
    };

} // namespace pcpp
 
#endif /* PACKETPP_APP_LAYER */