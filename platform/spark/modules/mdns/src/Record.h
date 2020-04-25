#pragma once

#include "UDPExtended.h"
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
    Label(std::string _name, std::shared_ptr<Record> _next = std::shared_ptr<Record>())
        : name(std::move(_name))
        , next(std::move(_next))
        , offset(0)
    {
        name.shrink_to_fit();
    }

    void writeFull(UDPExtended& udp) const;
    void writePtr(UDPExtended& udp) const;

    void write(UDPExtended& udp) const;
    void reset()
    {
        offset = 0;
    }
    std::string name;
    std::shared_ptr<Record> next;
    mutable uint16_t offset; // offset in current query or answer
};

class Record {

public:
    Record(Label label, uint16_t type, uint16_t cls, uint32_t ttl, bool announce = true);
    void announceRecord();

    void setAnswerRecord();

    bool isAnswerRecord();

    void setAdditionalRecord();

    bool isAdditionalRecord();

    void setKnownRecord();

    void write(UDPExtended& udp) const;

    void writeLabel(UDPExtended& udp) const;

    void writeLabelPtr(UDPExtended& udp) const;
    const Label& getLabel() const;
    void reset();
    void resetLabelOffset();

    bool match(std::vector<std::string>::const_iterator nameBegin,
               std::vector<std::string>::const_iterator nameEnd,
               uint16_t qtype, uint16_t qclass) const;

    virtual void matched(uint16_t qtype) = 0;

protected:
    virtual void writeSpecific(UDPExtended& udp) const = 0;

private:
    Label label;
    const uint16_t type;
    const uint16_t cls;
    const uint32_t ttl;
    const bool announce;
    bool answerRecord = false;
    bool additionalRecord = false;
    bool knownRecord = false;
};

class MetaRecord : public Record {

public:
    MetaRecord(Label label);
    void matched(uint16_t qtype) override final
    {
    }
    virtual void writeSpecific(UDPExtended& udp) const;
};

class NSECRecord : public Record {

public:
    // added to response by other records
    void matched(uint16_t qtype) override final
    {
    }
    NSECRecord(Label label);
};

class HostNSECRecord : public NSECRecord {

public:
    HostNSECRecord(Label label);

    virtual void writeSpecific(UDPExtended& udp) const;
};

class ARecord : public Record {

public:
    ARecord(Label label);
    virtual void writeSpecific(UDPExtended& udp) const;

    void matched(uint16_t qtype) override
    {
        switch (qtype) {
        case A_TYPE:
        case ANY_TYPE:
            this->setAnswerRecord();
            this->nsecRecord->setAdditionalRecord();
            break;

        default:
            this->nsecRecord->setAnswerRecord();
        }
    }
    std::shared_ptr<HostNSECRecord> nsecRecord;
};

class ServiceNSECRecord : public NSECRecord {

public:
    ServiceNSECRecord(Label label);

    virtual void writeSpecific(UDPExtended& udp) const;
};

class PTRRecord : public Record {

public:
    PTRRecord(Label label, bool meta = false);

    virtual void writeSpecific(UDPExtended& udp) const;
    void setTargetRecord(std::shared_ptr<Record> target);

    virtual void matched(uint16_t qtype) override final
    {
        if (qtype == PTR_TYPE || qtype == ANY_TYPE) {
            this->setAnswerRecord();
            targetRecord->setAdditionalRecord();
        }
    }

private:
    std::shared_ptr<Record> targetRecord;
};

class SRVRecord : public Record {

public:
    SRVRecord(Label label);

    virtual void writeSpecific(UDPExtended& udp) const;

    void setHostRecord(std::shared_ptr<Record> host);
    void setPort(uint16_t port);
    virtual void matched(uint16_t qtype) override final
    {
        hostRecord->setAnswerRecord();
        this->setAdditionalRecord();
    }

private:
    std::shared_ptr<Record> hostRecord;
    uint16_t port;
};

class TXTRecord : public Record {

public:
    TXTRecord(Label label, std::vector<std::string> entries);

    virtual void writeSpecific(UDPExtended& udp) const;
    virtual void matched(uint16_t qtype) override final
    {
        this->setAdditionalRecord();
    }

private:
    std::vector<std::string> data;
};
