//---------------------------------------------------------------------
//---------------------------------------------------------------------
#include "header.h"

void  add() {

//---------------------------------------------------------------------
//---------------------------------------------------------------------

//---------------------------------------------------------------------
//     addition of update to the vector u
//---------------------------------------------------------------------

      int  c, i, j, k, m;

      for (c = 1; c <= ncells; c++) {
         for (k = start(3,c); k <= cell_size(3,c)-end(3,c)-1; k++) {
            for (j = start(2,c); j <= cell_size(2,c)-end(2,c)-1; j++) {
               for (i = start(1,c); i <= cell_size(1,c)-end(1,c)-1; i++) {
                  for (m = 1; m <= 5; m++) {
                     u(m,i,j,k,c) = u(m,i,j,k,c) + rhs(m,i,j,k,c);
                  }
               }
            }
         }
      }

      return;
}