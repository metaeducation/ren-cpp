#ifndef RENCPP_RUNTIME_HPP
#define RENCPP_RUNTIME_HPP

//
// runtime.hpp
// This file is part of RenCpp
// Copyright (C) 2015-2018 HostileFork.com
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

#include <initializer_list>

#include "common.hpp"
#include "value.hpp"
#include "arrays.hpp"


namespace ren {


//
// BASE RUNTIME CLASS
//

//
// The runtime class is a singleton, and at the time of writing a bit of
// a hodge-podge as to whether it should be all static methods or
// what.  One reason it is a singleton came from performance and also
// having it "taken care of automatically" in the examples.  The downside
// of instantiating it automatically is that people can't derive from it;
// but the advantages of doing so are not clear when Engine() and Context()
// exist.  evaluate is static, and used by Engine and Context which are
// independent of either the Rebol or Red runtime instance (ren::runtime)
//
// Note operator overloads must be members, so being able to talk to
// a ren::runtime.foo as well as say ren::runtime(...) requires such
// methods to *not* be static, which curiously could make evaluate faster
// than the paren notation
//

class Runtime {
protected:
    friend class AnyArray;
    friend class AnyValue;
    friend class AnyString;
    friend class AnyWord;
    friend class Engine;

    static optional<AnyValue> evaluate(
        internal::Loadable const loadables[],
        size_t numLoadables,
        AnyContext const * contextPtr,
        Engine * engine
    );

public:
    static optional<AnyValue> evaluate(
        std::initializer_list<internal::Loadable> loadables,
        Engine * engine = nullptr
    ) {
        return evaluate(loadables.begin(), loadables.size(), nullptr, engine);
    }

    static optional<AnyValue> evaluate(
        std::initializer_list<internal::BlockLoadable<Block>> loadables,
        internal::ContextWrapper const & wrapper
    ) {
        return evaluate(
            loadables.begin(),
            loadables.size(),
            &wrapper.context,
            nullptr
        );
    }

    // Has ambiguity error from trying to turn the nullptr into a Loadable;
    // investigate what it is about the static method that has this problem

    /*template <typename... Ts>
    static inline AnyValue evaluate(Ts const &... args) {
        return evaluate({args...}, static_cast<Engine *>(nullptr));
    }*/

    template <typename... Ts>
    inline optional<AnyValue> operator()(Ts const &... args) const {
        return evaluate({args...}, static_cast<Engine *>(nullptr));
    }


    //
    // How to do a cancellation interface properly in threading environments
    // which may be making many requests?  This simple interface assumes one
    // evaluator thread to whom a cancel is being made from another thread
    // (that is not able to do evaluations at the same time)... because that
    // is what Rebol implemented.  A better interface is needed.
    //
    //     https://github.com/hostilefork/rencpp/issues/19
    //
public:
    virtual void cancel() = 0;

    virtual ~Runtime () {
    }
};


} // end namespace ren


#endif
