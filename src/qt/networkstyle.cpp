// Copyright (c) 2014 The Bitcoin Core developers
// Copyright (C) 2015 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "networkstyle.h"

#include "guiconstants.h"
#include "scicon.h"

#include <QApplication>

#include <stdexcept>

static const struct {
    const char *networkId;
    const char *appName;
    const int iconColorHueShift;
    const int iconColorSaturationReduction;
    const char *titleAddText;
} network_styles[] = {
    {"main", QAPP_APP_NAME_DEFAULT, 0, 0, ""},
    {"test", QAPP_APP_NAME_TESTNET, 70, 30, QT_TRANSLATE_NOOP("SplashScreen", "[testnet]")},
    {"regtest", QAPP_APP_NAME_TESTNET, 160, 30, "[regtest]"}
};
static const int network_styles_count = sizeof(network_styles)/sizeof(*network_styles);

static QImage fixIcon(const QImage &image, int iconColorHueShift, int iconColorSaturationReduction)
{
    if (iconColorSaturationReduction == 0 && iconColorHueShift == 0)
        return image;

    QImage copy(image);
    // traverse though lines
    for (int y=0; y < copy.height(); y++)
    {
        QRgb *scL = reinterpret_cast<QRgb*>(copy.scanLine(y));

        // loop through pixels
        for (int x=0; x < copy.width(); x++)
        {
            int h,s,l,a;
            // preserve alpha because QColor::getHsl doesen't return the alpha value
            a = qAlpha(scL[x]);
            QColor col(scL[x]);

            // get hue value
            col.getHsl(&h,&s,&l);

            // rotate color on RGB color circle
            // 70° should end up with the typical "testnet" green
            h+=iconColorHueShift;

            // change saturation value
            if (s>iconColorSaturationReduction)
            {
                s -= iconColorSaturationReduction;
            }
            col.setHsl(h,s,l,a);

            // set the pixel
            scL[x] = col.rgba();
        }
    }

    return copy;
}


NetworkStyle::NetworkStyle(const QString &networkId)
{
    int iconColorHueShift = 0;
    int iconColorSaturationReduction = 0;
    // char *titleAddText;

    for (int x=0; x<network_styles_count; ++x)
    {
        if (networkId == network_styles[x].networkId)
        {
            appName = network_styles[x].appName;
            titleAddText = qApp->translate("SplashScreen", network_styles[x].titleAddText);
            iconColorHueShift = network_styles[x].iconColorHueShift;
            iconColorSaturationReduction = network_styles[x].iconColorSaturationReduction;
            break;
        }
    }
    if (appName.isEmpty())
        throw std::runtime_error("Unknown networkId passed into NetworkStyle");

    appIcon = fixIcon(QImage(":/icons/bitcoin"), iconColorHueShift, iconColorSaturationReduction);
    Q_ASSERT(appIcon.width() == 1024);
    Q_ASSERT(appIcon.height() == 1024);
    trayAndWindowIcon = QIcon(QPixmap::fromImage(appIcon.scaled(256, 256)));
}
