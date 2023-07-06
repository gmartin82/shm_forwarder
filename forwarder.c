#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include "dds/dds.h"
#include "dds/ddsi/ddsi_serdata.h"

static volatile sig_atomic_t done = false;

static void sigint (int sig)
{
  (void) sig;
  done = true;
}

static dds_entity_t get_topic (dds_entity_t participant)
{
  dds_entity_t topic = -1;
  void *sample = NULL;
  dds_sample_info_t info;
  bool found = false;
  dds_return_t ret;

  dds_entity_t pub_reader = dds_create_reader (participant, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, NULL, NULL);
  assert (pub_reader >= 0);

  dds_entity_t waitset = dds_create_waitset (participant);
  (void) dds_set_status_mask (pub_reader, DDS_DATA_AVAILABLE_STATUS);
  (void) dds_waitset_attach (waitset, pub_reader, pub_reader);

  do
  {
    (void) dds_waitset_wait (waitset, NULL, 0, DDS_MSECS (100));
    dds_return_t n = dds_take (pub_reader, &sample, &info, 1, 1);
    if (n == 1 && info.valid_data)
    {
      dds_builtintopic_endpoint_t *ep = (dds_builtintopic_endpoint_t *) sample;
      const dds_typeinfo_t *type_info;
      dds_topic_descriptor_t *descriptor = NULL;
      ret = dds_builtintopic_get_endpoint_type_info (ep, &type_info);
      assert (ret == DDS_RETCODE_OK);

      if (strcmp (ep->topic_name, "pub_data") == 0 && type_info != NULL)
      {
        while (!found && !done)
        {
          ret = dds_create_topic_descriptor (DDS_FIND_SCOPE_GLOBAL, participant, type_info, DDS_MSECS (500), &descriptor);
          if (ret == DDS_RETCODE_OK && descriptor)
          {
            topic = dds_create_topic (participant, descriptor, ep->topic_name, NULL, NULL);
            assert (topic >= 0);
            dds_delete_topic_descriptor (descriptor);
            found = true;
          }
        }
      }
    }
  } while (!found && !done);

  return topic;
}


int main (int argc, char ** argv)
{
  dds_return_t ret;
  (void)argc;
  (void)argv;

  dds_entity_t participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  assert (participant >= 0);

  signal (SIGINT, sigint);

  dds_entity_t topic = get_topic (participant);
  if (topic < 0)
  {
    return 0;
  }

  dds_entity_t reader = dds_create_reader (participant, topic, NULL, NULL);

  while (!done)
  {
    dds_entity_t waitset = dds_create_waitset (participant);
    (void) dds_set_status_mask (reader, DDS_DATA_AVAILABLE_STATUS);
    (void) dds_waitset_attach (waitset, reader, reader);
    dds_waitset_wait (waitset, NULL, 0, DDS_MSECS (100));

    struct ddsi_serdata *serdata_rd = NULL;
    dds_sample_info_t info;
    dds_return_t n = dds_takecdr (reader, &serdata_rd, 1, &info, DDS_ANY_STATE);
    struct ddsi_keyhash key_hash;
    // struct ddsi_keyhash *key_hash = dds_alloc(sizeof(ddsi_keyhash));
    if (n > 0 && info.valid_data)
    {
      uint32_t sz = ddsi_serdata_size (serdata_rd);
      ddsrt_iovec_t data_in;
      struct ddsi_serdata *sdref = ddsi_serdata_to_ser_ref (serdata_rd, 0, sz, &data_in);
      assert (data_in.iov_len == sz);
      ddsi_serdata_unref (serdata_rd);

      unsigned char *buf = malloc (sz);
      memcpy (buf, data_in.iov_base, data_in.iov_len);
      ddsi_serdata_to_ser_unref (sdref, &data_in);

      // Raw data is in buf, do something meaningfull with this data (e.g. publish via Zenoh)
      bool print_received = true;
      if (print_received)
      {
        printf("Received: [");
        if (sz > 0)
        {
          printf("%u", buf[0]);
          for (int i = 1; i < sz; i++)
          {
            printf(", %u", buf[i]);
          }
        }
        printf("]\n");
      }

      free (buf);
    }
  }

  ret = dds_delete (participant);
  assert (ret == DDS_RETCODE_OK);

  return EXIT_SUCCESS;
}
