#ifndef _INCL_MDNS
#define _INCL_MDNS

#include "Buffer.h"
#include "Label.h"
#include "Record.h"
#include "spark_wiring_udp.h"
#include <map>
#include <vector>

#define MDNS_ADDRESS IPAddress(224, 0, 0, 251)
#define MDNS_PORT 5353

#define BUFFER_SIZE 512
#define HOSTNAME ""
#define META_SERVICE "_services._dns-sd._udp"

class MDNS {
public:
    MDNS()
        : buffer(BUFFER_SIZE)
    {
    }

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

    Label* ROOT = new Label("");
    Label* LOCAL = new Label("local", ROOT);
    MetaLabel* META = new MetaLabel("_services", new Label("_dns-sd", new Label("_udp", LOCAL)));
    Label::Matcher* matcher = new Label::Matcher();

    ARecord* aRecord;
    TXTRecord* txtRecord;

    std::map<std::string, Label*> labels;
    std::vector<Record*> records;
    std::string status = "Ok";

    QueryHeader readHeader(Buffer& buffer);
    void getResponses();
    void writeResponses();
    bool isAlphaDigitHyphen(std::string string);
    bool isNetUnicode(std::string string);
};

#endif
