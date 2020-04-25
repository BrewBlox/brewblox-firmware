#pragma once
#include "spark_wiring_udp.h"
class UDPExtended : public UDP {
private:
    uint16_t write_offset = 0; // UDP already implements an write_offset, but it is private

public:
    using UDP::UDP;

    uint16_t writeOffset()
    {
        return write_offset;
    }

    void put(uint8_t v)
    {
        write_offset += write(v);
    }

    void put(uint16_t v)
    {
        write_offset += write(uint8_t((v >> 8) & 0xff));
        write_offset += write(uint8_t(v & 0xff));
    }

    void put(uint32_t v)
    {
        write_offset += write(uint8_t((v >> 24) & 0xff));
        write_offset += write(uint8_t((v >> 16) & 0xff));
        write_offset += write(uint8_t((v >> 8) & 0xff));
        write_offset += write(uint8_t(v & 0xff));
    }

    void put(const std::string& s)
    {
        write_offset += write(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    }

    void get(uint8_t& v)
    {
        if (available() >= 1) {
            v = read();
        }
    }

    void get(char& v)
    {
        auto cptr = reinterpret_cast<uint8_t*>(&v);
        get(*cptr);
    }

    void get(uint16_t& v)
    {
        if (available() >= 2) {
            v = (uint16_t(read()) << 8) + uint16_t(read());
        }
    }

    void get(uint32_t& v)
    {
        if (available() >= 4) {
            v = (uint32_t(read()) << 24) + (uint32_t(read()) << 16) + (uint32_t(read()) << 8) + uint32_t(read());
        }
    }

    virtual int beginPacket(IPAddress ip, uint16_t port)
    {
        write_offset = 0;
        return UDP::beginPacket(ip, port);
    }
};