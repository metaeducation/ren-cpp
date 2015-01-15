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

#include "values.hpp"

namespace ren {

namespace internal {

// This class is necessary because we can't define a Series::iterator class
// to wrap a Series inside of a Series--it would be an incomplete definition

class Series_ : public Value {
protected:
    friend class Value;
    Series_ (Dont) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isSeries(); }

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

    Value operator*() const;
    Value operator->() const; // see notes on Value::operator->

    void head();
    void tail();
};

} // end namespace internal


class Series : public ren::internal::Series_ {
protected:
    friend class Value;
    Series (Dont) : Series_ (Dont::Initialize) {}
    inline bool isValid() const { return isSeries(); }

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
        friend class Series;
        internal::Series_ state;
        iterator (internal::Series_ const & state) :
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
            { return not state.isSameAs(other.state); }

        Value operator * () const { return *state; }
        Value operator-> () const { return state.operator->(); }
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

    // Note: Rebol/Red use 1-based indexing with a "zero-hole" by default

    Value operator[](size_t index) const;
};

} // end namespace ren

#endif
