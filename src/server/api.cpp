#include <iostream>
#include <cstring>

#include "logger.hpp"
#include "api.hpp"


using json = nlohmann::json;

ApiHandler::ApiHandler(Queue<SourceMeta>& src_queue) : src_queue_(src_queue) {}

void ApiHandler::sendResponse(struct mg_connection* conn, const std::string& status, const std::string& msg) {
    mg_printf(conn, "HTTP/1.1 %s\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s", 
              status.c_str(), (int)msg.length(), msg.c_str());
}

void ApiHandler::sendJson(struct mg_connection* conn, const std::string& status, const nlohmann::json& body) {
    std::string json_str = body.dump();
    mg_printf(conn, "HTTP/1.1 %s\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s", 
              status.c_str(), (int)json_str.length(), json_str.c_str());
}


bool ApiHandler::handlePost(CivetServer* server, struct mg_connection* conn) {
    const struct mg_request_info* req_info = mg_get_request_info(conn);
    
    if (std::strcmp(req_info->local_uri, "/api/stream/add") != 0) {
        sendResponse(conn, "404 Not Found", "Endpoint not found.");
        return true;
    }

    char buffer[1024];
    int d_len = mg_read(conn, buffer, sizeof(buffer) - 1);
    if (d_len <= 0) {
        sendResponse(conn, "400 Bad Request", "Error: Body is empty.");
        return true;
    }
    buffer[d_len] = '\0';

    json parsed;
    try {
        parsed = json::parse(buffer, buffer + d_len);
    } catch (const json::parse_error& e) {
        sendResponse(conn, "400 Bad Request", "Error: Invalid JSON.");
        return true;
    }

    std::string name = parsed.value("name", "");
    std::string uri = parsed.value("uri", "");

    if (name.empty() || uri.empty()) {
        sendResponse(conn, "400 Bad Request", "Error: 'name' or 'uri' is empty.");
        return true;
    }

    SourceMeta meta;
    meta.task = Task::ADD;
    meta.name = name;
    meta.uri = uri;
    src_queue_.push(std::move(meta));

    json response = {{"status", "added"}};
    sendJson(conn, "202 Accepted", response);
    return true;

}

bool ApiHandler::handleDelete(CivetServer *server, struct mg_connection *conn) {
    const struct mg_request_info* req_info = mg_get_request_info(conn);

    if (std::strcmp(req_info->local_uri, "/api/stream/remove") != 0) {
        sendResponse(conn, "404 Not Found", "Endpoint not found.");
        return true;
    }

    if (!req_info->query_string) {
        sendResponse(conn, "400 Bad Request", "Error: Query string is missing.");
        return true;
    }

    char cid_buf[32] = {0};
    mg_get_var(req_info->query_string, std::strlen(req_info->query_string), "camera_id", cid_buf, sizeof(cid_buf));

    if (std::strlen(cid_buf) == 0) {
        sendResponse(conn, "400 Bad Request", "Error: camera_id is missing.");
        return true;
    }

    SourceMeta meta;
    meta.task = Task::REMOVE;
    meta.camera_id = std::stoi(cid_buf);
    src_queue_.push(std::move(meta));

    json response = {{"status", "removed"}};
    sendJson(conn, "202 Accepted", response);
    return true;
}


Server::Server(Queue<SourceMeta> &src_queue, std::string port)
    : src_queue_(src_queue), port_(port) {}

Server::~Server() {
    stop();
}

bool Server::start() {
    if (server_ctx_ != nullptr) return false;
    
    std::vector<std::string> options = {
        "listening_ports", port_,
        "num_threads", "4"
    };

    try {
        server_ctx_ = std::make_unique<CivetServer>(options);
        handler_ = std::make_unique<ApiHandler>(src_queue_);
        server_ctx_->addHandler("/api/stream/add", *handler_);
        server_ctx_->addHandler("/api/stream/remove", *handler_);
        LOG_INFO("API server started on port {}.", port_);
    } catch (const std::exception& e) {
        LOG_ERROR("API server could not be started! Error: {}", e.what());
        return false;
    }

    return true;
}

void Server::stop() {
    if (server_ctx_ != nullptr) {
        server_ctx_.reset();
        handler_.reset();
        LOG_INFO("API server stopped.");
    }
}