//
// fakestdio.cpp
// This file is part of Ren Garden
// Copyright (C) 2015 MetÆducation
//
// Ren Garden is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Ren Garden is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Ren Garden.  If not, see <http://www.gnu.org/licenses/>.
//
// See http://ren-garden.metaeducation.com for more information on this project
//

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <ostream>
#include <streambuf>
#include <vector>

#include <QMutexLocker>

#include "fakestdio.h"
#include "renconsole.h"


// This wound up seeming a lot more complicated than it needed to be.  See
// notes on the articles sourcing the information and a request for
// review here:
//
//     https://github.com/metaeducation/ren-garden/issues/2


///
/// FAKE STANDARD OUTPUT
///



FakeStdoutBuffer::FakeStdoutBuffer(RenConsole & console, std::size_t buff_sz) :
    FakeStdoutResources (buff_sz),
    console (console)
{
    // -1 makes authoring of overflow() easier, so it can put the character
    // into the buffer when it happens.
    setp(buffer_.data(), buffer_.data() + buff_sz - 1);
}

bool FakeStdoutBuffer::processAndFlush() {
    std::ptrdiff_t n = pptr() - pbase();
    pbump(-n);

    if (n == 0)
        return true;

    *(pbase() + n) = '\0';

    // BUG: If this is UTF8 encoded we might end up on half a character...
    // https://github.com/metaeducation/ren-garden/issues/1

    QMutexLocker locker {&console.modifyMutex};

    if (not console.target) {
        console.appendText(QString(pbase()));
        return true;
    }

    if (console.target.isString()) {
        // dangerous call here, if append has any sort of output!
        // need versions of basic series routines that do not call out

        ren::runtime("append", console.target, pbase());
        return true;
    }

    assert(false);
    return false;
}

std::streambuf::int_type FakeStdoutBuffer::overflow(int_type ch) {
    if (ch != traits_type::eof())
    {
        assert(std::less_equal<char *>()(pptr(), epptr()));
        *pptr() = ch;
        pbump(1);

        if (processAndFlush())
            return ch;
    }

    return traits_type::eof();
}

int FakeStdoutBuffer::sync() {
    return processAndFlush() ? 0 : -1;
}






///
/// FAKE STANDARD INPUT
///

FakeStdinBuffer::FakeStdinBuffer (
    RenConsole & console,
    std::size_t buff_sz,
    std::size_t put_back
) :
    console (console),
    put_back_ (std::max(put_back, size_t(1))),
    buffer_ (std::max(buff_sz, put_back_) + put_back_)
{
    char *end = &buffer_.front() + buffer_.size();
    setg(end, end, end);
}


std::streambuf::int_type FakeStdinBuffer::underflow() {
    if (gptr() < egptr()) // buffer not exhausted
        return traits_type::to_int_type(*gptr());

    char *base = &buffer_.front();
    char *start = base;

    if (eback() == base) // true when this isn't the first fill
    {
        // Make arrangements for putback characters
        std::copy(egptr() - put_back_, egptr(), base);
        start += put_back_;
    }


    // start is now the start of the buffer, proper.

    int readCapacity = buffer_.size() - (start - base);

    // We need to lock the caller up until the console signals us
    // that it has read data.  We'll go by line for now.

    QMutexLocker lock {&console.inputMutex};
    emit requestInput();
    console.inputAvailable.wait(lock.mutex());

    std::size_t n = std::min(readCapacity, console.input.size());
    std::copy(console.input.data(), console.input.data() + n, start);
    console.input.right(console.input.size() - n);

    if (n == 0)
        return traits_type::eof();

    // Set buffer pointers
    setg(base, start, start + n);

    return traits_type::to_int_type(*gptr());
}
