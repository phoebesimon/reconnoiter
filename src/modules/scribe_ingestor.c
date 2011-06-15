#include "noit_defines.h"
#include "noit_module.h"
#include "eventer/eventer.h"
#include "utils/noit_log.h"
#include "utils/noit_b64.h"
#include "utils/noit_str.h"
#include "utils/noit_mkdir.h"
#include "utils/noit_getip.h"
#include "stratcon_datastore.h"
#include "stratcon_realtime_http.h"
#include "stratcon_iep.h"
#include "noit_conf.h"
#include "noit_check.h"
#include "noit_rest.h"
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <zlib.h>
#include <assert.h>
#include <errno.h>

static noit_log_stream_t scribe_err = NULL;
static noit_log_stream_t scribe_deb = NULL;
static noit_log_stream_t ingest_err = NULL;

typedef struct {
  char *remote_str;
  char *remote_cn;
  int id; // Not sure this is necessary we could use for 
  int fd;
  char *filename;
  eventer_jobq_t  *jobq; // TODO: wire this up
} scribe_interim_journal_t;

static void
scribe_ingest_launch_file_ingestion(const char *path,
                                               const char *remote_str,
                                               const char *remote_cn,
                                               const char *id_str) {
  scribe_interim_journal_t *ij;
  eventer_t ingest;

  ij = calloc(1, sizeof(*ij));
  ij->fd = open(path, O_RDONLY);
  if(ij->fd < 0) {
    noitL(noit_error, "cannot open journal '%s': %s\n",
          path, strerror(errno));
    free(ij);
    return;
  }
  close(ij->fd);
  ij->fd = -1;
  ij->filename = strdup(path);
  ij->remote_str = strdup(remote_str);
  ij->remote_cn = strdup(remote_cn);
  ij->id = atoi(id_str);
  // TODO: Attach the scribe pusher to this object
  // TODO: initialize the jobq and other items here
  // ij->cpool = get_conn_pool_for_remote(ij->remote_str, ij->remote_cn,
  //                                      ij->fqdn);
  noitL(noit_error, "ingesting old payload: %s\n", ij->filename);
  ingest = eventer_alloc();
  ingest->mask = EVENTER_ASYNCH;
  // TODO: Create a callback that ingests with the FD
  // ingest->callback = stratcon_ingest_asynch_execute;
  ingest->closure = ij;
  // TODO: Attach to a threadpool, use a job queue on the ij object
  eventer_add_asynch(ij->jobq, ingest);

}

static ingestor_api_t scribe_ingestor_api = {
  .launch_file_ingestion = scribe_ingest_launch_file_ingestion,
  .iep_check_preload = NULL,
  .storage_node_lookup = NULL,
  .submit_realtime_lookup = NULL,
  .get_noit_config = NULL,
  .save_config = NULL
};

static int scribe_ingestor_config(noit_module_generic_t *self, noit_hash_table *o) {
  // This is the module configuration, traditionally not much is done here on
  // normal modules.
  return 0;
}
static int scribe_ingestor_onload(noit_image_t *self) {
  return 0;
}
static int scribe_ingestor_init(noit_module_generic_t *self) {
  // TODO: Initialize locks that are needed
  //pthread_mutex_init(&ds_conns_lock, NULL);
  scribe_err = noit_log_stream_find("error/datastore");
  scribe_deb = noit_log_stream_find("debug/datastore");
  ingest_err = noit_log_stream_find("error/ingest");
  if(!ingest_err) ingest_err = noit_error;
  // TODO: Set global configuration of scribe settings here
  // host, port
  //if(!noit_conf_get_string(NULL, "/stratcon/database/journal/path",
  //                         &basejpath)) {
  //  noitL(noit_error, "/stratcon/database/journal/path is unspecified\n");
  //  exit(-1);
  //}
  stratcon_ingest_all_check_info();
  stratcon_ingest_all_storagenode_info();
  stratcon_ingest_sweep_journals();
  return stratcon_datastore_set_ingestor(&scribe_ingestor_api);
}

noit_module_generic_t scribe_ingestor_t = { 
  {
    NOIT_GENERIC_MAGIC,
    NOIT_GENERIC_ABI_VERSION,
    "scribe_ingestor",
    "scribe drive for data ingestion",
    "", 
    scribe_ingestor_onload,
  },  
  scribe_ingestor_config,
  scribe_ingestor_init
};
