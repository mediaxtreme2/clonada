#include "LicenseClient.h"
#include <juce_cryptography/juce_cryptography.h>

LicenseClient::LicenseClient() {
    loadCachedLicense();
}

LicenseClient::~LicenseClient() = default;

juce::File LicenseClient::getLicenseFile() const {
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    return appData.getChildFile("Clonada").getChildFile("license.json");
}

juce::String LicenseClient::getHardwareId() const {
    auto ids = juce::SystemStats::getUniqueDeviceID();
    return juce::SHA256(ids.toUTF8(), ids.getNumBytesAsUTF8()).toHexString();
}

juce::String LicenseClient::computeHmac(const juce::String& payload) const {
    juce::MemoryBlock keyBlock(kHmacSecret, strlen(kHmacSecret));
    juce::MemoryBlock msgBlock(payload.toRawUTF8(), payload.getNumBytesAsUTF8());

    // SHA256-HMAC
    const int blockSize = 64;
    juce::uint8 keyPad[64];
    std::memset(keyPad, 0, blockSize);
    if (keyBlock.getSize() <= (size_t)blockSize)
        std::memcpy(keyPad, keyBlock.getData(), keyBlock.getSize());
    else {
        auto hashed = juce::SHA256(keyBlock).toHexString();
        std::memcpy(keyPad, hashed.toRawUTF8(), std::min((size_t)32, (size_t)blockSize));
    }

    juce::uint8 ipad[64], opad[64];
    for (int i = 0; i < blockSize; ++i) {
        ipad[i] = keyPad[i] ^ 0x36;
        opad[i] = keyPad[i] ^ 0x5c;
    }

    juce::MemoryBlock innerBlock;
    innerBlock.append(ipad, blockSize);
    innerBlock.append(msgBlock.getData(), msgBlock.getSize());
    auto innerHash = juce::SHA256(innerBlock);

    juce::MemoryBlock outerBlock;
    outerBlock.append(opad, blockSize);
    auto innerHex = innerHash.toHexString();
    juce::MemoryBlock innerBytes;
    for (int i = 0; i < innerHex.length(); i += 2) {
        auto byte = (juce::uint8)innerHex.substring(i, i + 2).getHexValue32();
        innerBytes.append(&byte, 1);
    }
    outerBlock.append(innerBytes.getData(), innerBytes.getSize());

    return juce::SHA256(outerBlock).toHexString();
}

void LicenseClient::loadCachedLicense() {
    auto file = getLicenseFile();
    if (!file.existsAsFile()) return;

    auto json = juce::JSON::parse(file.loadFileAsString());
    if (auto* obj = json.getDynamicObject()) {
        cachedInfo_.licenseKey = obj->getProperty("key").toString();
        cachedInfo_.expiresAt = obj->getProperty("expires").toString();
        auto tier = obj->getProperty("tier").toString();
        cachedInfo_.tier = (tier == "advanced") ? Tier::Advanced : (tier == "basic") ? Tier::Basic : Tier::None;
        cachedInfo_.status = cachedInfo_.licenseKey.isNotEmpty() ? Status::Valid : Status::Unknown;
    }
}

void LicenseClient::saveCachedLicense() {
    auto file = getLicenseFile();
    file.getParentDirectory().createDirectory();

    auto obj = new juce::DynamicObject();
    obj->setProperty("key", cachedInfo_.licenseKey);
    obj->setProperty("expires", cachedInfo_.expiresAt);
    obj->setProperty("tier", cachedInfo_.tier == Tier::Advanced ? "advanced" : cachedInfo_.tier == Tier::Basic ? "basic" : "none");
    obj->setProperty("hardware_id", getHardwareId());

    file.replaceWithText(juce::JSON::toString(juce::var(obj)));
}

