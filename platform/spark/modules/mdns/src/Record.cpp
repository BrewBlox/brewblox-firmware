#include "Record.h"
#include "spark_wiring_wifi.h"
#include <memory>
#include <string>

constexpr const uint16_t LABEL_PTR_MASK = 0xc000;

void
Label::writeFull(UDPExtended& udp) const
{
    // number of writes written so far is the offset, because no reads are since packet start
    if (name.size()) {
        // skip empty name, label is fully defined by next
        this->offset = udp.writeOffset();
        udp.put(uint8_t(name.size()));
        udp.put(name);
    }
    if (next) {
        next->writeLabel(udp);
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

uint16_t
Label::writeSize() const
{
    if (offset) {
        return 2;
    }
    uint16_t size = 0;
    if (name.size() != 0) {
        // has own string
        size = 1 + name.size();
    }
    if (next) {
        return size + next->getLabel().writeSize(); // add size of next
    }
    return size + 1; // add closing zero
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
Record::resetLabelOffset()
{
    this->label.reset();
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
    udp.put(uint8_t(ip[0]));
    udp.put(uint8_t(ip[1]));
    udp.put(uint8_t(ip[2]));
    udp.put(uint8_t(ip[3]));
}

void
ARecord::matched(uint16_t qtype)
{
    if (qtype == A_TYPE || qtype == ANY_TYPE) {
        this->setAnswerRecord();
        if (nsecRecord) {
            // indicates that this is the only record. Signals we don't have an AAAA ipv6 record
            nsecRecord->setAdditionalRecord();
        }
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

HostNSECRecord::HostNSECRecord(Label label, ARecord* hostRecord)
    : NSECRecord(std::move(label))
{
    hostRecord->setNsecRecord(this);
}

void
HostNSECRecord::writeSpecific(UDPExtended& udp) const
{
    uint16_t labelSize = label.writeSize();
    udp.put(uint16_t(3 + labelSize));
    writeLabel(udp);
    udp.put(uint8_t(0));
    udp.put(uint8_t(1));
    udp.put(uint8_t(0x40));
}

ServiceNSECRecord::ServiceNSECRecord(Label label)
    : NSECRecord(std::move(label))
{
}

void
ServiceNSECRecord::writeSpecific(UDPExtended& udp) const
{
    uint16_t labelSize = label.writeSize();
    udp.put(uint16_t(7 + labelSize));
    writeLabel(udp);
    udp.put(uint8_t(0));
    udp.put(uint8_t(5));
    udp.put(uint8_t(0));
    udp.put(uint8_t(0));
    udp.put(uint8_t(0x80));
    udp.put(uint8_t(0));
    udp.put(uint8_t(0x40));
}

PTRRecord::PTRRecord(Label label, bool announce)
    : Record(std::move(label), PTR_TYPE, IN_CLASS, TTL_75MIN, announce)
{
}

void
PTRRecord::setTargetRecord(Record* target)
{
    targetRecord = target;
}

void
PTRRecord::writeSpecific(UDPExtended& udp) const
{
    udp.put(targetRecord->getLabel().writeSize());
    targetRecord->writeLabel(udp);
}

void
PTRRecord::matched(uint16_t qtype)
{
    targetRecord->matched(qtype); // Let target record handle which records are included
}

SRVRecord::SRVRecord(Label label, uint16_t _port, PTRRecord* ptr, ARecord* a)
    : Record(std::move(label), SRV_TYPE, IN_CLASS | CACHE_FLUSH, TTL_2MIN)
    , port(_port)
    , ptrRecord(ptr)
    , aRecord(a)
{
}

void
SRVRecord::writeSpecific(UDPExtended& udp) const
{
    uint16_t ptrLabelSize = aRecord->getLabel().writeSize();
    udp.put(uint16_t(6 + ptrLabelSize));
    udp.put(uint16_t(0));
    udp.put(uint16_t(0));
    udp.put(uint16_t(port));
    aRecord->writeLabel(udp);
}

TXTRecord::TXTRecord(Label label, std::vector<std::string> entries)
    : Record(std::move(label), TXT_TYPE, IN_CLASS | CACHE_FLUSH, TTL_75MIN)
    , data(std::move(entries))
{
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
