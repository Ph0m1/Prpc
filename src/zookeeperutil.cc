#include "zookeeperutil.h"

#include "application.h"
#include "logger.h"

void ZkClient::global_watcher(zhandle_t* zh, int type, int state,
                              const char* path, void* watcherCtx) {
  if (type == ZOO_SESSION_EVENT) {
    if (state == ZOO_CONNECTED_STATE) {
      ZkClient* zk_client = (ZkClient*)zoo_get_context(zh);
      if (zk_client) {
        sem_post(&zk_client->m_sem);
      }
    }
  }
}

ZkClient::ZkClient() : m_zhandle(nullptr) { sem_init(&m_sem, 0, 0); }

ZkClient::~ZkClient() {
  if (m_zhandle != nullptr) {
    zookeeper_close(m_zhandle);
  }
  sem_destroy(&m_sem);
}

void ZkClient::Start(std::function<void()> session_expired_cb) {
  m_session_expired_cb = session_expired_cb;

  std::string host =
      Papplication::GetInstance().GetConfig().Load("zookeeperip");
  std::string port =
      Papplication::GetInstance().GetConfig().Load("zookeeperport");
  std::string connstr = host + ":" + port;
  if (m_zhandle != nullptr) {
    zookeeper_close(m_zhandle);
    m_zhandle = nullptr;
  }

  m_zhandle = zookeeper_init(connstr.c_str(), global_watcher, 3000, nullptr,
                             nullptr, 0);

  if (m_zhandle == nullptr) {
    LOG(FATAL) << "zookeeper_init error!";
  }
  sem_wait(&m_sem);
  LOG(INFO) << "zookeeper_init success: " << "current address: " << connstr;
}

void ZkClient::Create(const char* path, const char* data, int datalen,
                      int state) {
  // Create a context to pass data to the callbacks
  auto context = new AsyncContext{.promise_rc = {},
                                  .promise_data = {},
                                  .zk_handle = m_zhandle,
                                  .path = std::string(path),
                                  .data = std::string(data, datalen),
                                  .state = state};
  auto future = context->promise_rc.get_future();

  // Start the asynchronous chain by checking if the node exists
  int rc = zoo_aexists(m_zhandle, path, 0, exists_completion_callback, context);
  if (rc != ZOK) {
    LOG(ERROR) << "zoo_aexists failed immediately for path: " << path
               << " with error code: " << rc;
    delete context;
    return;
  }

  // Block and wait for the entire async operation (exists -> create) to
  // complete
  int final_rc = future.get();

  if (final_rc == ZOK) {
    LOG(INFO) << "Path: " << path << " created/existed successfully.";
  } else {
    LOG(ERROR) << "Failed to create path: " << path
               << ". Final error code: " << final_rc;
  }
}

std::string ZkClient::GetData(const char* path) {
  auto context = new AsyncContext();
  auto future = context->promise_data.get_future();

  // Call the asynchronous get function
  int rc = zoo_aget(m_zhandle, path, 0, get_completion_callback, context);
  if (rc != ZOK) {
    LOG(ERROR) << "zoo_get error";
    delete context;
    return "";
  }

  // Block and wait for the asynchronous get operation to complete
  auto result_pair = future.get();
  if (result_pair.second != ZOK) {
    LOG(ERROR) << "Failed to get data for path: " << path
               << ". Final error code: " << result_pair.second;
  }

  return result_pair.first;
}

// Callback for zoo_aexists
void ZkClient::exists_completion_callback(int rc, const struct Stat* stat,
                                          const void* data) {
  auto context = static_cast<AsyncContext*>(const_cast<void*>(data));

  if (rc == ZOK) {
    // Node already exists, the operation is considered successful.
    context->promise_rc.set_value(ZOK);
    delete context;
  } else if (rc == ZNONODE) {
    // Node does not exist, proceed to create it.
    // The context is passed along to the next callback in the chain.
    int create_rc = zoo_acreate(context->zk_handle, context->path.c_str(),
                                context->data.c_str(), context->data.length(),
                                &ZOO_OPEN_ACL_UNSAFE, context->state,
                                create_completion_callback, context);
    if (create_rc != ZOK) {
      // If the call to zoo_acreate fails immediately, we must set the promise
      // to unblock the waiting thread and prevent a memory leak.
      context->promise_rc.set_value(create_rc);
      delete context;
    }
    // If zoo_acreate is successfully queued, the promise will be set in
    // create_completion_callback
  } else {
    // Another error occurred during zoo_aexists
    context->promise_rc.set_value(rc);
    delete context;
  }
}

// Callback for zoo_acreate
void ZkClient::create_completion_callback(int rc, const char* value,
                                          const void* data) {
  auto context = static_cast<AsyncContext*>(const_cast<void*>(data));
  // This is the final step in the create chain, set the promise value with the
  // result code.
  context->promise_rc.set_value(rc);

  // Clean up the context
  delete context;
}

// Callback for zoo_aget
void ZkClient::get_completion_callback(int rc, const char* value, int value_len,
                                       const struct Stat* stat,
                                       const void* data) {
  auto context = static_cast<AsyncContext*>(const_cast<void*>(data));

  if (rc == ZOK) {
    // Operation successful, set the promise with the data and success code
    context->promise_data.set_value({std::string(value, value_len), ZOK});
  } else {
    // Operation failed, set the promise with an empty string and the error code
    context->promise_data.set_value({"", rc});
  }

  // Clean up the context
  delete context;
}