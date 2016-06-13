/************************************************************
 *
 *                 OPEN TRANSACTIONS
 *
 *       Financial Cryptography and Digital Cash
 *       Library, Protocol, API, Server, CLI, GUI
 *
 *       -- Anonymous Numbered Accounts.
 *       -- Untraceable Digital Cash.
 *       -- Triple-Signed Receipts.
 *       -- Cheques, Vouchers, Transfers, Inboxes.
 *       -- Basket Currencies, Markets, Payment Plans.
 *       -- Signed, XML, Ricardian-style Contracts.
 *       -- Scripted smart contracts.
 *
 *  EMAIL:
 *  fellowtraveler@opentransactions.org
 *
 *  WEBSITE:
 *  http://www.opentransactions.org/
 *
 *  -----------------------------------------------------
 *
 *   LICENSE:
 *   This Source Code Form is subject to the terms of the
 *   Mozilla Public License, v. 2.0. If a copy of the MPL
 *   was not distributed with this file, You can obtain one
 *   at http://mozilla.org/MPL/2.0/.
 *
 *   DISCLAIMER:
 *   This program is distributed in the hope that it will
 *   be useful, but WITHOUT ANY WARRANTY; without even the
 *   implied warranty of MERCHANTABILITY or FITNESS FOR A
 *   PARTICULAR PURPOSE.  See the Mozilla Public License
 *   for more details.
 *
 ************************************************************/

#include "opentxs/core/crypto/TrezorCrypto.hpp"

#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/OTData.hpp"
#include "opentxs/core/Proto.hpp"
#include "opentxs/core/String.hpp"
#include "opentxs/core/app/App.hpp"
#include "opentxs/core/crypto/CryptoEngine.hpp"
#include "opentxs/core/crypto/CryptoHash.hpp"
#include "opentxs/core/crypto/CryptoSymmetric.hpp"
#include "opentxs/core/crypto/OTAsymmetricKey.hpp"
#include "opentxs/core/crypto/OTPassword.hpp"
#include "opentxs/core/util/Assert.hpp"

extern "C" {
#include <trezor-crypto/bip32.h>
#include <trezor-crypto/bip39.h>
#include <trezor-crypto/curves.h>
}

#include <stdint.h>
#include <memory>
#include <string>

