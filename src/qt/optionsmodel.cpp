// Copyright (c) 2011-2014 The Bitcoin Core developers
// Copyright (c) 2015 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "optionsmodel.h"

#include "bitcoinunits.h"
#include "guiutil.h"

#include "amount.h"
#include "init.h"
#include "main.h"
#include "net.h"
#include "txdb.h" // for -dbcache defaults

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#endif

#include <QNetworkProxy>
#include <QSettings>
#include <QStringList>

#include <boost/lexical_cast.hpp>

OptionsModel::OptionsModel(QObject *parent) :
    QAbstractListModel(parent)
{
    Init();
}

void OptionsModel::addOverriddenOption(const std::string &option)
{
    strOverriddenByCommandLine += QString::fromStdString(option) + "=" + QString::fromStdString(mapArgs[option]) + " ";
}

// Writes all missing QSettings with their default values
void OptionsModel::Init()
{
    QSettings settings;

    // Ensure restart flag is unset on client startup
    setRestartRequired(false);

    // These are Qt-only settings:

    // Window
    if (!settings.contains("fMinimizeToTray"))
        settings.setValue("fMinimizeToTray", false);
    fMinimizeToTray = settings.value("fMinimizeToTray").toBool();

    if (!settings.contains("fMinimizeOnClose"))
        settings.setValue("fMinimizeOnClose", false);
    fMinimizeOnClose = settings.value("fMinimizeOnClose").toBool();

    // Display
    if (!settings.contains("nDisplayUnit"))
        settings.setValue("nDisplayUnit", BitcoinUnits::BTC);
    nDisplayUnit = settings.value("nDisplayUnit").toInt();

    if (!settings.contains("strThirdPartyTxUrls"))
        settings.setValue("strThirdPartyTxUrls", "");
    strThirdPartyTxUrls = settings.value("strThirdPartyTxUrls", "").toString();

    if (!settings.contains("fCoinControlFeatures"))
        settings.setValue("fCoinControlFeatures", false);
    fCoinControlFeatures = settings.value("fCoinControlFeatures", false).toBool();

    // These are shared with the core or have a command-line parameter
    // and we want command-line parameters to overwrite the GUI settings.
    //
    // If setting doesn't exist create it with defaults.
    //
    // If SoftSetArg() or SoftSetBoolArg() return false we were overridden
    // by command-line and show this in the UI.

    // Main
    if (!settings.contains("nDatabaseCache"))
        settings.setValue("nDatabaseCache", (qint64)nDefaultDbCache);
    if (!SoftSetArg("-dbcache", settings.value("nDatabaseCache").toString().toStdString()))
        addOverriddenOption("-dbcache");

    if (!settings.contains("nThreadsScriptVerif"))
        settings.setValue("nThreadsScriptVerif", DEFAULT_SCRIPTCHECK_THREADS);
    if (!SoftSetArg("-par", settings.value("nThreadsScriptVerif").toString().toStdString()))
        addOverriddenOption("-par");

    // Wallet
#ifdef ENABLE_WALLET
    if (!settings.contains("bSpendZeroConfChange"))
        settings.setValue("bSpendZeroConfChange", true);
    if (!SoftSetBoolArg("-spendzeroconfchange", settings.value("bSpendZeroConfChange").toBool()))
        addOverriddenOption("-spendzeroconfchange");
#endif

    // Network

    if (!settings.contains("fUseReceiveShaping"))
        settings.setValue("fUseReceiveShaping", false);
    if (!settings.contains("fUseSendShaping"))
        settings.setValue("fUseSendShaping", false);

    if (!settings.contains("nReceiveBurst"))
        settings.setValue("nReceiveBurst", 300); // %
    if (!settings.contains("nReceiveAve"))
        settings.setValue("nReceiveAve", 500); // KB/s
    if (!settings.contains("nSendBurst"))
        settings.setValue("nSendBurst", 200); // %
    if (!settings.contains("nSendAve"))
        settings.setValue("nSendAve", 150); // KB/s

    bool inUse = settings.value("fUseReceiveShaping").toBool();
    int aveKB = settings.value("nReceiveAve").toInt();
    int burstPercent = settings.value("nReceiveBurst").toInt();

    std::string avg = boost::lexical_cast<std::string>(inUse ? aveKB : 0);
    std::string burst = boost::lexical_cast<std::string>(inUse ? (aveKB * burstPercent / 100) : 0);

    if (!SoftSetArg("-receiveavg", avg))
        addOverriddenOption("-receiveavg");
    if (!SoftSetArg("-receiveburst", burst))
        addOverriddenOption("-receiveburst");

    inUse = settings.value("fUseSendShaping").toBool();
    aveKB = settings.value("nSendAve").toInt();
    burstPercent = settings.value("nSendBurst").toInt();

    avg = boost::lexical_cast<std::string>(inUse ? aveKB : 0);
    burst = boost::lexical_cast<std::string>(inUse ? (aveKB * burstPercent / 100) : 0);

    if (!SoftSetArg("-sendavg", avg))
        addOverriddenOption("-sendavg");
    if (!SoftSetArg("-sendburst", burst))
        addOverriddenOption("-sendburst");

    if (!settings.contains("fUseUPnP"))
        settings.setValue("fUseUPnP", DEFAULT_UPNP);
    if (!SoftSetBoolArg("-upnp", settings.value("fUseUPnP").toBool()))
        addOverriddenOption("-upnp");

    if (!settings.contains("fListen"))
        settings.setValue("fListen", DEFAULT_LISTEN);
    if (!SoftSetBoolArg("-listen", settings.value("fListen").toBool()))
        addOverriddenOption("-listen");

    if (!settings.contains("fUseProxy"))
        settings.setValue("fUseProxy", false);
    if (!settings.contains("addrProxy"))
        settings.setValue("addrProxy", "127.0.0.1:9050");
    // Only try to set -proxy, if user has enabled fUseProxy
    if (settings.value("fUseProxy").toBool() && !SoftSetArg("-proxy", settings.value("addrProxy").toString().toStdString()))
        addOverriddenOption("-proxy");
    else if(!settings.value("fUseProxy").toBool() && !GetArg("-proxy", "").empty())
        addOverriddenOption("-proxy");

    // Display
    if (!settings.contains("language"))
        settings.setValue("language", "");
    if (!SoftSetArg("-lang", settings.value("language").toString().toStdString()))
        addOverriddenOption("-lang");

    language = settings.value("language").toString();
}

