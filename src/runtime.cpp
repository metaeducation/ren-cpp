#include <cassert>
#include <vector>

#include "rencpp/runtime.hpp"
#include "rencpp/engine.hpp"
#include "rencpp/context.hpp"


namespace ren {

Printer print (std::cout);

Runtime::Runtime() {

}


std::string Runtime::form(Value const & value) {
    const size_t defaultBufLen = 100;

    std::vector<char> buffer {defaultBufLen};

    size_t numBytes;

    // Note .data() method is const on std::string.
    //    http://stackoverflow.com/questions/7518732/

    switch (
        RenFormAsUtf8(
            value.origin, &value.cell, buffer.data(), defaultBufLen, &numBytes
        ))
    {
        case REN_SUCCESS:
            assert(numBytes <= defaultBufLen);
            break;

        case REN_BUFFER_TOO_SMALL: {
            assert(numBytes > defaultBufLen);
            buffer.reserve(numBytes);

            size_t numBytesNew;
            if (
                RenFormAsUtf8(
                    value.origin,
                    &value.cell,
                    buffer.data(),
                    numBytes,
                    &numBytesNew
                ) != REN_SUCCESS
            ) {
                throw std::runtime_error("Expansion failure in RenFormAsUtf8");
            }
            assert(numBytesNew == numBytes);
            break;
        }

        default:
            throw std::runtime_error("Unknown error in RenFormAsUtf8");
    }

    return std::string(buffer.data(), numBytes);
}

}
