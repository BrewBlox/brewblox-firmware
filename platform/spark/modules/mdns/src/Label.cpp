#include "Label.h"
#include "stdlib.h"

Label::Label(std::string name_, Label* nextLabel_, bool caseSensitive_)
    : data(std::move(name_))
    , caseSensitive(caseSensitive_)
    , nextLabel(nextLabel_)
{
}

uint8_t
Label::getWriteSize()
{
    Label* label = this;
    uint8_t size = 0;

    while (label != nullptr) {
        if (label->writeOffset == INVALID_OFFSET) { // new label, write full name
            size += label->data.size() + 1;
            label = label->nextLabel;
        } else { // pointer to previously written location can be used
            size += 2;
            label = nullptr;
        }
    }

    return size;
}

void
Label::write(Buffer* buffer)
{
    Label* label = this;

    while (label) {
        if (label->writeOffset == INVALID_OFFSET) {
            label->writeOffset = buffer->getOffset();

            buffer->writeUInt8(label->data.size());
            for (const auto& c : label->data) {
                buffer->writeUInt8(reinterpret_cast<const uint8_t&>(c));
            }
            label = label->nextLabel;
        } else {
            buffer->writeUInt16((LABEL_POINTER << 8) | label->writeOffset);
            label = nullptr;
        }
    }
}

void
Label::reset()
{
    Label* label = this;

    while (label != NULL) {
        label->writeOffset = INVALID_OFFSET;

        label = label->nextLabel;
    }
}

Label::Reader::Reader(Buffer* buffer)
{
    this->buffer = buffer;
}

bool
Label::Reader::hasNext()
{
    return c != END_OF_NAME && buffer->available() > 0;
}

uint8_t
Label::Reader::next()
{
    c = buffer->readUInt8();

    while ((c & LABEL_POINTER) == LABEL_POINTER) {
        if (buffer->available() > 0) {
            uint8_t c2 = buffer->readUInt8();

            uint16_t pointerOffset = ((c & ~LABEL_POINTER) << 8) | c2;

            buffer->mark();

            buffer->setOffset(pointerOffset);

            c = buffer->readUInt8();
        }
    }

    return c;
}

bool
Label::Reader::endOfName()
{
    return c == END_OF_NAME;
}

Label::Iterator::Iterator(Label* label)
{
    this->label = label;
    this->startLabel = label;
    this->size = label->data.size();
}

bool
Label::Iterator::match(uint8_t c)
{
    if (matches) {
        while (offset > size && label) {
            label = label->nextLabel;
            size = label->data.size();
            offset = 0;
        }

        matches = offset <= size && label && (label->data[offset] == c || (!label->caseSensitive && equalsIgnoreCase(c)));

        offset++;
    }

    return matches;
}

bool
Label::Iterator::matched()
{
    return matches;
}

bool
Label::Iterator::equalsIgnoreCase(uint8_t c)
{
    return (c >= 'a' && c <= 'z' && label->data[offset] == c - 32) || (c >= 'A' && c <= 'Z' && label->data[offset] == c + 32);
}

Label*
Label::Iterator::getStartLabel()
{
    return startLabel;
}

Label*
Label::Matcher::match(std::map<std::string, Label*> labels, Buffer* buffer)
{
    Iterator* iterators[labels.size()];

    std::map<std::string, Label*>::const_iterator i;
    uint8_t idx = 0;

    for (i = labels.begin(); i != labels.end(); ++i) {
        iterators[idx++] = new Iterator(i->second);
    }

    Reader* reader = new Reader(buffer);

    while (reader->hasNext()) {
        uint8_t size = reader->next();

        uint8_t idx = 0;

        for (uint8_t i = 0; i < labels.size(); i++) {
            iterators[i]->match(size);
        }

        while (idx < size && reader->hasNext()) {
            uint8_t c = reader->next();

            for (uint8_t i = 0; i < labels.size(); i++) {
                iterators[i]->match(c);
            }

            idx++;
        }
    }

    buffer->reset();

    Label* label = NULL;

    if (reader->endOfName()) {
        uint8_t idx = 0;

        while (label == NULL && idx < labels.size()) {
            if (iterators[idx]->matched()) {
                label = iterators[idx]->getStartLabel();
            }

            idx++;
        }
    }

    for (uint8_t i = 0; i < labels.size(); i++) {
        delete iterators[i];
    }

    delete reader;

    return label;
}

