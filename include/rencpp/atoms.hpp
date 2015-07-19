#ifndef RENCPP_INDIVISIBLES_HPP
#define RENCPP_INDIVISIBLES_HPP

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


//
// NONE AND UNSET CONSTRUCTION
//

//
// It makes sense to be able to create None and Unset *values*, which you
// can do with Value construction.  But why would you need a static type
// for the None and Unset *classes* in C++?  Do you need to statically
// check to make sure someone actually passed you a None?  :-/
//
// For completeness they are included for the moment, but are very unlikely
// to be useful in practice.  They really only make sense as instances of
// the Value base class.
//

constexpr Value::none_t none {Value::none_t::init{}};

constexpr Value::unset_t unset {Value::unset_t::init{}};

class Unset : public Value {
protected:
    friend class Value;
    Unset (Dont) noexcept : Value (Dont::Initialize) {}
    inline bool isValid() const { return isUnset(); }

public:
    Unset (Engine * engine = nullptr) : Value (unset, engine) {}
};


class None : public Value {
protected:
    friend class Value;
    None (Dont) noexcept : Value (Dont::Initialize) {}
    inline bool isValid() const { return isNone(); }

public:
    explicit None (Engine * engine = nullptr) : Value(none, engine) {}
};



//
// LOGIC
//

class Logic : public Value {
protected:
    friend class Value;
    Logic (Dont) noexcept : Value (Dont::Initialize) {}
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
        Value (b, engine)
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
        Value (static_cast<bool>(b), engine)
    {
    }

    operator bool () const;
};



//
// CHARACTER
//

class Character : public Value {
protected:
    friend class Value;
    friend class AnyString;
    Character (Dont) noexcept : Value (Dont::Initialize) {}
    inline bool isValid() const { return isCharacter(); }

public:
    Character (char c, Engine * engine = nullptr) :
        Value (c, engine)
    {
    }

    Character (wchar_t wc, Engine * engine = nullptr) :
        Value (wc, engine)
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

class Integer : public Value {
protected:
    friend class Value;
    Integer (Dont) noexcept : Value (Dont::Initialize) {}
    inline bool isValid() const { return isInteger(); }

public:
    Integer (int i, Engine * engine = nullptr) :
        Value (i, engine)
    {
    }

    operator int () const;
};



//
// FLOAT
//

class Float : public Value {
protected:
    friend class Value;
    Float (Dont) noexcept : Value (Dont::Initialize) {}
    inline bool isValid() const { return isFloat(); }

public:
    Float (double d, Engine * engine = nullptr) :
        Value (d, engine)
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

class Date : public Value {
protected:
    friend class Value;
    Date (Dont) noexcept : Value (Dont::Initialize) {}
    inline bool isValid() const { return isDate(); }

public:
    explicit Date (
        std::string const & str,
        Engine * engine = nullptr
    );
};


//
// IMAGE
//

//
// Rebol has a native IMAGE! type, which a few codecs have been written for
// to save and load.  We don't do much with it in RenCpp at this point
// unless you are building with the Qt classlib, in which case we need to
// go back and forth with a QImage.
//
// It's not clear if this should be in the standard RenCpp or if it belongs
// in some kind of extensions module.  In Rebol at least, the IMAGE! was
// available even in non-GUI builds.
//

class Image : public Value {
protected:
    friend class Value;
    Image (Dont) noexcept : Value (Dont::Initialize) {}
    inline bool isValid() const { return isImage(); }

public:
#if REN_CLASSLIB_QT == 1
    explicit Image (QImage const & image, Engine * engine = nullptr);
    operator QImage () const;
#endif
};


} // end namespace ren

#endif
