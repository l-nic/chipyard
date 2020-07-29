#ifndef PACKETPP_LNIC_LAYER
#define PACKETPP_LNIC_LAYER
 
#include "Layer.h"
 
#define LNIC_DATA_FLAG_MASK        0b1
#define LNIC_ACK_FLAG_MASK         0b10
#define LNIC_NACK_FLAG_MASK        0b100
#define LNIC_PULL_FLAG_MASK        0b1000
#define LNIC_CHOP_FLAG_MASK        0b10000
 
namespace pcpp
{

#pragma pack(push,1)
    struct lnichdr {
        uint8_t flags;
        uint16_t src_context;
        uint16_t dst_context;
        uint16_t msg_len;
        uint8_t pkt_offset;
        uint16_t pull_offset;
        uint16_t tx_msg_id;
        uint16_t buf_ptr;
        uint8_t buf_size_class;
        uint8_t padding[15];
    };
#pragma pack(pop)
 
 
    class LnicLayer : public Layer
    {
    public:
        LnicLayer(uint8_t* data, size_t dataLen, Layer* prevLayer, Packet* packet) : Layer(data, dataLen, prevLayer, packet) { m_Protocol = GenericPayload; }

        LnicLayer(uint8_t flags, uint16_t src_context, uint16_t dst_context, uint16_t msg_len, uint8_t pkt_offset, uint16_t pull_offset, uint16_t tx_msg_id, uint16_t buf_ptr, uint8_t buf_size_class);

        lnichdr* getLnicHeader() const { return (lnichdr*)m_Data; }

        void parseNextLayer() { return; }

        size_t getHeaderLen() const { return sizeof(lnichdr); }

        void computeCalculateFields() { return; }

        std::string toString() const;

        OsiModelLayer getOsiModelLayer() const { return OsiModelTransportLayer; }
    };

} // namespace pcpp
 
#endif /* PACKETPP_LNIC_LAYER */