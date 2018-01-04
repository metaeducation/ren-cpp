#ifndef RENCPP_REBOL_HPP
#define RENCPP_REBOL_HPP

//
// rebol.hpp
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

#include <mutex>
#include "runtime.hpp"

#ifndef NDEBUG
#include <unordered_map>
#endif

namespace ren {

namespace internal {
    class RebolHooks;
}

// Not only is Runtime implemented on a per-binding basis
// (hence not requiring virtual methods) but you can add more
// specialized methods that are peculiar to just this runtime

class RebolRuntime : public Runtime {
private:
    AnyContext * defaultContext;
    bool initialized;

public:
    friend class internal::Loadable;

private:
    friend class internal::RebolHooks;

public: // !!! temporary--values need it
    bool lazyInitializeIfNecessary();


public:
    RebolRuntime (bool someExtraInitFlag);

    void doMagicOnlyRebolCanDo();

    void cancel() override;

    ~RebolRuntime() override;
};

extern RebolRuntime runtime;

namespace internal {
    // Placeholder for better solution: mutex for management of refcounts
    extern std::mutex refcountMutex;
}

} // end namespace ren


namespace rebol = ren;


#endif
