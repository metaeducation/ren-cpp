//
// value.cpp
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

#include <ostream>
#include <vector>
#include <iostream>

#include "rencpp/value.hpp"
#include "rencpp/atoms.hpp"
#include "rencpp/context.hpp"
#include "rencpp/runtime.hpp"
#include "rencpp/error.hpp"
#include "rencpp/strings.hpp"

namespace ren {



// Even if asked not to initialize, we can't leave the type in a state where
// it cannot be safely freed.  Bad traversal pointers combined with bad data
// would be a problem.  Review this issue.

AnyValue::AnyValue (Dont) :
    next (nullptr),
    prev (nullptr)
{
}



AnyValue::operator bool() const {
    return not (isNone() or isFalse());
}


#ifdef REN_RUNTIME

//
// GENERALIZED APPLY
//

optional<AnyValue> AnyValue::apply_(
    internal::Loadable const loadables[],
    size_t numLoadables,
    Context const * contextPtr,
    Engine * engine
) const {
    AnyValue result (Dont::Initialize);

    Context context = contextPtr ? *contextPtr : Context::current(engine);

    if (constructOrApplyInitialize(
        context.getEngine(),
        &context,
        this, // no applicand
        loadables,
        numLoadables,
        nullptr, // don't construct
        &result // do apply
    )) {
        return result;
    }

    return nullopt;
}


optional<AnyValue> AnyValue::apply(
    std::initializer_list<internal::Loadable> loadables,
    internal::ContextWrapper const & wrapper
) const {
    // This one has to be in the implementation file because it appears in
    // AnyValue, using Loadable, which is derived from AnyValue...
    return apply_(
        loadables.begin(),
        loadables.size(),
        &wrapper.context,
        nullptr
    );
}


optional<AnyValue> AnyValue::apply(
    std::initializer_list<internal::Loadable> loadables,
    Engine * engine
) const {
    // This one has to be in the implementation file because it appears in
    // AnyValue, using Loadable, which is derived from AnyValue...
    return apply_(loadables.begin(), loadables.size(), nullptr, engine);
}

#endif


bool AnyValue::constructOrApplyInitialize(
    RenEngineHandle engine,
    Context const * context,
    AnyValue const * applicand,
    internal::Loadable const loadables[],
    size_t numLoadables,
    AnyValue * constructOutTypeIn,
    AnyValue * applyOut
) {
    AnyValue extraOut {AnyValue::Dont::Initialize};

    auto result = ::RenConstructOrApply(
        engine,
        &context->cell,
        &applicand->cell,
        numLoadables != 0 ? &loadables[0].cell : nullptr,
        numLoadables,
        sizeof(internal::Loadable),
        constructOutTypeIn ? &constructOutTypeIn->cell : nullptr,
        applyOut ? &applyOut->cell : nullptr,
        &extraOut.cell
    );

    switch (result) {
        case REN_SUCCESS:
            break;

        case REN_CONSTRUCT_ERROR:
            extraOut.finishInit(engine);
            assert(extraOut.isError());
            throw load_error {static_cast<Error>(extraOut)};

    #ifdef REN_RUNTIME
        case REN_APPLY_ERROR: {
            extraOut->finishInit(engine);
            assert(extraOut->isError());
            throw evaluation_error {static_cast<Error>(extraOut)};
        }

		case REN_EVALUATION_HALTED:
			throw evaluation_halt {};

		case REN_APPLY_THREW: {
            bool hasName = applyOut->tryFinishInit(engine);
            bool hasValue = extraOut->tryFinishInit(engine);
            throw evaluation_throw {
                hasValue ? optional<AnyValue>{extraOut} : nullopt,
                hasName ? optional<AnyValue>{*applyOut} : nullopt
            };
		}
    #endif

        default:
            throw std::runtime_error("Unknown error in RenConstructOrApply");
    }

    // It used to be required that we finalize the values before throwing
    // errors because (for instance) the tracking could be initialized.
    // That had to be changed because a Dont::Initialize was could construct
    // a type that could not survive an exception being thrown.  So we will
    // keep this finalization here just in case, because it should be safe now
    // to skip it in the case of an exception.

    if (constructOutTypeIn)
        constructOutTypeIn->finishInit(engine);

    if (applyOut) {
        // `tryFinishInit()` will give back false if the cell was not a value
        // (e.g. an "unset") which cues a caller requesting a value that they
        // should make a `nullopt` for the `optional<AnyValue>` instead of
        // considering the bits "good".
        return applyOut->tryFinishInit(engine);
    }

    // No apply requested, so same as not set
    return false;
}



//
// LOADABLE
//

internal::Loadable::Loadable (std::string const & source) :
    Loadable (Dont::Initialize)
{
    String value {source};
    cell = value.cell;
}

#if REN_CLASSLIB_QT == 1
internal::Loadable::Loadable (QString const & source) :
    Loadable (Dont::Initialize)
{
    String value {source};
    cell = value.cell;
}
#endif

} // end namespace ren
