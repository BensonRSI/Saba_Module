// xmodem_upload.cpp
//
// Simple XMODEM sender for a serial interface.
// Usage:
//   xmodem_upload <file> <serial_device> [baudrate] [preamble] [-v]
// Examples:
//   xmodem_upload firmware.bin /dev/ttyUSB0
//   xmodem_upload firmware.bin /dev/ttyUSB0 115200 "BOOT\n" -v

#include <bits/stdc++.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <errno.h>

using namespace std;

static const uint8_t SOH = 0x01;
static const uint8_t EOT = 0x04;
static const uint8_t ACK = 0x06;
static const uint8_t NAK = 0x15;
static const uint8_t CAN = 0x18;
static const uint8_t CPMEOF = 0x1A;
static const uint8_t CHAR_C = 'C';

int verbose = 0;

void vprint(const char *fmt, ...)
{
    if (!verbose)
        return;
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

static uint16_t crc16_xmodem(const uint8_t *data, size_t len)
{
    uint16_t crc = 0;
    for (size_t i = 0; i < len; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; ++j)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc & 0xFFFF;
}

int set_interface_attribs(int fd, int baudrate)
{
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0)
    {
        perror("tcgetattr");
        return -1;
    }
    cfmakeraw(&tty);

    // Input & output baud
    speed_t speed;
    switch (baudrate)
    {
    case 0:
        speed = B0;
        break;
    case 50:
        speed = B50;
        break;
    case 75:
        speed = B75;
        break;
    case 110:
        speed = B110;
        break;
    case 134:
        speed = B134;
        break;
    case 150:
        speed = B150;
        break;
    case 200:
        speed = B200;
        break;
    case 300:
        speed = B300;
        break;
    case 600:
        speed = B600;
        break;
    case 1200:
        speed = B1200;
        break;
    case 1800:
        speed = B1800;
        break;
    case 2400:
        speed = B2400;
        break;
    case 4800:
        speed = B4800;
        break;
    case 9600:
        speed = B9600;
        break;
    case 19200:
        speed = B19200;
        break;
    case 38400:
        speed = B38400;
        break;
    case 57600:
        speed = B57600;
        break;
    case 115200:
        speed = B115200;
        break;
    case 230400:
        speed = B230400;
        break;
    case 460800:
        speed = B460800;
        break;
    case 921600:
        speed = B921600;
        break;
    default:
        fprintf(stderr, "Unsupported baudrate %d, using 115200\n", baudrate);
        speed = B115200;
    }
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    // 8N1
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;

    // No flow control
    tty.c_cflag &= ~CRTSCTS;

    // Enable receiver, local mode
    tty.c_cflag |= (CLOCAL | CREAD);

    // Set attributes
    tty.c_cc[VMIN] = 0;  // non-blocking read
    tty.c_cc[VTIME] = 1; // 0.1s read timeout

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        perror("tcsetattr");
        return -1;
    }
    return 0;
}

ssize_t read_timeout(int fd, uint8_t *buf, size_t len, int timeout_ms)
{
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    int ret = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (ret > 0)
    {
        return read(fd, buf, len);
    }
    else if (ret == 0)
    {
        return 0; // timeout
    }
    else
    {
        return -1;
    }
}