void
Label::matched(uint16_t type, uint16_t cls)
{
}

HostLabel::HostLabel(Record* aRecord, Record* nsecRecord, std::string name, Label* nextLabel, bool caseSensitive)
    : Label(name, nextLabel, caseSensitive)
{
    this->aRecord = aRecord;
    this->nsecRecord = nsecRecord;
}

void
HostLabel::matched(uint16_t type, uint16_t cls)
{
    switch (type) {
    case A_TYPE:
    case ANY_TYPE:
        aRecord->setAnswerRecord();
        nsecRecord->setAdditionalRecord();
        break;

    default:
        nsecRecord->setAnswerRecord();
    }
}

ServiceLabel::ServiceLabel(Record* aRecord, std::string name, Label* nextLabel, bool caseSensitive)
    : Label(name, nextLabel, caseSensitive)
{
    this->aRecord = aRecord;
}

void
ServiceLabel::addInstance(Record* ptrRecord, Record* srvRecord, Record* txtRecord)
{
    ptrRecords.push_back(ptrRecord);
    srvRecords.push_back(srvRecord);
    txtRecords.push_back(txtRecord);
}

void
ServiceLabel::matched(uint16_t type, uint16_t cls)
{
    switch (type) {
    case PTR_TYPE:
    case ANY_TYPE:
        for (std::vector<Record*>::const_iterator i = ptrRecords.begin(); i != ptrRecords.end(); ++i) {
            (*i)->setAnswerRecord();
        }
        for (std::vector<Record*>::const_iterator i = srvRecords.begin(); i != srvRecords.end(); ++i) {
            (*i)->setAdditionalRecord();
        }
        for (std::vector<Record*>::const_iterator i = txtRecords.begin(); i != txtRecords.end(); ++i) {
            (*i)->setAdditionalRecord();
        }
        aRecord->setAdditionalRecord();
        break;
    }
}

InstanceLabel::InstanceLabel(Record* srvRecord, Record* txtRecord, Record* nsecRecord, Record* aRecord, std::string name, Label* nextLabel, bool caseSensitive)
    : Label(name, nextLabel, caseSensitive)
{
    this->srvRecord = srvRecord;
    this->txtRecord = txtRecord;
    this->nsecRecord = nsecRecord;
    this->aRecord = aRecord;
}

void
InstanceLabel::matched(uint16_t type, uint16_t cls)
{
    switch (type) {
    case SRV_TYPE:
        srvRecord->setAnswerRecord();
        txtRecord->setAdditionalRecord();
        nsecRecord->setAdditionalRecord();
        aRecord->setAdditionalRecord();
        break;

    case TXT_TYPE:
        txtRecord->setAnswerRecord();
        srvRecord->setAdditionalRecord();
        nsecRecord->setAdditionalRecord();
        aRecord->setAdditionalRecord();
        break;

    case ANY_TYPE:
        srvRecord->setAnswerRecord();
        txtRecord->setAnswerRecord();
        nsecRecord->setAdditionalRecord();
        aRecord->setAdditionalRecord();
        break;

    default:
        nsecRecord->setAnswerRecord();
    }
}

MetaLabel::MetaLabel(std::string name, Label* nextLabel)
    : Label(name, nextLabel)
{
    // Do nothing
}

void
MetaLabel::addService(Record* ptrRecord)
{
    records.push_back(ptrRecord);
}

void
MetaLabel::matched(uint16_t type, uint16_t cls)
{
    switch (type) {
    case PTR_TYPE:
    case ANY_TYPE:
        for (std::vector<Record*>::const_iterator i = this->records.begin(); i != this->records.end(); ++i) {
            (*i)->setAnswerRecord();
        }
        break;
    }
}
