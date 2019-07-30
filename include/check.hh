
#include <ctype.h>
#include <string.h>

#include <iostream>

static bool chkInt(const char *arg) {
  for (uint i = 0; i < strlen(arg); ++i) {
    if (!isdigit(arg[i])) {
      std::cout << std::string(arg) << " is not a number." << std::endl;
      exit(0);
    }
  }

  return true;
}
