#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>

class LicenseClient {
public:
    enum class Status { Unknown, Valid, Invalid, Expired, NetworkError };
    enum class Tier { None, Basic, Advanced };

    struct LicenseInfo {
        Status status = Status::Unknown;
        Tier tier = Tier::None;
        juce::String licenseKey;
        juce::String message;
        juce::String expiresAt;
    };

    LicenseClient();
    ~LicenseClient();

    void activate(const juce::String& key, std::function<void(LicenseInfo)> callback);
    void validate(std::function<void(LicenseInfo)> callback);
    void deactivate(std::function<void(bool)> callback);

    LicenseInfo getCurrentInfo() const { return cachedInfo_; }
    bool isActivated() const { return cachedInfo_.status == Status::Valid; }
    Tier getTier() const { return cachedInfo_.tier; }

    void loadCachedLicense();
    void saveCachedLicense();

private:
    static constexpr const char* kServerUrl = "http://155.133.27.205/api";
    static constexpr const char* kHmacSecret = "clonada_hmac_s3cr3t_2026";

    LicenseInfo cachedInfo_;
    juce::File getLicenseFile() const;
    juce::String getHardwareId() const;
    juce::String computeHmac(const juce::String& payload) const;
    LicenseInfo parseResponse(const juce::String& json);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenseClient)
};
