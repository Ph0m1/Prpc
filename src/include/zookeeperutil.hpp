#ifndef _zookeeperutil_h_
#define _zookeeperutil_h_
#include <semaphore.h>
#include <zookeeper/zookeeper.h>

#include <future>
#include <string>

class ZkClient {
 public:
  ZkClient();
  ~ZkClient();

  void Start();
  void Create(const char *path, const char *data, int datalen, int state = 0);
  std::string GetData(const char *path);

 private:
  zhandle_t *m_zhandle;

  struct AsyncContext {
    std::promise<int> promise_rc;
    std::promise<std::pair<std::string, int>> promise_data;
    zhandle_t *zk_handle;
    std::string path;
    std::string data;
    int state;
  };
  // Static global watcher function for zookeeper events
  static void global_watcher(zhandle_t *zh, int type, int state,
                             const char *path, void *watcherCtx);
  // Callback for zoo_aexists, used in the Create method
  static void exists_completion_callback(int rc, const struct Stat *stat,
                                         const void *data);

  // Callback for zoo_acreate, chained from the exists callback
  static void create_completion_callback(int rc, const char *value,
                                         const void *data);

  // Callback for zoo_aget, used in the GetData method
  static void get_completion_callback(int rc, const char *value, int value_len,
                                      const struct Stat *stat,
                                      const void *data);
};

#endif