#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/image.hpp"
#include "rencpp/engine.hpp"


namespace ren {

//
// IMAGE
//

bool Image::isValid(RenCell const & cell) {
    return IS_IMAGE(&cell);
}

#if REN_CLASSLIB_QT == 1

Image::Image (QImage const & image, Engine * engine) {
    // need to convert if this isn't true
    assert(image.format() == QImage::Format_ARGB32);

    REBCNT width = static_cast<REBCNT>(image.width());
    REBCNT height = static_cast<REBCNT>(image.height());

    VAL_SET(&cell, REB_IMAGE);
    REBSER * img = Make_Image(width, height, FALSE);
    std::copy(
        image.bits(),
        image.bits() + (sizeof(char[4]) * width * height),
        IMG_DATA(img)
    );
    Val_Init_Image(&cell, img);
    finishInit(engine->getHandle());
}


Image::operator QImage () const {
    QImage result {
        VAL_IMAGE_DATA(&cell),
        static_cast<int>(VAL_IMAGE_WIDE(&cell)),
        static_cast<int>(VAL_IMAGE_HIGH(&cell)),
        QImage::Format_ARGB32
    };

    return result;
}

#endif

} // end namespace ren
