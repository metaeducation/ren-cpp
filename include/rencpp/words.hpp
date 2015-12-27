#ifndef RENCPP_WORDS_HPP
#define RENCPP_WORDS_HPP

//
// words.hpp
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

namespace ren {

//
// ANYWORD
//

class AnyWord : public AnyValue {
protected:
    friend class AnyValue;
    AnyWord (Dont) : AnyValue (Dont::Initialize) {}
    static bool isValid(RenCell const & cell);

    // Friending doesn't seem to be enough for gcc 4.6, see SO writeup:
    //    http://stackoverflow.com/questions/32983193/
public:
    friend class Word;
    static void initWord(RenCell & cell);
    friend class GetWord;
    static void initGetWord(RenCell & cell);
    friend class SetWord;
    static void initSetWord(RenCell & cell);
    friend class LitWord;
    static void initLitWord(RenCell & cell);
    friend class Refinement;
    static void initRefinement(RenCell & cell);

protected:
    explicit AnyWord (
        char const * cstr,
        internal::CellFunction cellfun,
        AnyContext const * context = nullptr,
        Engine * engine = nullptr
    );

#if REN_CLASSLIB_QT == 1
    explicit AnyWord (
        QString const & str,
        internal::CellFunction cellfun,
        AnyContext const * context = nullptr,
        Engine * engine = nullptr
    );
#endif

    // Copy from any other AnyWord, preserve binding but change type
    explicit AnyWord (AnyWord const & other, internal::CellFunction cellfun);


protected:
    explicit AnyWord (
        std::string const & str,
        internal::CellFunction cellfun,
        AnyContext const & context
    ) :
        AnyWord (str.c_str(), cellfun, &context, nullptr)
    {
    }

    explicit AnyWord (
        std::string const & str,
        internal::CellFunction cellfun,
        Engine * engine = nullptr
    ) :
        AnyWord (str.c_str(), cellfun, nullptr, engine)
    {
    }


#if REN_CLASSLIB_QT == 1
    explicit AnyWord (
        QString const & str,
        internal::CellFunction cellfun,
        AnyContext * context = nullptr
    );
#endif

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
};

// http://stackoverflow.com/a/3052604/211160

template<>
inline std::string AnyWord::spellingOf<std::string>() const {
    return spellingOf_STD();
}

#if REN_CLASSLIB_QT == 1
template<>
inline QString AnyWord::spellingOf<QString>() const {
    return spellingOf_QT();
}
#endif



//
// ANYWORD_ SUBTYPE HELPER
//

namespace internal {


template <class C, CellFunction F>
class AnyWord_ : public AnyWord {
protected:
    friend class AnyValue;
    AnyWord_ (Dont) : AnyWord (Dont::Initialize) {}

public:
    explicit AnyWord_ (char const * cstr, Engine * engine = nullptr) :
        AnyWord (cstr, F, nullptr, engine)
    {
    }

    explicit AnyWord_ (char const * cstr, AnyContext & context) :
        AnyWord (cstr, F, &context, nullptr)
    {
    }

    explicit AnyWord_ (std::string const & str, Engine * engine = nullptr) :
        AnyWord (str.c_str(), F, nullptr, engine)
    {
    }

    explicit AnyWord_ (std::string const & str, AnyContext & context) :
        AnyWord (str.c_str(), F, &context, nullptr)
    {
    }

#if REN_CLASSLIB_QT == 1
    explicit AnyWord_ (QString const & str, Engine * engine = nullptr) :
        AnyWord (str, F, nullptr, engine)
    {
    }
    explicit AnyWord_ (QString const & str, AnyContext & context) :
        AnyWord (str, F, &context, nullptr)
    {
    }

#endif

    template<class C2, CellFunction F2>
    explicit AnyWord_ (
        internal::AnyWord_<C2, F2> const & other
    ) :
        AnyWord (other, F)
    {
    }
};

} // end namespace internal



//
// CONCRETE WORD TYPES
//

//
// For why these are classes and not typedefs:
//
//     https://github.com/hostilefork/rencpp/issues/49
//

class Word
    : public internal::AnyWord_<Word, &AnyWord::initWord>
{
protected:
    static bool isValid(RenCell const & cell);

public:
    friend class AnyValue;
    using AnyWord_<Word, &AnyWord::initWord>::AnyWord_;

public:
    template <typename... Ts>
    inline optional<AnyValue> operator()(Ts &&... args) const {
        return apply(std::forward<Ts>(args)...);
    }
};


class SetWord
    : public internal::AnyWord_<SetWord, &AnyWord::initSetWord>
{
protected:
    static bool isValid(RenCell const & cell);

public:
    friend class AnyValue;
    using AnyWord_<SetWord, &AnyWord::initSetWord>::AnyWord_;

public:
    template <typename... Ts>
    inline AnyValue operator()(Ts &&... args) const {
        // An expression like `x: (...)` cannot evaluate to not being set,
        // because it would generate an error.
        return *apply(std::forward<Ts>(args)...);
    }
};


class GetWord
    : public internal::AnyWord_<GetWord, &AnyWord::initGetWord>
{
protected:
    static bool isValid(RenCell const & cell);

public:
    friend class AnyValue;
    using AnyWord_<GetWord, &AnyWord::initGetWord>::AnyWord_;

    // A get-word! does not take any parameters, but it's nice to have a
    // shorthand for treating it something like a zero-parameter function
public:
#ifdef REN_RUNTIME
    inline optional<AnyValue> operator()() const {
        return apply();
    }
#endif
};


class LitWord
    : public internal::AnyWord_<LitWord, &AnyWord::initLitWord>
{
protected:
    static bool isValid(RenCell const & cell);

public:
    friend class AnyValue;
    using AnyWord_<LitWord, &AnyWord::initLitWord>::AnyWord_;
};


// REFINEMENT! is targeted for being subsumed into PATH!, with an optimization
// that allows for 1-element paths to fit inside a REBSER with no data
// allocation.  They look like paths and should act like them!

class Refinement
    : public internal::AnyWord_<Refinement, &AnyWord::initRefinement>
{
protected:
    static bool isValid(RenCell const & cell);

public:
    friend class AnyValue;
    using AnyWord_<Refinement, &AnyWord::initRefinement>::AnyWord_;
};

} // end namespace ren

#endif
