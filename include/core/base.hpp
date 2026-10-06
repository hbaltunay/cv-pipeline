#pragma once

class Process {
public:
    virtual ~Process() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;
};

