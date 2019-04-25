#include "BTC.h"
#include "bitcoin/base58.h"
#include "Util.h"
#include <QString>
#include <string.h>
#include <iostream>
#include <QMap>
#include <QSet>
#include <QPair>
#include <utility>
namespace BTC
{
    // Map of Net -> [Map of VerByte -> Kind]
    static QMap<Net, QMap<quint8, Address::Kind> > netVerByteKindMap = {
        { MainNet, { {0, Address::P2PKH },  {5, Address::P2SH} } },
        { TestNet, { {111, Address::P2PKH },{196, Address::P2SH} } },
    };

    /// -- Address --

    Address::Address(const QString &legacyAddress)
    {
        *this = Address::fromString(legacyAddress);
    }

    /*static*/
    Address Address::fromString(const QString &legacyAddress)
    {
        Address a;
        ByteArray dec;
        try {
            if (!bitcoin::DecodeBase58Check(legacyAddress.toUtf8().constData(), dec)) {
                Debug() << __FUNCTION__ << ": got bad address " << legacyAddress;
                return a;
            }
        } catch (const std::runtime_error &e) {
            Error() << "Internal error decoding address " << legacyAddress << ": " << e.what();
            return a;
        }
        a.verByte = dec[0];
        a.h160.resize(int(dec.size()-1));
        memcpy(a.h160.data(), &dec[1], dec.size()-1);
        a.net = BTC::Invalid;
        // figure out the net based on the verbyte, if the verbyte is in our map
        for (auto it = netVerByteKindMap.begin(); it != netVerByteKindMap.end(); ++it) {
            if (it.value().contains(a.verByte))
                a.net = it.key();
        }
        return a;
    }


    bool Address::isValid() const
    {
        return kind() != Invalid;
    }

    Address::Kind Address::kind() const
    {
        // NB: all the isValid() functions eventually end up here.
        if (h160.length() == 20) {
            auto it = netVerByteKindMap.find(net);
            if (it != netVerByteKindMap.end()) {
                auto it2 = it.value().find(verByte);
                if (it2 != it.value().end()) {
                    return it2.value();
                }
            }
        }
        return Invalid;
    }

    ByteArray Address::toScript() const
    {
        ByteArray script;
        if (kind() == P2PKH) { // kind() checks for validity
            script = ByteArray({
                OP_DUP, OP_HASH160, Byte(h160.length())
            }) + h160 + ByteArray({ OP_EQUALVERIFY, OP_CHECKSIG });
        } else if (kind() == P2SH) {
            script = ByteArray({
                OP_HASH160, quint8(h160.length())
            }) + h160 + ByteArray({ OP_EQUAL });
        }
        return script;
    }

    ByteArray Address::toScriptHash() const
    {
        ByteArray script(toScript()), ret;
        if (!script.isEmpty()) {
            auto hash = bitcoin::HashOnce(script.begin(), script.end());
            ret.insert(ret.end(), hash.begin(), hash.end());
        }
        return ret;
    }

    /// returns the ElectrumX 'scripthash_hex'
    QByteArray Address::toHashX() const
    {
        if (!cachedHashX.isEmpty())
            return cachedHashX;
        QByteArray ret;
        auto script = toScript();
        if (!script.isEmpty()) {
            // Note as a performance tweak here we don't call toScriptHash() as that would do extra copying.
            // Instead, we just reproduce some of its work here.
            bitcoin::uint256 hash = bitcoin::HashOnce(script.begin(), script.end());
            auto str = hash.GetHex(); /// this is reversed hex
            ret = str.c_str();
            cachedHashX = ret;
        }
        return ret;
    }

    /// if isValid, returns the legacy address string, base58 encoded
    QString Address::toString() const {
        QString ret;
        if (isValid()) {
            ByteArray vch = ByteArray({verByte}) + h160;
            auto str = bitcoin::EncodeBase58Check(vch);
            ret = QString::fromUtf8(str.c_str());
        }
        return ret;
    }

    /*static*/
    bool Address::isValid(const QString &legacyAddress, Net net)
    {
        Address a(legacyAddress);
        return a.isValid() && a.net == net;
    }


