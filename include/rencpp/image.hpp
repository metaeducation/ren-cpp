#ifndef RENCPP_IMAGE_HPP
#define RENCPP_IMAGE_HPP

//
// image.hpp
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
// IMAGE
//

//
// Rebol has a native IMAGE! type, which a few codecs have been written for
// to save and load.  We don't do much with it in RenCpp at this point
// unless you are building with the Qt classlib, in which case we need to
// go back and forth with a QImage.
//
// It's not clear if this should be in the standard RenCpp or if it belongs
// in some kind of extensions module.  In Rebol at least, the IMAGE! was
// available even in non-GUI builds.  It seems that the "codecs" and
// "extensions" do need to add new types that behave a bit as if they were
// built in, while perhaps having a more limited range of evaluator
// behavior.  This is under review.
//
// !!! Is an image an "atom"?  It is a sort of container, in the same sense
// that a string is.  We'll keep it a AnyValue for now.

class Image : public AnyValue {
protected:
    friend class AnyValue;
    Image (Dont) noexcept : AnyValue (Dont::Initialize) {}
    static bool isValid(RenCell const * cell);

public:
#if REN_CLASSLIB_QT == 1
    explicit Image (QImage const & image, Engine * engine = nullptr);
    operator QImage () const;
#endif
};

} // end namespace ren

#endif
