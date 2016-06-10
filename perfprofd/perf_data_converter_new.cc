
#include "perf_data_converter.h"

#include "thread_tree.h"

namespace wireless_android_logging_awp {

static bool ProcessRecord(std::unique_ptr<Record> record) {
  BuildThreadTree(*record, &thread_tree_);
  if (record->header.type == PERF_RECORD_SAMPLE) {
    sample_tree_builder_->ProcessSampleRecord(
        *static_cast<const SampleRecord*>(record.get()));
  }
  return true;
}

wireless_android_play_playlog::AndroidPerfProfile
RawPerfDataToAndroidPerfProfile(const std::string &perf_file)
{
  wireless_android_play_playlog::AndroidPerfProfile ret;


  std::unique_ptr<RecordFileReader> reader = RecordFileReader::CreateInstance(record_filename_);
  if (reader == nullptr) {
    return false;
  }
  bool result = reader->ReadDataSection(
      [this](std::unique_ptr<Record> record) {
        BuildThreadTree(*record, &thread_tree_);
        UnwindRecord(record.get());
        return record_file_writer_->WriteData(record->BinaryFormat());
      },
      false);
  if (!result) {
    return false;
  }
  if (!DumpAdditionalFeatures(args)) {
    return false;



  return ret;
}

}  // namespace wireless_android_logging_awp
