/**
*** Copyright (c) 2016-present,
*** Jaguar0625, gimre, BloodyRookie, Tech Bureau, Corp. All rights reserved.
***
*** This file is part of Catapult.
***
*** Catapult is free software: you can redistribute it and/or modify
*** it under the terms of the GNU Lesser General Public License as published by
*** the Free Software Foundation, either version 3 of the License, or
*** (at your option) any later version.
***
*** Catapult is distributed in the hope that it will be useful,
*** but WITHOUT ANY WARRANTY; without even the implied warranty of
*** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*** GNU Lesser General Public License for more details.
***
*** You should have received a copy of the GNU Lesser General Public License
*** along with Catapult. If not, see <http://www.gnu.org/licenses/>.
**/

#include "VrfKeyLinkTransactionPlugin.h"
#include "src/model/KeyLinkNotifications.h"
#include "src/model/VrfKeyLinkTransaction.h"
#include "catapult/model/NotificationSubscriber.h"
#include "catapult/model/TransactionPluginFactory.h"

using namespace catapult::model;

namespace catapult { namespace plugins {

	namespace {
		template<typename TTransaction>
		void Publish(const TTransaction& transaction, NotificationSubscriber& sub) {
			sub.notify(KeyLinkActionNotification(transaction.LinkAction));
			sub.notify(VrfKeyLinkNotification(transaction.SignerPublicKey, transaction.LinkedPublicKey, transaction.LinkAction));
		}
	}

	DEFINE_TRANSACTION_PLUGIN_FACTORY(VrfKeyLink, Default, Publish)
}}