LicenseClient::LicenseInfo LicenseClient::parseResponse(const juce::String& json) {
    LicenseInfo info;
    auto parsed = juce::JSON::parse(json);
    if (auto* obj = parsed.getDynamicObject()) {
        auto status = obj->getProperty("status").toString();
        if (status == "active" || status == "valid")
            info.status = Status::Valid;
        else if (status == "expired")
            info.status = Status::Expired;
        else
            info.status = Status::Invalid;

        info.message = obj->getProperty("message").toString();
        info.expiresAt = obj->getProperty("expires_at").toString();

        auto tier = obj->getProperty("tier").toString();
        info.tier = (tier == "advanced") ? Tier::Advanced : (tier == "basic") ? Tier::Basic : Tier::None;
    }
    return info;
}

void LicenseClient::activate(const juce::String& key, std::function<void(LicenseInfo)> callback) {
    auto hwId = getHardwareId();
    auto payload = key + "|" + hwId;
    auto hmac = computeHmac(payload);

    auto url = juce::URL(juce::String(kServerUrl) + "/activate")
        .withPOSTData("{\"license_key\":\"" + key + "\",\"hardware_id\":\"" + hwId + "\",\"hmac\":\"" + hmac + "\"}");

    auto options = juce::URL::InputStreamOptions(url)
        .withHttpRequestCmd("POST")
        .withExtraHeaders("Content-Type: application/json");

    juce::Thread::launch([this, url, options, key, callback]() {
        auto stream = url.createInputStream(options);
        LicenseInfo info;
        if (stream) {
            auto response = stream->readEntireStreamAsString();
            info = parseResponse(response);
            if (info.status == Status::Valid) {
                info.licenseKey = key;
                cachedInfo_ = info;
                saveCachedLicense();
            }
        } else {
            info.status = Status::NetworkError;
            info.message = "Could not reach license server";
        }
        juce::MessageManager::callAsync([callback, info]() { callback(info); });
    });
}

void LicenseClient::validate(std::function<void(LicenseInfo)> callback) {
    if (cachedInfo_.licenseKey.isEmpty()) {
        LicenseInfo info;
        info.status = Status::Invalid;
        info.message = "No license key stored";
        callback(info);
        return;
    }

    auto hwId = getHardwareId();
    auto payload = cachedInfo_.licenseKey + "|" + hwId;
    auto hmac = computeHmac(payload);
    auto key = cachedInfo_.licenseKey;

    auto url = juce::URL(juce::String(kServerUrl) + "/validate")
        .withPOSTData("{\"license_key\":\"" + key + "\",\"hardware_id\":\"" + hwId + "\",\"hmac\":\"" + hmac + "\"}");

    auto options = juce::URL::InputStreamOptions(url)
        .withHttpRequestCmd("POST")
        .withExtraHeaders("Content-Type: application/json");

    juce::Thread::launch([this, url, options, key, callback]() {
        auto stream = url.createInputStream(options);
        LicenseInfo info;
        if (stream) {
            auto response = stream->readEntireStreamAsString();
            info = parseResponse(response);
            info.licenseKey = key;
            cachedInfo_ = info;
            saveCachedLicense();
        } else {
            info = cachedInfo_;
            info.message = "Offline - using cached license";
        }
        juce::MessageManager::callAsync([callback, info]() { callback(info); });
    });
}

void LicenseClient::deactivate(std::function<void(bool)> callback) {
    auto hwId = getHardwareId();
    auto payload = cachedInfo_.licenseKey + "|" + hwId;
    auto hmac = computeHmac(payload);
    auto key = cachedInfo_.licenseKey;

    auto url = juce::URL(juce::String(kServerUrl) + "/deactivate")
        .withPOSTData("{\"license_key\":\"" + key + "\",\"hardware_id\":\"" + hwId + "\",\"hmac\":\"" + hmac + "\"}");

    auto options = juce::URL::InputStreamOptions(url)
        .withHttpRequestCmd("POST")
        .withExtraHeaders("Content-Type: application/json");

    juce::Thread::launch([this, url, options, callback]() {
        auto stream = url.createInputStream(options);
        bool success = false;
        if (stream) {
            auto response = stream->readEntireStreamAsString();
            auto info = parseResponse(response);
            success = true;
        }
        if (success) {
            cachedInfo_ = LicenseInfo{};
            auto file = getLicenseFile();
            file.deleteFile();
        }
        juce::MessageManager::callAsync([callback, success]() { callback(success); });
    });
}
