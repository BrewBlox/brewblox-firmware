#include "MDNS.h"
#include "spark_wiring_wifi.h"
#include <algorithm>
#include <cctype> // for std::tolower
#include <memory>

MDNS::MDNS(std::string hostname)
    : LOCAL(new MetaRecord(Label(std::string{"local"}, nullptr)))
    , UDP(new MetaRecord(Label(std::string{"_udp"}, LOCAL)))
    , TCP(new MetaRecord(Label(std::string{"_tcp"}, LOCAL)))
    , DNSSD(new MetaRecord(Label("_dns-sd", UDP)))
    , SERVICES(new MetaRecord(Label("_services", DNSSD)))
    , hostRecord(new ARecord(Label(std::move(hostname), LOCAL)))
    , records{hostRecord, new HostNSECRecord(Label(std::string(), hostRecord), hostRecord)}
    , metaRecords{LOCAL, UDP, TCP, DNSSD, SERVICES}
{
}

void
MDNS::addService(Protocol protocol, std::string serviceType, std::string serviceName, uint16_t port,
                 std::vector<std::string>&& txtEntries,
                 std::vector<std::string>&& subServices)
{
    Record* protocolRecord;
    if (protocol == Protocol::TCP) {
        protocolRecord = this->TCP;
    } else if (protocol == Protocol::UDP) {
        protocolRecord = this->UDP;
    } else {
        return;
    }

    // todo reserve vector space

    // A pointer record indicating where this service can be found
    auto ptrRecord = new PTRRecord(Label(std::move(serviceType), protocolRecord));

    // An enumeration record for DNS-SD
    auto enumerationRecord = new PTRRecord(Label(std::string(), this->SERVICES), false);
    enumerationRecord->setTargetRecord(ptrRecord);

    // the service record indicating under which name/port this service is available
    auto srvRecord = new SRVRecord(Label(std::move(serviceName), ptrRecord), port, ptrRecord, hostRecord);
    ptrRecord->setTargetRecord(srvRecord);

    // RFC 6763 says:
    //    An empty TXT record containing zero strings is not allowed [RFC1035].
    //    DNS-SD implementations MUST NOT emit empty TXT records.  DNS-SD
    //    clients MUST treat the following as equivalent:
    //    o  A TXT record containing a single zero byte.
    //       (i.e., a single empty string.)
    //    o  An empty (zero-length) TXT record.
    //       (This is not strictly legal, but should one be received, it should
    //       be interpreted as the same as a single empty string.)
    //    o  No TXT record.
    //       (i.e., an NXDOMAIN or no-error-no-answer response.)
    //    o  A TXT record containing a single zero byte.
    //       (i.e., a single empty string.)RFC 6763
    if (txtEntries.empty()) {
        txtEntries.push_back("");
    }
    auto txtRecord = new TXTRecord(Label(std::string(), srvRecord), std::move(txtEntries));

    auto nsecRecord = new ServiceNSECRecord(Label(std::string(), srvRecord));
    srvRecord->setTxtRecord(txtRecord);
    srvRecord->setNsecRecord(nsecRecord);

    // From RFC6762:
    //    On receipt of a question for a particular name, rrtype, and rrclass,
    //    for which a responder does have one or more unique answers, the
    //    responder MAY also include an NSEC record in the Additional Record
    //    Section indicating the nonexistence of other rrtypes for that name
    //    and rrclass.
    // So we include an NSEC record with the same label as the service record
    records.push_back(ptrRecord);
    records.push_back(srvRecord);
    records.push_back(txtRecord);
    records.push_back(nsecRecord);
    records.push_back(enumerationRecord);

    if (!subServices.empty()) {
        auto subMetaRecord = new MetaRecord(Label(std::string("_sub"), ptrRecord)); // meta record to hold _sub

        for (auto&& s : subServices) {
            auto subPTRRecord = new PTRRecord(Label(std::move(s), subMetaRecord));
            subPTRRecord->setTargetRecord(ptrRecord);
            records.push_back(subPTRRecord);
        }
    }
}

