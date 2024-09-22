#ifndef SERVER_HPP
#define SERVER_HPP

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.hpp>
#include <set>
#include <map>
#include <mutex>
#include <fstream>
#include <regex>

using json = nlohmann::json;
using websocketpp::connection_hdl;

class ChatServer {
public:
    ChatServer();
    void run(uint16_t port);

private:
    typedef websocketpp::server<websocketpp::config::asio> server;
    server m_server;
    std::set<connection_hdl, std::owner_less<connection_hdl>> m_connections;
    struct UserInfo {
        std::string password;
        bool is_admin;
    };
    std::map<std::string, UserInfo> m_users; // 用户名 -> 用户信息
    std::set<std::string> m_banned_users;
    std::map<connection_hdl, std::string, std::owner_less<connection_hdl>> m_connection_to_username;
    std::mutex m_mutex;

    void on_open(connection_hdl hdl);
    void on_close(connection_hdl hdl);
    void on_message(connection_hdl hdl, server::message_ptr msg);
    void on_fail(connection_hdl hdl);
    void log(const std::string &message);
    void handle_command(const json &j, connection_hdl hdl);
    void handle_login(const json &j, connection_hdl hdl);
    void handle_message(const json &j, connection_hdl hdl);
    void handle_private_message(const json &j, connection_hdl hdl);
    void handle_admin_command(const json &j, connection_hdl hdl);
    std::string get_username_by_connection(connection_hdl hdl);
    bool is_valid_username(const std::string &username);
    bool is_valid_password(const std::string &password);
    bool is_valid_message(const std::string &message);
    bool is_logged_in(connection_hdl hdl);
    bool is_admin(connection_hdl hdl);
    void send_response(connection_hdl hdl, const std::string &type, const std::string &status, const std::string &message, const json &broadcast_message = json());
    void broadcast_message(const json &message);
    void load_users();
    void save_users();
    void load_banned_users();
    void save_banned_users();
};

#endif // SERVER_HPP