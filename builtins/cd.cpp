#include "cd.h"
#include <vector>
#include <string>
#include "../Parser.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sstream>
#include "../utils.h"

// Changes "/./" and "//" sections in a path string into "/"
static void delete_dotslash_components(std::string &path) {

    std::stringstream ss;
    ss << path << '/'; // std::getline ignores the last component

    bool absolute = path.size() != 0 && path.at(0) == '/';
    if(absolute)
        path = "/";
    else
        path.clear();

    // TODO: use utils::Splitter
    std::string directory_name;
    bool first_part = true;
    while(std::getline(ss, directory_name, '/')) {
        if(!directory_name.empty() && directory_name != ".") {
            if(!first_part)
                path.push_back('/');
            path.append(directory_name);
            first_part = false;
        }
    }
}

static bool is_directory(const std::string &path) {
   struct stat statbuf;
   if (stat(path.c_str(), &statbuf) != 0)
       return false;
   return S_ISDIR(statbuf.st_mode);
}

static void usage() {
    fprintf(stderr, "%s\n", "cd: cd [-L|-P] <directory>");
}

int builtin_cd(const Command::Simple &cmd) {
    std::vector<std::string> argv = cmd.argv;

    enum { DotDotLogically, DotDotPhysically } operand_handling_mode = DotDotLogically;

    // Handle command line options: [-P|-L]
    while(argv.size() > 1 && argv.at(1).at(0) == '-') {
        if(argv.at(1) == "-P") {
            operand_handling_mode = DotDotPhysically;
        } else if(argv.at(1) == "-L") {
            operand_handling_mode = DotDotLogically;
        } else {
            fprintf(stderr, "cd: unknown option: '%s'\n", argv.at(1).c_str());
            usage();
            return 1;
        }
        argv.erase(argv.begin() + 1);
    }

    if(cmd.argv.size() > 2) {
        fprintf(stderr, "cd: too many arguments\n");
        usage();
        return 1;
    }

    // Following the steps described in IEEE Std 1003.1-2017:

    // 1. If no directory operand is given and the HOME environment variable is empty or undefined,
    // the default behavior is implementation-defined and no further steps shall be taken.
    if(cmd.argv.size() <= 1 && (getenv("HOME") == nullptr || strcmp(getenv("HOME"), "") == 0)) {
        return 0;
    }

    // 2. If no directory operand is given and the HOME environment variable is set to a non-empty value,
    // the cd utility shall behave as if the directory named in the HOME environment variable was specified as the directory operand.
    std::string directory_operand;
    if(cmd.argv.size() <= 1) {
        directory_operand = getenv("HOME");
    } else {
        directory_operand = cmd.argv.at(2);
    }

    // 3. If the directory operand begins with a <slash> character, set curpath to the operand and proceed to step 7.
    std::string curpath;
    if(directory_operand.size() > 0 && directory_operand.at(0) == '/') {
        curpath = directory_operand;
    } else {
        bool modified_from_cdpath = false;

        // 4. If the first component of the directory operand is dot or dot-dot, proceed to step 6.
        if(directory_operand != "."
                && directory_operand != ".."
                && strncmp(directory_operand.c_str(), "./", 2) != 0
                && strncmp(directory_operand.c_str(), "../", 3) != 0) {
            // 5. Starting with the first pathname in the <colon>-separated pathnames of CDPATH (see the ENVIRONMENT VARIABLES section)
            // if the pathname is non-null, test if the concatenation of that pathname, a <slash> character if that pathname did not
            // end with a <slash> character, and the directory operand names a directory. If the pathname is null, test
            // if the concatenation of dot, a <slash> character, and the operand names a directory.
            // In either case, if the resulting string names an existing directory, set curpath to that string and proceed to step 7.
            // Otherwise, repeat this step with the next pathname in CDPATH until all pathnames have been tested.
            const char *CDPATH = getenv("CDPATH");
            if(CDPATH) {
                std::stringstream stream;
                stream << CDPATH << ":"; // add a ':' to the end because std::getline will ignore the last section if it's empty
                std::string path;
                // TODO: Use utils::Splitter here
                while(std::getline(stream, path, ':')) {
                    if(path.size() == 0)
                        path = "./";
                    if(path.back() != '/')
                        path.push_back('/');
                    path += directory_operand;
                    if(is_directory(directory_operand)) {
                        curpath = path;
                        modified_from_cdpath = true;
                        continue;
                    }
                }
            }
        }
        if(!modified_from_cdpath) {
            // 6. Set curpath to the directory operand.
            curpath = directory_operand;
        }

    }

    // 7. If the -P option is in effect, proceed to step 10...
    if(operand_handling_mode == DotDotLogically) {
        // ... If curpath does not begin with a <slash> character, set curpath to the string formed by the concatenation of
        // the value of PWD, a <slash> character if the value of PWD did not end with a <slash> character, and curpath.
        if(curpath.size() == 0 || curpath.at(0) != '/') {

        }
        // 8. The curpath value shall then be converted to canonical form as follows, considering each component
        // from beginning to end, in sequence:

        // 8.a. Dot components and any <slash> characters that separate them from the next component shall be deleted.
        delete_dotslash_components(curpath);


        std::string preceding_components;
        // 8.b. For each dot-dot component, if there is a preceding component and it is neither root nor dot-dot, then:
        utils::Splitter(curpath).delim('/').for_each([&] (const std::string &part) {
            // 8.b.i. If the preceding component does not refer (in the context of pathname resolution with symbolic links followed)
            // to a directory, then the cd utility shall display an appropriate error message and no further steps shall be taken.

            if(part == "..") {

            } 

            return utils::Splitter::CONTINUE_LOOP;
        });



        // 8.b.ii. The preceding component, all <slash> characters separating the preceding component from dot-dot, dot-dot, and all
        // <slash> characters separating dot-dot from the following component (if any) shall be deleted.

        // 8.c. An implementation may further simplify curpath by removing any trailing <slash> characters that are not also
        // leading <slash> characters, replacing multiple non-leading consecutive <slash> characters with a single <slash>,
        // and replacing three or more leading <slash> characters with a single <slash>. If, as a result of this canonicalization,
        // the curpath variable is null, no further steps shall be taken.

        // 9. If curpath is longer than {PATH_MAX} bytes (including the terminating null) and the directory operand was not longer than
        // {PATH_MAX} bytes (including the terminating null), then curpath shall be converted from an absolute pathname to an equivalent
        // relative pathname if possible. This conversion shall always be considered possible if the value of PWD, with a trailing
        // <slash> added if it does not already have one, is an initial substring of curpath. Whether or not it is considered possible
        // under other circumstances is unspecified. Implementations may also apply this conversion if curpath is not longer than {PATH_MAX}
        // bytes or the directory operand was longer than {PATH_MAX} bytes.


    }

    // 10. The cd utility shall then perform actions equivalent to the chdir() function called with curpath as the path argument.
    // If these actions fail for any reason, the cd utility shall display an appropriate error message and the remainder of this
    // step shall not be executed. If the -P option is not in effect, the PWD environment variable shall be set to the value that
    // curpath had on entry to step 9 (i.e., before conversion to a relative pathname). If the -P option is in effect, the PWD
    // environment variable shall be set to the string that would be output by pwd -P. If there is insufficient permission on
    // the new directory, or on any parent of that directory, to determine the current working directory, the value of the PWD
    // environment variable is unspecified.
    if(chdir(curpath.c_str()) == -1) {
        perror("cd");
        return 1;
    }
    if(operand_handling_mode == DotDotLogically) {
        // TODO
    } else if(operand_handling_mode == DotDotPhysically) {
        // TODO
    }


    return 0;
}
