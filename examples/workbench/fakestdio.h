#ifndef FAKESTDIO_H
#define FAKESTDIO_H

//
// fakestdio.h
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


#include <iostream>
#include <cassert>

#include <QMutexLocker>

// This wound up seeming a lot more complicated than it needed to be.  See
// notes on the articles sourcing the information and a request for
// review here:
//
//     https://github.com/metaeducation/ren-garden/issues/2

class FakeStdoutResources {
public:
    FakeStdoutResources(size_t s) :
        buffer_ (s + 1) // for null terminator...
    {
    }

    ~FakeStdoutResources()
    {
    }

protected:
    std::vector<char> buffer_;
};



class FakeStdoutBuffer : protected FakeStdoutResources, public std::streambuf
{
private:
    RenConsole & mdi;

public:
    explicit FakeStdoutBuffer(RenConsole & mdi, std::size_t buff_sz = 256) :
        FakeStdoutResources (buff_sz),
        mdi(mdi)
    {
        // -1 makes authoring of overflow() easier, so it can put the character
        // into the buffer when it happens.
        setp(buffer_.data(), buffer_.data() + buff_sz - 1);
    }

protected:
    bool processAndFlush() {
        std::ptrdiff_t n = pptr() - pbase();
        pbump(-n);

        if (n == 0)
            return true;

        *(pbase() + n) = '\0';

        // BUG: If this is UTF8 encoded we might end up on half a character...
        // https://github.com/metaeducation/ren-garden/issues/1

        QMutexLocker locker {&mdi.modifyMutex};

        mdi.appendText(QString(pbase()));

        return true;
    }

    int_type overflow(int_type ch) {
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

    int sync() {
        return processAndFlush() ? 0 : -1;
    }

    // copy ctor and assignment not implemented;
    // copying not allowed
    FakeStdoutBuffer(const FakeStdoutBuffer &) = delete;
    FakeStdoutBuffer & operator= (const FakeStdoutBuffer &) = delete;
};



class FakeStdout : protected FakeStdoutBuffer, public std::ostream {
public:
    FakeStdout (RenConsole & mdi) :
        FakeStdoutBuffer (mdi),
        std::ostream (static_cast<FakeStdoutBuffer *>(this))
    {
    }

    ~FakeStdout () override
    {
        sync();
    }
};

#endif
