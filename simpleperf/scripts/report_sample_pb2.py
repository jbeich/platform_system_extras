# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: cmd_report_sample.proto
"""Generated protocol buffer code."""
from google.protobuf.internal import builder as _builder
from google.protobuf import descriptor as _descriptor
from google.protobuf import descriptor_pool as _descriptor_pool
from google.protobuf import symbol_database as _symbol_database
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor_pool.Default().AddSerializedFile(b'\n\x17\x63md_report_sample.proto\x12\x17simpleperf_report_proto\"\xed\x06\n\x06Sample\x12\x0c\n\x04time\x18\x01 \x01(\x04\x12\x11\n\tthread_id\x18\x02 \x01(\x05\x12\x41\n\tcallchain\x18\x03 \x03(\x0b\x32..simpleperf_report_proto.Sample.CallChainEntry\x12\x13\n\x0b\x65vent_count\x18\x04 \x01(\x04\x12\x15\n\revent_type_id\x18\x05 \x01(\r\x12I\n\x10unwinding_result\x18\x06 \x01(\x0b\x32/.simpleperf_report_proto.Sample.UnwindingResult\x1a\x94\x02\n\x0e\x43\x61llChainEntry\x12\x15\n\rvaddr_in_file\x18\x01 \x01(\x04\x12\x0f\n\x07\x66ile_id\x18\x02 \x01(\r\x12\x11\n\tsymbol_id\x18\x03 \x01(\x05\x12\x63\n\x0e\x65xecution_type\x18\x04 \x01(\x0e\x32<.simpleperf_report_proto.Sample.CallChainEntry.ExecutionType:\rNATIVE_METHOD\"b\n\rExecutionType\x12\x11\n\rNATIVE_METHOD\x10\x00\x12\x1a\n\x16INTERPRETED_JVM_METHOD\x10\x01\x12\x12\n\x0eJIT_JVM_METHOD\x10\x02\x12\x0e\n\nART_METHOD\x10\x03\x1a\xf0\x02\n\x0fUnwindingResult\x12\x16\n\x0eraw_error_code\x18\x01 \x01(\r\x12\x12\n\nerror_addr\x18\x02 \x01(\x04\x12M\n\nerror_code\x18\x03 \x01(\x0e\x32\x39.simpleperf_report_proto.Sample.UnwindingResult.ErrorCode\"\xe1\x01\n\tErrorCode\x12\x0e\n\nERROR_NONE\x10\x00\x12\x11\n\rERROR_UNKNOWN\x10\x01\x12\x1a\n\x16\x45RROR_NOT_ENOUGH_STACK\x10\x02\x12\x18\n\x14\x45RROR_MEMORY_INVALID\x10\x03\x12\x15\n\x11\x45RROR_UNWIND_INFO\x10\x04\x12\x15\n\x11\x45RROR_INVALID_MAP\x10\x05\x12\x1c\n\x18\x45RROR_MAX_FRAME_EXCEEDED\x10\x06\x12\x18\n\x14\x45RROR_REPEATED_FRAME\x10\x07\x12\x15\n\x11\x45RROR_INVALID_ELF\x10\x08\"9\n\rLostSituation\x12\x14\n\x0csample_count\x18\x01 \x01(\x04\x12\x12\n\nlost_count\x18\x02 \x01(\x04\"H\n\x04\x46ile\x12\n\n\x02id\x18\x01 \x01(\r\x12\x0c\n\x04path\x18\x02 \x01(\t\x12\x0e\n\x06symbol\x18\x03 \x03(\t\x12\x16\n\x0emangled_symbol\x18\x04 \x03(\t\"D\n\x06Thread\x12\x11\n\tthread_id\x18\x01 \x01(\r\x12\x12\n\nprocess_id\x18\x02 \x01(\r\x12\x13\n\x0bthread_name\x18\x03 \x01(\t\"\x99\x01\n\x08MetaInfo\x12\x12\n\nevent_type\x18\x01 \x03(\t\x12\x18\n\x10\x61pp_package_name\x18\x02 \x01(\t\x12\x10\n\x08\x61pp_type\x18\x03 \x01(\t\x12\x1b\n\x13\x61ndroid_sdk_version\x18\x04 \x01(\t\x12\x1a\n\x12\x61ndroid_build_type\x18\x05 \x01(\t\x12\x14\n\x0ctrace_offcpu\x18\x06 \x01(\x08\"C\n\rContextSwitch\x12\x11\n\tswitch_on\x18\x01 \x01(\x08\x12\x0c\n\x04time\x18\x02 \x01(\x04\x12\x11\n\tthread_id\x18\x03 \x01(\r\"\xde\x02\n\x06Record\x12\x31\n\x06sample\x18\x01 \x01(\x0b\x32\x1f.simpleperf_report_proto.SampleH\x00\x12\x36\n\x04lost\x18\x02 \x01(\x0b\x32&.simpleperf_report_proto.LostSituationH\x00\x12-\n\x04\x66ile\x18\x03 \x01(\x0b\x32\x1d.simpleperf_report_proto.FileH\x00\x12\x31\n\x06thread\x18\x04 \x01(\x0b\x32\x1f.simpleperf_report_proto.ThreadH\x00\x12\x36\n\tmeta_info\x18\x05 \x01(\x0b\x32!.simpleperf_report_proto.MetaInfoH\x00\x12@\n\x0e\x63ontext_switch\x18\x06 \x01(\x0b\x32&.simpleperf_report_proto.ContextSwitchH\x00\x42\r\n\x0brecord_dataB6\n com.android.tools.profiler.protoB\x10SimpleperfReportH\x03')

_builder.BuildMessageAndEnumDescriptors(DESCRIPTOR, globals())
_builder.BuildTopDescriptorsAndMessages(DESCRIPTOR, 'cmd_report_sample_pb2', globals())
if _descriptor._USE_C_DESCRIPTORS == False:

  DESCRIPTOR._options = None
  DESCRIPTOR._serialized_options = b'\n com.android.tools.profiler.protoB\020SimpleperfReportH\003'
  _SAMPLE._serialized_start=53
  _SAMPLE._serialized_end=930
  _SAMPLE_CALLCHAINENTRY._serialized_start=283
  _SAMPLE_CALLCHAINENTRY._serialized_end=559
  _SAMPLE_CALLCHAINENTRY_EXECUTIONTYPE._serialized_start=461
  _SAMPLE_CALLCHAINENTRY_EXECUTIONTYPE._serialized_end=559
  _SAMPLE_UNWINDINGRESULT._serialized_start=562
  _SAMPLE_UNWINDINGRESULT._serialized_end=930
  _SAMPLE_UNWINDINGRESULT_ERRORCODE._serialized_start=705
  _SAMPLE_UNWINDINGRESULT_ERRORCODE._serialized_end=930
  _LOSTSITUATION._serialized_start=932
  _LOSTSITUATION._serialized_end=989
  _FILE._serialized_start=991
  _FILE._serialized_end=1063
  _THREAD._serialized_start=1065
  _THREAD._serialized_end=1133
  _METAINFO._serialized_start=1136
  _METAINFO._serialized_end=1289
  _CONTEXTSWITCH._serialized_start=1291
  _CONTEXTSWITCH._serialized_end=1358
  _RECORD._serialized_start=1361
  _RECORD._serialized_end=1711
# @@protoc_insertion_point(module_scope)