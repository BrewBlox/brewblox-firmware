#include "MDNS.h"
#include "spark_wiring_wifi.h"
#include <algorithm>
#include <cctype> // for std::tolower
#include <memory>
#include <string>

MDNS::MDNS(std::string hostname)
    : LOCAL(std::make_shared<MetaRecord>(Label(std::string{"local"})))
    , hostRecord(std::make_shared<ARecord>(Label(std::move(hostname), this->LOCAL)))
    , txtRecord(std::make_shared<TXTRecord>(Label(std::string(), this->hostRecord)))
    , records{hostRecord, txtRecord, std::make_shared<HostNSECRecord>(Label(std::string(), this->hostRecord))}
{
}

std::shared_ptr<Record>
MDNS::findRecord(std::vector<std::string>::const_iterator nameBegin,
                 std::vector<std::string>::const_iterator nameEnd,
                 uint16_t qtype, uint16_t qclass)
{
    auto begin = records.cbegin();
    auto end = records.cend();
    auto record = std::find_if(begin, end, [&nameBegin, &nameEnd, &qtype, &qclass](const std::shared_ptr<Record>& r) {
        return r->match(nameBegin, nameEnd, qtype, qclass);
    });
    if (record != end) {
        return *record;
    }
    return std::shared_ptr<Record>();
}

void
MDNS::addService(std::string protocol, std::string serviceType, std::string serviceName, uint16_t port, std::vector<std::string>&& subServices)
{
    // labels as vector for looking up existing records
    std::vector<std::string> lookup = {std::move(protocol), std::move(serviceType)};

    // a protocol meta record, just to hold the protocol name. Exising record is used if found
    auto protocolRecord = findRecord(lookup.cbegin(), lookup.cbegin() + 1, 0, 0);
    if (!protocolRecord) {
        protocolRecord = std::make_shared<MetaRecord>(Label(lookup[0], LOCAL)); // create meta record for protocl
        records.reserve(4 + subServices.size());
        records.push_back(protocolRecord);
    } else {
        records.reserve(3 + subServices.size());
    }

    // A pointer record indicating where this service can be found
    auto servicePtrRecord = std::make_shared<PTRRecord>(Label(std::move(serviceType), protocolRecord));

    // the service record indicating that a service of this type is available at PTR
    auto srvRecord = std::make_shared<SRVRecord>(Label(std::move(serviceName), servicePtrRecord));
    servicePtrRecord->setTargetRecord(srvRecord);
    srvRecord->setHostRecord(hostRecord);
    srvRecord->setPort(port);

    // From RFC6762:
    //    On receipt of a question for a particular name, rrtype, and rrclass,
    //    for which a responder does have one or more unique answers, the
    //    responder MAY also include an NSEC record in the Additional Record
    //    Section indicating the nonexistence of other rrtypes for that name
    //    and rrclass.
    // So we include an NSEC record with the same label as the service record
    records.push_back(std::move(std::make_shared<InstanceNSECRecord>(Label(std::string(), srvRecord))));
    records.push_back(std::move(srvRecord));
    records.push_back(servicePtrRecord);

    if (!subServices.empty()) {
        auto subMetaRecord = std::make_shared<PTRRecord>(Label(std::string("_sub"), hostRecord), false); // meta record to hold _sub

        for (auto&& s : subServices) {
            auto subPTRRecord = std::make_shared<PTRRecord>(Label(std::move(s), subMetaRecord));
            subPTRRecord->setTargetRecord(servicePtrRecord);
            records.push_back(std::move(subPTRRecord));
        }
    }
}

void
MDNS::addTXTEntry(std::string entry)
{
    txtRecord->addEntry(std::move(entry));
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

        processQuery(q);

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
                    uint8_t len = 0;
                    udp.get(len);
                    std::string subname;
                    subname.reserve(len);
                    char c = 0;
                    while (len > 0 && udp.available() > 0) {
                        udp.get(c);
                        subname.push_back(std::tolower(c));
                        len--;
                    }
                    if (subname.empty()) {
                        break;
                    }
                    q.qname.push_back(std::move(subname));
                }
            } else {
                while (true) {
                    ; // whaaat?
                }
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

    for (auto& r : records) {
        if (r->isAnswerRecord()) {
            answerCount++;
        }
        if (r->isAdditionalRecord()) {
            additionalCount++;
        }
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