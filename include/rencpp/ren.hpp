#ifndef RENCPP_REN_HPP
#define RENCPP_REN_HPP

//
// ren.hpp
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

//
// This is currently the main include file for using Ren in projects.  At
// one time, there was an attempt to make "ren.hpp" only speak about data
// and "runtime.hpp" provide the runtime services, with the additional
// ability to include either "red.hpp" or "rebol.hpp" to get specific
// features.  However, which runtime you control is currently done by
// the build... not the headers, and you just include ren.hpp.
//

#include "value.hpp"
#include "atoms.hpp"
#include "words.hpp"
#include "series.hpp"
#include "strings.hpp"
#include "blocks.hpp"
#include "error.hpp"
#include "function.hpp"
#include "runtime.hpp"
#include "engine.hpp"
#include "context.hpp"

// !!! Even non-GUI builds want to be able to process images.  Yet this
// probably should be in the category of things done with a plug-in,
// (once plug-ins can act like fairly natural types)
#include "image.hpp"


//
// INCLUDE REBOL OR RED RUNTIME INSTANCE
//

//
// They will define an object derived from ren::Runtime, named ren::runtime
//

#ifndef REN_RUNTIME

#include "rencpp/rebol.hpp" // cheating for now, no C++ version...

#elif defined(REN_RUNTIME) and (REN_RUNTIME == REN_RUNTIME_RED)

#include "rencpp/red.hpp"

#elif defined(REN_RUNTIME) and (REN_RUNTIME == REN_RUNTIME_REBOL)

#include "rencpp/rebol.hpp"

#else

static_assert(false, "Unsupported runtime defined");

#endif


//
// HELPER TOOLS
//

//
// Things like the variadic print.  These perhaps should not be automatically
// included, but fun to do so for now.  Consider them a grab bag of ideas...
// like premade parse helper classes should go in there.
//

#include "helpers.hpp"

#endif
