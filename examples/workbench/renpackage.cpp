//
// renpackage.cpp
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

#include <QFile>
#include <QByteArray>
#include <QStandardPaths>
#include <QMessageBox>
#include <QUrl>
#include <QtNetwork>

#include "rencpp/ren.hpp"

#include "renpackage.h"


// Work in progress...currently just a stub

using namespace ren;

RenPackage::RenPackage (
    QString rcPrefix,
    QString /*urlPrefix*/,
    Block const & scripts,
    std::experimental::optional<ren::Context> context
) {
    if (not context) {
        data = Block {};
    }

    for (auto filename : scripts) {
        QFile file {rcPrefix + static_cast<Filename>(filename)};
        file.open(QIODevice::ReadOnly);
        QByteArray bytes = file.readAll();
        if (context)
            (*context)(bytes.data());
        else {
            // because pure Ren hasn't been thought about yet, and
            // runtime defaults to USER context, we have to force
            // unbound loading behavior.  (Remember again this is a thought
            // experiment contributing to hammer on the design of module
            // and on the API.  Note we are cheating by using runtime
            // functions to perform a data-oriented task.

            runtime("append", *data, filename);
            runtime(
                "append/only", *data,
                    "load/type", String {bytes.data()}, "'unbound"
            );
        }
    }
}

QNetworkAccessManager & RenPackage::getNetwork() {
    static QNetworkAccessManager network; // no parent, dies at shutdown
    return network;
}

void RenPackage::downloadLocally() {
    // DataLocation is deprecated as of 5.4 and wants you to use
    // "AppDataLocation" instead

    QStringList paths =
        QStandardPaths::standardLocations(QStandardPaths::DataLocation);

    QNetworkAccessManager & network = getNetwork();

    QNetworkReply * reply = network.get(QNetworkRequest(
        QUrl("https://raw.githubusercontent.com/hostilefork/rebmu/master/rebmu.reb")
    ));

    connect(
        &network, &QNetworkAccessManager::finished,
        this, &RenPackage::replyFinished,
        Qt::DirectConnection
    );
}

void RenPackage::replyFinished(QNetworkReply * reply) {
    assert(reply->error() == QNetworkReply::NoError);

    QString contentType =
        reply->header(QNetworkRequest::ContentTypeHeader).toString();

    assert(contentType.contains("charset=utf-8"));

    QString data = QString::fromUtf8(reply->readAll());

    data.truncate(256);
    QMessageBox::information(nullptr, "Data retrieved...", data);

    // We are responsible for submitting a delete request; cannot delete
    // directly in this handler
    reply->deleteLater();
}

RenPackage::~RenPackage () {
}
