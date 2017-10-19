#ifndef RENCPP_STRINGS_HPP
#define RENCPP_STRINGS_HPP

//
// strings.hpp
// This file is part of RenCpp
// Copyright (C) 2015 HostileFork.com
//
// Licensed under the Boost License, Version 1.0 (the "License")
//
//      http://www.boost.org/LICENSE_1_0.txt
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied.  See the License for the specific language governing
// permissions and limitations under the License.
//
// See http://rencpp.hostilefork.com for more information on this project
//

#include "value.hpp"
#include "atoms.hpp"
#include "series.hpp"

namespace ren {

//
// ANYSTRING
//

class AnyString : public AnySeries
{
protected:
    friend class AnyValue;
    AnyString (Dont) noexcept : AnySeries (Dont::Initialize) {}
    static bool isValid(REBVAL const * cell);

    // Friending doesn't seem to be enough for gcc 4.6, see SO writeup:
    //    http://stackoverflow.com/questions/32983193/
public:
    friend class String;
    static void initString(REBVAL *cell);
    friend class Tag;
    static void initTag(REBVAL *cell);
    friend class Filename;
    static void initFilename(REBVAL *cell);

protected:
    AnyString(
        char const * cstr,
        internal::CellFunction cellfun,
        Engine * engine = nullptr
    );

    AnyString (
        std::string const & str,
        internal::CellFunction cellfun,
        Engine * engine = nullptr
    );

#if REN_CLASSLIB_QT == 1
    AnyString (
        QString const & str,
        internal::CellFunction cellfun,
        Engine * engine = nullptr
    );
#endif

public:
    // This lets you pass ren::AnyString to anything that expected a
    // std::string and do so implicitly, which may or may not be a great idea:
    //
    //     https://github.com/hostilefork/rencpp/issues/6

    operator std::string () const { return to_string(*this); }

#if REN_CLASSLIB_QT == 1
    operator QString () const { return to_QString(*this); }
#endif


public:
    class iterator {
        friend class AnyString;
        AnySeries state;
        iterator (AnySeries const & state) :
            state (state)
        {
        }

    public:
        iterator & operator++() {
            ++state;
            return *this;
        }

        iterator & operator--() {
            --state;
            return *this;
        }

        iterator operator++(int) {
            auto temp = *this;
            operator++();
            return temp;
        }

        iterator operator--(int) {
            auto temp = *this;
            operator--();
            return temp;
        }

        bool operator==(iterator const & other) const
            { return state.isSameAs(other.state); }
        bool operator!=(iterator const & other) const
            { return !state.isSameAs(other.state); }

        Character operator * () const {
            return static_cast<Character>(*state);
        }
        Character operator-> () const {
            return static_cast<Character>(state.operator->()); }
    };

    iterator begin() const {
        return iterator (*this);
    }

    iterator end() const {
        auto temp = *this;
        temp.tail();
        return iterator (temp);
    }

public:
    template <class T = std::string>
    T spellingOf() const {
        throw std::runtime_error("Unspecialized version of spellingOf called");
    }

    std::string spellingOf_STD() const;

#if REN_CLASSLIB_QT == 1
    QString spellingOf_QT() const;
#endif

    bool hasSpelling(char const * spelling) const {
        return spellingOf_STD() == spelling;
    }

    bool isEqualTo(char const * cstr) const {
        return static_cast<std::string>(*this) == cstr;
    }
};


// http://stackoverflow.com/a/3052604/211160

template<>
inline std::string AnyString::spellingOf<std::string>() const {
    return spellingOf_STD();
}

#if REN_CLASSLIB_QT == 1
template<>
inline QString AnyString::spellingOf<QString>() const {
    return spellingOf_QT();
}
#endif



//
// ANYSTRING_ SUBTYPE HELPER
//

namespace internal {

template <class C, CellFunction F>
class AnyString_ : public AnyString {
protected:
    friend class AnyValue;
    AnyString_ (Dont) noexcept : AnyString (Dont::Initialize) {}

public:
    explicit AnyString_ (Engine * engine = nullptr) :
        AnyString ("", F, engine)
    {
    }

    explicit AnyString_ (char const * cstr, Engine * engine = nullptr) :
        AnyString (cstr, F, engine)
    {
    }

    explicit AnyString_ (std::string const & str, Engine * engine = nullptr) :
        AnyString (str.c_str(), F, engine)
    {
    }

#if REN_CLASSLIB_QT == 1
    explicit AnyString_ (QString const & str, Engine * engine = nullptr) :
        AnyString (str, F, engine)
    {
    }
#endif

};

} // end namespace internal



//
// CONCRETE STRING TYPES
//

//
// For why these are classes and not typedefs:
//
//     https://github.com/hostilefork/rencpp/issues/49
//

class String
    : public internal::AnyString_<String, &AnyString::initString>
{
protected:
    static bool isValid(REBVAL const * cell);

protected:
    String (Dont) noexcept : AnyString_ (Dont::Initialize) {}
    friend class AnyValue;

    // Only String allows you to use implicit construction from string
    // classes, because trying otherwise for the other string classes
    // proved to be too accident-prone:
    //
    //     https://github.com/hostilefork/rencpp/issues/6
    //
    // We can't inherit the constructors when we are switching them from
    // implicit to explicit when there are default parameters.  (???)
    // They become ambiguous for some reason.  @Morwenn has pointed out that
    // constructor inheritance is tricky and often breaks down:
    //
    //    http://stackoverflow.com/questions/24912280/
    //
    // So we retype them here, minus the "explicit".  :-/

public:
    String (char const * cstr, Engine * engine = nullptr) :
        AnyString_ (cstr, engine)
    {
    }

    String (std::string const & str, Engine * engine = nullptr) :
        AnyString_ (str.c_str(), engine)
    {
    }

#if REN_CLASSLIB_QT == 1
    String (QString const & str, Engine * engine = nullptr) :
        AnyString_ (str, engine)
    {
    }
#endif
};


class Tag
    : public internal::AnyString_<Tag, &AnyString::initTag>
{
protected:
    static bool isValid(REBVAL const * cell);

public:
    friend class AnyValue;
    using AnyString_<Tag, &AnyString::initTag>::AnyString_;
};


class Filename :
    public internal::AnyString_<Filename, &AnyString::initFilename>
{
protected:
    static bool isValid(REBVAL const * cell);

public:
    friend class AnyValue;
    using AnyString_<Filename, &AnyString::initFilename>::AnyString_;
};

} // end namespace ren

#endif
