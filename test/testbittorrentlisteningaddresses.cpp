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

#include <optional>

#include <QAbstractSocket>
#include <QFlags>
#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTest>

#include "base/bittorrent/listeningaddresses.h"

using namespace Qt::Literals::StringLiterals;

namespace
{
    using BitTorrent::BIND_ALL_ADDRESSES;
    using BitTorrent::BIND_DISABLED;
    using BitTorrent::BIND_TEMPORARY_IPV6_ADDRESSES;

    using Input = BitTorrent::ListeningAddressesInput;

    // the `bool` members of `BitTorrent::ListeningAddresses`
    enum ResultFlag
    {
        IsIfaceInvalid = 1,
        HasNoIPv4Address = 2,
        HasNoIPv6Address = 4,
        HasNoTemporaryIPv6Address = 8
    };
    Q_DECLARE_FLAGS(ResultFlags, ResultFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(ResultFlags)

    const QString IFACE = u"eth0"_s;
    const QString INVALID_IFACE = u"eth1"_s;

    // addresses of `IFACE`
    const QString IPV4_1 = u"192.0.2.10"_s;
    const QString IPV4_2 = u"192.0.2.11"_s;
    const QString IPV6_STABLE = u"2001:db8::1"_s;
    const QString IPV6_TEMPORARY_1 = u"2001:db8::a1"_s;
    const QString IPV6_TEMPORARY_2 = u"2001:db8::a2"_s;
    const QString IPV6_LINK_LOCAL = u"fe80::1%eth0"_s;

    // addresses that no interface has
    const QString MISSING_IPV4 = u"198.51.100.1"_s;
    const QString MISSING_IPV6 = u"2001:db8:1::1"_s;

    QList<QHostAddress> makeAddresses(const QStringList &addresses)
    {
        QList<QHostAddress> result;
        result.reserve(addresses.size());
        for (const QString &addr : addresses)
            result.append(QHostAddress(addr));
        return result;
    }

    // IPv4 and IPv6 addresses are mixed on purpose: the IPv4 ones come first in the result
    const QList<QHostAddress> IFACE_ADDRESSES = makeAddresses({IPV4_1, IPV6_STABLE, IPV6_TEMPORARY_1, IPV4_2
        , IPV6_TEMPORARY_2, IPV6_LINK_LOCAL});
    const QList<QHostAddress> TEMPORARY_ADDRESSES = makeAddresses({IPV6_TEMPORARY_1, IPV6_TEMPORARY_2});
    // addresses of all interfaces: the ones of `IFACE` and of the loopback interface
    const QList<QHostAddress> ALL_ADDRESSES = IFACE_ADDRESSES + makeAddresses({u"127.0.0.1"_s, u"::1"_s});

    // Any interface, with the addresses of all interfaces by default
    Input anyIface(const QString &ipv4Address, const QString &ipv6Address
        , const QList<QHostAddress> &addresses = ALL_ADDRESSES, const QList<QHostAddress> &temporaryAddresses = TEMPORARY_ADDRESSES)
    {
        Input input;
        input.addresses = addresses;
        input.temporaryIPv6Addresses = temporaryAddresses;
        input.ipv4Address = ipv4Address;
        input.ipv6Address = ipv6Address;
        return input;
    }

    // The valid interface `IFACE`, with its addresses by default
    Input iface(const QString &ipv4Address, const QString &ipv6Address
        , const QList<QHostAddress> &addresses = IFACE_ADDRESSES, const QList<QHostAddress> &temporaryAddresses = TEMPORARY_ADDRESSES)
    {
        Input input = anyIface(ipv4Address, ipv6Address, addresses, temporaryAddresses);
        input.ifaceName = IFACE;
        input.isIfaceValid = true;
        return input;
    }

    // The invalid interface `INVALID_IFACE`, which has no addresses
    Input invalidIface(const QString &ipv4Address, const QString &ipv6Address)
    {
        Input input = anyIface(ipv4Address, ipv6Address, {}, {});
        input.ifaceName = INVALID_IFACE;
        input.isIfaceValid = false;
        return input;
    }
}

class TestBittorrentListeningAddresses final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestBittorrentListeningAddresses)

public:
    TestBittorrentListeningAddresses() = default;

