#include "util/util.h"
#include "lib_recomp.hpp"
#include "lib_message_queue.h"
#include "lib_net.h"
#include "lib_packets.h"
#include <algorithm>

extern uint8_t PacketTypeToMessageType(PacketType packetType);

namespace util
{
    float SwapFloat(const uint8_t *ptr)
    {
        uint32_t val = ((uint32_t)ptr[0] << 24) | ((uint32_t)ptr[1] << 16) |
                       ((uint32_t)ptr[2] << 8) | ((uint32_t)ptr[3]);
        return BitsToFloat(val);
    }

    int16_t SwapInt16(const uint8_t *ptr)
    {
        return (int16_t)(((uint16_t)ptr[0] << 8) | ((uint16_t)ptr[1]));
    }

    uint16_t SwapUint16(const uint8_t *ptr)
    {
        return ((uint16_t)ptr[0] << 8) | ((uint16_t)ptr[1]);
    }

    uint32_t SwapUint32(const uint8_t *ptr)
    {
        return ((uint32_t)ptr[0] << 24) | ((uint32_t)ptr[1] << 16) |
               ((uint32_t)ptr[2] << 8) | ((uint32_t)ptr[3]);
    }

    template <typename PtrType>
    void SerializeGameMessageToMemory(uint8_t *rdram, const GameMessage &msg, PtrType buffer_ptr)
    {
        MEM_B(0, buffer_ptr) = msg.type;
        MEM_W(4, buffer_ptr) = msg.playerId;
        MEM_W(8, buffer_ptr) = msg.param1;
        MEM_W(12, buffer_ptr) = msg.param2;
        MEM_W(16, buffer_ptr) = msg.param3;
        MEM_W(20, buffer_ptr) = msg.param4;
        MEM_W(24, buffer_ptr) = msg.param5;
        MEM_W(28, buffer_ptr) = msg.param6;

        MEM_W(32, buffer_ptr) = FloatToBits(msg.paramF1);
        MEM_W(36, buffer_ptr) = FloatToBits(msg.paramF2);
        MEM_W(40, buffer_ptr) = FloatToBits(msg.paramF3);
        MEM_W(44, buffer_ptr) = FloatToBits(msg.paramF4);
        MEM_W(48, buffer_ptr) = FloatToBits(msg.paramF5);

        MEM_H(52, buffer_ptr) = msg.dataSize;

        for (size_t i = 0; i < MAX_MESSAGE_DATA_SIZE; i++)
        {
            MEM_B(54 + i, buffer_ptr) = msg.data[i];
        }
    }

    template void SerializeGameMessageToMemory<PTR(GameMessage)>(uint8_t *, const GameMessage &, PTR(GameMessage));

    template <typename PtrType>
    std::vector<uint8_t> ReadByteBufferFromMemory(uint8_t *rdram, PtrType bufPtr, int size)
    {
        std::vector<uint8_t> bytes;
        bytes.resize((size_t)size);
        for (int i = 0; i < size; i++)
        {
            bytes[(size_t)i] = MEM_B(i, bufPtr);
        }
        return bytes;
    }

    template std::vector<uint8_t> ReadByteBufferFromMemory<PTR(uint8_t)>(uint8_t *, PTR(uint8_t), int);

    template <typename PtrType>
    void ReadFloatsFromMemory(uint8_t *rdram, PtrType posPtr, float *outFloats, int count)
    {
        for (int i = 0; i < count; i++)
        {
            uint32_t bits = MEM_W(i * 4, posPtr);
            outFloats[i] = BitsToFloat(bits);
        }
    }

    template void ReadFloatsFromMemory<PTR(float)>(uint8_t *, PTR(float), float *, int);

    void ConvertNetEventToGameMessage(const NetEvent &evt, GameMessage &msg)
    {
        memset(&msg, 0, sizeof(GameMessage));

        msg.type = ::PacketTypeToMessageType(evt.type);
        msg.playerId = evt.playerId;

        if (!evt.textData.empty())
        {
            size_t copySize = std::min(evt.textData.size(), MAX_MESSAGE_DATA_SIZE - 1);
            memcpy(msg.data, evt.textData.c_str(), copySize);
            msg.data[copySize] = '\0';
            msg.dataSize = (uint16_t)(copySize + 1);
        }

        if (evt.intData.size() > 0)
            msg.param1 = evt.intData[0];
        if (evt.intData.size() > 1)
            msg.param2 = evt.intData[1];
        if (evt.intData.size() > 2)
            msg.param3 = evt.intData[2];
        if (evt.intData.size() > 3)
            msg.param4 = evt.intData[3];
        if (evt.intData.size() > 4)
            msg.param5 = evt.intData[4];
        if (evt.intData.size() > 5)
            msg.param6 = evt.intData[5];

        if (evt.floatData.size() > 0)
            msg.paramF1 = evt.floatData[0];
        if (evt.floatData.size() > 1)
            msg.paramF2 = evt.floatData[1];
        if (evt.floatData.size() > 2)
            msg.paramF3 = evt.floatData[2];
        if (evt.floatData.size() > 3)
            msg.paramF4 = evt.floatData[3];
        if (evt.floatData.size() > 4)
            msg.paramF5 = evt.floatData[4];
    }

    void DeserializePuppetData(const void *puppet_data, PuppetUpdatePacket &pak)
    {
        const uint8_t *byte_ptr = (const uint8_t *)puppet_data;
        const float *float_ptr = (const float *)puppet_data;

        pak.x = float_ptr[0];
        pak.y = float_ptr[1];
        pak.z = float_ptr[2];
        pak.yaw = float_ptr[3];
        pak.pitch = float_ptr[4];
        pak.roll = float_ptr[5];
        pak.anim_duration = float_ptr[6];
        pak.anim_timer = float_ptr[7];

        const int16_t *int16_ptr_32 = (const int16_t *)&byte_ptr[32];
        pak.level_id = int16_ptr_32[0];
        pak.map_id = int16_ptr_32[1];

        const uint16_t *uint16_ptr_38 = (const uint16_t *)&byte_ptr[38];
        pak.anim_id = uint16_ptr_38[0];

        pak.model_id = byte_ptr[40];
        pak.flags = byte_ptr[41];
        pak.playback_direction = byte_ptr[42];
        pak.playback_type = byte_ptr[43];
    }
}
