#include "OneWire.h"
#include "OneWireAddress.h"
#include "OneWireDevice.h"
#include "OneWireScanningFactory.h"
#include "blox/TempSensorOneWireBlock.h"
#include "cbox/Object.h"
#include "cbox/ObjectContainer.h"
#include <memory>
#include <vector>

/**
 * Simple mock factory that emulates object discovery
 * Normally, a scanning factory will scan some type of communication bus
 * This factory just has a list of candidates. If a LongIntObject with that value doesn't exist, it creates it.
 */
class MockOneWireScanningFactory : public OneWireScanningFactory {
private:
    std::vector<OneWireAddress> adressesOnBus = {
        0x1111'1111'1111'1128, // DS18B20
        0x2222'2222'2222'2228, // DS18B20
        0x3333'3333'3333'3328, // DS18B20
        0x4444'4444'4444'443A, // DS2413
        0x5555'5555'5555'5529, // DS2408
    };
    std::vector<OneWireAddress>::const_iterator nextAddress;

public:
    MockOneWireScanningFactory(cbox::ObjectContainer& objects, OneWire& ow)
        : OneWireScanningFactory(objects, ow)
    {
        reset();
    }

    virtual ~MockOneWireScanningFactory() = default;

    virtual void reset() override final
    {
        nextAddress = adressesOnBus.cbegin();
    }

    virtual OneWireAddress next() override final
    {
        if (nextAddress != adressesOnBus.cend()) {
            return (*nextAddress++);
        }
        return 0;
    }
};
