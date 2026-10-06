#pragma once

#include <memory>
#include <CivetServer.h>
#include <nlohmann/json.hpp>

#include "base.hpp"
#include "meta.hpp"
#include "queue.hpp"


class ApiHandler: public CivetHandler {
private:
    Queue<SourceMeta>& src_queue_;

    void sendResponse(struct mg_connection* conn, const std::string& status, const std::string& msg);
    void sendJson(struct mg_connection* conn, const std::string& status, const nlohmann::json& body);

public:
    explicit ApiHandler(Queue<SourceMeta>& src_queue);

    bool handlePost(CivetServer *server, struct mg_connection *conn) override;
    bool handleDelete(CivetServer *server, struct mg_connection *conn) override;
};


class Server : public Process {
private:
    Queue<SourceMeta>& src_queue_;
    std::string port_;
    std::unique_ptr<CivetServer> server_ctx_ = nullptr;
    std::unique_ptr<ApiHandler> handler_ = nullptr;

public:
    Server(Queue<SourceMeta>& src_queue, std::string port);
    ~Server() override;

    bool start() override;
    void stop() override;
};