#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <cmath>
#include "walsh.hpp"
#include "frame.hpp"

using namespace cdma;

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "\033[31m[FAILED]\033[0m " << msg << " (" << #cond << " at " << __FILE__ << ":" << __LINE__ << ")\n"; \
            return false; \
        } \
    } while (0)

#define ASSERT_EQUAL(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            std::cerr << "\033[31m[FAILED]\033[0m " << msg << " (Expected: " << (b) << ", Got: " << (a) << " at " << __FILE__ << ":" << __LINE__ << ")\n"; \
            return false; \
        } \
    } while (0)

bool test_walsh_power_of_2() {
    std::cout << "  [Test] Power of 2 Dimension Calculator... ";
    ASSERT_EQUAL(WalshMatrix::next_power_of_2(0), 2, "n=0 should yield 2");
    ASSERT_EQUAL(WalshMatrix::next_power_of_2(1), 2, "n=1 should yield 2");
    ASSERT_EQUAL(WalshMatrix::next_power_of_2(2), 2, "n=2 should yield 2");
    ASSERT_EQUAL(WalshMatrix::next_power_of_2(3), 4, "n=3 should yield 4");
    ASSERT_EQUAL(WalshMatrix::next_power_of_2(4), 4, "n=4 should yield 4");
    ASSERT_EQUAL(WalshMatrix::next_power_of_2(5), 8, "n=5 should yield 8");
    ASSERT_EQUAL(WalshMatrix::next_power_of_2(8), 8, "n=8 should yield 8");
    ASSERT_EQUAL(WalshMatrix::next_power_of_2(9), 16, "n=9 should yield 16");
    std::cout << "\033[32mPASSED\033[0m\n";
    return true;
}

bool test_hadamard_generation_and_orthogonality() {
    std::cout << "  [Test] Sylvester Hadamard Matrix Orthogonality Invariant... ";
    std::vector<size_t> test_sizes = {2, 4, 8, 16, 32, 64};

    for (size_t m : test_sizes) {
        auto H = WalshMatrix::generate(m);
        ASSERT_EQUAL(H.size(), m, "Matrix row count should equal M");
        for (size_t i = 0; i < m; ++i) {
            ASSERT_EQUAL(H[i].size(), m, "Matrix col count should equal M");
            for (size_t j = 0; j < m; ++j) {
                ASSERT_TRUE(H[i][j] == 1 || H[i][j] == -1, "Entries must be strictly +1 or -1");
            }
        }
        ASSERT_TRUE(WalshMatrix::verify_orthogonality(H), "Walsh matrix must satisfy orthogonality");
    }
    std::cout << "\033[32mPASSED\033[0m\n";
    return true;
}

bool test_chip_modulation_and_spreading() {
    std::cout << "  [Test] Bipolar Modulation & Walsh Spreading... ";
    std::vector<int8_t> code = {+1, -1, +1, -1};

    // Bit 1: +1 * W
    auto chips_1 = WalshMatrix::spread_bit(1, code);
    std::vector<int8_t> expected_1 = {+1, -1, +1, -1};
    ASSERT_TRUE(chips_1 == expected_1, "Bit 1 spreading failed");

    // Bit 0: -1 * W
    auto chips_0 = WalshMatrix::spread_bit(0, code);
    std::vector<int8_t> expected_0 = {-1, +1, -1, +1};
    ASSERT_TRUE(chips_0 == expected_0, "Bit 0 spreading failed");

    // Silent: 0 * W
    auto chips_s = WalshMatrix::spread_bit(-1, code);
    std::vector<int8_t> expected_s = {0, 0, 0, 0};
    ASSERT_TRUE(chips_s == expected_s, "Silent spreading failed");

    std::cout << "\033[32mPASSED\033[0m\n";
    return true;
}

bool test_linear_superposition_and_decoding() {
    std::cout << "  [Test] Channel Superposition & CDMA Despreading... ";
    auto H = WalshMatrix::generate(4);
    // H[0] = [+1, +1, +1, +1]
    // H[1] = [+1, -1, +1, -1]
    // H[2] = [+1, +1, -1, -1]
    // H[3] = [+1, -1, -1, +1]

    // Station 1 sends 1  -> [+1, +1, +1, +1]
    // Station 2 sends 0  -> [-1, +1, -1, +1]
    // Station 3 silent   -> [ 0,  0,  0,  0]
    // Station 4 sends 1  -> [+1, -1, -1, +1]
    // Superposition S    = [+1, +1, -1, +3]

    std::vector<std::vector<int8_t>> chips = {
        WalshMatrix::spread_bit(1, H[0]),
        WalshMatrix::spread_bit(0, H[1]),
        WalshMatrix::spread_bit(-1, H[2]),
        WalshMatrix::spread_bit(1, H[3])
    };

    auto S = WalshMatrix::superimpose(chips, 4);
    std::vector<int32_t> expected_S = {1, 1, -1, 3};
    ASSERT_TRUE(S == expected_S, "Superposition vector mismatch");

    // Reconstruct each station
    auto d1 = WalshMatrix::decode(S, H[0]);
    ASSERT_EQUAL(d1.raw_bit, 1, "Station 1 decoded bit");
    ASSERT_EQUAL(d1.dot_product, 4, "Station 1 inner product");

    auto d2 = WalshMatrix::decode(S, H[1]);
    ASSERT_EQUAL(d2.raw_bit, 0, "Station 2 decoded bit");
    ASSERT_EQUAL(d2.dot_product, -4, "Station 2 inner product");

    auto d3 = WalshMatrix::decode(S, H[2]);
    ASSERT_EQUAL(d3.raw_bit, -1, "Station 3 decoded silent");
    ASSERT_EQUAL(d3.dot_product, 0, "Station 3 inner product");

    auto d4 = WalshMatrix::decode(S, H[3]);
    ASSERT_EQUAL(d4.raw_bit, 1, "Station 4 decoded bit");
    ASSERT_EQUAL(d4.dot_product, 4, "Station 4 inner product");

    std::cout << "\033[32mPASSED\033[0m\n";
    return true;
}

