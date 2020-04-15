#ifndef _INCL_MDNS
#define _INCL_MDNS

#include "Buffer.h"
// #include "Label.h"
#include "Record.h"
#include "spark_wiring_udp.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

#define MDNS_ADDRESS IPAddress(224, 0, 0, 251)
#define MDNS_PORT 5353

#define BUFFER_SIZE 512

class MDNS {
public:
    MDNS(std::string hostname);

    bool setHostname(std::string hostname);

    bool addService(std::string protocol, std::string service, uint16_t port, std::string instance, std::vector<std::string> subServices = {});

    void addTXTEntry(std::string key, std::string value = "");

    bool begin(bool announce = false);

    bool processQueries();

private:
    struct QueryHeader {
        uint16_t id;
        uint16_t flags;
        uint16_t qdcount;
        uint16_t ancount;
        uint16_t nscount;
        uint16_t arcount;
    };

    UDP udp;
    Buffer buffer;

    std::shared_ptr<PTRRecord> ROOT = std::make_shared<PTRRecord>(Label(""), true);
    std::shared_ptr<PTRRecord> LOCAL = std::make_shared<PTRRecord>(Label("local", ROOT), true);
    std::shared_ptr<ARecord> hostRecord;
    std::shared_ptr<TXTRecord> txtRecord;
    std::vector<std::shared_ptr<Record>> records;
    std::string status = "Ok";

    QueryHeader readHeader(Buffer& buffer);
    void getResponses();
    void writeResponses();
    bool isAlphaDigitHyphen(std::string string);
    bool isNetUnicode(std::string string);
};

#endif
