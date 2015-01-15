//
// rensyntaxer.cpp
// This file is part of Ren Garden
// Copyright (C) 2015 MetÆducation
//
// Ren Garden is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Ren Garden is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Ren Garden.  If not, see <http://www.gnu.org/licenses/>.
//
// See http://ren-garden.metaeducation.com for more information on this project
//

#include "rencpp/ren.hpp"

#include "rensyntaxer.h"


//
// Ideally this would be done with separately sandboxed "Engines", a feature
// supported by RenCpp's scaffolding but not currently by the Rebol or Red
// runtimes.  But without sandboxed engines it can still be possible; Rebol
// TASK! has some thread local storage that can do transcode on multiple
// threads independently.
//

std::pair<int, int> RenSyntaxer::rangeForWholeToken(
    QString buffer, int offset
) const {
    if (buffer.isEmpty())
        return std::make_pair(0, 0);

    return std::make_pair(
        0,
        static_cast<ren::Integer>(ren::runtime("0 +", offset))
    );
}


RenSyntaxer::~RenSyntaxer () {
}
