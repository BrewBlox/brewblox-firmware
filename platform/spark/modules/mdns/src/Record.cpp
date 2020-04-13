#include "Record.h"

#include "Label.h"
#include "spark_wiring_wifi.h"

Record::Record(uint16_t type, uint16_t cls, uint32_t ttl, bool announce)
{
    this->type = type;
    this->cls = cls;
    this->ttl = ttl;
    this->announce = announce;
}

void
Record::setLabel(Label* label)
{
    this->label = label;
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
Record::write(Buffer* buffer)
{
    label->write(buffer);
    buffer->writeUInt16(type);
    buffer->writeUInt16(cls);
    buffer->writeUInt32(ttl);
    writeSpecific(buffer);
}

void
Record::reset()
{
    this->answerRecord = false;
    this->additionalRecord = false;
    this->knownRecord = false;
}

Label*
Record::getLabel()
{
    return label;
}

ARecord::ARecord()
    : Record(A_TYPE, IN_CLASS | CACHE_FLUSH, TTL_2MIN)
{
}

void
ARecord::writeSpecific(Buffer* buffer)
{
    buffer->writeUInt16(4);
    IPAddress ip = spark::WiFi.localIP();
    for (int i = 0; i < IP_SIZE; i++) {
        buffer->writeUInt8(ip[i]);
    }
}

NSECRecord::NSECRecord()
    : Record(NSEC_TYPE, IN_CLASS | CACHE_FLUSH, TTL_2MIN)
{
}

HostNSECRecord::HostNSECRecord()
    : NSECRecord()
{
}

void
HostNSECRecord::writeSpecific(Buffer* buffer)
{
    buffer->writeUInt16(5);
    getLabel()->write(buffer);
    buffer->writeUInt8(0);
    buffer->writeUInt8(1);
    buffer->writeUInt8(0x40);
}

InstanceNSECRecord::InstanceNSECRecord()
    : NSECRecord()
{
}

void
InstanceNSECRecord::writeSpecific(Buffer* buffer)
{
    buffer->writeUInt16(9);
    getLabel()->write(buffer);
    buffer->writeUInt8(0);
    buffer->writeUInt8(5);
    buffer->writeUInt8(0);
    buffer->writeUInt8(0);
    buffer->writeUInt8(0x80);
    buffer->writeUInt8(0);
    buffer->writeUInt8(0x40);
}

PTRRecord::PTRRecord(bool meta)
    : Record(PTR_TYPE, IN_CLASS, TTL_75MIN, !meta)
{
}

void
PTRRecord::writeSpecific(Buffer* buffer)
{
    buffer->writeUInt16(targetLabel->getWriteSize());
    targetLabel->write(buffer);
}

void
PTRRecord::setTargetLabel(Label* label)
{
    targetLabel = label;
}

SRVRecord::SRVRecord()
    : Record(SRV_TYPE, IN_CLASS | CACHE_FLUSH, TTL_2MIN)
{
}

void
SRVRecord::writeSpecific(Buffer* buffer)
{
    buffer->writeUInt16(6 + hostLabel->getWriteSize());
    buffer->writeUInt16(0);
    buffer->writeUInt16(0);
    buffer->writeUInt16(port);
    hostLabel->write(buffer);
}

void
SRVRecord::setHostLabel(Label* label)
{
    hostLabel = label;
}

void
SRVRecord::setPort(uint16_t port)
{
    this->port = port;
}

TXTRecord::TXTRecord()
    : Record(TXT_TYPE, IN_CLASS | CACHE_FLUSH, TTL_75MIN)
{
}

void
TXTRecord::addEntry(std::string key, std::string value)
{
    std::string entry = key;

    if (!value.empty()) {
        entry += '=';
        entry += value;
    }

    data.push_back(entry);
}

void
TXTRecord::writeSpecific(Buffer* buffer)
{
    uint16_t size = 0;

    for (const auto& s : data) {
        size += s.size() + 1;
    }

    buffer->writeUInt16(size);

    for (const auto& s : data) {
        uint8_t length = s.size();

        buffer->writeUInt8(length);

        for (const auto& c : s) {
            buffer->writeUInt8(reinterpret_cast<const uint8_t&>(c));
        }
    }
}
