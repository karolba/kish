#include "utils.h"
#include <unistd.h>
#include <sys/wait.h>
#include "Global.h"
#include <algorithm>
#include <cmath>
#include <cassert>
#include "job_control.h"

#include <iostream>

#undef min

namespace utils {


template<typename T>
static std::size_t min_size(std::vector<T> v) {
    std::size_t minimum = std::numeric_limits<std::size_t>::max();
    for(const T &t : v) {
        if(t.size() < minimum) {
            minimum = t.size();
        }
    }
    return minimum;
}

std::string common_prefix(const std::vector<std::string> &strings)  {
    assert(!strings.empty());

    std::size_t minimum_size = min_size(strings);

    bool all_strings_equal = true;
    std::string prefix;
    for(std::size_t i = 0; i < minimum_size; i++) {
        // assume characters are similar - if they aren't pop them at the end of the function
        // simplifies utf-8 handling
        prefix.push_back(strings.at(0).at(i));

        bool all_characters_equal = true;
        for(std::size_t string_i = 1; string_i < strings.size(); string_i++) {
            if(strings.at(0).at(i) != strings.at(string_i).at(i)) {
                all_characters_equal = false;
                break;
            }
        }
        if(!all_characters_equal) {
            all_strings_equal = false;
            break;
        }
    }

    if(!all_strings_equal) {
        // remove last character erronously put in prefix in the loop
        prefix.pop_back();
    }
    while(!prefix.empty() && front_of_multibyte_utf8_codepoint(*prefix.end())) {
        // in case of a removed multibyte utf-8 character, get rid of the front part as well
        prefix.pop_back();
    }

    return prefix;
}

// heavily adapted from the libc++ version of getline()
std::istream &getline_multiple_delimeters(std::istream &is, std::string &str, std::string_view delimiters) {
    std::ios_base::iostate state = std::ios_base::goodbit;
    str.clear();
    std::streamsize extr = 0;
    bool delimitBySpace = delimiters.find(' ') != delimiters.npos;
    bool delimitByTab = delimiters.find('\t') != delimiters.npos;
    bool delimitByNl = delimiters.find('\n') != delimiters.npos;
    while (true)
    {
        int i = is.rdbuf()->sbumpc();
        if(i == std::streambuf::traits_type::eof()) {
           state |= std::ios_base::eofbit;
           break;
        }

        ++extr;
        char ch = i;
        if(delimiters.find(ch) != std::string::npos) {
            // is.rdbuf()->sputbackc(ch);
            break;
        }

        // Ignore multiple continuous whitespace characters
        if((delimitBySpace && ch == ' ') || (delimitByTab && ch == '\t') || (delimitByNl && ch == '\n')) {
            int nextToEat;
            while(delimiters.find((nextToEat = is.rdbuf()->snextc())) != std::string::npos) {
                nextToEat = delimiters.at(nextToEat);
                if((delimitBySpace && nextToEat == ' ') || (delimitByTab && nextToEat == '\t') || (delimitByNl && nextToEat == '\n')) {
                    // eat
                    is.rdbuf()->sbumpc();
                }
            }
        }
        str.push_back(ch);

    }
    if (extr == 0) {
       state |= std::ios_base::failbit;
    }
    is.setstate(state);
    return is;
}

std::string_view remove_utf8_prefix(std::string_view view, std::size_t prefix) {
    for(std::size_t i = 0; i < prefix; i++) {
        if(front_of_multibyte_utf8_codepoint(view.at(i))) {
            i -= 1;
        }

        view.remove_prefix(1);
    }

    return view;
}


} // namespace utils