private slots:
    void testSpecialValues() const
    {
        // the values are stored in the settings, so they must not change
        QVERIFY(BIND_ALL_ADDRESSES.isEmpty());
        QCOMPARE(BIND_DISABLED, u"disabled"_s);
        QCOMPARE(BIND_TEMPORARY_IPV6_ADDRESSES, u"temporary"_s);
    }

    void testNormalizeBindAddress_data() const
    {
        QTest::addColumn<QString>("value");
        QTest::addColumn<QAbstractSocket::NetworkLayerProtocol>("protocol");
        QTest::addColumn<bool>("isValid");
        QTest::addColumn<QString>("expected");

        const auto ipv4 = QAbstractSocket::IPv4Protocol;
        const auto ipv6 = QAbstractSocket::IPv6Protocol;

        // special values
        QTest::newRow("all addresses, IPv4") << BIND_ALL_ADDRESSES << ipv4 << true << BIND_ALL_ADDRESSES;
        QTest::newRow("all addresses, IPv6") << BIND_ALL_ADDRESSES << ipv6 << true << BIND_ALL_ADDRESSES;
        QTest::newRow("disabled, IPv4") << BIND_DISABLED << ipv4 << true << BIND_DISABLED;
        QTest::newRow("disabled, IPv6") << BIND_DISABLED << ipv6 << true << BIND_DISABLED;
        QTest::newRow("temporary, IPv6") << BIND_TEMPORARY_IPV6_ADDRESSES << ipv6 << true << BIND_TEMPORARY_IPV6_ADDRESSES;
        QTest::newRow("temporary, IPv4") << BIND_TEMPORARY_IPV6_ADDRESSES << ipv4 << false << QString();

        // addresses
        QTest::newRow("IPv4") << u"192.0.2.10"_s << ipv4 << true << u"192.0.2.10"_s;
        QTest::newRow("IPv4 loopback") << u"127.0.0.1"_s << ipv4 << true << u"127.0.0.1"_s;
        QTest::newRow("IPv6") << u"2001:db8::a1"_s << ipv6 << true << u"2001:db8::a1"_s;
        QTest::newRow("IPv6 in upper case") << u"2001:DB8::A1"_s << ipv6 << true << u"2001:db8::a1"_s;
        QTest::newRow("IPv6 in long form")
            << u"2001:0db8:0000:0000:0000:0000:0000:00a1"_s << ipv6 << true << u"2001:db8::a1"_s;
        QTest::newRow("IPv6 loopback") << u"::1"_s << ipv6 << true << u"::1"_s;
        QTest::newRow("IPv6 link-local with scope id") << u"fe80::1%eth0"_s << ipv6 << true << u"fe80::1%eth0"_s;
        QTest::newRow("IPv6 containing ffff") << u"2001:db8::ffff:1"_s << ipv6 << true << u"2001:db8::ffff:1"_s;
        // "ffff" at the position of the IPv4-mapped prefix, but with a non-zero prefix before it
        QTest::newRow("IPv6 with embedded IPv4")
            << u"2001:db8::ffff:192.0.2.10"_s << ipv6 << true << u"2001:db8::ffff:c000:20a"_s;

        // other family
        QTest::newRow("IPv6 for IPv4") << u"2001:db8::a1"_s << ipv4 << false << QString();
        QTest::newRow("IPv4-mapped IPv6 for IPv4") << u"::ffff:192.0.2.10"_s << ipv4 << false << QString();
        QTest::newRow("IPv4 for IPv6") << u"192.0.2.10"_s << ipv6 << false << QString();
        // an IPv4-mapped IPv6 address is an IPv4 address
        QTest::newRow("IPv4-mapped IPv6") << u"::ffff:192.0.2.10"_s << ipv6 << false << QString();
        QTest::newRow("IPv4-mapped IPv6 in hexadecimal form") << u"::ffff:c000:20a"_s << ipv6 << false << QString();
        QTest::newRow("IPv4-mapped IPv4 wildcard") << u"::ffff:0.0.0.0"_s << ipv6 << false << QString();

        // wildcards
        QTest::newRow("IPv4 wildcard") << u"0.0.0.0"_s << ipv4 << false << QString();
        QTest::newRow("IPv4 wildcard for IPv6") << u"0.0.0.0"_s << ipv6 << false << QString();
        QTest::newRow("IPv6 wildcard") << u"::"_s << ipv6 << false << QString();
        QTest::newRow("IPv6 wildcard in long form") << u"0:0:0:0:0:0:0:0"_s << ipv6 << false << QString();
        QTest::newRow("IPv6 wildcard with scope id") << u"::%eth0"_s << ipv6 << false << QString();
        QTest::newRow("IPv6 wildcard for IPv4") << u"::"_s << ipv4 << false << QString();

        // garbage
        QTest::newRow("garbage, IPv4") << u"abc"_s << ipv4 << false << QString();
        QTest::newRow("garbage, IPv6") << u"abc"_s << ipv6 << false << QString();
        QTest::newRow("IPv4 out of range") << u"192.0.2.256"_s << ipv4 << false << QString();
        QTest::newRow("IPv6 with invalid digit") << u"2001:db8::g"_s << ipv6 << false << QString();
        QTest::newRow("IPv4 with port") << u"192.0.2.10:6881"_s << ipv4 << false << QString();
        QTest::newRow("IPv4 subnet") << u"192.0.2.0/24"_s << ipv4 << false << QString();
        QTest::newRow("interface name, IPv4") << u"eth0"_s << ipv4 << false << QString();
        QTest::newRow("interface name, IPv6") << u"eth0"_s << ipv6 << false << QString();
    }

    void testNormalizeBindAddress() const
    {
        QFETCH(QString, value);
        QFETCH(QAbstractSocket::NetworkLayerProtocol, protocol);
        QFETCH(bool, isValid);
        QFETCH(QString, expected);

        const std::optional<QString> result = BitTorrent::normalizeBindAddress(value, protocol);
        QCOMPARE(result.has_value(), isValid);
        if (!result)
            return;

        QCOMPARE(*result, expected);
        // a normalized value is accepted again as it is
        QVERIFY(BitTorrent::normalizeBindAddress(*result, protocol) == result);
    }

    void testResolveListeningAddresses_data() const
    {
        QTest::addColumn<Input>("input");
        QTest::addColumn<QStringList>("IPs");
        QTest::addColumn<QStringList>("invalidAddresses");
        QTest::addColumn<QStringList>("missingAddresses");
        QTest::addColumn<ResultFlags>("flags");

        // any interface: `isIfaceValid` is ignored, it is `false` in every row

        QTest::newRow("any interface, defaults")
            << anyIface(BIND_ALL_ADDRESSES, BIND_ALL_ADDRESSES)
            << QStringList {u"0.0.0.0"_s, u"::"_s} << QStringList {} << QStringList {} << ResultFlags {};
        QTest::newRow("any interface, defaults without addresses")
            << anyIface(BIND_ALL_ADDRESSES, BIND_ALL_ADDRESSES, {}, {})
            << QStringList {u"0.0.0.0"_s, u"::"_s} << QStringList {} << QStringList {} << ResultFlags {};
        QTest::newRow("any interface, IPv4 only")
            << anyIface(BIND_ALL_ADDRESSES, BIND_DISABLED)
            << QStringList {u"0.0.0.0"_s} << QStringList {} << QStringList {} << ResultFlags {};
        QTest::newRow("any interface, IPv6 only")
            << anyIface(BIND_DISABLED, BIND_ALL_ADDRESSES)
            << QStringList {u"::"_s} << QStringList {} << QStringList {} << ResultFlags {};
        QTest::newRow("any interface, both disabled")
            << anyIface(BIND_DISABLED, BIND_DISABLED)
            << QStringList {} << QStringList {} << QStringList {} << ResultFlags {};
        QTest::newRow("any interface, temporary IPv6")
            << anyIface(BIND_ALL_ADDRESSES, BIND_TEMPORARY_IPV6_ADDRESSES)
            << QStringList {u"0.0.0.0"_s, IPV6_TEMPORARY_1, IPV6_TEMPORARY_2} << QStringList {} << QStringList {} << ResultFlags {};
        QTest::newRow("any interface, temporary IPv6 without temporary addresses")
            << anyIface(BIND_ALL_ADDRESSES, BIND_TEMPORARY_IPV6_ADDRESSES, ALL_ADDRESSES, {})
            << QStringList {u"0.0.0.0"_s} << QStringList {} << QStringList {} << ResultFlags {HasNoTemporaryIPv6Address};
        QTest::newRow("any interface, IPv4 disabled, temporary IPv6 without temporary addresses")
            << anyIface(BIND_DISABLED, BIND_TEMPORARY_IPV6_ADDRESSES, ALL_ADDRESSES, {})
            << QStringList {} << QStringList {} << QStringList {} << ResultFlags {HasNoTemporaryIPv6Address};
        QTest::newRow("any interface, specific IPv4")
            << anyIface(IPV4_1, BIND_ALL_ADDRESSES)
            << QStringList {IPV4_1, u"::"_s} << QStringList {} << QStringList {} << ResultFlags {};
        QTest::newRow("any interface, specific IPv6")
            << anyIface(BIND_ALL_ADDRESSES, IPV6_STABLE)
            << QStringList {u"0.0.0.0"_s, IPV6_STABLE} << QStringList {} << QStringList {} << ResultFlags {};
        QTest::newRow("any interface, missing addresses")
            << anyIface(MISSING_IPV4, MISSING_IPV6)
            << QStringList {MISSING_IPV4, MISSING_IPV6} << QStringList {} << QStringList {MISSING_IPV4, MISSING_IPV6}
            << ResultFlags {};
        QTest::newRow("any interface, temporary as IPv4")
            << anyIface(BIND_TEMPORARY_IPV6_ADDRESSES, BIND_ALL_ADDRESSES)
            << QStringList {u"::"_s} << QStringList {BIND_TEMPORARY_IPV6_ADDRESSES} << QStringList {} << ResultFlags {};
        QTest::newRow("any interface, other families")
            << anyIface(IPV6_STABLE, IPV4_1)
            << QStringList {} << QStringList {IPV6_STABLE, IPV4_1} << QStringList {} << ResultFlags {};
        QTest::newRow("any interface, wildcards")
            << anyIface(u"0.0.0.0"_s, u"::"_s)
            << QStringList {} << QStringList {u"0.0.0.0"_s, u"::"_s} << QStringList {} << ResultFlags {};
        QTest::newRow("any interface, IPv4 disabled, IPv4-mapped IPv6")
            << anyIface(BIND_DISABLED, u"::ffff:192.0.2.10"_s)
            << QStringList {} << QStringList {u"::ffff:192.0.2.10"_s} << QStringList {} << ResultFlags {};
        QTest::newRow("any interface, IPv4 disabled, IPv6 containing ffff")
            << anyIface(BIND_DISABLED, u"2001:db8::ffff:1"_s)
            << QStringList {u"2001:db8::ffff:1"_s} << QStringList {} << QStringList {u"2001:db8::ffff:1"_s} << ResultFlags {};

        // selected interface, all addresses of both families: the interface name only

        QTest::newRow("interface, all addresses")
            << iface(BIND_ALL_ADDRESSES, BIND_ALL_ADDRESSES)
            << QStringList {IFACE} << QStringList {} << QStringList {} << ResultFlags {};
        QTest::newRow("interface, all addresses without addresses")
            << iface(BIND_ALL_ADDRESSES, BIND_ALL_ADDRESSES, {}, {})
            << QStringList {IFACE} << QStringList {} << QStringList {} << ResultFlags {};
        QTest::newRow("invalid interface, all addresses")
            << invalidIface(BIND_ALL_ADDRESSES, BIND_ALL_ADDRESSES)
            << QStringList {INVALID_IFACE} << QStringList {} << QStringList {} << ResultFlags {IsIfaceInvalid};

        // selected interface, all addresses of one family: never a wildcard

        QTest::newRow("interface, all IPv4, IPv6 disabled")
            << iface(BIND_ALL_ADDRESSES, BIND_DISABLED)
            << QStringList {IPV4_1, IPV4_2} << QStringList {} << QStringList {} << ResultFlags {};
        QTest::newRow("interface without IPv4, all IPv4, IPv6 disabled")
            << iface(BIND_ALL_ADDRESSES, BIND_DISABLED, makeAddresses({IPV6_STABLE, IPV6_LINK_LOCAL}), {})
            << QStringList {} << QStringList {} << QStringList {} << ResultFlags {HasNoIPv4Address};
        QTest::newRow("interface, all IPv6, IPv4 disabled")
            << iface(BIND_DISABLED, BIND_ALL_ADDRESSES)
            << QStringList {IPV6_STABLE, IPV6_TEMPORARY_1, IPV6_TEMPORARY_2, IPV6_LINK_LOCAL} << QStringList {} << QStringList {}
            << ResultFlags {};
        QTest::newRow("interface without IPv6, all IPv6, IPv4 disabled")
            << iface(BIND_DISABLED, BIND_ALL_ADDRESSES, makeAddresses({IPV4_1}), {})
            << QStringList {} << QStringList {} << QStringList {} << ResultFlags {HasNoIPv6Address};
        QTest::newRow("interface without IPv6, specific IPv4, all IPv6")
            << iface(IPV4_1, BIND_ALL_ADDRESSES, makeAddresses({IPV4_1}), {})
            << QStringList {IPV4_1} << QStringList {} << QStringList {} << ResultFlags {HasNoIPv6Address};
        QTest::newRow("interface, all IPv4, temporary IPv6")
            << iface(BIND_ALL_ADDRESSES, BIND_TEMPORARY_IPV6_ADDRESSES)
            << QStringList {IPV4_1, IPV4_2, IPV6_TEMPORARY_1, IPV6_TEMPORARY_2} << QStringList {} << QStringList {}
            << ResultFlags {};
        QTest::newRow("interface, all IPv4, temporary IPv6 without temporary addresses")
            << iface(BIND_ALL_ADDRESSES, BIND_TEMPORARY_IPV6_ADDRESSES, IFACE_ADDRESSES, {})
            << QStringList {IPV4_1, IPV4_2} << QStringList {} << QStringList {} << ResultFlags {HasNoTemporaryIPv6Address};
        QTest::newRow("interface without addresses, all IPv4, temporary IPv6")
            << iface(BIND_ALL_ADDRESSES, BIND_TEMPORARY_IPV6_ADDRESSES, {}, {})
            << QStringList {} << QStringList {} << QStringList {} << ResultFlags {HasNoIPv4Address | HasNoTemporaryIPv6Address};
        QTest::newRow("interface, all IPv4, specific IPv6")
            << iface(BIND_ALL_ADDRESSES, IPV6_STABLE)
            << QStringList {IPV4_1, IPV4_2, IPV6_STABLE} << QStringList {} << QStringList {} << ResultFlags {};
        QTest::newRow("interface, specific IPv4, all IPv6")
            << iface(IPV4_2, BIND_ALL_ADDRESSES)
            << QStringList {IPV4_2, IPV6_STABLE, IPV6_TEMPORARY_1, IPV6_TEMPORARY_2, IPV6_LINK_LOCAL} << QStringList {} << QStringList {}
            << ResultFlags {};

        // selected interface, specific addresses

        QTest::newRow("interface, specific IPv4")
            << iface(IPV4_2, BIND_DISABLED)
            << QStringList {IPV4_2} << QStringList {} << QStringList {} << ResultFlags {};
        QTest::newRow("interface, missing IPv4")
            << iface(MISSING_IPV4, BIND_DISABLED)
            << QStringList {MISSING_IPV4} << QStringList {} << QStringList {MISSING_IPV4} << ResultFlags {};
        QTest::newRow("interface, specific IPv6")
            << iface(BIND_DISABLED, IPV6_STABLE)
            << QStringList {IPV6_STABLE} << QStringList {} << QStringList {} << ResultFlags {};
        QTest::newRow("interface, specific IPv6 in upper case")
            << iface(BIND_DISABLED, u"2001:DB8::1"_s)
            << QStringList {IPV6_STABLE} << QStringList {} << QStringList {} << ResultFlags {};
        QTest::newRow("interface, missing IPv6 in upper case")
            << iface(BIND_DISABLED, u"2001:DB8:1::1"_s)
            << QStringList {MISSING_IPV6} << QStringList {} << QStringList {u"2001:DB8:1::1"_s} << ResultFlags {};
        QTest::newRow("interface, link-local IPv6 without scope id")
            << iface(BIND_DISABLED, u"fe80::1"_s)
            << QStringList {u"fe80::1"_s} << QStringList {} << QStringList {} << ResultFlags {};
        QTest::newRow("interface, link-local IPv6 with scope id")
            << iface(BIND_DISABLED, u"fe80::1%eth0"_s, makeAddresses({u"fe80::1"_s}), {})
            << QStringList {u"fe80::1%eth0"_s} << QStringList {} << QStringList {} << ResultFlags {};
        QTest::newRow("interface, specific IPv4 and IPv6")
            << iface(IPV4_1, IPV6_TEMPORARY_1)
            << QStringList {IPV4_1, IPV6_TEMPORARY_1} << QStringList {} << QStringList {} << ResultFlags {};

        // selected interface, values that are not valid for their family

        QTest::newRow("interface, other families")
            << iface(IPV6_STABLE, IPV4_1)
            << QStringList {} << QStringList {IPV6_STABLE, IPV4_1} << QStringList {} << ResultFlags {};
        QTest::newRow("interface, wildcards")
            << iface(u"0.0.0.0"_s, u"::"_s)
            << QStringList {} << QStringList {u"0.0.0.0"_s, u"::"_s} << QStringList {} << ResultFlags {};
        QTest::newRow("interface, IPv6 wildcard in long form")
            << iface(BIND_DISABLED, u"0:0:0:0:0:0:0:0"_s)
            << QStringList {} << QStringList {u"0:0:0:0:0:0:0:0"_s} << QStringList {} << ResultFlags {};
        QTest::newRow("interface, all IPv4, IPv4-mapped IPv4 wildcard")
            << iface(BIND_ALL_ADDRESSES, u"::ffff:0.0.0.0"_s)
            << QStringList {IPV4_1, IPV4_2} << QStringList {u"::ffff:0.0.0.0"_s} << QStringList {} << ResultFlags {};
        QTest::newRow("interface, garbage")
            << iface(u"192.0.2.256"_s, u"2001:db8::g"_s)
            << QStringList {} << QStringList {u"192.0.2.256"_s, u"2001:db8::g"_s} << QStringList {} << ResultFlags {};
        QTest::newRow("interface, interface name as addresses")
            << iface(IFACE, IFACE)
            << QStringList {} << QStringList {IFACE, IFACE} << QStringList {} << ResultFlags {};
        QTest::newRow("interface, temporary as IPv4")
            << iface(BIND_TEMPORARY_IPV6_ADDRESSES, BIND_DISABLED)
            << QStringList {} << QStringList {BIND_TEMPORARY_IPV6_ADDRESSES} << QStringList {} << ResultFlags {};

        // invalid interface: it has no addresses

        QTest::newRow("invalid interface, specific IPv4")
            << invalidIface(IPV4_1, BIND_DISABLED)
            << QStringList {IPV4_1} << QStringList {} << QStringList {IPV4_1} << ResultFlags {IsIfaceInvalid};
        QTest::newRow("invalid interface, specific IPv4 and IPv6")
            << invalidIface(IPV4_1, IPV6_STABLE)
            << QStringList {IPV4_1, IPV6_STABLE} << QStringList {} << QStringList {IPV4_1, IPV6_STABLE} << ResultFlags {IsIfaceInvalid};
        QTest::newRow("invalid interface, all IPv4, IPv6 disabled")
            << invalidIface(BIND_ALL_ADDRESSES, BIND_DISABLED)
            << QStringList {} << QStringList {} << QStringList {} << ResultFlags {IsIfaceInvalid | HasNoIPv4Address};
        QTest::newRow("invalid interface, IPv4 disabled, temporary IPv6")
            << invalidIface(BIND_DISABLED, BIND_TEMPORARY_IPV6_ADDRESSES)
            << QStringList {} << QStringList {} << QStringList {} << ResultFlags {IsIfaceInvalid | HasNoTemporaryIPv6Address};
        QTest::newRow("invalid interface, garbage, all IPv6")
            << invalidIface(u"abc"_s, BIND_ALL_ADDRESSES)
            << QStringList {} << QStringList {u"abc"_s} << QStringList {} << ResultFlags {IsIfaceInvalid | HasNoIPv6Address};
        QTest::newRow("invalid interface, both disabled")
            << invalidIface(BIND_DISABLED, BIND_DISABLED)
            << QStringList {} << QStringList {} << QStringList {} << ResultFlags {IsIfaceInvalid};
    }

    void testResolveListeningAddresses() const
    {
        QFETCH(Input, input);
        QFETCH(QStringList, IPs);
        QFETCH(QStringList, invalidAddresses);
        QFETCH(QStringList, missingAddresses);
        QFETCH(ResultFlags, flags);

        const BitTorrent::ListeningAddresses result = BitTorrent::resolveListeningAddresses(input);
        QCOMPARE(result.IPs, IPs);
        QCOMPARE(result.isIfaceInvalid, flags.testFlag(IsIfaceInvalid));
        QCOMPARE(result.invalidAddresses, invalidAddresses);
        QCOMPARE(result.missingAddresses, missingAddresses);
        QCOMPARE(result.hasNoIPv4Address, flags.testFlag(HasNoIPv4Address));
        QCOMPARE(result.hasNoIPv6Address, flags.testFlag(HasNoIPv6Address));
        QCOMPARE(result.hasNoTemporaryIPv6Address, flags.testFlag(HasNoTemporaryIPv6Address));
    }
};

QTEST_APPLESS_MAIN(TestBittorrentListeningAddresses)
#include "testbittorrentlisteningaddresses.moc"
