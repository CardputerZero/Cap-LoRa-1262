#pragma once

#include <array>
#include <chrono>

namespace cap_lora {

class LoraDutyCycle {
public:
    using Clock   = std::chrono::steady_clock;
    using Airtime = std::chrono::microseconds;
    using Minute  = std::chrono::minutes;

    bool reserve(Clock::time_point now, Airtime airtime, int duty_percent)
    {
        if (duty_percent == 100) return true;
        if (duty_percent <= 0 || duty_percent > 100 || airtime <= Airtime::zero() || airtime >= Minute{1}) return false;

        constexpr auto hour = std::chrono::duration_cast<Airtime>(std::chrono::hours{1});
        const auto budget   = hour * duty_percent / 100;
        const auto minute   = std::chrono::floor<Minute>(now.time_since_epoch());

        Airtime used{};
        for (const auto& bucket : buckets_) {
            if (bucket.minute >= minute - std::chrono::hours{1}) used += bucket.airtime;
        }
        if (airtime > budget - used) return false;

        // Charge the full frame up front and expire it conservatively, one
        // hour after the end of the minute in which it finishes.
        const auto end_minute = std::chrono::floor<Minute>((now + airtime).time_since_epoch());
        const auto offset     = end_minute.count() % kBucketCount;
        auto& bucket          = buckets_[(offset + kBucketCount) % kBucketCount];
        if (bucket.minute != end_minute) bucket = {end_minute, Airtime::zero()};
        bucket.airtime += airtime;
        return true;
    }

private:
    struct Bucket {
        Minute minute = Minute::min();
        Airtime airtime{};
    };

    // The current minute, the preceding hour, and a frame ending next minute.
    static constexpr int kBucketCount = 62;
    std::array<Bucket, kBucketCount> buckets_{};
};

}  // namespace cap_lora
