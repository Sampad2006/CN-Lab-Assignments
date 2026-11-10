#include "walsh.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <random>
#include <algorithm>

namespace cdma {

std::string WalshCode::to_string() const {
    return WalshMatrix::format_chips(chips);
}

size_t WalshMatrix::next_power_of_2(size_t n) {
    if (n <= 1) return 2;
    size_t p = 1;
    while (p < n) {
        p <<= 1;
    }
    return p;
}

std::vector<std::vector<int8_t>> WalshMatrix::generate(size_t m) {
    if (m == 0 || (m & (m - 1)) != 0) {
        throw std::invalid_argument("Walsh matrix size M must be a power of 2, received: " + std::to_string(m));
    }

    std::vector<std::vector<int8_t>> matrix(m, std::vector<int8_t>(m, 0));
    matrix[0][0] = 1;

    for (size_t k = 1; k < m; k <<= 1) {
        for (size_t i = 0; i < k; ++i) {
            for (size_t j = 0; j < k; ++j) {
                int8_t val = matrix[i][j];
                matrix[i][j]       =  val; // Top-left: H_k
                matrix[i][j + k]   =  val; // Top-right: H_k
                matrix[i + k][j]   =  val; // Bottom-left: H_k
                matrix[i + k][j + k] = static_cast<int8_t>(-val); // Bottom-right: -H_k
            }
        }
    }

    return matrix;
}

bool WalshMatrix::verify_orthogonality(const std::vector<std::vector<int8_t>>& matrix) {
    size_t m = matrix.size();
    if (m == 0) return false;

    for (size_t i = 0; i < m; ++i) {
        if (matrix[i].size() != m) return false;
        for (size_t j = 0; j < m; ++j) {
            int64_t dot = dot_product(matrix[i], matrix[j]);
            if (i == j) {
                if (dot != static_cast<int64_t>(m)) return false;
            } else {
                if (dot != 0) return false;
            }
        }
    }
    return true;
}

int64_t WalshMatrix::dot_product(const std::vector<int8_t>& a, const std::vector<int8_t>& b) {
    if (a.size() != b.size()) {
        throw std::invalid_argument("Vector length mismatch in dot_product");
    }
    int64_t sum = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        sum += static_cast<int64_t>(a[i]) * static_cast<int64_t>(b[i]);
    }
    return sum;
}

int64_t WalshMatrix::dot_product(const std::vector<int32_t>& a, const std::vector<int8_t>& b) {
    if (a.size() != b.size()) {
        throw std::invalid_argument("Vector length mismatch in dot_product");
    }
    int64_t sum = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        sum += static_cast<int64_t>(a[i]) * static_cast<int64_t>(b[i]);
    }
    return sum;
}

std::vector<int8_t> WalshMatrix::spread_bit(BitState state, const std::vector<int8_t>& walsh_code) {
    size_t m = walsh_code.size();
    std::vector<int8_t> chips(m, 0);

    int multiplier = 0;
    if (state == BitState::BIT_ONE) {
        multiplier = 1;
    } else if (state == BitState::BIT_ZERO) {
        multiplier = -1;
    } else {
        multiplier = 0; // Silent
    }

    if (multiplier != 0) {
        for (size_t i = 0; i < m; ++i) {
            chips[i] = static_cast<int8_t>(multiplier * walsh_code[i]);
        }
    }
    return chips;
}

std::vector<int8_t> WalshMatrix::spread_bit(int raw_bit, const std::vector<int8_t>& walsh_code) {
    BitState state;
    if (raw_bit == 1) {
        state = BitState::BIT_ONE;
    } else if (raw_bit == 0) {
        state = BitState::BIT_ZERO;
    } else {
        state = BitState::SILENT;
    }
    return spread_bit(state, walsh_code);
}

