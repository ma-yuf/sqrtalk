#include "server.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <regex>

ChatServer::ChatServer() {
    m_server.init_asio();
    m_server.set_open_handler(bind(&ChatServer::on_open, this, std::placeholders::_1));
    m_server.set_close_handler(bind(&ChatServer::on_close, this, std::placeholders::_1));
    m_server.set_message_handler(bind(&ChatServer::on_message, this, std::placeholders::_1, std::placeholders::_2));
    m_server.set_fail_handler(bind(&ChatServer::on_fail, this, std::placeholders::_1));
    load_users();
    load_banned_users();
}

void ChatServer::run(uint16_t port) {
    m_server.set_reuse_addr(true); // 允许端口重用
    m_server.listen(port);
    m_server.start_accept();
    try {
        m_server.run();
    } catch (const std::exception &e) {
        log(std::string("Server run exception: ") + e.what());
    } catch (...) {
        log("Unknown server run exception.");
    }
}

void ChatServer::on_open(connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connections.insert(hdl);
    log("New connection opened.");
}

void ChatServer::on_close(connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connections.erase(hdl);
    log("Connection closed.");
}

void ChatServer::on_fail(connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(m_mutex);
    log("Connection failed.");
}

void ChatServer::on_message(connection_hdl hdl, server::message_ptr msg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        json j = json::parse(msg->get_payload());
        if (!j.contains("type")) {
            log("Message does not contain type.");
            send_response(hdl, "error", "error", "Message does not contain type.");
            return;
        }
        std::string type = j["type"];
        if (type == "login") {
            handle_login(j, hdl);
        } else {
            if (!is_logged_in(hdl)) {
                send_response(hdl, type, "error", "User not logged in.");
                return;
            }
            if (type == "message") {
                handle_message(j, hdl);
            } else if (type == "private_message") {
                handle_private_message(j, hdl);
            } else if (type == "command") {
                handle_command(j, hdl);
            } else {
                log("Unknown message type: " + type);
                send_response(hdl, "error", "error", "Unknown message type: " + type);
            }
        }
    } catch (const std::exception &e) {
        log(std::string("Error parsing message: ") + e.what());
        send_response(hdl, "error", "error", "Error parsing message: " + std::string(e.what()));
    } catch (...) {
        log("Unknown error parsing message.");
        send_response(hdl, "error", "error", "Unknown error parsing message.");
    }
}

void ChatServer::log(const std::string &message) {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream time_stream;
    time_stream << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S");

    std::string log_message = time_stream.str() + " " + message;

    std::ofstream log_file("server.log", std::ios_base::app);
    log_file << log_message << std::endl;
    std::cout << log_message << std::endl;
}

bool ChatServer::is_valid_username(const std::string &username) {
    std::regex pattern("^[a-zA-Z0-9_]+$");
    return std::regex_match(username, pattern);
}

bool ChatServer::is_valid_password(const std::string &password) {
    std::regex pattern("^[a-zA-Z0-9_]+$");
    return std::regex_match(password, pattern) && !password.empty();
}

bool ChatServer::is_valid_message(const std::string &message) {
    return message.length() <= 256; // 限制消息长度为256字符
}

bool ChatServer::is_logged_in(connection_hdl hdl) {
    return m_connection_to_username.find(hdl) != m_connection_to_username.end();
}

bool ChatServer::is_admin(connection_hdl hdl) {
    std::string username = get_username_by_connection(hdl);
    return m_users[username].is_admin;
}

