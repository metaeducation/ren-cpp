#ifndef RENCPP_ATOMS_HPP
#define RENCPP_ATOMS_HPP

//
// atoms.hpp
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

//
// These classes inherit from Value, without inheriting Value's constructors.
// Since they inherited from Value you can pass them places that are
// expecting a Value.  However, a static_cast<> must be used if you want
// to go the other way (which may throw an exception on a bad cast).
//
// At minimum, each derived class must provide these methods:
//
// protected:
//    friend class Value;
//    Foo (Dont) : Value (Dont::Initialize) {}
//    inline bool isValid() const { return ...; }
//
// These are needed by the base class casting operator in Value, which has
// to be able to ask if the result of a cast is a valid instance for the
// specific or general type.  We're trying not to pay for virtual dispatch
// here, which is why it's so weird...and we don't want to expose this
// internal to users--which is why Value needs to be a friend.
//


namespace ren {


class Atom : public Value {
protected:
    friend class Value;
    Atom (Dont) noexcept : Value (Dont::Initialize) {}
    inline bool isValid() const { return isAtom(); }

public:
    // We need to inherit Value's constructors, as an Atom can be
    // initialized from any of the literal value initialization forms.
    using Value::Value;

public:
    // !!! Are there any common things that all atoms are able to do?  There
    // is some precedent in suggesting that any atom value in Rebol can
    // be compared against zero by setting all its bits to zero and setting
    // the header to the type.  That's questionable.
};



//
// NONE CONSTRUCTION
//

//
// It makes sense to be able to create a None *value*, which you can do with
// Value construction.  But why would you need a static type for the None
// class in C++?  Do you need to statically check to make sure someone
// actually passed you a None?  :-/
//
// (To put this another way: you would rarely make a parameter to a function
// in Rebol that could only be NONE!, as it carries no information.  Usually
// it would be part of a typeset such that NONE! was being expressed as an
// alternative to some other option.)
//
// For completeness a None `class` is included for the moment, but you would
// probably never find much of a reason to use it.
//

constexpr Value::none_t none {Value::none_t::init{}};

class None : public Atom {
protected:
    friend class Value;
    None (Dont) noexcept : Atom (Dont::Initialize) {}
    inline bool isValid() const { return isNone(); }

public:
    explicit None (Engine * engine = nullptr) : Atom (none, engine) {}
};



//
// LOGIC
//

class Logic : public Atom {
protected:
    friend class Value;
    Logic (Dont) noexcept : Atom (Dont::Initialize) {}
    inline bool isValid() const { return isLogic(); }

public:
    // Trick so that Logic can be implicitly constructed from bool but not
    // from a type implicitly convertible to bool (which requires explicit
    // construction):
    //
    //     https://github.com/hostilefork/rencpp/issues/24
    //

    template <typename T>
    Logic (
        const T & b,
        Engine * engine = nullptr,
        typename std::enable_if<
            std::is_same<T, bool>::value,
            void *
        >::type = nullptr
    ) :
        Atom (b, engine)
    {
    }

    template <typename T>
    explicit Logic (
        const T & b,
        Engine * engine = nullptr,
        typename std::enable_if<
            not std::is_same<T, bool>::value
            and std::is_convertible<T, bool>::value,
            void *
        >::type = nullptr
    ) :
        Atom (static_cast<bool>(b), engine)
    {
    }

    operator bool () const;
};



//
// CHARACTER
//

class Character : public Atom {
protected:
    friend class Value;
    friend class AnyString;
    Character (Dont) noexcept : Atom (Dont::Initialize) {}
    inline bool isValid() const { return isCharacter(); }

public:
    Character (char c, Engine * engine = nullptr) :
        Atom (c, engine)
    {
    }

    Character (wchar_t wc, Engine * engine = nullptr) :
        Atom (wc, engine)
    {
    }

    Character (int i, Engine * engine = nullptr);


    // Characters represent codepoints.  These conversion operators are for
    // convenience, but note that the char and wchar_t may throw if your
    // codepoint is too high for that; hence codepoint is explicitly an
    // unsigned long.

    explicit operator char () const;
    explicit operator wchar_t () const;

    unsigned long codepoint() const;

    // How to expose UTF8 encoding?

#if REN_CLASSLIB_QT == 1
    operator QChar () const;
#endif

public:
    // Because Characters have a unique iteration in AnyString types, the
    // -> override mentioned in Value applies to them too.  See note there.

    Character const * operator->() const { return this; }
    Character * operator->() { return this; }
};



//
// INTEGER
//

class Integer : public Atom {
protected:
    friend class Value;
    Integer (Dont) noexcept : Atom (Dont::Initialize) {}
    inline bool isValid() const { return isInteger(); }

public:
    Integer (int i, Engine * engine = nullptr) :
        Atom (i, engine)
    {
    }

    operator int () const;
};



//
// FLOAT
//

class Float : public Atom {
protected:
    friend class Value;
    Float (Dont) noexcept : Atom (Dont::Initialize) {}
    inline bool isValid() const { return isFloat(); }

public:
    Float (double d, Engine * engine = nullptr) :
        Atom (d, engine)
    {
    }

    operator double () const;
};



//
// DATE
//

//
// The C and C++ date and time routines all seem about counting since
// epoch or in dealing with time measuring of relative intervals on
// a CPU clock.  They are not very serviceable for working with arbitrary
// dates that could reach far back into the past and general calendaring
// math.  The closest analogue seems to be:
//
//     http://stackoverflow.com/questions/6730422/
//
// Hesitant to put in a dependency just for that.  If you want to extract
// information from the date you can do that with calls into the
// evaluator.
//

class Date : public Atom {
protected:
    friend class Value;
    Date (Dont) noexcept : Atom (Dont::Initialize) {}
    inline bool isValid() const { return isDate(); }

public:
    explicit Date (
        std::string const & str,
        Engine * engine = nullptr
    );
};


} // end namespace ren

#endif
