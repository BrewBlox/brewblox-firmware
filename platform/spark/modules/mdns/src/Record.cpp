#include "Record.h"
#include "spark_wiring_wifi.h"
#include <memory>
#include <string>

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
Record::writeLabel(Buffer& buffer) const
{
    label.write(buffer);
}

void
Record::write(Buffer& buffer) const
{
    writeLabel(buffer);
    buffer.writeUInt16(type);
    buffer.writeUInt16(cls);
    buffer.writeUInt32(ttl);
    writeSpecific(buffer);
}

void
Record::reset()
{
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
ARecord::writeSpecific(Buffer& buffer) const
{
    buffer.writeUInt16(4);
    IPAddress ip = spark::WiFi.localIP();
    for (int i = 0; i < IP_SIZE; i++) {
        buffer.writeUInt8(ip[i]);
    }
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
HostNSECRecord::writeSpecific(Buffer& buffer) const
{
    buffer.writeUInt16(5);
    writeLabel(buffer);
    buffer.writeUInt8(0);
    buffer.writeUInt8(1);
    buffer.writeUInt8(0x40);
}

InstanceNSECRecord::InstanceNSECRecord(Label label)
    : NSECRecord(std::move(label))
{
}

void
InstanceNSECRecord::writeSpecific(Buffer& buffer) const
{
    buffer.writeUInt16(9);
    writeLabel(buffer);
    buffer.writeUInt8(0);
    buffer.writeUInt8(5);
    buffer.writeUInt8(0);
    buffer.writeUInt8(0);
    buffer.writeUInt8(0x80);
    buffer.writeUInt8(0);
    buffer.writeUInt8(0x40);
}

PTRRecord::PTRRecord(Label label, Label target, bool meta)
    : Record(std::move(label), PTR_TYPE, IN_CLASS, TTL_75MIN, !meta)
    , targetLabel(std::move(target))
{
}

void
PTRRecord::writeSpecific(Buffer& buffer) const
{
    targetLabel.write(buffer);
}

SRVRecord::SRVRecord(Label label)
    : Record(std::move(label), SRV_TYPE, IN_CLASS | CACHE_FLUSH, TTL_2MIN)
{
}

void
SRVRecord::writeSpecific(Buffer& buffer) const
{
    uint16_t hostLabelSize = 0;
    if (hostRecord) {
        hostLabelSize = hostRecord->getLabel().name.size();
    }
    buffer.writeUInt16(6 + hostLabelSize);
    buffer.writeUInt16(0);
    buffer.writeUInt16(0);
    buffer.writeUInt16(port);
    if (hostLabelSize) {
        hostRecord->writeLabel(buffer);
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
TXTRecord::writeSpecific(Buffer& buffer) const
{
    uint16_t size = 0;

    for (const auto& s : data) {
        size += s.size() + 1;
    }

    buffer.writeUInt16(size);

    for (const auto& s : data) {
        uint8_t length = s.size();

        buffer.writeUInt8(length);

        for (const auto& c : s) {
            buffer.writeUInt8(reinterpret_cast<const uint8_t&>(c));
        }
    }
}
