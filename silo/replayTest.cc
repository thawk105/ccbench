
#include <iostream>

#include "include/common.hh"
#include "include/log.hh"
#include "include/tuple.hh"

#include "../include/debug.hh"
#include "../include/fileio.hh"

using std::cout;
using std::endl;

extern void genLogFile(std::string &logpath, const int thid);

int main() {
  std::string logpath;
  genLogFileName(logpath, 1);
  File loadfile(logpath, O_RDONLY, 0644);

  LogHeader loadhd;
  LogRecord logrec;

  loadfile.read((void *)&loadhd, sizeof(LogHeader));
  cout << "chkSum : " << loadhd.chkSum << endl;
  cout << "logRecNum : " << loadhd.logRecNum << endl;

  int chkSum = 0;
  for (unsigned int i = 0; i < loadhd.logRecNum; ++i) {
    loadfile.read((void *)&logrec, sizeof(LogRecord));
    chkSum += logrec.computeChkSum();
    // cout << "tid : " << logrec.tid << endl;
    // cout << "key : " << logrec.key << endl;
    // cout << "val : " << logrec.val << endl << endl;
  }

  cout << "computed chkSum : " << chkSum << endl;
  cout << "before chkSum + after chkSum = " << loadhd.chkSum + chkSum << endl;

  return 0;
}