void OptionsModel::Reset()
{
    QSettings settings;

    // Remove all entries from our QSettings object
    settings.clear();

    // default setting for OptionsModel::StartAtStartup - disabled
    if (GUIUtil::GetStartOnSystemStartup())
        GUIUtil::SetStartOnSystemStartup(false);
}

int OptionsModel::rowCount(const QModelIndex & parent) const
{
    return OptionIDRowCount;
}

// read QSettings values and return them
QVariant OptionsModel::data(const QModelIndex & index, int role) const
{
    if(role == Qt::EditRole)
    {
        QSettings settings;
        switch(index.row())
        {
        case StartAtStartup:
            return GUIUtil::GetStartOnSystemStartup();
        case MinimizeToTray:
            return fMinimizeToTray;
        case MapPortUPnP:
#ifdef USE_UPNP
            return settings.value("fUseUPnP");
#else
            return false;
#endif
        case MinimizeOnClose:
            return fMinimizeOnClose;

        // default proxy
        case ProxyUse:
            return settings.value("fUseProxy", false);
        case ProxyIP: {
            // contains IP at index 0 and port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            return strlIpPort.at(0);
        }
        case ProxyPort: {
            // contains IP at index 0 and port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            return strlIpPort.at(1);
        }

#ifdef ENABLE_WALLET
        case SpendZeroConfChange:
            return settings.value("bSpendZeroConfChange");
#endif
        case DisplayUnit:
            return nDisplayUnit;
        case ThirdPartyTxUrls:
            return strThirdPartyTxUrls;
        case Language:
            return settings.value("language");
        case CoinControlFeatures:
            return fCoinControlFeatures;
        case DatabaseCache:
            return settings.value("nDatabaseCache");
        case ThreadsScriptVerif:
            return settings.value("nThreadsScriptVerif");
        case Listen:
            return settings.value("fListen");
        case UseReceiveShaping:
            return settings.value("fUseReceiveShaping");
        case UseSendShaping:
            return settings.value("fUseSendShaping");
        case ReceiveBurst:
            return settings.value("nReceiveBurst");
        case ReceiveAve:
            return settings.value("nReceiveAve");
        case SendBurst:
            return settings.value("nSendBurst");
        case SendAve:
            return settings.value("nSendAve");
        default:
            return QVariant();
        }
    }
    return QVariant();
}

