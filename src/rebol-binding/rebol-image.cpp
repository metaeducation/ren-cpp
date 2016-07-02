#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/image.hpp"
#include "rencpp/engine.hpp"

#include "rebol-common.hpp"


namespace ren {

//
// IMAGE
//

bool Image::isValid(RenCell const * cell) {
    return IS_IMAGE(AS_C_REBVAL(cell));
}

#if REN_CLASSLIB_QT == 1

Image::Image (QImage const & image, Engine * engine) {
    // need to convert if this isn't true
    assert(image.format() == QImage::Format_ARGB32);

    REBCNT width = static_cast<REBCNT>(image.width());
    REBCNT height = static_cast<REBCNT>(image.height());

    VAL_RESET_HEADER(AS_REBVAL(cell), REB_IMAGE);
    REBSER * img = Make_Image(width, height, FALSE);
    std::copy(
        image.bits(),
        image.bits() + (sizeof(char[4]) * width * height),
        IMG_DATA(img)
    );
    Val_Init_Image(AS_REBVAL(cell), img);
    finishInit(engine->getHandle());
}


Image::operator QImage () const {
    QImage result {
        VAL_IMAGE_DATA(AS_C_REBVAL(cell)),
        static_cast<int>(VAL_IMAGE_WIDE(AS_C_REBVAL(cell))),
        static_cast<int>(VAL_IMAGE_HIGH(AS_C_REBVAL(cell))),
        QImage::Format_ARGB32
    };

    return result;
}

#endif

} // end namespace ren
