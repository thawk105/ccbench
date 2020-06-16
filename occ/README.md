# Single version concurrency control format
- This is the format for adding single-version concurrency control.

## Where to edit
### Source files
- result.cc
  - L12-14 : Declare the variables with appropriate names.
- sv_format.cc<br>
Set an appropriate file name.
  - L48-66 : Edit it appropriately when you enable log persistence.
  - L106-133 : Define a single transaction workflow.
- util.cc<br>
  - L105-117 : Set initial value of record member appropriately.
  - leaderWork function : Define the job of a leader thread.
- Makefile<br>
  - Edit the file name "sv_format / SV_FORMAT" appropriately.
  - L9-37 : Define your preprocessor definition properly to determine the configuration.
  - Please remove the libraries and options that cannot be used in the experimental environment.
- transaction.cc<br>
  - Define the tbegin/validationPhase/abort/writePhase/read/write function properly.
   Delete unnecessary functions and add necessary functions to the transaction workflow.
### Header files
- include/common.hh<br>
  - Define the global variables and workload configuration variables that are necessary for Concurrency Control.
- include/log.hh<br>
  - Define the members of LogRecord class as appropriate. Please modify accordingly.
- include/result.hh<br>
  - Change the name of the variable appropriately.
- include/silo_op_element.hh<br>
  - Change the file name appropriately.
  - Change the members of ReadElement/WriteElement appropriately. And make corrections accordingly.
- include/transaction.hh<br>
  - Modify the members of TransactionStatus class appropriately as necessary.
  - TxnExecutor class is information that should be held by the worker thread. Please add any information necessary for concurrency control to the class members.
- include/tuple.hh<br>
  - Declare the metadata to be stored in the record header in the Tuple class.
  - attention : To improve the performance, no keys are stored. When a one-dimensional array is used as a DB table, the index position is the key. If you use masstree, the key-value is stored in the leaf node of masstree and the value is a pointer to the record, so there is no need to store the key.
- include/util.hh<br>
  - Edit it appropriately in conjunction with util.cc.
