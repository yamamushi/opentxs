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

#include "Plugin.hpp"

#include "opentxs/api/storage/Storage.hpp"
#include "opentxs/core/Log.hpp"

#define OT_METHOD "opentxs::Plugin::"

namespace opentxs
{

Plugin::Plugin(
    const api::storage::Storage& storage,
    const StorageConfig& config,
    const Digest& hash,
    const Random& random,
    const Flag& bucket)
    : config_(config)
    , random_(random)
    , storage_(storage)
    , digest_(hash)
    , current_bucket_(bucket)
{
}

bool Plugin::Load(
    const std::string& key,
    const bool checking,
    std::string& value) const
{
    if (key.empty()) {
        if (!checking) {
            otErr << OT_METHOD << __FUNCTION__
                  << ": Error: Tried to load empty key" << std::endl;
        }

        return false;
    }

    bool valid = false;
    const bool bucket{current_bucket_};

    if (LoadFromBucket(key, value, bucket)) { valid = 0 < value.size(); }

    if (!valid) {
        // try again in the other bucket
        if (LoadFromBucket(key, value, !bucket)) {
            valid = 0 < value.size();
        } else {
            // just in case...
            if (LoadFromBucket(key, value, bucket)) {
                valid = 0 < value.size();
            }
        }
    }

    if (!valid && !checking) {
        otWarn << OT_METHOD << __FUNCTION__
               << ": Specified object is not found." << std::endl
               << "Hash: " << key << std::endl
               << "Size: " << value.size() << std::endl;
    }

    return valid;
}

bool Plugin::Migrate(
    const std::string& key,
    const opentxs::api::storage::Driver& to) const
{
    if (key.empty()) { return false; }

    std::string value;
    const bool targetBucket{current_bucket_};
    auto sourceBucket = targetBucket;

    if (&to == this) { sourceBucket = !targetBucket; }

    // try to load the key from the source bucket
    if (LoadFromBucket(key, value, sourceBucket)) {

        // save to the target bucket
        if (to.Store(false, key, value, targetBucket)) {
            return true;
        } else {
            otErr << OT_METHOD << __FUNCTION__ << ": Save failure."
                  << std::endl;

            return false;
        }
    }

    // If the key is not in the source bucket, it should be in the target
    // bucket
    const bool exists = to.LoadFromBucket(key, value, targetBucket);

    if (!exists) {
        otInfo << OT_METHOD << __FUNCTION__ << ": Missing key." << std::endl;

        return false;
    }

    return true;
}

bool Plugin::Store(
    const bool isTransaction,
    const std::string& key,
    const std::string& value,
    const bool bucket) const
{
    std::promise<bool> promise;
    auto future = promise.get_future();
    store(isTransaction, key, value, bucket, &promise);

    return future.get();
}

void Plugin::Store(
    const bool isTransaction,
    const std::string& key,
    const std::string& value,
    const bool bucket,
    std::promise<bool>& promise) const
{
    std::thread thread(
        &Plugin::store, this, isTransaction, key, value, bucket, &promise);
    thread.detach();
}

bool Plugin::Store(
    const bool isTransaction,
    const std::string& value,
    std::string& key) const
{
    const bool bucket{current_bucket_};

    if (digest_) {
        digest_(storage_.HashType(), value, key);

        return Store(isTransaction, key, value, bucket);
    }

    return false;
}
}  // namespace opentxs
