#ifndef UTIL_EXTLIB_H
#define UTIL_EXTLIB_H

// =========================================================================== //
// Common utility functions for the external library
// =========================================================================== //

#include <cstdint>
#include <cstring>
#include <vector>

// Forward declarations
struct GameMessage;
struct NetEvent;
struct PuppetUpdatePacket;

namespace util
{
    float SwapFloat(const uint8_t *ptr);
    int16_t SwapInt16(const uint8_t *ptr);
    uint16_t SwapUint16(const uint8_t *ptr);
    uint32_t SwapUint32(const uint8_t *ptr);

    inline float BitsToFloat(uint32_t bits)
    {
        float result;
        std::memcpy(&result, &bits, sizeof(float));
        return result;
    }

    inline uint32_t FloatToBits(float value)
    {
        uint32_t result;
        std::memcpy(&result, &value, sizeof(float));
        return result;
    }

    template <typename PtrType>
    void SerializeGameMessageToMemory(uint8_t *rdram, const GameMessage &msg, PtrType buffer_ptr);

    template <typename PtrType>
    std::vector<uint8_t> ReadByteBufferFromMemory(uint8_t *rdram, PtrType bufPtr, int size);

    template <typename PtrType>
    void ReadFloatsFromMemory(uint8_t *rdram, PtrType posPtr, float *outFloats, int count);

    void ConvertNetEventToGameMessage(const NetEvent &evt, GameMessage &msg);

    void DeserializePuppetData(const void *puppet_data, PuppetUpdatePacket &pak);
}

#endif