std::vector<int32_t> WalshMatrix::superimpose(
    const std::vector<std::vector<int8_t>>& station_chips,
    size_t m
) {
    std::vector<int32_t> composite(m, 0);
    for (const auto& chips : station_chips) {
        size_t len = std::min(m, chips.size());
        for (size_t k = 0; k < len; ++k) {
            composite[k] += chips[k];
        }
    }
    return composite;
}

WalshMatrix::DecodeResult WalshMatrix::decode(
    const std::vector<int32_t>& composite_signal,
    const std::vector<int8_t>& walsh_code,
    double decision_threshold
) {
    size_t m = walsh_code.size();
    if (composite_signal.size() != m || m == 0) {
        throw std::invalid_argument("Size mismatch between composite signal and Walsh code");
    }

    int64_t dot = dot_product(composite_signal, walsh_code);
    double normalized = static_cast<double>(dot) / static_cast<double>(m);

    DecodeResult result;
    result.dot_product = dot;
    result.normalized = normalized;

    if (normalized >= decision_threshold) {
        result.state = BitState::BIT_ONE;
        result.raw_bit = 1;
        result.description = "Bit 1";
    } else if (normalized <= -decision_threshold) {
        result.state = BitState::BIT_ZERO;
        result.raw_bit = 0;
        result.description = "Bit 0";
    } else {
        result.state = BitState::SILENT;
        result.raw_bit = -1;
        result.description = "Silent (Idle)";
    }

    return result;
}

std::vector<int> WalshMatrix::string_to_bits(const std::string& text) {
    std::vector<int> bits;
    bits.reserve(text.size() * 8);
    for (unsigned char c : text) {
        for (int b = 7; b >= 0; --b) {
            bits.push_back((c >> b) & 1);
        }
    }
    return bits;
}

std::string WalshMatrix::bits_to_string(const std::vector<int>& bits) {
    std::string text;
    text.reserve(bits.size() / 8);
    for (size_t i = 0; i + 7 < bits.size(); i += 8) {
        unsigned char c = 0;
        bool all_silent = true;
        for (int b = 0; b < 8; ++b) {
            if (bits[i + b] == 1) {
                c |= (1 << (7 - b));
                all_silent = false;
            } else if (bits[i + b] == 0) {
                all_silent = false;
            }
        }
        if (all_silent || c == 0) {
            break;
        }
        text.push_back(static_cast<char>(c));
    }
    return text;
}

std::vector<int32_t> WalshMatrix::add_channel_noise(
    const std::vector<int32_t>& signal,
    double noise_stddev,
    uint32_t seed
) {
    if (noise_stddev <= 0.0) return signal;

    std::mt19937 gen(seed == 0 ? std::random_device{}() : seed);
    std::normal_distribution<double> dist(0.0, noise_stddev);

    std::vector<int32_t> noisy_signal = signal;
    for (auto& sample : noisy_signal) {
        double noise = dist(gen);
        sample += static_cast<int32_t>(std::round(noise));
    }
    return noisy_signal;
}

std::string WalshMatrix::format_chips(const std::vector<int8_t>& chips) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < chips.size(); ++i) {
        if (i > 0) oss << ", ";
        if (chips[i] > 0) {
            oss << "+1";
        } else if (chips[i] < 0) {
            oss << "-1";
        } else {
            oss << " 0";
        }
    }
    oss << "]";
    return oss.str();
}

std::string WalshMatrix::format_signal(const std::vector<int32_t>& signal) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < signal.size(); ++i) {
        if (i > 0) oss << ", ";
        if (signal[i] >= 0) oss << "+";
        oss << signal[i];
    }
    oss << "]";
    return oss.str();
}

std::string WalshMatrix::format_matrix(const std::vector<std::vector<int8_t>>& matrix) {
    std::ostringstream oss;
    size_t m = matrix.size();
    for (size_t i = 0; i < m; ++i) {
        oss << "  W_" << std::setw(2) << std::left << (i + 1) << ": ";
        oss << format_chips(matrix[i]) << "\n";
    }
    return oss.str();
}

} // namespace cdma
