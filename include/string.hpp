#pragma once

static void
genStringRepeatedNumber(char *string, size_t val_size, size_t thid)
{
  // generate write value for this thread.
  uint num(thid), digit(1);
  while (num != 0) {
    num /= 10;
    if (num != 0) ++digit;
  }
  char thidString[digit];
  sprintf(thidString, "%ld", thid); 
  for (size_t i = 0; i < val_size;) {
    for (uint j = 0; j < digit; ++j) {
      string[i] = thidString[j];
      ++i;
      if (i == val_size - 2) {
        break;
      }
    }
  }
  string[val_size - 1] = '\0';
  // -----
}
