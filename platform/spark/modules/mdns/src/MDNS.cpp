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
    , records{hostRecord, new HostNSECRecord(Label(hostRecord), hostRecord)}
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

    records.reserve(records.size() + 5 + subServices.size());

    // A pointer record indicating where this service can be found
    auto ptrRecord = new PTRRecord(Label(std::move(serviceType), protocolRecord));

    // An enumeration record for DNS-SD
    auto enumerationRecord = new PTRRecord(Label(this->SERVICES), false);
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
    auto txtRecord = new TXTRecord(Label(srvRecord), std::move(txtEntries));

    auto nsecRecord = new ServiceNSECRecord(Label(srvRecord));
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
        // create meta record to hold _sub prefix
        auto subMetaRecord = new MetaRecord(Label(std::string("_sub"), ptrRecord));
        metaRecords.push_back(std::move(subMetaRecord));

        for (auto&& s : subServices) {
            auto subPTRRecord = new PTRRecord(Label(std::move(s), subMetaRecord));
            subPTRRecord->setTargetRecord(ptrRecord);
            records.push_back(std::move(subPTRRecord));
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

    udp.begin(MDNS_PORT);
    udp.joinMulticast(MDNS_ADDRESS);

    // TODO: Probing: check if host/SRV records we will announce are already in use

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

        if (q.questions.size() > 0) {
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

    auto bufferSize = udp.available(); // offset in udp is private, calculate from remaining

    if (udp.available() >= 12) {
        udp.get(q.header.id);
        udp.get(q.header.flags);
        udp.get(q.header.qdcount);
        udp.get(q.header.ancount);
        udp.get(q.header.nscount);
        udp.get(q.header.arcount);
    }
    if ((q.header.flags & 0x8000) == 0 && q.header.qdcount > 0) {
        q.questions.reserve(q.header.qdcount);
        while (q.questions.size() < q.header.qdcount && udp.available() > 4) {
            Query::Question question;
            while (true) {
                auto offset = bufferSize - udp.available();
                uint8_t strlen = 0;
                udp.get(strlen);
                if (strlen & uint8_t(0xc0)) {
                    // pointer to earlier qname
                    uint8_t byte2 = 0;
                    udp.get(byte2);
                    uint16_t ptrVal = (uint16_t(strlen & uint8_t(0x3F)) << 8) + byte2;
                    // find earlier qname
                    bool found = false;
                    for (const auto& earlierQuestion : q.questions) {
                        auto o = earlierQuestion.qnameOffset.cbegin();
                        auto o_end = earlierQuestion.qnameOffset.cend();
                        auto s = earlierQuestion.qname.cbegin();
                        auto s_end = earlierQuestion.qname.cend();
                        for (; o < o_end && s < s_end; o++, s++) {
                            if (*o == ptrVal) {
                                question.qname.reserve(question.qname.size() + s_end - s);
                                // append matching part of qname
                                question.qname.insert(question.qname.end(), s, s_end);
                                found = true;
                                break;
                            }
                        }
                        if (found) {
                            break;
                        }
                    }
                    if (found) {
                        break;
                    } else {
                        // invalid pointer in question, ignore packet
                        q.header.qdcount = 0;
                        return q;
                    }
                } else if (strlen) {
                    // new (part of) qname
                    std::string subname;
                    subname.reserve(strlen);
                    for (uint8_t len = 0; len < strlen && udp.available() > 0; len++) {
                        char c = 0;
                        udp.get(c);
                        subname.push_back(std::tolower(c));
                    }
                    question.qname.push_back(std::move(subname));
                    question.qnameOffset.push_back(offset);
                } else {
                    break;
                }
            }
            if (udp.available() >= 4) {
                udp.get(question.qtype);
                udp.get(question.qclass);
            } else {
                // missing fields in question, ignore packet
                q.header.qdcount = 0;
                return q;
            }
            q.questions.push_back(std::move(question));
        }
    }
    return q;
}

void
MDNS::processQuery(const Query& query)
{
    for (const auto& question : query.questions) {
        processQuestion(question);
    }
}

void
MDNS::processQuestion(const Query::Question& question)
{
    if (question.qname.size() > 0) {
        for (const auto& r : records) {
            if (r->match(question.qname.cbegin(), question.qname.cend(), question.qtype, question.qclass)) {
                r->matched(question.qtype);
            }
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