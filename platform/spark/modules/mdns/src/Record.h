#pragma once

#include "Buffer.h"
#include <memory>
#include <string>
#include <vector>
#define IN_CLASS 1
#define CACHE_FLUSH 0x8000

#define A_TYPE 0x01
#define PTR_TYPE 0x0c
#define TXT_TYPE 0x10
#define AAAA_TYPE 0x1c
#define SRV_TYPE 0x21
#define NSEC_TYPE 0x2f

#define ANY_TYPE 0xFF

#define TTL_2MIN 120
#define TTL_75MIN 4500

#define IP_SIZE 4

#define DOT '.'

#define END_OF_NAME 0x0
#define LABEL_POINTER 0xc0
#define MAX_LABEL_SIZE 63
#define INVALID_OFFSET -1

#define UNKNOWN_NAME -1
#define BUFFER_UNDERFLOW -2

class Record;

class Label {
public:
    Label(std::string _name)
        : name(std::move(_name))
        , next()
    {
    }
    Label(std::string _name, const std::shared_ptr<Record>& _next)
        : name(std::move(_name))
        , next(_next)
    {
    }

    void write(Buffer& buffer) const
    {
        buffer.writeUInt8(name.size());
        for (const auto& c : name) {
            buffer.writeUInt8(reinterpret_cast<const uint8_t&>(c));
        }
    }

    std::string name;
    std::shared_ptr<Record> next;
};

class Record {

public:
    void announceRecord();

    void setAnswerRecord();

    bool isAnswerRecord();

    void setAdditionalRecord();

    bool isAdditionalRecord();

    void setKnownRecord();

    void write(Buffer& buffer) const;

    void writeLabel(Buffer& buffer) const;
    const Label& getLabel() const;
    void reset();

protected:
    Record(Label label, uint16_t type, uint16_t cls, uint32_t ttl, bool announce = true);

    virtual void writeSpecific(Buffer& buffer) const = 0;

private:
    const Label label;
    const uint16_t type;
    const uint16_t cls;
    const uint32_t ttl;
    const bool announce;
    bool answerRecord = false;
    bool additionalRecord = false;
    bool knownRecord = false;
};

class ARecord : public Record {

public:
    ARecord(Label label);

    virtual void writeSpecific(Buffer& buffer) const;
};

class NSECRecord : public Record {

public:
    NSECRecord(Label label);
};

class HostNSECRecord : public NSECRecord {

public:
    HostNSECRecord(Label label);

    virtual void writeSpecific(Buffer& buffer) const;
};

class InstanceNSECRecord : public NSECRecord {

public:
    InstanceNSECRecord(Label label);

    virtual void writeSpecific(Buffer& buffer) const;
};

class PTRRecord : public Record {

public:
    PTRRecord(Label label, bool meta = false);

    virtual void writeSpecific(Buffer& buffer) const;
    void setTargetRecord(std::shared_ptr<Record> target);

private:
    std::shared_ptr<Record> targetRecord;
};

class SRVRecord : public Record {

public:
    SRVRecord(Label label);

    virtual void writeSpecific(Buffer& buffer) const;

    void setHostRecord(std::shared_ptr<Record> host);
    void setPort(uint16_t port);

private:
    std::shared_ptr<Record> hostRecord;
    uint16_t port;
};

class TXTRecord : public Record {

public:
    TXTRecord(Label label);

    virtual void writeSpecific(Buffer& buffer) const;

    void addEntry(std::string key, std::string value = "");

private:
    std::vector<std::string> data;
};
