#include "CryptHandler.hpp"

CryptHandler::CryptHandler() {
    generateKey();
}

CryptHandler::CryptHandler(const std::array<uint8_t, 32>& clientPublicKey) : CryptHandler() {
    setOtherPublicKey(clientPublicKey);
}

void CryptHandler::generateKey() {
    crypto_box_keypair(publicKey.data(), secretKey.data());
    makeSharedKey();
}

std::array<uint8_t, 32> CryptHandler::getPublicKey() const {
    return publicKey;
}

std::array<uint8_t, 32> CryptHandler::setOtherPublicKey(const std::array<uint8_t, 32>& clientPublicKey) {
    memcpy(otherPublicKey.data(), clientPublicKey.data(), crypto_box_PUBLICKEYBYTES);
    hasOtherPublicKey.store(true);
    return makeSharedKey();
}

bool CryptHandler::checkOtherPublicKeyStatus() const {
    return hasOtherPublicKey.load();
}

bool CryptHandler::getSharedKey(std::array<uint8_t, 32>& outputSharedKey) {
    std::lock_guard<std::mutex> lock(sharedKeyLock);
    if (hasSharedKey) {
        outputSharedKey = sharedKey;
        return true;
    }
    return false;
}

bool CryptHandler::encrypt(const std::vector<uint8_t>& plainText, std::vector<uint8_t>& cipherText, std::array<uint8_t, 12>& nonce) {
    cipherText.resize(plainText.size() + 16);
    size_t cipherTextSizeOutput;
    randombytes_buf(nonce.data(), 12);

    std::lock_guard<std::mutex> lock(sharedKeyLock);
    int res = crypto_aead_chacha20poly1305_ietf_encrypt(
        cipherText.data(), &cipherTextSizeOutput,
        plainText.data(), plainText.size(),
        reinterpret_cast<const uint8_t*>(CIPHER_AAD), sizeof(CIPHER_AAD),
        nullptr, nonce.data(), sharedKey.data()
    );

    if (res == -1) {
        sodium_memzero(cipherText.data(), cipherText.size());
        sodium_memzero(nonce.data(), nonce.size());
        return false;
    }
    return true;
}

bool CryptHandler::decrypt(const std::vector<uint8_t>& cipherText, const std::array<uint8_t, 12>& nonce, std::vector<uint8_t>& plainText) {
    plainText.resize(cipherText.size() - 16);
    size_t plainTextSizeOutput;

    std::lock_guard<std::mutex> lock(sharedKeyLock);
    int res = crypto_aead_chacha20poly1305_ietf_decrypt(
        plainText.data(), &plainTextSizeOutput,
        nullptr, cipherText.data(), cipherText.size(),
        reinterpret_cast<const uint8_t*>(CIPHER_AAD), sizeof(CIPHER_AAD),
        nonce.data(), sharedKey.data()
    );

    if (res == -1) {
        sodium_memzero(plainText.data(), plainText.size());
        return false;
    }
    return true;
}

std::array<uint8_t, 32> CryptHandler::makeSharedKey() {
    std::lock_guard<std::mutex> lock(sharedKeyLock);
    if (hasOtherPublicKey.load()) {
        crypto_box_beforenm(sharedKey.data(), otherPublicKey.data(), secretKey.data());
    }
    hasSharedKey = true;
    return sharedKey;
}

CryptHandler::~CryptHandler() {
    sodium_memzero(publicKey.data(), publicKey.size());
    sodium_memzero(secretKey.data(), secretKey.size());
    sodium_memzero(sharedKey.data(), sharedKey.size());
}