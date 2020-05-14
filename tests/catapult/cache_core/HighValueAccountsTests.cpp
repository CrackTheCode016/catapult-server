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

#include "catapult/cache_core/HighValueAccounts.h"
#include "tests/test/cache/AccountStateCacheTestUtils.h"
#include "tests/test/core/AddressTestUtils.h"
#include "tests/test/other/DeltaElementsTestUtils.h"
#include "tests/TestHarness.h"

namespace catapult { namespace cache {

#define TEST_CLASS HighValueAccountsTests

	// region test utils

	namespace {
		constexpr auto Harvesting_Mosaic_Id = MosaicId(9876);
		constexpr auto Min_Harvester_Balance = Amount(1'000'000);
		constexpr auto Min_Voter_Balance = Amount(2'000'000);

		cache::AccountStateCacheTypes::Options CreateOptions() {
			auto options = test::CreateDefaultAccountStateCacheOptions(MosaicId(1111), Harvesting_Mosaic_Id);
			options.MinHarvesterBalance = Min_Harvester_Balance;
			options.MinVoterBalance = Min_Voter_Balance;
			return options;
		}

		model::AddressSet GenerateRandomAddresses(size_t count) {
			model::AddressSet addresses;
			for (auto i = 0u; i < count; ++i)
				addresses.insert(test::GenerateRandomAddress());

			return addresses;
		}
	}

	// endregion

	// region view - constructor

	TEST(TEST_CLASS, View_CanCreateEmptyView) {
		// Act:
		HighValueAccounts accounts;

		// Assert:
		EXPECT_TRUE(accounts.addresses().empty());
	}

	TEST(TEST_CLASS, View_CanCreateAroundAddresses) {
		// Act:
		auto addresses = GenerateRandomAddresses(3);
		HighValueAccounts accounts(addresses);

		// Assert:
		EXPECT_EQ(3u, accounts.addresses().size());
		EXPECT_EQ(addresses, accounts.addresses());
	}

	TEST(TEST_CLASS, View_CanCreateAroundMovedAddresses) {
		// Act:
		auto addresses = GenerateRandomAddresses(3);
		auto addressesCopy = addresses;
		HighValueAccounts accounts(std::move(addresses));

		// Assert:
		EXPECT_EQ(3u, accounts.addresses().size());
		EXPECT_EQ(addressesCopy, accounts.addresses());
	}

	// endregion

	// region updater - constructor

	TEST(TEST_CLASS, Updater_CanCreateAroundAddresses) {
		// Act:
		auto addresses = GenerateRandomAddresses(3);
		HighValueAccountsUpdater updater(CreateOptions(), addresses);

		// Assert:
		EXPECT_EQ(3u, updater.currentAddresses().size());
		EXPECT_EQ(addresses, updater.currentAddresses());

		EXPECT_TRUE(updater.removedAddresses().empty());
	}

	// endregion

	// region updater - harvester eligible accounts

	namespace {
		using MemorySetType = AccountStateCacheTypes::PrimaryTypes::BaseSetDeltaType::SetType::MemorySetType;

