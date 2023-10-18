#include "node_file.h"  // NOLINT(build/include_inline)
#include "node_file-inl.h"

#if defined(_WIN32)
#include<windows.h>           // for windows
#else
#include<unistd.h>               // for linux
#endif

namespace node {

namespace fs {

using v8::FunctionCallbackInfo;
using v8::Int32;
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

static void UnlinkSync(const FunctionCallbackInfo<Value>& args) {
  const int argc = args.Length();
  CHECK_EQ(argc, 3);  // path, retries, retry delay

  CHECK(args[1]->IsInt32());
  CHECK(args[2]->IsInt32());

  Environment* env = Environment::GetCurrent(args);
  BufferValue path(env->isolate(), args[0]);

  THROW_IF_INSUFFICIENT_PERMISSIONS(
      env, permission::PermissionScope::kFileSystemWrite, path.ToStringView());

  const auto tries = args[1].As<Int32>()->Value() + 1;
  const auto retry_delay = args[2].As<Int32>()->Value();
  constexpr std::array<int, 5> retryable_errors = {
      EBUSY, EMFILE, ENFILE, ENOTEMPTY, EPERM};

  uv_fs_t req;

  for (int i = 1; i <= tries; i++) {
    FS_SYNC_TRACE_BEGIN(unlink);
    auto err = uv_fs_unlink(nullptr, &req, *path, nullptr);
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

} // end namespace fs

}  // end namespace node