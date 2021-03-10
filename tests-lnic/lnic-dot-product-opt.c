#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"
#include "dot-product.h"

/* Dot Product:
 * - Compute the dot product of each msg with the in-memory data.
 */

// Process message directly out of register file. 
#define DOT_PROD_OPT

#include "lnic-dot-product.h"

int main(void)
{
  lnic_dot_prod();
  return 0; // unreachable
}

