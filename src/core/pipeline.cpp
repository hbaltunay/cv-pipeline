#include "pipeline.hpp"
#include "logger.hpp"


Pipeline::Pipeline(std::string name) : name_(std::move(name)) {}

Pipeline::~Pipeline() {
    stop();
}

void Pipeline::add(std::unique_ptr<Process> process) {
    if (!process) {
        LOG_WARN("A null process cannot be added.");
        return;
    }
    processes_.push_back(std::move(process));
}

void Pipeline::start() {
    if (running_) return;
    running_ = true;
    for (auto& proc : processes_) {
        proc->start();
    }
    LOG_INFO("The pipeline started.");
}

void Pipeline::stop() {
    if (!running_) return;
    running_ = false;
    for (auto it = processes_.rbegin(); it != processes_.rend(); ++it) {
        (*it)->stop();
    }
    LOG_INFO("Pipeline stopped.");
}