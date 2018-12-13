#ifndef __http_test__
#define __http_test__

using namespace std;

#include <vector>
#include <string>
#include <istream>
#include <sstream>
#include <ostream>

namespace khct {
namespace http {

std::istream &read_header(std::istream &in, std::string &out);
class Resp;

void create_random_chunked_body(std::ostream &chunkedBody, std::ostream &expectBody);
}

struct khct::http::Resp {
    bool _add_status_100 = false;

    std::vector<std::string> headers;
    std::string body;
    std::string to_string();
    std::istringstream to_istringstream();
    Resp();
    Resp(std::istream& is, bool add_status_100 = false);
};
}


#endif
