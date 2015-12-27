#ifndef RENGARDEN_RENPACKAGE_H
#define RENGARDEN_RENPACKAGE_H

//
// renpackage.h
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

#include <QtNetwork/QNetworkAccessManager>

//
// Another exploratory attempt...which could be improved somehow or perhaps
// replaced entirely.  It may be a synonym for what MODULE! was supposed
// to be able to do, but does not quite do today.
//
// A RenPackage is simply a way of keeping some files up to date in the
// Ren Garden system.  A package's files may be cached in the binary as
// resources, but it may be desirable to update them even without updating
// Ren Garden.  The goal is to find a suitable directory on the user's
// system to put the files.
//
// It doesn't do any dynamic updates as of yet, only testing the network
// API.  Just gets the files out of the QRC resource embedded in the
// Ren Garden executable for now.
//

#include "optional/optional.hpp"

#include <QString>

#include "rencpp/ren.hpp"

class RenPackage : public QObject {

    Q_OBJECT

public:
    RenPackage (
        QString rcPrefix,
        QString urlPrefix,
        ren::Block const & spec,
        ren::optional<ren::AnyContext> context
    );

    virtual ~RenPackage ();

private:
    QNetworkAccessManager & getNetwork();

public:
    void downloadLocally();

private slots:
    void replyFinished(QNetworkReply * reply);

private:
    // If data-only, the loaded data for each file, which can be indexed
    ren::optional<ren::Block> data;
public:
    ren::AnyValue getData(ren::Filename const & filename) const {
        return (*data)[filename];
    }
};

#endif
