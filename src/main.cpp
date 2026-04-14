#include <Geode/Geode.hpp>

#include <xblazegmd.geode-api/include/XblazeAPI.hpp>
#include <arc/prelude.hpp>
#include <string>

using namespace geode::prelude;

inline void quickErrorNotification(const std::string& msg) {
	if (!Mod::get()->getSettingValue<bool>("show-errors")) return;
	xblazeapi::quickErrorNotificationTS(msg);
}

$on_game(Loaded) {
	if (!GameToolbox::doWeHaveInternet()) return;

	async::spawn([] -> arc::Future<> {
		auto accManager = GJAccountManager::get();
		if (accManager->m_accountID < 0 || accManager->m_GJP2.empty()) {
			log::error("Not logged in!");
			co_return;
		}

		while (true) {
			/// Get friend requests
			auto res = co_await xblazeapi::requestGDServers("getGJFriendRequests20.php", fmt::format(
				"accountID={}&gjp2={}&secret=Wmfd2893gb7",
				accManager->m_accountID,
				accManager->m_GJP2
			));

			if (res.isOk()) {
				/// idc abt the metadata
				auto meta = string::split(res.unwrap(), "#");
				auto friendReqs = string::split(meta[0], "|");

				for (const auto& req : friendReqs) {
					auto friendReq = xblazeapi::formatResponse(req);
					if (friendReq["35"] == "") { // "35" = message
						log::info("Found empty friend req by '{}', declining...", friendReq["1"]); // "1" = username
						auto decRes = co_await xblazeapi::requestGDServers("deleteGJFriendRequests20.php", fmt::format(
							"accountID={}&gjp2={}&targetAccountID={}&secret=Wmfd2893gb7",
							accManager->m_accountID,
							accManager->m_GJP2,
							friendReq["16"] // "16" = accountID
						));

						if (decRes.isErr()) {
							log::error("{}", decRes.unwrapErr());
							quickErrorNotification(fmt::format("Could not decline friend request by '{}': {}", friendReq["1"], decRes.unwrapErr()));
						} else {
							log::info("Successfully declined friend request by '{}'", friendReq["1"]);
						}

						// To be 100% sure this won't get me rate limited
						co_await xblazeapi::sleepSecs(Mod::get()->getSettingValue<int64_t>("cooldown"));
					} else {
						log::debug("Skipping friend request by '{}'", friendReq["1"]);
					}
				}
			} else if (res.unwrapErr() != -2) { // -2 means there is no friend requests (I think)
				log::error("{}", res.unwrapErr());
				quickErrorNotification(fmt::format("Could not get friend requests: {}", res.unwrapErr()));
			}

			co_await xblazeapi::sleepSecs(Mod::get()->getSettingValue<int64_t>("interval"));
		}
	});
}