    /*static*/
    bool Address::test()
    {
        //const char *badAddress = "1C3SoftYBC2bbDzCadZxDrfbnobEXLBLQQ";
        const char *anAddress = "1C3SoftYBC2bbDzCadZxDrfbnobEXLBLQZ";
        Address a = anAddress, b; //bad(badAddress);
        b = a;
        Address c(a);
        c = b;
        std::cout << "Decoded -> VerByte: " << int(a.verByte) <<  "  Hash160 (hex): " << a.h160.toHex().constData() << std::endl;
        ByteArray v = { 'a', ' ', 'b', 'c', 0 };
        ByteArray v2 = "this is a test";
        auto vcat = ByteArray({'a','b','c',' '}) + v2;
        std::vector<Byte> v3(v2); // support construction from ByteArray to vector
        ByteArray v4(v3); // support construction from vector to ByteArray
        std::cout << "Init list test: " << v.data() << std::endl;
        std::cout << "Init string test: " << v2.charData() << std::endl;
        std::cout << "Chained c'tor string test: " << v4.charData() << std::endl;
        std::cout << "Concat test: " << (vcat + ByteArray({0})).charData() << std::endl;
        std::cout << "Concat test 2: " << ((vcat+"..more stuff")+ByteArray({'z','z','z',0})).charData() << std::endl;
        std::cout << "v < v2 : " << int(v < v2) << std::endl;
        std::cout << "IsValid: " << a.isValid() << " kind: " << a.kind() << std::endl;
        std::cout << "Script Hex of: " << a.toString().toUtf8().constData() << " = " << a.toScript().toQHex().constData() << std::endl;
        std::cout << "Script Hash (Hex) of: " << a.toString().toUtf8().constData() << " = " << a.toScriptHash().toQHex().constData() << std::endl;
        std::cout << "HashX of " << a.toString().toUtf8().constData() << " = " << a.toHashX().constData() << std::endl;
        c = a;
        std::cout << "HashX again " << c.toString().toUtf8().constData() << " = " << c.toHashX().constData() << std::endl;
        std::cout << "c==a : " << int(c==a) << std::endl;
        std::cout << "c==b : " << int(c==b) << " (cached?,cached?): (" << int(!c.cachedHashX.isEmpty()) << "," << int(!b.cachedHashX.isEmpty()) << ")" << std::endl;
        //std::cout << "Testnet: " << a.toString().toUtf8().constData() << std::endl;
        return a.isValid() && a.toString() == anAddress && a == b;
    }

    // -- ByteArray --
    ByteArray::ByteArray() : std::vector<Byte>() {}
    ByteArray::ByteArray(const std::vector<Byte> &b) : std::vector<Byte>(b) {}
    ByteArray::ByteArray(std::vector<Byte> &&o) : std::vector<Byte>(std::move(o)) {}
    ByteArray::ByteArray(const std::initializer_list<Byte> &il) : std::vector<Byte>(il) {}
    ByteArray::ByteArray(const QByteArray &a) { (*this) = a; } // leverage operator=
    ByteArray::ByteArray(const QString &s) { (*this) = s; } // leverage operator=
    static Byte emptyBytes[sizeof(long)] = {0}; ///< C++ init would have been zero anyway. We do it like this to illustrate the point to the casual observer.
    Byte *ByteArray::data()
    {
        if (!empty()) return &(*this)[0];
        return emptyBytes;
    }
    const Byte* ByteArray::constData() const
    {
        if (!empty()) return &(*this)[0];
        return emptyBytes;
    }
    ByteArray ByteArray::operator+(const std::vector<Byte> &b) const
    {
        ByteArray ret(*this);
        ret += b;
        return ret;
    }
    ByteArray ByteArray::operator+(const QByteArray & o) const
    {
        ByteArray ret(*this);
        ret += o;
        return ret;
    }
    ByteArray ByteArray::operator+(const QString &s) const
    {
        ByteArray ret(*this);
        ret += s;
        return ret;
    }
    ByteArray ByteArray::operator+(const std::initializer_list<Byte> &il) const
    {
        ByteArray ret(*this);
        ret += il;
        return ret;
    }
    ByteArray & ByteArray::operator+=(const std::vector<Byte> & b)
    {
        if (!b.empty())
            insert(end(), b.begin(), b.end());
        return *this;
    }
    ByteArray & ByteArray::operator+=(const QByteArray &b)
    {
        if (!b.isEmpty())
            insert(end(), b.begin(), b.end());
        return *this;
    }
    ByteArray & ByteArray::operator+=(const QString &s)
    {
        return (*this) += s.toUtf8();
    }
    ByteArray & ByteArray::operator+=(const std::initializer_list<Byte> &il)
    {
        return (*this) += ByteArray(il);
    }

    ByteArray & ByteArray::operator=(const std::vector<Byte> &a)
    {
        clear();
        return (*this) += a;
    }
    ByteArray & ByteArray::operator=(const QByteArray &a)
    {
        clear();
        return (*this) += a;
    }
    ByteArray & ByteArray::operator=(const QString &a)
    {
        clear();
        return (*this) += a;
    }
    ByteArray & ByteArray::operator=(const std::initializer_list<Byte> &il)
    {
        clear();
        return *this += il;
    }
    ByteArray::operator QByteArray() const
    {
        QByteArray ret;
        if (!empty())
            ret.append(reinterpret_cast<const char *>(constData()), length());
        return ret;
    }
    ByteArray ByteArray::toHex() const
    {
        return ByteArray(toQHex());
    }
    QByteArray ByteArray::toQHex() const
    {
        QByteArray qba = *this;
        return qba.toHex();
    }
}
