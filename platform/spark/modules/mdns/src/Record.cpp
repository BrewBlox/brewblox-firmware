#include "Record.h"
#include "spark_wiring_wifi.h"
#include <memory>
#include <string>

void
Label::write(UDPExtended& udp) const
{
    udp.put(name);
    if (next) {
        next->writeLabel(udp);
    } else {
        udp.put(uint8_t(0)); // write closing zero
    }
}

Record::Record(Label label, uint16_t type, uint16_t cls, uint32_t ttl, bool announce)
    : label(std::move(label))
    , type(type)
    , cls(cls)
    , ttl(ttl)
    , announce(announce)
{
}

void
Record::announceRecord()
{
    if (this->announce) {
        this->answerRecord = true;
    }
}

void
Record::setAnswerRecord()
{
    this->answerRecord = true;
}

bool
Record::isAnswerRecord()
{
    return answerRecord && !knownRecord;
}

void
Record::setAdditionalRecord()
{
    this->additionalRecord = true;
}

bool
Record::isAdditionalRecord()
{
    return additionalRecord && !answerRecord && !knownRecord;
}

void
Record::setKnownRecord()
{
    this->knownRecord = true;
}

void
Record::writeLabel(UDPExtended& udp) const
{
    label.write(udp);
}

void
Record::write(UDPExtended& udp) const
{
    writeLabel(udp);
    udp.put(type);
    udp.put(cls);
    udp.put(ttl);
    writeSpecific(udp);
}

bool
Record::match(std::vector<std::string>::const_iterator qnameBegin, std::vector<std::string>::const_iterator qnameEnd, uint16_t qtype, uint16_t qclass) const
{
    // use qtype 0 as don't care
    if ((!(qtype == type || qtype == ANY_TYPE)) || (qclass != cls || qclass == 0 || cls == 0)) {
        return false;
    }
    if (label.name.size() == 0 && label.next) {
        // empty label that fully uses the label of another record
        return label.next->match(qnameBegin, qnameEnd, ANY_TYPE, 0);
    }

    if (qnameBegin->size() == label.name.size()) {
        if (qnameBegin->compare(label.name) == 0) {
            // matched this part of name, recursively check remainder
            if (label.next) {
                auto qnameNext = qnameBegin + 1;
                if (qnameNext == qnameEnd) {
                    return false; // query name is shorter than record name
                }
                // for next label we don't care about the qtype anymore
                return label.next->match(qnameNext, qnameEnd, ANY_TYPE, 0);
            }
            return true;
        }
    }
    return false;
}

void
Record::reset()
{
    this->label.offset = 0;
    this->answerRecord = false;
    this->additionalRecord = false;
    this->knownRecord = false;
}

const Label&
Record::getLabel() const
{
    return label;
}

ARecord::ARecord(Label label)
    : Record(std::move(label), A_TYPE, IN_CLASS | CACHE_FLUSH, TTL_2MIN)
{
}

void
ARecord::writeSpecific(UDPExtended& udp) const
{
    udp.put(uint16_t(4));
    IPAddress ip = spark::WiFi.localIP();
    // TODO use IP from UDP class
    for (int i = 0; i < IP_SIZE; i++) {
        udp.put(uint8_t(ip[i]));
    }
}

MetaRecord::MetaRecord(Label label)
    : Record(std::move(label), 0, 0, 0, false)
{
}

void
MetaRecord::writeSpecific(UDPExtended& udp) const
{
}

NSECRecord::NSECRecord(Label label)
    : Record(std::move(label), NSEC_TYPE, IN_CLASS | CACHE_FLUSH, TTL_2MIN)
{
}

HostNSECRecord::HostNSECRecord(Label label)
    : NSECRecord(std::move(label))
{
}

void
HostNSECRecord::writeSpecific(UDPExtended& udp) const
{
    udp.put(uint16_t(5));
    writeLabel(udp);
    udp.put(uint8_t(0));
    udp.put(uint8_t(1));
    udp.put(uint8_t(0x40));
}

InstanceNSECRecord::InstanceNSECRecord(Label label)
    : NSECRecord(std::move(label))
{
}

void
InstanceNSECRecord::writeSpecific(UDPExtended& udp) const
{
    udp.put(uint16_t(9));
    writeLabel(udp);
    udp.put(uint8_t(0));
    udp.put(uint8_t(5));
    udp.put(uint8_t(0));
    udp.put(uint8_t(0));
    udp.put(uint8_t(0x80));
    udp.put(uint8_t(0));
    udp.put(uint8_t(0x40));
}

PTRRecord::PTRRecord(Label label, bool meta)
    : Record(std::move(label), PTR_TYPE, IN_CLASS, TTL_75MIN, !meta)
{
}

void
PTRRecord::setTargetRecord(std::shared_ptr<Record> target)
{
    targetRecord = std::move(target);
}

void
PTRRecord::writeSpecific(UDPExtended& udp) const
{
    if (targetRecord) {
        targetRecord->writeLabel(udp);
    }
}

SRVRecord::SRVRecord(Label label)
    : Record(std::move(label), SRV_TYPE, IN_CLASS | CACHE_FLUSH, TTL_2MIN)
{
}

void
SRVRecord::writeSpecific(UDPExtended& udp) const
{
    uint16_t hostLabelSize = 0;
    if (hostRecord) {
        hostLabelSize = hostRecord->getLabel().name.size();
    }
    udp.put(uint16_t(6 + hostLabelSize));
    udp.put(uint16_t(0));
    udp.put(uint16_t(0));
    udp.put(uint16_t(port));
    if (hostLabelSize) {
        hostRecord->writeLabel(udp);
    }
}

void
SRVRecord::setHostRecord(std::shared_ptr<Record> host)
{
    hostRecord = std::move(host);
}

void
SRVRecord::setPort(uint16_t port)
{
    this->port = port;
}

TXTRecord::TXTRecord(Label label)
    : Record(std::move(label), TXT_TYPE, IN_CLASS | CACHE_FLUSH, TTL_75MIN)
{
}

void
TXTRecord::addEntry(std::string entry)
{
    entry.shrink_to_fit();
    data.push_back(std::move(entry));
}

void
TXTRecord::writeSpecific(UDPExtended& udp) const
{
    uint16_t size = 0;

    for (const auto& s : data) {
        size += s.size() + 1;
    }

    udp.put(size);

    for (const auto& s : data) {
        uint8_t length = s.size();

        udp.put(length);
        udp.put(s);
    }
}
