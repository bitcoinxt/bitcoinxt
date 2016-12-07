// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_NETWORKSTYLE_H
#define BITCOIN_QT_NETWORKSTYLE_H

#include <QIcon>
#include <QPixmap>
#include <QString>

/* Coin network-specific GUI style information */
class NetworkStyle
{
public:
    /**
     * Create style associated with provided BIP70 network id
     * The constructor throws an runtime error if the network id is unknown.
     */
    NetworkStyle(const QString &networkId);

    QString getAppName() const { return appName; }
    QString getTitleAddText() const { return titleAddText; }
    QImage getAppIcon() const { return appIcon; }
    QIcon getTrayAndWindowIcon() const { return trayAndWindowIcon; }



private:
    QString appName;
    QImage appIcon;
    QIcon trayAndWindowIcon;
    QString titleAddText;
};

#endif // BITCOIN_QT_NETWORKSTYLE_H