bool test_all_permutations_4stations() {
    std::cout << "  [Test] Exhaustive 4-Station Bit Permutation Matrix (3^4 = 81 cases)... ";
    auto H = WalshMatrix::generate(4);
    int states[3] = {-1, 0, 1}; // Silent, Bit 0, Bit 1

    for (int s1 : states) {
        for (int s2 : states) {
            for (int s3 : states) {
                for (int s4 : states) {
                    std::vector<std::vector<int8_t>> chips = {
                        WalshMatrix::spread_bit(s1, H[0]),
                        WalshMatrix::spread_bit(s2, H[1]),
                        WalshMatrix::spread_bit(s3, H[2]),
                        WalshMatrix::spread_bit(s4, H[3])
                    };
                    auto S = WalshMatrix::superimpose(chips, 4);

                    ASSERT_EQUAL(WalshMatrix::decode(S, H[0]).raw_bit, s1, "Permutation match Stn 1");
                    ASSERT_EQUAL(WalshMatrix::decode(S, H[1]).raw_bit, s2, "Permutation match Stn 2");
                    ASSERT_EQUAL(WalshMatrix::decode(S, H[2]).raw_bit, s3, "Permutation match Stn 3");
                    ASSERT_EQUAL(WalshMatrix::decode(S, H[3]).raw_bit, s4, "Permutation match Stn 4");
                }
            }
        }
    }
    std::cout << "\033[32mPASSED\033[0m\n";
    return true;
}

bool test_string_message_conversion() {
    std::cout << "  [Test] String to Bitstream Serialization & Inversion... ";
    std::string text = "CDMA-2026 Lab 4 Walsh Codes @ Jadavpur University";
    auto bits = WalshMatrix::string_to_bits(text);
    ASSERT_EQUAL(bits.size(), text.size() * 8, "Bit count should be len * 8");

    std::string recovered = WalshMatrix::bits_to_string(bits);
    ASSERT_EQUAL(recovered, text, "String round-trip equality");
    std::cout << "\033[32mPASSED\033[0m\n";
    return true;
}

bool test_frame_codec_serialization() {
    std::cout << "  [Test] Binary Protocol Framing & Network Endianness... ";
    Packet pkt(MsgType::SLOT_START, 7, 42, {0xAA, 0xBB, 0xCC});
    auto bytes = FrameCodec::serialize_packet(pkt);

    Packet parsed;
    ASSERT_TRUE(FrameCodec::parse_packet(bytes, parsed), "Packet deserialization failed");
    ASSERT_TRUE(parsed.type == MsgType::SLOT_START, "MsgType mismatch");
    ASSERT_EQUAL(parsed.station_id, 7, "Station ID mismatch");
    ASSERT_EQUAL(parsed.round_seq, 42, "Round sequence mismatch");
    ASSERT_EQUAL(parsed.payload.size(), 3, "Payload length mismatch");
    ASSERT_EQUAL(parsed.payload[0], 0xAA, "Payload byte 0");

    // Test ConfigSync serialization
    ConfigSyncData cfg;
    cfg.code_len = 4;
    cfg.total_stations = 3;
    cfg.walsh_matrix = WalshMatrix::generate(4);
    auto cfg_bytes = FrameCodec::encode_config_sync(cfg);

    ConfigSyncData decoded_cfg;
    ASSERT_TRUE(FrameCodec::decode_config_sync(cfg_bytes, decoded_cfg), "ConfigSync decode failed");
    ASSERT_EQUAL(decoded_cfg.code_len, 4, "ConfigSync code_len");
    ASSERT_EQUAL(decoded_cfg.total_stations, 3, "ConfigSync total_stations");
    ASSERT_TRUE(decoded_cfg.walsh_matrix == cfg.walsh_matrix, "ConfigSync matrix equality");

    std::cout << "\033[32mPASSED\033[0m\n";
    return true;
}

int main() {
    std::cout << "\n\033[1m========================================================\033[0m\n"
              << "\033[1m      CDMA Walsh Code Protocol Unit Test Suite          \033[0m\n"
              << "\033[1m========================================================\033[0m\n";

    bool ok = true;
    ok &= test_walsh_power_of_2();
    ok &= test_hadamard_generation_and_orthogonality();
    ok &= test_chip_modulation_and_spreading();
    ok &= test_linear_superposition_and_decoding();
    ok &= test_all_permutations_4stations();
    ok &= test_string_message_conversion();
    ok &= test_frame_codec_serialization();

    std::cout << "\033[1m========================================================\033[0m\n";
    if (ok) {
        std::cout << "\033[1;32m  ALL UNIT TESTS PASSED SUCCESSFULLY! (7/7)\033[0m\n";
        return 0;
    } else {
        std::cout << "\033[1;31m  SOME TESTS FAILED!\033[0m\n";
        return 1;
    }
}
