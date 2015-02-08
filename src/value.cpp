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

#include "rencpp/value.hpp"
#include "rencpp/indivisibles.hpp"
#include "rencpp/context.hpp"
#include "rencpp/runtime.hpp"
#include "rencpp/error.hpp"
#include "rencpp/strings.hpp"

namespace ren {


bool Value::needsRefcount() const {
    return Runtime::needsRefcount(cell);
}



// Even if asked not to initialize, we can't leave the type in a state where
// it cannot be safely freed.  A bad refcount pointer combined with bad data
// would be a problem.  Review this issue.

Value::Value (Dont) :
    refcountPtr {nullptr}
{
}



Value::operator bool() const {
    if (isUnset())
        throw has_no_value();

    return not (isNone() or isFalse());
}


#ifdef REN_RUNTIME

///
/// GENERALIZED APPLY
///

Value Value::apply_(
    internal::Loadable const loadables[],
    size_t numLoadables,
    Context const * contextPtr,
    Engine * engine
) const {
    Value result (Dont::Initialize);

    Context context = contextPtr ? *contextPtr : Context::current(engine);

    constructOrApplyInitialize(
        context.getEngine(),
        &context,
        this, // no applicand
        loadables,
        numLoadables,
        nullptr, // don't construct
        &result // do apply
    );

    return result;
}


Value Value::apply(
    std::initializer_list<internal::Loadable> loadables,
    internal::ContextWrapper const & wrapper
) const {
    // This one has to be in the implementation file because it appears in
    // Value, using Loadable, which is derived from Value...
    return apply_(
        loadables.begin(),
        loadables.size(),
        &wrapper.context,
        nullptr
    );
}


Value Value::apply(
    std::initializer_list<internal::Loadable> loadables,
    Engine * engine
) const {
    // This one has to be in the implementation file because it appears in
    // Value, using Loadable, which is derived from Value...
    return apply_(loadables.begin(), loadables.size(), nullptr, engine);
}

#endif


void Value::constructOrApplyInitialize(
    RenEngineHandle engine,
    Context const * context,
    Value const * applicand,
    internal::Loadable const loadables[],
    size_t numLoadables,
    Value * constructOutTypeIn,
    Value * applyOut
) {
    Error errorOut {Value::Dont::Initialize};

    auto result = ::RenConstructOrApply(
        engine,
        &context->cell,
        &applicand->cell,
        numLoadables != 0 ? &loadables[0].cell : nullptr,
        numLoadables,
        sizeof(internal::Loadable),
        constructOutTypeIn ? &constructOutTypeIn->cell : nullptr,
        applyOut ? &applyOut->cell : nullptr,
        &errorOut.cell
    );

    switch (result) {
        case REN_SUCCESS:
            break;

        case REN_CONSTRUCT_ERROR:
            errorOut.finishInit(engine);
            throw load_error {errorOut};

    #ifdef REN_RUNTIME
        case REN_APPLY_ERROR:
            errorOut.finishInit(engine);
            throw evaluation_error {errorOut};

        case REN_EVALUATION_CANCELLED:
            throw evaluation_cancelled {};

        case REN_EVALUATION_EXITED: {
            // Special case: the error's cell isn't an error, but rather an
            // Integer (rare, which is why Error gets the in-place cell write)

            Integer status (Value::Dont::Initialize);
            status.cell = errorOut.cell;
            status.finishInit(engine);

            throw exit_command {status};
        }
    #endif

        default:
            throw std::runtime_error("Unknown error in RenConstructOrApply");
    }

    // It used to be required that we finalize the values before throwing
    // errors because (for instance) the refcount could be initialized.
    // That had to be changed because a Dont::Initialize was could construct
    // a type that could not survive an exception being thrown.  So we will
    // keep this finalization here just in case, because it should be safe now
    // to skip it in the case of an exception.

    if (constructOutTypeIn)
        constructOutTypeIn->finishInit(engine);

    if (applyOut)
        applyOut->finishInit(engine);
}



///
/// LOADABLE
///

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
