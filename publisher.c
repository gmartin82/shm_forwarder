#include "dds/dds.h"
#include "dds/ddsc/dds_loan_api.h"
#include "ForwarderData.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

static volatile sig_atomic_t done = false;

static void sigint (int sig)
{
  (void) sig;
  done = true;
}

int main (int argc, char ** argv)
{
  dds_return_t ret;
  (void)argc;
  (void)argv;

  dds_entity_t participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  assert (participant >= 0);

  dds_entity_t topic = dds_create_topic (participant, &ForwarderData_Msg_8192_desc, "pub_data", NULL, NULL);
  assert (topic >= 0);

  dds_entity_t writer = dds_create_writer (participant, topic, NULL, NULL);
  assert (writer >= 0);

  signal (SIGINT, sigint);

  void* sample = ForwarderData_Msg_8192__alloc();
  ForwarderData_Msg_Base* ptr = (ForwarderData_Msg_Base*)sample;
  ptr->payloadsize = 8192 - (uint32_t) sizeof(ForwarderData_Msg_Base);
  printf ("ptr->payloadsize = %i\n", ptr->payloadsize);
  ptr->count = 0;
  memset((char*)sample + offsetof(ForwarderData_Msg_8192, payload), 'a', ptr->payloadsize);
  
  while (!done)
  {
    void *loaned_sample;

    if ((ret = dds_loan_sample(writer, &loaned_sample)) < 0)
    {
      DDS_FATAL("dds_loan_sample: %s\n", dds_strretcode(-ret));
    }
    memcpy(loaned_sample, sample, ptr->payloadsize);

    printf ("Writing message - count = %ld\n", ptr->count);
    ret = dds_write (writer, loaned_sample);
    assert (ret == DDS_RETCODE_OK);
    ptr->count++;
    dds_sleepfor (DDS_MSECS (1000));
  }

  dds_free (ptr);

  ret = dds_delete (participant);
  assert (ret == DDS_RETCODE_OK);

  return EXIT_SUCCESS;
}
