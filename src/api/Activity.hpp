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
 *  fellowtraveler\opentransactions.org
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

#ifndef OPENTXS_API_ACTIVITY_IMPLEMENTATION_HPP
#define OPENTXS_API_ACTIVITY_IMPLEMENTATION_HPP

#include "Internal.hpp"

#include "opentxs/api/Activity.hpp"
#include "opentxs/core/Lockable.hpp"

#include <map>
#include <mutex>

namespace opentxs::api::implementation
{
class Activity : virtual public opentxs::api::Activity, Lockable
{
public:
    bool AddBlockchainTransaction(
        const Identifier& nymID,
        const Identifier& threadID,
        const StorageBox box,
        const proto::BlockchainTransaction& transaction) const override;

    bool AddPaymentEvent(
        const Identifier& nymID,
        const Identifier& threadID,
        const StorageBox type,
        const Identifier& itemID,
        const Identifier& workflowID,
        std::chrono::time_point<std::chrono::system_clock> time) const override;

    bool MoveIncomingBlockchainTransaction(
        const Identifier& nymID,
        const Identifier& fromThreadID,
        const Identifier& toThreadID,
        const std::string& txid) const override;

    /**   Load a mail object
     *
     *    \param[in] nym the identifier of the nym who owns the mail box
     *    \param[in] id the identifier of the mail object
     *    \param[in] box the box from which to retrieve the mail object
     *    \returns A smart pointer to the object. The smart pointer will not be
     *             instantiated if the object does not exist or is invalid.
     */
    std::unique_ptr<Message> Mail(
        const Identifier& nym,
        const Identifier& id,
        const StorageBox& box) const override;

    /**   Store a mail object
     *
     *    \param[in] nym the identifier of the nym who owns the mail box
     *    \param[in] mail the mail object to be stored
     *    \param[in] box the box from which to retrieve the mail object
     *    \returns The id of the stored message. The string will be empty if
     *             the mail object can not be stored.
     */
    std::string Mail(
        const Identifier& nym,
        const Message& mail,
        const StorageBox box) const override;

    /**   Obtain a list of mail objects in a specified box
     *
     *    \param[in] nym the identifier of the nym who owns the mail box
     *    \param[in] box the box to be listed
     */
    ObjectList Mail(const Identifier& nym, const StorageBox box) const override;

    /**   Delete a mail object
     *
     *    \param[in] nym the identifier of the nym who owns the mail box
     *    \param[in] mail the mail object to be stored
     *    \param[in] box the box from which to retrieve the mail object
     *    \returns The id of the stored message. The string will be empty if
     *             the mail object can not be stored.
     */
    bool MailRemove(
        const Identifier& nym,
        const Identifier& id,
        const StorageBox box) const override;

    /**   Retrieve the text from a message
     *
     *    \param[in] nym the identifier of the nym who owns the mail box
     *    \param[in] id the identifier of the mail object
     *    \param[in] box the box from which to retrieve the mail object
     *    \returns A smart pointer to the object. The smart pointer will not be
     *             instantiated if the object does not exist or is invalid.
     */
    std::shared_ptr<const std::string> MailText(
        const Identifier& nym,
        const Identifier& id,
        const StorageBox& box) const override;

    /**   Mark a thread item as read
     *
     *    \param[in] nymId the identifier of the nym who owns the thread
     *    \param[in] threadId the thread containing the item to be marked
     *    \param[in] itemId the identifier of the item to be marked read
     *    \returns False if the nym, thread, or item does not exist
     */
    bool MarkRead(
        const Identifier& nymId,
        const Identifier& threadId,
        const Identifier& itemId) const override;

    /**   Mark a thread item as unread
     *
     *    \param[in] nymId the identifier of the nym who owns the thread
     *    \param[in] threadId the thread containing the item to be marked
     *    \param[in] itemId the identifier of the item to be marked unread
     *    \returns False if the nym, thread, or item does not exist
     */
    bool MarkUnread(
        const Identifier& nymId,
        const Identifier& threadId,
        const Identifier& itemId) const override;

