#include "MDNS.h"
#include "spark_wiring_wifi.h"

bool
MDNS::setHostname(std::string hostname)
{
    bool success = true;
    std::string status = "Ok";

    if (labels[HOSTNAME]) {
        status = "Hostname already set";
        success = false;
    }

    if (success && hostname.length() < MAX_LABEL_SIZE && isAlphaDigitHyphen(hostname)) {
        aRecord = new ARecord();

        HostNSECRecord* hostNSECRecord = new HostNSECRecord();

        records.push_back(aRecord);
        records.push_back(hostNSECRecord);

        Label* label = new HostLabel(aRecord, hostNSECRecord, std::move(hostname), LOCAL);

        labels[HOSTNAME] = label;
        labels[META_SERVICE] = META;

        aRecord->setLabel(label);
        hostNSECRecord->setLabel(label);
    } else {
        status = success ? "Invalid hostname" : status;
        success = false;
    }

    return success;
}

bool
MDNS::addService(std::string protocol, std::string service, uint16_t port, std::string instance, std::vector<std::string> subServices)
{
    bool success = true;
    std::string status = "Ok";

    if (!labels[HOSTNAME]) {
        status = "Hostname not set";
        success = false;
    }

    if (success && protocol.length() < MAX_LABEL_SIZE - 1 && service.length() < MAX_LABEL_SIZE - 1 && instance.length() < MAX_LABEL_SIZE && isAlphaDigitHyphen(protocol) && isAlphaDigitHyphen(service) && isNetUnicode(instance)) {

        PTRRecord* ptrRecord = new PTRRecord();
        SRVRecord* srvRecord = new SRVRecord();
        txtRecord = new TXTRecord();
        InstanceNSECRecord* instanceNSECRecord = new InstanceNSECRecord();
        PTRRecord* enumerationRecord = new PTRRecord(true);

        records.push_back(ptrRecord);
        records.push_back(srvRecord);
        records.push_back(txtRecord);
        records.push_back(instanceNSECRecord);
        records.push_back(enumerationRecord);

        std::string serviceString = "_" + service + "._" + protocol;

        Label* protocolLabel = new Label("_" + protocol, LOCAL);

        if (labels[serviceString] == nullptr) {
            labels[serviceString] = new ServiceLabel(aRecord, "_" + service, protocolLabel);
        }

        ((ServiceLabel*)labels[serviceString])->addInstance(ptrRecord, srvRecord, txtRecord);

        std::string instanceString = instance + "._" + service + "._" + protocol;

        labels[instanceString] = new InstanceLabel(srvRecord, txtRecord, instanceNSECRecord, aRecord, instance, labels[serviceString], true);
        META->addService(enumerationRecord);

        for (std::vector<std::string>::const_iterator i = subServices.begin(); i != subServices.end(); ++i) {
            std::string subServiceString = "_" + *i + "._sub." + serviceString;

            if (labels[subServiceString] == nullptr) {
                labels[subServiceString] = new ServiceLabel(aRecord, "_" + *i, new Label("_sub", labels[serviceString]));
            }

            PTRRecord* subPTRRecord = new PTRRecord();
            PTRRecord* enumerationSubPTRRecord = new PTRRecord(true);

            subPTRRecord->setLabel(labels[subServiceString]);
            subPTRRecord->setTargetLabel(labels[instanceString]);

            enumerationSubPTRRecord->setLabel(META);
            enumerationSubPTRRecord->setTargetLabel(labels[subServiceString]);

            records.push_back(subPTRRecord);
            records.push_back(enumerationSubPTRRecord);

            ((ServiceLabel*)labels[subServiceString])->addInstance(subPTRRecord, srvRecord, txtRecord);
            META->addService(enumerationSubPTRRecord);
        }

        ptrRecord->setLabel(labels[serviceString]);
        ptrRecord->setTargetLabel(labels[instanceString]);
        srvRecord->setLabel(labels[instanceString]);
        srvRecord->setPort(port);
        srvRecord->setHostLabel(labels[HOSTNAME]);
        txtRecord->setLabel(labels[instanceString]);
        instanceNSECRecord->setLabel(labels[instanceString]);
        enumerationRecord->setLabel(META);
        enumerationRecord->setTargetLabel(labels[serviceString]);
    } else {
        status = success ? "Invalid name" : status;
        success = false;
    }

    return success;
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

    udp->begin(MDNS_PORT);
    udp->joinMulticast(MDNS_ADDRESS);

    // TODO: Probing

    if (announce) {
        for (std::vector<Record*>::const_iterator i = records.begin(); i != records.end(); ++i) {
            (*i)->announceRecord();
        }

        writeResponses();
    }

    return true;
}

bool
MDNS::processQueries()
{
    uint16_t n = udp->parsePacket();

    if (n > 0) {
        buffer.read(udp);

        udp->flush();

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
            Label* label = matcher->match(labels, buffer);

            if (buffer.available() >= 4) {
                uint16_t type = buffer.readUInt16();
                uint16_t cls = buffer.readUInt16();

                if (label != nullptr) {

                    label->matched(type, cls);
                }
            } else {
                status = "Buffer underflow at index " + buffer.getOffset();
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

    for (std::vector<Record*>::const_iterator i = records.begin(); i != records.end(); ++i) {
        if ((*i)->isAnswerRecord()) {
            answerCount++;
        }
        if ((*i)->isAdditionalRecord()) {
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

        for (std::vector<Record*>::const_iterator i = records.begin(); i != records.end(); ++i) {
            if ((*i)->isAnswerRecord()) {
                (*i)->write(buffer);
            }
        }

        for (std::vector<Record*>::const_iterator i = records.begin(); i != records.end(); ++i) {
            if ((*i)->isAdditionalRecord()) {
                (*i)->write(buffer);
            }
        }
    }

    if (buffer.available() > 0) {
        udp->beginPacket(MDNS_ADDRESS, MDNS_PORT);

        buffer.write(udp);

        udp->endPacket();
    }

    for (std::map<std::string, Label*>::const_iterator i = labels.begin(); i != labels.end(); ++i) {
        i->second->reset();
    }

    for (std::vector<Record*>::const_iterator i = records.begin(); i != records.end(); ++i) {
        (*i)->reset();
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
