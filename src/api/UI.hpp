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

#ifndef OPENTXS_API_UI_IMPLEMENTATION_HPP
#define OPENTXS_API_UI_IMPLEMENTATION_HPP

#include "Internal.hpp"

namespace opentxs::api::implementation
{
class UI : virtual public opentxs::api::UI, Lockable
{
public:
    const ui::AccountActivity& AccountActivity(
        const Identifier& nymID,
        const Identifier& accountID) const override;
    const ui::ActivitySummary& ActivitySummary(
        const Identifier& nymID) const override;
    const ui::ActivityThread& ActivityThread(
        const Identifier& nymID,
        const Identifier& threadID) const override;
    const ui::Contact& Contact(const Identifier& contactID) const override;
    const ui::ContactList& ContactList(const Identifier& nymID) const override;
    const ui::MessagableList& MessagableList(
        const Identifier& nymID) const override;
    const ui::PayableList& PayableList(
        const Identifier& nymID,
        proto::ContactItemType currency) const override;
    const ui::Profile& Profile(const Identifier& contactID) const override;

    ~UI();

private:
    friend Factory;

    /** NymID, AccountID */
    using AccountKey = std::pair<OTIdentifier, OTIdentifier>;
    using AccountActivityMap =
        std::map<AccountKey, std::unique_ptr<ui::AccountActivity>>;
    using ActivitySummaryMap =
        std::map<Identifier, std::unique_ptr<ui::ActivitySummary>>;
    using ActivityThreadID = std::pair<OTIdentifier, OTIdentifier>;
    using ActivityThreadMap =
        std::map<ActivityThreadID, std::unique_ptr<ui::ActivityThread>>;
    using ContactMap = std::map<OTIdentifier, std::unique_ptr<ui::Contact>>;
    using ContactListMap =
        std::map<Identifier, std::unique_ptr<ui::ContactList>>;
    using MessagableListMap =
        std::map<Identifier, std::unique_ptr<ui::MessagableList>>;
    using PayableListMap = std::map<
        std::pair<Identifier, proto::ContactItemType>,
        std::unique_ptr<ui::PayableList>>;
    using ProfileMap = std::map<OTIdentifier, std::unique_ptr<ui::Profile>>;

    const opentxs::network::zeromq::Context& zmq_;
    const api::Activity& activity_;
    const api::ContactManager& contact_;
    const api::client::Sync& sync_;
    const api::client::Wallet& wallet_;
    const api::client::Workflow& workflow_;
    const Flag& running_;
    mutable AccountActivityMap accounts_{};
    mutable ActivitySummaryMap activity_summaries_{};
    mutable ContactMap contacts_{};
    mutable ContactListMap contact_lists_{};
    mutable MessagableListMap messagable_lists_{};
    mutable PayableListMap payable_lists_{};
    mutable ActivityThreadMap activity_threads_{};
    mutable ProfileMap profiles_{};
    OTZMQReplyCallback widget_callback_;
    OTZMQReplySocket widget_update_collector_;
    OTZMQPublishSocket widget_update_publisher_;

    UI(const opentxs::network::zeromq::Context& zmq,
       const api::Activity& activity,
       const api::ContactManager& contact,
       const api::client::Sync& sync,
       const api::client::Wallet& wallet,
       const api::client::Workflow& workflow,
       const Flag& running);
    UI() = delete;
    UI(const UI&) = delete;
    UI(UI&&) = delete;
    UI& operator=(const UI&) = delete;
    UI& operator=(UI&&) = delete;
};
}  // namespace opentxs::api::implementation
#endif  // OPENTXS_API_UI_IMPLEMENTATION_HPP
