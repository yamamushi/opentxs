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

#include "stdafx.hpp"

#include "opentxs/client/OTWallet.hpp"

#include "opentxs/api/client/Wallet.hpp"
#include "opentxs/api/crypto/Crypto.hpp"
#include "opentxs/api/storage/Storage.hpp"
#include "opentxs/api/Native.hpp"
#if OT_CASH
#include "opentxs/cash/Purse.hpp"
#endif  // OT_CASH
#include "opentxs/core/crypto/Bip32.hpp"
#include "opentxs/core/crypto/Bip39.hpp"
#include "opentxs/core/crypto/NymParameters.hpp"
#include "opentxs/core/crypto/OTASCIIArmor.hpp"
#include "opentxs/core/crypto/OTCachedKey.hpp"
#include "opentxs/core/crypto/OTPassword.hpp"
#include "opentxs/core/crypto/OTPasswordData.hpp"
#include "opentxs/core/crypto/OTSymmetricKey.hpp"
#include "opentxs/core/util/Assert.hpp"
#include "opentxs/core/util/OTDataFolder.hpp"
#include "opentxs/core/util/Tag.hpp"
#include "opentxs/core/Account.hpp"
#include "opentxs/core/Contract.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/Nym.hpp"
#include "opentxs/core/OTStorage.hpp"
#include "opentxs/core/OTStringXML.hpp"
#include "opentxs/OT.hpp"
#include "opentxs/Proto.hpp"
#include "opentxs/Types.hpp"

#include <irrxml/irrXML.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

