#include "node_file.h"  // NOLINT(build/include_inline)
#include "node_file-inl.h"

#include <optional>

#if defined(_WIN32)
#include<windows.h>           // for windows
#else
#include<unistd.h>               // for linux
#endif

namespace node {

namespace fs {

using v8::FunctionCallbackInfo;
using v8::Int32;
using v8::Isolate;
using v8::Value;

#define TRACE_NAME(name) "fs.sync." #name
#define GET_TRACE_ENABLED                                                      \
  (*TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(                                \
       TRACING_CATEGORY_NODE2(fs, sync)) != 0)
#define FS_SYNC_TRACE_BEGIN(syscall, ...)                                      \
  if (GET_TRACE_ENABLED)                                                       \
    TRACE_EVENT_BEGIN(                                                         \
        TRACING_CATEGORY_NODE2(fs, sync), TRACE_NAME(syscall), ##__VA_ARGS__);
#define FS_SYNC_TRACE_END(syscall, ...)                                        \
  if (GET_TRACE_ENABLED)                                                       \
    TRACE_EVENT_END(                                                           \
        TRACING_CATEGORY_NODE2(fs, sync), TRACE_NAME(syscall), ##__VA_ARGS__);

static void UnlinkSync(char* path, uint32_t max_retries, uint32_t retry_delay) {
  Isolate* isolate = Isolate::GetCurrent();
  Environment* env = Environment::GetCurrent(isolate);

  THROW_IF_INSUFFICIENT_PERMISSIONS(
      env, permission::PermissionScope::kFileSystemWrite, path);

  const auto tries = max_retries + 1;
  constexpr std::array<int, 5> retryable_errors = {
      EBUSY, EMFILE, ENFILE, ENOTEMPTY, EPERM};

  uv_fs_t req;

  for (uint64_t i = 1; i <= tries; i++) {
    FS_SYNC_TRACE_BEGIN(unlink);
    auto err = uv_fs_unlink(nullptr, &req, path, nullptr);
    FS_SYNC_TRACE_END(unlink);

    if(!is_uv_error(err)) {
      return;
    }

    if (i < tries && retry_delay > 0 &&
        std::find(retryable_errors.begin(), retryable_errors.end(), err) != retryable_errors.end()) {
      sleep(i * retry_delay * 1e-3);
    } else if (err == UV_ENOENT) {
      return;
    } else if (i == tries) {
      env->ThrowUVException(err, nullptr, "unlink");
      return;
    }
  }
}

constexpr bool is_uv_error_enoent(int result) {
  return result == UV_ENOENT;
}

static void FixWINEPERMSync(char* path, uint32_t max_retries, uint32_t retry_delay) {
  int chmod_result;
  uv_fs_t chmod_req;
  FS_SYNC_TRACE_BEGIN(chmod);
  chmod_result = uv_fs_chmod(nullptr, &chmod_req, path, 0666, nullptr);
  FS_SYNC_TRACE_END(chmod);

  if(is_uv_error(chmod_result)) {
    if (chmod_result == UV_ENOENT) {
      return;
    } else {
      // @TODO throw original error
      return;
    }
  }

  int stat_result;
  uv_fs_t stat_req;
  FS_SYNC_TRACE_BEGIN(stat);
  stat_result = uv_fs_stat(nullptr, &stat_req, path, nullptr);
  FS_SYNC_TRACE_END(stat);

  if(is_uv_error(stat_result)) {
    if(stat_result != UV_ENOENT){
      // @TODO throw original error
    }
    return;
  }

  if(S_ISDIR(stat_req.statbuf.st_mode)) {
    // @TODO call rmdirSync
  } else {
    return UnlinkSync(path, max_retries, retry_delay);
  }
}


} // end namespace fs

}  // end namespace node