namespace opentxs
{

std::string TrezorCrypto::toWords(const OTPassword& seed) const
{
    std::string wordlist(
        ::mnemonic_from_data(
            static_cast<const uint8_t*>(seed.getMemory()),
            seed.getMemorySize()));
    return wordlist;
}

void TrezorCrypto::WordsToSeed(
    const std::string words,
    OTPassword& seed,
    const std::string passphrase) const
{
    OT_ASSERT_MSG(!words.empty(), "Mnemonic was blank.");
    OT_ASSERT_MSG(!passphrase.empty(), "Passphrase was blank.");

    seed.SetSize(512/8);

    ::mnemonic_to_seed(
        words.c_str(),
        passphrase.c_str(),
        static_cast<uint8_t*>(seed.getMemoryWritable()),
        nullptr);
}

std::string TrezorCrypto::SeedToFingerprint(
    const EcdsaCurve& curve,
    const OTPassword& seed) const
{
    auto node = InstantiateHDNode(curve, seed);

    if (node) {
        OTData pubkey(
            static_cast<void*>(node->public_key),
            sizeof(node->public_key));
        Identifier identifier;
        identifier.CalculateDigest(pubkey);
        String fingerprint(identifier);

        return fingerprint.Get();
    }

    return "";
}

serializedAsymmetricKey TrezorCrypto::SeedToPrivateKey(
    const EcdsaCurve& curve,
    const OTPassword& seed) const
{
    serializedAsymmetricKey derivedKey;
    auto node = InstantiateHDNode(curve, seed);

    OT_ASSERT_MSG(node, "Derivation of root node failed.");

    if (node) {
        derivedKey = HDNodeToSerialized(
            CryptoAsymmetric::CurveToKeyType(curve),
            *node,
            TrezorCrypto::DERIVE_PRIVATE);

        if (derivedKey) {
            OTPassword root;
            App::Me().Crypto().Hash().Digest(
                proto::HASHTYPE_BTC160,
                seed,
                root);
            derivedKey->mutable_path()->set_root(
                root.getMemory(),
                root.getMemorySize());
        }
    }

    return derivedKey;
}

serializedAsymmetricKey TrezorCrypto::GetChild(
    const proto::AsymmetricKey& parent,
    const uint32_t index) const
{
    auto node = SerializedToHDNode(parent);

    if (proto::KEYMODE_PRIVATE == parent.mode()) {
        hdnode_private_ckd(node.get(), index);
    } else {
        hdnode_public_ckd(node.get(), index);
    }
    serializedAsymmetricKey key = HDNodeToSerialized(
        parent.type(),
        *node,
        TrezorCrypto::DERIVE_PRIVATE);

    return key;
}

serializedAsymmetricKey TrezorCrypto::HDNodeToSerialized(
    const proto::AsymmetricKeyType& type,
    const HDNode& node,
    const DerivationMode privateVersion) const
{
    serializedAsymmetricKey key = std::make_shared<proto::AsymmetricKey>();

    key->set_version(1);
    key->set_type(type);

    if (privateVersion) {
        key->set_mode(proto::KEYMODE_PRIVATE);
        key->set_chaincode(node.chain_code, sizeof(node.chain_code));

        OTPassword plaintextKey;
        plaintextKey.setMemory(node.private_key, sizeof(node.private_key));
        OTData encryptedKey;
        BinarySecret masterPassword(
            App::Me().Crypto().AES().InstantiateBinarySecretSP());
        masterPassword = CryptoSymmetric::GetMasterKey("");

        bool encrypted = Libsecp256k1::EncryptPrivateKey(
            plaintextKey,
            *masterPassword,
            encryptedKey);

        if (encrypted) {
            key->set_key(encryptedKey.GetPointer(), encryptedKey.GetSize());
        }
    } else {
        key->set_mode(proto::KEYMODE_PUBLIC);
        key->set_key(node.public_key, sizeof(node.public_key));
    }

    return key;
}

std::unique_ptr<HDNode> TrezorCrypto::InstantiateHDNode(const EcdsaCurve& curve)
{
    auto entropy = App::Me().Crypto().AES().InstantiateBinarySecretSP();

    OT_ASSERT_MSG(entropy, "Failed to obtain entropy.");

    entropy->randomizeMemory(256/8);

    auto output = InstantiateHDNode(curve, *entropy);

    OT_ASSERT(output);

    output->depth = 0;
    output->fingerprint = 0;
    output->child_num = 0;
    OTPassword::zeroMemory(output->chain_code, sizeof(output->chain_code));
    OTPassword::zeroMemory(output->private_key, sizeof(output->private_key));
    OTPassword::zeroMemory(output->public_key, sizeof(output->public_key));

    return output;
}

std::unique_ptr<HDNode> TrezorCrypto::InstantiateHDNode(
    const EcdsaCurve& curve,
    const OTPassword& seed)
{
    std::unique_ptr<HDNode> output;
    output.reset(new HDNode);

    OT_ASSERT_MSG(output, "Instantiation of HD node failed.");

    auto curveName = CurveName(curve);

    if (1 > curveName.size()) { return output; }

    int result = ::hdnode_from_seed(
        static_cast<const uint8_t*>(seed.getMemory()),
        seed.getMemorySize(),
        CurveName(curve).c_str(),
        output.get());

    OT_ASSERT_MSG((1 == result), "Setup of HD node failed.");

    ::hdnode_fill_public_key(output.get());

    return output;
}

std::unique_ptr<HDNode> TrezorCrypto::SerializedToHDNode(
    const proto::AsymmetricKey& serialized) const
{
    auto node = InstantiateHDNode(
        CryptoAsymmetric::KeyTypeToCurve(serialized.type()));

    OTPassword::safe_memcpy(
        &(node->chain_code[0]),
        sizeof(node->chain_code),
        serialized.chaincode().c_str(),
        serialized.chaincode().size(),
        false);

    if (proto::KEYMODE_PRIVATE == serialized.mode()) {

        OTData encryptedKey(
            serialized.key().c_str(),
            serialized.key().size());
        BinarySecret plaintextKey(
            App::Me().Crypto().AES().InstantiateBinarySecretSP());
        BinarySecret masterPassword(
            App::Me().Crypto().AES().InstantiateBinarySecretSP());
        masterPassword = CryptoSymmetric::GetMasterKey("");

        Libsecp256k1::DecryptPrivateKey(
            encryptedKey,
            *masterPassword,
            *plaintextKey);

        OTPassword::safe_memcpy(
            &(node->private_key[0]),
            sizeof(node->private_key),
            plaintextKey->getMemory(),
            plaintextKey->getMemorySize(),
            false);
    } else {
        OTPassword::safe_memcpy(
            &(node->public_key[0]),
            sizeof(node->public_key),
            serialized.key().c_str(),
            serialized.key().size(),
            false);
    }

    return node;
}

serializedAsymmetricKey TrezorCrypto::PrivateToPublic(
    const proto::AsymmetricKey& key) const
{
    serializedAsymmetricKey publicVersion;
    std::shared_ptr<HDNode> node = SerializedToHDNode(key);

    if (node) {
        hdnode_fill_public_key(node.get());

        // This will cause the next function to serialize as public
        OTPassword::zeroMemory(node->private_key, sizeof(node->private_key));

        publicVersion = HDNodeToSerialized(
            key.type(),
            *node,
            TrezorCrypto::DERIVE_PUBLIC);

        if (publicVersion) {
            publicVersion->set_role(key.role());
        }
    }

    return publicVersion;
}

std::string TrezorCrypto::CurveName(const EcdsaCurve& curve)
{
    switch (curve) {
        case (EcdsaCurve::SECP256K1) : {
            return ::SECP256K1_NAME;
        }
        default : {}
    }

    return "";
}
} // namespace opentxs
