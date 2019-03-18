
#include <iostream>

#include "include/log.hpp"
#include "include/tuple.hpp"
#include "include/common.hpp"

#include "../include/fileio.hpp"

using std::cout;
using std::endl;

int
main()
{
  std::string logpath("/work/tanabe/ccbench/silo/log/log1");
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
    //cout << "tid : " << logrec.tid << endl;
    //cout << "key : " << logrec.key << endl;
    //cout << "val : " << logrec.val << endl << endl;
  }

  cout << "computed chkSum : " << chkSum << endl;
  cout << "before chkSum + after chkSum = " << loadhd.chkSum + chkSum << endl;

  return 0;
}
