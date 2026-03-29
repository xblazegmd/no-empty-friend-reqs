#include <Geode/Geode.hpp>

#include <arc/prelude.hpp>
#include <string>

using namespace geode::prelude;

/// Sends a request to the GD Servers (duh)
arc::Future<Result<std::string>> requestGDServers(
	const std::string& endpoint,
	const std::string& body
) {
	auto req = web::WebRequest()
		.userAgent("")
		.bodyString(body)
		.timeout(std::chrono::seconds(Mod::get()->getSettingValue<int64_t>("timeout")));

	auto res = co_await req.post("https://www.boomlings.com/database/" + endpoint);
	if (!res.ok()) {
		co_return Err("Failed to request endpoint '{}' ({}): {}", endpoint, res.code(), res.errorMessage());
	}

	if (res.string().isErr()) {
		co_return Err("Could not get response from endpoint '{}': {}", endpoint, res.string().unwrapErr());
	}

	auto ret = res.string().unwrap();
	auto num = utils::numFromString<int>(ret);
	if (num.isOk() && num.unwrap() < 0) {
		co_return Err("Failed to request endpoint '{}': {}", endpoint, num.unwrap());
	}

	co_return Ok(ret);
}

/// Formats the spaghetti mess the server's response is
utils::StringMap<std::string> formatServerResponse(const std::string& res) {
	auto pieces = string::split(res, ":");
	utils::StringMap<std::string> ret;

	for (int i = 0; i < pieces.size(); i += 2) {
		ret[pieces[i]] = pieces[i + 1];
	}

	return ret;
}

/// zzz...
arc::Future<> sleep(int s) {
	co_await arc::sleep(asp::Duration::fromSecs(s));
}

/// Easy way of showing an error notification in-game
inline void showErrorNotification(const std::string& msg) {
	if (!Mod::get()->getSettingValue<bool>("show-errors")) return;
	geode::queueInMainThread([msg] {
		Notification::create(msg, NotificationIcon::Error)->show();
	});
}

$on_game(Loaded) {
	async::spawn([] -> arc::Future<> {
		auto accManager = GJAccountManager::get();
		if (accManager->m_accountID < 0 || accManager->m_GJP2.empty()) {
			log::error("Not logged in!");
			co_return;
		}

		while (true) {
			/// Get friend requests
			auto res = co_await requestGDServers("getGJFriendRequests20.php", fmt::format(
				"accountID={}&gjp2={}&secret=Wmfd2893gb7",
				accManager->m_accountID,
				accManager->m_GJP2
			));

			if (res.isOk()) {
				/// idc abt the metadata
				auto meta = string::split(res.unwrap(), "#");
				auto friendReqs = string::split(meta[0], "|");

				for (const auto& req : friendReqs) {
					auto friendReq = formatServerResponse(req);
					if (friendReq["35"] == "") { // "35" = message
						log::info("Found empty friend req by '{}', declining...", friendReq["1"]); // "1" = username
						auto decRes = co_await requestGDServers("deleteGJFriendRequests20.php", fmt::format(
							"accountID={}&gjp2={}&targetAccountID={}&secret=Wmfd2893gb7",
							accManager->m_accountID,
							accManager->m_GJP2,
							friendReq["16"] // "16" = accountID
						));

						if (decRes.isErr()) {
							log::error("{}", decRes.unwrapErr());
							showErrorNotification(fmt::format("Could not decline friend request by '{}': {}", friendReq["1"], decRes.unwrapErr()));
						} else {
							log::info("Successfully declined friend request by '{}'", friendReq["1"]);
						}

						// To be 100% sure this won't get me rate limited
						co_await sleep(Mod::get()->getSettingValue<int64_t>("cooldown"));
					} else {
						log::debug("Skipping friend request by '{}'", friendReq["1"]);
					}
				}
			} else {
				log::error("{}", res.unwrapErr());
				showErrorNotification(fmt::format("Could not get friend requests: {}", res.unwrapErr()));
			}

			co_await sleep(Mod::get()->getSettingValue<int64_t>("interval"));
		}
	});
}