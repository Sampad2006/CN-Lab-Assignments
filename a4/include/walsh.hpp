#ifndef A4_WALSH_HPP
#define A4_WALSH_HPP

#include <vector>
#include <string>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <ostream>

namespace cdma {

// Bipolar bit representations
enum class BitState : int8_t {
    BIT_ZERO = -1,  // Binary 0 maps to bipolar -1
    SILENT   = 0,   // Inactive station (no transmission)
    BIT_ONE  = 1    // Binary 1 maps to bipolar +1
};

// Represents a single Walsh code sequence of length M
struct WalshCode {
    uint32_t station_id;
    size_t length;
    std::vector<int8_t> chips; // Each chip is +1 or -1

    WalshCode() : station_id(0), length(0) {}
    WalshCode(uint32_t id, const std::vector<int8_t>& c)
        : station_id(id), length(c.size()), chips(c) {}

    std::string to_string() const;
};

// Comprehensive Walsh / Hadamard matrix operations and CDMA mathematical kernel
class WalshMatrix {
public:
    // Computes smallest power of 2 >= n, with minimum size 2
    static size_t next_power_of_2(size_t n);

    // Generates an M x M Hadamard matrix using Sylvester's recursive construction
    // H_1 = [1], H_{2k} = [ [H_k, H_k], [H_k, -H_k] ]
    static std::vector<std::vector<int8_t>> generate(size_t m);

    // Checks strict orthogonality: W_i . W_j = M * delta_{ij}
    static bool verify_orthogonality(const std::vector<std::vector<int8_t>>& matrix);

    // Inner product of two chip vectors
    static int64_t dot_product(const std::vector<int8_t>& a, const std::vector<int8_t>& b);
    static int64_t dot_product(const std::vector<int32_t>& a, const std::vector<int8_t>& b);

    // Modulate / spread a data bit using a Walsh code
    // bit == 1  -> +1 * W
    // bit == 0  -> -1 * W
    // bit == -1 (silent) -> 0 * W (vector of zeros)
    static std::vector<int8_t> spread_bit(BitState state, const std::vector<int8_t>& walsh_code);
    static std::vector<int8_t> spread_bit(int raw_bit, const std::vector<int8_t>& walsh_code);

    // Linear algebraic superposition of multiple station chip vectors
    // S[k] = sum_{i=1}^N C_{i}[k]
    static std::vector<int32_t> superimpose(
        const std::vector<std::vector<int8_t>>& station_chips,
        size_t m
    );

    // Correlation and decoding
    // Computes raw dot product sum = (S . W)
    // Normalized decision = sum / M
    struct DecodeResult {
        BitState state;
        int raw_bit;          // 1, 0, or -1 (silent)
        int64_t dot_product;  // raw S . W
        double normalized;    // (S . W) / M
        std::string description;
    };

    static DecodeResult decode(
        const std::vector<int32_t>& composite_signal,
        const std::vector<int8_t>& walsh_code,
        double decision_threshold = 0.5
    );

    // Convert string message to bit vector and back
    static std::vector<int> string_to_bits(const std::string& text);
    static std::string bits_to_string(const std::vector<int>& bits);

    // Additive channel noise simulation for robustness evaluation
    static std::vector<int32_t> add_channel_noise(
        const std::vector<int32_t>& signal,
        double noise_stddev,
        uint32_t seed = 0
    );

    // Pretty-printing helpers
    static std::string format_chips(const std::vector<int8_t>& chips);
    static std::string format_signal(const std::vector<int32_t>& signal);
    static std::string format_matrix(const std::vector<std::vector<int8_t>>& matrix);
};

} // namespace cdma

#endif // A4_WALSH_HPP
