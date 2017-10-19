#ifndef RENCPP_SERIES_HPP
#define RENCPP_SERIES_HPP

//
// series.hpp
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

namespace internal {

// This class is necessary because we can't define a Series::iterator class
// to wrap a Series inside of a Series--it would be an incomplete definition

class AnySeries_ : public AnyValue {
protected:
    friend class AnyValue;
    AnySeries_ (Dont) noexcept : AnyValue (Dont::Initialize) {}
    static bool isValid(REBVAL const * cell);

public:
    // We don't return values here because that would leak the internal
    // types.  It's technically possible to write a variant of things like
    // head() and tail() for every type but reinventing Rebol/Red is not
    // really the point.  Mutating types as they are vs. returning a new
    // base type could be a good option for working in the C++ world, if
    // iterators are available for things like enumerating.

    void operator++();
    void operator--();

    void operator++(int);
    void operator--(int);

    AnyValue operator*() const;
    AnyValue operator->() const; // see notes on AnyValue::operator->

    void head();
    void tail();
};

} // end namespace internal


class AnySeries : public ren::internal::AnySeries_ {
protected:
    friend class AnyValue;
    AnySeries (Dont) noexcept : AnySeries_ (Dont::Initialize) {}
    static bool isValid(REBVAL const * cell);

    //
    // If you wonder why C++ would need a separate iterator type for a Series
    // instead of doing as Rebol does and just using a Series, see this:
    //
    //    https://github.com/hostilefork/rencpp/issues/25
    //
    // The series thus functions as the state, but is a separate type that
    // has to be wrapped up.
public:
    class iterator {
        friend class AnySeries;
        internal::AnySeries_ state;
        iterator (internal::AnySeries_ const & state) :
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

        AnyValue operator * () const { return *state; }
        AnyValue operator-> () const { return state.operator->(); }
    };

    iterator begin() const {
        return iterator (*this);
    }

    iterator end() const {
        auto temp = *this;
        temp.tail(); // see remarks on tail modifying vs. returning a value
        return iterator (temp);
    }

    size_t length() const;

    bool isEmpty() const { return length() == 0; }


    // All series can be accessed by index, but there is no general rule
    // about any other way to index into them.  But if you have a base
    // class series and don't know what it is, you need to be able to use
    // the broader indexing method on it.  So this takes any AnyValue, with
    // risk of giving you a runtime error for a bad combination.

    // Note: Rebol/Red use 1-based indexing with a "zero-hole" by default

    AnyValue operator[](AnyValue const & index) const;
};

} // end namespace ren

#endif