void ChatServer::handle_login(const json &j, connection_hdl hdl) {
    if (!j.contains("username") || !j.contains("password")) {
        log("Login message does not contain username or password.");
        send_response(hdl, "login", "error", "Login message does not contain username or password.");
        return;
    }
    std::string username = j["username"];
    std::string password = j["password"];
    if (!is_valid_username(username)) {
        log("Invalid username format.");
        send_response(hdl, "login", "error", "Invalid username format.");
        return;
    }
    if (password.empty()) {
        log("Password cannot be empty.");
        send_response(hdl, "login", "error", "Password cannot be empty.");
        return;
    }
    load_banned_users(); // 每次登录时从文件读取被ban用户信息
    if (m_banned_users.find(username) != m_banned_users.end()) {
        log("Banned user attempted to login: " + username);
        send_response(hdl, "login", "error", "User is banned.");
        return;
    }
    load_users(); // 每次登录时从文件读取用户信息
    if (m_users.find(username) != m_users.end()) {
        if (m_users[username].password == password) {
            m_connection_to_username[hdl] = username;
            log("User logged in: " + username);
            send_response(hdl, "login", "success", "Login successful.");
        } else {
            log("Login failed for user: " + username);
            send_response(hdl, "login", "error", "Login failed.");
        }
    } else {
        // 自动注册
        m_users[username] = {password, false}; // 默认不是管理员
        save_users();
        m_connection_to_username[hdl] = username;
        log("User registered and logged in: " + username);
        send_response(hdl, "login", "success", "Registration and login successful.");
    }
}

void ChatServer::handle_message(const json &j, connection_hdl hdl) {
    std::string username = get_username_by_connection(hdl);
    std::string content = j["message"];
    if (!is_valid_message(content)) {
        log("Invalid message length.");
        send_response(hdl, "message", "error", "Invalid message length.");
        return;
    }
    log("Message from " + username + ": " + content);
    json response;
    response["type"] = "message";
    response["username"] = username;
    response["content"] = content;
    broadcast_message(response);
    send_response(hdl, "message", "success", "Message sent.");
}

void ChatServer::handle_private_message(const json &j, connection_hdl hdl) {
    std::string from = get_username_by_connection(hdl);
    std::string to = j["to"];
    std::string content = j["content"];
    if (!is_valid_message(content)) {
        log("Invalid message length.");
        send_response(hdl, "private_message", "error", "Invalid message length.");
        return;
    }
    log("Private message from " + from + " to " + to + ": " + content);
    json response;
    response["type"] = "private_message";
    response["from"] = from;
    response["to"] = to;
    response["content"] = content;
    for (auto &conn : m_connections) {
        if (get_username_by_connection(conn) == to) {
            m_server.send(conn, response.dump(), websocketpp::frame::opcode::text);
            send_response(hdl, "private_message", "success", "Private message sent.");
            return;
        }
    }
    send_response(hdl, "private_message", "error", "User not found.");
}

void ChatServer::handle_command(const json &j, connection_hdl hdl) {
    if (!j.contains("command")) {
        send_response(hdl, "command", "error", "Command message does not contain command.");
        return;
    }
    std::string username = get_username_by_connection(hdl);
    std::string command = j["command"];
    if (command.rfind("/kick", 0) == 0 || command.rfind("/ban", 0) == 0) {
        if (!is_admin(hdl)) {
            send_response(hdl, "command", "error", "Unauthorized admin command.");
            return;
        }
        handle_admin_command(j, hdl);
    } else if (command == "/list") {
        json response;
        response["type"] = "user_list";
        for (const auto &conn : m_connections) {
            response["users"].push_back(get_username_by_connection(conn));
        }
        send_response(hdl, "command", "success", "User list sent.", response);
    } else {
        send_response(hdl, "command", "error", "Unknown command.");
    }
}

