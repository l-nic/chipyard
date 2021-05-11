#include <stdio.h>
#include "lnic.h"

int main(int argc, char** argv)
{
  lnic_add_context(0, 0);
  printf("Hello World\n");
  while (1);
  return 0;
}
