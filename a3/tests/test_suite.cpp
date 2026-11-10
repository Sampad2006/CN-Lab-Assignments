#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>
#include <thread>
#include <chrono>
#include "frame.hpp"
#include "channel.hpp"
#include "csma.hpp"
#include "socket.hpp"

using namespace csma;

void test_frame_serialization() {
    std::cout << "[Test 1] Testing Frame Serialization & Deserialization... ";
    Message msg;
    msg.header.msg_type = static_cast<uint8_t>(MsgType::MSG_START_TX);
    msg.header.station_id = 42;
    msg.header.frame_seq = 7;
    msg.header.channel_state = static_cast<uint8_t>(ChannelState::BUSY);
    msg.header.duration_ms = 15;
    msg.header.collided = 0;
    msg.payload = "HELLO_CSMA_FRAME_DATA";

    std::vector<uint8_t> bytes = serialize_message(msg);
    assert(bytes.size() == sizeof(MessageHeader) + msg.payload.size());

    Message parsed;
    bool ok = deserialize_message(bytes.data(), bytes.size(), parsed);
    assert(ok);
    assert(parsed.header.msg_type == static_cast<uint8_t>(MsgType::MSG_START_TX));
    assert(parsed.header.station_id == 42);
    assert(parsed.header.frame_seq == 7);
    assert(parsed.header.channel_state == static_cast<uint8_t>(ChannelState::BUSY));
    assert(parsed.header.duration_ms == 15);
    assert(parsed.payload == "HELLO_CSMA_FRAME_DATA");

    std::cout << "PASSED\n";
}

void test_channel_state_transitions() {
    std::cout << "[Test 2] Testing Channel State Machine & Collision Handling... ";
    Channel channel;
    channel.set_verbose(false);

    // Initial state: IDLE
    assert(channel.get_state() == ChannelState::IDLE);

    // Station 1 starts TX
    Endpoint ep1("/tmp/test_station1.sock");
    bool ok1 = channel.handle_start_tx(1, 1, 10, ep1);
    assert(ok1 == true);
    assert(channel.get_state() == ChannelState::BUSY);

    // Station 2 starts TX while Station 1 is active -> COLLISION!
    Endpoint ep2("/tmp/test_station2.sock");
    bool ok2 = channel.handle_start_tx(2, 1, 10, ep2);
    assert(ok2 == false);
    assert(channel.get_state() == ChannelState::COLLISION);

    // Check collision status for station 1 and 2
    assert(channel.handle_check_cd(1) == true);
    assert(channel.handle_check_cd(2) == true);

    // Station 1 ends TX (collided)
    bool end1 = channel.handle_end_tx(1, 1, 64);
    assert(end1 == false); // Failed due to collision

    // Station 2 ends TX (collided)
    bool end2 = channel.handle_end_tx(2, 1, 64);
    assert(end2 == false);

    // Now channel should return to IDLE
    assert(channel.get_state() == ChannelState::IDLE);

    // Test successful single transmission
    bool single_tx = channel.handle_start_tx(3, 1, 10, ep1);
    assert(single_tx == true);
    assert(channel.get_state() == ChannelState::BUSY);
    assert(channel.handle_check_cd(3) == false);
    bool end3 = channel.handle_end_tx(3, 1, 64);
    assert(end3 == true);
    assert(channel.get_state() == ChannelState::IDLE);

    ChannelStats stats = channel.get_stats();
    assert(stats.successful_frames == 1);
    assert(stats.collided_frames == 2);
    assert(stats.collision_events == 1);

    std::cout << "PASSED\n";
}

void test_jam_signal() {
    std::cout << "[Test 3] Testing CSMA/CD Jam Signal Behavior... ";
    Channel channel;
    channel.set_verbose(false);

    Endpoint ep1("/tmp/test_station1.sock");
    channel.handle_start_tx(1, 1, 20, ep1);
    channel.handle_jam(1);

    assert(channel.get_state() == ChannelState::COLLISION);
    ChannelStats stats = channel.get_stats();
    assert(stats.jam_signals == 1);

    channel.handle_end_tx(1, 1, 64);
    assert(channel.get_state() == ChannelState::IDLE);

    std::cout << "PASSED\n";
}

void test_socket_ipc() {
    std::cout << "[Test 4] Testing Socket Datagram Client-Server IPC... ";
    UdpSocket srv;
    assert(srv.bind_auto(19876, 0));

    std::thread th([&]() {
        Endpoint client_src;
        std::vector<uint8_t> buf(1024);
        ssize_t n = srv.recv_from(buf.data(), buf.size(), client_src, 1000);
        assert(n > 0);
        Message m;
        assert(deserialize_message(buf.data(), n, m));
        assert(m.payload == "PING");

        Message resp;
        resp.header.msg_type = static_cast<uint8_t>(MsgType::MSG_SENSE_RESP);
        resp.payload = "PONG";
        std::vector<uint8_t> out = serialize_message(resp);
        ssize_t sent = srv.send_to(out, client_src);
        assert(sent > 0);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    UdpSocket cli;
    assert(cli.bind_auto(19876, 1));
    Endpoint srv_ep("/tmp/csma_channel_19876.sock");
    Message req;
    req.header.msg_type = static_cast<uint8_t>(MsgType::MSG_SENSE_REQ);
    req.payload = "PING";
    std::vector<uint8_t> out = serialize_message(req);
    ssize_t sent = cli.send_to(out, srv_ep);
    assert(sent > 0);

    Endpoint resp_src;
    std::vector<uint8_t> resp_buf(1024);
    ssize_t n = cli.recv_from(resp_buf.data(), resp_buf.size(), resp_src, 1000);
    assert(n > 0);
    Message resp;
    assert(deserialize_message(resp_buf.data(), n, resp));
    assert(resp.payload == "PONG");

    th.join();
    std::cout << "PASSED\n";
}

int main() {
    std::cout << "========================================================\n"
              << "       Running Assignment 3 Unit Test Suite             \n"
              << "========================================================\n";
    test_frame_serialization();
    test_channel_state_transitions();
    test_jam_signal();
    test_socket_ipc();
    std::cout << "========================================================\n"
              << "       ALL UNIT TESTS PASSED SUCCESSFULLY!              \n"
              << "========================================================\n";
    return 0;
}