void ChatServer::handle_admin_command(const json &j, connection_hdl hdl) {
    if (!j.contains("password")) {
        send_response(hdl, "command", "error", "Admin command does not contain password.");
        return;
    }
    std::string username = get_username_by_connection(hdl);
    std::string command = j["command"];
    std::string password = j["password"];
    load_users(); // 每次验证管理员时从文件读取信息
    if (m_users.find(username) == m_users.end() || m_users[username].password != password || !m_users[username].is_admin) {
        log("Unauthorized admin command attempt by: " + username);
        send_response(hdl, "command", "error", "Unauthorized admin command.");
        return;
    }
    if (command.rfind("/kick", 0) == 0) {
        std::string target = command.substr(6);
        log("Admin " + username + " kicked user: " + target);
        for (auto &conn : m_connections) {
            if (get_username_by_connection(conn) == target) {
                m_server.close(conn, websocketpp::close::status::normal, "Kicked by admin");
                send_response(hdl, "command", "success", "User kicked.");
                return;
            }
        }
        send_response(hdl, "command", "error", "User not found.");
    } else if (command.rfind("/ban", 0) == 0) {
        std::string target = command.substr(5);
        if (m_users.find(target) == m_users.end()) {
            send_response(hdl, "command", "error", "User not found.");
            return;
        }
        m_banned_users.insert(target);
        save_banned_users();
        log("Admin " + username + " banned user: " + target);
        for (auto &conn : m_connections) {
            if (get_username_by_connection(conn) == target) {
                m_server.close(conn, websocketpp::close::status::normal, "Banned by admin");
                send_response(hdl, "command", "success", "User banned.");
                return;
            }
        }
        send_response(hdl, "command", "success", "User banned.");
    }
}

std::string ChatServer::get_username_by_connection(connection_hdl hdl) {
    return m_connection_to_username[hdl];
}

void ChatServer::send_response(connection_hdl hdl, const std::string &type, const std::string &status, const std::string &message, const json &broadcast_message) {
    json response;
    response["type"] = type;
    response["status"] = status;
    response["message"] = message;
    try {
        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
    } catch (const std::exception &e) {
        log(std::string("Error sending response: ") + e.what());
    } catch (...) {
        log("Unknown error sending response.");
    }

    if (!broadcast_message.empty() && type != "command") {
        this->broadcast_message(broadcast_message);
    }
}

void ChatServer::broadcast_message(const json &message) {
    for (auto &conn : m_connections) {
        try {
            m_server.send(conn, message.dump(), websocketpp::frame::opcode::text);
        } catch (const std::exception &e) {
            log(std::string("Error broadcasting message: ") + e.what());
        } catch (...) {
            log("Unknown error broadcasting message.");
        }
    }
}

void ChatServer::load_users() {
    std::ifstream file("users.json");
    if (file.is_open()) {
        try {
            json j;
            file >> j;
            for (auto& [username, info] : j["users"].items()) {
                m_users[username] = {info["password"], info["is_admin"]};
            }
        } catch (const std::exception &e) {
            log(std::string("Error loading users: ") + e.what());
        } catch (...) {
            log("Unknown error loading users.");
        }
    } else {
        log("Could not open users.json for reading.");
    }
}

void ChatServer::save_users() {
    json j;
    for (const auto& [username, info] : m_users) {
        j["users"][username] = {{"password", info.password}, {"is_admin", info.is_admin}};
    }
    std::ofstream file("users.json");
    if (file.is_open()) {
        try {
            std::string content = j.dump(4);
            if (content.back() != '\n') {
                content += '\n'; // 确保内容以换行符结尾
            }
            file << content;
        } catch (const std::exception &e) {
            log(std::string("Error saving users: ") + e.what());
        } catch (...) {
            log("Unknown error saving users.");
        }
    } else {
        log("Could not open users.json for writing.");
    }
}

void ChatServer::load_banned_users() {
    std::ifstream file("banned_users.json");
    if (file.is_open()) {
        try {
            json j;
            file >> j;
            if (j.contains("banned_users")) {
                m_banned_users = j["banned_users"].get<std::set<std::string>>();
            }
        } catch (const std::exception &e) {
            log(std::string("Error loading banned users: ") + e.what());
        } catch (...) {
            log("Unknown error loading banned users.");
        }
    } else {
        log("Could not open banned_users.json for reading.");
    }
}

void ChatServer::save_banned_users() {
    json j;
    j["banned_users"] = m_banned_users;
    std::ofstream file("banned_users.json");
    if (file.is_open()) {
        try {
            std::string content = j.dump(4);
            if (content.back() != '\n') {
                content += '\n'; // 确保内容以换行符结尾
            }
            file << content;
        } catch (const std::exception &e) {
            log(std::string("Error saving banned users: ") + e.what());
        } catch (...) {
            log("Unknown error saving banned users.");
        }
    } else {
        log("Could not open banned_users.json for writing.");
    }
}