int main(int argc, char **argv)
{
    // Parse command line with getopt: -f <file> -d <serial_device> [-b <baudrate>] [-p <preamble>] [-v] [-h]
    auto usage = [&](const char *prog)
    {
        fprintf(stderr, "Usage: %s -f <file> -d <serial_device> [-b <baudrate>] [-p <preamble>] [-v]\n", prog);
        fprintf(stderr, "Examples:\n");
        fprintf(stderr, "  %s -f firmware.bin -d /dev/ttyUSB0\n", prog);
        fprintf(stderr, "  %s -f firmware.bin -d /dev/ttyUSB0 -b 115200 -p \"BOOT\\n\" -v\n", prog);
    };

    auto unescape = [](const string &s) -> string
    {
        string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i)
        {
            if (s[i] == '\\' && i + 1 < s.size())
            {
                char c = s[++i];
                switch (c)
                {
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case '\\':
                    out.push_back('\\');
                    break;
                default:
                    out.push_back(c);
                    break;
                }
            }
            else
            {
                out.push_back(s[i]);
            }
        }
        return out;
    };

    string filename;
    string serial_dev;
    int baudrate = 115200;
    string preamble;

    int opt;
    while ((opt = getopt(argc, argv, "f:d:b:p:vh")) != -1)
    {
        switch (opt)
        {
        case 'f':
            filename = optarg;
            break;
        case 'd':
            serial_dev = optarg;
            break;
        case 'b':
            try
            {
                baudrate = stoi(optarg);
            }
            catch (...)
            {
                fprintf(stderr, "Invalid baudrate: %s\n", optarg);
                return 1;
            }
            break;
        case 'p':
            preamble = unescape(optarg);
            break;
        case 'v':
            verbose = 1;
            break;
        case 'h':
        default:
            usage(argv[0]);
            return 1;
        }
    }

    if (filename.empty() || serial_dev.empty())
    {
        usage(argv[0]);
        return 1;
    }

    FILE *fp = fopen(filename.c_str(), "rb");
    if (!fp)
    {
        perror("fopen");
        return 1;
    }

    int fd = open(serial_dev.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0)
    {
        perror("open serial");
        fclose(fp);
        return 1;
    }

    if (set_interface_attribs(fd, baudrate) != 0)
    {
        close(fd);
        fclose(fp);
        return 1;
    }

    // Flush serial buffers
    tcflush(fd, TCIOFLUSH);

    // Send preamble if provided
    if (!preamble.empty())
    {
        ssize_t w = write(fd, preamble.c_str(), preamble.size());
        tcdrain(fd);
        vprint("Sent preamble (%zd bytes): \"%s\"\n", w, preamble.c_str());
    }

    // Wait for receiver to request start: 'C' (CRC) or NAK (checksum)
    vprint("Waiting for receiver to request XMODEM start (C or NAK)...\n");
    bool use_crc = false;
    int max_wait = 30 * 1000; // 30s
    int waited = 0;
    uint8_t ch;
    bool got_start = false;
    while (waited < max_wait)
    {
        ssize_t r = read_timeout(fd, &ch, 1, 500);
        if (r < 0)
        {
            perror("read");
            close(fd);
            fclose(fp);
            return 1;
        }
        else if (r == 0)
        {
            waited += 500;
            continue;
        }
        else
        {
            if (ch == CHAR_C)
            {
                use_crc = true;
                got_start = true;
                break;
            }
            else if (ch == NAK)
            {
                use_crc = false;
                got_start = true;
                break;
            }
            else if (ch == CAN)
            {
                fprintf(stderr, "Received CAN, aborting\n");
                close(fd);
                fclose(fp);
                return 1;
            }
            else
            {
                vprint("Ignored byte 0x%02x while waiting for start\n", ch);
            }
        }
    }

    if (!got_start)
    {
        fprintf(stderr, "No start request from receiver (timeout)\n");
        close(fd);
        fclose(fp);
        return 1;
    }

    vprint("Receiver requested %s mode\n", use_crc ? "CRC" : "checksum");

    // Read file into memory in 128-byte blocks
    vector<vector<uint8_t>> blocks;
    while (1)
    {
        vector<uint8_t> buf(128, CPMEOF);
        size_t got = fread(buf.data(), 1, 128, fp);
        if (got == 0)
            break;
        if (got < 128)
        {
            // pad with CPMEOF (0x1A)
            for (size_t i = got; i < 128; ++i)
                buf[i] = CPMEOF;
        }
        blocks.push_back(buf);
        if (got < 128)
            break; // last block
    }
    fclose(fp);

    if (blocks.empty())
    {
        // send a single empty block? XMODEM expects at least one block of 128 padded
        blocks.push_back(vector<uint8_t>(128, CPMEOF));
    }

    int block_num = 1;
    const int max_retries = 10;
    for (size_t idx = 0; idx < blocks.size(); ++idx, ++block_num)
    {
        const vector<uint8_t> &data = blocks[idx];
        int retries = 0;
        bool block_ack = false;
        while (retries < max_retries && !block_ack)
        {
            // Build packet
            vector<uint8_t> pkt;
            pkt.push_back(SOH);
            pkt.push_back((uint8_t)(block_num & 0xFF));
            pkt.push_back((uint8_t)(~block_num & 0xFF));
            pkt.insert(pkt.end(), data.begin(), data.end());
            if (use_crc)
            {
                uint16_t crc = crc16_xmodem(data.data(), data.size());
                pkt.push_back((uint8_t)((crc >> 8) & 0xFF));
                pkt.push_back((uint8_t)(crc & 0xFF));
            }
            else
            {
                uint8_t sum = 0;
                for (auto b : data)
                    sum = (uint8_t)(sum + b);
                pkt.push_back(sum);
            }

            ssize_t w = write(fd, pkt.data(), pkt.size());
            if (w < 0)
            {
                perror("write");
                close(fd);
                return 1;
            }
            tcdrain(fd);
            vprint("Sent block %d (size %zd), attempt %d\n", block_num, pkt.size(), retries + 1);

            // Wait for ACK/NAK/CAN
            uint8_t resp = 0;
            int resp_wait_ms = 10000; // 10s
            ssize_t rr = read_timeout(fd, &resp, 1, resp_wait_ms);
            if (rr < 0)
            {
                perror("read");
                close(fd);
                return 1;
            }
            else if (rr == 0)
            {
                vprint("No response to block %d (timeout)\n", block_num);
                retries++;
                continue;
            }
            else
            {
                if (resp == ACK)
                {
                    block_ack = true;
                    vprint("Block %d ACK\n", block_num);
                    break;
                }
                else if (resp == NAK)
                {
                    vprint("Block %d NAK, retrying\n", block_num);
                    retries++;
                    continue;
                }
                else if (resp == CAN)
                {
                    fprintf(stderr, "Received CAN, aborting\n");
                    close(fd);
                    return 1;
                }
                else
                {
                    vprint("Received 0x%02x in response to block %d, ignoring\n", resp, block_num);
                    // continue waiting a little before retry
                    retries++;
                    continue;
                }
            }
        }
        if (!block_ack)
        {
            fprintf(stderr, "Failed to send block %d after %d retries\n", block_num, max_retries);
            close(fd);
            return 1;
        }
        printf("Sent block %d/%zu\n", block_num, blocks.size());
    }

    // Send EOT until ACK
    int eot_retries = 0;
    bool eot_ok = false;
    while (eot_retries < max_retries && !eot_ok)
    {
        uint8_t e = EOT;
        ssize_t ret = write(fd, &e, 1);
        if (ret < 0)
        {
            perror("write EOT");
            close(fd);
            return 1;
        }
        tcdrain(fd);
        vprint("Sent EOT, attempt %d\n", eot_retries + 1);
        uint8_t resp = 0;
        ssize_t rr = read_timeout(fd, &resp, 1, 10000);
        if (rr > 0 && resp == ACK)
        {
            eot_ok = true;
            break;
        }
        else
        {
            eot_retries++;
        }
    }
    if (!eot_ok)
    {
        fprintf(stderr, "Failed to complete EOT/ACK\n");
        close(fd);
        return 1;
    }

    printf("File \"%s\" uploaded successfully (%zu blocks)\n", filename.c_str(), blocks.size());
    close(fd);
    return 0;
}