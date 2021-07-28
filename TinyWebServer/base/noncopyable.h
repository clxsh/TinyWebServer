#pragma once

class noncopyable {
protected:
    noncopyable() {}
    ~noncopyable() {}

public:
    // using c++11 delete to implement noncopyable
    noncopyable(const noncopyable &) = delete;
    noncopyable& operator=(const noncopyable &) = delete;
};