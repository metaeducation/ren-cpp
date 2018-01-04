//
// function.cpp
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
// permissions and limitations under the License..
//
// See http://rencpp.hostilefork.com for more information on this project
//

#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/function.hpp"

#include "common.hpp"

namespace ren {

namespace internal {
    std::mutex keepaliveMutex;
}


//
// TYPE DETECTION
//

bool Function::isValid(REBVAL const * cell) {
    return IS_FUNCTION(cell);
}


// This is the *actual* C function which is poked into the Rebol FUNCTION!,
// and gets dispatched to when that function is invoked.  The frame parameter
// contains all of the information about the call, such as the arguments and
// where to write the result value.
//
// It then calls through to the specialized templated function "shim" that is
// designed to unpack the parameters and create a call to the C++ function
// (or other "Callable" item that implements `operator()`).  This "magically"
// allows ren::Function to dispatch to arbitrary-arity functions (see notes
// in %function.hpp)
//
int32_t Function::Ren_Cpp_Dispatcher(struct Reb_Frame *f)
{
    REBARR *info = VAL_ARRAY(FUNC_BODY(f->original));

    RenEngineHandle engine;
    engine.data = cast(int,
        cast(REBUPT, VAL_HANDLE_POINTER(void, ARR_AT(info, 0)))
    );

    internal::RenShimPointer shim = cast(internal::RenShimPointer,
        VAL_HANDLE_POINTER(void, ARR_AT(info, 1))
    );

    // Note that this is a raw pointer to a C++ object.  The only code that
    // knows how to free it is the "freer" function (held in the handle's
    // code pointer), and this freeing occurs when the handle is GC'd
    //
    void *cppfun = VAL_HANDLE_POINTER(void, ARR_AT(info, 2));

    // To be idiomatic for C++, we want to be able to throw a ren::Error using
    // C++ exceptions from within a ren::Function.  Yet since the calling
    // code from the runtime is C and not C++, it cannot catch C++ exceptions.
    //
    // This means that if an exception were thrown during a function call
    // in the interpreter, it would jump completely past the interpreter
    // stack.  Any code that did a PUSH_TRAP() wouldn't have an opportunity
    // to run the corresponding code in case of an error, which frequently
    // does cleanup of things the system doesn't tidy up automatically.  So we
    // have to translate exceptions into fail() calls here.
    //
    // NOTE that fail() cannot be called when any non-"POD" (plain old data)
    // C++ objects are on the stack!  That means nothing with a constructor
    // or destructor.  It might appear to be *technically* legal to longjmp
    // from out of a "catch by reference" if you had no other C++ objects,
    // but this would run afoul of the destructor for the *temporary copy of
    // the object* which is used to provide that reference:
    //
    // https://stackoverflow.com/q/25723736/#comment80593686_25724185
    //
    // Hence the failing cases fall through.  If `what` is set, then the error
    // is initialized using that (UTF8 decoding can fail, as could memory 

    optional<std::exception> exception;

    try {
        // Note: RenCpp does not expose the ability to fail() directly; it
        // always goes through `throw`.  Hence, any failures that occur will
        // come from re-entering the interpreter via a call that RenCpp has
        // a guard on, that will only let exceptions bubble up.  Hence we
        // do not have to worry about a PUSH_TRAP here.
        //
        (*shim)(f->out, engine, cppfun, f);
        return R_OUT;
    }
    catch (bad_optional_access const &) {
        exception = std::runtime_error {
            "C++ code dereferenced empty optional, no further details"
            " (try setting a breakpoint in optional.hpp)"
        };
    }
    catch (Error const & e) {
        Move_Value(f->out, e.cell);
    }
    catch (optional<Error> const & e) {
        if (e == nullopt)
            exception = std::runtime_error {
                "ren::nullopt optional<Error> thrown from ren::Function"
            };
        else
            Move_Value(f->out, e->cell);
    }
    catch (AnyValue const & v) {
        //
        // In C++ `throw` is an error mechanism, and using it for general
        // non-localized control (as Rebol uses THROW) is considered abuse
        //
        if (!hasType<Error>(v))
            exception = std::runtime_error {
                "Non-ERROR! Value thrown from ren::Function"
            };
        else
            Move_Value(f->out, v.cell);
    }
    catch (optional<AnyValue> const & v) {
        if (!hasType<Error>(v))
            exception = std::runtime_error {
                "Non-ERROR! optional<AnyValue> thrown from ren::Function"
            };
        else
            Move_Value(f->out, v->cell);
    }
    catch (evaluation_error const & e) {
        //
        // Ren runtime code throws non-std::exception errors, and so
        // we tolerate C++ code doing the same if it's running as if it
        // were the runtime.  But we also allow the evaluation_error that
        // bubbles up through C++ to be extracted as an error to thread
        // back into the Ren runtime.  This way you can write a C++
        // routine that doesn't know if it's being called by other C++
        // code (hence having std::exception expectations) or just as
        // the implementation of a ren::Function

        Move_Value(f->out, e.error().cell);
    }
    catch (evaluation_throw const & t) {
        //
        // We have to fabricate a THROWN() name label with the actual
        // thrown value stored aside.  Making pointer reference
        // temporaries is needed to suppress a compiler warning:
        //
        //    http://stackoverflow.com/a/2281928/211160

        if (t.name())
            Move_Value(f->out, t.name()->cell);
        else
            Init_Void(f->out);

        if (t.value())
            CONVERT_NAME_TO_THROWN(f->out, t.value()->cell);
        else
            CONVERT_NAME_TO_THROWN(f->out, VOID_CELL);

        return R_OUT_IS_THROWN;
    }
    catch (load_error const & e) {
        Move_Value(f->out, e.error()->cell);
    }
    catch (evaluation_halt const &) {
        Move_Value(f->out, TASK_HALT_ERROR);
    }
    catch (std::exception const & e) {
        exception = e;
    }
    catch (...) { // Mystery error.  No string available.
        exception = std::runtime_error {
            "Exception from ren::Function not std::Exception or Error"
        };
    }

    if (exception) {
        //
        // A more sophisticated version of this might save a copy of the
        // exception in a handle, and parameterize the ERROR! with it...so it
        // could be rethrown in its original form *if* there's C++ code to
        // catch it.  But for now, just extract the string.

        DECLARE_LOCAL (what);
        Init_String(what, Make_UTF8_May_Fail(cb_cast(exception->what())));
        
        fail (::Error(RE_USER, what));
    }

    // We now should have all the C++ objects cleared from this stack,
    // so at least as far as THIS function is concerned, a longjmp
    // should be safe.
    //
    assert(IS_ERROR(f->out));
    fail (VAL_CONTEXT(f->out));
}


static void CppFunCleaner(const REBVAL *v) {
    assert(IS_HANDLE(v));
    
    auto freer = reinterpret_cast<internal::RenCppfunFreer>(
        VAL_HANDLE_LEN(v)
    );

    // The "freer" knows how to `delete` the specific C++ std::function subtype
    // that was being held onto by the handle's data pointer
    //
    (freer)(VAL_HANDLE_POINTER(void, v));
}


//
// FUNCTION FINALIZER FOR EXTENSION
//

void Function::finishInitSpecial(
    RenEngineHandle engine,
    Block const & spec,
    internal::RenShimPointer shim,
    void *cppfun, // a std::function object, with varying type signatures
    internal::RenCppfunFreer freer
) {
    // This must be true for the Ren_Cpp_Dispatcher to be binary-compatible
    // with the expectations of Rebol about function dispatchers.
    //
    static_assert(sizeof(REB_R) == sizeof(int32_t), "REB_R is not int32_t");

    REBFUN *fun = Make_Function(
        Make_Paramlist_Managed_May_Fail(spec.cell, MKF_KEYWORDS),
        reinterpret_cast<REBNAT>(&Ren_Cpp_Dispatcher), // REB_R not exported
        NULL, // no underlying function, this is fundamental,
        NULL // no exemplar
    );

    // The C++ function interface that is generated is typed specifically to
    // the parameters of the extension function.  This is what allows for
    // calling them in a way that looks like calling a C function ordinarily,
    // as well as getting type checking (int parameters for INTEGER!, strings
    // for STRING!, etc.)
    //
    // However, since all those functions have different type signatures, they
    // are different datatypes entirely.  They are funneled through a common
    // "unpacker" shim function, which is called by the dispatcher.

    REBARR *info = Make_Array(3);
    Init_Handle_Simple(
        Alloc_Tail_Array(info),
        cast(void*, cast(REBUPT, engine.data)), // data
        0 // len
    );
    Init_Handle_Simple(
        Alloc_Tail_Array(info),
        cast(void*, shim), // code
        0 // len
    );
    Init_Handle_Managed(
        Alloc_Tail_Array(info),
        cast(void*, cast(REBUPT, cppfun)), // data
        cast(REBUPT, freer), // hide type-aware freeing function in the "len"
        &CppFunCleaner // function to call when this handle gets GC'd
    );

    Init_Block(FUNC_BODY(fun), info);

    Move_Value(cell, FUNC_VALUE(fun));

    AnyValue::finishInit(engine);
}

} // end namespace ren
