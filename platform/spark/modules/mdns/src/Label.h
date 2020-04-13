#ifndef _INCL_LABEL
#define _INCL_LABEL

#include "Buffer.h"
#include "Record.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

#define DOT '.'

#define END_OF_NAME 0x0
#define LABEL_POINTER 0xc0
#define MAX_LABEL_SIZE 63
#define INVALID_OFFSET -1

#define UNKNOWN_NAME -1
#define BUFFER_UNDERFLOW -2

class Label {
private:
    class Iterator;

public:
    class Matcher {
    public:
        Label* match(std::map<std::string, Label*> labels, Buffer& buffer);
    };

    Label(std::string name, Label* nextLabel = nullptr, bool caseSensitive = false);

    uint8_t getWriteSize();

    void write(Buffer& buffer);

    virtual void matched(uint16_t type, uint16_t cls);

    void reset();

private:
    class Reader {
    public:
        Reader(Buffer& buffer);

        bool hasNext();

        uint8_t next();

        bool endOfName();

    private:
        Buffer& buffer;
        uint8_t c = 1;
    };

    class Iterator {
    public:
        Iterator(Label* label);

        bool match(uint8_t c);

        bool matched();

        Label* getStartLabel();

    private:
        Label* startLabel;
        Label* label;
        uint8_t size;
        uint8_t offset = 0;
        bool matches = true;

        bool equalsIgnoreCase(uint8_t c);
    };

    std::string data;
    bool caseSensitive;
    Label* nextLabel;
    int16_t writeOffset = INVALID_OFFSET;
};

class HostLabel : public Label {

public:
    HostLabel(std::shared_ptr<ARecord> aRecord, std::shared_ptr<NSECRecord> nsecRecord, std::string name, Label* nextLabel = nullptr, bool caseSensitive = false);

    virtual void matched(uint16_t type, uint16_t cls);

private:
    std::shared_ptr<Record> aRecord;
    std::shared_ptr<Record> nsecRecord;
};

class ServiceLabel : public Label {

public:
    ServiceLabel(std::shared_ptr<ARecord> aRecord, std::string name, Label* nextLabel = nullptr, bool caseSensitive = false);

    void addInstance(std::shared_ptr<PTRRecord> ptrRecord, std::shared_ptr<SRVRecord> srvRecord, std::shared_ptr<TXTRecord> txtRecord);

    virtual void matched(uint16_t type, uint16_t cls);

private:
    std::shared_ptr<Record> aRecord;
    std::vector<std::shared_ptr<Record>> ptrRecords;
    std::vector<std::shared_ptr<Record>> srvRecords;
    std::vector<std::shared_ptr<Record>> txtRecords;
};

class InstanceLabel : public Label {

public:
    InstanceLabel(std::shared_ptr<SRVRecord> srvRecord,
                  std::shared_ptr<TXTRecord> txtRecord,
                  std::shared_ptr<NSECRecord> nsecRecord,
                  std::shared_ptr<ARecord> aRecord,
                  std::string name,
                  Label* nextLabel = nullptr,
                  bool caseSensitive = false);

    virtual void matched(uint16_t type, uint16_t cls);

private:
    std::shared_ptr<SRVRecord> srvRecord;
    std::shared_ptr<TXTRecord> txtRecord;
    std::shared_ptr<NSECRecord> nsecRecord;
    std::shared_ptr<ARecord> aRecord;
};

class MetaLabel : public Label {

public:
    MetaLabel(std::string name, Label* nextLabel);

    void addService(std::shared_ptr<Record> ptrRecord);

    virtual void matched(uint16_t type, uint16_t cls);

private:
    std::vector<std::shared_ptr<Record>> records;
};

#endif