// write QSettings values
bool OptionsModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
    bool successful = true; /* set to false on parse error */
    bool changeSendShaper = false;
    bool changeReceiveShaper = false;
    if(role == Qt::EditRole)
    {
        QSettings settings;
        switch(index.row())
        {
        case StartAtStartup:
            successful = GUIUtil::SetStartOnSystemStartup(value.toBool());
            break;
        case MinimizeToTray:
            fMinimizeToTray = value.toBool();
            settings.setValue("fMinimizeToTray", fMinimizeToTray);
            break;
        case MapPortUPnP: // core option - can be changed on-the-fly
            settings.setValue("fUseUPnP", value.toBool());
            MapPort(value.toBool());
            break;
        case MinimizeOnClose:
            fMinimizeOnClose = value.toBool();
            settings.setValue("fMinimizeOnClose", fMinimizeOnClose);
            break;

        // default proxy
        case ProxyUse:
            if (settings.value("fUseProxy") != value) {
                settings.setValue("fUseProxy", value.toBool());
                setRestartRequired(true);
            }
            break;
        case ProxyIP: {
            // contains current IP at index 0 and current port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            // if that key doesn't exist or has a changed IP
            if (!settings.contains("addrProxy") || strlIpPort.at(0) != value.toString()) {
                // construct new value from new IP and current port
                QString strNewValue = value.toString() + ":" + strlIpPort.at(1);
                settings.setValue("addrProxy", strNewValue);
                setRestartRequired(true);
            }
        }
        break;
        case ProxyPort: {
            // contains current IP at index 0 and current port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            // if that key doesn't exist or has a changed port
            if (!settings.contains("addrProxy") || strlIpPort.at(1) != value.toString()) {
                // construct new value from current IP and new port
                QString strNewValue = strlIpPort.at(0) + ":" + value.toString();
                settings.setValue("addrProxy", strNewValue);
                setRestartRequired(true);
            }
        }
        break;
#ifdef ENABLE_WALLET
        case SpendZeroConfChange:
            if (settings.value("bSpendZeroConfChange") != value) {
                settings.setValue("bSpendZeroConfChange", value);
                setRestartRequired(true);
            }
            break;
#endif
        case DisplayUnit:
            setDisplayUnit(value);
            break;
        case ThirdPartyTxUrls:
            if (strThirdPartyTxUrls != value.toString()) {
                strThirdPartyTxUrls = value.toString();
                settings.setValue("strThirdPartyTxUrls", strThirdPartyTxUrls);
                setRestartRequired(true);
            }
            break;
        case Language:
            if (settings.value("language") != value) {
                settings.setValue("language", value);
                setRestartRequired(true);
            }
            break;
        case CoinControlFeatures:
            fCoinControlFeatures = value.toBool();
            settings.setValue("fCoinControlFeatures", fCoinControlFeatures);
            emit coinControlFeaturesChanged(fCoinControlFeatures);
            break;
        case DatabaseCache:
            if (settings.value("nDatabaseCache") != value) {
                settings.setValue("nDatabaseCache", value);
                setRestartRequired(true);
            }
            break;
        case ThreadsScriptVerif:
            if (settings.value("nThreadsScriptVerif") != value) {
                settings.setValue("nThreadsScriptVerif", value);
                setRestartRequired(true);
            }
            break;
        case Listen:
            if (settings.value("fListen") != value) {
                settings.setValue("fListen", value);
                setRestartRequired(true);
            }
            break;
        case UseReceiveShaping:
            if (settings.value("fUseReceiveShaping") != value) {
                settings.setValue("fUseReceiveShaping",value);
                changeReceiveShaper = true;
            }
            break;
        case UseSendShaping:
            if (settings.value("fUseSendShaping") != value) {
                settings.setValue("fUseSendShaping",value);
                changeSendShaper = true;
            }
            break;
        case ReceiveBurst:
            if (settings.value("nReceiveBurst") != value) {
                settings.setValue("nReceiveBurst",value);
                changeReceiveShaper = true;
            }
            break;
        case ReceiveAve:
            if (settings.value("nReceiveAve") != value) {
                settings.setValue("nReceiveAve",value);
                changeReceiveShaper = true;
            }
            break;
        case SendBurst:
            if (settings.value("nSendBurst") != value) {
                settings.setValue("nSendBurst",value);
                changeSendShaper = true;
            }
            break;
        case SendAve:
            if (settings.value("nSendAve") != value) {
                settings.setValue("nSendAve",value);
                changeSendShaper = true;
            }
            break;
        default:
            break;
        }

        if (changeReceiveShaper) {
            extern CLeakyBucket receiveShaper;
            if (settings.value("fUseReceiveShaping").toBool()) {
                int ave = 1000 * settings.value("nReceiveAve").toInt();
                int burst = ave * settings.value("nReceiveBurst").toInt() / 100;
                receiveShaper.set(burst, ave);
            }
            else
            {
                receiveShaper.disable();
            }
        }

        if (changeSendShaper) {
            extern CLeakyBucket sendShaper;
            if (settings.value("fUseSendShaping").toBool()) {
                int ave = 1000 * settings.value("nSendAve").toInt();
                int burst = ave * settings.value("nSendBurst").toInt() / 100;
                sendShaper.set(burst, ave);
            }
            else
            {
                sendShaper.disable();
            }
        }
    }

    emit dataChanged(index, index);

    return successful;
}

/** Updates current unit in memory, settings and emits displayUnitChanged(newUnit) signal */
void OptionsModel::setDisplayUnit(const QVariant &value)
{
    if (!value.isNull())
    {
        QSettings settings;
        nDisplayUnit = value.toInt();
        settings.setValue("nDisplayUnit", nDisplayUnit);
        emit displayUnitChanged(nDisplayUnit);
    }
}

bool OptionsModel::getProxySettings(QNetworkProxy& proxy) const
{
    // Directly query current base proxy, because
    // GUI settings can be overridden with -proxy.
    proxyType curProxy;
    if (GetProxy(NET_IPV4, curProxy)) {
        proxy.setType(QNetworkProxy::Socks5Proxy);
        proxy.setHostName(QString::fromStdString(curProxy.proxy.ToStringIP()));
        proxy.setPort(curProxy.proxy.GetPort());

        return true;
    }
    else
        proxy.setType(QNetworkProxy::NoProxy);

    return false;
}

void OptionsModel::setRestartRequired(bool fRequired)
{
    QSettings settings;
    return settings.setValue("fRestartRequired", fRequired);
}

bool OptionsModel::isRestartRequired()
{
    QSettings settings;
    return settings.value("fRestartRequired", false).toBool();
}