    ChequeData Cheque(
        const Identifier& nym,
        const std::string& id,
        const std::string& workflow) const override;

    /**   Summarize a payment workflow event in human-friendly test form
     *
     *    \param[in] nym the identifier of the nym who owns the thread
     *    \param[in] id the identifier of the payment item
     *    \param[in] workflow the identifier of the payment workflow
     *    \returns A smart pointer to the object. The smart pointer will not be
     *             instantiated if the object does not exist or is invalid.
     */
    std::shared_ptr<const std::string> PaymentText(
        const Identifier& nym,
        const std::string& id,
        const std::string& workflow) const override;

    /**   Asynchronously cache the most recent items in each of a nym's threads
     *
     *    \param[in] nymID the identifier of the nym who owns the thread
     *    \param[in] count the number of items to preload in each thread
     */
    void PreloadActivity(const Identifier& nymID, const std::size_t count)
        const override;

    /**   Asynchronously cache the items in an activity thread
     *
     *    \param[in] nymID the identifier of the nym who owns the thread
     *    \param[in] threadID the thread containing the items to be cached
     *    \param[in] start the first item to be cached
     *    \param[in] count the number of items to cache
     */
    void PreloadThread(
        const Identifier& nymID,
        const Identifier& threadID,
        const std::size_t start,
        const std::size_t count) const override;

    std::shared_ptr<proto::StorageThread> Thread(
        const Identifier& nymID,
        const Identifier& threadID) const override;

    /**   Obtain a list of thread ids for the specified nym
     *
     *    \param[in] nym the identifier of the nym
     *    \param[in] unreadOnly if true, only return threads with unread items
     */
    ObjectList Threads(const Identifier& nym, const bool unreadOnly = false)
        const override;

    /**   Return the total number of unread thread items for a nym
     *
     *    \param[in] nymId
     */
    std::size_t UnreadCount(const Identifier& nym) const override;

    std::string ThreadPublisher(const Identifier& nym) const override;

    ~Activity() = default;

private:
    friend class implementation::Native;

    typedef std::map<Identifier, std::shared_ptr<const std::string>> MailCache;

    const ContactManager& contact_;
    const storage::Storage& storage_;
    const client::Wallet& wallet_;
    const opentxs::network::zeromq::Context& zmq_;
    mutable std::mutex mail_cache_lock_;
    mutable MailCache mail_cache_;
    mutable std::mutex publisher_lock_;
    mutable std::map<Identifier, OTZMQPublishSocket> thread_publishers_;

    /**   Migrate nym-based thread IDs to contact-based thread IDs
     *
     *    This method should only be called by the ContactManager on startup
     */
    void MigrateLegacyThreads() const;
    void activity_preload_thread(
        const Identifier nymID,
        const std::size_t count) const;
    void preload(
        const Identifier nym,
        const Identifier id,
        const StorageBox box) const;
    void thread_preload_thread(
        const std::string nymID,
        const std::string threadID,
        const std::size_t start,
        const std::size_t count) const;

    std::shared_ptr<const Contact> nym_to_contact(
        const std::string& nymID) const;
    const opentxs::network::zeromq::PublishSocket& get_publisher(
        const Identifier& nymID) const;
    const opentxs::network::zeromq::PublishSocket& get_publisher(
        const Identifier& nymID,
        std::string& endpoint) const;
    void publish(const Identifier& nymID, const std::string& threadID) const;

    Activity(
        const ContactManager& contact,
        const storage::Storage& storage,
        const client::Wallet& wallet,
        const opentxs::network::zeromq::Context& zmq);
    Activity() = delete;
    Activity(const Activity&) = delete;
    Activity(Activity&&) = delete;
    Activity& operator=(const Activity&) = delete;
    Activity& operator=(Activity&&) = delete;
};
}  // namespace opentxs::api::implementation
#endif  // OPENTXS_API_ACTIVITY_IMPLEMENTATION_HPP
