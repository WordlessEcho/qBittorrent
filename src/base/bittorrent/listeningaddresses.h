/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  WordlessEcho <wordless@echo.moe>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#pragma once

#include <optional>

#include <QAbstractSocket>
#include <QHostAddress>
#include <QList>
#include <QString>
#include <QStringList>

using namespace Qt::Literals::StringLiterals;

namespace BitTorrent
{
    // Special values of the "IPv4 address to bind to" and "IPv6 address to bind to" settings.
    // Any other value is one IP address of that family.
    inline const QString BIND_ALL_ADDRESSES;  // the default
    inline const QString BIND_DISABLED = u"disabled"_s;
    inline const QString BIND_TEMPORARY_IPV6_ADDRESSES = u"temporary"_s;  // IPv6 only

    // Returns the value to store for `value`: a special value as it is, or the normalized address.
    // Returns `std::nullopt` if `value` is not valid for `protocol` (IPv4 or IPv6).
    // Wildcard addresses ("0.0.0.0", "::") are not valid, use `BIND_ALL_ADDRESSES`.
    // IPv4-mapped IPv6 addresses ("::ffff:192.0.2.1") are not valid IPv6 values.
    std::optional<QString> normalizeBindAddress(const QString &value, QAbstractSocket::NetworkLayerProtocol protocol);

    struct ListeningAddressesInput
    {
        // `QNetworkInterface::name()` of the selected interface. Empty means any interface.
        QString ifaceName;
        bool isIfaceValid = false;  // ignored if `ifaceName` is empty
        // Addresses of the selected interface, or of all interfaces if `ifaceName` is empty
        QList<QHostAddress> addresses;
        // Usable temporary IPv6 addresses of the selected interface, or of all interfaces
        QList<QHostAddress> temporaryIPv6Addresses;
        QString ipv4Address;  // `BIND_ALL_ADDRESSES`, `BIND_DISABLED` or an IPv4 address
        QString ipv6Address;  // also `BIND_TEMPORARY_IPV6_ADDRESSES`, or an IPv6 address
    };

    struct ListeningAddresses
    {
        // IP addresses, "0.0.0.0", "::" or the interface name. With the interface name
        // libtorrent binds all addresses of the interface and follows their changes.
        QStringList IPs;
        bool isIfaceInvalid = false;
        // Configured values that are not valid for their family. They are not in `IPs`.
        QStringList invalidAddresses;
        // Configured addresses that were not found. They are in `IPs`, they can come online later.
        QStringList missingAddresses;
        bool hasNoIPv4Address = false;  // "all IPv4 addresses" of the selected interface found none
        bool hasNoIPv6Address = false;  // "all IPv6 addresses" of the selected interface found none
        bool hasNoTemporaryIPv6Address = false;
    };

    // Never gives a wildcard address for a selected interface,
    // so that a missing address cannot make libtorrent listen on other interfaces.
    ListeningAddresses resolveListeningAddresses(const ListeningAddressesInput &input);
}
