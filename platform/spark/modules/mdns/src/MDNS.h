#pragma once
// #include "Buffer.h"
// #include "Label.h"
#include "Record.h"
#include "UDPExtended.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

#define MDNS_ADDRESS IPAddress(224, 0, 0, 251)
#define MDNS_PORT 5353

#define BUFFER_SIZE 512

class MDNS {
public:
    enum class Protocol {
        UDP,
        TCP,
    };

    MDNS(std::string hostname);
    ~MDNS()
    {
        for (auto r : records) {
            delete r;
        }
        for (auto r : metaRecords) {
            delete r;
        }
    }

    void addService(Protocol protocol, std::string serviceType, const std::string serviceName, uint16_t port,
                    std::vector<std::string>&& txtEntries = std::vector<std::string>(),
                    std::vector<std::string>&& subServices = std::vector<std::string>());

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

    struct Query {
        Query()
            : header{0}
        {
        }
        ~Query() = default;

        struct Question {
            std::vector<std::string> qname;
            std::vector<uint16_t> qnameOffset;
            uint16_t qtype = 0;
            uint16_t qclass = 0;
        };

        QueryHeader header;
        std::vector<Question> questions;
    };

    UDPExtended udp;

    // meta records for re-using labels
    MetaRecord* LOCAL;
    MetaRecord* UDP;
    MetaRecord* TCP;
    MetaRecord* DNSSD;
    MetaRecord* SERVICES;

    // actual records that are checked
    ARecord* hostRecord;

    // vectors of records to iterate over them
    std::vector<Record*> records;
    std::vector<MetaRecord*> metaRecords;

    Query getQuery();

    void processQuery(const Query& q);
    void processQuestion(const Query::Question& question);
    void writeResponses();
};