bool
MDNS::begin(bool announce)
{
    // Wait for spark::WiFi to connect
    if (!spark::WiFi.ready()) {
        return false;
    }

    //     udp.setBuffer(BUFFER_SIZE, buffer.data);
    udp.begin(MDNS_PORT);
    udp.joinMulticast(MDNS_ADDRESS);

    // TODO: Probing

    if (announce) {
        for (auto& r : records) {
            r->announceRecord();
        }

        writeResponses();
    }

    return true;
}

bool
MDNS::processQueries()
{
    uint16_t n = udp.parsePacket();

    if (n > 0) {
        auto q = getQuery();

        if (q.qname.size() > 0) {
            processQuery(q);
        }

        udp.flush_buffer();

        writeResponses();
    }

    return n > 0;
}

MDNS::Query
MDNS::getQuery()
{
    Query q;

    if (udp.available() >= 12) {
        udp.get(q.header.id);
        udp.get(q.header.flags);
        udp.get(q.header.qdcount);
        udp.get(q.header.ancount);
        udp.get(q.header.nscount);
        udp.get(q.header.arcount);
    }
    if ((q.header.flags & 0x8000) == 0 && q.header.qdcount > 0) {
        uint8_t count = 0;
        while (count < q.header.qdcount && udp.available() > 4) {
            if (count == 0) { // only process first question for now
                while (true) {
                    uint8_t strlen = 0;
                    udp.get(strlen);
                    if (strlen) {
                        std::string subname;
                        subname.reserve(strlen);
                        for (uint8_t len = 0; len < strlen && udp.available() > 0; len++) {
                            char c = 0;
                            udp.get(c);
                            subname.push_back(std::tolower(c));
                        }
                        q.qname.push_back(std::move(subname));
                    } else {
                        break;
                    }
                }
            } else { // TODO: handle multiple questions
            }
            count++;
        }

        if (udp.available() >= 4) {
            udp.get(q.qtype);
            udp.get(q.qclass);
        }
    }
    return q;
}

void
MDNS::processQuery(const Query& q)
{
    for (const auto& r : records) {
        if (r->match(q.qname.cbegin(), q.qname.cend(), q.qtype, q.qclass)) {
            r->matched(q.qtype);
        }
    }
}

void
MDNS::writeResponses()
{
    uint8_t answerCount = 0;
    uint8_t additionalCount = 0;

    for (auto& r : metaRecords) {
        r->resetLabelOffset();
    }

    for (auto& r : records) {
        if (r->isAnswerRecord()) {
            answerCount++;
        }
        if (r->isAdditionalRecord()) {
            additionalCount++;
        }
        r->resetLabelOffset();
    }

    if (answerCount > 0) {
        udp.beginPacket(MDNS_ADDRESS, MDNS_PORT);

        udp.put(uint16_t(0x0));
        udp.put(uint16_t(0x8400));
        udp.put(uint16_t(0x0));
        udp.put(uint16_t(answerCount));
        udp.put(uint16_t(0x0));
        udp.put(uint16_t(additionalCount));

        for (auto& r : records) {
            if (r->isAnswerRecord()) {
                r->write(udp);
            }
        }

        for (auto& r : records) {
            if (r->isAdditionalRecord()) {
                r->write(udp);
            }
        }
        udp.endPacket();
    }

    for (auto& r : records) {
        r->reset();
    }
}

/*
bool
MDNS::isAlphaDigitHyphen(std::string s)
{
    for (const auto& c : s) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-')) {
            return false;
        }
    }
    return true;
}

bool
MDNS::isNetUnicode(std::string s)
{
    for (const auto& c : s) {
        if (!(c >= 0x1f && c != 0x7f)) {
            return false;
        }
    }

    return true;
}
*/