		std::vector<Amount> GetHarvesterEligibleTestBalances() {
			return {
				Amount(Min_Harvester_Balance),
				Amount(Min_Harvester_Balance - Amount(1)),
				Amount(Min_Harvester_Balance + Amount(1)),
				Amount(Min_Harvester_Balance - Amount(100'000)),
				Amount(Min_Harvester_Balance + Amount(100'000)),
				Amount(Min_Voter_Balance)
			};
		}

		std::vector<Address> AddAccountsWithBalances(MemorySetType& set, const std::vector<Amount>& balances) {
			auto addresses = test::GenerateRandomDataVector<Address>(balances.size());
			for (auto i = 0u; i < balances.size(); ++i) {
				auto accountState = state::AccountState(addresses[i], Height(1));
				accountState.Balances.credit(Harvesting_Mosaic_Id, balances[i]);
				set.emplace(accountState.Address, accountState);
			}

			return addresses;
		}

		model::AddressSet Pick(const std::vector<Address>& addresses, std::initializer_list<size_t> indexes) {
			model::AddressSet selectedAddresses;
			for (auto index : indexes)
				selectedAddresses.insert(addresses[index]);

			return selectedAddresses;
		}
	}

	namespace {
		auto& SelectAdded(test::DeltaElementsTestUtils::Wrapper<MemorySetType>& deltas) {
			return deltas.Added;
		}

		auto& SelectCopied(test::DeltaElementsTestUtils::Wrapper<MemorySetType>& deltas) {
			return deltas.Copied;
		}
	}

	namespace {
		template<typename TSetSelector>
		void AssertHarvesterEligibleAccountsProcessedAsAddAllNew(TSetSelector setSelector) {
			// Arrange:
			test::DeltaElementsTestUtils::Wrapper<MemorySetType> deltas;
			auto addedAddresses = AddAccountsWithBalances(setSelector(deltas), GetHarvesterEligibleTestBalances());

			model::AddressSet addresses;
			HighValueAccountsUpdater updater(CreateOptions(), addresses);

			// Act:
			updater.update(deltas.deltas());

			// Assert:
			EXPECT_EQ(4u, updater.currentAddresses().size());
			EXPECT_EQ(Pick(addedAddresses, { 0, 2, 4, 5 }), updater.currentAddresses());

			EXPECT_TRUE(updater.removedAddresses().empty());
		}

		template<typename TSetSelector>
		void AssertHarvesterEligibleAccountsProcessedAsAddSomeExist(TSetSelector setSelector) {
			// Arrange:
			test::DeltaElementsTestUtils::Wrapper<MemorySetType> deltas;
			auto addedAddresses = AddAccountsWithBalances(setSelector(deltas), GetHarvesterEligibleTestBalances());

			model::AddressSet addresses(addedAddresses.cbegin(), addedAddresses.cbegin() + 4);
			HighValueAccountsUpdater updater(CreateOptions(), addresses);

			// Act:
			updater.update(deltas.deltas());

			// Assert:
			EXPECT_EQ(4u, updater.currentAddresses().size());
			EXPECT_EQ(Pick(addedAddresses, { 0, 2, 4, 5 }), updater.currentAddresses());

			EXPECT_EQ(2u, updater.removedAddresses().size());
			EXPECT_EQ(Pick(addedAddresses, { 1, 3 }), updater.removedAddresses());
		}
	}

	TEST(TEST_CLASS, Updater_HarvesterEligible_CanProcessAddedWhenAllNew) {
		AssertHarvesterEligibleAccountsProcessedAsAddAllNew(SelectAdded);
	}

	TEST(TEST_CLASS, Updater_HarvesterEligible_CanProcessAddedWhenSomeExist) {
		AssertHarvesterEligibleAccountsProcessedAsAddSomeExist(SelectAdded);
	}

	TEST(TEST_CLASS, Updater_HarvesterEligible_CanProcessCopiedWhenAllNew) {
		AssertHarvesterEligibleAccountsProcessedAsAddAllNew(SelectCopied);
	}

	TEST(TEST_CLASS, Updater_HarvesterEligible_CanProcessCopiedWhenSomeExist) {
		AssertHarvesterEligibleAccountsProcessedAsAddSomeExist(SelectCopied);
	}

	TEST(TEST_CLASS, Updater_HarvesterEligible_CanProcessRemovedWhenAllNew) {
		// Arrange:
		test::DeltaElementsTestUtils::Wrapper<MemorySetType> deltas;
		auto addedAddresses = AddAccountsWithBalances(deltas.Removed, GetHarvesterEligibleTestBalances());

		model::AddressSet addresses;
		HighValueAccountsUpdater updater(CreateOptions(), addresses);

		// Act:
		updater.update(deltas.deltas());

		// Assert:
		EXPECT_TRUE(updater.currentAddresses().empty());

		EXPECT_TRUE(updater.removedAddresses().empty());
	}

	TEST(TEST_CLASS, Updater_HarvesterEligible_CanProcessRemovedWhenSomeExist) {
		// Arrange:
		test::DeltaElementsTestUtils::Wrapper<MemorySetType> deltas;
		auto addedAddresses = AddAccountsWithBalances(deltas.Removed, GetHarvesterEligibleTestBalances());

		model::AddressSet addresses(addedAddresses.cbegin(), addedAddresses.cbegin() + 4);
		HighValueAccountsUpdater updater(CreateOptions(), addresses);

		// Act:
		updater.update(deltas.deltas());

		// Assert:
		EXPECT_TRUE(updater.currentAddresses().empty());

		EXPECT_EQ(4u, updater.removedAddresses().size());
		EXPECT_EQ(Pick(addedAddresses, { 0, 1, 2, 3 }), updater.removedAddresses());
	}

	TEST(TEST_CLASS, Updater_HarvesterEligible_CanProcessMixed) {
		// Arrange: add seven [5 match {0, 2, 4, 5, 6}]
		test::DeltaElementsTestUtils::Wrapper<MemorySetType> deltas;
		auto addedAddresses = AddAccountsWithBalances(deltas.Added, {
			Amount(1'100'000), Amount(900'000), Amount(1'000'000), Amount(800'000), Amount(1'200'000), Amount(1'400'000), Amount(1'300'000)
		});

		// - modify three [4 match {0, 1, 4, 6}]
		deltas.Copied.insert(*deltas.Added.find(addedAddresses[1])).first->second.Balances.credit(Harvesting_Mosaic_Id, Amount(100'000));
		deltas.Copied.insert(*deltas.Added.find(addedAddresses[2])).first->second.Balances.debit(Harvesting_Mosaic_Id, Amount(1));
		deltas.Copied.insert(*deltas.Added.find(addedAddresses[5])).first->second.Balances.debit(Harvesting_Mosaic_Id, Amount(300'000));

		// - delete three [3 match {0, 4, 5}]
		deltas.Removed.insert(*deltas.Added.find(addedAddresses[1]));
		deltas.Removed.insert(*deltas.Added.find(addedAddresses[3]));
		deltas.Removed.insert(*deltas.Added.find(addedAddresses[6]));

		model::AddressSet addresses(addedAddresses.cbegin(), addedAddresses.cbegin() + 3);
		HighValueAccountsUpdater updater(CreateOptions(), addresses);

		// Act:
		updater.update(deltas.deltas());

		// Assert:
		EXPECT_EQ(3u, updater.currentAddresses().size());
		EXPECT_EQ(Pick(addedAddresses, { 0, 4, 5 }), updater.currentAddresses());

		EXPECT_EQ(2u, updater.removedAddresses().size());
		EXPECT_EQ(Pick(addedAddresses, { 1, 2 }), updater.removedAddresses());
	}

	// endregion

	// region updater - detachView

	TEST(TEST_CLASS, Updater_DetachViewReturnsExpectedView) {
		// Arrange:
		test::DeltaElementsTestUtils::Wrapper<MemorySetType> deltas;
		auto addedAddresses = AddAccountsWithBalances(deltas.Added, GetHarvesterEligibleTestBalances());

		model::AddressSet addresses(addedAddresses.cbegin(), addedAddresses.cbegin() + 3);
		HighValueAccountsUpdater updater(CreateOptions(), addresses);
		updater.update(deltas.deltas());

		// Act:
		auto accounts = updater.detachView();

		// Assert:
		EXPECT_EQ(4u, accounts.addresses().size());
		EXPECT_EQ(Pick(addedAddresses, { 0, 2, 4, 5 }), accounts.addresses());

		// - updater is reset
		EXPECT_EQ(3u, updater.currentAddresses().size());
		EXPECT_EQ(addresses, updater.currentAddresses());

		EXPECT_TRUE(updater.removedAddresses().empty());
	}

	// endregion
}}