namespace opentxs
{

OTWallet::OTWallet(
    const api::Crypto& crypto,
    const api::storage::Storage& storage)
    : Lockable()
    , crypto_(crypto)
    , storage_(storage)
#if OT_CASH
    , m_pWithdrawalPurse(nullptr)
#endif
    , m_strName()
    , m_strVersion()
    , m_strFilename()
    , m_strDataFolder(OTDataFolder::Get())
    , m_mapAccounts()
    , m_mapExtraKeys()
{
}

void OTWallet::release(const Lock&)
{
    m_mapAccounts.clear();
    m_mapExtraKeys.clear();
}

#if OT_CASH
// While waiting on server response to a withdrawal, we keep the private coin
// data here so we can unblind the response.
// This information is so important (as important as the digital cash token
// itself, until the unblinding is done) that we need to save the file right
// away.
void OTWallet::AddPendingWithdrawal(const Purse& thePurse)
{
    Lock lock(lock_);
    // TODO maintain a list here (I don't know why, the server response is
    // nearly
    // instant and then it's done.)

    // TODO notice I don't check the pointer here to see if it's already set, I
    // just start using it.. Fix that.
    m_pWithdrawalPurse = const_cast<Purse*>(&thePurse);
}  // TODO WARNING: If this data is lost before the transaction is completed,
   // the user will be unable to unblind his tokens and make them spendable.
   // So this data MUST be SAVED until the successful withdrawal is verified!

void OTWallet::RemovePendingWithdrawal()
{
    Lock lock(lock_);

    if (m_pWithdrawalPurse) delete m_pWithdrawalPurse;

    m_pWithdrawalPurse = nullptr;
}
#endif  // OT_CASH

std::string OTWallet::GetPhrase()
{
#if OT_CRYPTO_SUPPORTED_KEY_HD
    const std::string defaultFingerprint = storage_.DefaultSeed();
    const bool firstTime = defaultFingerprint.empty();

    if (firstTime) {
        Lock lock(lock_);
        save_wallet(lock);
    }

    return crypto_.BIP39().Passphrase(defaultFingerprint);
#else
    return "";
#endif
}

std::string OTWallet::GetSeed()
{
#if OT_CRYPTO_SUPPORTED_KEY_HD
    const std::string defaultFingerprint = storage_.DefaultSeed();
    const bool firstTime = defaultFingerprint.empty();

    if (firstTime) {
        Lock lock(lock_);
        save_wallet(lock);
    }

    return crypto_.BIP32().Seed(defaultFingerprint);
#else
    return "";
#endif
}

std::string OTWallet::GetWords()
{
#if OT_CRYPTO_SUPPORTED_KEY_HD
    const std::string defaultFingerprint = storage_.DefaultSeed();
    const bool firstTime = defaultFingerprint.empty();

    if (firstTime) {
        Lock lock(lock_);
        save_wallet(lock);
    }

    return crypto_.BIP39().Words(defaultFingerprint);
#else
    return "";
#endif
}

std::string OTWallet::ImportSeed(
    const OTPassword& words,
    const OTPassword& passphrase) const
{
#if OT_CRYPTO_WITH_BIP39
    return crypto_.BIP39().ImportSeed(words, passphrase);
#else
    return "";
#endif
}

std::size_t OTWallet::GetAccountCount() const { return m_mapAccounts.size(); }

// used by high-level wrapper.
bool OTWallet::GetAccount(
    const std::size_t iIndex,
    Identifier& THE_ID,
    String& THE_NAME) const
{
    Lock lock(lock_);

    if (iIndex < GetAccountCount()) {
        std::size_t iCurrentIndex{0};

        for (auto& [id, entry] : m_mapAccounts) {
            auto& pAccount = std::get<3>(entry);

            OT_ASSERT(pAccount);

            if (iIndex == iCurrentIndex) {
                THE_ID = id;
                pAccount->GetName(THE_NAME);

                return true;
            }

            ++iCurrentIndex;
        }
    }

    return false;
}

#if OT_CASH
Purse* OTWallet::GetPendingWithdrawal()
{
    Lock lock(lock_);

    return m_pWithdrawalPurse;
}
#endif

void OTWallet::DisplayStatistics(String& strOutput) const
{
    Lock lock(lock_);
    strOutput.Concatenate(
        "\n-------------------------------------------------\n");
    strOutput.Concatenate("WALLET STATISTICS:\n");

    strOutput.Concatenate("\nPSEUDONYM(s):\n\n");

    for (auto& it : storage_.LocalNyms()) {
        const auto& nymId = Identifier::Factory(it);
        const auto& pNym = OT::App().Wallet().Nym(nymId);

        OT_ASSERT(pNym);

        pNym->DisplayStatistics(strOutput);
    }

    strOutput.Concatenate(
        "-------------------------------------------------\n");
    strOutput.Concatenate("ACCOUNTS:\n\n");

    for (auto& it : m_mapAccounts) {
        auto& pAccount = std::get<3>(it.second);

        OT_ASSERT_MSG(
            pAccount,
            "nullptr account pointer in "
            "OTWallet::m_mapAccounts, "
            "OTWallet::DisplayStatistics");

        pAccount->DisplayStatistics(strOutput);
        strOutput.Concatenate(
            "-------------------------------------------------\n\n");
    }
}

void OTWallet::add_account(const Lock& lock, std::shared_ptr<Account>& theAcct)
{
    OT_ASSERT(theAcct)
    OT_ASSERT(verify_lock(lock))

    const auto ACCOUNT_ID = Identifier::Factory(*theAcct);
    auto existing = m_mapAccounts.find(ACCOUNT_ID);

    if (m_mapAccounts.end() != existing) {
        auto& account = std::get<3>(existing->second);

        OT_ASSERT(account)

        String name{};
        account->GetName(name);

        if (name.Exists()) { theAcct->SetName(name); }

        account = theAcct;
    } else {
        auto [entry, success] = m_mapAccounts.emplace(
            ACCOUNT_ID,
            AccountEntry{Identifier::Factory(),
                         Identifier::Factory(),
                         Identifier::Factory(),
                         nullptr});

        OT_ASSERT(success)

        auto& [nymID, serverID, unitID, account] = entry->second;
        nymID = theAcct->GetNymID();
        serverID = theAcct->GetPurportedNotaryID();
        unitID = theAcct->GetInstrumentDefinitionID();
        account = theAcct;
    }
}

void OTWallet::AddAccount(std::shared_ptr<Account>& theAcct)
{
    OT_ASSERT(theAcct)

    Lock lock(lock_);
    add_account(lock, theAcct);
}

// // Look up an account by ID and see if it is in the wallet.
// If it is, return a pointer to it, otherwise return nullptr.
std::shared_ptr<Account> OTWallet::get_account(
    const Lock& lock,
    const Identifier& theAccountID)
{
    OT_ASSERT(verify_lock(lock))

    auto it = m_mapAccounts.find(theAccountID);

    if (m_mapAccounts.end() == it) { return nullptr; }

    return std::get<3>(it->second);
}

// // Look up an account by ID and see if it is in the wallet.
// If it is, return a pointer to it, otherwise return nullptr.
std::shared_ptr<Account> OTWallet::GetAccount(const Identifier& theAccountID)
{
    Lock lock(lock_);

    return get_account(lock, theAccountID);
}

// works with the name too.
std::shared_ptr<Account> OTWallet::GetAccountPartialMatch(
    std::string PARTIAL_ID)
{
    Lock lock(lock_);
    // loop through the accounts and find one with a specific ID.
    for (auto& it : m_mapAccounts) {
        auto& pAccount = std::get<3>(it.second);

        OT_ASSERT(pAccount);

        auto anAccountID = Identifier::Factory();
        pAccount->GetIdentifier(anAccountID);
        String strTemp(anAccountID);
        std::string strIdentifier = strTemp.Get();

        if (strIdentifier.compare(0, PARTIAL_ID.length(), PARTIAL_ID) == 0) {

            return pAccount;
        }
    }

    // Okay, let's try it by name, then...
    //
    for (auto& it : m_mapAccounts) {
        auto& pAccount = std::get<3>(it.second);

        OT_ASSERT(pAccount);

        String strName;
        pAccount->GetName(strName);
        std::string str_Name = strName.Get();

        if (str_Name.compare(0, PARTIAL_ID.length(), PARTIAL_ID) == 0) {

            return pAccount;
        }
    }

    return nullptr;
}

std::shared_ptr<Account> OTWallet::GetIssuerAccount(
    const Identifier& theInstrumentDefinitionID)
{
    Lock lock(lock_);
    // loop through the accounts and find one with a specific instrument
    // definition ID. (And with the issuer type set.)
    for (auto& it : m_mapAccounts) {
        auto& pIssuerAccount = std::get<3>(it.second);

        OT_ASSERT(pIssuerAccount);

        if ((pIssuerAccount->GetInstrumentDefinitionID() ==
             theInstrumentDefinitionID) &&
            (pIssuerAccount->IsIssuer())) {

            return pIssuerAccount;
        }
    }

    return nullptr;
}

bool OTWallet::verify_account(
    const Lock& lock,
    const Nym& theNym,
    Account& theAcct,
    const Identifier& NOTARY_ID,
    const String& strAcctID,
    const char* szFuncName)
{
    OT_ASSERT(verify_lock(lock))

    const char* szFunc =
        (nullptr != szFuncName) ? szFuncName : "OTWallet::VerifyAssetAccount";

    if (NOTARY_ID != theAcct.GetRealNotaryID()) {
        const String s1(NOTARY_ID), s2(theAcct.GetRealNotaryID());
        otOut << "OTWallet::VerifyAssetAccount " << szFunc
              << ": Notary ID passed in (" << s1
              << ") didn't match the one "
                 "on the account ("
              << s2 << "). Acct ID: " << strAcctID << "\n";
        return false;
    }

    if (!theAcct.VerifyOwner(theNym))  // Verifies Ownership.
    {
        otOut << "OTWallet::VerifyAssetAccount " << szFunc
              << ": Nym (ID: " << theNym.GetConstID().str()
              << ") is not the owner of the account: " << strAcctID << "\n";
        return false;
    }

    if (false == theAcct.VerifyAccount(theNym))  // Verifies ContractID and
                                                 // Signature.
    {
        otOut << "OTWallet::VerifyAssetAccount " << szFunc
              << ": Account signature or AccountID fails to verify. "
                 "NymID: "
              << theNym.GetConstID().str() << "  AcctID: " << strAcctID << "\n";
        return false;
    }
    // By this point, I know that everything checks out. Signature and Account
    // ID,
    // Nym is owner, etc.

    return true;
}

// No need to cleanup the account returned, it's owned by the wallet.
//
std::shared_ptr<Account> OTWallet::GetOrLoadAccount(
    const Nym& theNym,
    const Identifier& ACCT_ID,
    const Identifier& NOTARY_ID,
    const char* szFuncName)
{
    Lock lock(lock_);
    const char* szFunc =
        (nullptr != szFuncName) ? szFuncName : "OTWallet::GetOrLoadAccount";

    const String strAcctID(ACCT_ID);

    auto pAccount = get_account(lock, ACCT_ID);

    if (nullptr == pAccount)  // It wasn't there already, so we'll have to load
                              // it...
    {
        otOut << "OTWallet::GetOrLoadAccount " << szFunc
              << ": There's no asset account in the wallet with that ID ("
              << strAcctID
              << "). "
                 "Attempting to load it from storage...\n";
        pAccount = load_account(lock, theNym, ACCT_ID, NOTARY_ID, szFuncName);
    }  // pAccount == nullptr.

    // It either was already there, or it loaded successfully...
    //
    if (false == bool(pAccount))  // pAccount EXISTS...
    {
        otErr << "OTWallet::GetOrLoadAccount " << szFunc
              << ": Error loading Asset Account: " << strAcctID << "\n";
        return nullptr;
    }

    return pAccount;
}

// No need to cleanup the account returned, it's owned by the wallet.
//
// We don't care if this asset account is already loaded in the wallet.
// Presumably, the user has just download the latest copy of the account
// from the server, and the one in the wallet is old, so now this function
// is being called to load the new one from storage and update the wallet.
//
std::shared_ptr<Account> OTWallet::load_account(
    const Lock& lock,
    const Nym& theNym,
    const Identifier& ACCT_ID,
    const Identifier& NOTARY_ID,
    const char* szFuncName)
{
    OT_ASSERT(verify_lock(lock))

    const char* szFunc =
        (nullptr != szFuncName) ? szFuncName : "OTWallet::LoadAccount";
    const String strAcctID(ACCT_ID);
    std::shared_ptr<Account> pAccount(
        Account::LoadExistingAccount(ACCT_ID, NOTARY_ID));

    if (pAccount) {
        bool bVerified = verify_account(
            lock, theNym, *pAccount, NOTARY_ID, strAcctID, szFunc);

        if (!bVerified) { return nullptr; }

        // If I had to load it myself, that means I need to add it to the
        // wallet. (Whereas if GetAccount() had worked, then it would ALREADY
        // be in the wallet, and thus I shouldn't add it twice...)
        add_account(lock, pAccount);
    } else {
        otErr << "OTWallet::LoadAccount " << szFunc
              << ": Failed loading Asset Account: " << strAcctID << "\n";

        return nullptr;
    }

    return pAccount;
}

// No need to cleanup the account returned, it's owned by the wallet.
//
// We don't care if this asset account is already loaded in the wallet.
// Presumably, the user has just download the latest copy of the account
// from the server, and the one in the wallet is old, so now this function
// is being called to load the new one from storage and update the wallet.
//
std::shared_ptr<Account> OTWallet::LoadAccount(
    const Nym& theNym,
    const Identifier& ACCT_ID,
    const Identifier& NOTARY_ID,
    const char* szFuncName)
{
    Lock lock(lock_);

    return load_account(lock, theNym, ACCT_ID, NOTARY_ID, szFuncName);
}

// higher level version of this will require a server message, in addition to
// removing from wallet.
bool OTWallet::RemoveAccount(const Identifier& theTargetID)
{
    Lock lock(lock_);

    return (1 == m_mapAccounts.erase(theTargetID));
}

bool OTWallet::save_contract(const Lock& lock, String& strContract)
{
    OT_ASSERT(verify_lock(lock))

    Tag tag("wallet");

    // Name is in the clear in memory,
    // and base64 in storage.
    OTASCIIArmor ascName;

    if (m_strName.Exists()) {
        ascName.SetString(m_strName, false);  // linebreaks == false
    }

    auto& cachedKey = crypto_.DefaultKey();
    tag.add_attribute("name", m_strName.Exists() ? ascName.Get() : "");
    tag.add_attribute(
        "version", cachedKey.IsGenerated() ? "2.0" : m_strVersion.Get());

    if (cachedKey.IsGenerated())  // If it exists, then serialize it.
    {
        OTASCIIArmor ascMasterContents;

        if (cachedKey.SerializeTo(ascMasterContents)) {
            tag.add_tag("cachedKey", ascMasterContents.Get());
        } else
            otErr << "OTWallet::SaveContract: Failed trying to write master "
                     "key to wallet.\n";
    }

    // Save the extra symmetric keys. (The ones the client app might use to
    // encrypt his local sql-lite DB's record of his Bitmessage connect string,
    // or any other local data.)
    //
    for (auto& it : m_mapExtraKeys) {
        const std::string str_id = it.first;
        std::shared_ptr<OTSymmetricKey> pKey = it.second;

        String strKeyID(str_id.c_str());
        OTASCIIArmor ascKeyID;

        ascKeyID.SetString(
            strKeyID,
            false);  // linebreaks=false (true by default.)

        OTASCIIArmor ascKeyContents;

        if (pKey && pKey->SerializeTo(ascKeyContents)) {
            TagPtr pTag(new Tag("symmetricKey", ascKeyContents.Get()));
            pTag->add_attribute("id", ascKeyID.Get());
            tag.add_tag(pTag);
        } else
            otErr << "OTWallet::SaveContract: Failed trying to serialize "
                     "symmetric keys to wallet.\n";
    }

    for (auto& it : m_mapAccounts) {
        auto& pAccount = std::get<3>(it.second);

        OT_ASSERT_MSG(
            pAccount,
            "nullptr account pointer in "
            "OTWallet::m_mapAccounts, "
            "OTWallet::SaveContract");

        pAccount->SaveContractWallet(tag);
    }

    std::string str_result;
    tag.output(str_result);

    strContract.Concatenate("%s", str_result.c_str());

    return true;
}

// Let's say you have client-app data that you want to keep in encrypted form.
// Well, use this function to create/retrieve a symmetric key based on an ID.
// For example, "mc_sql_lite" might be the name of the symmetric key that I use
// to encrypt sensitive contents in the sql*lite DB.
// This function will find or create the key and return it to you. The key is
// encrypted to the master key in the wallet, so you never actually have to type
// a password to use it, except when the master key itself has expired.
//
std::shared_ptr<OTSymmetricKey> OTWallet::getOrCreateExtraKey(
    const std::string& str_KeyID,
    const std::string* pReason)
{
    //  const std::string str_KeyID("mc_sql_lite");

    // Get the appropriate symmetric key from the wallet.
    // (Which we will decrypt using pMaster.)
    // Once it's decrypted, we'll use this key for encrypting/decrypting
    // the sql*lite DB data on the client side.
    //
    std::shared_ptr<OTSymmetricKey> pExtraKey = getExtraKey(str_KeyID);

    // (If it doesn't exist, let's just create it here.)
    //
    if (!pExtraKey) {
        // The extra keys, like the Nyms, are all encrypted to the master key
        // for the wallet.
        // Thus, to create a new extra symmetrical key, we need to get the
        // master key from OTCachedKey...
        //
        auto& cachedKey = crypto_.DefaultKey();
        OTPassword master_password;
        const bool bGotMasterPW = cachedKey.GetMasterPassword(
            cachedKey,
            master_password,
            (nullptr == pReason) ? "" : pReason->c_str());
        String strNewKeyOutput;

        if (bGotMasterPW && OTSymmetricKey::CreateNewKey(
                                strNewKeyOutput, nullptr, &master_password)) {
            std::shared_ptr<OTSymmetricKey> pNewExtraKey(new OTSymmetricKey);
            Lock lock(lock_);

            if (pNewExtraKey && pNewExtraKey->SerializeFrom(strNewKeyOutput) &&
                add_extra_key(lock, str_KeyID, pNewExtraKey)) {
                pExtraKey = pNewExtraKey;
                save_wallet(lock);
            }
        }  // if (bGotMasterPW)
    }

    return pExtraKey;
}

// The "extra" symmetric keys in the wallet are all, like the Nyms, encrypted
// to the wallet's master key. So whenever the wallet's master key is changed,
// this method needs to be called as well, to update those extra symmetric keys
// to the new master key. (Otherwise they'll stop working.)
//
bool OTWallet::ChangePassphrasesOnExtraKeys(
    const OTPassword& oldPassphrase,
    const OTPassword& newPassphrase)
{
    Lock lock(lock_);
    // First we copy all the keys over to a new map, since we aren't going
    // to copy the changed ones back to the actual map unless EVERYTHING
    // succeeds.
    //
    mapOfSymmetricKeys mapChanged;

    for (auto& it : m_mapExtraKeys) {
        const std::string str_id = it.first;
        std::shared_ptr<OTSymmetricKey> pOldKey = it.second;
        auto thePayload = Data::Factory();

        if (pOldKey && pOldKey->SerializeTo(thePayload)) {
            std::shared_ptr<OTSymmetricKey> pNewKey(new OTSymmetricKey);

            if (pNewKey && pNewKey->SerializeFrom(thePayload))
                mapChanged.insert(
                    std::pair<std::string, std::shared_ptr<OTSymmetricKey>>(
                        str_id, pNewKey));
            else
                return false;
        } else
            return false;
    }

    // We're still here? Must have been a success so far.
    // Next we'll loop through mapChanged, and change the passphrase
    // on each key in there. If they all succeed, we'll clear the old
    // map and copy mapChanged into it.
    //
    for (auto& it : mapChanged) {
        std::shared_ptr<OTSymmetricKey> pNewKey = it.second;

        if (pNewKey) {
            if (!pNewKey->ChangePassphrase(oldPassphrase, newPassphrase))
                return false;
        } else
            return false;
    }

    // Still here? Must have been successful changing the passphrases
    // on all the various extra symmetric keys. So let's clear the main
    // map and copy the changed map into it.
    //
    m_mapExtraKeys.clear();
    m_mapExtraKeys = mapChanged;

    return true;
}

bool OTWallet::Encrypt_ByKeyID(
    const std::string& key_id,
    const String& strPlaintext,
    String& strOutput,
    const String* pstrDisplay,
    bool bBookends)
{
    if (key_id.empty() || !strPlaintext.Exists()) return false;

    std::string str_Reason((nullptr != pstrDisplay) ? pstrDisplay->Get() : "");

    std::shared_ptr<OTSymmetricKey> pKey =
        getOrCreateExtraKey(key_id, &str_Reason);

    if (pKey) {
        auto& cachedKey = crypto_.DefaultKey();
        OTPassword master_password;

        if (cachedKey.GetMasterPassword(cachedKey, master_password)) {

            return OTSymmetricKey::Encrypt(
                *pKey,
                strPlaintext,
                strOutput,
                pstrDisplay,
                bBookends,
                &master_password);
        }
    }

    return false;
}
bool OTWallet::Decrypt_ByKeyID(
    const std::string& key_id,
    const String& strCiphertext,
    String& strOutput,
    const String* pstrDisplay)
{
    if (key_id.empty() || !strCiphertext.Exists()) { return false; }

    std::shared_ptr<OTSymmetricKey> pKey = getExtraKey(key_id);

    if (pKey) {
        auto& cachedKey = crypto_.DefaultKey();
        OTPassword master_password;

        if (cachedKey.GetMasterPassword(cachedKey, master_password)) {

            return OTSymmetricKey::Decrypt(
                *pKey, strCiphertext, strOutput, pstrDisplay, &master_password);
        }
    }

    return false;
}

std::shared_ptr<OTSymmetricKey> OTWallet::getExtraKey(
    const std::string& str_id) const
{
    Lock lock(lock_);

    if (str_id.empty()) return std::shared_ptr<OTSymmetricKey>();

    auto it = m_mapExtraKeys.find(str_id);

    if (it != m_mapExtraKeys.end())  // It's already there (can't add it.)
    {
        std::shared_ptr<OTSymmetricKey> pKey = it->second;

        return pKey;
    }

    return std::shared_ptr<OTSymmetricKey>();
}

bool OTWallet::add_extra_key(
    const Lock& lock,
    const std::string& str_id,
    std::shared_ptr<OTSymmetricKey> pKey)
{
    OT_ASSERT(verify_lock(lock))

    if (str_id.empty() || !pKey) return false;

    auto it = m_mapExtraKeys.find(str_id);

    if (it != m_mapExtraKeys.end())  // It's already there (can't add it.)
        return false;

    m_mapExtraKeys.insert(
        std::pair<std::string, std::shared_ptr<OTSymmetricKey>>(str_id, pKey));

    return true;
}

bool OTWallet::addExtraKey(
    const std::string& str_id,
    std::shared_ptr<OTSymmetricKey> pKey)
{
    Lock lock(lock_);

    return add_extra_key(lock, str_id, pKey);
}

// Pass in the name only, NOT the full path. If you pass nullptr, it remembers
// full path from last time. (Better to do that.)
bool OTWallet::save_wallet(const Lock& lock, const char* szFilename)
{
    OT_ASSERT(verify_lock(lock))

    if (nullptr != szFilename) m_strFilename.Set(szFilename);

    if (!m_strFilename.Exists()) {
        otErr << __FUNCTION__ << ": Filename Dosn't Exist!\n";
        OT_FAIL;
    }

    bool bSuccess = false;
    String strContract;

    if (save_contract(lock, strContract)) {

        // Try to save the wallet to local storage.
        //
        String strFinal;
        OTASCIIArmor ascTemp(strContract);

        if (false ==
            ascTemp.WriteArmoredString(strFinal, "WALLET"))  // todo hardcoding.
        {
            otErr << "OTWallet::SaveWallet: Error saving wallet (failed "
                     "writing armored string):\n"
                  << m_strDataFolder << Log::PathSeparator() << m_strFilename
                  << "\n";
            return false;
        }

        // Wallet file is the only one in data_folder (".") and not a subfolder
        // of that.
        bSuccess = OTDB::StorePlainString(
            strFinal.Get(),
            ".",
            m_strFilename.Get());  // <==== Store Plain String
    }

    return bSuccess;
}

// Pass in the name only, NOT the full path. If you pass nullptr, it remembers
// full path from last time. (Better to do that.)
bool OTWallet::SaveWallet(const char* szFilename)
{
    Lock lock(lock_);

    return save_wallet(lock, szFilename);
}
/*

<wallet name="" version="2.0">

<cachedKey>
CkwAAQCAAAD//wAAAAhVRpwTzc+1NAAAABCKe14aROG8v/ite3un3bBCAAAAINyw
HXTM/x449Al2z8zBHBTRF77jhHkYLj8MIgqrJ2Ep
</cachedKey>

</wallet>

 */
bool OTWallet::LoadWallet(const char* szFilename)
{
    OT_ASSERT_MSG(
        m_strFilename.Exists() || (nullptr != szFilename),
        "OTWallet::LoadWallet: nullptr filename.\n");

    Lock lock(lock_);
    release(lock);

    // The directory is "." because unlike every other OT file, the wallet file
    // doesn't go into a subdirectory, but it goes into the main data_folder
    // itself.
    // Every other file, however, needs to specify its folder AND filename (and
    // both
    // of those will be appended to the local path to form the complete file
    // path.)
    //
    if (!m_strFilename.Exists())        // If it's not already set, then set it.
        m_strFilename.Set(szFilename);  // (We know nullptr wasn't passed in, in
                                        // this case.)

    if (nullptr == szFilename)  // If nullptr was passed in, then set the
                                // pointer to existing string.
        szFilename = m_strFilename.Get();  // (We know existing string is there,
                                           // in this case.)

    if (!OTDB::Exists(".", szFilename)) {
        otErr << __FUNCTION__ << ": Wallet file does not exist: " << szFilename
              << ". Creating...\n";

        const char* szContents = "<wallet name=\"\" version=\"1.0\">\n"
                                 "\n"
                                 "</wallet>\n";

        if (!OTDB::StorePlainString(szContents, ".", szFilename)) {
            otErr << __FUNCTION__
                  << ": Error: Unable to create blank wallet file.\n";
            OT_FAIL;
        }
    }

    String strFileContents(OTDB::QueryPlainString(".", szFilename));  // <===
                                                                      // LOADING
                                                                      // FROM
                                                                      // DATA
                                                                      // STORE.

    if (!strFileContents.Exists()) {
        otErr << __FUNCTION__ << ": Error reading wallet file: " << szFilename
              << "\n";
        return false;
    }

    bool bNeedToSaveAgain = false;

    {
        OTStringXML xmlFileContents(strFileContents);

        if (!xmlFileContents.DecodeIfArmored()) {
            otErr << __FUNCTION__
                  << ": Input string apparently was encoded and then failed "
                     "decoding. Filename: "
                  << szFilename
                  << " \n"
                     "Contents: \n"
                  << strFileContents << "\n";
            return false;
        }

        irr::io::IrrXMLReader* xml =
            irr::io::createIrrXMLReader(xmlFileContents);

        // parse the file until end reached
        while (xml && xml->read()) {
            // strings for storing the data that we want to read out of the file
            String NymName;
            String NymID;
            String AssetName;
            String InstrumentDefinitionID;
            String ServerName;
            String NotaryID;
            String AcctName;
            String AcctID;
            const String strNodeName(xml->getNodeName());

            switch (xml->getNodeType()) {
                case irr::io::EXN_NONE:
                case irr::io::EXN_TEXT:
                case irr::io::EXN_COMMENT:
                case irr::io::EXN_ELEMENT_END:
                case irr::io::EXN_CDATA:
                    // in this xml file, the only text which occurs is the
                    // messageText
                    // messageText = xml->getNodeData();
                    break;
                case irr::io::EXN_ELEMENT: {
                    if (strNodeName.Compare("wallet")) {
                        OTASCIIArmor ascWalletName =
                            xml->getAttributeValue("name");

                        if (ascWalletName.Exists())
                            ascWalletName.GetString(
                                m_strName,
                                false);  // linebreaks == false

                        //                      m_strName            =
                        // xml->getAttributeValue("name");
                        //                      OTLog::OTPath        =
                        // xml->getAttributeValue("path");
                        m_strVersion = xml->getAttributeValue("version");

                        otWarn << "\nLoading wallet: " << m_strName
                               << ", version: " << m_strVersion << "\n";
                    } else if (strNodeName.Compare("cachedKey")) {
                        OTASCIIArmor ascCachedKey;

                        if (Contract::LoadEncodedTextField(xml, ascCachedKey)) {
                            // We successfully loaded the cachedKey from file,
                            // so let's SET it as the cached key globally...
                            auto& cachedKey =
                                crypto_.LoadDefaultKey(ascCachedKey);

                            if (!cachedKey.HasHashCheck()) {
                                OTPassword tempPassword;
                                tempPassword.zeroMemory();
                                bNeedToSaveAgain = cachedKey.GetMasterPassword(
                                    cachedKey,
                                    tempPassword,
                                    "We do not have a check hash yet for this "
                                    "password, please enter your password",
                                    true);
                            }
                        }

                        otWarn << "Loading cachedKey:\n"
                               << ascCachedKey << "\n";
                    } else if (strNodeName.Compare("symmetricKey")) {
                        String strKeyID;
                        OTASCIIArmor ascKeyID = xml->getAttributeValue("id");
                        OTASCIIArmor ascSymmetricKey;

                        if (!ascKeyID.Exists() ||
                            !ascKeyID.GetString(strKeyID, false))  // linebreaks
                                                                   // ==
                            // false (true by
                            // default.)
                            otErr << __FUNCTION__
                                  << ": Failed loading "
                                     "symmetricKey ID (it was "
                                     "blank.)\n";

                        else if (Contract::LoadEncodedTextField(
                                     xml, ascSymmetricKey)) {
                            std::shared_ptr<OTSymmetricKey> pKey(
                                new OTSymmetricKey);

                            if (!pKey || !pKey->SerializeFrom(ascSymmetricKey))
                                otErr
                                    << __FUNCTION__
                                    << ": Failed serializing symmetricKey from "
                                       "string (id: "
                                    << strKeyID << ")\n";
                            else {
                                const std::string str_id(strKeyID.Get());

                                if (!add_extra_key(lock, str_id, pKey))
                                    otErr << __FUNCTION__
                                          << ": Failed adding serialized "
                                             "symmetricKey to wallet (id: "
                                          << strKeyID << ")\n";
                            }
                        }
                    } else if (strNodeName.Compare("account")) {
                        OTASCIIArmor ascAcctName =
                            xml->getAttributeValue("name");

                        if (ascAcctName.Exists())
                            ascAcctName.GetString(
                                AcctName,
                                false);  // linebreaks == false

                        AcctID = xml->getAttributeValue("accountID");
                        NotaryID = xml->getAttributeValue("notaryID");
                        otInfo << "\n------------------------------------------"
                                  "----"
                                  "----------------------------\n"
                                  "****Account**** (wallet listing)\n"
                                  " Account Name: "
                               << AcctName << "\n   Account ID: " << AcctID
                               << "\n    Notary ID: " << NotaryID << "\n";
                        const auto ACCOUNT_ID = Identifier::Factory(AcctID),
                                   NOTARY_ID = Identifier::Factory(NotaryID);
                        std::shared_ptr<Account> pAccount(
                            Account::LoadExistingAccount(
                                ACCOUNT_ID, NOTARY_ID));

                        if (pAccount) {
                            pAccount->SetName(AcctName);
                            add_account(lock, pAccount);
                        } else {
                            otErr
                                << __FUNCTION__
                                << ": Error loading existing Asset Account.\n";
                        }
                    }
                    // This tag is no longer saved in the wallet, but it is
                    // still parsed to allow conversion of existing wallets.
                    // From now on, the BIP39 class tracks the last used index
                    // individually for each seed rather than globally in the
                    // wallet (which assumed only one seed existed).
                    else if (strNodeName.Compare("hd")) {
                        std::uint32_t index = String::StringToUint(
                            xml->getAttributeValue("index"));
                        // An empty string will load the default seed
                        std::string seed = "";
                        crypto_.BIP39().UpdateIndex(seed, index);
                    } else {
                        // unknown element type
                        otErr << __FUNCTION__ << ": unknown element type: "
                              << xml->getNodeName() << "\n";
                    }
                } break;
                default:
                    otLog5 << __FUNCTION__
                           << ": Unknown XML type: " << xml->getNodeName()
                           << "\n";
                    break;
            }
        }  // while xml->read()

        //
        // delete the xml parser after usage
        if (xml) delete xml;
    }

    // In case we converted any of the Nyms to the new "master key" encryption.
    if (bNeedToSaveAgain) save_wallet(lock, szFilename);

    return true;
}

std::set<AccountInfo> OTWallet::AccountList() const
{
    std::set<AccountInfo> output{};

    Lock lock(lock_);

    for (const auto& [accountID, entry] : m_mapAccounts) {
        const auto& [nymID, serverID, unitID, account] = entry;

        OT_ASSERT(account)

        output.emplace(accountID, nymID, serverID, unitID);
    }

    return output;
}

OTWallet::~OTWallet()
{
    Lock lock(lock_);
    release(lock);
}
}  // namespace opentxs
