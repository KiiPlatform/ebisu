#include <vector>
#include <string>
#include <istream>
#include <iostream>
#include <random>
#include "http_test.h"

std::istream& khct::http::read_header(std::istream &in, std::string &out)
{
    char c;
    while (in.get(c).good())
    {
        if (c == '\r')
        {
            c = in.peek();
            if (in.good())
            {
                if (c == '\n')
                {
                    in.ignore();
                    break;
                }
            }
        }
        out.append(1, c);
    }
    return in;
}

void khct::http::create_random_chunked_body(std::ostream &chunkedBody, std::ostream &expectBody) {
    string baseC =
        "0123456789"
        " \t\r\n"
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    random_device rd;
    mt19937 mt(rd());
    uniform_int_distribution<> randNum(10, 50);
    uniform_int_distribution<> randSize(10, 200);
    uniform_int_distribution<> randBody(0, baseC.size() - 1);
    const char* CRLF = "\r\n";
    for (int chunkNum = 0; chunkNum < randNum(mt); ++chunkNum) {
        size_t chunkSize = randSize(mt);
        chunkedBody << hex << chunkSize << CRLF;
        for (int i = 0; i < chunkSize; ++i) {
            const char c = baseC[randBody(mt)];
            chunkedBody << c;
            expectBody << c;
        }
        chunkedBody << CRLF;
    }
    chunkedBody << "0" << CRLF << CRLF;
}

std::string khct::http::Resp::to_string() {
    ostringstream o;
    vector<string> sub = headers;

    if (_add_status_100) {
        const char* CRLF = "\r\n";
        o << sub[0];
        o << CRLF;
        o << sub[1];
        o << CRLF;
        o << CRLF;
        sub.erase(sub.begin());
        sub.erase(sub.begin());
    }

    for (string h : sub) {
        o << h;
        o << "\r\n";
    }
    o << "\r\n";
    o << body;
    return o.str();
}

std::istringstream khct::http::Resp::to_istringstream() {
    return std::istringstream(this->to_string());
}

khct::http::Resp::Resp() {}

khct::http::Resp::Resp(std::istream& is, bool add_status_100) : _add_status_100(add_status_100) {
    is.seekg(0, std::ios::end);
    std::streampos length = is.tellg();
    is.seekg(0, std::ios::beg);

    if (add_status_100) {
        this->headers.push_back("HTTP/1.1 100 Continue");
        this->headers.push_back("Host: api.kii.com");
    }
    while(is.tellg() < length && is.good()) {
        std::string header = "";
        read_header(is, header);
        if (header == "") {
            break;
        }
        this->headers.push_back(header);
    }
    // Read body
    char* buffer = new char[1024];
    std::ostringstream os;
    while (is.tellg() < length && is.good()) {
        is.read(buffer, 1024);
        size_t len = is.gcount();
        os.write(buffer, len);
    }
    delete[] buffer;
    this->body = os.str();
}
