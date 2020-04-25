#include "Record.h"
#include "spark_wiring_wifi.h"
#include <memory>
#include <string>

constexpr const uint16_t LABEL_PTR_MASK = 0xc000;

void
Label::writeFull(UDPExtended& udp) const
{
    // number of writes written so far is the offset, because no reads are since packet start
    this->offset = udp.available();

    udp.put(uint8_t(name.size()));
    udp.put(name);
    if (next) {
        next->write(udp);
    } else {
        udp.put(uint8_t(0)); // write closing zero
    }
}

void
Label::writePtr(UDPExtended& udp) const
{
    udp.put(uint16_t(LABEL_PTR_MASK | this->offset));
}

void
Label::write(UDPExtended& udp) const
{
    if (offset) {
        writePtr(udp);
    } else {
        writeFull(udp);
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
Record::writeLabelPtr(UDPExtended& udp) const
{
    label.writePtr(udp);
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
    if (!(qtype == type || qtype == ANY_TYPE || type == ANY_TYPE) || (qclass != (cls & ~uint16_t(CACHE_FLUSH)))) {
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
                return label.next->match(qnameNext, qnameEnd, ANY_TYPE, qclass);
            }
            return true;
        }
    }
    return false;
}

void
Record::reset()
{
    this->label.reset();
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
    : Record(std::move(label), ANY_TYPE, IN_CLASS, 0, false)
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
    writeLabelPtr(udp);
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
    writeLabelPtr(udp);
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
        targetRecord->writeLabelPtr(udp);
    } else {
        udp.put(LABEL_PTR_MASK); // write 0 ptr
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
