#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void main(void)
{
   int i=0;
   char test[10]=" ";
   
   
   printf("\nlength:%2d %2d .%s.\n",strlen(test),sizeof(test),test);
   for(i=0;i<=strlen(test)+1;i++)
      printf("char %2d: %3d .%c.\n",i+1,test[i],test[i]);

   strcpy(test,"test");
   printf("\nlength:%2d %2d .%s.\n",strlen(test),sizeof(test),test);
   for(i=0;i<=strlen(test)+1;i++)
      printf("char %2d: %3d .%c.\n",i+1,test[i],test[i]);

   strcpy(test,"1234567890abcd");
   printf("\nlength:%2d %2d .%s.\n",strlen(test),sizeof(test),test);
   for(i=0;i<=strlen(test)+1;i++)
      printf("char %2d: %3d .%c.\n",i+1,test[i],test[i]);
   
   strcpy(test,"test");
   printf("\nlength:%2d %2d .%s.\n",strlen(test),sizeof(test),test);
   for(i=0;i<=15;i++)
      printf("char %2d: %3d .%c.\n",i+1,test[i],test[i]);

   exit(0);
}
