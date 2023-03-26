#pragma once

#include <iostream>
#include <string>

class  Timestamp
{
    public:
    Timestamp();
    explicit Timestamp(int64_t microSecondsSinceEpoch);
    static Timestamp now();
    std::string toString() const;
    private:
        int64_t microSecondsSinceEpoch_; //必须include <iostream>否则会报错
};