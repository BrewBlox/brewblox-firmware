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

//#define DOT '.'

//#define END_OF_NAME 0x0
//#define LABEL_POINTER 0xc0
//#define MAX_LABEL_SIZE 63
//#define INVALID_OFFSET -1

//#define UNKNOWN_NAME -1
//#define BUFFER_UNDERFLOW -2

class Record;

class Label {
public:
    Label(std::string _name, Record* _next)
        : name(std::move(_name))
        , next(_next)
        , offset(0)
    {
        //  If string has extra room allocated, free it. String will not grow
        name.shrink_to_fit();
    }

    // Label can have the exact same label as another record, in which case the own name is omitted
    Label(Record* _next)
        : next(_next)
        , offset(0)
    {
        //  If string has extra room allocated, free it. String will not grow
        name.shrink_to_fit();
    }

    Label(const Label&) = delete; // no copy
    Label(Label&&) = default;     // only move
    ~Label() = default;

    void writeFull(UDPExtended& udp) const;
    void writePtr(UDPExtended& udp) const;
    uint16_t writeSize() const;

    void write(UDPExtended& udp) const;
    void reset()
    {
        offset = 0;
    }
    std::string name;
    Record* next;
    mutable uint16_t offset; // offset in current query or answer
};

class Record {

public:
    Record(Label label, uint16_t type, uint16_t cls, uint32_t ttl, bool announce = true);
    Record(const Record&) = delete; // no copy
    Record(Record&&) = default;     // only move
    virtual ~Record() = default;
    void announceRecord();

    void setAnswerRecord();

    bool isAnswerRecord();

    void setAdditionalRecord();

    bool isAdditionalRecord();

    void setKnownRecord();

    void write(UDPExtended& udp) const;

    void writeLabel(UDPExtended& udp) const;

    const Label& getLabel() const;
    void reset();
    void resetLabelOffset();

    bool match(std::vector<std::string>::const_iterator nameBegin,
               std::vector<std::string>::const_iterator nameEnd,
               uint16_t qtype, uint16_t qclass) const;

    virtual void matched(uint16_t qtype) = 0;

protected:
    virtual void writeSpecific(UDPExtended& udp) const = 0;
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
        // meta records are only used for labels and should not be sent in a response
    }
    virtual void writeSpecific(UDPExtended& udp) const;
};

class HostNSECRecord;
class ARecord : public Record {
public:
    ARecord(Label label);
    virtual void writeSpecific(UDPExtended& udp) const;

    void matched(uint16_t qtype) override final;

    void setNsecRecord(HostNSECRecord* nsec)
    {
        nsecRecord = nsec;
    }

    HostNSECRecord* nsecRecord;
};

class NSECRecord : public Record {
public:
    NSECRecord(Label label);
};

class HostNSECRecord : public NSECRecord {

public:
    HostNSECRecord(Label label, ARecord* hostRecord);

    void matched(uint16_t qtype) override final
    {
        this->setAdditionalRecord();
    }

    virtual void writeSpecific(UDPExtended& udp) const;
};

class ServiceNSECRecord : public NSECRecord {

public:
    ServiceNSECRecord(Label label);

    void matched(uint16_t qtype) override final
    {
        // added to response by SRV record
    }

    virtual void writeSpecific(UDPExtended& udp) const;
};

class PTRRecord : public Record {

public:
    PTRRecord(Label label, bool announce = true);

    virtual void writeSpecific(UDPExtended& udp) const;
    void setTargetRecord(Record* target);

    virtual void matched(uint16_t qtype) override final;

private:
    Record* targetRecord;
};

class TXTRecord : public Record {

public:
    TXTRecord(Label label, std::vector<std::string> entries);

    virtual void writeSpecific(UDPExtended& udp) const;
    virtual void matched(uint16_t qtype) override final
    {
        // will be included by SRV record
    }

private:
    std::vector<std::string> data;
};

class SRVRecord : public Record {

public:
    SRVRecord(Label label, uint16_t port, PTRRecord* ptr, ARecord* a);

    virtual void writeSpecific(UDPExtended& udp) const;

    void setHostRecord(Record* host);
    void setPort(uint16_t port);
    virtual void matched(uint16_t qtype) override final
    {
        switch (qtype) {
        case PTR_TYPE:
        case ANY_TYPE:
            ptrRecord->setAnswerRecord();
            this->setAdditionalRecord();
            if (txtRecord) {
                txtRecord->setAdditionalRecord();
            }
            aRecord->setAdditionalRecord();
            aRecord->nsecRecord->setAdditionalRecord();
            break;
        case SRV_TYPE:
            this->setAnswerRecord();
            ptrRecord->setAdditionalRecord();
            if (txtRecord) {
                txtRecord->setAdditionalRecord();
            }
            aRecord->setAdditionalRecord();
            aRecord->nsecRecord->setAdditionalRecord();
            break;
        case TXT_TYPE:
            if (txtRecord) {
                txtRecord->setAnswerRecord();
                this->setAdditionalRecord();
                ptrRecord->setAdditionalRecord();
                aRecord->setAdditionalRecord();
                aRecord->nsecRecord->setAdditionalRecord();
            }
            break;
        default:
            if (nsecRecord) {
                nsecRecord->setAnswerRecord();
            }
        }
    }

    // TXT and NSEC record will use use this record for their label, so need to be set after construction
    void setTxtRecord(TXTRecord* txt)
    {
        this->txtRecord = txt;
    }

    void setNsecRecord(ServiceNSECRecord* nsec)
    {
        this->nsecRecord = nsec;
    }

private:
    uint16_t port;
    PTRRecord* ptrRecord;
    TXTRecord* txtRecord;
    ServiceNSECRecord* nsecRecord;
    ARecord* aRecord;
};