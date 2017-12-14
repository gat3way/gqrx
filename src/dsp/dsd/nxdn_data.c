#include "dsd.h"

void
processNXDNData (dsd_opts * opts, dsd_state * state)
{
  int i;
  char msg[1024];

  msg[0] = 0;

  if (opts->errorbars == 1)
    {
      sprintf (msg,"DATA    ");
      strcat(state->msgbuf,msg);
    }

  for (i = 0; i < 30; i++)
    {
      (void)getDibit (opts, state);
#ifdef NXDN_DUMP
      printf ("%c", dibit + 48);
#endif
    }
#ifdef NXDN_DUMP
  printf (" ");
#endif

  for (i = 0; i < 144; i++)
    {
      (void)getDibit (opts, state);
#ifdef NXDN_DUMP
      printf ("%c", dibit + 48);
#endif
    }

  if (opts->errorbars == 1)
    {
      sprintf (msg,"\n");
      strcat(state->msgbuf,msg);
    }
}
