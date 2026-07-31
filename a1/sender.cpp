#include <bits/stdc++.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <iomanip>
#define ll long long
using namespace std;

const size_t H=18;//header
const size_t P=44;//payload
const size_t T=2;//trailer
const size_t F=64;//18+44+2

//header definition
struct Header{
    uint8_t sn[8]={};
    uint8_t destmac[6]={};
    uint8_t srcmac[8]={};
    uint16_t len=44;
}

uint16_t ChecksumDummy(uint8_t* data, size_t len) {
    uint32_t sum = 0;//to check for wrap around
    for (size_t i = 0; i < len; i += 2) {
        uint16_t word = (data[i] << 8) | ((i + 1 < len) ? data[i + 1] : 0);//convert 8 bit bytes to 16 bit conjugates
        sum+=word;
        if (sum > 0xFFFF) sum = (sum & 0xFFFF) + 1;//wrap around
    }
    return static_cast<uint16_t>(~sum);//cast complement to 16
}

vector<uint8_t> buildFrame(Header& header,vector<uint8_t>& payload){
    vector<uint8_t> frame(F,0x00);
    memcpy(frame.data(),&header,H);

    size_t temp_len=min(payload.size(),P);
    size_t pad_len=P-temp_len;
    for (size_t i = 0; i < pad_len; ++i)frame[HEADER_SIZE + i] = 0x00; // Explicit Left Zero-Padding

    for (size_t i = 0; i < copy_len; ++i)frame[HEADER_SIZE + pad_len + i] = payload[i];//real payload

    uint16_t checksum=ChecksumDummy(frame.data(),H+P);
    
    //filling last 2 slots with trailer fsc
    frame[62] = (checksum >> 8) & 0xFF;//msb
    frame[63] = checksum & 0xFF;//lsb

    return frame;

}

void hexDump(vector<uint8_t>& frame) {
    for (size_t i = 0; i < frame.size(); ++i) {
        cout << hex << setw(2) << setfill('0') << static_cast<int>(frame[i]) << " ";
        if ((i + 1) % 16 == 0) cout << "\n";
    }
    cout << dec << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file>\n";
        return 1;
    }

    std::ifstream inFile(argv[1], std::ios::binary);
    if (!inFile) {
        std::cerr << "Error opening file: " << argv[1] << "\n";
        return 1;
    }

    FrameHeader header;
    std::vector<uint8_t> buffer(PAYLOAD_SIZE);
    int frameCount = 0;

    std::cout << "[Sender] Reading file and generating 64-byte frames...\n\n";

    while (inFile.read(reinterpret_cast<char*>(buffer.data()), PAYLOAD_SIZE) || inFile.gcount() > 0) {
        size_t bytesRead = inFile.gcount();
        std::vector<uint8_t> currentPayload(buffer.begin(), buffer.begin() + bytesRead);

        std::vector<uint8_t> frame = buildFrame(header, currentPayload);
        
        std::cout << "--- Frame #" << ++frameCount << " (" << frame.size() << " bytes) ---\n";
        hexDump(frame);
    }

    inFile.close();
    return 0;
}


