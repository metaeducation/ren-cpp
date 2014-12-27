#ifndef FAKESTDIO_H
#define FAKESTDIO_H

#include <iostream>
#include <cassert>

// http://www.angelikalanger.com/Articles/C++Report/IOStreamsDerivation/IOStreamsDerivation.html

class FakeStdoutResources {
public:
    FakeStdoutResources(size_t s) :
        buffer_ (s + 1)
    {
    }

    ~FakeStdoutResources()
    {
    }

protected:
    std::vector<char> buffer_;
};


// http://www.mr-edd.co.uk/blog/beginners_guide_streambuf

class FakeStdoutBuffer : protected FakeStdoutResources, public std::streambuf
{
private:
    RenConsole & mdi;

public:
    explicit FakeStdoutBuffer(RenConsole & mdi, std::size_t buff_sz = 256) :
        FakeStdoutResources (buff_sz),
        mdi(mdi)
    {
        // "-1 to make overflow() easier" (?)
        setp(buffer_.data(), buffer_.data() + buffer_.size() - 1);
    }

protected:
    bool processAndFlush() {
        std::ptrdiff_t n = pptr() - pbase();
        pbump(-n);

        if (n == 0)
            return true;

        *(pbase() + n) = '\0';
        // If this is UTF8 encoded we might end up on half a character...
        // should only flush on UTF8 boundaries
        QTextCursor cursor = QTextCursor (mdi.document());
        cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
        cursor.insertText(QString(pbase()));
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
