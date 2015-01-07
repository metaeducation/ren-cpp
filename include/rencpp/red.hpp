#ifndef RENCPP_RED_HPP
#define RENCPP_RED_HPP

//
// red.hpp
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

#include "ren.hpp"
#include "runtime.hpp"

namespace ren {

// Not only is Runtime implemented on a per-binding basis
// (hence not requiring virtual methods) but you can add more
// specialized methods that are peculiar to just this runtime

class RedRuntime : public Runtime {
public:
    friend class internal::Loadable;

    //
    // These values mirror the numbers in red/runtime/macros.reds
    //
    // What's very cool is that the binding is abstracted so you can plug in a
    // different model of what a cell is, or what values mean.  Even the
    // 0-based trick in the Loadable isn't baked in...
    //
    enum DatatypeID {
        TYPE_ALIEN, // TYPE_VALUE in macros.reds
        TYPE_DATATYPE,
        TYPE_UNSET,
        TYPE_NONE,
        TYPE_LOGIC,
        TYPE_BLOCK,
        TYPE_STRING,
        TYPE_INTEGER,
        TYPE_SYMBOL,
        TYPE_CONTEXT,
        TYPE_WORD,
        TYPE_SET_WORD,
        TYPE_LIT_WORD,
        TYPE_GET_WORD,
        TYPE_REFINEMENT,
        TYPE_CHAR,
        TYPE_NATIVE,
        TYPE_ACTION,
        TYPE_OP,
        TYPE_FUNCTION,
        TYPE_PATH,
        TYPE_LIT_PATH,
        TYPE_SET_PATH,
        TYPE_GET_PATH,
        TYPE_PAREN,
        TYPE_ROUTINE,
        TYPE_ISSUE,
        TYPE_FILE,
        TYPE_URL,
        TYPE_BITSET,
        TYPE_POINT,
        TYPE_OBJECT,
        TYPE_FLOAT,
        TYPE_BINARY,

        TYPE_TYPESET,
        TYPE_ERROR,

        TYPE_CLOSURE,

        TYPE_PORT
    };

    inline static RedCell makeCell4(
        int32_t const & header,
        int32_t const & data1,
        int32_t const & data2,
        int32_t const & data3
    ) {
        RedCell result;
        result.header = header;
        result.data1 = data1;
        result.s.data2 = data2;
        result.s.data3 = data3;
        return result; // Move semantics, no more cost than {a, b, c, d}
    }

    inline static RedCell makeCell3(
        int32_t const & header,
        int32_t const & data1,
        double const & dataD
    ) {
        // checked only at compile time
        static_assert(
            sizeof(double) == sizeof(int32_t) * 2,
            "Double is not exactly the size of two int32s"
        );

        RedCell result;
        result.header = header;
        result.data1 = data1;
        result.dataD = dataD;
        return result; // Again, move semantics
    }

public:
    RedRuntime (bool someExtraInitFlag);

    static DatatypeID getDatatypeID(RedCell const & cell);
    inline static DatatypeID getDatatypeID(Value const & value) {
        return getDatatypeID(getCell(value));
    }

    void doMagicOnlyRedCanDo();

    ~RedRuntime() override;
};

extern RedRuntime runtime;

} // end namespace ren

#ifndef NDEBUG
std::ostream & operator<<(std::ostream & os, ren::RedRuntime::DatatypeID id);
#endif

namespace red = ren;

#endif
