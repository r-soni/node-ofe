#include <nan.h>
#include <v8-profiler.h>
#include <stdlib.h>
#if defined(_WIN32)
  #include <time.h>
  #define snprintf _snprintf
#else
  #include <sys/time.h>
#endif

using namespace v8;

class FileOutputStream: public OutputStream {
public:
  FileOutputStream(FILE* stream): stream_(stream) { }
  virtual int GetChunkSize() {
    return 65536;
  }
  virtual void EndOfStream() { }
  virtual WriteResult WriteAsciiChunk(char* data, int size) {
    const size_t len = static_cast<size_t>(size);
    size_t off = 0;
    while (off < len && !feof(stream_) && !ferror(stream_))
      off += fwrite(data + off, 1, len - off, stream_);
    return off == len ? kContinue : kAbort;
  }

private:
  FILE* stream_;
};

static void OnFatalError(const char* location, bool is_heap_oom) {
  if (location)
    fprintf(stderr, "FATAL ERROR: %s\n", location);

  fprintf(stderr, "Generating HeapDump\n");

  time_t rawtime;
  struct tm* timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);

  char filename[256];
  strftime(filename, sizeof(filename),"%Y%m%dT%H%M%S.heapsnapshot", timeinfo);
  FILE* fp = fopen(filename, "w");
  if (fp == NULL) abort();

#if NODE_VERSION_AT_LEAST(0, 11, 13)
  Isolate* isolate = Isolate::GetCurrent();
#if NODE_VERSION_AT_LEAST(3, 0, 0)
  const HeapSnapshot* snap =
  isolate->GetHeapProfiler()->TakeHeapSnapshot();
#else
  const HeapSnapshot* snap =
  isolate->GetHeapProfiler()->TakeHeapSnapshot(String::Empty(isolate));
#endif
#else
  const HeapSnapshot* snap = HeapProfiler::TakeSnapshot(String::Empty());
#endif
  FileOutputStream stream(fp);
  snap->Serialize(&stream, HeapSnapshot::kJSON);
  fclose(fp);
  exit(1);
}

NAN_METHOD(Method) {
  Nan::HandleScope scope;
  Isolate* isolate = Isolate::GetCurrent();
  isolate->SetOOMErrorHandler(OnFatalError);
  info.GetReturnValue().Set(Nan::New("done").ToLocalChecked());
}

void Init(Handle<Object> target) {
  target->Set(Nan::New("call").ToLocalChecked(),
    Nan::New<FunctionTemplate>(Method)->GetFunction());
}

NODE_MODULE(ofe, Init)
