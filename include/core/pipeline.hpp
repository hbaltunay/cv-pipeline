#pragma once

#include <memory>
#include <string>
#include <vector>

#include "base.hpp"


class Pipeline {
private:
    std::string name_;
    std::vector<std::unique_ptr<Process>> processes_;
    bool running_ = false;

public:
    explicit Pipeline(std::string name);
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    void add(std::unique_ptr<Process> process);
    void start();
    void stop();
};