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

#include "listeningaddresses.h"

#include <algorithm>

#include "base/utils/net.h"

namespace
{
    std::optional<QHostAddress> parseBindAddress(const QString &value, const QAbstractSocket::NetworkLayerProtocol protocol)
    {
        const QHostAddress addr {value};
        if (addr.protocol() != protocol)
            return std::nullopt;

        // A wildcard would make libtorrent listen on all interfaces
        if ((addr == QHostAddress(QHostAddress::AnyIPv4)) || (addr == QHostAddress(QHostAddress::AnyIPv6)))
            return std::nullopt;

        // An IPv4-mapped IPv6 address (e.g. "::ffff:0.0.0.0") is an IPv4 address
        if (protocol == QAbstractSocket::IPv6Protocol)
        {
            const Q_IPV6ADDR bytes = addr.toIPv6Address();
            const bool isIPv4Mapped = std::all_of(bytes.c, (bytes.c + 10), [](const quint8 byte) { return byte == 0; })
                && (bytes.c[10] == 0xFF) && (bytes.c[11] == 0xFF);
            if (isIPv4Mapped)
                return std::nullopt;
        }

        return addr;
    }

    bool containsAddress(const QList<QHostAddress> &addresses, const QHostAddress &addr)
    {
        if (addr.protocol() == QAbstractSocket::IPv6Protocol)
            return Utils::Net::containsIPv6Address(addresses, addr);

        return addresses.contains(addr);
    }
}

std::optional<QString> BitTorrent::normalizeBindAddress(const QString &value, const QAbstractSocket::NetworkLayerProtocol protocol)
{
    if ((value == BIND_ALL_ADDRESSES) || (value == BIND_DISABLED))
        return value;

    const std::optional<QHostAddress> addr = parseBindAddress(value, protocol);
    if (!addr)
        return std::nullopt;

    return addr->toString();
}

BitTorrent::ListeningAddresses BitTorrent::resolveListeningAddresses(const ListeningAddressesInput &input)
{
    ListeningAddresses result;

    const bool isAnyIface = input.ifaceName.isEmpty();
    if (!isAnyIface && !input.isIfaceValid)
        result.isIfaceInvalid = true;

    // Libtorrent binds all addresses of an interface given by its name.
    // This also holds while the interface is missing, so that it can come online later.
    if (!isAnyIface && (input.ipv4Address == BIND_ALL_ADDRESSES) && (input.ipv6Address == BIND_ALL_ADDRESSES))
    {
        result.IPs.append(input.ifaceName);
        return result;
    }

    // Used when the setting is "all addresses": adds all addresses of `protocol`
    // and returns whether there is at least one
    const auto addAllOfFamily = [&input, &result](const QAbstractSocket::NetworkLayerProtocol protocol) -> bool
    {
        bool isFound = false;
        for (const QHostAddress &addr : input.addresses)
        {
            if (addr.protocol() == protocol)
            {
                result.IPs.append(addr.toString());
                isFound = true;
            }
        }
        return isFound;
    };

    const auto addConfiguredAddress = [&input, &result](const QString &value, const QAbstractSocket::NetworkLayerProtocol protocol)
    {
        const std::optional<QHostAddress> addr = parseBindAddress(value, protocol);
        if (!addr)
        {
            result.invalidAddresses.append(value);
            return;
        }

        if (!containsAddress(input.addresses, *addr))
            result.missingAddresses.append(value);
        result.IPs.append(addr->toString());
    };

    // IPv4
    if (input.ipv4Address == BIND_ALL_ADDRESSES)
    {
        if (isAnyIface)
            result.IPs.append(u"0.0.0.0"_s);
        else if (!addAllOfFamily(QAbstractSocket::IPv4Protocol))
            result.hasNoIPv4Address = true;
    }
    else if (input.ipv4Address != BIND_DISABLED)
    {
        addConfiguredAddress(input.ipv4Address, QAbstractSocket::IPv4Protocol);
    }

    // IPv6
    if (input.ipv6Address == BIND_ALL_ADDRESSES)
    {
        if (isAnyIface)
            result.IPs.append(u"::"_s);
        else if (!addAllOfFamily(QAbstractSocket::IPv6Protocol))
            result.hasNoIPv6Address = true;
    }
    else if (input.ipv6Address != BIND_DISABLED)
    {
        addConfiguredAddress(input.ipv6Address, QAbstractSocket::IPv6Protocol);
    }

    return result;
}
