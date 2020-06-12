
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

  loadfile.read((void *) &loadhd, sizeof(LogHeader));
  cout << "chkSum_ : " << loadhd.chkSum_ << endl;
  cout << "logRecNum_ : " << loadhd.logRecNum_ << endl;

  int chkSum_ = 0;
  for (unsigned int i = 0; i < loadhd.logRecNum_; ++i) {
    loadfile.read((void *) &logrec, sizeof(LogRecord));
    chkSum_ += logrec.computeChkSum();
    cout << "tid : " << logrec.tid_ << endl;
    cout << "key : " << logrec.key_ << endl;
    cout << "val : " << logrec.val_ << endl << endl;
  }

  cout << "computed chkSum_ : " << chkSum_ << endl;
  cout << "before chkSum_ + after chkSum_ = " << loadhd.chkSum_ + chkSum_
       << endl;

  return 0;
}
