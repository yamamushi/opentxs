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

#ifndef OPENTXS_CORE_CRYPTO_NYMPARAMETERS_HPP
#define OPENTXS_CORE_CRYPTO_NYMPARAMETERS_HPP

#include <opentxs-proto/verify/VerifyCredentials.hpp>

#include <opentxs/core/crypto/Credential.hpp>
#include <opentxs/core/crypto/OTAsymmetricKey.hpp>

namespace opentxs
{

class NymParameters
{
public:
    enum NymParameterType: int32_t {
      ERROR,
      LEGACY,
      SECP256K1
    };

    NymParameterType nymParameterType();

    OTAsymmetricKey::KeyType AsymmetricKeyType() const;

    void setNymParameterType(NymParameterType theKeytype);

    Credential::CredentialType credentialType() const;

    void setCredentialType(Credential::CredentialType theCredentialtype);

    inline proto::SourceType SourceType() const
    {
        return sourceType_;
    }

    inline void SetSourceType(proto::SourceType sType)
    {
        sourceType_ = sType;
    }

    inline proto::SourceProofType SourceProofType() const
    {
        return sourceProofType_;
    }

    inline void SetSourceProofType(proto::SourceProofType sType)
    {
        sourceProofType_ = sType;
    }

#if defined(OT_CRYPTO_SUPPORTED_KEY_RSA)
    int32_t keySize();

    void setKeySize(int32_t keySize);
    NymParameters(const int32_t keySize);
#endif

    NymParameters(
        NymParameterType theKeytype,
        Credential::CredentialType theCredentialtype);
    NymParameters() = default;
    ~NymParameters() = default;

#if defined(OT_CRYPTO_WITH_BIP32)
    inline uint32_t Nym() const
    {
        return nym_;
    }

    inline void SetNym(const uint32_t path)
    {
        nym_ = path;
    }
    inline uint32_t Credset() const
    {
        return credset_;
    }

    inline void SetCredset(const uint32_t path)
    {
        credset_ = path;
    }
    inline uint32_t CredIndex() const
    {
        return cred_index_;
    }

    inline void SetCredIndex(const uint32_t path)
    {
        cred_index_ = path;
    }
#endif

private:
    proto::SourceType sourceType_ = proto::SOURCETYPE_PUBKEY;
    proto::SourceProofType sourceProofType_ =
        proto::SOURCEPROOFTYPE_SELF_SIGNATURE;

#if defined(OT_CRYPTO_SUPPORTED_KEY_SECP256K1)
    NymParameterType nymType_ = NymParameterType::SECP256K1;
    Credential::CredentialType credentialType_ = Credential::HD;
#elif defined(OT_CRYPTO_SUPPORTED_KEY_RSA)
    NymParameterType nymType_ = NymParameterType::LEGACY;
    Credential::CredentialType credentialType_ = Credential::LEGACY;
#else
    NymParameterType nymType_ = NymParameterType::ERROR;
    Credential::CredentialType credentialType_ = Credential::ERROR_TYPE;
#endif

//----------------------------------------
// CRYPTO ALGORITHMS
//----------------------------------------
#if defined(OT_CRYPTO_SUPPORTED_KEY_RSA)
  int32_t nBits_ = 1024;
#endif
#if defined(OT_CRYPTO_WITH_BIP32)
  uint32_t nym_ = 0;
  uint32_t credset_ = 0;
  uint32_t cred_index_ = 0;
#endif

};

} // namespace opentxs

#endif // OPENTXS_CORE_CRYPTO_NYMPARAMETERS_HPP