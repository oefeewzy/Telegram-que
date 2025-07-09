#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>
#include <iostream>
#include <string>
#include <ctime>
#include <functional>
#include <random>
#include <cstring>

using namespace td_api;
using td::td_api::make_object;

struct Config {
    std::string message;
    std::string chat;
    std::time_t first_time;
    int count = 1;
    int period_minutes = 0;
    bool use_period = false;
};

std::time_t random_time_within_period(std::time_t now, int period_minutes) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, period_minutes * 60);
    return now + dis(gen);
}

template <typename T>
void send_query(td::Client &client, td_api::object_ptr<td_api::Function> f,
                std::function<void(object_ptr<T>)> handler) {
    auto id = client.send(std::move(f));
    while (true) {
        auto response = client.receive(10);
        if (!response) continue;
        if (response->get_id() == id) {
            auto result = td::move_tl_object_as<T>(response->move_object());
            handler(std::move(result));
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    Config cfg = {"Hello from TDLib!", "telegram", std::time(nullptr) + 60};

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--message") == 0 && i+1 < argc) cfg.message = argv[++i];
        else if (strcmp(argv[i], "--chat") == 0 && i+1 < argc) cfg.chat = argv[++i];
        else if (strcmp(argv[i], "--time") == 0 && i+1 < argc) {
            struct tm tm{};
            strptime(argv[++i], "%Y-%m-%d %H:%M:%S", &tm);
            cfg.first_time = mktime(&tm);
        }
        else if (strcmp(argv[i], "--period") == 0 && i+1 < argc) {
            cfg.period_minutes = atoi(argv[++i]);
            cfg.use_period = true;
        }
        else if (strcmp(argv[i], "--count") == 0 && i+1 < argc) {
            cfg.count = atoi(argv[++i]);
        }
    }

    td::Client client;
    client.send(make_object<set_tdlib_parameters>(
        make_object<tdlibParameters>(
            "/tmp/tdlib", "/tmp/tdlib", true, true, false, 123456, "YOUR_API_HASH",
            "en", "Linux", "1.0", false, false, false, false, false, ""
        )));
    client.send(make_object<check_database_encryption_key>(""));

    while (true) {
        auto response = client.receive(10);
        if (!response) continue;
        auto *auth = dynamic_cast<updateAuthorizationState*>(response->get());
        if (!auth) continue;

        auto state = auth->authorization_state_;
        switch (state->get_id()) {
            case authorizationStateWaitPhoneNumber::ID: {
                std::string phone;
                std::cout << "Enter phone: ";
                std::cin >> phone;
                client.send(make_object<setAuthenticationPhoneNumber>(phone, nullptr));
                break;
            }
            case authorizationStateWaitCode::ID: {
                std::string code;
                std::cout << "Enter code: ";
                std::cin >> code;
                client.send(make_object<checkAuthenticationCode>(code));
                break;
            }
            case authorizationStateWaitPassword::ID: {
                std::string pwd;
                std::cout << "Enter 2FA password: ";
                std::cin >> pwd;
                client.send(make_object<checkAuthenticationPassword>(pwd));
                break;
            }
            case authorizationStateReady::ID:
                goto auth_done;
        }
    }
auth_done:

    int64_t chat_id = 0;
    send_query<chat>(client, make_object<searchPublicChat>(cfg.chat),
        [&](object_ptr<chat> res) {
            chat_id = res->id_;
        });
    if (!chat_id) {
        std::cerr << "Failed to find chat" << std::endl;
        return 1;
    }

    for (int i = 0; i < cfg.count; ++i) {
        std::time_t target_time = cfg.first_time + i * 86400;
        if (cfg.use_period) {
            target_time = random_time_within_period(target_time, cfg.period_minutes);
        }

        std::cout << "Scheduling for: " << std::ctime(&target_time);

        client.send(make_object<sendMessage>(
            chat_id, 0, 0, nullptr,
            make_object<messageSchedulingStateSendAtDate>(static_cast<int32_t>(target_time)),
            make_object<inputMessageText>(make_object<formattedText>(cfg.message, nullptr), false, true)
        ));
    }

    std::cout << "All messages scheduled. Exiting.\n";
    return 0;
}