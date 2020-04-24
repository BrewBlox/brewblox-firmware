#ifndef _INCL_MDNS
#define _INCL_MDNS

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

    void addService(Protocol protocol, std::string serviceType, const std::string serviceName,
                    uint16_t port, std::vector<std::string>&& subServices = std::vector<std::string>());

    void addTXTEntry(std::string entry);

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
            , qtype{0}
            , qclass{0}
        {
        }
        ~Query() = default;
        std::vector<std::string> qname;
        QueryHeader header;
        uint16_t qtype;
        uint16_t qclass;
    };

    UDPExtended udp;

    // meta records for re-using labels
    std::shared_ptr<MetaRecord> LOCAL;
    std::shared_ptr<MetaRecord> UDP;
    std::shared_ptr<MetaRecord> TCP;
    std::shared_ptr<MetaRecord> DNSSD;
    std::shared_ptr<MetaRecord> SERVICES;

    // actual records
    std::shared_ptr<ARecord> hostRecord;
    std::shared_ptr<TXTRecord> txtRecord;

    std::vector<std::shared_ptr<Record>> records;

    Query getQuery();

    void processQuery(const Query& q);
    void writeResponses();
    //bool isAlphaDigitHyphen(std::string string);
    //bool isNetUnicode(std::string string);

    std::shared_ptr<Record> findRecord(std::vector<std::string>::const_iterator qname,
                                       std::vector<std::string>::const_iterator qnameEnd,
                                       uint16_t qtype, uint16_t qclass);
};

#endif
