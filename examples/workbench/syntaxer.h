//
// syntaxer.h
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

#ifndef SYNTAXER_H
#define SYNTAXER_H

#include <utility>

#include <QString>

//
// The journey of 1,000 miles begins with a single step.  Rather than tackle
// syntax highlighting immediately, let's start with a thought experiment about
// merely being able to double click and have a range calculated for a token.
// So if | represents your cursor and you had:
//
//     print {Hello| World}
//
// Then the offset would realize you wanted to select from the opening curly
// brace to the closing curly brace, vs merely selecting hello (as a default
// text edit might.)
//
// This very simple example offers a chance to start thinking about most of
// the "big" issues of threading, when you are to try using the Ren runtime
// to answer such questions.  Because you must be able to safely handle a
// LOAD or TRANSCODE of the buffer while an evaluation is going on in the
// console.  (A user can still select while a command is running, can't they?)
//
// Right now, "Syntaxer" does not need to speak in the QTextEdit vernacular; 
// we just extract buffers of interest out and call it.
//

class Syntaxer {
public:
    virtual std::pair<int, int> rangeForWholeToken(
        QString buffer, int offset
    ) const = 0;

    virtual ~Syntaxer () {}
};

#endif
