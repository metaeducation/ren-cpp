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

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <ostream>
#include <streambuf>
#include <vector>

#include <QObject>
#include <QMutexLocker>

// This wound up seeming a lot more complicated than it needed to be.  See
// notes on the articles sourcing the information and a request for
// review here:
//
//     https://github.com/metaeducation/ren-garden/issues/2


class RenConsole;

///
/// FAKE STANDARD OUTPUT
///

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
    RenConsole & console;

public:
    explicit FakeStdoutBuffer(RenConsole & console, std::size_t buff_sz = 256);

protected:
    bool processAndFlush();

    int_type overflow(int_type ch) override;

    int sync() override;
};


class FakeStdout : protected FakeStdoutBuffer, public std::ostream {
public:
    FakeStdout (RenConsole & console) :
        FakeStdoutBuffer (console),
        std::ostream (static_cast<FakeStdoutBuffer *>(this))
    {
    }

    ~FakeStdout () override
    {
        FakeStdoutBuffer::sync();
    }
};



///
/// FAKE STANDARD INPUT
///

class FakeStdinBuffer : public QObject, public std::streambuf
{
    Q_OBJECT

    RenConsole & console;
    const std::size_t put_back_;
    std::vector<char> buffer_;

public:
    explicit FakeStdinBuffer (
        RenConsole & console,
        std::size_t buff_sz = 256,
        std::size_t put_back = 8
    );

signals:
    void requestInput();

private:
    int_type underflow() override;
};


class FakeStdin : public FakeStdinBuffer, public std::istream {

public:
    FakeStdin (RenConsole & console) :
        FakeStdinBuffer (console),
        std::istream (static_cast<FakeStdinBuffer *>(this))
    {
    }

    ~FakeStdin () override
    {
        FakeStdinBuffer::sync();
    }
};


#endif
