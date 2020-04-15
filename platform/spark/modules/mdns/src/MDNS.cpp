#include "MDNS.h"
#include "spark_wiring_wifi.h"
#include <memory>

MDNS::MDNS(std::string hostname)
    : buffer(BUFFER_SIZE)
    , hostRecord(std::make_shared<ARecord>(Label(hostname, this->LOCAL)))
    , txtRecord(std::make_shared<TXTRecord>(Label("", this->hostRecord)))
{

    records.push_back(hostRecord);
    records.push_back(txtRecord);
    records.push_back(std::make_shared<HostNSECRecord>(Label("", this->hostRecord)));
}

bool
MDNS::addService(std::string protocol, std::string service, uint16_t port, std::string instance, std::vector<std::string> subServices = {})
{
    if (protocol.length() < MAX_LABEL_SIZE - 1 && service.length() < MAX_LABEL_SIZE - 1 && instance.length() < MAX_LABEL_SIZE && isAlphaDigitHyphen(protocol) && isAlphaDigitHyphen(service) && isNetUnicode(instance)) {

        Label protocolLabel("_" + protocol, LOCAL);

        auto protocolRecord = std::make_shared<PTRRecord>(protocolLabel, false); // meta record

        Label serviceLabel("_" + service, protocolRecord);
        auto servicePtrRecord = std::make_shared<PTRRecord>(serviceLabel);

        Label instanceLabel(instance, servicePtrRecord);
        auto srvRecord = std::make_shared<SRVRecord>(instanceLabel);
        servicePtrRecord->setTargetRecord(srvRecord);

        auto txtRecord = std::make_shared<TXTRecord>(Label("", srvRecord));
        auto instanceNSECRecord = std::make_shared<InstanceNSECRecord>(Label("", srvRecord));

        srvRecord->setHostRecord(hostRecord);
        srvRecord->setPort(port);

        records.push_back(servicePtrRecord);
        records.push_back(std::move(srvRecord));
        records.push_back(std::move(txtRecord));
        records.push_back(std::move(instanceNSECRecord));

        for (auto const& s : subServices) {
            std::string subServiceString = "_" + s + "._sub.";

            auto subPTRRecord = std::make_shared<PTRRecord>(Label(std::move(subServiceString), hostRecord));

            subPTRRecord->setTargetRecord(servicePtrRecord);
            records.push_back(subPTRRecord);
        }
        return true;
    }
    return false;
}

void
MDNS::addTXTEntry(std::string key, std::string value)
{
    txtRecord->addEntry(key, value);
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
        buffer.read(udp);

        udp.flush();

        getResponses();

        buffer.clear();

        writeResponses();
    }

    return n > 0;
}

void
MDNS::getResponses()
{
    QueryHeader header = readHeader(buffer);

    if ((header.flags & 0x8000) == 0 && header.qdcount > 0) {
        uint8_t count = 0;

        while (count++ < header.qdcount && buffer.available() > 0) {
            for (const auto& r : records) {
                Label* label = matcher->match(labels, buffer);

                if (buffer.available() >= 4) {
                    uint16_t type = buffer.readUInt16();
                    uint16_t cls = buffer.readUInt16();

                    if (label != nullptr) {

                        label->matched(type, cls);
                    }
                } else {
                    // status = "Buffer underflow at index " + buffer.getOffset();
                }
            }
        }
    }
}

MDNS::QueryHeader
MDNS::readHeader(Buffer& buffer)
{
    QueryHeader header;

    if (buffer.available() >= 12) {
        header.id = buffer.readUInt16();
        header.flags = buffer.readUInt16();
        header.qdcount = buffer.readUInt16();
        header.ancount = buffer.readUInt16();
        header.nscount = buffer.readUInt16();
        header.arcount = buffer.readUInt16();
    }

    return header;
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
        buffer.writeUInt16(0x0);
        buffer.writeUInt16(0x8400);
        buffer.writeUInt16(0x0);
        buffer.writeUInt16(answerCount);
        buffer.writeUInt16(0x0);
        buffer.writeUInt16(additionalCount);

        for (auto& r : records) {
            if (r->isAnswerRecord()) {
                r->write(buffer);
            }
        }

        for (auto& r : records) {
            if (r->isAdditionalRecord()) {
                r->write(buffer);
            }
        }
    }

    if (buffer.available() > 0) {
        udp.beginPacket(MDNS_ADDRESS, MDNS_PORT);

        buffer.write(udp);

        udp.endPacket();
    }

    for (auto& r : records) {
        r->reset();
    }
}

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
